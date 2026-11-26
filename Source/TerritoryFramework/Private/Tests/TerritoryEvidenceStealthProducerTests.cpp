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
#include "NarrativeStableActor.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

namespace
{
	/**
	 * World and actors the exposure producer needs.
	 *
	 * The world carries an authority GameMode because the ledger refuses every mutation in a
	 * client world, and the bClientWorld build exists to prove exactly that refusal rather than a
	 * setup mistake. The infiltrator is a guard character so it carries a durable identity: a
	 * player character has none, and the row would then rely on the acting faction alone.
	 */
	struct FTFStealthExposureFixture
	{
		UWorld* World = nullptr;
		ATerritoryVolume* Volume = nullptr;
		ATerritoryGuardCharacter* Infiltrator = nullptr;
		ATerritoryGuardCharacter* Observer = nullptr;
		UTerritoryControlSubsystem* Control = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;

		static constexpr int32 VolumeGuidSeed = 71;
		static constexpr int32 InfiltratorGuidSeed = 81;
		static constexpr int32 ObserverGuidSeed = 91;

		bool Build(const bool bClientWorld)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) return false;

			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());
			if (bClientWorld)
			{
				// The engine derives the net mode from an authored travel URL when no NetDriver
				// exists yet, which is the only stable way to build a client world in a test.
				World->NextURL = TEXT("127.0.0.1:7777");
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			Volume = World->SpawnActor<ATerritoryVolume>(
				ATerritoryVolume::StaticClass(), FTransform::Identity, SpawnParams);
			Infiltrator = World->SpawnActor<ATerritoryGuardCharacter>(
				ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, SpawnParams);
			Observer = World->SpawnActor<ATerritoryGuardCharacter>(
				ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, SpawnParams);
			if (!Volume || !Infiltrator || !Observer) return false;

			Volume->SetActorGUID_Implementation(FGuid(VolumeGuidSeed, 72, 73, 74));
			Infiltrator->SetActorGUID_Implementation(FGuid(InfiltratorGuidSeed, 82, 83, 84));
			Observer->SetActorGUID_Implementation(FGuid(ObserverGuidSeed, 92, 93, 94));
			Volume->SetRole(bClientWorld ? ROLE_SimulatedProxy : ROLE_Authority);

			Control = World->GetSubsystem<UTerritoryControlSubsystem>();
			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			return Control != nullptr && Ledger != nullptr;
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

/**
 * A committed exposure becomes one durable fact, and re-running the same episode writes nothing
 * new: the record is keyed by the episode itself, not by how often the producer was called.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceStealthExposedProducer,
	"TerritoryFramework.Evidence.Producer.StealthExposureRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceStealthExposedProducer::RunTest(const FString& Parameters)
{
	FTFStealthExposureFixture Fixture;
	if (!TestTrue(TEXT("Authority fixture builds"), Fixture.Build(/*bClientWorld=*/false)))
	{
		Fixture.TearDown();
		return false;
	}

	const FGuid InfiltratorGuid =
		INarrativeStableActor::Execute_GetActorGUID(Fixture.Infiltrator);
	Fixture.Control->RecordStealthExposedEvidence(
		*Fixture.Volume, *Fixture.Infiltrator, Fixture.Observer);

	const TArray<FTerritoryEvidenceRecord> Rows = Fixture.Ledger->GetEvidenceRecords();
	TestEqual(TEXT("One exposure writes exactly one row"), Rows.Num(), 1);
	if (Rows.Num() == 1)
	{
		const FTerritoryEvidenceRecord& Row = Rows[0];
		TestEqual(TEXT("The row is an exposure fact"), Row.EvidenceTag,
			TerritoryEvidenceTags::StealthExposed.GetTag());
		TestEqual(TEXT("The infiltrator is the subject"), Row.SubjectId, InfiltratorGuid);
		TestTrue(TEXT("The infiltrator keeps a durable identity"), Row.SubjectId.IsValid());
		TestEqual(TEXT("The fact is scoped to the Place it happened in"), Row.SourceTerritory,
			Fixture.Volume->GetTerritoryTag());
		TestEqual(TEXT("A single exposure weighs one"), Row.Weight, 1);
		TestTrue(TEXT("The episode carries a replay key"), Row.ContextId.IsValid());
	}

	// The same episode replayed is the same fact: the key absorbs it instead of counting twice.
	Fixture.Control->RecordStealthExposedEvidence(
		*Fixture.Volume, *Fixture.Infiltrator, Fixture.Observer);
	TestEqual(TEXT("Replaying one episode adds nothing"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);
	TestEqual(TEXT("And it is not counted as a commit either"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 1);

	Fixture.TearDown();
	return true;
}

/** Without authority an exposure is never a fact, and the ledger must not grow. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceStealthExposedRefusedOnClient,
	"TerritoryFramework.Evidence.Producer.StealthExposureRefusedOnClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceStealthExposedRefusedOnClient::RunTest(const FString& Parameters)
{
	FTFStealthExposureFixture Fixture;
	if (!TestTrue(TEXT("Client fixture builds"), Fixture.Build(/*bClientWorld=*/true)))
	{
		Fixture.TearDown();
		return false;
	}

	TestEqual(TEXT("The fixture really is a client world"),
		Fixture.World->GetNetMode(), NM_Client);

	Fixture.Control->RecordStealthExposedEvidence(
		*Fixture.Volume, *Fixture.Infiltrator, Fixture.Observer);

	TestEqual(TEXT("A client world records no exposure"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);
	TestEqual(TEXT("A client world commits no exposure"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 0);
	TestEqual(TEXT("A client world evicts nothing"), Fixture.Ledger->GetEvictedEvidenceCount(), 0);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
