#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeASCActor.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryGameplayStateTask.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryGameplayStateExactTags,
	"TerritoryFramework.Tales.Regression.ExactTagsUseExplicitOwnershipAndChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryGameplayStateExactTags::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Gameplay task world"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Actor = World->SpawnActor<ANarrativeASCActor>();
	auto* Owner = World->SpawnActor<AActor>();
	auto* Tales = NewObject<UTalesComponent>(Owner);
	auto* ASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Actor);
	if (!TestNotNull(TEXT("Real Native ASC"), ASC)) { World->DestroyWorld(false); return false; }
	const FGameplayTag Parent = FGameplayTag::RequestGameplayTag(TEXT("Narrative.State"));
	const FGameplayTag Child = FGameplayTag::RequestGameplayTag(TEXT("Narrative.State.Stumbling"));
	ASC->AddLooseGameplayTag(Child);
	const auto MakeTask = [&](ETerritoryGameplayStateObjective Objective, bool Exact, bool CountInitial)
	{
		auto* Task = NewObject<UTerritoryGameplayStateTask>(Tales);
		Task->OwningComp = Tales;
		Task->Objective = Objective;
		Task->RequiredTags.AddTag(Parent);
		Task->bExactTagMatch = Exact;
		Task->bCompleteIfAlreadySatisfied = CountInitial;
		auto* Provider = NewObject<UNarrativeActorProvider_LevelReference>(Task);
		Provider->SoftActorReference = Actor;
		Task->SubjectProvider = Provider;
		Task->MarkerSettings.bAddNavigationMarker = false;
		Task->BeginTask();
		Task->HandleProviderActorReady(Actor);
		return Task;
	};
	auto* Hierarchical = MakeTask(ETerritoryGameplayStateObjective::AllTagsPresent, false, true);
	TestTrue(TEXT("Normal matching accepts a child tag"), Hierarchical->IsComplete());
	Hierarchical->EndTask();
	auto* Exact = MakeTask(ETerritoryGameplayStateObjective::AllTagsPresent, true, true);
	TestFalse(TEXT("Exact parent match rejects an implied parent"), Exact->IsComplete());
	TestTrue(TEXT("Aggregate parent count is nevertheless positive"), ASC->GetTagCount(Parent) > 0);
	ASC->AddLooseGameplayTag(Parent);
	TestTrue(TEXT("Explicit parent addition completes without removing the child"), Exact->IsComplete());
	Exact->EndTask();
	TestFalse(TEXT("End removes the exact tag listener"), ASC->RegisterGameplayTagEvent(
		Parent, EGameplayTagEventType::AnyCountChange).IsBoundToObject(Exact));

	auto* Removed = MakeTask(ETerritoryGameplayStateObjective::AllTagsAbsent, true, true);
	TestFalse(TEXT("Explicit parent blocks the absence objective"), Removed->IsComplete());
	TestTrue(TEXT("Exact removal listens for count changes"), ASC->RegisterGameplayTagEvent(
		Parent, EGameplayTagEventType::AnyCountChange).IsBoundToObject(Removed));
	ASC->RemoveLooseGameplayTag(Parent);
	TestTrue(TEXT("Absence preview excludes the implied parent after removal"), Removed->IsGameplayStateSatisfiedBy(Actor));
	// Engine versions that omit this removal callback still complete from the
	// bounded state reconciliation. A future engine callback fix is also valid.
	Removed->TickTask_Implementation();
	TestTrue(TEXT("Explicit removal completes even with a child still held"), Removed->IsComplete());
	TestTrue(TEXT("Child state is preserved"), ASC->HasMatchingGameplayTag(Child));
	Removed->EndTask();

	auto* ChangedSubject = MakeTask(ETerritoryGameplayStateObjective::AllTagsPresent, true, false);
	ChangedSubject->HandleProviderActorReady(nullptr);
	ASC->AddLooseGameplayTag(Parent);
	TestFalse(TEXT("A missing provider subject cannot complete from the old ASC"), ChangedSubject->IsComplete());
	TestFalse(TEXT("Missing subject removes its previous listener"), ASC->RegisterGameplayTagEvent(
		Parent, EGameplayTagEventType::AnyCountChange).IsBoundToObject(ChangedSubject));
	ChangedSubject->HandleProviderActorReady(Actor);
	ChangedSubject->TickTask_Implementation();
	TestFalse(TEXT("Late binding respects require-new-transition authoring"), ChangedSubject->IsComplete());
	ASC->RemoveLooseGameplayTag(Parent);
	ASC->AddLooseGameplayTag(Parent);
	TestTrue(TEXT("Reappearing subject observes its next real transition"), ChangedSubject->IsComplete());
	ChangedSubject->EndTask();
	ASC->RemoveLooseGameplayTag(Parent);

	Owner->SetRole(ROLE_SimulatedProxy);
	auto* ClientTask = MakeTask(ETerritoryGameplayStateObjective::AllTagsPresent, true, false);
	ASC->AddLooseGameplayTag(Parent);
	TestTrue(TEXT("Client can read the exact state"), ClientTask->IsGameplayStateSatisfiedBy(Actor));
	TestEqual(TEXT("Native authority check prevents client progress mutation"), ClientTask->CurrentProgress, 0);
	ClientTask->EndTask();
	Owner->SetRole(ROLE_Authority);

	// Native restores task progress through SetProgress while Tales is loading;
	// these new transient listeners must neither reset nor add to that value.
	auto* Restored = MakeTask(ETerritoryGameplayStateObjective::AllTagsPresent, true, false);
	Restored->RequiredQuantity = 3;
	Tales->bIsLoading = true;
	Restored->SetProgress(2);
	Restored->EndTask();
	Restored->BeginTask();
	Restored->HandleProviderActorReady(Actor);
	Tales->bIsLoading = false;
	TestEqual(TEXT("Native load preserves progress while listeners are rebuilt"), Restored->CurrentProgress, 2);
	Restored->EndTask();
	ASC->RemoveLooseGameplayTag(Parent);
	ASC->RemoveLooseGameplayTag(Child);
	World->DestroyWorld(false);
	return true;
}

#endif
