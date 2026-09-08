#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

bool UTerritoryAuditEventProbe::InjectNegativeVehicleUsageForPIE(
	ATerritoryWorldState* State, const FGuid AssaultID)
{
	if (!IsValid(State) || !State->GetWorld()
		|| State->GetWorld()->WorldType != EWorldType::PIE || !AssaultID.IsValid()) return false;
	TArray<FTerritoryAssaultRecord> Records = State->GetAllAssaultSummaries();
	FTerritoryAssaultRecord* Record = Records.FindByPredicate(
		[&](const FTerritoryAssaultRecord& Candidate) { return Candidate.AssaultID == AssaultID; });
	if (!Record || Record->IsTerminal()) return false;
	FArrayProperty* SavedProperty = FindFProperty<FArrayProperty>(
		ATerritoryWorldState::StaticClass(), TEXT("SavedAssaults"));
	if (!SavedProperty) return false;
	Record->VehicleDeploymentsUsed = -1;
	*SavedProperty->ContainerPtrToValuePtr<TArray<FTerritoryAssaultRecord>>(State) = MoveTemp(Records);
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSavedVehicleBudget,
	"TerritoryFramework.CounterAttack.SaveLoad.NegativeVehicleUsageCannotGrantDeployments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSavedVehicleBudget::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Saved vehicle budget world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(450, 1, 2, 3);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->ApplyToTerritory(Place);
	const auto RoundTrip = [](FTerritoryAssaultRecord Record)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Save(Writer, false);
		Save.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Save, &Record, nullptr);
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Load(Reader, true);
		Load.ArIsSaveGame = true;
		FTerritoryAssaultRecord Loaded;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Load, &Loaded, nullptr);
		return Loaded;
	};
	for (const int32 Negative : {-1, MIN_int32})
	{
		for (int32 Field = 0; Field < 3; ++Field)
		{
			FTerritoryAssaultRecord Record;
			Record.AssaultID = FGuid(450, 4, Field, Negative);
			Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
			Record.TargetTerritory = Definition->TerritoryTag;
			Record.AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
			Record.State = ETerritoryAssaultState::Active;
			Record.PlannedForce = 8;
			Record.KilledForce = 2;
			Record.WithdrawnForce = 1;
			Record.PendingReserveForce = 5;
			Record.MaximumVehicleDeployments = 2;
			Record.VehicleDeploymentsUsed = 1;
			Record.VehicleDeploymentsByApproach = {{TEXT("WestRoad"), 1}};
			Record.DecisionSeed = 450;
			Record.DecisionRoll = 0.375f;
			Record.PhysicalStateVersion = 1;
			if (Field == 0) Record.MaximumVehicleDeployments = Negative;
			if (Field == 1) Record.VehicleDeploymentsUsed = Negative;
			if (Field == 2) Record.VehicleDeploymentsByApproach[0].Count = Negative;
			Counter->RestorePersistentState({RoundTrip(Record)});
			const auto Restored = Counter->GetPersistentState()[0];
			TestEqual(TEXT("Negative car limits/usage fail closed during load"), Restored.State, ETerritoryAssaultState::Cancelled);
			TestEqual(TEXT("Cancellation identifies the invalid saved configuration"), Restored.Resolution, ETerritoryAssaultResolution::ConfigurationInvalid);
			TestEqual(TEXT("Loading does not alter real casualties"), Restored.KilledForce, 2);
			TestEqual(TEXT("Remaining force is withdrawn once, not granted fresh reserve"), Restored.WithdrawnForce, 6);
			TestEqual(TEXT("Invalid saved car credit cannot activate reserve"), Restored.PendingReserveForce, 0);
			TestEqual(TEXT("Malformed record preserves the finite total"), Restored.GetAccountedForce(), 8);
			TestTrue(TEXT("Published budgets contain no negative car counts"), Restored.MaximumVehicleDeployments >= 0 && Restored.VehicleDeploymentsUsed >= 0 && Restored.VehicleDeploymentsByApproach[0].Count >= 0);
			TestEqual(TEXT("Load repair does not reroll the seed"), Restored.DecisionSeed, Record.DecisionSeed);
			TestEqual(TEXT("Load repair does not reroll the decision"), Restored.DecisionRoll, Record.DecisionRoll);
			State->ExportPersistentState();
			const auto Published = State->GetAllAssaultSummaries();
			TestTrue(TEXT("The existing late-join snapshot exposes the verified cancellation"), Published.Num() == 1 && Published[0].State == ETerritoryAssaultState::Cancelled);
			TestTrue(TEXT("Cancelled records cannot reconstruct Native NPCs"), Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Place).IsEmpty());
			Counter->RestorePersistentState({RoundTrip(Restored)});
			TestEqual(TEXT("A second load cannot charge withdrawal twice"), Counter->GetPersistentState()[0].WithdrawnForce, 6);
			TestEqual(TEXT("A second load preserves the cancellation"), Counter->GetPersistentState()[0].State, ETerritoryAssaultState::Cancelled);
		}
	}
	FTerritoryAssaultRecord Valid;
	Valid.AssaultID = FGuid(450, 5, 6, 7);
	Valid.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Valid.TargetTerritory = Definition->TerritoryTag;
	Valid.State = ETerritoryAssaultState::Active;
	Valid.PlannedForce = Valid.PendingReserveForce = 8;
	Valid.MaximumVehicleDeployments = 2;
	Valid.VehicleDeploymentsUsed = 1;
	Valid.VehicleDeploymentsByApproach = {{TEXT("WestRoad"), 1}};
	Valid.PhysicalStateVersion = 1;
	Counter->RestorePersistentState({RoundTrip(Valid)});
	const auto Unchanged = Counter->GetPersistentState()[0];
	TestEqual(TEXT("A valid saved assault remains active"), Unchanged.State, ETerritoryAssaultState::Active);
	TestEqual(TEXT("A valid saved car remains charged"), Unchanged.VehicleDeploymentsUsed, 1);
	TestEqual(TEXT("A valid saved difficulty budget remains unchanged"), Unchanged.MaximumVehicleDeployments, 2);
	return true;
}

#endif
