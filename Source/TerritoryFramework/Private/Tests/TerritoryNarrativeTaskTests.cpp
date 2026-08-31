#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Tales/QuestTask.h"
#include "Tales/TerritoryAIObservationTask.h"
#include "Tales/TerritoryAssaultTask.h"
#include "Tales/TerritoryCaptureTask.h"
#include "Tales/TerritoryCharacterActionTask.h"
#include "Tales/TerritoryCombatProgressTask.h"
#include "Tales/TerritoryDisguiseTask.h"
#include "Tales/TerritoryGameplayStateTask.h"
#include "Tales/TerritoryStateTask.h"
#include "UnrealFramework/NarrativeCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryNarrativeTaskContract,
	"TerritoryFramework.Tales.Tasks.NarrativeContractAndMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryNarrativeTaskContract::RunTest(const FString& Parameters)
{
	for (const UClass* TaskClass : {
		UTerritoryCaptureTask::StaticClass(),
		UTerritoryStateTask::StaticClass(),
		UTerritoryAssaultTask::StaticClass(),
		UTerritoryDisguiseTask::StaticClass(),
		UTerritoryCharacterActionTask::StaticClass(),
		UTerritoryGameplayStateTask::StaticClass(),
		UTerritoryCombatProgressTask::StaticClass(),
		UTerritoryAIObservationTask::StaticClass() })
	{
		TestTrue(*FString::Printf(TEXT("%s inherits Narrative Task"),
			*TaskClass->GetName()), TaskClass->IsChildOf(UNarrativeTask::StaticClass()));
		TestTrue(*FString::Printf(TEXT("%s is inline authorable"),
			*TaskClass->GetName()), TaskClass->HasAnyClassFlags(CLASS_EditInlineNew));
		#if WITH_EDITOR
		TestFalse(*FString::Printf(TEXT("%s has useful class help"),
			*TaskClass->GetName()), TaskClass->GetMetaData(TEXT("ToolTip")).IsEmpty());
		#endif
	}

	const UClass* StateTaskClass = UTerritoryStateTask::StaticClass();
	TestNotNull(TEXT("State task exposes one Territory target"),
		StateTaskClass->FindPropertyByName(TEXT("TargetTerritory")));
	TestNotNull(TEXT("State task exposes an objective"),
		StateTaskClass->FindPropertyByName(TEXT("Objective")));
	TestNotNull(TEXT("State task exposes a read-only preview query"),
		StateTaskClass->FindFunctionByName(TEXT("IsObjectiveSatisfiedBy")));
	TestNotNull(TEXT("Movement task exposes a read-only state preview"),
		UTerritoryCharacterActionTask::StaticClass()->FindFunctionByName(
			TEXT("IsActionStateSatisfiedBy")));
	TestNotNull(TEXT("GAS task exposes a read-only state preview"),
		UTerritoryGameplayStateTask::StaticClass()->FindFunctionByName(
			TEXT("IsGameplayStateSatisfiedBy")));
	TestNotNull(TEXT("AI task exposes a read-only state preview"),
		UTerritoryAIObservationTask::StaticClass()->FindFunctionByName(
			TEXT("IsAIStateSatisfiedBy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryNarrativeStateTaskEvaluation,
	"TerritoryFramework.Tales.Tasks.StateObjectiveEvaluationIsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryNarrativeStateTaskEvaluation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Territory task test world exists"), World)) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Territory task test Place exists"), Territory))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	if (!TestTrue(TEXT("Territory task fixture tags exist"),
		TerritoryTag.IsValid() && Bandits.IsValid() && Heroes.IsValid()))
	{
		World->DestroyWorld(false);
		return false;
	}

	UTerritoryPlaceDefinition* Definition =
		NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = TerritoryTag;
	Definition->DisplayName = FText::FromString(TEXT("Blacksmith"));
	Definition->StableTerritoryGUID = FGuid::NewGuid();
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	TestTrue(TEXT("Task fixture Definition applies to its Place"),
		Definition->ApplyToTerritory(Territory));

	UTerritoryStateTask* Task = NewObject<UTerritoryStateTask>();
	Task->TargetTerritory = TerritoryTag;
	Task->Objective = ETerritoryStateTaskObjective::BecomeUnclaimed;
	TestTrue(TEXT("Unclaimed objective reads the current political state"),
		Task->IsObjectiveSatisfiedBy(Territory));

	FTerritoryOwnershipData Data = Territory->GetOwnershipData();
	Data.OwningFaction = Bandits;
	Data.ContestingFaction = Heroes;
	Data.State = ETerritoryState::Contested;
	Data.Availability = ETerritoryAvailability::Unlocked;
	Data.ControlProgress = 0.25f;
	TestTrue(TEXT("Task fixture enters Contested through the real Territory commit"),
		Territory->CommitOwnershipData(Data));
	Task->Objective = ETerritoryStateTaskObjective::BecomeContested;
	TestTrue(TEXT("Contested objective reads the committed state"),
		Task->IsObjectiveSatisfiedBy(Territory));
	Task->Objective = ETerritoryStateTaskObjective::BecomeClaimed;
	TestFalse(TEXT("Claimed objective does not accept Contested"),
		Task->IsObjectiveSatisfiedBy(Territory));

	Task->Objective = ETerritoryStateTaskObjective::EnterTerritory;
	TestTrue(TEXT("Generated description uses the friendly Territory name"),
		Task->GetTaskDescription().ToString().Contains(TEXT("Blacksmith")));
	TestEqual(TEXT("Objective checks never mutate Territory state"),
		Territory->GetTerritoryState(), ETerritoryState::Contested);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryTaskOutcomes,
	"TerritoryFramework.Tales.Tasks.StoryBossAndChaseOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryTaskOutcomes::RunTest(const FString& Parameters)
{
	const FGameplayTag TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	if (!TestTrue(TEXT("Story fixture tags exist"),
		TerritoryTag.IsValid() && Bandits.IsValid())) return false;

	FTerritoryAssaultRecord Record;
	Record.TargetTerritory = TerritoryTag;
	Record.AttackingFaction = Bandits;
	Record.LaunchMode = ETerritoryAssaultLaunchMode::StoryPursuit;
	Record.StoryScenarioID = TEXT("Blacksmith_Underboss");
	Record.State = ETerritoryAssaultState::Defeated;
	Record.Resolution = ETerritoryAssaultResolution::AllAttackersRemoved;
	Record.KilledForce = 1;
	Record.WithdrawnForce = 2;

	UTerritoryAssaultTask* Task = NewObject<UTerritoryAssaultTask>();
	Task->TargetTerritory = TerritoryTag;
	Task->AttackingFaction = Bandits;
	Task->ScenarioID = TEXT("Blacksmith_Underboss");
	Task->RequiredQuantity = 1;
	Task->Objective = ETerritoryAssaultTaskObjective::StoryBossDefeated;
	TestTrue(TEXT("Finite story boss death satisfies the boss objective"),
		Task->IsObjectiveSatisfiedByRecord(Record));

	Record.LaunchMode = ETerritoryAssaultLaunchMode::StrategicCounterattack;
	TestFalse(TEXT("An ordinary counterattack cannot spoof a story boss task"),
		Task->IsObjectiveSatisfiedByRecord(Record));
	Record.LaunchMode = ETerritoryAssaultLaunchMode::StoryPursuit;
	Record.Resolution = ETerritoryAssaultResolution::TargetEscaped;
	Task->Objective = ETerritoryAssaultTaskObjective::StoryTargetReachedExit;
	TestTrue(TEXT("Road-exit escape is distinguished from distance loss"),
		Task->IsObjectiveSatisfiedByRecord(Record));
	Task->Objective = ETerritoryAssaultTaskObjective::StoryChaseDistanceLost;
	TestFalse(TEXT("Road-exit escape does not satisfy distance-loss branch"),
		Task->IsObjectiveSatisfiedByRecord(Record));
	Record.Resolution = ETerritoryAssaultResolution::ChaseDistanceLost;
	TestTrue(TEXT("Distance loss satisfies only its explicit branch"),
		Task->IsObjectiveSatisfiedByRecord(Record));

	Task->Objective = ETerritoryAssaultTaskObjective::WithdrawnAttackers;
	TestEqual(TEXT("Withdrawal progress follows the durable record"),
		Task->GetObjectiveProgressFromRecord(Record), 2);
	Task->ScenarioID = TEXT("Another_Scenario");
	TestEqual(TEXT("A different Scenario ID contributes no progress"),
		Task->GetObjectiveProgressFromRecord(Record), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCommunityTaskReadOnlyQueries,
	"TerritoryFramework.Tales.Tasks.CommunityReadOnlyQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCommunityTaskReadOnlyQueries::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Positive fractional combat magnitude still counts one"),
		UTerritoryCombatProgressTask::MagnitudeToProgress(0.25f), 1);
	TestEqual(TEXT("Combat magnitude rounds to Narrative integer progress"),
		UTerritoryCombatProgressTask::MagnitudeToProgress(19.6f), 20);
	TestEqual(TEXT("Negative combat magnitude contributes nothing"),
		UTerritoryCombatProgressTask::MagnitudeToProgress(-10.f), 0);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Community task test world exists"), World)) return false;
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ANarrativeCharacter* Character = World->SpawnActor<ANarrativeCharacter>(
		ANarrativeCharacter::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Narrative Character fixture exists"), Character))
	{
		World->DestroyWorld(false);
		return false;
	}

	UTerritoryCharacterActionTask* MovementTask =
		NewObject<UTerritoryCharacterActionTask>();
	MovementTask->Objective = ETerritoryCharacterActionObjective::Crouch;
	const bool bOriginalCrouched = Character->bIsCrouched;
	TestEqual(TEXT("Movement preview reads crouch without changing it"),
		MovementTask->IsActionStateSatisfiedBy(Character), bOriginalCrouched);
	TestEqual(TEXT("Movement preview leaves the Character unchanged"),
		Character->bIsCrouched, bOriginalCrouched);

	const FGameplayTag TestTag = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.State.Stumbling"), false);
	UNarrativeAbilitySystemComponent* AbilitySystem =
		Character->GetNarrativeAbilitySystemComponent();
	if (TestTrue(TEXT("GAS task fixture is available"),
		TestTag.IsValid() && AbilitySystem != nullptr))
	{
		UTerritoryGameplayStateTask* GameplayTask =
			NewObject<UTerritoryGameplayStateTask>();
		GameplayTask->Objective = ETerritoryGameplayStateObjective::AllTagsPresent;
		GameplayTask->RequiredTags.AddTag(TestTag);
		AbilitySystem->AddLooseGameplayTag(TestTag);
		TestTrue(TEXT("GAS preview reads a real ASC-owned tag"),
			GameplayTask->IsGameplayStateSatisfiedBy(Character));
		TestTrue(TEXT("GAS preview does not remove the observed tag"),
			AbilitySystem->HasMatchingGameplayTag(TestTag));
		AbilitySystem->RemoveLooseGameplayTag(TestTag);
	}

	UTerritoryAIObservationTask* AITask =
		NewObject<UTerritoryAIObservationTask>();
	AITask->Objective = ETerritoryAIObservationObjective::ActorAvailable;
	TestTrue(TEXT("AI Actor Available accepts a live provided actor"),
		AITask->IsAIStateSatisfiedBy(Character));
	AITask->Objective = ETerritoryAIObservationObjective::ReachLocation;
	AITask->DestinationLocation = Character->GetActorLocation();
	AITask->DistanceTolerance = 1.f;
	TestTrue(TEXT("AI location preview uses the observed actor position"),
		AITask->IsAIStateSatisfiedBy(Character));
	TestEqual(TEXT("AI preview does not move the observed actor"),
		Character->GetActorLocation(), FVector::ZeroVector);

	World->DestroyWorld(false);
	return true;
}

#endif
