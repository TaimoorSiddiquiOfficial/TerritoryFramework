#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryAssetPreload.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryAssetReadiness,
	"TerritoryFramework.Assets.PendingLoadsCannotSpendOrSurviveRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryAssetReadiness::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto Stall = [](FTerritoryAssetPreload& Cache, const FSoftObjectPath& Path)
	{
		auto Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Path, FStreamableDelegate(), FStreamableManager::DefaultAsyncLoadPriority, false, true);
		Cache.Entries.Add(Path).Handle = Handle;
		return Handle;
	};
	auto* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	auto* Profile = NewObject<UTerritoryProductionProfile>();
	FTerritoryProductionSiteRecord Site;
	Site.TerritoryGUID = FGuid::NewGuid();
	Site.ProductionProfile = Profile;
	Site.LastEvaluatedCycle = 16;
	const auto PendingProduction = Stall(Economy->ProductionAssets, Site.ProductionProfile.ToSoftObjectPath());
	TestTrue(TEXT("Real streamable handle starts stalled"), PendingProduction && PendingProduction->IsStalled());
	Economy->EvaluateProductionSite(Site, 17);
	TestEqual(TEXT("Pending production reports readiness"), Site.LastStatus, ETerritoryProductionStatus::LoadingAssets);
	TestEqual(TEXT("Pending production cannot advance a cycle"), Site.LastEvaluatedCycle, int64(16));
	TestTrue(TEXT("Pending production cannot create a paid checkpoint"), Economy->GetProductionCheckpoints().IsEmpty());
	TestTrue(TEXT("Existing observation timer will retry the same cycle"), Economy->bProductionAssetsPending);
	const uint64 ProductionGeneration = Economy->ProductionAssets.GetGeneration();
	Economy->RestoreProductionState({}, {}, {});
	TestTrue(TEXT("Campaign restore cancels old production load"), PendingProduction->WasCanceled());
	TestTrue(TEXT("Production restore advances residency generation"), Economy->ProductionAssets.GetGeneration() > ProductionGeneration);
	TestFalse(TEXT("Old load cannot keep a replacement campaign pending"), Economy->bProductionAssetsPending);
	PendingProduction->StartStalledHandle();
	TestTrue(TEXT("Cancelled old production load cannot reinsert a site"), Economy->GetAllProductionSites().IsEmpty());

	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->StableTerritoryGUID = FGuid::NewGuid();
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NewObject<UNPCDefinition>();
	Force.AttackerDefinition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	Definition->CounterAttackProfile->FactionForces = {Force};
	Definition->ApplyToTerritory(Place);
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid::NewGuid();
	Record.State = ETerritoryAssaultState::Active;
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = Force.Faction;
	Record.PlannedForce = Record.PendingReserveForce = 6;
	Record.DecisionRoll = 0.375f;
	Counter->Assaults.Add(Record.AssaultID, Record);
	Counter->AssaultAssets.Reset();
	const auto PendingClass = Stall(Counter->AssaultAssets, Force.AttackerDefinition->NPCClassPath.ToSoftObjectPath());
	Counter->SpawnNextWave(Counter->Assaults.FindChecked(Record.AssaultID), Place);
	const auto& Waiting = Counter->Assaults.FindChecked(Record.AssaultID);
	TestEqual(TEXT("Pending class preserves the active encounter"), Waiting.State, ETerritoryAssaultState::Active);
	TestEqual(TEXT("Pending class cannot spend finite reserves"), Waiting.PendingReserveForce, 6);
	TestEqual(TEXT("Pending class is not a spawn failure"), Waiting.ConsecutiveSpawnFailures, 0);
	TestEqual(TEXT("Pending class cannot reroll the decision"), Waiting.DecisionRoll, 0.375f);
	const uint64 AssaultGeneration = Counter->AssaultAssets.GetGeneration();
	Counter->RestorePersistentState({});
	TestTrue(TEXT("Assault restore cancels old class load"), PendingClass->WasCanceled());
	TestTrue(TEXT("Assault restore advances residency generation"), Counter->AssaultAssets.GetGeneration() > AssaultGeneration);
	PendingClass->StartStalledHandle();
	TestTrue(TEXT("An old load cannot resurrect an assault"), Counter->Assaults.IsEmpty());
	Definition->CounterAttackProfile->FactionForces[0].SignatureVehicleClass = ANarrativeVehicleBase::StaticClass();
	FTerritoryFactionAssaultConfig UnrelatedForce;
	UnrelatedForce.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	UnrelatedForce.SignatureVehicleClass = TSoftClassPtr<ANarrativeVehicleBase>(FSoftObjectPath(TEXT("/Game/UnusedMissingVehicle.UnusedMissingVehicle_C")));
	Definition->CounterAttackProfile->FactionForces.Add(UnrelatedForce);
	FTerritoryAssaultApproach VehicleApproach;
	VehicleApproach.ApproachID = TEXT("ReadinessRoute");
	VehicleApproach.bEnabled = true;
	VehicleApproach.EntryType = ETerritoryAssaultEntryType::NarrativeVehicle;
	Definition->CounterAttackApproaches = {VehicleApproach};
	Definition->ApplyToTerritory(Place);
	TestEqual(TEXT("Unused faction vehicle and overridden fallback cannot block this force"),
		Counter->PrepareAssaultAssets(Place, &Record), ETerritoryAssetReadiness::Ready);

	auto ColdLoad = MakeShared<FTerritoryAssetPreload>();
	const FSoftObjectPath MeshPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const bool bWasResident = MeshPath.ResolveObject() != nullptr;
	const auto FirstReadiness = ColdLoad->Poll({MeshPath});
	TestTrue(TEXT("Valid engine asset is resident or pending"), FirstReadiness != ETerritoryAssetReadiness::Failed);
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ColdLoad, MeshPath, Deadline, bWasResident]()
	{
		const auto Readiness = ColdLoad->Poll({MeshPath});
		if (Readiness == ETerritoryAssetReadiness::Pending && FPlatformTime::Seconds() < Deadline) return false;
		TestEqual(TEXT("Actual async completion becomes ready"), Readiness, ETerritoryAssetReadiness::Ready);
		if (Readiness == ETerritoryAssetReadiness::Ready)
			AddInfo(bWasResident ? TEXT("Engine mesh was warm in this process.") : TEXT("Engine mesh loaded cold through the async loader."));
		ColdLoad->Reset();
		return true;
	}));
	return true;
}
#endif
