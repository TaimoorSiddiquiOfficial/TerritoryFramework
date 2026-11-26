// Copyright (c) Hop Trendy. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/TerritoryDisguiseProfile.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "NarrativeStableActor.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"

namespace
{
	/**
	 * World and actors the disguise producer needs.
	 *
	 * The disguised actor is a guard character so it carries a durable identity; the observer is
	 * represented by the faction tag the compromise names, because that tag - not an actor - is
	 * what the disguise layer records as the acting side.
	 */
	struct FTFDisguiseCompromiseFixture
	{
		UWorld* World = nullptr;
		ATerritoryVolume* Volume = nullptr;
		ATerritoryGuardCharacter* Disguised = nullptr;
		UTerritoryDisguiseProfile* Profile = nullptr;
		UTerritoryDisguiseSubsystem* Disguises = nullptr;
		UTerritoryEvidenceLedgerSubsystem* Ledger = nullptr;

		FGameplayTag ObserverFaction;
		FGameplayTag OtherFaction;

		static constexpr int32 VolumeGuidSeed = 101;
		static constexpr int32 DisguisedGuidSeed = 111;

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
				World->NextURL = TEXT("127.0.0.1:7777");
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags |= RF_Transient;
			Volume = World->SpawnActor<ATerritoryVolume>(
				ATerritoryVolume::StaticClass(), FTransform::Identity, SpawnParams);
			Disguised = World->SpawnActor<ATerritoryGuardCharacter>(
				ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, SpawnParams);
			if (!Volume || !Disguised) return false;

			Volume->SetActorGUID_Implementation(FGuid(VolumeGuidSeed, 102, 103, 104));
			Disguised->SetActorGUID_Implementation(FGuid(DisguisedGuidSeed, 112, 113, 114));
			Volume->SetRole(bClientWorld ? ROLE_SimulatedProxy : ROLE_Authority);

			ObserverFaction = FGameplayTag::RequestGameplayTag(
				TEXT("Narrative.Factions.Bandits"), /*ErrorIfNotFound=*/false);
			OtherFaction = FGameplayTag::RequestGameplayTag(
				TEXT("Narrative.Factions.Heroes"), /*ErrorIfNotFound=*/false);
			if (!ObserverFaction.IsValid() || !OtherFaction.IsValid()) return false;

			Profile = NewObject<UTerritoryDisguiseProfile>(GetTransientPackage());
			Profile->PerceivedFaction = ObserverFaction;

			Disguises = World->GetSubsystem<UTerritoryDisguiseSubsystem>();
			Ledger = World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>();
			return Disguises != nullptr && Ledger != nullptr;
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
 * Burning one faction's cover is a durable fact, once per faction: the same faction burning it
 * again changes nothing and adds nothing, while a second faction burning it is a second fact.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceDisguiseCompromisedProducer,
	"TerritoryFramework.Evidence.Producer.DisguiseCompromisedRecordsFact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceDisguiseCompromisedProducer::RunTest(const FString& Parameters)
{
	FTFDisguiseCompromiseFixture Fixture;
	if (!TestTrue(TEXT("Authority fixture builds"), Fixture.Build(/*bClientWorld=*/false)))
	{
		Fixture.TearDown();
		return false;
	}

	if (!TestTrue(TEXT("A valid uniform activates"),
		Fixture.Disguises->ActivateDisguise(Fixture.Disguised, Fixture.Profile)))
	{
		Fixture.TearDown();
		return false;
	}
	TestEqual(TEXT("Activating a cover is not a compromise fact"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);

	const FGuid DisguisedGuid =
		INarrativeStableActor::Execute_GetActorGUID(Fixture.Disguised);
	TestTrue(TEXT("The cover is compromised for the observing faction"),
		Fixture.Disguises->CompromiseDisguise(
			Fixture.Disguised, Fixture.ObserverFaction, Fixture.Volume));

	const TArray<FTerritoryEvidenceRecord> Rows = Fixture.Ledger->GetEvidenceRecords();
	TestEqual(TEXT("One compromise writes exactly one row"), Rows.Num(), 1);
	if (Rows.Num() == 1)
	{
		const FTerritoryEvidenceRecord& Row = Rows[0];
		TestEqual(TEXT("The row is a compromise fact"), Row.EvidenceTag,
			TerritoryEvidenceTags::DisguiseCompromised.GetTag());
		TestEqual(TEXT("The disguised actor is the subject"), Row.SubjectId, DisguisedGuid);
		TestEqual(TEXT("The observing faction acted"), Row.InstigatorFaction,
			Fixture.ObserverFaction);
		TestEqual(TEXT("The fact is scoped to the Place it happened in"), Row.SourceTerritory,
			Fixture.Volume->GetTerritoryTag());
		TestEqual(TEXT("A single compromise weighs one"), Row.Weight, 1);
		TestTrue(TEXT("The episode carries a replay key"), Row.ContextId.IsValid());
	}

	// The refusal inside CompromiseDisguise is the guard: a repeated burn is not a new fact.
	TestFalse(TEXT("Burning the same cover for the same faction is refused"),
		Fixture.Disguises->CompromiseDisguise(
			Fixture.Disguised, Fixture.ObserverFaction, Fixture.Volume));
	TestEqual(TEXT("A refused repeat adds no row"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 1);

	// A different faction discovering the same cover is a different fact about the same actor.
	TestTrue(TEXT("A second faction can burn the same cover"),
		Fixture.Disguises->CompromiseDisguise(
			Fixture.Disguised, Fixture.OtherFaction, Fixture.Volume));
	TestEqual(TEXT("The second faction adds a second row"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 2);
	TestEqual(TEXT("Both rows are compromises"), Fixture.Ledger->GetEvidenceWeight(
		TerritoryEvidenceTags::DisguiseCompromised.GetTag()), 2);

	Fixture.TearDown();
	return true;
}

/** A client world cannot burn a cover at all, so it can never grow the ledger. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceDisguiseCompromisedRefusedOnClient,
	"TerritoryFramework.Evidence.Producer.DisguiseCompromiseRefusedOnClient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceDisguiseCompromisedRefusedOnClient::RunTest(const FString& Parameters)
{
	FTFDisguiseCompromiseFixture Fixture;
	if (!TestTrue(TEXT("Client fixture builds"), Fixture.Build(/*bClientWorld=*/true)))
	{
		Fixture.TearDown();
		return false;
	}

	TestEqual(TEXT("The fixture really is a client world"),
		Fixture.World->GetNetMode(), NM_Client);

	TestFalse(TEXT("A client world cannot activate a cover"),
		Fixture.Disguises->ActivateDisguise(Fixture.Disguised, Fixture.Profile));
	TestFalse(TEXT("A client world cannot burn a cover"),
		Fixture.Disguises->CompromiseDisguise(
			Fixture.Disguised, Fixture.ObserverFaction, Fixture.Volume));

	TestEqual(TEXT("A client world records no compromise"),
		Fixture.Ledger->GetRetainedEvidenceCount(), 0);
	TestEqual(TEXT("A client world commits no compromise"),
		Fixture.Ledger->GetTotalRecordedEvidenceCount(), 0);

	Fixture.TearDown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
