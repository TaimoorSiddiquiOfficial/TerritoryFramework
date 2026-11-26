#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryWorldState.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEconomyCallbackReentry,
	"TerritoryFramework.Economy.Regression.CurrencyAndProductionCallbackReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEconomyCallbackReentry::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Economy callback world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	ANarrativeGameState* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
	Clock->SetRole(ROLE_Authority);
	World->SetGameState(Clock);
	FFloatProperty* ClockValue = FindFProperty<FFloatProperty>(Clock->GetClass(), TEXT("AccumulatedTime"));
	if (!TestNotNull(TEXT("Narrative save clock is available for the fixture"), ClockValue))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	ClockValue->SetPropertyValue_InContainer(Clock, 2400.f);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardCharacter* Account = World->SpawnActor<ATerritoryGuardCharacter>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
	UNarrativeInventoryComponent* Inventory = Account->GetInventoryComponent();
	Inventory->SetCurrency(0);
	Inventory->SetCapacity(32);
	Inventory->SetWeightCapacity(100.f);
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	Economy->IncomePayoutPolicy = ETerritoryIncomePayoutPolicy::SharedNarrativeAccount;
	TestTrue(TEXT("Explicit Narrative account registration succeeds"),
		Economy->RegisterFactionCurrencyAccount(Heroes, Economy->IncomePayoutPolicy, Account));
	Economy->FactionTreasuries.FindOrAdd(Heroes).IncomePerTick = 10;
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	Inventory->OnCurrencyChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);
	bool bAttemptedNestedTick = false;
	Probe->CurrencyCallback = [Economy, &bAttemptedNestedTick]()
	{
		if (!bAttemptedNestedTick)
		{
			bAttemptedNestedTick = true;
			Economy->OnEconomyTick();
		}
	};
	Economy->OnEconomyTick();
	TestTrue(TEXT("Actual Narrative currency callback attempted a nested payout"), bAttemptedNestedTick);
	TestEqual(TEXT("One timer settlement credits income exactly once"), Inventory->GetCurrency(), 10);
	Probe->CurrencyCallback = nullptr;
	Economy->OnEconomyTick();
	TestEqual(TEXT("A later independent timer settlement remains enabled"), Inventory->GetCurrency(), 20);
	Inventory->PrepareForSave_Implementation();
	Inventory->SetCurrency(0);
	Inventory->Load_Implementation();
	TestEqual(TEXT("Narrative save/load retains the nonduplicated settlement"), Inventory->GetCurrency(), 20);

	UTerritoryProductionProfile* Profile = NewObject<UTerritoryProductionProfile>();
	FTerritoryProductionRule& Rule = Profile->Rules.AddDefaulted_GetRef();
	Rule.RuleTag = TerritoryProductionTags::FarmLivestock;
	FTerritoryResourceRate& Output = Rule.Outputs.AddDefaulted_GetRef();
	Output.ItemClass = UNarrativeItem::StaticClass();
	Output.QuantityPerCycle = 1;
	FTerritoryProductionSiteRecord Site;
	Site.StateRulesVersion = 1; // Current-format detached fixture; legacy rebinding has its own regression.
	Site.TerritoryGUID = FGuid(131, 132, 133, 134);
	Site.OwnerFaction = Heroes;
	Site.TerritoryState = ETerritoryState::Claimed;
	Site.ProductionProfile = Profile;
	Economy->ProductionSites.Add(Site.TerritoryGUID, Site);
	FTerritoryProductionCheckpoint Checkpoint;
	Checkpoint.TerritoryGUID = Site.TerritoryGUID;
	Checkpoint.OwnerFaction = Heroes;
	Checkpoint.RuleTag = Rule.RuleTag;
	Checkpoint.LastProcessedCycle = 0;
	const FString Key = Economy->MakeProductionCheckpointKey(Site.TerritoryGUID, Rule.RuleTag);
	Economy->ProductionCheckpoints.Add(Key, Checkpoint);
	TestTrue(TEXT("Resource routing uses the same real Narrative account"),
		Economy->RegisterFactionResourceAccount(Heroes, Account));
	Inventory->OnItemAdded.AddDynamic(Probe, &UTerritoryAuditEventProbe::ItemAdded);
	bool bAttemptedNestedProduction = false;
	Probe->ItemCallback = [Economy, &bAttemptedNestedProduction]()
	{
		if (!bAttemptedNestedProduction)
		{
			bAttemptedNestedProduction = true;
			Economy->ProcessResourceProduction();
		}
	};
	Economy->ProcessResourceProduction();
	TestTrue(TEXT("Actual Narrative item callback attempted recursive production"), bAttemptedNestedProduction);
	const TSoftClassPtr<UNarrativeItem> ItemClass(UNarrativeItem::StaticClass());
	TestEqual(TEXT("A production cycle creates one output despite item callback reentry"),
		Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 1);
	Probe->ItemCallback = nullptr;
	const auto SavedCheckpoints = Economy->GetProductionCheckpoints();
	const auto SavedSites = Economy->GetAllProductionSites();
	Economy->RestoreProductionState(SavedCheckpoints, SavedSites, {});
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("Reloading the completed cycle cannot award it again"),
		Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 1);
	ClockValue->SetPropertyValue_InContainer(Clock, 4800.f);
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("A later campaign cycle remains eligible"),
		Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 2);
	auto* Directory = World->SpawnActor<ATerritoryWorldState>();
	Directory->OnProductionStateChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::ProductionStateChanged);
	FTerritoryProductionSiteRecord RateSite = Site;
	RateSite.CurrencyRatesVersion = 1;
	RateSite.bCurrencyCapitalPolicyKnown = true;
	RateSite.bAllowCurrencyIncome = true;
	RateSite.BaseCurrencyIncome = 10;
	RateSite.ProductionProfile.Reset();
	Economy->RestoreProductionState({}, {RateSite}, {});
	int32 RateNotifications = 0;
	Probe->ProductionCallback = [&]()
	{
		// The bound avoids crashing a regressed implementation while proving that
		// the public callback cannot recursively publish another rate calculation.
		if (++RateNotifications < 3) Economy->RecalculateIncome(Heroes);
	};
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Rate observers cannot recursively republish currency rates"), RateNotifications, 1);
	const int32 CurrencyBeforeRestoreCallback = Inventory->GetCurrency();
	Probe->ProductionCallback = [&]()
	{
		TMap<FGameplayTag, FTerritoryTreasury> RestoredRates;
		RestoredRates.FindOrAdd(Heroes).IncomePerTick = 77;
		Economy->RestoreTreasuryState(RestoredRates);
	};
	Economy->OnEconomyTick();
	TestEqual(TEXT("A callback-restored campaign cannot receive the old timer's payment"), Inventory->GetCurrency(), CurrencyBeforeRestoreCallback);
	Probe->ProductionCallback = nullptr;
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
