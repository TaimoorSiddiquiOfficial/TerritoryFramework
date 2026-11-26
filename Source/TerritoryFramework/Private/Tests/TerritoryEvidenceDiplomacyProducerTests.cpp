// Evidence producers — D1 Phase 2, batch 1: the diplomacy hook.
//
// A diplomacy act is authored (declare war, sign a treaty, break one), so each committed act is a
// physical consequence the ledger keeps. Expiry is the deliberate exception: nobody performs an
// expiry, so the ledger must stay silent about it even though diplomacy still reports the event.
//
// These tests drive the real subsystem mutators and read the ledger afterwards; they never call
// RecordEvidenceRecord themselves. The world fixture follows TerritoryEvidenceProducerTests.cpp,
// and the client-world seam (a travel URL deriving NM_Client) follows TerritoryEvidenceAuthorityTests.cpp.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

namespace
{
	/** One world with diplomacy and the evidence ledger in it. */
	struct FTFEvidenceDiplomacyFixture
	{
		UWorld* World = nullptr;
		ANarrativeGameState* GameState = nullptr;
		UTerritoryDiplomacySubsystem* Diplomacy = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;

		bool Build(FAutomationTestBase& Test, const bool bClientWorld = false)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!Test.TestNotNull(TEXT("Diplomacy world exists"), World))
			{
				return false;
			}
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

			if (bClientWorld)
			{
				// A test world has no NetDriver, so the travel URL is what derives the net mode
				// (UWorld::AttemptDeriveFromURL). The authority test uses the same seam.
				World->NextURL = TEXT("127.0.0.1:7777");
			}

			// Created, not spawned: SpawnActor on a vendor GameState runs the vendor save
			// subsystem's actor-identity path, which asserts on an unauthored GUID.
			GameState = NewObject<ANarrativeGameState>(World->PersistentLevel);
			World->SetGameState(GameState);

			Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			return Test.TestNotNull(TEXT("Diplomacy subsystem exists"), Diplomacy)
				&& Test.TestNotNull(TEXT("Evidence ledger exists"), Ledger);
		}

		/** Moves the vendor campaign clock. The member is protected, so it is written by reflection. */
		void SetCampaignClock(const float Seconds) const
		{
			if (FProperty* Property = FindFProperty<FProperty>(
					ANarrativeGameState::StaticClass(), TEXT("AccumulatedTime")))
			{
				Property->SetValue_InContainer(GameState, &Seconds);
			}
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

	/** The pair identity a treaty fact is filed under, built the same way the record builds it. */
	FGuid MakePairKey(const FGameplayTag& FactionA, const FGameplayTag& FactionB)
	{
		FTreatyRecord Pair;
		Pair.FactionA = FactionA;
		Pair.FactionB = FactionB;
		return Pair.GetCanonicalKey();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceDiplomacyAct,
	"TerritoryFramework.Evidence.Producer.DiplomacyActRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceDiplomacyAct::RunTest(const FString& Parameters)
{
	FTFEvidenceDiplomacyFixture Fixture;
	if (!Fixture.Build(*this))
	{
		Fixture.TearDown();
		return false;
	}

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	TestTrue(TEXT("The faction tags this case needs resolve"), Heroes.IsValid() && Bandits.IsValid());
	TestTrue(TEXT("A non-degenerate pair has a durable key"), MakePairKey(Heroes, Bandits).IsValid());
	TestTrue(TEXT("The pair key does not depend on argument order"),
		MakePairKey(Bandits, Heroes) == MakePairKey(Heroes, Bandits));

	Fixture.SetCampaignClock(25.f);
	TestEqual(TEXT("Nothing is recorded before any diplomacy act"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	Fixture.Diplomacy->DeclareWar(Heroes, Bandits);

	TestEqual(TEXT("Declaring war records exactly one fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);
	const TArray<FTerritoryEvidenceRecord> AfterWar = Fixture.Ledger->GetEvidenceRecords();
	if (AfterWar.Num() == 1)
	{
		TestEqual(TEXT("A declaration of war is a broken treaty"),
			AfterWar[0].EvidenceTag.ToString(), TerritoryEvidenceTags::TreatyBroken.GetTag().ToString());
		TestEqual(TEXT("The acting faction is the instigator"),
			AfterWar[0].InstigatorFaction.ToString(), Heroes.ToString());
		TestTrue(TEXT("The fact is filed under the faction pair, not under either faction alone"),
			AfterWar[0].SubjectId == MakePairKey(Bandits, Heroes));
		TestEqual(TEXT("One act carries weight one"), AfterWar[0].Weight, 1);
		TestEqual(TEXT("The fact carries the campaign time it happened at"),
			AfterWar[0].GameTimeSeconds, 25.0);
	}

	// Forming an alliance is a second, different fact about the same relationship.
	Fixture.Diplomacy->FormAlliance(Heroes, Bandits);
	TestEqual(TEXT("Signing an alliance records a second fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 2);
	TestEqual(TEXT("The war fact survives the alliance"),
		Fixture.Ledger->GetEvidenceWeight(TerritoryEvidenceTags::TreatyBroken.GetTag()), 1);
	TestEqual(TEXT("The alliance is counted separately from the war"),
		Fixture.Ledger->GetEvidenceWeight(TerritoryEvidenceTags::TreatySigned.GetTag()), 1);

	// Breaking an alliance is hostile again, and it is its own fact.
	Fixture.Diplomacy->BreakAlliance(Heroes, Bandits);
	TestEqual(TEXT("Breaking an alliance records a third fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 3);
	TestEqual(TEXT("Two broken-treaty facts now stand against the pair"),
		Fixture.Ledger->GetEvidenceWeight(TerritoryEvidenceTags::TreatyBroken.GetTag()), 2);
	TestEqual(TEXT("Every authored act also reached the diplomacy history"),
		Fixture.Diplomacy->GetDiplomacyHistory().Num(), 3);

	Fixture.TearDown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceDiplomacyRefused,
	"TerritoryFramework.Evidence.Producer.RejectedActWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceDiplomacyRefused::RunTest(const FString& Parameters)
{
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));

	// A client replica refuses every diplomacy mutation, so it can never produce a fact.
	{
		FTFEvidenceDiplomacyFixture Client;
		if (!Client.Build(*this, /*bClientWorld=*/true))
		{
			Client.TearDown();
			return false;
		}
		TestTrue(TEXT("The client fixture really is a client world"),
			Client.World->GetNetMode() == NM_Client);

		Client.Diplomacy->DeclareWar(Heroes, Bandits);
		TestEqual(TEXT("A refused diplomacy act writes no evidence"),
			Client.Ledger->GetRetainedEvidenceCount(), 0);
		TestEqual(TEXT("A refused diplomacy act writes no diplomacy history"),
			Client.Diplomacy->GetDiplomacyHistory().Num(), 0);
		Client.TearDown();
	}

	// On the authority, an act that changes nothing records nothing new.
	{
		FTFEvidenceDiplomacyFixture Server;
		if (!Server.Build(*this))
		{
			Server.TearDown();
			return false;
		}

		Server.Diplomacy->DeclareWar(Heroes, Bandits);
		TestEqual(TEXT("The authority records the first war declaration"),
			Server.Ledger->GetRetainedEvidenceCount(), 1);

		Server.Diplomacy->DeclareWar(Heroes, Bandits);
		TestEqual(TEXT("Repeating a war declaration adds no fact"),
			Server.Ledger->GetRetainedEvidenceCount(), 1);
		TestEqual(TEXT("Repeating a war declaration adds no event"),
			Server.Diplomacy->GetDiplomacyHistory().Num(), 1);

		Server.Diplomacy->BreakAlliance(Heroes, Bandits);
		TestEqual(TEXT("Breaking a treaty that does not exist adds no fact"),
			Server.Ledger->GetRetainedEvidenceCount(), 1);
		TestEqual(TEXT("Breaking a treaty that does not exist adds no event"),
			Server.Diplomacy->GetDiplomacyHistory().Num(), 1);
		Server.TearDown();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceDiplomacyExpiry,
	"TerritoryFramework.Evidence.Producer.ExpiredTreatyWritesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceDiplomacyExpiry::RunTest(const FString& Parameters)
{
	FTFEvidenceDiplomacyFixture Fixture;
	if (!Fixture.Build(*this))
	{
		Fixture.TearDown();
		return false;
	}

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));

	// A timed treaty restored from a save, already due to expire.
	FTreatyRecord Timed;
	Timed.FactionA = Heroes;
	Timed.FactionB = Bandits;
	Timed.State = EDiplomacyState::TradeAgreement;
	Timed.bPermanent = false;
	Timed.SignedGameTime = 5.f;
	Timed.ExpiryGameTime = 50.f;
	Fixture.Diplomacy->RestorePersistentState({ Timed }, TMap<FGameplayTag, int32>(), TArray<FDiplomacyEvent>());
	TestTrue(TEXT("The timed treaty is in force before it expires"),
		Fixture.Diplomacy->GetDiplomacyState(Heroes, Bandits) == EDiplomacyState::TradeAgreement);

	Fixture.SetCampaignClock(100.f);
	Fixture.Diplomacy->CheckTreatyExpirations();

	TestEqual(TEXT("The expiry is still reported as a diplomacy event"),
		Fixture.Diplomacy->GetDiplomacyHistory().Num(), 1);
	TestEqual(TEXT("An expiry nobody performed writes no evidence"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);
	TestTrue(TEXT("The timed treaty is gone"),
		Fixture.Diplomacy->GetDiplomacyState(Heroes, Bandits) == EDiplomacyState::None);

	// Control: the same world still records an authored act.
	Fixture.Diplomacy->DeclareWar(Heroes, Bandits);
	TestEqual(TEXT("An authored act in the same world still records a fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
