#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

/**
 * AGENTS.md §10.2 (save/load test) for the evidence ledger: the durable window is the save
 * mirror on ATerritoryWorldState, so what survives a save boundary is exactly what
 * ExportPersistentState wrote, and a load restores that window instead of the live one.
 *
 * The case walks the real path the shipping code uses at a save boundary
 * (ExportPersistentState) and at a load (ImportPersistentState), then checks the three
 * things a restore must not do: lose rows, duplicate rows, or count restored rows as new
 * commits. It finishes by proving the retention cap still governs the restored window.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerPersistence,
	"TerritoryFramework.Evidence.Persistence.SaveLoadRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerPersistence::RunTest(const FString& Parameters)
{
	// Same fixture as the projection and diplomacy replication cases: a registered world
	// context, actors initialized for play, then BeginPlay. The WorldState binds the ledger's
	// live delegate from its own BeginPlay, so the world has to be a begun world for the
	// publish path under test to be the shipping one.
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence persistence world exists"), World))
	{
		GEngine->DestroyWorldContext(World);
		return false;
	}
	WorldContext.SetCurrentWorld(World);

	auto Teardown = [World]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	UTerritoryEvidenceLedgerSubsystem* Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryWorldState* State = World->SpawnActor<ATerritoryWorldState>(
		ATerritoryWorldState::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("The ledger is created with its world"), Ledger)
		|| !TestNotNull(TEXT("A WorldState snapshot exists"), State))
	{
		Teardown();
		return false;
	}

	State->SetRole(ROLE_Authority);
	// Authored identity, as the placed actor carries in a real level: without it the snapshot
	// reports its save/load is disabled at Error level, which the automation framework records
	// as a failed test.
	State->WorldStateGUID = FGuid(11, 12, 13, 14);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	State->DispatchBeginPlay();

	// Native tags convert to FGameplayTag; comparing them with name lookups proves the
	// declarations registered under the paths the durable records carry.
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag CaptureCommitted = TerritoryEvidenceTags::CaptureCommitted.GetTag();
	const FGameplayTag GuardKilled = TerritoryEvidenceTags::GuardKilled.GetTag();
	const FGameplayTag TreatySigned = TerritoryEvidenceTags::TreatySigned.GetTag();
	const FGameplayTag TreatyBroken = TerritoryEvidenceTags::TreatyBroken.GetTag();
	const FGameplayTag StealthExposed = TerritoryEvidenceTags::StealthExposed.GetTag();
	TestTrue(TEXT("The fixture tags are registered under their durable paths"),
		Heroes.IsValid()
		&& CaptureCommitted == FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Evidence.Capture.Committed"), false)
		&& GuardKilled == FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Evidence.Combat.GuardKilled"), false)
		&& TreatySigned == FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Evidence.Treaty.Signed"), false)
		&& TreatyBroken == FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Evidence.Treaty.Broken"), false)
		&& StealthExposed == FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Evidence.Stealth.Exposed"), false));
	if (!Heroes.IsValid())
	{
		Teardown();
		return false;
	}

	const FGuid Subject = FGuid(21, 22, 23, 24);

	// Three durable facts, recorded in a known order with distinct weights.
	const FTerritoryEvidenceMutationResult First = Ledger->RecordEvidence(
		CaptureCommitted, Subject, Heroes, FGameplayTag(), FGuid(31, 32, 33, 34), 4);
	const FTerritoryEvidenceMutationResult Second = Ledger->RecordEvidence(
		GuardKilled, Subject, Heroes, FGameplayTag(), FGuid(41, 42, 43, 44), 2);
	const FTerritoryEvidenceMutationResult Third = Ledger->RecordEvidence(
		TreatySigned, Subject, Heroes, FGameplayTag(), FGuid(51, 52, 53, 54), 6);
	TestTrue(TEXT("All three facts commit"), First.bSuccess && Second.bSuccess && Third.bSuccess);
	TestEqual(TEXT("The live window holds the three facts"),
		Ledger->GetRetainedEvidenceCount(), 3);

	// ─── Save boundary ───
	State->ExportPersistentState();
	TestEqual(TEXT("The save boundary writes the durable mirror"),
		State->SavedEvidence.Num(), 3);
	TestEqual(TEXT("The save boundary publishes the read model"),
		State->ReplicatedEvidence.Num(), 3);
	if (State->SavedEvidence.Num() == 3)
	{
		TestTrue(TEXT("The saved window keeps insertion order"),
			State->SavedEvidence[0].EvidenceTag == CaptureCommitted);
		TestTrue(TEXT("The saved window keeps the newest row last"),
			State->SavedEvidence[2].EvidenceTag == TreatySigned);
		TestEqual(TEXT("The saved window keeps weights"),
			State->SavedEvidence[0].Weight, 4);
		TestTrue(TEXT("The saved window keeps the subject"),
			State->SavedEvidence[0].SubjectId == Subject);
	}

	// A fact recorded after the save boundary belongs to the live session, not to the save:
	// a later load has to come back to the three rows above.
	const FTerritoryEvidenceMutationResult AfterSave = Ledger->RecordEvidence(
		TreatyBroken, Subject, Heroes, FGameplayTag(), FGuid(61, 62, 63, 64), 9);
	TestTrue(TEXT("A post-save fact commits"), AfterSave.bSuccess);
	TestEqual(TEXT("The live read model moves past the save boundary"),
		State->ReplicatedEvidence.Num(), 4);

	// ─── Load ───
	State->ImportPersistentState();
	TestEqual(TEXT("The load restores the saved window, not the live one"),
		State->ReplicatedEvidence.Num(), 3);
	TestEqual(TEXT("The load re-syncs the owning subsystem"),
		Ledger->GetEvidenceRecords().Num(), 3);
	TestEqual(TEXT("A restored window is a complete answer"),
		Ledger->IsEvidenceLedgerReady(), true);
	TestEqual(TEXT("A restore is not a new commit"),
		Ledger->GetTotalRecordedEvidenceCount(), 4);
	TestEqual(TEXT("A restore evicts nothing"),
		Ledger->GetEvictedEvidenceCount(), 0);
	TestEqual(TEXT("Filtered reads work off the restored window"),
		Ledger->GetEvidenceForSubject(Subject).Num(), 3);
	TestEqual(TEXT("Weight queries work off the restored window"),
		Ledger->GetEvidenceWeight(TreatySigned), 6);
	TestEqual(TEXT("The newest post-save fact is not in the restored window"),
		Ledger->GetEvidenceForTag(TreatyBroken).Num(), 0);

	// Re-importing the same mirror is idempotent: a retried load cannot duplicate a window.
	State->ImportPersistentState();
	TestEqual(TEXT("Re-importing the same mirror keeps the window size"),
		Ledger->GetEvidenceRecords().Num(), 3);
	TestEqual(TEXT("Re-importing evicts nothing"),
		Ledger->GetEvictedEvidenceCount(), 0);

	// ─── Retention still governs the restored window ───
	// Three restored rows plus this one is four, so a cap of two evicts exactly two, oldest
	// first: the restored window stays bounded after a load.
	Ledger->MaxEvidenceRecords = 2;
	const FTerritoryEvidenceMutationResult Capped = Ledger->RecordEvidence(
		StealthExposed, Subject, Heroes, FGameplayTag(), FGuid(71, 72, 73, 74), 1);
	TestTrue(TEXT("A fact commits under the lowered cap"), Capped.bSuccess);
	TestEqual(TEXT("The restored window is still bounded"),
		Ledger->GetRetainedEvidenceCount(), 2);
	TestEqual(TEXT("The two oldest restored rows are the ones evicted"),
		Ledger->GetEvictedEvidenceCount(), 2);
	TestEqual(TEXT("The evicted row is gone from the window"),
		Ledger->GetEvidenceForTag(CaptureCommitted).Num(), 0);

	Teardown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
