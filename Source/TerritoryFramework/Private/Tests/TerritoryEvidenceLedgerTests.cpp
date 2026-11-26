#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

/**
 * Behavioural tests for the evidence ledger.
 *
 * These are world-level tests, not reflection tests: every assertion reads state back
 * from the ledger after a mutation the same way a gameplay producer would drive it.
 * The ledger is a World Subsystem, so each case owns its world.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerValidation,
	"TerritoryFramework.Evidence.Behavior.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerValidation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence validation world exists"), World))
	{
		return false;
	}

	UTerritoryEvidenceLedgerSubsystem* Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
	if (!TestNotNull(TEXT("The ledger is created with its world"), Ledger))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Market = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	const FGuid Subject(11, 12, 13, 14);

	// 1. A fact has to name an observable event.
	FTerritoryEvidenceMutationResult Untagged = Ledger->RecordEvidence(
		FGameplayTag(), Subject, Faction, FGameplayTag(), FGuid());
	TestFalse(TEXT("A fact without a tag is rejected"), Untagged.bSuccess);
	TestFalse(TEXT("A rejection explains itself"), Untagged.FailureReason.IsEmpty());

	// 2. An attitude is not an observation, so a foreign tag family is refused.
	FTerritoryEvidenceMutationResult WrongFamily = Ledger->RecordEvidence(
		Faction, Subject, Faction, FGameplayTag(), FGuid());
	TestFalse(TEXT("A tag outside Territory.Evidence.* is rejected"), WrongFamily.bSuccess);
	TestTrue(TEXT("The rejection names the tag family"),
		WrongFamily.FailureReason.ToString().Contains(TEXT("Territory.Evidence")));

	// 3. An observation nobody is attached to cannot be read back usefully.
	FTerritoryEvidenceMutationResult Unattributed = Ledger->RecordEvidence(
		TerritoryEvidenceTags::GuardKilled, FGuid(), FGameplayTag(), FGameplayTag(), FGuid());
	TestFalse(TEXT("A fact with neither subject nor faction is rejected"), Unattributed.bSuccess);
	TestEqual(TEXT("A rejected fact leaves no row behind"), Ledger->GetRetainedEvidenceCount(), 0);
	TestEqual(TEXT("A rejected fact does not advance the recorded counter"),
		Ledger->GetTotalRecordedEvidenceCount(), 0);

	// 4. Faction-only and subject-only attribution are both legal.
	FTerritoryEvidenceMutationResult FactionOnly = Ledger->RecordEvidence(
		TerritoryEvidenceTags::TreatySigned, FGuid(), Faction, FGameplayTag(), FGuid());
	TestTrue(TEXT("A faction-attributed fact is accepted"), FactionOnly.bSuccess);
	FTerritoryEvidenceMutationResult SubjectOnly = Ledger->RecordEvidence(
		TerritoryEvidenceTags::StealthExposed, Subject, FGameplayTag(), FGameplayTag(), FGuid());
	TestTrue(TEXT("A subject-attributed fact is accepted"), SubjectOnly.bSuccess);

	// 5. Weight is bounded, and the bound clamps instead of rejecting.
	FTerritoryEvidenceMutationResult Zero = Ledger->RecordEvidence(
		TerritoryEvidenceTags::GuardKilled, Subject, Faction, FGameplayTag(), FGuid(), 0);
	TestTrue(TEXT("Weight 0 is clamped, not rejected"), Zero.bSuccess);
	TestEqual(TEXT("Weight 0 clamps up to the minimum"), Zero.Record.Weight, 1);
	FTerritoryEvidenceMutationResult Huge = Ledger->RecordEvidence(
		TerritoryEvidenceTags::GuardKilled, Subject, Faction, FGameplayTag(), FGuid(), 100000);
	TestTrue(TEXT("An oversized weight is accepted and clamped"), Huge.bSuccess);
	TestEqual(TEXT("Weight above the cap clamps to MaxEvidenceWeight"),
		Huge.Record.Weight, Ledger->MaxEvidenceWeight);

	// 6. A Territory-scoped fact stays reachable through the scoped queries.
	FTerritoryEvidenceMutationResult Scoped = Ledger->RecordEvidence(
		TerritoryEvidenceTags::CaptureCommitted, FGuid(), Faction, Market, FGuid(), 5);
	TestTrue(TEXT("A Territory-scoped fact is accepted"), Scoped.bSuccess);
	TestEqual(TEXT("The Territory query returns exactly that fact"),
		Ledger->GetEvidenceForTerritory(Market).Num(), 1);
	TestEqual(TEXT("The tag query returns every row carrying that tag"),
		Ledger->GetEvidenceForTag(TerritoryEvidenceTags::CaptureCommitted).Num(), 1);

	// 7. Weight queries sum the rows that match, and nothing else.
	// The GuardKilled rows in this case are the two clamped ones below: Weight 0 -> 1 and
	// 100000 -> MaxEvidenceWeight. The subject-only row above deliberately carries a
	// different tag so this sum proves the clamp and the tag filter together.
	const int32 GuardKilledWeight = 1 + Ledger->MaxEvidenceWeight;
	TestEqual(TEXT("The tag weight sums only the exact tag"),
		Ledger->GetEvidenceWeight(TerritoryEvidenceTags::GuardKilled), GuardKilledWeight);
	TestEqual(TEXT("The subject weight narrows to one durable subject"),
		Ledger->GetEvidenceWeightForSubject(TerritoryEvidenceTags::GuardKilled, Subject),
		GuardKilledWeight);
	TestEqual(TEXT("A different subject reports no weight for that tag"),
		Ledger->GetEvidenceWeightForSubject(TerritoryEvidenceTags::GuardKilled, FGuid(99, 98, 97, 96)), 0);
	TestEqual(TEXT("A fact recorded against another faction is not counted for this one"),
		Ledger->GetEvidenceForTag(TerritoryEvidenceTags::TreatyBroken).Num(), 0);

	// 8. An invalid filter reports nothing instead of reporting everything.
	TestEqual(TEXT("An invalid subject filter returns no rows"),
		Ledger->GetEvidenceForSubject(FGuid()).Num(), 0);
	TestEqual(TEXT("An invalid tag filter returns no rows"),
		Ledger->GetEvidenceForTag(FGameplayTag()).Num(), 0);
	TestEqual(TEXT("An invalid Territory filter returns no rows"),
		Ledger->GetEvidenceForTerritory(FGameplayTag()).Num(), 0);
	TestEqual(TEXT("An invalid tag weight is zero, not a whole-ledger sum"),
		Ledger->GetEvidenceWeight(FGameplayTag()), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerIdempotency,
	"TerritoryFramework.Evidence.Behavior.Idempotency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerIdempotency::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence idempotency world exists"), World))
	{
		return false;
	}

	UTerritoryEvidenceLedgerSubsystem* Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
	if (!TestNotNull(TEXT("The ledger is created with its world"), Ledger))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Market = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	const FGuid Subject(21, 22, 23, 24);
	const FGuid Context(31, 32, 33, 34);

	// A retried commit of one assault or capture must not record twice.
	FTerritoryEvidenceMutationResult Committed = Ledger->RecordEvidence(
		TerritoryEvidenceTags::CaptureCommitted, Subject, Faction, Market, Context, 3);
	TestTrue(TEXT("The first commit succeeds"), Committed.bSuccess);
	TestFalse(TEXT("The first commit is not reported as a replay"), Committed.bAlreadyRecorded);

	FTerritoryEvidenceMutationResult Replay = Ledger->RecordEvidence(
		TerritoryEvidenceTags::CaptureCommitted, Subject, Faction, Market, Context, 3);
	TestFalse(TEXT("A retried commit is not a new success"), Replay.bSuccess);
	TestTrue(TEXT("A retried commit is reported as already recorded"), Replay.bAlreadyRecorded);
	TestFalse(TEXT("A replay still explains itself"), Replay.FailureReason.IsEmpty());
	TestEqual(TEXT("A replay returns the stored row, not the incoming one"),
		Replay.Record.ContextId, Context);
	TestEqual(TEXT("A replay leaves exactly one row"), Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("A replay does not double-count the weight"),
		Ledger->GetEvidenceWeight(TerritoryEvidenceTags::CaptureCommitted), 3);
	TestEqual(TEXT("A replay does not advance the recorded counter"),
		Ledger->GetTotalRecordedEvidenceCount(), 1);

	// Identity is the triple, so a new context is a new fact.
	FTerritoryEvidenceMutationResult NewContext = Ledger->RecordEvidence(
		TerritoryEvidenceTags::CaptureCommitted, Subject, Faction, Market, FGuid(41, 42, 43, 44), 3);
	TestTrue(TEXT("The same fact in a new context is a new row"), NewContext.bSuccess);
	TestEqual(TEXT("The new context is retained beside the first"),
		Ledger->GetEvidenceWeight(TerritoryEvidenceTags::CaptureCommitted), 6);

	// ...and so is the same context about a different subject.
	FTerritoryEvidenceMutationResult NewSubject = Ledger->RecordEvidence(
		TerritoryEvidenceTags::CaptureCommitted, FGuid(51, 52, 53, 54), Faction, Market, Context, 3);
	TestTrue(TEXT("The same context about another subject is a new row"), NewSubject.bSuccess);
	TestEqual(TEXT("Three distinct identities are retained"),
		Ledger->GetRetainedEvidenceCount(), 3);

	// Without a context identity there is no idempotency claim to make.
	TestTrue(TEXT("A context-less fact is accepted"),
		Ledger->RecordEvidence(TerritoryEvidenceTags::QuestCompleted, Subject, Faction, FGameplayTag(), FGuid()).bSuccess);
	TestTrue(TEXT("A repeated context-less fact is accepted again"),
		Ledger->RecordEvidence(TerritoryEvidenceTags::QuestCompleted, Subject, Faction, FGameplayTag(), FGuid()).bSuccess);
	TestEqual(TEXT("Context-less facts cannot claim to be replays"),
		Ledger->GetEvidenceWeight(TerritoryEvidenceTags::QuestCompleted), 2);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerRetention,
	"TerritoryFramework.Evidence.Behavior.Retention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerRetention::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence retention world exists"), World))
	{
		return false;
	}

	UTerritoryEvidenceLedgerSubsystem* Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
	if (!TestNotNull(TEXT("The ledger is created with its world"), Ledger))
	{
		World->DestroyWorld(false);
		return false;
	}

	Ledger->MaxEvidenceRecords = 3;

	// Insertion order decides what a bounded window drops, because a producer may
	// replay a historical fact with an older timestamp.
	for (int32 Weight = 1; Weight <= 5; ++Weight)
	{
		FTerritoryEvidenceMutationResult Committed = Ledger->RecordEvidence(
			TerritoryEvidenceTags::GuardKilled, FGuid(61, 62, 63, 64), FGameplayTag(),
			FGameplayTag(), FGuid(70 + Weight, 0, 0, 0), Weight);
		TestTrue(TEXT("A fact inside the bounded window commits"), Committed.bSuccess);
	}

	TestEqual(TEXT("The window keeps exactly the configured rows"), Ledger->GetRetainedEvidenceCount(), 3);
	TestEqual(TEXT("Eviction is counted, never silent"), Ledger->GetEvictedEvidenceCount(), 2);
	TestEqual(TEXT("The recorded counter remembers the full history"),
		Ledger->GetTotalRecordedEvidenceCount(), 5);
	TestEqual(TEXT("The oldest row is the one that was dropped"),
		Ledger->GetEvidenceForTag(TerritoryEvidenceTags::GuardKilled)[0].Weight, 3);
	TestEqual(TEXT("The newest row survives"),
		Ledger->GetEvidenceForTag(TerritoryEvidenceTags::GuardKilled)[2].Weight, 5);
	TestTrue(TEXT("A consumer can tell the window from the full history"),
		Ledger->GetTotalRecordedEvidenceCount() > Ledger->GetRetainedEvidenceCount());

	// A restored window obeys the same bound, so a save cannot smuggle in an unbounded
	// ledger from an older build with a larger cap.
	TArray<FTerritoryEvidenceRecord> Restored;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FTerritoryEvidenceRecord Row;
		Row.EvidenceTag = TerritoryEvidenceTags::TreatySigned;
		Row.SubjectId = FGuid(81, 82, 83, 84);
		Row.ContextId = FGuid(90 + Index, 0, 0, 0);
		Row.Weight = 1;
		Row.GameTimeSeconds = static_cast<double>(Index);
		Restored.Add(Row);
	}
	Ledger->RestorePersistentState(Restored);
	TestEqual(TEXT("A restored window is bounded like a live one"),
		Ledger->GetRetainedEvidenceCount(), 3);
	TestEqual(TEXT("Evictions from a restore are counted too"), Ledger->GetEvictedEvidenceCount(), 4);

	// A degenerate cap still keeps the newest fact rather than dropping everything.
	Ledger->MaxEvidenceRecords = 0;
	TestTrue(TEXT("A degenerate cap still accepts a commit"),
		Ledger->RecordEvidence(TerritoryEvidenceTags::CaptureCommitted, FGuid(), 
			FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits")), FGameplayTag(), FGuid(), 1).bSuccess);
	TestEqual(TEXT("A degenerate cap keeps the newest row"), Ledger->GetRetainedEvidenceCount(), 1);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerReadiness,
	"TerritoryFramework.Evidence.Behavior.BundleReadinessAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerReadiness::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence readiness world exists"), World))
	{
		return false;
	}

	UTerritoryEvidenceLedgerSubsystem* Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
	if (!TestNotNull(TEXT("The ledger is created with its world"), Ledger))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	const FGuid Subject(101, 102, 103, 104);

	// Before the world begins play nobody owns the window, so an empty read is unknown.
	TestFalse(TEXT("A ledger that has not begun play reports unready"), Ledger->IsEvidenceLedgerReady());

	// Startup may legitimately record before BeginPlay; recording does not depend on it.
	TestTrue(TEXT("A fact commits during server startup"),
		Ledger->RecordEvidence(TerritoryEvidenceTags::TreatySigned, FGuid(), Faction, FGameplayTag(), FGuid(111, 112, 113, 114), 1).bSuccess);
	TestFalse(TEXT("Recording alone does not declare the window complete"),
		Ledger->IsEvidenceLedgerReady());

	// The engine calls this from UWorld::BeginPlay before actors start play
	// (Engine/Private/World.cpp:6167), which is why the projection can publish from its
	// own BeginPlay. A world made by the test harness never begins play, so the hook is
	// invoked directly here.
	Ledger->OnWorldBeginPlay(*World);
	TestTrue(TEXT("A server declares readiness when the world begins play"),
		Ledger->IsEvidenceLedgerReady());

	// A replica is hydrated by the projection; the window it receives is a real answer,
	// including when it is empty.
	TArray<FTerritoryEvidenceRecord> Hydrated;
	FTerritoryEvidenceRecord Row;
	Row.EvidenceTag = TerritoryEvidenceTags::AttackerKilled;
	Row.SubjectId = Subject;
	Row.InstigatorFaction = Bandits;
	Row.ContextId = FGuid(121, 122, 123, 124);
	Row.Weight = 2;
	Hydrated.Add(Row);
	Hydrated.Add(Row);
	Ledger->RestorePersistentState(Hydrated);
	TestEqual(TEXT("A hydrated replica holds the replicated rows"),
		Ledger->GetEvidenceRecords().Num(), 2);
	TestTrue(TEXT("A hydrated replica treats its window as a complete answer"),
		Ledger->IsEvidenceLedgerReady());
	TestEqual(TEXT("Hydration replaces the window instead of appending to it"),
		Ledger->GetEvidenceForTag(TerritoryEvidenceTags::TreatySigned).Num(), 0);

	Ledger->RestorePersistentState({});
	TestEqual(TEXT("An empty replicated window clears the replica"), Ledger->GetEvidenceRecords().Num(), 0);
	TestTrue(TEXT("An empty replicated window is still a real answer"),
		Ledger->IsEvidenceLedgerReady());

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
