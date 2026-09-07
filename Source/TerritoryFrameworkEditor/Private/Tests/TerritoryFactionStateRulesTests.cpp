#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Economy/TerritoryProductionTags.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Items/NarrativeItem.h"
#include "NarrativeSave.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Core/TerritoryGuardCharacter.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/UnrealType.h"

namespace TerritoryFactionRulesTests
{
struct FWorldFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ANarrativeGameState* Clock = nullptr;
	FWorldFixture()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		World->SetGameMode(FURL());
		Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
		Clock->SetRole(ROLE_Authority);
		World->SetGameState(Clock);
		SetTime(2400.f);
	}
	void SetTime(float Time) const
	{
		FindFProperty<FFloatProperty>(Clock->GetClass(), TEXT("AccumulatedTime"))->SetPropertyValue_InContainer(Clock, Time);
	}
	~FWorldFixture()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};

UTerritoryPlaceDefinition* MakeDefinition()
{
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(371,372,373,374);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->InitialGuardCount = 0;
	Definition->PeriodicIncome = 100;
	return Definition;
}

FTerritoryOwnershipData Claim(ATerritoryVolume* Territory, FGameplayTag Owner)
{
	FTerritoryOwnershipData Data = Territory->GetOwnershipData();
	Data.State = ETerritoryState::Claimed;
	Data.OwningFaction = Owner;
	Data.ControlProgress = 1.f;
	Data.DesiredGuardCount = 0;
	Data.PeriodicIncome = 100;
	return Data;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFactionStateRulesIntegration,
	"TerritoryFramework.StateRules.NarrativeOwnerTransitionsSaveAndClients",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFactionStateRulesIntegration::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionRulesTests;
	FWorldFixture Fixture;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Definition = MakeDefinition();
	auto& Common = Definition->StateConfigs.FindChecked(ETerritoryState::Claimed);
	Common.bAllowPeriodicIncome = false;
	Common.CounterAttackPolicy = ETerritoryStateCounterAttackPolicy::QuestOnly;
	auto& HeroesRules = Common.FactionOverrides.Add(Heroes);
	HeroesRules.bAllowPeriodicIncome = true;
	HeroesRules.EntryEvents.Add(NewObject<UTerritoryAuditNarrativeEvent>(Definition));
	auto* Territory = Fixture.World->SpawnActor<ATerritoryProperty>();
	TestTrue(TEXT("Existing Definition applies faction rules"), Definition->ApplyToTerritory(Territory));
	auto* Other = Fixture.World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Other);
	const auto* FirstRules = Territory->GetStateGameplayRules(ETerritoryState::Claimed, Heroes);
	const auto* OtherRules = Other->GetStateGameplayRules(ETerritoryState::Claimed, Heroes);
	TestTrue(TEXT("Faction Narrative events are cloned per territory"), FirstRules->EntryEvents[0] != OtherRules->EntryEvents[0]
		&& FirstRules->EntryEvents[0]->GetOuter() == Territory);
	int32 HeroEntries = 0, HeroExits = 0, DefaultEntries = 0;
	auto* EnterHero = CastChecked<UTerritoryAuditNarrativeEvent>(FirstRules->EntryEvents[0]);
	EnterHero->Callback = [&]() { ++HeroEntries; };
	auto* ExitHero = NewObject<UTerritoryAuditNarrativeEvent>(Territory);
	ExitHero->Callback = [&]() { ++HeroExits; };
	auto* EnterDefault = NewObject<UTerritoryAuditNarrativeEvent>(Territory);
	EnterDefault->Callback = [&]() { ++DefaultEntries; };
	auto& Runtime = Territory->RuntimeStateConfigs.FindChecked(ETerritoryState::Claimed);
	Runtime.FactionOverrides.FindChecked(Heroes).ExitEvents = {ExitHero};
	Runtime.EntryEvents = {EnterDefault};
	TestTrue(TEXT("Incoming owner selects Heroes Entry Events with empty world context"), Territory->CommitOwnershipData(Claim(Territory, Heroes)));
	TestEqual(TEXT("Only Heroes reward bundle fired"), HeroEntries, 1);
	TestEqual(TEXT("Common reward bundle did not leak to override"), DefaultEntries, 0);
	TestEqual(TEXT("Heroes earn currency"), Territory->GetEffectiveIncome(), 100);
	auto* Deny = NewObject<UTerritoryAuditCondition>(Territory);
	Deny->Callback = []() { return false; };
	Runtime.EntryConditions = {Deny};
	TestFalse(TEXT("Incoming Bandits use common failure condition even though outgoing Heroes permit entry"), Territory->CommitOwnershipData(Claim(Territory, Bandits)));
	TestEqual(TEXT("Rejected transition fires no exit reward"), HeroExits, 0);
	Runtime.EntryConditions.Empty();
	TestTrue(TEXT("Real same-state ownership handover succeeds"), Territory->CommitOwnershipData(Claim(Territory, Bandits)));
	TestEqual(TEXT("Exit uses outgoing Heroes, not the now-committed Bandits"), HeroExits, 1);
	TestEqual(TEXT("Unlisted owner uses common entry bundle"), DefaultEntries, 1);
	TestEqual(TEXT("Bandits do not earn currency"), Territory->GetEffectiveIncome(), 0);
	TestFalse(TEXT("Same-owner reset does not duplicate rewards"), Territory->CommitOwnershipData(Claim(Territory, Bandits)));
	auto* Save = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative creates the authoritative actor record"), Save->CreateActorRecord(Territory, Record));
	Territory->ForceSetOwningFaction(Heroes);
	const int32 EntriesBeforeLoad = HeroEntries + DefaultEntries;
	Save->LoadActorFromRecord(Territory, Record);
	TestEqual(TEXT("Save/load derives rules from restored owner"), Territory->GetEffectiveIncome(), 0);
	TestEqual(TEXT("Loading does not replay reward events"), HeroEntries + DefaultEntries, EntriesBeforeLoad);
	for (int32 Client = 0; Client < 2; ++Client)
	{
		auto* Replica = Fixture.World->SpawnActor<ATerritoryProperty>();
		Definition->ApplyToTerritory(Replica);
		Replica->SetRole(ROLE_SimulatedProxy);
		Replica->OwnershipData = Territory->GetOwnershipData(); // Replicated property payload.
		TestEqual(TEXT("Both client read models select the same owner rules"), Replica->GetEffectiveIncome(), 0);
		TestFalse(TEXT("Client cannot change ownership/rewards"), Replica->CommitOwnershipData(Claim(Replica, Heroes)));
		Replica->SetRole(ROLE_Authority);
	}
	const FGuid StableID = Territory->GetTerritoryGUID();
	Territory->Destroy();
	auto* Rebound = Fixture.World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Rebound);
	Save->LoadActorFromRecord(Rebound, Record);
	TestEqual(TEXT("Recreated streamed actor retains stable identity"), Rebound->GetTerritoryGUID(), StableID);
	TestEqual(TEXT("Rebound actor retains faction income policy"), Rebound->GetEffectiveIncome(), 0);
	TestNotNull(TEXT("Inherited Blueprint entry-event property remains available"), FTerritoryStateConfig::StaticStruct()->FindPropertyByName(TEXT("EntryEvents")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFactionCounterPolicyIntegration,
	"TerritoryFramework.StateRules.CounterattackWarQuestAndFiniteScheduling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFactionCounterPolicyIntegration::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionRulesTests;
	FWorldFixture Fixture;
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Definition = MakeDefinition();
	auto* Profile = NewObject<UTerritoryCounterAttackProfile>();
	Profile->bRequireReinforcementCapabilityForStrategicCounterattacks = false;
	auto* NPC = NewObject<UNPCDefinition>(Profile);
	NPC->CharacterID = TEXT("TF_FactionRulesCharacter");
	NPC->NPCID = TEXT("TF_FactionRulesNPC");
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	auto& Force = Profile->FactionForces.AddDefaulted_GetRef();
	Force.Faction = Bandits;
	Force.AttackerDefinition = NPC;
	Force.StagingRequirement = ETerritoryAssaultStagingRequirement::None;
	Force.ScheduleMode = ETerritoryCounterScheduleMode::SingleAssault;
	Force.PlannedForce = Force.WaveSize = 4;
	Force.MilitaryPower = 100.f;
	Definition->CounterAttackProfile = Profile;
	auto* Territory = Fixture.World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Territory);
	Territory->CommitOwnershipData(Claim(Territory, Heroes));
	auto* Counter = Fixture.World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Diplomacy = Fixture.World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	auto* Registry = Fixture.World->GetSubsystem<UTerritoryRegistrySubsystem>();
	Registry->RegisterTerritory(Territory);
	auto SetPolicy = [&](ETerritoryStateCounterAttackPolicy Policy)
	{
		Definition->StateConfigs.FindChecked(ETerritoryState::Claimed).CounterAttackPolicy = Policy;
		Territory->ApplyTerritoryDefinition();
	};
	FGameplayTag Selected;
	FTerritoryAssaultEvaluationInput Input;
	FTerritoryAssaultEvaluationResult Evaluation;
	FText Reason;
	SetPolicy(ETerritoryStateCounterAttackPolicy::QuestOnly);
	Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::War);
	TestFalse(TEXT("Quest-only blocks automatic preview"), Counter->GetBestEligibleAttackerPreview(Territory, Bandits, Selected, Input, Evaluation, Reason));
	TestTrue(TEXT("Quest-only allows explicit Wave preview"), Counter->GetBestAuthoredWaveAttackerPreview(Territory, Bandits, Selected, Input, Evaluation, Reason));
	TestFalse(TEXT("Direct automatic schedule cannot bypass QuestOnly"), Counter->ScheduleCounterAttack(Territory, Bandits));
	SetPolicy(ETerritoryStateCounterAttackPolicy::Disabled);
	TestFalse(TEXT("Disabled blocks explicit story preview"), Counter->GetBestAuthoredWaveAttackerPreview(Territory, Bandits, Selected, Input, Evaluation, Reason));
	FTerritoryStoryPursuitOptions StoryOptions;
	TestFalse(TEXT("Disabled rejects the actual explicit schedule API too"),
		Counter->TryScheduleAssaultWithReason(Territory, Bandits, ETerritoryAssaultLaunchMode::StrategicCounterattack, StoryOptions, Reason));
	SetPolicy(ETerritoryStateCounterAttackPolicy::QuestOnly);
	auto& Authored = Definition->StateConfigs.FindChecked(ETerritoryState::Claimed);
	Authored.AllowedAttackingFactions.AddTag(Heroes);
	Territory->ApplyTerritoryDefinition();
	TestFalse(TEXT("An explicit Wave cannot bypass the exact attacker allowlist"),
		Counter->TryScheduleAssaultWithReason(Territory, Bandits, ETerritoryAssaultLaunchMode::StrategicCounterattack, StoryOptions, Reason));
	Authored.AllowedAttackingFactions.Reset();
	SetPolicy(ETerritoryStateCounterAttackPolicy::WhileAtWar);
	for (EDiplomacyState Peace : {EDiplomacyState::None, EDiplomacyState::Alliance, EDiplomacyState::TradeAgreement, EDiplomacyState::NonAggression, EDiplomacyState::Ceasefire})
	{
		Diplomacy->SetDiplomacyState(Bandits, Heroes, Peace);
		TestFalse(TEXT("Automatic war policy never bypasses a non-war relationship"), Counter->GetBestEligibleAttackerPreview(Territory, Bandits, Selected, Input, Evaluation, Reason));
	}
	Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::War);
	Counter->TryScheduleRecurringStrategicAssaults();
	TestEqual(TEXT("War starts one finite schedule without an ownership-change event"), Counter->Assaults.Num(), 1);
	if (Counter->Assaults.Num() != 1) return false;
	FTerritoryAssaultRecord Scheduled = Counter->Assaults.CreateConstIterator()->Value;
	Counter->TryScheduleRecurringStrategicAssaults();
	TestEqual(TEXT("Another evaluation does not duplicate the schedule"), Counter->Assaults.Num(), 1);
	TestEqual(TEXT("War-driven scheduling starts in saved grace"), Scheduled.State, ETerritoryAssaultState::Grace);
	TestTrue(TEXT("Grace alone spawns no attackers"), Counter->LiveParticipants.IsEmpty());
	Counter->RestorePersistentState({Scheduled}, Counter->GetPersistentCycleState());
	TestEqual(TEXT("Reload preserves the decision seed"), Counter->Assaults.FindChecked(Scheduled.AssaultID).DecisionSeed, Scheduled.DecisionSeed);
	SetPolicy(ETerritoryStateCounterAttackPolicy::QuestOnly);
	Counter->AdvanceAssault(Counter->Assaults.FindChecked(Scheduled.AssaultID));
	TestEqual(TEXT("New story protection cancels an undeployed automatic force"), Counter->Assaults.FindChecked(Scheduled.AssaultID).Resolution, ETerritoryAssaultResolution::StateRuleBlocked);
	Scheduled.State = ETerritoryAssaultState::WaitingForPlayerProximity;
	Scheduled.SelectedApproaches = {TEXT("PolicyRecheck")};
	Counter->RestorePersistentState({Scheduled}, Counter->GetPersistentCycleState());
	TestFalse(TEXT("Immediate activation also rechecks policy after a warning callback"),
		Counter->ActivateAssault(Counter->Assaults.FindChecked(Scheduled.AssaultID), Territory));
	TestEqual(TEXT("Direct activation rejects before consuming force or routes"),
		Counter->Assaults.FindChecked(Scheduled.AssaultID).Resolution, ETerritoryAssaultResolution::StateRuleBlocked);
	Counter->Assaults.Empty(); // Terminal-history trimming must not reroll an old initial decision.
	SetPolicy(ETerritoryStateCounterAttackPolicy::WhileAtWar);
	Counter->TryScheduleRecurringStrategicAssaults();
	TestTrue(TEXT("Durable high-water prevents a new initial roll after history trimming"), Counter->Assaults.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFactionProductionPolicyIntegration,
	"TerritoryFramework.StateRules.ProductionUnloadedPolicyAndLegacyRebind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFactionProductionPolicyIntegration::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionRulesTests;
	FWorldFixture Fixture;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	auto* Account = Fixture.World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
	auto* Inventory = Account->GetInventoryComponent();
	Inventory->SetCapacity(32);
	Inventory->SetWeightCapacity(100.f);
	auto* Economy = Fixture.World->GetSubsystem<UTerritoryEconomySubsystem>();
	TestTrue(TEXT("Narrative inventory registered"), Economy->RegisterFactionResourceAccount(Heroes, Account));
	auto* Definition = MakeDefinition();
	auto& Common = Definition->StateConfigs.FindChecked(ETerritoryState::Claimed);
	Common.bAllowResourceProduction = false;
	Common.FactionOverrides.Add(Heroes).bAllowResourceProduction = true;
	auto* Profile = NewObject<UTerritoryProductionProfile>();
	FTerritoryProductionRule Rule;
	Rule.RuleTag = TerritoryProductionTags::FarmLivestock;
	FTerritoryResourceRate Output;
	Output.ItemClass = UNarrativeItem::StaticClass();
	Output.QuantityPerCycle = 1;
	Rule.Outputs.Add(Output);
	Profile->Rules.Add(Rule);
	FTerritoryProductionSiteRecord Site;
	Site.StateRulesVersion = 1;
	Site.TerritoryDefinition = Definition;
	Site.TerritoryGUID = Definition->StableTerritoryGUID;
	Site.TerritoryTag = Definition->TerritoryTag;
	Site.OwnerFaction = Heroes;
	Site.TerritoryState = ETerritoryState::Claimed;
	Site.ProductionProfile = Profile;
	FTerritoryProductionCheckpoint Checkpoint;
	Checkpoint.TerritoryGUID = Site.TerritoryGUID;
	Checkpoint.OwnerFaction = Heroes;
	Checkpoint.RuleTag = Rule.RuleTag;
	Checkpoint.LastProcessedCycle = 0;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	SaveArchive.ArIsSaveGame = true;
	FTerritoryProductionSiteRecord::StaticStruct()->SerializeItem(SaveArchive, &Site, nullptr);
	FTerritoryProductionSiteRecord RoundTrip;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	FTerritoryProductionSiteRecord::StaticStruct()->SerializeItem(LoadArchive, &RoundTrip, nullptr);
	TestEqual(TEXT("Production archive retains the authored Definition"), RoundTrip.TerritoryDefinition.Get(), static_cast<UTerritoryDefinition*>(Definition));
	TestEqual(TEXT("Production archive retains the migration version"), RoundTrip.StateRulesVersion, 1);
	TestEqual(TEXT("Production archive retains the exact owner"), RoundTrip.OwnerFaction, Heroes);
	Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	Economy->ProcessResourceProduction();
	const TSoftClassPtr<UNarrativeItem> ItemClass(UNarrativeItem::StaticClass());
	TestEqual(TEXT("Unloaded Place uses Definition owner override to produce"), Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 1);
	Common.FactionOverrides.FindChecked(Heroes).bAllowResourceProduction = false;
	Fixture.SetTime(4800.f);
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("Blocked owner produces no extra items"), Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 1);
	TestEqual(TEXT("Blocked cycle is consumed without future backpay"), Economy->GetProductionCheckpoints()[0].LastProcessedCycle, int64(2));
	Common.FactionOverrides.FindChecked(Heroes).bAllowResourceProduction = true;
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("Reallowing cannot replay a blocked cycle"), Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 1);
	Site.StateRulesVersion = 0;
	Site.TerritoryDefinition.Reset();
	Economy->RestoreProductionState({Checkpoint}, {Site}, {});
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("Unbound legacy site cannot bypass new state rules"), Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 1);
	auto* Property = Fixture.World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Property);
	Property->ProductionProfile = Profile;
	Property->CommitOwnershipData(Claim(Property, Heroes));
	Economy->RefreshProductionSite(Property);
	TestEqual(TEXT("Loaded actor migrates the legacy rule reference"), Economy->GetAllProductionSites()[0].StateRulesVersion, 1);
	Fixture.SetTime(7200.f);
	Economy->ProcessResourceProduction();
	TestEqual(TEXT("Migrated site earns only the next cycle"), Inventory->GetTotalQuantityOfItemExact(ItemClass, false), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFactionCapitalRewardIntegration,
	"TerritoryFramework.StateRules.AuthoredCapitalRewardUsesFactionInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFactionCapitalRewardIntegration::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionRulesTests;
	FWorldFixture Fixture;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Economy = Fixture.World->GetSubsystem<UTerritoryEconomySubsystem>();
	Economy->IncomePayoutPolicy = ETerritoryIncomePayoutPolicy::SharedNarrativeAccount;
	auto* HeroAccount = Fixture.World->SpawnActor<ATerritoryGuardCharacter>();
	auto* BanditAccount = Fixture.World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(HeroAccount)->AddFaction(Heroes);
	Cast<INarrativeTeamAgentInterface>(BanditAccount)->AddFaction(Bandits);
	TestTrue(TEXT("Heroes use an explicit Native account"), Economy->RegisterFactionCurrencyAccount(Heroes, Economy->IncomePayoutPolicy, HeroAccount));
	TestTrue(TEXT("Bandits also have a valid payable account"), Economy->RegisterFactionCurrencyAccount(Bandits, Economy->IncomePayoutPolicy, BanditAccount));
	auto* Definition = NewObject<UTerritoryDistrictDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	Definition->StableTerritoryGUID = FGuid(375,376,377,378);
	Definition->TerritoryActorClass = ATerritoryDistrict::StaticClass();
	Definition->bIsCapital = true;
	Definition->CapitalCaptureReward = 37;
	auto& ClaimedRules = Definition->StateConfigs.FindChecked(ETerritoryState::Claimed);
	ClaimedRules.bAllowCapitalCaptureReward = false;
	ClaimedRules.FactionOverrides.Add(Heroes).bAllowCapitalCaptureReward = true;
	auto* District = Fixture.World->SpawnActor<ATerritoryDistrict>();
	TestTrue(TEXT("District applies the authored reward"), Definition->ApplyToTerritory(District));
	const int32 BeforeHero = HeroAccount->GetInventoryComponent()->GetCurrency();
	const int32 BeforeBandit = BanditAccount->GetInventoryComponent()->GetCurrency();
	District->SetDerivedControl(Heroes, ETerritoryState::Claimed, FTerritoryTransitionContext());
	TestEqual(TEXT("Allowed faction receives the authored amount, not hardcoded 500"), HeroAccount->GetInventoryComponent()->GetCurrency(), BeforeHero + 37);
	District->SetDerivedControl(Bandits, ETerritoryState::Claimed, FTerritoryTransitionContext());
	TestEqual(TEXT("Disallowed faction receives no bonus despite having a valid account"), BanditAccount->GetInventoryComponent()->GetCurrency(), BeforeBandit);
	District->SetDerivedControl(Heroes, ETerritoryState::Claimed, FTerritoryTransitionContext());
	const int32 AfterRecapture = HeroAccount->GetInventoryComponent()->GetCurrency();
	District->SetDerivedControl(Heroes, ETerritoryState::Claimed, FTerritoryTransitionContext());
	TestEqual(TEXT("Same-owner aggregate reconciliation cannot replay bonus"), HeroAccount->GetInventoryComponent()->GetCurrency(), AfterRecapture);
	District->SetRole(ROLE_SimulatedProxy);
	struct FRewardEventParams { FGameplayTag CapturingFaction; } EventParams{Heroes};
	District->ProcessEvent(District->FindFunctionChecked(TEXT("OnDistrictFullyCaptured")), &EventParams);
	TestEqual(TEXT("Client callback cannot award currency"), HeroAccount->GetInventoryComponent()->GetCurrency(), AfterRecapture);
	District->SetRole(ROLE_Authority);
	return true;
}

#endif
