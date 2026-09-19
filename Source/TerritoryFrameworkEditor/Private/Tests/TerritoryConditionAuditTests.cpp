#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Tales/TerritoryConditionGroup.h"
#include "Tales/TerritoryOwnershipCondition.h"
#include "Tales/TerritoryStoryConditions.h"
#include "Tales/TerritoryStoryEvents.h"
#include "Tales/TerritoryLockEvent.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/TalesComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "NarrativeSave.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/UnrealType.h"

namespace TerritoryConditionAudit
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
	}
	~FWorldFixture()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStateConditionLockAndStreaming,
	"TerritoryFramework.Tales.Regression.StateLockEventsSaveStreamingAndClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFStateConditionLockAndStreaming::RunTest(const FString&)
{
	TerritoryConditionAudit::FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag Owner = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = Tag;
	Definition->StableTerritoryGUID = FGuid(907, 908, 909, 910);
	Definition->InitialGuardCount = 0;
	auto* Territory = Fixture.World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Territory);
	auto* Registry = Fixture.World->GetSubsystem<UTerritoryRegistrySubsystem>();
	Registry->RegisterTerritory(Territory);
	FTerritoryOwnershipData Data = Territory->GetOwnershipData();
	Data.State = ETerritoryState::Claimed;
	Data.OwningFaction = Owner;
	Data.Availability = ETerritoryAvailability::Unlocked;
	Territory->CommitOwnershipData(Data);
	auto* Tales = NewObject<UTalesComponent>(Territory);
	auto* Condition = NewObject<UTerritoryStateCondition>(Territory);
	Condition->TerritoryToCheck = Tag;
	Condition->RequiredState = ETerritoryState::Locked;
	auto* Lock = NewObject<UTerritoryLockEvent>(Territory);
	Lock->TargetTerritoryTag = Tag;
	Lock->ExecuteEvent(nullptr, nullptr, Tales);
	TestTrue(TEXT("Native lock event sets the availability lock"), Territory->IsLocked());
	TestEqual(TEXT("Lock keeps political ownership"), Territory->GetTerritoryState(), ETerritoryState::Claimed);
	TestTrue(TEXT("Existing serialized Locked condition now passes"), Condition->CheckCondition(nullptr, nullptr, Tales));
	auto* Node = NewObject<UNarrativeNodeBase>(Territory);
	Node->Conditions = {Condition};
	TestTrue(TEXT("Native dialogue node sees the corrected lock check"), Node->AreConditionsMet(nullptr, nullptr, Tales));
	Condition->bNot = true;
	TestFalse(TEXT("Native Not reverses the raw lock result once"), Node->AreConditionsMet(nullptr, nullptr, Tales));
	Condition->bNot = false;
	Condition->RequiredState = ETerritoryState::Claimed;
	TestTrue(TEXT("Claimed and Locked can both be true"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Condition->Query = ETerritoryStateConditionQuery::Availability;
	Condition->RequiredAvailability = ETerritoryAvailability::Locked;
	TestTrue(TEXT("New local lock query agrees"), Condition->CheckCondition(nullptr, nullptr, Tales));
	const auto Saved = Territory->GetOwnershipData();
	auto* Unlock = NewObject<UTerritoryUnlockEvent>(Territory);
	Unlock->TargetTerritoryTag = Tag;
	Unlock->UnlockScope = ETerritoryUnlockScope::ForceExact;
	Unlock->ExecuteEvent(nullptr, nullptr, Tales);
	TestFalse(TEXT("Unlock event makes Locked condition fail"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Territory->CommitOwnershipData(Saved);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	SaveArchive.ArIsSaveGame = true;
	Territory->Serialize(SaveArchive);
	Territory->CommitOwnershipData(Data);
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	Territory->Serialize(LoadArchive);
	TestTrue(TEXT("Saved availability is read without treating Claimed as unlocked"), Condition->CheckCondition(nullptr, nullptr, Tales));
	auto* Directory = Fixture.World->SpawnActor<ATerritoryWorldState>();
	Directory->PublishTerritorySummary(Territory);
	Registry->UnregisterTerritory(Territory);
	TestFalse(TEXT("Loaded-only compatibility default fails after streaming out"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Condition->bAllowUnloadedTerritory = true;
	TestTrue(TEXT("Opt-in campaign projection preserves lock while actor is absent"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Condition->Query = ETerritoryStateConditionQuery::Loaded;
	TestFalse(TEXT("A directory row never pretends an actor is loaded"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Condition->Query = ETerritoryStateConditionQuery::Known;
	TestTrue(TEXT("Known query accepts the existing campaign row"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Condition->TerritoryToCheck = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.CastleHill"));
	TestFalse(TEXT("Missing directory rows are unknown rather than Unclaimed"), Condition->CheckCondition(nullptr, nullptr, Tales));
	Condition->TerritoryToCheck = Tag;
	Registry->RegisterTerritory(Territory);
	Territory->SetRole(ROLE_SimulatedProxy);
	Condition->Query = ETerritoryStateConditionQuery::Availability;
	Unlock->ExecuteEvent(nullptr, nullptr, Tales);
	TestTrue(TEXT("Client-role actor rejects unlock event mutation"), Territory->IsLocked());
	TestTrue(TEXT("Replicated ownership can be read by a client-role condition"), Condition->CheckCondition(nullptr, nullptr, Tales));
	FTerritoryGarrisonSnapshot Snapshot;
	Snapshot.ReserveGuards = 17;
	Snapshot.PendingDeployments = 3;
	auto* SnapshotProperty = FindFProperty<FStructProperty>(Territory->GetClass(), TEXT("GarrisonSnapshot"));
	*SnapshotProperty->ContainerPtrToValuePtr<FTerritoryGarrisonSnapshot>(Territory) = Snapshot;
	auto* Garrison = NewObject<UTerritoryGarrisonCondition>(Territory);
	Garrison->TerritoryToCheck = Tag;
	Garrison->Metric = ETerritoryGarrisonMetric::RemainingReserve;
	Garrison->Comparison = ETerritoryIntegerComparison::Equal;
	Garrison->Value = 17;
	TestTrue(TEXT("Client reserves use replicated counts even with no guard post actors"), Garrison->CheckCondition(nullptr, nullptr, Tales));
	Garrison->Metric = ETerritoryGarrisonMetric::PendingReserveDeployments;
	Garrison->Value = 3;
	TestTrue(TEXT("Client pending replacements use the replicated count"), Garrison->CheckCondition(nullptr, nullptr, Tales));
	Territory->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFConditionGroupBehavior,
	"TerritoryFramework.Tales.Regression.NestedAllAnyNotAndCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFConditionGroupBehavior::RunTest(const FString&)
{
	auto* Group = NewObject<UTerritoryConditionGroup>();
	auto* Yes = NewObject<UTerritoryAuditCondition>(Group);
	auto* No = NewObject<UTerritoryAuditCondition>(Group);
	Yes->Callback = [] { return true; };
	No->Callback = [] { return false; };
	auto* Tales = NewObject<UTalesComponent>();
	TestFalse(TEXT("Empty groups fail"), Group->CheckCondition(nullptr, nullptr, Tales));
	Group->Conditions = {Yes, No};
	TestFalse(TEXT("All rejects one failing child"), Group->CheckCondition(nullptr, nullptr, Tales));
	Group->Match = ETerritoryConditionGroupMatch::Any;
	TestTrue(TEXT("Any accepts one passing child"), Group->CheckCondition(nullptr, nullptr, Tales));
	Group->Match = ETerritoryConditionGroupMatch::All;
	No->bNot = true;
	TestTrue(TEXT("Child Not works through Native evaluation"), Group->CheckCondition(nullptr, nullptr, Tales));
	auto* Outer = NewObject<UTerritoryConditionGroup>();
	Outer->Conditions = {Group, Yes};
	TestTrue(TEXT("Nested groups use original condition semantics"), Outer->CheckCondition(nullptr, nullptr, Tales));
	Group->Conditions.Add(Outer);
	TestFalse(TEXT("Circular nested groups fail without recursion"), Outer->CheckCondition(nullptr, nullptr, Tales));
	Group->Conditions = {Yes, No};
	No->bNot = false;
	Yes->Callback = [Group] { Group->Conditions.Reset(); return true; };
	TestFalse(TEXT("A callback cannot erase later requirements from the current group"), Group->CheckCondition(nullptr, nullptr, Tales));
	Group->Conditions = {Yes, nullptr};
	TestFalse(TEXT("Null rows fail rather than allowing a reward"), Group->CheckCondition(nullptr, nullptr, Tales));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTalesModularWaveFilter,
	"TerritoryFramework.Tales.Regression.WaveCancellationStoryFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFTalesModularWaveFilter::RunTest(const FString&)
{
	auto* Event = NewObject<UTerritoryCancelEnemyWavesEvent>();
	FTerritoryAssaultRecord Record;
	Record.State = ETerritoryAssaultState::ScheduledWarning;
	Record.StoryScenarioID = TEXT("Retake");
	Record.AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	TestTrue(TEXT("Empty filters preserve existing cancellation selection"), Event->MatchesAssault(Record));
	Event->ScenarioID = TEXT("BossEscape");
	TestFalse(TEXT("Unrelated story encounter is retained"), Event->MatchesAssault(Record));
	Event->ScenarioID = TEXT("Retake");
	TestTrue(TEXT("Matching story encounter is selected"), Event->MatchesAssault(Record));
	Record.State = ETerritoryAssaultState::Active;
	TestFalse(TEXT("Active force remains protected by default"), Event->MatchesAssault(Record));
	Event->bIncludePhysicallyActiveAssaults = true;
	TestTrue(TEXT("Explicit active cancellation permits the matching force"), Event->MatchesAssault(Record));
	Event->AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	TestFalse(TEXT("Faction and story filters must both pass"), Event->MatchesAssault(Record));
	Event->AttackingFaction = Record.AttackingFaction;
	Record.State = ETerritoryAssaultState::Defeated;
	TestFalse(TEXT("A completed encounter cannot be cancelled again"), Event->MatchesAssault(Record));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTalesDynamicReputation,
	"TerritoryFramework.Tales.Regression.ReputationFollowsNarrativeFaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFTalesDynamicReputation::RunTest(const FString&)
{
	TerritoryConditionAudit::FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const auto Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const auto Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* State = Fixture.World->SpawnActor<ANarrativePlayerState>();
	State->SetFactions(FGameplayTagContainer(Heroes));
	auto* Pawn = NewObject<ANarrativePlayerCharacter>(Fixture.World->PersistentLevel);
	Pawn->SetRole(ROLE_Authority);
	Pawn->SetPlayerState(State);
	auto* Controller = NewObject<ANarrativePlayerController>(Fixture.World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	Controller->SetPlayerState(State);
	Controller->SetOwnedCharacter(Pawn);
	Controller->SetPawn(Pawn);
	auto* Tales = NewObject<UTalesComponent>(Controller);
	auto* Diplomacy = Fixture.World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	auto* Event = NewObject<UTerritoryModifyReputationEvent>();
	Event->Faction = Bandits;
	Event->FactionSource = ETerritoryCaptureFactionSource::ControllerPawnFaction;
	Event->Value = 12;
	Event->ExecuteEvent(Pawn, Controller, Tales);
	TestEqual(TEXT("Dynamic reputation credits the player's current Heroes faction"), Diplomacy->GetReputation(Heroes), 12);
	TestEqual(TEXT("Unused explicit fallback does not receive reputation"), Diplomacy->GetReputation(Bandits), 0);
	auto* Condition = NewObject<UTerritoryReputationCondition>();
	Condition->FactionSource = ETerritoryCaptureFactionSource::NarrativeTargetFaction;
	Condition->Value = 12;
	TestTrue(TEXT("Dynamic condition reads the same faction"), Condition->CheckCondition(Pawn, Controller, Tales));
	State->SetFactions(FGameplayTagContainer(Bandits));
	TestFalse(TEXT("Betrayal immediately changes the faction being checked"), Condition->CheckCondition(Pawn, Controller, Tales));
	Event->ExecuteEvent(Pawn, Controller, Tales);
	TestEqual(TEXT("Following event credits the newly adopted faction"), Diplomacy->GetReputation(Bandits), 12);
	TestEqual(TEXT("Old faction reputation is preserved"), Diplomacy->GetReputation(Heroes), 12);
	Event->ExecuteEvent(nullptr, nullptr, nullptr);
	TestEqual(TEXT("Missing dynamic context cannot credit the explicit fallback"), Diplomacy->GetReputation(Bandits), 12);
	auto* Deny = NewObject<UTerritoryAuditCondition>(Event);
	Deny->Callback = [] { return false; };
	Event->Conditions = {Deny};
	Event->ExecuteEvent(Pawn, Controller, Tales);
	TestEqual(TEXT("Inherited event conditions block the mutation"), Diplomacy->GetReputation(Bandits), 12);
	auto* Directory = Fixture.World->SpawnActor<ATerritoryWorldState>();
	Directory->ExportPersistentState();
	Diplomacy->SetReputation(Bandits, 0);
	Directory->ImportPersistentState();
	TestTrue(TEXT("Restored reputation still satisfies the current faction condition"), Condition->CheckCondition(Pawn, Controller, Tales));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFOwnershipWaitsForParticipantFaction,
	"TerritoryFramework.Tales.Regression.OwnershipWaitsForParticipantFaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTFOwnershipWaitsForParticipantFaction::RunTest(const FString&)
{
	TerritoryConditionAudit::FWorldFixture Fixture;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const auto Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const auto Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	const auto Tag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = Tag;
	Definition->StableTerritoryGUID = FGuid(301, 302, 303, 304);
	Definition->InitialGuardCount = 0;
	auto* Place = Fixture.World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Place);
	auto* Registry = Fixture.World->GetSubsystem<UTerritoryRegistrySubsystem>();
	Registry->RegisterTerritory(Place);
	FTerritoryOwnershipData Data = Place->GetOwnershipData();
	Data.State = ETerritoryState::Claimed;
	Data.OwningFaction = Bandits;
	TestTrue(TEXT("Fixture has a real claimed owner"), Place->CommitOwnershipData(Data));
	auto* Condition = NewObject<UTerritoryOwnershipCondition>(Place);
	Condition->TerritoryToCheck = Tag;
	auto* Node = NewObject<UNarrativeNodeBase>(Place);
	Node->Conditions = {Condition};
	auto* Pawn = NewObject<ANarrativePlayerCharacter>(Fixture.World->PersistentLevel);
	Pawn->SetRole(ROLE_Authority);
	auto* Controller = NewObject<ANarrativePlayerController>(Fixture.World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	Controller->SetOwnedCharacter(Pawn);
	Controller->SetPawn(Pawn);
	auto* Tales = NewObject<UTalesComponent>(Controller);
	TestTrue(TEXT("A deliberate world-only check keeps its existing meaning"), Condition->CheckCondition(nullptr, nullptr, nullptr));
	TestFalse(TEXT("A pawn without player state cannot accept an unrelated owner"), Condition->CheckCondition(Pawn, nullptr, nullptr));
	TestFalse(TEXT("A controller without player state cannot accept an unrelated owner"), Condition->CheckCondition(nullptr, Controller, nullptr));
	TestFalse(TEXT("Tales-only context is still a participant, not a world-only check"), Condition->CheckCondition(nullptr, nullptr, Tales));
	TestFalse(TEXT("Native node evaluation rejects a participant whose faction is not ready"), Node->AreConditionsMet(Pawn, Controller, Tales));
	auto* State = Fixture.World->SpawnActor<ANarrativePlayerState>();
	Pawn->SetPlayerState(State);
	Controller->SetPlayerState(State);
	State->SetFactions(FGameplayTagContainer(Heroes));
	TestFalse(TEXT("Initialized Heroes cannot pass Bandit ownership"), Condition->CheckCondition(nullptr, nullptr, Tales));
	State->SetFactions(FGameplayTagContainer(Bandits));
	TestTrue(TEXT("Adopting the owning faction passes through the Narrative node"), Node->AreConditionsMet(Pawn, Controller, Tales));
	FNarrativeActorRecord Record;
	auto* Save = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
	TestTrue(TEXT("Ownership is saved through Narrative"), Save->CreateActorRecord(Place, Record));
	Data.OwningFaction = Heroes;
	Place->CommitOwnershipData(Data);
	TestFalse(TEXT("A changed owner updates the condition"), Condition->CheckCondition(Pawn, Controller, Tales));
	Save->LoadActorFromRecord(Place, Record);
	TestTrue(TEXT("Restored ownership updates the condition"), Condition->CheckCondition(Pawn, Controller, Tales));
	Registry->UnregisterTerritory(Place);
	AddExpectedError(TEXT("not found in registry"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Unloaded territory fails even for a valid participant"), Condition->CheckCondition(Pawn, Controller, Tales));
	Registry->RegisterTerritory(Place);
	Place->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("Client-role reads use the supplied participant and replicated owner"), Condition->CheckCondition(Pawn, Controller, Tales));
	// Native SetFactions ignores an empty container; use its supported removal API.
	State->RemoveFaction(Bandits);
	TestFalse(TEXT("Losing faction context fails again without retaining a previous answer"), Condition->CheckCondition(Pawn, Controller, Tales));
	Condition->RequiredOwner = Bandits;
	TestTrue(TEXT("An explicit authored owner does not need a participant faction"), Condition->CheckCondition(Pawn, Controller, Tales));
	Place->SetRole(ROLE_Authority);
	return true;
}

#endif
