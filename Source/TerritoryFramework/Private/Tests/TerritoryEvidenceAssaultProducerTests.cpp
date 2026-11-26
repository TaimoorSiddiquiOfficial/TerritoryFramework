// Copyright (c) Hop Trendy. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

namespace
{
	/**
	 * World, assault record and registered attacker the casualty producer needs.
	 *
	 * The record and the live participant are seeded through the same private seam the assault
	 * autonomy regression uses, so the test drives the real removal callback instead of a spawn,
	 * pathing and combat harness it does not need. The client build exists to prove that the
	 * callback refuses a client world before it can reach the ledger.
	 */
	struct FTFAssaultEvidenceFixture
	{
		UWorld* World = nullptr;
		UTerritoryCounterAttackSubsystem* Counter = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;
		ATerritoryAssaultCharacter* Attacker = nullptr;
		FTerritoryAssaultRecord Record;

		static constexpr int32 AttackerGuidSeed = 131;

		bool Build(const bool bClientWorld)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) return false;

			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			if (bClientWorld)
			{
				// No NetDriver here, so the engine derives the net mode from the travel URL
				// (UWorld::AttemptDeriveFromURL) and the world reports NM_Client.
				World->NextURL = TEXT("127.0.0.1:7777");
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			Attacker = World->SpawnActor<ATerritoryAssaultCharacter>(
				ATerritoryAssaultCharacter::StaticClass(), FTransform::Identity, SpawnParams);
			if (!Attacker) return false;
			Attacker->SetActorGUID_Implementation(FGuid(AttackerGuidSeed, 132, 133, 134));

			Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			if (!Counter || !Ledger) return false;

			Record.AssaultID = FGuid(141, 142, 143, 144);
			Record.TargetTerritoryGUID = FGuid(151, 152, 153, 154);
			Record.TargetTerritory =
				FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
			Record.AttackingFaction =
				FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
			Record.DefendingFaction =
				FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
			Record.State = ETerritoryAssaultState::Active;
			Record.PlannedForce = 2;
			Record.AliveForce = 1;
			Record.PendingReserveForce = 0;

			Attacker->AssaultParticipant->Configure(Record.AssaultID, Record.TargetTerritoryGUID,
				Record.TargetTerritory, Record.AttackingFaction);
			Attacker->AssaultParticipant->BeginPlay();
			// The record and the live registration are seeded by each test body: those containers
			// are private state, and only the test classes themselves hold the friend access.
			return true;
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
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceAssaultKilledProducer,
	"TerritoryFramework.Evidence.Producer.AttackerKilledRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceAssaultKilledProducer::RunTest(const FString& Parameters)
{
	FTFAssaultEvidenceFixture Fixture;
	if (!TestTrue(TEXT("Assault evidence fixture world was created"), Fixture.Build(false)))
	{
		Fixture.TearDown();
		return false;
	}

	// Seeded here rather than in the fixture: these containers are private state, and only the
	// automation test class holds the friend access that the existing assault tests rely on.
	Fixture.Counter->Assaults.Add(Fixture.Record.AssaultID, Fixture.Record);
	Fixture.Counter->LiveParticipants.FindOrAdd(Fixture.Record.AssaultID).Add(Fixture.Attacker);

	Fixture.Counter->NotifyParticipantRemoved(Fixture.Record.AssaultID, Fixture.Attacker, true);

	const TArray<FTerritoryEvidenceRecord> Rows = Fixture.Ledger->GetEvidenceRecords();
	if (!TestEqual(TEXT("One killed attacker writes exactly one durable fact"), Rows.Num(), 1))
	{
		Fixture.TearDown();
		return false;
	}

	const FTerritoryEvidenceRecord& Row = Rows[0];
	TestEqual(TEXT("The fact is the attacker-death tag"), Row.EvidenceTag,
		TerritoryEvidenceTags::AttackerKilled.GetTag());
	TestEqual(TEXT("The subject is the dead attacker's stable identity"), Row.SubjectId,
		Fixture.Attacker->GetActorGUID_Implementation());
	TestEqual(TEXT("The defender that holds the Territory is credited"), Row.InstigatorFaction,
		Fixture.Record.DefendingFaction);
	TestEqual(TEXT("The fact is attributed to the assaulted Territory"), Row.SourceTerritory,
		Fixture.Record.TargetTerritory);
	TestEqual(TEXT("One death is one unit of weight"), Row.Weight, 1);
	TestTrue(TEXT("A deterministic context id makes a replay idempotent"), Row.ContextId.IsValid());
	TestEqual(TEXT("The casualty is committed in the assault record"),
		Fixture.Counter->Assaults.FindChecked(Fixture.Record.AssaultID).KilledForce, 1);
	TestEqual(TEXT("Death removes one living attacker"),
		Fixture.Counter->Assaults.FindChecked(Fixture.Record.AssaultID).AliveForce, 0);

	// The same physical NPC cannot die twice: with no live registration left, the second report
	// is refused before it can reach the ledger, so the fact count must not move.
	Fixture.Counter->NotifyParticipantRemoved(Fixture.Record.AssaultID, Fixture.Attacker, true);
	TestEqual(TEXT("A repeated removal report cannot duplicate the fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("A refused report is not counted as a commit"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceAssaultKilledRefusedOnClient,
	"TerritoryFramework.Evidence.Producer.AttackerKilledRefusedOnClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceAssaultKilledRefusedOnClient::RunTest(const FString& Parameters)
{
	FTFAssaultEvidenceFixture Fixture;
	if (!TestTrue(TEXT("Client assault evidence fixture world was created"), Fixture.Build(true)))
	{
		Fixture.TearDown();
		return false;
	}

	TestEqual(TEXT("The fixture world really is a client"), Fixture.World->GetNetMode(), NM_Client);

	// Seeded through the same private seam as the authority case, so the refusal is proven for a
	// client that really does hold the record rather than for a fixture that never had one.
	Fixture.Counter->Assaults.Add(Fixture.Record.AssaultID, Fixture.Record);
	Fixture.Counter->LiveParticipants.FindOrAdd(Fixture.Record.AssaultID).Add(Fixture.Attacker);

	Fixture.Counter->NotifyParticipantRemoved(Fixture.Record.AssaultID, Fixture.Attacker, true);

	TestEqual(TEXT("A client cannot record an attacker death"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);
	TestEqual(TEXT("A client cannot count an evidence commit"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 0);
	TestEqual(TEXT("A client cannot consume the server's casualty bookkeeping"),
		Fixture.Counter->Assaults.FindChecked(Fixture.Record.AssaultID).KilledForce, 0);
	TestEqual(TEXT("A client cannot consume the server's living force"),
		Fixture.Counter->Assaults.FindChecked(Fixture.Record.AssaultID).AliveForce, 1);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
