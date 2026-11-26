// Copyright (c) Hop Trendy. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "NarrativeStableActor.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "Tales/Dialogue.h"
#include "Tales/Quest.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryDialogueLifecycleComponent.h"
#include "UnrealFramework/NarrativeGameState.h"

namespace
{
	/**
	 * A character that owns a real Tales component, with Territory's observer attached the same
	 * way the player management subsystem attaches it for a live player.
	 *
	 * The Tales component is the authority that broadcasts quest and dialogue completion, so the
	 * checks below drive those delegates instead of reaching into the observer.
	 */
	struct FTFEvidenceTalesFixture
	{
		UWorld* World = nullptr;
		ATerritoryGuardCharacter* Character = nullptr;
		UTalesComponent* Tales = nullptr;
		UTerritoryDialogueLifecycleComponent* Observer = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;
		UDialogue* Dialogue = nullptr;
		UQuest* Quest = nullptr;

		static constexpr int32 CharacterGuidSeed = 121;

		bool Build()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) return false;

			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());
			World->SetGameState(NewObject<ANarrativeGameState>(World->PersistentLevel));
			// The ledger marks itself ready when the world begins play - the same point a live
			// session reaches before any quest or dialogue can finish - so the fixture must let
			// the world begin, or every fact would be dropped as "recorded too early".
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();

			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			Character = World->SpawnActor<ATerritoryGuardCharacter>(
				ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, SpawnParams);
			if (!Character) return false;
			Character->SetActorGUID_Implementation(FGuid(CharacterGuidSeed, 122, 123, 124));

			Tales = NewObject<UTalesComponent>(Character, TEXT("Tales"));
			if (!Tales) return false;
			Tales->RegisterComponent();

			Observer = UTerritoryDialogueLifecycleComponent::FindOrCreateForTales(Tales);
			if (!Observer) return false;

			Dialogue = NewObject<UDialogue>(Character, TEXT("EvidenceDialogue"));
			Quest = NewObject<UQuest>(Character, TEXT("EvidenceQuest"));
			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			return Dialogue != nullptr && Quest != nullptr && Ledger != nullptr;
		}

		void TearDown()
		{
			if (World)
			{
				// End the dialogue while it is still reachable. Native deinitializes the current
				// dialogue in TalesComponent::EndPlay, and by that point world teardown has
				// already left the dialogue unreachable; its BlueprintImplementableEvent
				// OnEndDialogue then trips the VM assertion !IsUnreachable() in ScriptCore.cpp and
				// takes the whole editor process down (observed: the crash landed on the next
				// test in the suite). ExitDialogue runs the same deinitialization on a live object
				// and clears CurrentDialogue, so teardown has nothing left to do.
				if (Tales && IsValid(Dialogue) && Tales->GetCurrentDialogue() == Dialogue)
				{
					Tales->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
				}
				if (IsValid(Dialogue)) Dialogue->OwningComp = nullptr;
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
				World = nullptr;
			}
		}
	};
}

/**
 * A quest that reached its success state is a durable fact about the character that owned it,
 * and one success is one fact: the same quest succeeding again is the same episode, so it is
 * absorbed by the ledger's replay key instead of growing the window.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceTalesQuestSucceeded,
	"TerritoryFramework.Evidence.Producer.QuestSuccessRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceTalesQuestSucceeded::RunTest(const FString& Parameters)
{
	FTFEvidenceTalesFixture Fixture;
	if (!TestTrue(TEXT("Tales fixture builds"), Fixture.Build()))
	{
		Fixture.TearDown();
		return false;
	}

	const FGuid CharacterGuid =
		INarrativeStableActor::Execute_GetActorGUID(Fixture.Character);

	Fixture.Tales->OnQuestSucceeded.Broadcast(Fixture.Quest, FText::FromString(TEXT("Done")));

	const TArray<FTerritoryEvidenceRecord> Rows = Fixture.Ledger->GetEvidenceRecords();
	TestEqual(TEXT("One quest success writes exactly one row"), Rows.Num(), 1);
	if (Rows.Num() == 1)
	{
		const FTerritoryEvidenceRecord& Row = Rows[0];
		TestEqual(TEXT("The row is a quest completion"), Row.EvidenceTag,
			TerritoryEvidenceTags::QuestCompleted.GetTag());
		TestEqual(TEXT("The character that owned the quest is the subject"), Row.SubjectId,
			CharacterGuid);
		TestEqual(TEXT("One completion weighs one"), Row.Weight, 1);
		TestTrue(TEXT("The episode carries a replay key"), Row.ContextId.IsValid());
	}

	// The replay key is derived from the fact, the character and the campaign clock, so the same
	// success in the same episode is not a second fact.
	Fixture.Tales->OnQuestSucceeded.Broadcast(Fixture.Quest, FText::FromString(TEXT("Done")));
	TestEqual(TEXT("The same success again adds no row"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("The same success again commits no fact"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

/**
 * Only a dialogue that ran out of lines counts as completed. An exit the player caused, a
 * distance exit, a replacement and a cinematic interruption are all ways a dialogue stops
 * without completing, so none of them may leave a fact behind.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceTalesDialogueCompleted,
	"TerritoryFramework.Evidence.Producer.DialogueCompletionRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceTalesDialogueCompleted::RunTest(const FString& Parameters)
{
	FTFEvidenceTalesFixture Fixture;
	if (!TestTrue(TEXT("Tales fixture builds"), Fixture.Build()))
	{
		Fixture.TearDown();
		return false;
	}

	// The observer's identity guard compares the dialogue against the session's current one.
	Fixture.Dialogue->OwningComp = Fixture.Tales;
	Fixture.Tales->CurrentDialogue = Fixture.Dialogue;

	Fixture.Tales->OnDialogueFinished.Broadcast(Fixture.Dialogue,
		/*bStartingNewDialogue=*/false, EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("Walking away from a dialogue is not a completion"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	Fixture.Tales->OnDialogueFinished.Broadcast(Fixture.Dialogue,
		/*bStartingNewDialogue=*/false, EExitDialogueReason::EDR_TooFarAway);
	TestEqual(TEXT("A distance exit is not a completion"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	Fixture.Tales->OnDialogueFinished.Broadcast(Fixture.Dialogue,
		/*bStartingNewDialogue=*/true, EExitDialogueReason::EDR_NewDialogueStarted);
	TestEqual(TEXT("A replaced dialogue is not a completion"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	Fixture.Tales->OnDialogueFinished.Broadcast(Fixture.Dialogue,
		/*bStartingNewDialogue=*/false, EExitDialogueReason::EDR_NoLines);

	const TArray<FTerritoryEvidenceRecord> Rows = Fixture.Ledger->GetEvidenceRecords();
	TestEqual(TEXT("A dialogue that ran out of lines writes one row"), Rows.Num(), 1);
	if (Rows.Num() == 1)
	{
		TestEqual(TEXT("The row is a dialogue completion"), Rows[0].EvidenceTag,
			TerritoryEvidenceTags::DialogueCompleted.GetTag());
		TestEqual(TEXT("The character that held the dialogue is the subject"), Rows[0].SubjectId,
			INarrativeStableActor::Execute_GetActorGUID(Fixture.Character));
	}

	Fixture.Tales->OnDialogueFinished.Broadcast(Fixture.Dialogue,
		/*bStartingNewDialogue=*/false, EExitDialogueReason::EDR_NoLines);
	TestEqual(TEXT("The same completion again adds no row"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

/** A client-side Tales component never produced an authority fact, and cannot start now. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceTalesRefusedWithoutAuthority,
	"TerritoryFramework.Evidence.Producer.TalesFactRefusedWithoutObserver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceTalesRefusedWithoutAuthority::RunTest(const FString& Parameters)
{
	FTFEvidenceTalesFixture Fixture;
	if (!TestTrue(TEXT("Tales fixture builds"), Fixture.Build()))
	{
		Fixture.TearDown();
		return false;
	}

	// A character that carries no Territory observer has nothing bound, so its Tales component
	// reporting a success is heard by no one; the world must stay accountable for the fact only
	// when an observer exists.
	Fixture.Observer->DestroyComponent();

	Fixture.Tales->OnQuestSucceeded.Broadcast(Fixture.Quest, FText::FromString(TEXT("Done")));
	TestEqual(TEXT("With no observer bound, a quest success records nothing"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
