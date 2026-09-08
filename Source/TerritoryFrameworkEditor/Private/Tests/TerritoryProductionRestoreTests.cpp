#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Economy/TerritoryProductionTags.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Items/NarrativeItem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProductionRestoreCallbacks,
	"TerritoryFramework.Production.Regression.CallbackRestoreAndClockBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProductionRestoreCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Production restore world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	ANarrativeGameState* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
	Clock->SetRole(ROLE_Authority);
	World->SetGameState(Clock);
	FFloatProperty* ClockValue = FindFProperty<FFloatProperty>(Clock->GetClass(), TEXT("AccumulatedTime"));
	ClockValue->SetPropertyValue_InContainer(Clock, 2400.f);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardCharacter* Account = World->SpawnActor<ATerritoryGuardCharacter>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
	UNarrativeInventoryComponent* Inventory = Account->GetInventoryComponent();
	Inventory->SetCapacity(32);
	Inventory->SetWeightCapacity(100.f);
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	TestTrue(TEXT("Resource account is the authoritative Narrative inventory"),
		Economy->RegisterFactionResourceAccount(Heroes, Account));
	UTerritoryProductionProfile* Profile = NewObject<UTerritoryProductionProfile>();
	FTerritoryProductionRule Rule;
	Rule.RuleTag = TerritoryProductionTags::FarmLivestock;
	FTerritoryResourceRate Output;
	Output.ItemClass = UNarrativeItem::StaticClass();
	Output.QuantityPerCycle = 1;
	Rule.Outputs.Add(Output);
	Profile->Rules.Add(Rule);
	FTerritoryProductionSiteRecord Site;
	Site.StateRulesVersion = 1; // Current-format detached fixture; legacy rebinding has its own regression.
	Site.TerritoryGUID = FGuid(181, 182, 183, 184);
	Site.OwnerFaction = Heroes;
	Site.TerritoryState = ETerritoryState::Claimed;
	Site.ProductionProfile = Profile;
	FTerritoryProductionCheckpoint Checkpoint;
	Checkpoint.TerritoryGUID = Site.TerritoryGUID;
	Checkpoint.OwnerFaction = Heroes;
	Checkpoint.RuleTag = Rule.RuleTag;
	Checkpoint.LastProcessedCycle = 0;
	Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	FTerritoryProductionSiteRecord RestoredSite = Site;
	RestoredSite.LastStatus = ETerritoryProductionStatus::InvalidProfile;
	RestoredSite.RuleStates.AddDefaulted();
	FTerritoryProductionCheckpoint RestoredCheckpoint = Checkpoint;
	RestoredCheckpoint.LastProcessedCycle = 99;
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	Inventory->OnItemAdded.AddDynamic(Probe, &UTerritoryAuditEventProbe::ItemAdded);
	Economy->OnProductionSettled.AddDynamic(Probe, &UTerritoryAuditEventProbe::ProductionSettled);
	int32 Notifications = 0;
	Probe->ProductionCallback = [&]() { ++Notifications; };
	bool bRestoredFromItem = false;
	Probe->ItemCallback = [&]()
	{
		bRestoredFromItem = true;
		Economy->RestoreProductionState({RestoredCheckpoint}, {RestoredSite}, {});
	};
	Economy->ProcessResourceProduction();
	TestTrue(TEXT("A real Narrative item callback restored the same site identity"), bRestoredFromItem);
	TestEqual(TEXT("The superseded calculation emits no stale settlement"), Notifications, 0);
	TestEqual(TEXT("Restore retains its checkpoint instead of the old continuation"),
		Economy->GetProductionCheckpoints()[0].LastProcessedCycle, int64(99));
	TestEqual(TEXT("Restore retains its replacement read model"),
		Economy->GetAllProductionSites()[0].LastStatus, ETerritoryProductionStatus::InvalidProfile);
	Probe->ItemCallback = nullptr;
	Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	Probe->ProductionCallback = [&]()
	{
		++Notifications;
		Economy->RestoreProductionState({}, {}, {});
	};
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("Settlement callback ran exactly once before clearing the campaign"), Notifications, 1);
	TestTrue(TEXT("A cleared campaign is not repopulated by the old site continuation"), Economy->GetAllProductionSites().IsEmpty());
	TestTrue(TEXT("A cleared campaign keeps its empty checkpoint set"), Economy->GetProductionCheckpoints().IsEmpty());
	Probe->ProductionCallback = nullptr;
	Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	Probe->ItemCallback = [&]() { Profile->Rules.Empty(); };
	Economy->ProcessResourceProduction();
	TestTrue(TEXT("Rule callback actually cleared the source profile"), Profile->Rules.IsEmpty());
	TestEqual(TEXT("The active evaluation retains its copied rule"),
		Economy->GetAllProductionSites()[0].RuleStates[0].RuleTag, Rule.RuleTag);
	TestEqual(TEXT("The active copied rule commits one checkpoint"), Economy->GetProductionCheckpoints()[0].LastProcessedCycle, int64(1));
	Probe->ItemCallback = nullptr;
	ATerritoryProperty* RefreshedProperty = World->SpawnActor<ATerritoryProperty>();
	RefreshedProperty->SetActorGUID_Implementation(Site.TerritoryGUID);
	FTerritoryOwnershipData Claimed = RefreshedProperty->GetOwnershipData();
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Claimed.ControlProgress = 1.f;
	Claimed.DesiredGuardCount = 0;
	RefreshedProperty->CommitOwnershipData(Claimed);
	RefreshedProperty->ProductionProfile = Profile;
	Rule.Outputs[0].QuantityPerCycle = 2;
	Profile->Rules = {Rule};
	Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	bool bActorRefreshed = false;
	Probe->ItemCallback = [&]()
	{
		bActorRefreshed = true;
		Economy->RefreshProductionSite(RefreshedProperty);
	};
	Economy->ProcessResourceProduction();
	TestTrue(TEXT("Narrative callback refreshed the live property inputs"), bActorRefreshed);
	TestEqual(TEXT("Actor refresh preserves the cycle for already produced items"),
		Economy->GetProductionCheckpoints()[0].LastProcessedCycle, int64(1));
	Probe->ItemCallback = nullptr;
	const TSoftClassPtr<UNarrativeItem> ItemClass(UNarrativeItem::StaticClass());
	const int32 QuantityAfterRefresh = Inventory->GetTotalQuantityOfItemExact(ItemClass, false);
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("A refreshed actor cannot produce that cycle twice"),
		Inventory->GetTotalQuantityOfItemExact(ItemClass, false), QuantityAfterRefresh);
	for (float InvalidTime : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		ClockValue->SetPropertyValue_InContainer(Clock, InvalidTime);
		TestEqual(TEXT("Invalid Narrative clock values cannot become cycle indexes"), Economy->GetCurrentProductionCycle(), int64(INDEX_NONE));
	}
	ClockValue->SetPropertyValue_InContainer(Clock, 2400.f);
	for (float InvalidLength : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::min()})
	{
		Economy->ProductionCycleLength = InvalidLength;
		TestEqual(TEXT("Invalid or overflowing cycle durations are rejected"), Economy->GetCurrentProductionCycle(), int64(INDEX_NONE));
	}
	Economy->ProductionCycleLength = 2400.f;
	TestEqual(TEXT("A valid clock still resolves deterministically"), Economy->GetCurrentProductionCycle(), int64(1));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProductionAccountChanges,
	"TerritoryFramework.Production.Regression.AccountChangeDuringSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProductionAccountChanges::RunTest(const FString& Parameters)
{
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestNotNull(TEXT("Account change world"), World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		World->SetGameMode(FURL());
		ANarrativeGameState* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
		Clock->SetRole(ROLE_Authority);
		World->SetGameState(Clock);
		FFloatProperty* ClockValue = FindFProperty<FFloatProperty>(Clock->GetClass(), TEXT("AccumulatedTime"));
		ClockValue->SetPropertyValue_InContainer(Clock, 2400.f);
		TGuardValue<bool> ActorCallbacks(GAllowActorScriptExecutionInEditor, true);
		auto* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
		auto* Original = World->SpawnActor<ATerritoryGuardCharacter>();
		auto* Successor = World->SpawnActor<ATerritoryGuardCharacter>();
		const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		for (auto* Account : {Original, Successor})
		{
			Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
			Account->GetInventoryComponent()->SetCapacity(16);
			Account->GetInventoryComponent()->SetWeightCapacity(100.f);
			Account->GetInventoryComponent()->TryAddItemFromClass(UTerritoryAuditResourceA::StaticClass(), 2, false);
		}
		TestTrue(TEXT("Original depot is selected before production"), Economy->RegisterFactionResourceAccount(Heroes, Original));
		auto* Inventory = Original->GetInventoryComponent();
		auto* SuccessorInventory = Successor->GetInventoryComponent();
		const auto Count = [](UNarrativeInventoryComponent* Target, UClass* Class)
		{ return Target->GetTotalQuantityOfItemExact(TSoftClassPtr<UNarrativeItem>(Class), false); };
		const auto Rate = [](UClass* Class, int32 Quantity)
		{ FTerritoryResourceRate Value; Value.ItemClass = Class; Value.QuantityPerCycle = Quantity; return Value; };
		auto* Profile = NewObject<UTerritoryProductionProfile>();
		FTerritoryProductionRule Recipe;
		Recipe.RuleTag = TerritoryProductionTags::FarmLivestock;
		Recipe.Inputs = {Rate(UTerritoryAuditResourceA::StaticClass(), 2)};
		Recipe.Outputs = {Rate(UTerritoryAuditResourceB::StaticClass(), 1), Rate(UTerritoryAuditResourceC::StaticClass(), 1)};
		Profile->Rules = {Recipe};
		FTerritoryProductionSiteRecord Site;
		Site.StateRulesVersion = 1;
		Site.TerritoryGUID = FGuid(920, 921, 922, Scenario + 1);
		Site.OwnerFaction = Heroes;
		Site.TerritoryState = ETerritoryState::Claimed;
		Site.ProductionProfile = Profile;
		FTerritoryProductionCheckpoint Checkpoint;
		Checkpoint.TerritoryGUID = Site.TerritoryGUID;
		Checkpoint.OwnerFaction = Heroes;
		Checkpoint.RuleTag = Recipe.RuleTag;
		Checkpoint.LastProcessedCycle = 0;
		Economy->RestoreProductionState({Checkpoint}, {Site}, {});
		auto* Probe = NewObject<UTerritoryAuditEventProbe>();
		Inventory->OnItemAdded.AddDynamic(Probe, &UTerritoryAuditEventProbe::ItemAdded);
		bool bChanged = false;
		Probe->ItemCallback = [&]()
		{
			if (bChanged) return;
			bChanged = true;
			if (Scenario < 2) Economy->RegisterFactionResourceAccount(Heroes, Successor, Scenario == 0 ? 10 : 0);
			else
			{
				Cast<INarrativeTeamAgentInterface>(Original)->RemoveFaction(Heroes);
				Economy->RefreshFactionResourceAccount(Heroes);
			}
		};
		Economy->ProcessResourceProduction();
		Probe->ItemCallback = nullptr;
		Inventory->OnItemAdded.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ItemAdded);
		TestTrue(TEXT("Actual Native output callback changes account selection"), bChanged);
		TestEqual(TEXT("Changed account cannot receive a completed production batch"), Economy->GetAllProductionSites()[0].LastStatus, ETerritoryProductionStatus::SettlementChanged);
		TestEqual(TEXT("Original input is returned to its own inventory"), Count(Inventory, UTerritoryAuditResourceA::StaticClass()), 2);
		TestEqual(TEXT("First output is removed from the displaced account"), Count(Inventory, UTerritoryAuditResourceB::StaticClass()), 0);
		TestEqual(TEXT("Second output never reaches the displaced account"), Count(Inventory, UTerritoryAuditResourceC::StaticClass()), 0);
		TestEqual(TEXT("Compensation never draws from the successor"), Count(SuccessorInventory, UTerritoryAuditResourceA::StaticClass()), 2);
		TestEqual(TEXT("An in-flight batch is not redirected to the successor"), Count(SuccessorInventory, UTerritoryAuditResourceB::StaticClass()), 0);
		const auto SavedCheckpoints = Economy->GetProductionCheckpoints();
		const auto SavedSites = Economy->GetAllProductionSites();
		Inventory->PrepareForSave_Implementation();
		Inventory->Load_Implementation();
		Economy->RestoreProductionState(SavedCheckpoints, SavedSites, Economy->GetAllResourceSnapshots());
		Economy->ProcessResourceProduction();
		TestEqual(TEXT("Loading does not replay a cancelled cycle"), Count(SuccessorInventory, UTerritoryAuditResourceB::StaticClass()), 0);
		TestEqual(TEXT("Native save retains compensated stock"), Count(Inventory, UTerritoryAuditResourceA::StaticClass()), 2);
		// A later cycle deliberately uses the new selected depot.
		Economy->RegisterFactionResourceAccount(Heroes, Successor, 10);
		ClockValue->SetPropertyValue_InContainer(Clock, 4800.f);
		Economy->ProcessResourceProduction();
		TestEqual(TEXT("Next cycle produces in the selected successor"), Count(SuccessorInventory, UTerritoryAuditResourceC::StaticClass()), 1);
		TestEqual(TEXT("Next cycle does not touch the former depot"), Count(Inventory, UTerritoryAuditResourceA::StaticClass()), 2);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	return true;
}

#endif
