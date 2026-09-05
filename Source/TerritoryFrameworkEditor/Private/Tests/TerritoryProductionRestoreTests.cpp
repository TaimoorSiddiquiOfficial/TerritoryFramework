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

#endif
