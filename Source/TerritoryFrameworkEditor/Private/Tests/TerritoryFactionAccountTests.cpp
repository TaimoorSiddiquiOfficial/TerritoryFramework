#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Economy/TerritoryFactionResourceAccountComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "NarrativeGameplayTags.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Tales/TerritoryStoryEvents.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace TerritoryFactionAccountTests
{
struct FWorldFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldFixture()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		World->SetGameMode(FURL());
		auto* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
		Clock->SetRole(ROLE_Authority);
		World->SetGameState(Clock);
	}
	~FWorldFixture()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};

struct FPlayerFixture
{
	ANarrativePlayerController* Controller;
	ANarrativePlayerState* State;
	ANarrativePlayerCharacter* Character;
	UTerritoryFactionResourceAccountComponent* Account;
	FPlayerFixture(UWorld* World, FGameplayTag Faction)
	{
		State = World->SpawnActor<ANarrativePlayerState>();
		State->SetFactions(FGameplayTagContainer(Faction));
		Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
		Character->SetRole(ROLE_Authority);
		Character->SetPlayerState(State);
		Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		Controller->SetRole(ROLE_Authority);
		Controller->SetPlayerState(State);
		Controller->SetOwnedCharacter(Character);
		Controller->SetPawn(Character);
		World->AddController(Controller);
		Account = NewObject<UTerritoryFactionResourceAccountComponent>(Controller);
		Controller->AddInstanceComponent(Account);
		Account->BindingMode = ETerritoryResourceAccountBinding::OwnerPrimaryFaction;
	}
	~FPlayerFixture() { Account->UnregisterResourceAccount(); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFactionResourceSelection,
	"TerritoryFramework.Factions.ResourceAccounts.PriorityConflictAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFactionResourceSelection::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionAccountTests;
	FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Economy = Fixture.World->GetSubsystem<UTerritoryEconomySubsystem>();
	FPlayerFixture First(Fixture.World, Heroes), Second(Fixture.World, Heroes);
	TestTrue(TEXT("First eligible account registers"), First.Account->RegisterResourceAccount());
	TestFalse(TEXT("A tied second account reports it is not selected"), Second.Account->RegisterResourceAccount());
	TestTrue(TEXT("Equal priorities produce an explicit conflict"), Economy->HasFactionResourceAccountConflict(Heroes));
	TestNull(TEXT("A conflict never arbitrarily awards either player's inventory"), Economy->GetFactionResourceAccount(Heroes));
	TestFalse(TEXT("The earlier component no longer claims it is selected"), First.Account->IsResourceAccountRegistered());
	TestTrue(TEXT("Conflict reaches the saved/replicated read model"), Economy->GetFactionResourceSnapshot(Heroes).bAccountConflict);
	First.Account->UnregisterResourceAccount();
	Second.Account->UnregisterResourceAccount();
	TestTrue(TEXT("Opposite order starts with the other account"), Second.Account->RegisterResourceAccount());
	TestFalse(TEXT("Opposite registration order has the same blocked outcome"), First.Account->RegisterResourceAccount());
	TestNull(TEXT("Order cannot break a priority tie"), Economy->GetFactionResourceAccount(Heroes));
	First.Account->AccountPriority = 100;
	TestTrue(TEXT("A deliberate priority resolves the conflict"), First.Account->RegisterResourceAccount());
	TestFalse(TEXT("Lower-priority account status stays truthful"), Second.Account->IsResourceAccountRegistered());
	TestEqual(TEXT("Higher-priority account wins"), Economy->GetFactionResourceAccount(Heroes), static_cast<AActor*>(First.Controller));
	First.Account->UnregisterResourceAccount();
	TestTrue(TEXT("Removing the selected account promotes the remaining eligible account"), Second.Account->IsResourceAccountRegistered());
	TestTrue(TEXT("Removal refreshes storage instead of falsely marking it offline"), Economy->GetFactionResourceSnapshot(Heroes).bStorageAvailable);
	TestFalse(TEXT("A different faction cannot register this player"), Economy->RegisterFactionResourceAccount(Bandits, Second.Controller));
	Second.Controller->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A non-authoritative account cannot register"), Second.Account->RegisterResourceAccount());
	Second.Controller->SetRole(ROLE_Authority);
	FWorldFixture Foreign;
	auto* ForeignActor = Foreign.World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(ForeignActor)->AddFaction(Heroes);
	TestFalse(TEXT("A foreign-world inventory cannot register"), Economy->RegisterFactionResourceAccount(Heroes, ForeignActor));
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	Probe->AccountCallback = [Economy, &Second](FGameplayTag Faction)
	{
		Economy->UnregisterFactionResourceAccount(Faction, Second.Controller);
	};
	Economy->OnFactionResourceAccountChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::ResourceAccountChanged);
	TestFalse(TEXT("A callback removing the account cannot leave a successful registration result"), Second.Account->RegisterResourceAccount());
	int32 ReentrantNotifications = 0;
	Probe->AccountCallback = [Economy, &Second, &ReentrantNotifications](FGameplayTag Faction)
	{
		++ReentrantNotifications;
		Economy->RegisterFactionResourceAccount(Faction, Second.Controller);
	};
	TestTrue(TEXT("A listener may repeat registration without recursive notification"), Second.Account->RegisterResourceAccount());
	TestEqual(TEXT("Repeated registration callback emits one notification"), ReentrantNotifications, 1);
	Economy->OnFactionResourceAccountChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ResourceAccountChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFactionSwitchAccountAndUI,
	"TerritoryFramework.Factions.ResourceAccounts.NarrativeSwitchSaveAndUI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFactionSwitchAccountAndUI::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionAccountTests;
	FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	FPlayerFixture Player(Fixture.World, Heroes);
	Player.Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	auto* Economy = Fixture.World->GetSubsystem<UTerritoryEconomySubsystem>();
	TestTrue(TEXT("Player begins as the selected Heroes account"), Player.Account->RegisterResourceAccount());
	auto* Inventory = Player.Character->GetInventoryComponent();
	Inventory->SetCurrency(5000);
	Inventory->SetCapacity(16);
	Inventory->SetWeightCapacity(100.f);
	Inventory->TryAddItemFromClass(UTerritoryAuditResourceA::StaticClass(), 7, false);
	auto* Widget = NewObject<UTerritoryAuditEconomyWidget>(Player.Controller);
	Widget->SetOwningPlayer(Player.Controller);
	Widget->ConstructForAudit();
	TestEqual(TEXT("Automatic UI starts at Heroes"), Widget->GetDisplayFaction(), Heroes);
	auto* Event = NewObject<UTerritorySetNarrativePlayerFactionsEvent>();
	Event->NewFactions = FGameplayTagContainer(Bandits);
	TestTrue(TEXT("Native faction replacement succeeds"), Event->ApplyToPlayerState(Player.State));
	TestTrue(TEXT("Existing component rebinds through Narrative notification"), Player.Account->IsResourceAccountRegistered());
	TestEqual(TEXT("Component follows the new faction"), Player.Account->GetResourceAccountFaction(), Bandits);
	TestFalse(TEXT("The old explicit account was removed"), Economy->IsFactionResourceAccountSelected(Heroes, Player.Controller));
	TestTrue(TEXT("The new account is explicitly selected, not merely a fallback"), Economy->IsFactionResourceAccountSelected(Bandits, Player.Controller));
	TestEqual(TEXT("An existing economy screen follows replacement"), Widget->GetDisplayFaction(), Bandits);
	TestEqual(TEXT("Faction replacement retains personal currency"), Inventory->GetCurrency(), 5000);
	TestEqual(TEXT("Faction replacement retains real Narrative items"), Inventory->GetTotalQuantityOfItemExact(UTerritoryAuditResourceA::StaticClass(), false), 7);
	Widget->SetDisplayFaction(Heroes);
	TestEqual(TEXT("Explicit spectator view remains fixed"), Widget->GetDisplayFaction(), Heroes);
	Widget->SetDisplayFaction(FGameplayTag());
	TestEqual(TEXT("Empty selection resumes automatic view"), Widget->GetDisplayFaction(), Bandits);
	TArray<uint8> SavedState;
	FMemoryWriter Writer(SavedState);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, true);
	SaveArchive.ArIsSaveGame = true;
	Player.State->Serialize(SaveArchive);
	Inventory->PrepareForSave_Implementation();
	Player.State->SetFactions(FGameplayTagContainer(Heroes));
	Inventory->SetCurrency(1);
	FMemoryReader Reader(SavedState);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	Player.State->Serialize(LoadArchive);
	Inventory->Load_Implementation();
	Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>()->OnFinishedLoad.Broadcast();
	TestTrue(TEXT("Native load completion rebuilds explicit faction binding"), Economy->IsFactionResourceAccountSelected(Bandits, Player.Controller));
	TestEqual(TEXT("Saved Native faction is restored"), Player.Account->GetResourceAccountFaction(), Bandits);
	TestEqual(TEXT("Native inventory save retains the player's currency"), Inventory->GetCurrency(), 5000);
	TestEqual(TEXT("Load refreshes automatic UI"), Widget->GetDisplayFaction(), Bandits);
	Player.Account->BindingMode = ETerritoryResourceAccountBinding::FixedFaction;
	Player.Account->Faction = Bandits;
	Player.Account->RegisterResourceAccount();
	Player.State->SetFactions(FGameplayTagContainer(Heroes));
	TestFalse(TEXT("Fixed storage cannot follow a defector into another faction"), Player.Account->IsResourceAccountRegistered());
	TestFalse(TEXT("Fixed storage removes ineligible old registration"), Economy->IsFactionResourceAccountSelected(Bandits, Player.Controller));
	Widget->DestructForAudit();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPoliticalMembershipEvent,
	"TerritoryFramework.Factions.StoryEvent.AtomicPrimaryAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPoliticalMembershipEvent::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionAccountTests;
	FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	FPlayerFixture Player(Fixture.World, Heroes);
	auto* Event = NewObject<UTerritorySetNarrativePlayerFactionsEvent>();
	Event->NewFactions = FGameplayTagContainer(Bandits);
	Event->PrimaryFaction = Bandits;
	Event->bReplaceExistingFactions = false;
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	int32 Notifications = 0;
	Probe->FactionCallback = [&Notifications] { ++Notifications; };
	Player.Character->OnFactionUpdated.AddDynamic(Probe, &UTerritoryAuditEventProbe::FactionChanged);
	TestTrue(TEXT("Adding membership supports an explicit political faction"), Event->ApplyToPlayerState(Player.State));
	TestEqual(TEXT("Multiple memberships commit through one Native notification"), Notifications, 1);
	TestTrue(TEXT("Add preserves Heroes membership"), Player.State->GetFactions().HasTagExact(Heroes));
	TestEqual(TEXT("The authored political faction is primary"), UTerritoryBlueprintLibrary::GetActorPrimaryFaction(Fixture.World, Player.Controller), Bandits);
	const FGameplayTagContainer BeforeInvalid = Player.State->GetFactions();
	Event->PrimaryFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Police"));
	TestFalse(TEXT("Primary choice must be a final membership"), Event->ApplyToPlayerState(Player.State));
	Event->PrimaryFaction = FGameplayTag();
	Event->NewFactions.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith")));
	TestFalse(TEXT("A Territory tag cannot become faction membership"), Event->ApplyToPlayerState(Player.State));
	TestEqual(TEXT("Rejected event cannot partially mutate membership"), Player.State->GetFactions(), BeforeInvalid);
	TestEqual(TEXT("Rejected event does not broadcast success"), Notifications, 1);
	TArray<uint8> SavedMembership;
	FMemoryWriter Writer(SavedMembership);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, true);
	SaveArchive.ArIsSaveGame = true;
	Player.State->Serialize(SaveArchive);
	Player.State->SetFactions(FGameplayTagContainer(Heroes));
	FMemoryReader Reader(SavedMembership);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	Player.State->Serialize(LoadArchive);
	TestEqual(TEXT("Native save preserves the authored primary membership order"), UTerritoryBlueprintLibrary::GetActorPrimaryFaction(Fixture.World, Player.Controller), Bandits);
	TestTrue(TEXT("Native save preserves additional memberships"), Player.State->GetFactions().HasTagExact(Heroes));
	Event->NewFactions = FGameplayTagContainer(Heroes);
	Player.State->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("The story helper rejects non-authoritative PlayerState"), Event->ApplyToPlayerState(Player.State));
	Player.State->SetRole(ROLE_Authority);
	TestFalse(TEXT("Narrative faction root cannot be used as identity"), UTerritoryBlueprintLibrary::IsNarrativeFactionTag(FNarrativeGameplayTags::Get().Narrative_Factions));
	TestFalse(TEXT("Universal hostility is not territory ownership identity"), UTerritoryBlueprintLibrary::IsPoliticalFactionTag(FNarrativeGameplayTags::Get().Narrative_Factions_HostileAll));
	Player.State->RemoveFaction(Heroes);
	Player.State->RemoveFaction(Bandits);
	TestFalse(TEXT("An intentionally empty Native membership does not invent Heroes"), UTerritoryBlueprintLibrary::GetActorPrimaryFaction(Fixture.World, Player.Controller).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFMixedFactionDialogue,
	"TerritoryFramework.Factions.Dialogue.SharedMembershipCannotHideWar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFMixedFactionDialogue::RunTest(const FString& Parameters)
{
	using namespace TerritoryFactionAccountTests;
	FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Guard = Fixture.World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Guard)->AddFaction(Bandits);
	FPlayerFixture Player(Fixture.World, Heroes);
	Player.State->AddFaction(Bandits);
	auto* Diplomacy = Fixture.World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	Diplomacy->DeclareWar(Heroes, Bandits);
	bool bSameFaction = false;
	auto* Dialogue = Guard->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>();
	TestEqual(TEXT("Mixed membership dialogue keeps the hostile pair"), Dialogue->ResolveRelationshipForInteractor(Player.Character, bSameFaction), EDiplomacyState::War);
	TestFalse(TEXT("Same-faction dialogue cannot override War dialogue"), bSameFaction);
	TestEqual(TEXT("Native combat agrees that the mixed membership is hostile"), Fixture.World->GetGameState<ANarrativeGameState>()->GetFactionsAttitudeTowardsFactions(Guard->GetFactions(), Player.State->GetFactions()), ETeamAttitude::Hostile);
	Diplomacy->FormAlliance(Heroes, Bandits);
	TestEqual(TEXT("A friendly shared membership still gets Alliance"), Dialogue->ResolveRelationshipForInteractor(Player.Character, bSameFaction), EDiplomacyState::Alliance);
	TestTrue(TEXT("The genuine same-faction branch remains available in peace"), bSameFaction);
	return true;
}

#endif
