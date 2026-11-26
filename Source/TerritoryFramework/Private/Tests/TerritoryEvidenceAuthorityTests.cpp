#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

/**
 * AGENTS.md §4.2 (server authority) and §10.3 (server authority test) for the evidence ledger.
 *
 * The client world here is not a mock: it is a world the engine itself reports as NM_Client.
 * UWorld::GetNetMode answers from InternalGetNetMode (Engine/Private/World.cpp:9598-9623), which
 * with no net driver installed falls through to AttemptDeriveFromURL (World.cpp:9643-9683): a
 * NextURL that carries a host and no "listen" option is a client, one with "listen" is a listen
 * server, and no URL at all is standalone. So the gate under test reads World->GetNetMode()
 * exactly as it does in a session, and the fixture fails loudly if that precondition ever stops
 * holding instead of quietly asserting nothing.
 *
 * The truth table it pins, with one payload used on both worlds so the difference is authority
 * and not validation:
 *   server : RecordEvidence          -> committed
 *   server : IsEvidenceLedgerReady   -> true          ("no facts yet" is a real answer)
 *   client : RecordEvidence          -> refused, nothing retained, nothing counted
 *   client : RecordEvidenceRecord    -> refused too    (the gate is not a Blueprint courtesy)
 *   client : IsEvidenceLedgerReady   -> false          (empty means unknown until hydrated)
 *   client : RestorePersistentState  -> accepted, and readiness follows, because the replicated
 *                                       mirror is the only way a client may hold rows; recording
 *                                       stays refused afterwards.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerAuthority,
	"TerritoryFramework.Evidence.Authority.ClientRejectsRecords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerAuthority::RunTest(const FString& Parameters)
{
	// Native tags convert to FGameplayTag; the shared constants keep both worlds identical.
	const FGameplayTag CaptureCommitted = TerritoryEvidenceTags::CaptureCommitted.GetTag();
	const FGameplayTag GuardKilled = TerritoryEvidenceTags::GuardKilled.GetTag();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"), /*ErrorIfNotFound=*/false);
	if (!TestTrue(TEXT("The evidence and faction tags resolved"), CaptureCommitted.IsValid() && GuardKilled.IsValid() && Heroes.IsValid()))
	{
		return false;
	}

	// The payload both worlds will be asked to record.
	const FGuid Subject(31, 32, 33, 34);
	const FGuid Context(41, 42, 43, 44);
	const int32 Weight = 5;

	struct FWorldFixture
	{
		UWorld* World = nullptr;
		FWorldContext* Context = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;

		void Destroy() const
		{
			if (World)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
			}
		}
	};

	// TravelUrl nullptr -> standalone server; a "host:port" URL -> the engine derives NM_Client.
	auto MakeWorld = [](const TCHAR* TravelUrl)
	{
		FWorldFixture Fixture;
		Fixture.Context = &GEngine->CreateNewWorldContext(EWorldType::Game);
		Fixture.World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Fixture.World)
		{
			// No world means nothing to tear down by pointer; the case fails on the null check
			// below rather than continuing with a half-built fixture.
			return Fixture;
		}

		Fixture.Context->SetCurrentWorld(Fixture.World);
		if (TravelUrl)
		{
			Fixture.World->NextURL = TravelUrl;
		}

		Fixture.Ledger = Fixture.World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
		return Fixture;
	};

	// ─── The client world ───
	FWorldFixture Client = MakeWorld(TEXT("127.0.0.1:7777"));
	if (!TestNotNull(TEXT("The client world exists"), Client.World)
		|| !TestNotNull(TEXT("The client ledger exists"), Client.Ledger))
	{
		Client.Destroy();
		return false;
	}

	Client.World->InitializeActorsForPlay(FURL());
	Client.World->BeginPlay();

	// Precondition: if this ever stops holding, the case below would assert nothing meaningful,
	// so it is reported as a failure of the fixture rather than silently skipped.
	const bool bClientWorld = TestEqual(TEXT("The client world reports the client net mode"),
		static_cast<int32>(Client.World->GetNetMode()), static_cast<int32>(NM_Client));

	if (bClientWorld)
	{
		const FTerritoryEvidenceMutationResult Refused = Client.Ledger->RecordEvidence(
			CaptureCommitted, Subject, Heroes, FGameplayTag(), Context, Weight);
		TestFalse(TEXT("A client cannot record evidence"), Refused.bSuccess);
		TestTrue(TEXT("The refused client fact explains itself"), !Refused.FailureReason.IsEmpty());
		TestEqual(TEXT("A refused client fact is not retained"), Client.Ledger->GetRetainedEvidenceCount(), 0);
		TestEqual(TEXT("A refused client fact is not counted"), Client.Ledger->GetTotalRecordedEvidenceCount(), 0);
		TestFalse(TEXT("A client ledger is unknown before the mirror arrives"), Client.Ledger->IsEvidenceLedgerReady());

		// The plain C++ path is gated identically: authority is not a Blueprint-only courtesy.
		FTerritoryEvidenceRecord Direct;
		Direct.EvidenceTag = GuardKilled;
		Direct.SubjectId = FGuid(51, 52, 53, 54);
		Direct.InstigatorFaction = Heroes;
		Direct.Weight = 3;
		Direct.ContextId = FGuid(61, 62, 63, 64);
		const FTerritoryEvidenceMutationResult DirectRefused = Client.Ledger->RecordEvidenceRecord(Direct);
		TestFalse(TEXT("The direct C++ record path is refused on a client too"), DirectRefused.bSuccess);
		TestEqual(TEXT("The refused direct fact is not retained"), Client.Ledger->GetRetainedEvidenceCount(), 0);

		// The replicated mirror is the one way a client owns rows, and it is what the WorldState
		// sync calls: restoring is hydration, not recording.
		FTerritoryEvidenceRecord Mirrored;
		Mirrored.EvidenceTag = CaptureCommitted;
		Mirrored.SubjectId = Subject;
		Mirrored.InstigatorFaction = Heroes;
		Mirrored.Weight = 7;
		Mirrored.GameTimeSeconds = 120.0;
		TArray<FTerritoryEvidenceRecord> MirroredRows;
		MirroredRows.Add(Mirrored);
		Client.Ledger->RestorePersistentState(MirroredRows);

		TestEqual(TEXT("The client mirror accepts the replicated window"), Client.Ledger->GetRetainedEvidenceCount(), 1);
		TestTrue(TEXT("A hydrated client window is a real answer"), Client.Ledger->IsEvidenceLedgerReady());
		TestEqual(TEXT("Hydrating is not recording"), Client.Ledger->GetTotalRecordedEvidenceCount(), 0);

		const FTerritoryEvidenceMutationResult AfterHydration = Client.Ledger->RecordEvidence(
			CaptureCommitted, Subject, Heroes, FGameplayTag(), Context, Weight);
		TestFalse(TEXT("A hydrated client still cannot record"), AfterHydration.bSuccess);
		TestEqual(TEXT("The refused fact did not join the hydrated window"), Client.Ledger->GetRetainedEvidenceCount(), 1);
	}

	Client.Destroy();

	// ─── The server control: same payload, authority present ───
	FWorldFixture Server = MakeWorld(nullptr);
	if (!TestNotNull(TEXT("The server world exists"), Server.World)
		|| !TestNotNull(TEXT("The server ledger exists"), Server.Ledger))
	{
		Server.Destroy();
		return false;
	}

	Server.World->InitializeActorsForPlay(FURL());
	Server.World->BeginPlay();

	TestNotEqual(TEXT("The control world is not a client"),
		static_cast<int32>(Server.World->GetNetMode()), static_cast<int32>(NM_Client));
	TestTrue(TEXT("A server ledger is ready with no facts"), Server.Ledger->IsEvidenceLedgerReady());

	const FTerritoryEvidenceMutationResult Committed = Server.Ledger->RecordEvidence(
		CaptureCommitted, Subject, Heroes, FGameplayTag(), Context, Weight);
	TestTrue(TEXT("The server records the very fact the client was refused"), Committed.bSuccess);
	TestEqual(TEXT("The server retained it"), Server.Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("The committed fact keeps the caller's weight"), Committed.Record.Weight, Weight);
	TestEqual(TEXT("The server counts it as one commit"), Server.Ledger->GetTotalRecordedEvidenceCount(), 1);

	Server.Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
