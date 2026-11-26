#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "UObject/UnrealType.h"

/**
 * The evidence ledger has exactly one owner of its durable window (the server-side
 * subsystem) and one projection to clients (ATerritoryWorldState). These cases prove the
 * three contracts the projection claims:
 *
 *   1. A committed fact reaches the replicated read model and the save mirror.
 *   2. A rejected fact reaches neither.
 *   3. A client replica stays "unknown" until the window arrives, and never appends
 *      authoritative rows on its own (AGENTS.md §4.2).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerProjection,
	"TerritoryFramework.WorldState.Evidence.ProjectionAndReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerProjection::RunTest(const FString& Parameters)
{
	// Same fixture as the diplomacy replication case: a registered world context, actors
	// initialized for play, then BeginPlay. The WorldState subscribes to live updates from
	// its own BeginPlay, so a world that never begins is not the world this projection runs
	// in, and a hand-made binding would not be the one that ships.
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Evidence projection world exists"), World))
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
	// A second snapshot, used below as the authority control: if the production subscription
	// is the broken link then neither snapshot receives, and if a peer-specific effect were
	// involved the two would disagree.
	ATerritoryWorldState* Control = World->SpawnActor<ATerritoryWorldState>(
		ATerritoryWorldState::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("The ledger is created with its world"), Ledger)
		|| !TestNotNull(TEXT("A WorldState snapshot exists"), State)
		|| !TestNotNull(TEXT("A control snapshot exists"), Control))
	{
		Teardown();
		return false;
	}

	State->SetRole(ROLE_Authority);
	Control->SetRole(ROLE_Authority);
	// Authored identity, as the placed actor carries in a real level: without it the snapshot
	// reports that its save/load is disabled at Error level, which the automation framework
	// records as a failed test.
	State->WorldStateGUID = FGuid(11, 12, 13, 14);
	Control->WorldStateGUID = FGuid(15, 16, 17, 18);
	// The world's BeginPlay runs each world subsystem's OnWorldBeginPlay; calling it by hand
	// as well trips the subsystem's own "already begun" ensure and would report a failure
	// that the shipping path never produces.
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	State->DispatchBeginPlay();
	Control->DispatchBeginPlay();

	// Preconditions the live push depends on, asserted separately so a failure names the
	// broken link instead of only reporting an empty read model.
	TestTrue(TEXT("The snapshot actor is on the authority"), State->HasAuthority());
	TestNotNull(TEXT("The snapshot actor resolves its world"), State->GetWorld());
	TestNotNull(TEXT("The live handler is reflectively callable"),
		State->FindFunction(TEXT("OnEvidenceRecordedLive")));

	// The binding that ships: ATerritoryWorldState::BeginPlay -> SubscribeToLiveUpdates.
	TestTrue(TEXT("The WorldState bound the ledger's live delegate in BeginPlay"),
		Ledger->OnEvidenceRecorded.Contains(State, TEXT("OnEvidenceRecordedLive")));
	TestTrue(TEXT("The ledger has a live listener after binding"), Ledger->OnEvidenceRecorded.IsBound());
	TestTrue(TEXT("The authority control bound the same delegate in its own BeginPlay"),
		Ledger->OnEvidenceRecorded.Contains(Control, TEXT("OnEvidenceRecordedLive")));

	const FGameplayTag Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Market = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	const FGuid Subject(31, 32, 33, 34);

	FTerritoryEvidenceMutationResult First = Ledger->RecordEvidence(
		TerritoryEvidenceTags::CaptureCommitted, Subject, Faction, Market, FGuid(41, 42, 43, 44), 4);
	TestTrue(TEXT("The first fact commits on the server"), First.bSuccess);
	TestEqual(TEXT("A committed fact reaches the replicated read model"),
		State->GetEvidenceLedger().Num(), 1);
	// The save mirror belongs to the save boundary, not to a live fact: this actor defines
	// SavedX as "the state at the last publish/export", so a live push must not move it.
	TestEqual(TEXT("A live fact does not move the save mirror"),
		State->SavedEvidence.Num(), 0);
	TestTrue(TEXT("Publishing marks the projection complete"),
		State->bEvidenceLedgerPublished);

	// The seeding path is independent of the live delegate: actor BeginPlay and the save
	// export both call it because a restored ledger broadcasts nothing.
	State->PublishEvidenceLedger();
	TestEqual(TEXT("An explicit publish seeds the window a restored ledger never broadcasts"),
		State->GetEvidenceLedger().Num(), 1);

	TestTrue(TEXT("A second fact commits"),
		Ledger->RecordEvidence(TerritoryEvidenceTags::GuardKilled, FGuid(), Faction, Market,
			FGuid(45, 46, 47, 48), 2).bSuccess);

	// Snapshot once and guard the index: a failed assertion must report, never
	// index an empty array (array bounds are a fatal assert, not a test failure).
	const TArray<FTerritoryEvidenceRecord> TwoRows = State->GetEvidenceLedger();
	TestEqual(TEXT("Every committed fact is pushed live"), TwoRows.Num(), 2);
	if (TwoRows.Num() == 2)
	{
		TestEqual(TEXT("The read model keeps recording order"), TwoRows[1].Weight, 2);
	}

	// A fact the ledger refused must not appear anywhere downstream.
	FTerritoryEvidenceMutationResult Rejected = Ledger->RecordEvidence(
		Faction, Subject, Faction, Market, FGuid(49, 50, 51, 52), 1);
	TestFalse(TEXT("A non-evidence tag is refused"), Rejected.bSuccess);
	TestEqual(TEXT("A refused fact never reaches the read model"),
		State->GetEvidenceLedger().Num(), 2);
	// One row, from the explicit publish before the second fact: neither the live push nor the
	// refused fact moved the save boundary.
	TestEqual(TEXT("A refused fact never reaches the save mirror"),
		State->SavedEvidence.Num(), 1);

	// Explicit publish is the seeding path: actor BeginPlay and the save export both call
	// it, because a restored ledger broadcasts nothing.
	State->PublishEvidenceLedger();
	TestEqual(TEXT("Explicit publish republishes the whole window"),
		State->ReplicatedEvidence.Num(), 2);
	TestEqual(TEXT("The published window matches the ledger it mirrors"),
		State->ReplicatedEvidence.Num(), Ledger->GetRetainedEvidenceCount());

	// A server read is always a real answer.
	TestTrue(TEXT("A server treats its own read as an answer"), State->IsEvidenceLedgerReady());

	// Client parity: before the window arrives an empty read is unknown, not "no evidence".
	State->SetRole(ROLE_SimulatedProxy);
	State->bEvidenceLedgerPublished = false;
	State->ReplicatedEvidence.Reset();
	State->SavedEvidence.Reset();
	TestFalse(TEXT("An unhydrated client reports the ledger as unknown"),
		State->IsEvidenceLedgerReady());
	TestEqual(TEXT("An unknown client exposes no rows"),
		State->GetEvidenceLedger().Num(), 0);

	// A client-role snapshot must not append authoritative state on its own.
	TestTrue(TEXT("The server ledger still records while a client reads"),
		Ledger->RecordEvidence(TerritoryEvidenceTags::CaptureLost, Subject, Faction, Market,
			FGuid(53, 54, 55, 56), 6).bSuccess);
	TestEqual(TEXT("A client-role projection cannot append authoritative rows"),
		State->GetEvidenceLedger().Num(), 0);
	TestEqual(TEXT("...and cannot publish a save mirror either"),
		State->SavedEvidence.Num(), 0);

	// The second snapshot, bound through the same production BeginPlay path, proves the push
	// keeps firing on an authority peer while the client-role snapshot refuses to append:
	// one pushed row per accepted fact, and none for the refused one.
	TestEqual(TEXT("The second authority snapshot received every accepted fact"),
		Control->ReplicatedEvidence.Num(), 3);

	// Replication arrival: the window lands, the replica hydrates, readiness flips.
	State->ReplicatedEvidence = Ledger->GetPersistentState();
	State->SyncEvidenceLedgerFromReplicatedState();
	State->bEvidenceLedgerPublished = true;
	TestEqual(TEXT("The hydrated replica holds the replicated window"),
		Ledger->GetEvidenceRecords().Num(), State->ReplicatedEvidence.Num());
	TestEqual(TEXT("Hydration restores the ledger's own rows"),
		Ledger->GetEvidenceRecords().Num(), 3);
	TestTrue(TEXT("A hydrated client can trust an empty read"), State->IsEvidenceLedgerReady());
	TestTrue(TEXT("A hydrated client ledger is ready"), Ledger->IsEvidenceLedgerReady());

	// Reflection contract: the projection is replicated, and the readiness flag stays
	// declared after the rows it describes so a client cannot report readiness first.
	TestNotNull(TEXT("ReplicatedEvidence is a reflected property"),
		FindFProperty<FProperty>(ATerritoryWorldState::StaticClass(), TEXT("ReplicatedEvidence")));
	TestNotNull(TEXT("The readiness flag is a reflected property"),
		FindFProperty<FProperty>(ATerritoryWorldState::StaticClass(), TEXT("bEvidenceLedgerPublished")));
	TestNotNull(TEXT("SavedEvidence is a reflected save property"),
		FindFProperty<FProperty>(ATerritoryWorldState::StaticClass(), TEXT("SavedEvidence")));

	int32 RowsIndex = INDEX_NONE;
	int32 FlagIndex = INDEX_NONE;
	int32 Index = 0;
	for (TFieldIterator<FProperty> It(ATerritoryWorldState::StaticClass()); It; ++It, ++Index)
	{
		if (It->GetName() == TEXT("ReplicatedEvidence"))
		{
			RowsIndex = Index;
		}
		else if (It->GetName() == TEXT("bEvidenceLedgerPublished"))
		{
			FlagIndex = Index;
		}
	}
	TestTrue(TEXT("Both projection properties are reflected in declaration order"),
		RowsIndex != INDEX_NONE && FlagIndex != INDEX_NONE);
	TestTrue(TEXT("The readiness flag is declared after its rows (RepIndex rule)"),
		FlagIndex > RowsIndex);

	Teardown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
