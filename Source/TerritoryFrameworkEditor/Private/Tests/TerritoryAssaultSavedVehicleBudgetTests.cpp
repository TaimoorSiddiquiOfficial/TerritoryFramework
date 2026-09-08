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
	"TerritoryFramework.CounterAttack.SaveLoad.InvalidVehicleLedgerCannotGrantDeployments",
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
	FTerritoryAssaultRecord Base;
	Base.AssaultID = FGuid(450, 4, 5, 6);
	Base.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Base.TargetTerritory = Definition->TerritoryTag;
	Base.AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Base.State = ETerritoryAssaultState::Active;
	Base.PlannedForce = 8;
	Base.KilledForce = 2;
	Base.WithdrawnForce = 1;
	Base.PendingReserveForce = 5;
	Base.MaximumVehicleDeployments = 2;
	Base.VehicleDeploymentsUsed = 1;
	Base.VehicleDeploymentsByApproach = {{TEXT("WestRoad"), 1}};
	Base.DecisionSeed = 450;
	Base.DecisionRoll = 0.375f;
	Base.PhysicalStateVersion = 1;
	const auto CheckInvalid = [&](const FString& Case, const FTerritoryAssaultRecord& Record)
	{
		Counter->RestorePersistentState({RoundTrip(Record)});
		const auto Restored = Counter->GetPersistentState()[0];
		TestEqual(Case + TEXT(": invalid ledger fails closed during load"), Restored.State, ETerritoryAssaultState::Cancelled);
		TestEqual(Case + TEXT(": cancellation reports invalid saved configuration"), Restored.Resolution, ETerritoryAssaultResolution::ConfigurationInvalid);
		TestEqual(Case + TEXT(": actual casualties remain consumed"), Restored.KilledForce, Record.KilledForce);
		TestEqual(Case + TEXT(": remaining force withdraws once"), Restored.WithdrawnForce, Record.PlannedForce - Record.KilledForce);
		TestEqual(Case + TEXT(": reserve cannot activate"), Restored.PendingReserveForce, 0);
		TestEqual(Case + TEXT(": finite total is conserved"), Restored.GetAccountedForce(), Record.PlannedForce);
		TestTrue(Case + TEXT(": published limits are bounded"), Restored.MaximumVehicleDeployments >= 0 && Restored.MaximumVehicleDeployments <= 8
			&& Restored.VehicleDeploymentsUsed >= 0 && Restored.VehicleDeploymentsUsed <= Restored.MaximumVehicleDeployments);
		TSet<FName> PublishedApproaches;
		for (const auto& Entry : Restored.VehicleDeploymentsByApproach)
		{
			TestTrue(Case + TEXT(": published approach rows are unique and bounded"), !Entry.ApproachID.IsNone()
				&& !PublishedApproaches.Contains(Entry.ApproachID) && Entry.Count >= 0 && Entry.Count <= 8);
			PublishedApproaches.Add(Entry.ApproachID);
		}
		TestTrue(Case + TEXT(": published row count is bounded"), PublishedApproaches.Num() <= 8);
		TestEqual(Case + TEXT(": decision seed is not rerolled"), Restored.DecisionSeed, Record.DecisionSeed);
		TestEqual(Case + TEXT(": decision roll is not rerolled"), Restored.DecisionRoll, Record.DecisionRoll);
		State->ExportPersistentState();
		const auto Published = State->GetAllAssaultSummaries();
		TestTrue(Case + TEXT(": late-join snapshot contains verified cancellation"), Published.Num() == 1 && Published[0].State == ETerritoryAssaultState::Cancelled);
		TestTrue(Case + TEXT(": cancelled record cannot reconstruct Native NPCs"), Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Place).IsEmpty());
		Counter->RestorePersistentState({RoundTrip(Restored)});
		TestEqual(Case + TEXT(": second load preserves withdrawal"), Counter->GetPersistentState()[0].WithdrawnForce, Restored.WithdrawnForce);
		TestEqual(Case + TEXT(": second load preserves cancellation"), Counter->GetPersistentState()[0].State, ETerritoryAssaultState::Cancelled);
	};
	for (const int32 Negative : {-1, MIN_int32})
	{
		for (int32 Field = 0; Field < 3; ++Field)
		{
			FTerritoryAssaultRecord Record = Base;
			if (Field == 0) Record.MaximumVehicleDeployments = Negative;
			if (Field == 1) Record.VehicleDeploymentsUsed = Negative;
			if (Field == 2) Record.VehicleDeploymentsByApproach[0].Count = Negative;
			CheckInvalid(FString::Printf(TEXT("Negative %d field %d"), Negative, Field), Record);
		}
	}
	for (const int32 Excess : {9, MAX_int32})
	{
		auto Record = Base;
		Record.MaximumVehicleDeployments = Excess;
		CheckInvalid(TEXT("Inflated difficulty limit"), Record);
		Record = Base;
		Record.VehicleDeploymentsUsed = Record.VehicleDeploymentsByApproach[0].Count = Excess;
		CheckInvalid(TEXT("Overspent difficulty budget"), Record);
	}
	for (const int32 Incorrect : {0, 2, MAX_int32})
	{
		auto Record = Base;
		Record.VehicleDeploymentsByApproach[0].Count = Incorrect;
		CheckInvalid(TEXT("Approach/global total disagreement"), Record);
	}
	auto Malformed = Base;
	Malformed.VehicleDeploymentsByApproach.Insert({TEXT("WestRoad"), 0}, 0);
	CheckInvalid(TEXT("Duplicate first row grants another approach deployment"), Malformed);
	Malformed = Base;
	Malformed.VehicleDeploymentsByApproach[0].ApproachID = NAME_None;
	CheckInvalid(TEXT("Missing approach identity"), Malformed);
	Malformed = Base;
	Malformed.VehicleDeploymentsByApproach.Reset();
	CheckInvalid(TEXT("Spent car has no approach charge"), Malformed);
	Malformed = Base;
	for (int32 Index = 0; Index < 8; ++Index)
		Malformed.VehicleDeploymentsByApproach.Add({FName(*FString::Printf(TEXT("Foot%d"), Index)), 0});
	CheckInvalid(TEXT("Too many saved approaches"), Malformed);
	Malformed = Base;
	Malformed.PlannedForce = MAX_int32;
	Malformed.PendingReserveForce = MAX_int32 - 3;
	Malformed.VehicleDeploymentsByApproach = {{TEXT("WestRoad"), MAX_int32}, {TEXT("EastRoad"), MAX_int32}};
	CheckInvalid(TEXT("Large force and charge totals cannot overflow"), Malformed);

	Counter->RestorePersistentState({RoundTrip(Base)});
	const auto Unchanged = Counter->GetPersistentState()[0];
	TestEqual(TEXT("A valid saved assault remains active"), Unchanged.State, ETerritoryAssaultState::Active);
	TestEqual(TEXT("A valid saved car remains charged"), Unchanged.VehicleDeploymentsUsed, 1);
	TestEqual(TEXT("A valid saved difficulty budget remains unchanged"), Unchanged.MaximumVehicleDeployments, 2);
	auto Boundary = Base;
	Boundary.MaximumVehicleDeployments = Boundary.VehicleDeploymentsUsed = 8;
	Boundary.VehicleDeploymentsByApproach = {{TEXT("WestRoad"), 8}};
	Counter->RestorePersistentState({RoundTrip(Boundary)});
	TestEqual(TEXT("Eight already spent cars remain valid"), Counter->GetPersistentState()[0].State, ETerritoryAssaultState::Active);
	auto Foot = Base;
	Foot.MaximumVehicleDeployments = Foot.VehicleDeploymentsUsed = 0;
	Foot.VehicleDeploymentsByApproach = {{TEXT("OnFoot"), 0}};
	Counter->RestorePersistentState({RoundTrip(Foot)});
	TestEqual(TEXT("On-foot zero-charge rows remain valid"), Counter->GetPersistentState()[0].State, ETerritoryAssaultState::Active);
	auto Legacy = Base;
	Legacy.PhysicalStateVersion = 0;
	Legacy.AliveForce = 2;
	Legacy.PendingReserveForce = 3;
	Counter->RestorePersistentState({RoundTrip(Legacy)});
	const auto Migrated = Counter->GetPersistentState()[0];
	TestEqual(TEXT("Valid legacy assault stays active"), Migrated.State, ETerritoryAssaultState::Active);
	TestEqual(TEXT("Legacy survivor credit is conserved"), Migrated.LegacySurvivorsToRestore, 2);
	TestTrue(TEXT("Legacy car credit uses its existing charge"), Migrated.LegacyVehicleRestoreCredits.Num() == 1 && Migrated.LegacyVehicleRestoreCredits[0].Count == 1);
	Counter->RestorePersistentState({RoundTrip(Migrated)});
	TestEqual(TEXT("Second legacy load keeps the spent car"), Counter->GetPersistentState()[0].VehicleDeploymentsUsed, 1);
	// Loading saved WorldState from a client actor must not replace server state.
	State->ExportPersistentState();
	Counter->RestorePersistentState({RoundTrip(Foot)});
	State->SetRole(ROLE_SimulatedProxy);
	State->Load_Implementation();
	TestEqual(TEXT("Client load cannot import saved server car usage"), Counter->GetPersistentState()[0].VehicleDeploymentsUsed, 0);
	State->SetRole(ROLE_Authority);
	// Validation is independent of whether the target's World Partition cell is loaded.
	Place->Destroy();
	Malformed = Base;
	Malformed.MaximumVehicleDeployments = 9;
	CheckInvalid(TEXT("Target streamed out"), Malformed);
	return true;
}

#endif
