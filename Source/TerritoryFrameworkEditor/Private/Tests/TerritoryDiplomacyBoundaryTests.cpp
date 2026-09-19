#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyMutationBoundaries,
	"TerritoryFramework.Diplomacy.Regression.VerifiedEventsAndBoundedReputation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyMutationBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Diplomacy world exists"), World)) return false;
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Diplomacy->DeclareWar(Heroes, Heroes);
	Diplomacy->DeclarePeace(Heroes, FGameplayTag());
	Diplomacy->FormAlliance(FGameplayTag(), Bandits);
	Diplomacy->SignNonAggression(Heroes, Heroes);
	TestEqual(TEXT("Rejected pairs never produce successful diplomacy events"), Diplomacy->GetDiplomacyHistory().Num(), 0);
	TestEqual(TEXT("Rejected pairs never create treaties"), Diplomacy->GetAllTreaties().Num(), 0);
	Diplomacy->RestorePersistentState({}, {}, {});
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>(World);
	Probe->DiplomacyCallback = [&](FGameplayTag A, FGameplayTag B, EDiplomacyState State)
	{
		if (State == EDiplomacyState::Alliance) Diplomacy->DeclareWar(A, B);
	};
	Diplomacy->OnDiplomacyStateChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::DiplomacyChanged);
	Diplomacy->FormAlliance(Heroes, Bandits);
	TestEqual(TEXT("Callback-authored final treaty is authoritative"), Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::War);
	TestEqual(TEXT("Superseded alliance does not record false success"), Diplomacy->GetDiplomacyHistory().Num(), 1);
	Diplomacy->OnDiplomacyStateChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::DiplomacyChanged);
	Diplomacy->DeclareWar(Heroes, Bandits);
	TestEqual(TEXT("Idempotent war does not duplicate history"), Diplomacy->GetDiplomacyHistory().Num(), 1);
	Diplomacy->SetReputation(Heroes, MAX_int32);
	Diplomacy->AddReputation(Heroes, 10);
	TestEqual(TEXT("Positive reputation saturates safely"), Diplomacy->GetReputation(Heroes), MAX_int32);
	Diplomacy->SetReputation(Heroes, MIN_int32);
	Diplomacy->AddReputation(Heroes, -10);
	TestEqual(TEXT("Negative reputation saturates safely"), Diplomacy->GetReputation(Heroes), MIN_int32);
	Diplomacy->RestorePersistentState({}, {}, {});
	Diplomacy->SignTradeAgreement(Heroes, Bandits, 10.f);
	TestEqual(TEXT("Timed treaty without a Narrative clock is rejected"), Diplomacy->GetAllTreaties().Num(), 0);
	TestEqual(TEXT("Rejected timed treaty has no success event"), Diplomacy->GetDiplomacyHistory().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyRestoreBoundaries,
	"TerritoryFramework.Diplomacy.Regression.RestoreCanonicalPairs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyRestoreBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Treaty restore world exists"), World)) return false;
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	FTreatyRecord First;
	First.FactionA = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	First.FactionB = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	First.State = EDiplomacyState::Alliance;
	First.SignedGameTime = 1.f;
	FTreatyRecord Newer = First;
	Swap(Newer.FactionA, Newer.FactionB);
	Newer.State = EDiplomacyState::Ceasefire;
	Newer.SignedGameTime = 2.f;
	Newer.bPermanent = false;
	Newer.ExpiryGameTime = 20.f;
	Diplomacy->RestorePersistentState({First, Newer}, {}, {});
	TestEqual(TEXT("A canonical pair has one restored authority"), Diplomacy->GetAllTreaties().Num(), 1);
	TestEqual(TEXT("Newest signed duplicate survives migration"), Diplomacy->GetDiplomacyState(First.FactionA, First.FactionB), EDiplomacyState::Ceasefire);
	const TArray<FTreatyRecord> Saved = Diplomacy->GetAllTreaties();
	Diplomacy->RestorePersistentState({}, {}, {});
	Diplomacy->RestorePersistentState(Saved, {}, {});
	TestEqual(TEXT("Restoring twice stays idempotent"), Diplomacy->GetAllTreaties().Num(), 1);
	if (Diplomacy->GetAllTreaties().Num() == 1)
	{
		TestEqual(TEXT("Migration preserves timed expiry"), Diplomacy->GetAllTreaties()[0].ExpiryGameTime, 20.f);
	}
	Diplomacy->RestorePersistentState({Newer, First}, {}, {});
	TestEqual(TEXT("Duplicate selection is independent of save array order"), Diplomacy->GetDiplomacyState(First.FactionA, First.FactionB), EDiplomacyState::Ceasefire);
	TestEqual(TEXT("Restoration never invents story events"), Diplomacy->GetDiplomacyHistory().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFReputationDeclaresDiplomacy,
	"TerritoryFramework.Diplomacy.Regression.ReputationDeclaresDiplomacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFReputationDeclaresDiplomacy::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Diplomacy world exists"), World)) return false;
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();

	// Off by default. This is the assertion that protects every existing project: a
	// number their quests already move must not start declaring wars on its own.
	TestFalse(TEXT("Reputation-driven diplomacy is off by default"),
		Settings->bReputationDrivesDiplomacy);
	TestFalse(TEXT("The subsystem reports the policy as off"),
		Diplomacy->IsReputationDrivenDiplomacyEnabled());
	Diplomacy->SetReputation(Bandits, -1000);
	TestEqual(TEXT("With the policy off, ruined reputation declares nothing"),
		Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::None);

	// This world has no player controller, so an unset subject must resolve to nothing
	// rather than to some guess. Turning the policy on alone must still change nothing.
	TestFalse(TEXT("No player controller means no resolvable subject"),
		Diplomacy->GetReputationSubjectFaction().IsValid());

	{
		TGuardValue<bool> Policy(Settings->bReputationDrivesDiplomacy, true);
		TestTrue(TEXT("The policy reads as on once enabled"),
			Diplomacy->IsReputationDrivenDiplomacyEnabled());

		Diplomacy->SetReputation(Bandits, -1000);
		TestEqual(TEXT("Still nothing: an unresolved subject is never guessed at"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::None);

		Diplomacy->SetReputationSubjectFaction(Heroes);
		TestEqual(TEXT("The authored subject is used verbatim"),
			Diplomacy->GetReputationSubjectFaction(), Heroes);

		Diplomacy->SetReputation(Bandits, -100);
		TestEqual(TEXT("Reputation below the hostile threshold declares War"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::War);

		const TArray<FTreatyRecord> Declared = Diplomacy->GetAllTreaties();
		TestEqual(TEXT("Reputation declares exactly one treaty"), Declared.Num(), 1);
		if (Declared.Num() == 1)
		{
			TestTrue(TEXT("The treaty records that reputation owns it"),
				Declared[0].bReputationDerived);
		}

		// Reputation raised back out of hostile must withdraw the war it declared.
		// Otherwise a player could never earn their way back out of a bad reputation.
		Diplomacy->SetReputation(Bandits, 0);
		TestEqual(TEXT("Recovering reputation withdraws the war it declared"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::None);
		TestEqual(TEXT("Withdrawing leaves no treaty behind"),
			Diplomacy->GetAllTreaties().Num(), 0);

		Diplomacy->SetReputation(Bandits, 100);
		TestEqual(TEXT("Reputation above the allied threshold declares an Alliance"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::Alliance);

		// An authored treaty is an explicit statement and outranks the number.
		Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::Ceasefire);
		Diplomacy->SetReputation(Bandits, -100);
		TestEqual(TEXT("An authored treaty is not overwritten by reputation"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::Ceasefire);

		Diplomacy->SetReputation(Bandits, 100);
		TestEqual(TEXT("Reputation also does not raise a treaty it never owned"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::Ceasefire);

		// Once authored, the treaty stays authored even after reputation moves again.
		const TArray<FTreatyRecord> Authored = Diplomacy->GetAllTreaties();
		if (TestEqual(TEXT("The authored treaty is still the only one"), Authored.Num(), 1))
		{
			TestFalse(TEXT("An authored write takes ownership away from reputation"),
				Authored[0].bReputationDerived);
		}

		Diplomacy->SetReputation(Bandits, 0);
		TestEqual(TEXT("Reputation returning to neutral spares the authored treaty"),
			Diplomacy->GetDiplomacyState(Bandits, Heroes), EDiplomacyState::Ceasefire);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFReputationExplicitSubject,
	"TerritoryFramework.Diplomacy.Regression.ExplicitSubjectIgnoresPlayerOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFReputationExplicitSubject::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World exists"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();
	TGuardValue<bool> Policy(Settings->bReputationDrivesDiplomacy, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	TArray<ANarrativePlayerController*> Controllers;
	for (FGameplayTag Faction : {Heroes, Bandits})
	{
		auto* State = World->SpawnActor<ANarrativePlayerState>();
		State->SetFactions(FGameplayTagContainer(Faction));
		auto* Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
		Character->SetRole(ROLE_Authority);
		Character->SetPlayerState(State);
		auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		Controller->SetRole(ROLE_Authority);
		Controller->SetPlayerState(State);
		Controller->SetOwnedCharacter(Character);
		Controller->SetPawn(Character);
		World->AddController(Controller);
		Controllers.Add(Controller);
		TestEqual(TEXT("Fixture exposes a real Narrative faction"),
			UTerritoryBlueprintLibrary::GetActorPrimaryFaction(Diplomacy, Character), Faction);
	}
	for (int32 Order = 0; Order < 2; ++Order)
	{
		Diplomacy->SetReputation(Bandits, -100);
		TestFalse(TEXT("Connected players never choose the campaign subject"),
			Diplomacy->GetReputationSubjectFaction().IsValid());
		TestEqual(TEXT("Unselected subject cannot declare war on either player"),
			Diplomacy->GetAllTreaties().Num(), 0);
		World->RemoveController(Controllers[Order]);
		World->AddController(Controllers[Order]);
	}
	Diplomacy->SetReputationSubjectFaction(Heroes);
	Controllers[0]->GetPlayerState<ANarrativePlayerState>()->SetFactions(FGameplayTagContainer(Bandits));
	TestEqual(TEXT("A player's faction switch does not silently change the campaign ledger"),
		Diplomacy->GetReputationSubjectFaction(), Heroes);
	Diplomacy->SetReputation(Bandits, -100);
	TestTrue(TEXT("An explicit subject allows the intended war"), Diplomacy->IsAtWar(Heroes, Bandits));
	Diplomacy->SetReputationSubjectFaction(FGameplayTag());
	Diplomacy->SetReputation(Bandits, 100);
	TestTrue(TEXT("Clearing the subject pauses treaty changes without guessing a player"),
		Diplomacy->IsAtWar(Heroes, Bandits));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFReputationPersistenceRoundTrip,
	"TerritoryFramework.Diplomacy.SaveLoad.SubjectAndTreatyProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFReputationPersistenceRoundTrip::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Save world exists"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();
	TGuardValue<bool> Policy(Settings->bReputationDrivesDiplomacy, true);
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->SetActorGUID_Implementation(FGuid(291, 292, 293, 294));
	State->SubscribeToLiveUpdates();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	FNarrativeActorRecord EmptyRecord;
	TestTrue(TEXT("Narrative saves the default subject"), Save->CreateActorRecord(State, EmptyRecord));
	Diplomacy->SetReputationSubjectFaction(Heroes);
	TestEqual(TEXT("Subject publishes without waiting for a score change or save"),
		State->ReplicatedReputationSubjectFaction, Heroes);
	Diplomacy->SetReputation(Bandits, -100);
	TestTrue(TEXT("Live treaty snapshot sees provenance before callbacks finish"),
		State->GetTreatyBetween(Heroes, Bandits).bReputationDerived);
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative saves the chosen subject and war"), Save->CreateActorRecord(State, Record));
	Diplomacy->RestorePersistentState({}, {}, {});
	Save->LoadActorFromRecord(State, Record);
	TestEqual(TEXT("Actual Narrative actor load restores subject"), Diplomacy->GetReputationSubjectFaction(), Heroes);
	TestTrue(TEXT("Actual Narrative actor load restores provenance"),
		State->GetTreatyBetween(Heroes, Bandits).bReputationDerived);
	Diplomacy->SetReputation(Bandits, 0);
	TestFalse(TEXT("Reputation can end its own war after loading"), Diplomacy->IsAtWar(Heroes, Bandits));

	Diplomacy->SetReputation(Bandits, -100);
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
	TestFalse(TEXT("A quest claiming the same war publishes authored provenance immediately"),
		State->GetTreatyBetween(Heroes, Bandits).bReputationDerived);
	TestTrue(TEXT("Narrative saves the authored war"), Save->CreateActorRecord(State, Record));
	Diplomacy->RestorePersistentState({}, {}, {});
	Save->LoadActorFromRecord(State, Record);
	Diplomacy->SetReputation(Bandits, 0);
	TestTrue(TEXT("Reputation preserves the quest's war after loading"), Diplomacy->IsAtWar(Heroes, Bandits));

	// Exercise the same OnRep hydration used by each client, without a live socket.
	UWorld* ClientWorld = UWorld::CreateWorld(EWorldType::Game, false);
	for (int32 ClientIndex = 0; ClientIndex < 2; ++ClientIndex)
	{
		auto* Replica = ClientWorld->SpawnActor<ATerritoryWorldState>();
		Replica->SetRole(ROLE_SimulatedProxy);
		Replica->ReplicatedReputationSubjectFaction = State->ReplicatedReputationSubjectFaction;
		Replica->ReplicatedReputation = State->ReplicatedReputation;
		Replica->ReplicatedTreaties = State->ReplicatedTreaties;
		Replica->OnRep_DiplomacyState();
		auto* ClientDiplomacy = ClientWorld->GetSubsystem<UTerritoryDiplomacySubsystem>();
		TestEqual(TEXT("Replica hydration restores the explicit subject"),
			ClientDiplomacy->GetReputationSubjectFaction(), Heroes);
		TestTrue(TEXT("Replica hydration restores the authored war"), ClientDiplomacy->IsAtWar(Heroes, Bandits));
		Replica->Destroy();
	}
	ClientWorld->DestroyWorld(false);
	Save->LoadActorFromRecord(State, EmptyRecord);
	TestFalse(TEXT("Loading an empty campaign clears a previous subject"),
		Diplomacy->GetReputationSubjectFaction().IsValid());
	TestFalse(TEXT("Empty campaign clears the replicated subject too"),
		State->ReplicatedReputationSubjectFaction.IsValid());
	State->UnsubscribeFromLiveUpdates();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFReputationAuthoredCallback,
	"TerritoryFramework.Diplomacy.Regression.ReputationRespectsAuthoredCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFReputationAuthoredCallback::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Callback world exists"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();
	TGuardValue<bool> Policy(Settings->bReputationDrivesDiplomacy, true);
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	Probe->DiplomacyCallback = [this, Diplomacy](FGameplayTag A, FGameplayTag B, EDiplomacyState State)
	{
		if (State != EDiplomacyState::War) return;
		const TArray<FTreatyRecord> Treaties = Diplomacy->GetAllTreaties();
		TestTrue(TEXT("Observers see reputation ownership during the original notification"),
			Treaties.Num() == 1 && Treaties[0].bReputationDerived);
		Diplomacy->SetDiplomacyState(A, B, EDiplomacyState::Ceasefire);
	};
	Diplomacy->OnDiplomacyStateChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::DiplomacyChanged);
	Diplomacy->SetReputationSubjectFaction(Heroes);
	Diplomacy->SetReputation(Bandits, -100);
	TestEqual(TEXT("The callback's authored ceasefire wins"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::Ceasefire);
	Diplomacy->SetReputation(Bandits, 100);
	TestEqual(TEXT("Later reputation cannot overwrite that authored ceasefire"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::Ceasefire);
	Diplomacy->OnDiplomacyStateChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::DiplomacyChanged);
	World->DestroyWorld(false);
	return true;
}

#endif
