#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyWorldStateLiveBridge,
	"TerritoryFramework.Diplomacy.Replication.RichTreatyAndHistoryStayCurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyWorldStateLiveBridge::RunTest(const FString& Parameters)
{
	// An EditorPreview world assigns spawned actors ROLE_None because it is neither a
	// server nor a client. That fixture made every authority-only live handler return
	// before exercising the replication read model. A transient actor has the same
	// authority role as a server-side WorldState without inventing a fake net world.
	ATerritoryWorldState* WorldState = NewObject<ATerritoryWorldState>();
	TestNotNull(TEXT("Territory WorldState created"), WorldState);
	if (!WorldState) return false;
	TestTrue(TEXT("Live bridge fixture executes with server authority"),
		WorldState->HasAuthority());

	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	TestTrue(TEXT("Test faction tags resolve"), Bandits.IsValid() && Heroes.IsValid());

	FReplicatedTreaty Existing;
	Existing.TreatyID = FGuid::NewGuid();
	Existing.FactionA = Bandits;
	Existing.FactionB = Heroes;
	Existing.State = EDiplomacyState::TradeAgreement;
	Existing.SignedGameTime = 12.0;
	Existing.ExpiryGameTime = 97.0;
	Existing.bPermanent = false;
	WorldState->SetTreaty(Existing);

	UFunction* StateHandler = WorldState->FindFunction(TEXT("OnDiplomacyChangedLive"));
	TestNotNull(TEXT("Live treaty bridge is bound as a UFUNCTION"), StateHandler);
	if (StateHandler)
	{
		WorldState->OnDiplomacyChangedLive(Bandits, Heroes, EDiplomacyState::Ceasefire);
		const FReplicatedTreaty Changed = WorldState->GetTreatyBetween(Bandits, Heroes);
		TestEqual(TEXT("Live state mutation reaches the client treaty row"),
			Changed.State, EDiplomacyState::Ceasefire);
		TestEqual(TEXT("A transient lookup gap does not erase signed time"),
			Changed.SignedGameTime, 12.0);
		TestEqual(TEXT("A transient lookup gap does not erase treaty expiry"),
			Changed.ExpiryGameTime, 97.0);
		TestFalse(TEXT("A transient lookup gap preserves timed-treaty policy"),
			Changed.bPermanent);

		WorldState->OnDiplomacyChangedLive(Bandits, Heroes, EDiplomacyState::None);
		TestEqual(TEXT("None removes the replicated treaty instead of retaining a ghost row"),
			WorldState->GetAllTreaties().Num(), 0);
	}

	UFunction* EventHandler = WorldState->FindFunction(TEXT("OnDiplomacyEventLive"));
	TestNotNull(TEXT("Live diplomacy-history bridge is bound as a UFUNCTION"), EventHandler);
	if (EventHandler)
	{
		for (int32 Index = 0; Index < 505; ++Index)
		{
			FDiplomacyEvent Event;
			Event.FactionA = Bandits;
			Event.FactionB = Heroes;
			Event.GameTime = static_cast<float>(Index);
			WorldState->OnDiplomacyEventLive(Event);
		}

		const FArrayProperty* HistoryProperty = FindFProperty<FArrayProperty>(
			ATerritoryWorldState::StaticClass(), TEXT("ReplicatedDiplomacyHistory"));
		TestNotNull(TEXT("Replicated diplomacy history property exists"), HistoryProperty);
		if (HistoryProperty)
		{
			void* HistoryAddress = HistoryProperty->ContainerPtrToValuePtr<void>(WorldState);
			FScriptArrayHelper History(HistoryProperty, HistoryAddress);
			TestEqual(TEXT("Live diplomacy history remains bounded"), History.Num(), 500);
			if (History.Num() == 500)
			{
				const FDiplomacyEvent* First =
					reinterpret_cast<const FDiplomacyEvent*>(History.GetRawPtr(0));
				const FDiplomacyEvent* Last =
					reinterpret_cast<const FDiplomacyEvent*>(History.GetRawPtr(History.Num() - 1));
				TestEqual(TEXT("History trims oldest live events"), First->GameTime, 5.f);
				TestEqual(TEXT("History retains newest live event"), Last->GameTime, 504.f);
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyStandingTreatyPublishesAtStartup,
	"TerritoryFramework.Diplomacy.Replication.StandingTreatyPublishesAtWorldBeginPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The test above drives OnDiplomacyChangedLive directly, so it passes whether or not the
// live delegate is ever bound. This one runs the real world ordering instead: a standing war
// authored in project faction data, loaded by the subsystem at OnWorldBeginPlay, with a
// placed WorldState that subscribes immediately afterwards. It is the control for the
// startup publication gap — the standing war is what a client currently never hears about.
bool FTFDiplomacyStandingTreatyPublishesAtStartup::RunTest(const FString& Parameters)
{
	// This fixture deliberately authors no WorldStateGUID so the save path stays out of the
	// way and live publication is the only thing under test.
	AddExpectedError(TEXT("has no authored WorldStateGUID"),
		EAutomationExpectedErrorFlags::Contains, 1);

	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	if (!TestTrue(TEXT("Test faction tags resolve"), Bandits.IsValid() && Heroes.IsValid()))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Startup publication world exists"), World))
	{
		return false;
	}
	WorldContext.SetCurrentWorld(World);

	auto Teardown = [World]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	// The session begins with a standing war already authored in project faction data. A
	// live session was observed to start exactly here: the subsystem knows the war, and the
	// replicated read model carried none of it.
	//
	// Created with NewObject rather than SpawnActor deliberately. SpawnActor fires the
	// vendor save subsystem's OnActorSpawned, which calls Execute_GetActorGUID on anything
	// implementing the stable-actor interface; ANarrativeGameState implements it without a
	// native GetActorGUID, so the interface default asserts and takes the process down.
	// NewObject bypasses spawn handlers, and the actor only needs to exist as the world's
	// GameState for LoadFromGameState to read it.
	ANarrativeGameState* GameState = NewObject<ANarrativeGameState>(World->PersistentLevel);
	if (!TestNotNull(TEXT("Narrative GameState fixture exists"), GameState))
	{
		Teardown();
		return false;
	}
	FFactionAttitudeData BanditView;
	BanditView.AttitudeMap.Add(Heroes, ETeamAttitude::Hostile);
	GameState->FactionAllianceMap.Add(Bandits, BanditView);
	FFactionAttitudeData HeroView;
	HeroView.AttitudeMap.Add(Bandits, ETeamAttitude::Hostile);
	GameState->FactionAllianceMap.Add(Heroes, HeroView);
	World->SetGameState(GameState);

	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!TestNotNull(TEXT("Diplomacy subsystem exists"), Diplomacy))
	{
		Teardown();
		return false;
	}

	// Same ordering as a real load: actors initialize first, then BeginPlay runs the world
	// subsystems and only afterwards the actors. The WorldState is present the whole time,
	// exactly as the placed TerritoryWorldState_1 is in the shipping level.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryWorldState* WorldState = World->SpawnActor<ATerritoryWorldState>(
		ATerritoryWorldState::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Placed Territory WorldState exists"), WorldState))
	{
		Teardown();
		return false;
	}
	WorldState->SetRole(ROLE_Authority);

	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	// Premise control. Were the fixture failing to load a standing war, an empty replicated
	// model below would look like the defect while actually being a broken fixture.
	TestEqual(TEXT("Fixture loaded the standing war into the authoritative subsystem"),
		Diplomacy->GetAllTreaties().Num(), 1);
	// Pins the fixture's semantics, so a future change to the attitude mapping reports here
	// rather than masquerading as a publication failure two assertions further down.
	TestEqual(TEXT("Standing hostility reads as War, not as a treaty-free world"),
		Diplomacy->AttitudeToDiplomacyState(ETeamAttitude::Hostile), EDiplomacyState::War);

	WorldState->DispatchBeginPlay();
	// The assertion the delegate-free test above cannot make: deleting the AddDynamic in
	// SubscribeToLiveUpdates leaves that test green, and would turn this one red.
	TestTrue(TEXT("Authoritative WorldState bound the live diplomacy delegate"),
		Diplomacy->OnDiplomacyStateChanged.Contains(
			WorldState, TEXT("OnDiplomacyChangedLive")));

	// The defect this test exists for. The delegate is bound and live changes do replicate,
	// but the standing war that predates the subscription never reaches the read model.
	TestEqual(TEXT("A standing treaty reaches the replicated model at world begin"),
		WorldState->GetAllTreaties().Num(), 1);
	TestEqual(TEXT("The standing treaty replicates as War, not as a default row"),
		WorldState->GetTreatyBetween(Bandits, Heroes).State, EDiplomacyState::War);

	Teardown();
	return true;
}

#endif
