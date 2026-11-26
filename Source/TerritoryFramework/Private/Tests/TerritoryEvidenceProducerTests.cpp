// Evidence producers — D1 Phase 2, batch 1: the capture-commit hook.
//
// These tests exercise the producer end to end: a real authority mutation through
// UTerritoryControlSubsystem, then the ledger the mutation is required to feed. They do not
// call the ledger directly — that contract is covered by TerritoryEvidenceLedgerTests.cpp.
//
// The world fixture follows TerritoryVolumeRuleCallbackTests.cpp: an authority GameMode is what
// ApplyTerritoryMutationValidated checks first, so a world without one can only test rejection.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryMutationTypes.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	/** One authority world with a mutable Place in it, mirroring the capture regression fixture. */
	struct FTFEvidenceProducerFixture
	{
		UWorld* World = nullptr;
		ATerritoryProperty* Territory = nullptr;
		UTerritoryControlSubsystem* Control = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;

		bool Build(FAutomationTestBase& Test)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!Test.TestNotNull(TEXT("Producer world exists"), World))
			{
				return false;
			}
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());

			Territory = World->SpawnActor<ATerritoryProperty>();
			if (!Test.TestNotNull(TEXT("Place exists"), Territory))
			{
				return false;
			}
			Territory->SetActorGUID_Implementation(FGuid(31, 32, 33, 34));

			Control = World->GetSubsystem<UTerritoryControlSubsystem>();
			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			return Test.TestNotNull(TEXT("Control subsystem exists"), Control)
				&& Test.TestNotNull(TEXT("Evidence ledger exists"), Ledger);
		}

		void TearDown()
		{
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
				World = nullptr;
			}
		}

		/** A request that only the Place and the new owner constrain; every policy bypass is explicit. */
		FTerritoryMutationRequest MakeRequest(const FGameplayTag& NewOwner) const
		{
			FTerritoryMutationRequest Request;
			Request.Territory = Territory;
			Request.NewOwner = NewOwner;
			Request.TransitionContext.RequestingFaction = NewOwner;
			Request.bBypassConditions = true;
			Request.bBypassDiplomacy = true;
			Request.bBypassLock = true;
			return Request;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceCaptureCommitProducer,
	"TerritoryFramework.Evidence.Producer.CaptureCommitRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceCaptureCommitProducer::RunTest(const FString& Parameters)
{
	FTFEvidenceProducerFixture Fixture;
	if (!Fixture.Build(*this))
	{
		Fixture.TearDown();
		return false;
	}
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	TestTrue(TEXT("The faction tags this case needs resolve"),
		Heroes.IsValid() && Bandits.IsValid());
	TestEqual(TEXT("Nothing is recorded before any capture"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	TestEqual(TEXT("The first capture commits"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Heroes)).Result,
		ETerritoryMutationResult::Success);

	const TArray<FTerritoryEvidenceRecord> AfterFirst = Fixture.Ledger->GetEvidenceRecords();
	if (!TestEqual(TEXT("A committed capture records exactly one fact"), AfterFirst.Num(), 1))
	{
		Fixture.TearDown();
		return false;
	}
	const FTerritoryEvidenceRecord Capture = AfterFirst[0];
	TestEqual(TEXT("The fact is a capture"), Capture.EvidenceTag,
		TerritoryEvidenceTags::CaptureCommitted.GetTag());
	TestEqual(TEXT("The Place is the subject"), Capture.SubjectId, Fixture.Territory->GetTerritoryGUID());
	TestEqual(TEXT("The capturing faction is the instigator"), Capture.InstigatorFaction, Heroes);
	TestEqual(TEXT("The fact is scoped to the Place it happened in"), Capture.SourceTerritory,
		Fixture.Territory->GetTerritoryTag());
	TestTrue(TEXT("The fact carries a transition context id"), Capture.ContextId.IsValid());
	TestTrue(TEXT("The fact is stamped with campaign time"), Capture.GameTimeSeconds >= 0.0);
	TestEqual(TEXT("The commit counter counts the fact"), Fixture.Ledger->GetTotalRecordedEvidenceCount(), 1);

	// A later, different transfer must not be swallowed by the first one's context key.
	TestEqual(TEXT("A second transfer commits"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Bandits)).Result,
		ETerritoryMutationResult::Success);

	const TArray<FTerritoryEvidenceRecord> AfterSecond = Fixture.Ledger->GetEvidenceRecords();
	if (!TestEqual(TEXT("Each committed transfer records its own fact"), AfterSecond.Num(), 2))
	{
		Fixture.TearDown();
		return false;
	}
	TestEqual(TEXT("The second fact names the second captor"), AfterSecond[1].InstigatorFaction, Bandits);
	TestTrue(TEXT("Two distinct transitions never share a context id"),
		AfterSecond[1].ContextId != Capture.ContextId);

	// AGENTS.md §4.3: the fact exists before the delegate runs, so a listener that inspects the
	// ledger during OnTerritoryControlChanged sees the capture that woke it.
	TestTrue(TEXT("The final committed owner is the second captor"),
		Fixture.Territory->GetOwningFaction() == Bandits);

	Fixture.TearDown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceCaptureCommitRejections,
	"TerritoryFramework.Evidence.Producer.RejectedMutationWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceCaptureCommitRejections::RunTest(const FString& Parameters)
{
	FTFEvidenceProducerFixture Fixture;
	if (!Fixture.Build(*this))
	{
		Fixture.TearDown();
		return false;
	}
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	TestTrue(TEXT("The faction tag this case needs resolves"), Heroes.IsValid());

	// A request with no target is rejected before any state change.
	FTerritoryMutationRequest Empty = Fixture.MakeRequest(Heroes);
	Empty.Territory = nullptr;
	TestTrue(TEXT("A mutation without a Place is rejected"),
		Fixture.Control->ApplyTerritoryMutation(Empty).Result != ETerritoryMutationResult::Success);
	TestEqual(TEXT("A rejected mutation writes no evidence"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	// A client replica cannot commit, so it cannot produce a fact either.
	Fixture.Territory->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("A client replica cannot commit an ownership change"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Heroes)).Result
			!= ETerritoryMutationResult::Success);
	TestEqual(TEXT("A replica-side rejection writes no evidence"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	// The authority still commits, and the count tracks committed captures only.
	Fixture.Territory->SetRole(ROLE_Authority);
	TestEqual(TEXT("The authority commits the capture"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Heroes)).Result,
		ETerritoryMutationResult::Success);
	TestEqual(TEXT("Exactly one fact exists after one committed capture"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);

	Fixture.Territory->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("A second replica-side attempt is still rejected"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Heroes)).Result
			!= ETerritoryMutationResult::Success);
	TestEqual(TEXT("A rejection after a commit leaves the committed fact alone"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("Restoring a replica never counts as a new observation"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

/**
 * A release is the other half of the same physical fact: the Place really did change hands, it
 * just changed to nobody. The acting faction is then the one that gave it up, and the row must
 * say so rather than reporting a capture with an empty owner.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceCaptureReleaseProducer,
	"TerritoryFramework.Evidence.Producer.ReleaseRecordsLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceCaptureReleaseProducer::RunTest(const FString& Parameters)
{
	FTFEvidenceProducerFixture Fixture;
	if (!Fixture.Build(*this))
	{
		Fixture.TearDown();
		return false;
	}

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"), false);
	TestTrue(TEXT("The faction tags this case needs resolve"), Heroes.IsValid() && Bandits.IsValid());

	TestEqual(TEXT("The first capture commits"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Heroes)).Result,
		ETerritoryMutationResult::Success);
	TestEqual(TEXT("A capture records exactly one fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);

	// Release back to unclaimed: a real ownership change to no owner.
	FTerritoryMutationRequest Release = Fixture.MakeRequest(FGameplayTag());
	Release.DesiredState = ETerritoryState::Unclaimed;
	TestEqual(TEXT("Releasing the Place commits"),
		Fixture.Control->ApplyTerritoryMutation(Release).Result,
		ETerritoryMutationResult::Success);
	TestEqual(TEXT("A release records one more fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 2);
	TestEqual(TEXT("The release is recorded as a loss"),
		Fixture.Ledger->GetEvidenceWeight(TerritoryEvidenceTags::CaptureLost.GetTag()), 1);
	TestEqual(TEXT("The capture fact is still there"),
		Fixture.Ledger->GetEvidenceWeight(TerritoryEvidenceTags::CaptureCommitted.GetTag()), 1);

	const TArray<FTerritoryEvidenceRecord> LostRows =
		Fixture.Ledger->GetEvidenceForTag(TerritoryEvidenceTags::CaptureLost.GetTag());
	if (TestEqual(TEXT("Exactly one loss row exists"), LostRows.Num(), 1))
	{
		// The faction that gave the Place up is the one that acted.
		TestEqual(TEXT("A release names the faction that gave the Place up"),
			LostRows[0].InstigatorFaction, Heroes);
		TestEqual(TEXT("A release keeps the Place as the subject"),
			LostRows[0].SubjectId, FGuid(31, 32, 33, 34));
		TestTrue(TEXT("A release carries a deterministic context id"),
			LostRows[0].ContextId.IsValid());
	}

	// Releasing an already unowned Place is refused outright (there is no owner to take it from),
	// so it can never reach the producer and writes nothing.
	TestTrue(TEXT("Releasing an unowned Place is refused"),
		Fixture.Control->ApplyTerritoryMutation(Release).Result
			!= ETerritoryMutationResult::Success);
	TestEqual(TEXT("A refused release records no fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 2);

	// A capture after a release is a new fact, not a replay of the first one.
	TestEqual(TEXT("The Place can be captured again"),
		Fixture.Control->ApplyTerritoryMutation(Fixture.MakeRequest(Bandits)).Result,
		ETerritoryMutationResult::Success);
	TestEqual(TEXT("A recapture records its own fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 3);
	TestEqual(TEXT("Both captures are attributed to the same subject"),
		Fixture.Ledger->GetEvidenceWeightForSubject(
			TerritoryEvidenceTags::CaptureCommitted.GetTag(), FGuid(31, 32, 33, 34)), 2);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
