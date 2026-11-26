// Copyright (c) Hop Trendy. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

namespace
{
	/**
	 * World and actors the defender-death producer needs.
	 *
	 * The world carries an authority GameMode because the ledger refuses every mutation in a
	 * client world, and the bClientWorld build exists to prove exactly that refusal rather than
	 * a setup mistake. Defenders are seeded into the private registration set, the same seam the
	 * existing defender-event regression uses, so the test exercises the death callback itself
	 * instead of an ASC binding and timer harness it does not need.
	 */
	struct FTFGuardKilledFixture
	{
		UWorld* World = nullptr;
		ATerritoryVolume* Volume = nullptr;
		ATerritoryGuardCharacter* Guard = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;

		static constexpr int32 VolumeGuidSeed = 41;
		static constexpr int32 GuardGuidSeed = 51;

		bool Build(const bool bClientWorld)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) return false;

			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());

			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			Volume = World->SpawnActor<ATerritoryVolume>(
				ATerritoryVolume::StaticClass(), FTransform::Identity, SpawnParams);
			Guard = World->SpawnActor<ATerritoryGuardCharacter>(
				ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, SpawnParams);
			if (!Volume || !Guard) return false;

			Volume->SetActorGUID_Implementation(FGuid(VolumeGuidSeed, 42, 43, 44));
			Guard->SetActorGUID_Implementation(FGuid(GuardGuidSeed, 52, 53, 54));
			Volume->SetRole(bClientWorld ? ROLE_SimulatedProxy : ROLE_Authority);

			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			return Ledger != nullptr;
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

/** A committed defender death becomes one durable evidence fact, exactly once. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardKilledEvidence,
	"TerritoryFramework.Evidence.Producer.GuardKilledRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardKilledEvidence::RunTest(const FString& Parameters)
{
	FTFGuardKilledFixture Fixture;
	if (!Fixture.Build(/*bClientWorld=*/false))
	{
		AddError(TEXT("Guard-death fixture failed to build an authority world"));
		Fixture.TearDown();
		return false;
	}

	// Seeded directly, as the defender-event regression does: the death callback is what this
	// test exercises, not the ASC binding and retry timer that normally fills this set.
	Fixture.Volume->RegisteredDefenders.Add(Fixture.Guard);
	Fixture.Volume->OnDefenderDied(Fixture.Guard, nullptr, true);

	TestEqual(TEXT("A committed defender death records exactly one fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);

	const TArray<FTerritoryEvidenceRecord> Rows = Fixture.Ledger->GetEvidenceRecords();
	if (Rows.Num() == 1)
	{
		TestEqual(TEXT("The fact is tagged as a guard kill"),
			Rows[0].EvidenceTag, TerritoryEvidenceTags::GuardKilled.GetTag());
		TestEqual(TEXT("The subject is the defender's stable GUID"),
			Rows[0].SubjectId, FGuid(FTFGuardKilledFixture::GuardGuidSeed, 52, 53, 54));
		TestEqual(TEXT("A unit death weighs one"),
			Rows[0].Weight, 1);
		TestFalse(TEXT("An environmental death carries no kill credit"),
			Rows[0].InstigatorFaction.IsValid());
		TestTrue(TEXT("The row carries a replay-safe context id"),
			Rows[0].ContextId.IsValid());
	}

	// The first callback unregistered the defender, so the duplicate is refused before it can
	// reach the producer - the same guard cannot record its death twice.
	Fixture.Volume->OnDefenderDied(Fixture.Guard, nullptr, true);
	TestEqual(TEXT("A duplicate death callback records nothing further"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("Only one commit was counted"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

/** Without authority the death never reaches the commit, so it must not reach the ledger. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardKilledRefusedWithoutAuthority,
	"TerritoryFramework.Evidence.Producer.GuardKilledRefusedOnClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardKilledRefusedWithoutAuthority::RunTest(const FString& Parameters)
{
	FTFGuardKilledFixture Fixture;
	if (!Fixture.Build(/*bClientWorld=*/true))
	{
		AddError(TEXT("Guard-death fixture failed to build a client world"));
		Fixture.TearDown();
		return false;
	}

	TestFalse(TEXT("The client fixture really is a client"),
		Fixture.Volume->HasAuthority());

	Fixture.Volume->RegisteredDefenders.Add(Fixture.Guard);
	Fixture.Volume->OnDefenderDied(Fixture.Guard, nullptr, true);

	TestEqual(TEXT("A client-side death callback records no fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);
	TestEqual(TEXT("Nothing was committed on the client"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 0);
	TestEqual(TEXT("The ledger counter was not advanced"),
		Fixture.Ledger->GetEvictedEvidenceCount(), 0);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
