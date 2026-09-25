#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/NarrativeLevelSequencePlayer.h"
#include "Cinematics/TerritoryCutsceneTeardown.h"
#include "Cinematics/TerritoryDialogueShot.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TerritoryStoryEvents.h"
#include "TerritoryCinematicAudienceProbe.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/UnrealType.h"

/**
 * The paused-guard seam. HandleSequenceStopped is private because only the sequence player's own
 * delegates should reach it, but the guard it holds - a paused player has not stopped - cannot be
 * reached any other way: the engine broadcasts OnFinished after Pause() only from
 * FinishPlaybackInternal, which a test cannot trigger without driving playback to its authored end.
 * Territory's light rig lifecycle test reaches its equivalent end-of-playback handler the same way.
 */
class FTFTerritoryCutsceneTeardownTestAccess
{
public:
	static void SequenceStopped(UTerritoryCutsceneTeardownComponent* Component)
	{
		if (Component) Component->HandleSequenceStopped();
	}

	/**
	 * One turn of the audience reconcile.
	 *
	 * The reconcile's real entry point is the world's MovieSceneSequenceTick delegate, which only
	 * fires from UWorld::Tick, and the component cannot tick any other way - ALevelSequenceActor
	 * leaves bCanEverTick false and RegisterActorTickFunctions is protected. A headless fixture world
	 * has no game loop, so the test drives the handler directly rather than ticking the world.
	 *
	 * This covers the reconcile's *logic*, and only its logic. Two things it cannot show, both of
	 * them stated here rather than left to be inferred from a green run:
	 *
	 * That the world's sequence tick delegate is what actually invokes the handler in play. The
	 * reconcile subscribes to it and nothing in a headless fixture can fire it.
	 *
	 * And the registration-order consequence that follows from being on that delegate - the tick
	 * multicast invokes in reverse registration order, so the release lands one frame after the
	 * engine's suppression. Every assertion here drives the handler by hand, so it would read the
	 * same if the delegate were never wired at all.
	 *
	 * The listen-server harness was run for this batch and does not close either gap: its fixture
	 * authors a faction audience with no explicit controllers, so on a listen server the vendor's
	 * fallback resolves that audience to the authority world's only local controller and the
	 * reconcile has nothing to release. It proves the audience *bound* - no local controller was
	 * frozen without being named by the sequence that froze it - and not the release. See
	 * Docs/Verification/CUTSCENE_AUDIENCE_2026-09-24.md, which records the gap and what a fixture
	 * would need in order to close it.
	 */
	static void SequenceTick(UTerritoryCutsceneTeardownComponent* Component)
	{
		if (Component) Component->HandleSequenceTick(0.f);
	}

	/**
	 * One play-start, driven directly.
	 *
	 * Unlike SequenceStopped, this one is *not* here because the engine call is out of reach: Play()
	 * runs synchronously in a headless fixture - NeedsQueueLatentAction is IsEvaluating(), which is
	 * false outside an evaluation callback - so a test can reach the real OnPlay by resuming a paused
	 * player, and does exactly that where the assertion is about the engine. This seam is for the other
	 * kind of leg: the debt is incurred at a play-start whether or not playback is still running, so a
	 * leg that wants to assert what an *unanswered* debt does has to be able to open one without the
	 * engine also applying the suppression that the same call triggers.
	 */
	static void SequenceStarted(UTerritoryCutsceneTeardownComponent* Component)
	{
		if (Component) Component->HandleSequenceStarted();
	}

	/** How many controllers the reconcile resolved as outside the audience. */
	static int32 UncoveredCount(const UTerritoryCutsceneTeardownComponent* Component)
	{
		return Component ? Component->UncoveredControllers.Num() : 0;
	}

	/** How many of them are still owed a release for the current play-run. */
	static int32 OwedCount(const UTerritoryCutsceneTeardownComponent* Component)
	{
		return Component ? Component->OwedReleases.Num() : 0;
	}

	/** Whether the component is currently subscribed to the world's sequence tick. */
	static bool HasTickSubscription(const UTerritoryCutsceneTeardownComponent* Component)
	{
		return Component && Component->SequenceTickHandle.IsValid();
	}

	/** Whether the component is currently answering this player's play-starts. */
	static bool HasPlayStartBinding(const UTerritoryCutsceneTeardownComponent* Component)
	{
		return Component && Component->SequencePlayer.IsValid()
			&& Component->SequencePlayer->OnPlay.Contains(Component,
				GET_FUNCTION_NAME_CHECKED(UTerritoryCutsceneTeardownComponent, HandleSequenceStarted));
	}

	/** Whether the one teardown has already been armed for this actor. */
	static bool IsArmed(const UTerritoryCutsceneTeardownComponent* Component)
	{
		return Component && Component->bTeardownArmed;
	}
};

namespace TerritoryCutsceneTests
{
	/**
	 * The isolated-world fixture the other Territory cinematic tests use. The Narrative clock is
	 * built with NewObject rather than spawned: spawning a Narrative game state asserts inside the
	 * save subsystem's spawn handler.
	 */
	struct FWorldFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ANarrativeGameState* Clock = nullptr;

		FWorldFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());
			Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
			Clock->SetRole(ROLE_Authority);
			World->SetGameState(Clock);
			// Required before anything is spawned in this world. AActor::PostActorConstruction marks
			// an actor garbage when World->AreActorsInitialized() is false, and that flag is only set
			// here (UWorld::InitializeActorsForPlay, World.cpp:6021) - so a controller spawned without
			// it never reaches AController::PostInitializeComponents, never calls UWorld::AddController,
			// and is therefore absent from GetPlayerControllerIterator. The faction resolver walks that
			// iterator, so without this call no audience is ever found.
			World->InitializeActorsForPlay(FURL());
		}

		~FWorldFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};

	/**
	 * A viewer whose pawn carries the faction. The pawn is what real players put their Narrative
	 * membership on, and ResolveFactionPlayerContext matches the controller, its pawn and its
	 * player state.
	 *
	 * The controller is given a real local player because a cutscene viewer is a local player in
	 * production: ULevelSequencePlayer::EnableCinematicMode suppresses input on local controllers,
	 * so a fixture without one would not exercise the path a real audience takes.
	 *
	 * Templated so the audience tests can build the recording subclass through the same recipe: a
	 * controller that is set up differently from production's would not prove anything about the
	 * audience the engine resolves.
	 */
	template <typename ControllerType>
	ControllerType* MakeLocalViewer(UWorld* World, const FGameplayTag& Faction)
	{
		if (!World) return nullptr;
		UGameInstance* Instance = World->GetGameInstance();
		ControllerType* Viewer = World->SpawnActor<ControllerType>();
		ATerritoryGuardCharacter* Body = World->SpawnActor<ATerritoryGuardCharacter>();
		if (!Viewer || !Body) return nullptr;
		if (Instance)
		{
			// ULocalPlayer is ClassWithin=Engine, so its outer must be the engine even though the
			// game instance owns it.
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			if (LocalPlayer)
			{
				Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
				Viewer->SetPlayer(LocalPlayer);
			}
		}
		Viewer->Possess(Body);
		if (INarrativeTeamAgentInterface* TeamAgent =
			Cast<INarrativeTeamAgentInterface>(Body))
		{
			TeamAgent->AddFaction(Faction);
		}
		return Viewer;
	}

	APlayerController* MakeFactionViewer(UWorld* World, const FGameplayTag& Faction)
	{
		return MakeLocalViewer<APlayerController>(World, Faction);
	}

	/**
	 * A viewer that records every cinematic transition it is put through.
	 *
	 * A local controller's final bCinematicMode cannot distinguish "suppressed and released" from
	 * "never touched", so a test asserting only the flag would pass against a reconcile that never
	 * ran. The recording is what makes the claim falsifiable.
	 */
	ATerritoryCinematicRecordingController* MakeRecordingViewer(UWorld* World,
		const FGameplayTag& Faction)
	{
		return MakeLocalViewer<ATerritoryCinematicRecordingController>(World, Faction);
	}

	/**
	 * The engine's own suppression, reproduced verbatim.
	 *
	 * ULevelSequencePlayer::EnableCinematicMode is private and only reachable from OnStartedPlaying,
	 * which fires on the player's first update after Play() - and a headless fixture world has no
	 * game loop to produce that update. So the test performs the call the engine would make, with the
	 * engine's own argument order and the actor's own settings
	 * (LevelSequencePlayer.cpp:395), rather than approximating it. The live harness is what proves the
	 * engine really makes it.
	 */
	void SuppressCinematics(UWorld* World, const FMovieSceneSequencePlaybackSettings& Settings)
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator();
			Iterator; ++Iterator)
		{
			APlayerController* Controller = Iterator->Get();
			if (Controller && Controller->IsLocalController())
			{
				Controller->SetCinematicMode(true, Settings.bHidePlayer, Settings.bHideHud,
					Settings.bDisableMovementInput, Settings.bDisableLookAtInput);
			}
		}
	}

	/**
	 * A real Level Sequence asset with no authored content. The vendor factory only needs a valid
	 * sequence to build a player from, and an empty one keeps this test about the audience and the
	 * playback settings Territory controls rather than about authored camera work.
	 */
	ULevelSequence* MakeSequence(UObject* Outer)
	{
		ULevelSequence* Sequence = NewObject<ULevelSequence>(Outer);
		Sequence->Initialize();
		return Sequence;
	}

	int32 CountCutscenes(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ANarrativeLevelSequenceActor> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	void DestroyCutscenes(UWorld* World)
	{
		TArray<ANarrativeLevelSequenceActor*> Actors;
		for (TActorIterator<ANarrativeLevelSequenceActor> It(World); It; ++It)
		{
			Actors.Add(*It);
		}
		for (ANarrativeLevelSequenceActor* Actor : Actors)
		{
			if (IsValid(Actor)) Actor->Destroy();
		}
	}

	/** The single sequence actor currently in the world, or null when the count is not exactly one. */
	ANarrativeLevelSequenceActor* SoleCutscene(UWorld* World)
	{
		ANarrativeLevelSequenceActor* Found = nullptr;
		for (TActorIterator<ANarrativeLevelSequenceActor> It(World); It; ++It)
		{
			if (Found) return nullptr;
			Found = *It;
		}
		return Found;
	}

	UTerritoryPlayCutsceneEvent* MakeEvent(UObject* Outer,
		const FGameplayTag& AudienceFaction, ULevelSequence* Sequence)
	{
		UTerritoryPlayCutsceneEvent* Event =
			NewObject<UTerritoryPlayCutsceneEvent>(Outer);
		Event->AudienceFaction = AudienceFaction;
		Event->CutsceneSequence = Sequence;
		return Event;
	}

	/**
	 * Run the cutscene event once against a fresh world and hand back the actor it created.
	 *
	 * GraceSeconds is authored on the event before it runs, so the teardown policy under test is the
	 * one production would read rather than a test-only override.
	 *
	 * Playback is no longer started by hand here, and removing that is part of the fix rather than
	 * tidying. Auto Play is forced off by the event now, so this world would otherwise be left with a
	 * stopped player and a Stop(), Pause() or skip below would act on nothing - which is exactly how
	 * the old hand-play masked the ordering: the event armed teardown during ExecuteEvent, and the
	 * helper played afterwards, so the two were never in the same frame and the leak the ordering
	 * could cause was unreachable from any test. The event starts the sequence itself as its last step,
	 * so playback is under way by the time ExecuteEvent returns.
	 */
	ANarrativeLevelSequenceActor* PlayCutscene(FWorldFixture& Fixture, float GraceSeconds)
	{
		const FGameplayTag Heroes =
			FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		MakeFactionViewer(Fixture.World, Heroes);
		UTerritoryPlayCutsceneEvent* Event =
			MakeEvent(Fixture.World, Heroes, MakeSequence(Fixture.World));
		Event->TeardownGraceSeconds = GraceSeconds;
		Event->ExecuteEvent(nullptr, nullptr, nullptr);
		return SoleCutscene(Fixture.World);
	}

	/**
	 * Build a sequence actor through the vendor factory exactly as the event does, but without the
	 * event, so a test can drive playback and arming in a chosen order. Used by the tests that own the
	 * ordering themselves.
	 */
	ANarrativeLevelSequenceActor* SpawnCutscene(UWorld* World, APlayerController* Viewer,
		const FNarrativeSequencePlaybackSettings& Settings)
	{
		if (!World || !Viewer) return nullptr;
		ANarrativeLevelSequenceActor* Cutscene = nullptr;
		ANarrativeLevelSequenceActor::CreateNarrativeLevelSequencePlayer(World, {Viewer},
			FVector::ZeroVector, 0.f, MakeSequence(World), Settings, Cutscene);
		return Cutscene;
	}

	/** The teardown Territory attached to this cutscene, if any. */
	UTerritoryCutsceneTeardownComponent* TeardownOn(ANarrativeLevelSequenceActor* Cutscene)
	{
		return Cutscene
			? Cutscene->FindComponentByClass<UTerritoryCutsceneTeardownComponent>()
			: nullptr;
	}

	/**
	 * Read one flag out of a shot's playback settings.
	 *
	 * PlaybackSettings is protected on the vendor dialogue sequence, so the value the engine will
	 * actually use is read through its property offset rather than by adding a public accessor to
	 * production code that only a test would call. This reads the real CDO value: the assertion on
	 * top of it is about behaviour, not about the property existing.
	 */
	bool ReadShotPlaybackFlag(const UObject* Shot, const TCHAR* FlagName)
	{
		if (!Shot) return false;
		const FStructProperty* Settings = FindFProperty<FStructProperty>(
			Shot->GetClass(), TEXT("PlaybackSettings"));
		if (!Settings) return false;
		const void* SettingsValue = Settings->ContainerPtrToValuePtr<void>(Shot);
		const FBoolProperty* Flag = FindFProperty<FBoolProperty>(
			Settings->Struct, FlagName);
		return Flag && SettingsValue
			&& Flag->GetPropertyValue_InContainer(SettingsValue);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Contract
// ═══════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneEventContract,
	"TerritoryFramework.Presentation.Cutscenes.EventBlueprintContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneEventContract::RunTest(const FString&)
{
	UClass* EventClass = UTerritoryPlayCutsceneEvent::StaticClass();
	TestNotNull(TEXT("The cutscene event exists"), EventClass);
	if (!EventClass) return false;

	TestTrue(TEXT("A cutscene event is a Narrative event"),
		EventClass->IsChildOf(UNarrativeEvent::StaticClass()));
	TestTrue(TEXT("A cutscene event can be authored inline in the defender and floor event arrays"),
		EventClass->HasAnyClassFlags(CLASS_EditInlineNew));

	// Cutscene Sequence: a soft asset reference a designer points at a Level Sequence package, so
	// the Definition does not hard-load every cinematic it might play.
	const FProperty* SequenceProperty =
		FindFProperty<FProperty>(EventClass, TEXT("CutsceneSequence"));
	TestNotNull(TEXT("The sequence is reflected"), SequenceProperty);
	if (SequenceProperty)
	{
		TestTrue(TEXT("The sequence is a soft asset reference, so a Definition does not hard-load its cutscenes"),
			SequenceProperty->IsA<FSoftObjectProperty>());
		TestTrue(TEXT("A designer can author the sequence"), SequenceProperty->HasAnyPropertyFlags(CPF_Edit));
	}

	// Playback Settings: the vendor's settings struct, so Territory configures playback with the
	// same vocabulary Narrative shots already use.
	const FStructProperty* SettingsProperty =
		FindFProperty<FStructProperty>(EventClass, TEXT("PlaybackSettings"));
	TestNotNull(TEXT("Playback settings are reflected"), SettingsProperty);
	if (SettingsProperty)
	{
		TestTrue(TEXT("Playback settings are the Narrative sequence settings"),
			SettingsProperty->Struct
				== FNarrativeSequencePlaybackSettings::StaticStruct());
		TestTrue(TEXT("A designer can author playback settings"),
			SettingsProperty->HasAnyPropertyFlags(CPF_Edit));
	}

	const FFloatProperty* RelevancyProperty =
		FindFProperty<FFloatProperty>(EventClass, TEXT("RelevancyDist"));
	TestNotNull(TEXT("The relevancy radius is reflected"), RelevancyProperty);
	UTerritoryPlayCutsceneEvent* CDO =
		EventClass->GetDefaultObject<UTerritoryPlayCutsceneEvent>();
	TestNotNull(TEXT("The cutscene event has a default object"), CDO);
	if (!CDO) return false;
	TestTrue(TEXT("The relevancy radius defaults to always relevant, so an audience is never silently culled"),
		FMath::IsNearlyZero(CDO->RelevancyDist));
	// The engine treats a zero radius as always-relevant and a non-zero one as a squared cull
	// distance, which is why the default is zero rather than a positive guess.
	TestTrue(TEXT("The vendor factory treats zero as always relevant, which is what the default assumes"),
		CDO->RelevancyDist <= KINDA_SMALL_NUMBER);

	TestNotNull(TEXT("The audience faction is reflected"),
		FindFProperty<FProperty>(EventClass, TEXT("AudienceFaction")));
	const FObjectPropertyBase* ProfileProperty =
		FindFProperty<FObjectPropertyBase>(EventClass, TEXT("LightRigProfile"));
	TestNotNull(TEXT("The optional light rig profile is reflected"), ProfileProperty);
	if (ProfileProperty)
	{
		TestTrue(TEXT("The light rig profile is a Territory cinematic profile"),
			ProfileProperty->PropertyClass
				== UTerritoryCinematicLightRigProfile::StaticClass());
	}

	// A cutscene is a moment the player watched. Refiring it on quest load would replay it.
	TestFalse(TEXT("A cutscene does not refire when the quest holding it loads"),
		CDO->bRefireOnLoad);

	// The event has its own behaviour, not an inherited stub: its graph text is authored and
	// names the missing case explicitly rather than printing an empty asset name. Called through
	// the vendor's public BlueprintNativeEvent entry point - the same one the graph node uses to
	// draw its title - rather than the protected override Territory declares.
	TestEqual(TEXT("The graph text names the cutscene it plays"),
		CDO->GetGraphDisplayText(),
		FString(TEXT("Cutscene: play (none authored)")));
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Behavioural: the dialogue shot is a real cutscene, not just a hidden HUD
// ═══════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryDialogueShotInputContract,
	"TerritoryFramework.Presentation.Cutscenes.DialogueShotSuppressesInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryDialogueShotInputContract::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	const UTerritoryDialogueShot* TerritoryShot =
		GetDefault<UTerritoryDialogueShot>();
	TestNotNull(TEXT("The Territory shot has a default object"), TerritoryShot);
	if (!TerritoryShot) return false;

	// The engine only reaches SetCinematicMode at all when one of these four flags is set; the HUD
	// flag is what already made Territory shots enter cinematic mode.
	TestTrue(TEXT("A Territory shot still hides the HUD"),
		ReadShotPlaybackFlag(TerritoryShot, TEXT("bHideHud")));
	TestTrue(TEXT("A Territory shot requests movement suppression"),
		ReadShotPlaybackFlag(TerritoryShot, TEXT("bDisableMovementInput")));
	TestTrue(TEXT("A Territory shot requests look suppression"),
		ReadShotPlaybackFlag(TerritoryShot, TEXT("bDisableLookAtInput")));

	// The rule only means something if Territory is the one asking for it: the vendor shot this
	// derives from leaves both flags off, so the player keeps walking around during a dialogue.
	const UNarrativeDialogueSequence* VendorShot =
		GetDefault<UNarrativeDialogueSequence>();
	TestNotNull(TEXT("The vendor shot has a default object"), VendorShot);
	if (VendorShot)
	{
		TestFalse(TEXT("The unmodified vendor shot leaves movement input enabled"),
			ReadShotPlaybackFlag(VendorShot, TEXT("bDisableMovementInput")));
		TestFalse(TEXT("The unmodified vendor shot leaves look input enabled"),
			ReadShotPlaybackFlag(VendorShot, TEXT("bDisableLookAtInput")));
	}

	// The release path is what makes the suppression safe. Playback pauses after each line, so a
	// shot is only survivable because the dialogue stops the sequence when it ends.
	TestTrue(TEXT("A Territory shot still pauses at the end of each line"),
		ReadShotPlaybackFlag(TerritoryShot, TEXT("bPauseAtEnd")));
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Behavioural: the audience
// ═══════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneAudienceFallback,
	"TerritoryFramework.Presentation.Cutscenes.AudienceFallbackUsesFactionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneAudienceFallback::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	ULevelSequence* Sequence = MakeSequence(Fixture.World);
	UTerritoryPlayCutsceneEvent* Event =
		MakeEvent(Fixture.World, Heroes, Sequence);

	// This is the empty-context path: a defender defeat with no instigator passes no player at
	// all, and the authored faction is what must resolve the audience.
	Event->ExecuteEvent(nullptr, nullptr, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("A defender defeat with no instigator still plays the cutscene for its faction"),
		Cutscene);
	if (!Cutscene) return false;

	TestEqual(TEXT("Exactly one cutscene is started"), CountCutscenes(Fixture.World), 1);
	TestEqual(TEXT("The cutscene reaches exactly the faction's player"),
		Cutscene->OwnerControllers.Num(), 1);
	if (Cutscene->OwnerControllers.Num() == 1)
	{
		TestEqual(TEXT("The cutscene reaches the resolved faction player"),
			Cutscene->OwnerControllers[0].Get(), Viewer);
	}

	// Determinism: the same faction must always resolve to the same player, even when several
	// players share the membership. Path-sorted selection is what makes a save/load replay the
	// same cutscene for the same person.
	APlayerController* SecondViewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A second player can share the faction"), SecondViewer);
	const UTerritoryControlSubsystem* Control =
		Fixture.World->GetSubsystem<UTerritoryControlSubsystem>();
	TestNotNull(TEXT("The control subsystem owns faction player resolution"), Control);
	if (Control && SecondViewer)
	{
		APlayerController* FirstCall =
			Control->ResolveFactionPlayerContext(Heroes).PlayerController.Get();
		APlayerController* SecondCall =
			Control->ResolveFactionPlayerContext(Heroes).PlayerController.Get();
		TestNotNull(TEXT("A shared faction still resolves a player"), FirstCall);
		TestEqual(TEXT("Faction player resolution is stable across repeated calls"),
			FirstCall, SecondCall);
	}

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneExplicitAudience,
	"TerritoryFramework.Presentation.Cutscenes.ExplicitControllerOutranksAuthoredFaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneExplicitAudience::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	MakeFactionViewer(Fixture.World, Heroes);
	APlayerController* Explicit = Fixture.World->SpawnActor<APlayerController>();
	TestNotNull(TEXT("An explicit controller can be built"), Explicit);
	if (!Explicit) return false;

	ULevelSequence* Sequence = MakeSequence(Fixture.World);
	UTerritoryPlayCutsceneEvent* Event =
		MakeEvent(Fixture.World, Heroes, Sequence);

	// The event's own context is authoritative. A cutscene must reach the player the transition
	// was actually about, not whoever happens to match the authored faction.
	Event->ExecuteEvent(nullptr, Explicit, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene plays for the explicit context"), Cutscene);
	if (Cutscene)
	{
		TestEqual(TEXT("Only the explicit controller is in the audience"),
			Cutscene->OwnerControllers.Num(), 1);
		if (Cutscene->OwnerControllers.Num() == 1)
		{
			TestEqual(TEXT("The explicit controller outranks the authored faction"),
				Cutscene->OwnerControllers[0].Get(), Explicit);
		}
	}

	DestroyCutscenes(Fixture.World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Behavioural: the anti-strand rule
// ═══════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneNeverPausesAtEnd,
	"TerritoryFramework.Presentation.Cutscenes.PauseAtEndIsForcedOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneNeverPausesAtEnd::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	ULevelSequence* Sequence = MakeSequence(Fixture.World);
	UTerritoryPlayCutsceneEvent* Event =
		MakeEvent(Fixture.World, Heroes, Sequence);

	// Deliberately author the one configuration that would strand the player: the engine releases
	// movement and look only from the sequence player's OnStopped, and a sequence that pauses at
	// its end fires OnPause instead.
	Event->PlaybackSettings.bPauseAtEnd = true;
	Event->PlaybackSettings.bAutoPlay = false;

	Event->ExecuteEvent(nullptr, nullptr, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene still starts"), Cutscene);
	if (!Cutscene) return false;

	TestFalse(TEXT("Pause At End is forced off so the sequence reaches a real stop"),
		Cutscene->PlaybackSettings.bPauseAtEnd);
	// This assertion used to read the other way, and it is the one existing test the fix had to change.
	// Auto Play was forced *on* because the event relied on the vendor's BeginPlay to start playback;
	// that is exactly the window claim 8 is about, so the event now forces it off and starts the
	// sequence itself, after teardown is armed. The assertion below is what keeps the inversion from
	// degenerating into "the settings are safe because nothing ever plays".
	TestFalse(TEXT("Auto Play is forced off, because the engine starting the sequence is the window this closes"),
		Cutscene->PlaybackSettings.bAutoPlay);
	TestFalse(TEXT("The authored pausing configuration did not survive to the player"),
		Cutscene->NarrativeSequenceParams.bPauseAtEnd);

	// The contract the old Auto Play assertion was really about, asserted directly instead: an event
	// that never plays is not a cutscene, and this one plays.
	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;
	TestTrue(TEXT("The cutscene is playing even though Auto Play is off"),
		Player->IsPlaying());

	DestroyCutscenes(Fixture.World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Failure paths
// ═══════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneFailurePaths,
	"TerritoryFramework.Presentation.Cutscenes.MissingInputsStartNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneFailurePaths::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ULevelSequence* Sequence = MakeSequence(Fixture.World);

	// No explicit context and no authored faction. Guessing a player here would play a story
	// moment for a stranger, so the event must report why and start nothing.
	UTerritoryPlayCutsceneEvent* NoAudience =
		MakeEvent(Fixture.World, FGameplayTag(), Sequence);
	NoAudience->ExecuteEvent(nullptr, nullptr, nullptr);
	TestEqual(TEXT("No resolvable audience starts no cutscene"),
		CountCutscenes(Fixture.World), 0);

	// An authored faction nobody is currently playing as resolves to an empty context, which is
	// the same outcome as no faction at all.
	UTerritoryPlayCutsceneEvent* AbsentFaction =
		MakeEvent(Fixture.World, Heroes, Sequence);
	AbsentFaction->ExecuteEvent(nullptr, nullptr, nullptr);
	TestEqual(TEXT("A faction with no connected player starts no cutscene"),
		CountCutscenes(Fixture.World), 0);

	// A resolvable audience but nothing to play. The event reports the missing asset rather than
	// creating an empty sequence actor that would suppress input for a frame and release it.
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (Viewer)
	{
		UTerritoryPlayCutsceneEvent* NoSequence =
			MakeEvent(Fixture.World, Heroes, nullptr);
		NoSequence->ExecuteEvent(nullptr, nullptr, nullptr);
		TestEqual(TEXT("An event with no authored sequence starts no cutscene"),
			CountCutscenes(Fixture.World), 0);
	}

	// Now prove the same world does start one, so the zero counts above are the missing input
	// rather than a fixture that could never spawn a cutscene at all.
	if (Viewer)
	{
		UTerritoryPlayCutsceneEvent* Working =
			MakeEvent(Fixture.World, Heroes, Sequence);
		Working->ExecuteEvent(nullptr, nullptr, nullptr);
		TestEqual(TEXT("The same fixture does start a cutscene once every input is present"),
			CountCutscenes(Fixture.World), 1);
	}

	DestroyCutscenes(Fixture.World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════════
// The sequence actor's lifetime
//
// The vendor factory spawns the actor, returns it through OutActor and never destroys it, so the
// actor leaks for the rest of the session unless the caller discharges that contract. These tests
// are that caller's proof: they drive the real engine playback paths and assert on real lifetime.
// ═══════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneTeardownOnStop,
	"TerritoryFramework.Presentation.Cutscenes.SequenceActorIsDestroyedAfterItStops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneTeardownOnStop::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	FWorldFixture Fixture;
	ANarrativeLevelSequenceActor* Cutscene = PlayCutscene(Fixture, 1.f);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	// The actor nobody owned is now owned: production attaches its teardown to the actor it created,
	// rather than tracking it somewhere that could outlive its subject.
	TestNotNull(TEXT("Starting a cutscene arms its teardown"), TeardownOn(Cutscene));
	TestEqual(TEXT("A cutscene still playing has no lifespan armed"), Cutscene->GetLifeSpan(), 0.f);

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;
	// StopInternal only reaches its OnStop broadcast from inside an IsPlaying-or-IsPaused branch, so
	// this assertion is what keeps the stop below from being a silent no-op.
	TestTrue(TEXT("The cutscene is playing before it is stopped"), Player->IsPlaying());

	// Stop() is the engine path a real cutscene ends by: a natural finish reaches StopInternal through
	// FinishPlaybackInternal, and a skip reaches StopInternal directly.
	Player->Stop();

	// GetLifeSpan returns the timer remaining, which starts at the authored grace and counts down, so
	// the assertion is a range rather than an equality.
	TestTrue(TEXT("Stopping the cutscene arms the teardown with the authored grace"),
		Cutscene->GetLifeSpan() > 0.f && Cutscene->GetLifeSpan() <= 1.f);

	// Drive the timer rather than waiting on it. LifeSpanExpired is exactly what the world's lifespan
	// timer calls, so this is the destruction the grace would have caused, deterministically.
	Cutscene->LifeSpanExpired();
	TestTrue(TEXT("The sequence actor is destroyed once its lifespan expires"),
		!IsValid(Cutscene) || Cutscene->IsActorBeingDestroyed());
	TestEqual(TEXT("No cutscene actor is left behind"), CountCutscenes(Fixture.World), 0);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneTeardownOnSkip,
	"TerritoryFramework.Presentation.Cutscenes.SequenceActorIsDestroyedOnSkipToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneTeardownOnSkip::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	FWorldFixture Fixture;
	ANarrativeLevelSequenceActor* Cutscene = PlayCutscene(Fixture, 1.f);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;
	TestTrue(TEXT("The cutscene is playing before it is skipped"), Player->IsPlaying());

	// This is the path that makes OnStop the load-bearing binding rather than a belt-and-braces
	// second one: GoToEndAndStop calls StopInternal directly, so FinishPlaybackInternal never runs.
	int32 FinishedBroadcasts = 0;
	Player->OnNativeFinished.BindLambda([&FinishedBroadcasts] { ++FinishedBroadcasts; });
	Player->GoToEndAndStop();

	// The claim above, measured rather than asserted from the engine source: a teardown bound only to
	// OnFinished would leak every skipped cutscene.
	TestEqual(TEXT("Skipping to the end does not broadcast OnFinished at all"),
		FinishedBroadcasts, 0);
	TestTrue(TEXT("Skipping to the end still arms the teardown"),
		Cutscene->GetLifeSpan() > 0.f && Cutscene->GetLifeSpan() <= 1.f);

	Player->OnNativeFinished.Unbind();
	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneTeardownZeroGrace,
	"TerritoryFramework.Presentation.Cutscenes.ZeroGraceDestroysImmediately",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneTeardownZeroGrace::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	FWorldFixture Fixture;
	ANarrativeLevelSequenceActor* Cutscene = PlayCutscene(Fixture, 0.f);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;

	Player->Stop();

	// The trap this guards is specific: AActor::SetLifeSpan takes its clear-the-timer branch for any
	// value <= 0, so an implementation that armed the grace unconditionally would leave the actor
	// alive with no timer - the same silent leak, now depending on the authored value.
	TestTrue(TEXT("Zero grace destroys the actor as soon as the sequence stops"),
		!IsValid(Cutscene) || Cutscene->IsActorBeingDestroyed());
	TestEqual(TEXT("No cutscene actor is left behind"), CountCutscenes(Fixture.World), 0);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneTeardownPaused,
	"TerritoryFramework.Presentation.Cutscenes.PausedPlayerIsNotTornDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneTeardownPaused::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	FWorldFixture Fixture;
	ANarrativeLevelSequenceActor* Cutscene = PlayCutscene(Fixture, 1.f);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("Starting a cutscene arms its teardown"), Teardown);
	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Teardown || !Player) return false;

	// The dialogue-shaped case the guard exists for: the engine holds a sequence on its last frame
	// while a line is still displayed, and broadcasts OnFinished after Pause().
	Player->Pause();
	TestTrue(TEXT("Pause holds the player rather than stopping it"), Player->IsPaused());
	TestFalse(TEXT("A paused player is not playing"), Player->IsPlaying());

	FTFTerritoryCutsceneTeardownTestAccess::SequenceStopped(Teardown);

	TestTrue(TEXT("A paused cutscene is left alive"),
		IsValid(Cutscene) && !Cutscene->IsActorBeingDestroyed());
	TestEqual(TEXT("A paused cutscene arms no teardown"), Cutscene->GetLifeSpan(), 0.f);

	// The control for the two assertions above. Without it, a component that never worked at all
	// would satisfy them, and the zero would prove nothing.
	Player->Stop();
	TestTrue(TEXT("The same cutscene does arm its teardown once the player has really stopped"),
		Cutscene->GetLifeSpan() > 0.f);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneTeardownAuthority,
	"TerritoryFramework.Presentation.Cutscenes.TeardownIsServerAuthoritative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneTeardownAuthority::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	// Built through the vendor factory directly rather than through the event, so the role set below
	// is the only thing that has touched this actor's authority.
	FNarrativeSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	Settings.TagsToApplyWhilstBound.Reset();
	ANarrativeLevelSequenceActor* Cutscene = nullptr;
	ANarrativeLevelSequenceActor::CreateNarrativeLevelSequencePlayer(Fixture.World, {Viewer},
		FVector::ZeroVector, 0.f, MakeSequence(Fixture.World), Settings, Cutscene);
	TestNotNull(TEXT("The vendor factory creates a cutscene actor"), Cutscene);
	if (!Cutscene) return false;

	// A client holds a replica of the server's actor. Destroying its own copy would desync the
	// cutscene from the server that owns it, and a locally destroyed replica comes back.
	Cutscene->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("A client does not arm a teardown for a replicated cutscene actor"),
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Cutscene, 1.f) == nullptr);
	TestNull(TEXT("A client adds no teardown component"),
		Cutscene->FindComponentByClass<UTerritoryCutsceneTeardownComponent>());

	// The control: the identical call on the authority does arm one.
	Cutscene->SetRole(ROLE_Authority);
	TestNotNull(TEXT("The authority does arm a teardown"),
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Cutscene, 1.f));

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneTeardownGraceAuthorable,
	"TerritoryFramework.Presentation.Cutscenes.TeardownGraceIsAuthorable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneTeardownGraceAuthorable::RunTest(const FString&)
{
	UClass* EventClass = UTerritoryPlayCutsceneEvent::StaticClass();
	const FFloatProperty* GraceProperty =
		FindFProperty<FFloatProperty>(EventClass, TEXT("TeardownGraceSeconds"));
	TestNotNull(TEXT("The teardown grace is reflected"), GraceProperty);
	if (GraceProperty)
	{
		TestTrue(TEXT("A designer can author the teardown grace"),
			GraceProperty->HasAnyPropertyFlags(CPF_Edit));
		TestTrue(TEXT("The teardown grace is readable in Blueprint"),
			GraceProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}

	const UTerritoryPlayCutsceneEvent* CDO =
		EventClass->GetDefaultObject<UTerritoryPlayCutsceneEvent>();
	TestNotNull(TEXT("The cutscene event has a default object"), CDO);
	if (CDO)
	{
		// A default of zero would destroy the actor the instant its sequence stops, cutting off any
		// client whose own copy is a moment behind, so the shipped default must leave a grace.
		TestTrue(TEXT("The default grace leaves a client a moment behind room to finish"),
			CDO->TeardownGraceSeconds > 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutscenePlaybackStartedByEvent,
	"TerritoryFramework.Presentation.Cutscenes.PlaybackIsStartedByTheEventAfterArming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutscenePlaybackStartedByEvent::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	MakeFactionViewer(Fixture.World, Heroes);
	UTerritoryPlayCutsceneEvent* Event =
		MakeEvent(Fixture.World, Heroes, MakeSequence(Fixture.World));
	Event->TeardownGraceSeconds = 1.f;
	// Authored on purpose, and authored true. The claim is that the event overrides it, so a designer
	// who fills this in expecting the vendor's Auto Play to drive the cutscene gets Territory's
	// ordering instead - and the cutscene still plays.
	Event->PlaybackSettings.bAutoPlay = true;
	Event->PlaybackSettings.bPauseAtEnd = true;

	Event->ExecuteEvent(nullptr, nullptr, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The event creates exactly one cutscene actor"), Cutscene);
	if (!Cutscene) return false;

	TestNotNull(TEXT("The teardown that owns the actor's lifetime is armed"), TeardownOn(Cutscene));

	// This is the window claim 8 describes. The vendor factory spawns the actor with deferred
	// construction so BeginPlay runs inside the factory, and BeginPlay calls Play() whenever Auto Play
	// is set - so this value decides whether playback could have started before the teardown above
	// existed. It is read off the actor the factory built, through the engine base property the
	// vendor's own BeginPlay reads.
	TestFalse(TEXT("Auto Play is forced off, so the factory cannot start the cutscene itself"),
		ReadShotPlaybackFlag(Cutscene, TEXT("bAutoPlay")));
	TestFalse(TEXT("Pause At End is forced off, so a cutscene always reaches a real stop"),
		ReadShotPlaybackFlag(Cutscene, TEXT("bPauseAtEnd")));

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;

	// Nothing in this test plays by hand, and the two assertions above rule out the factory having
	// played. So Territory started this, and the only place it can have done so is after arming.
	TestTrue(TEXT("The event started the sequence itself, once teardown was armed"),
		Player->IsPlaying());
	TestEqual(TEXT("A cutscene that is playing has had no lifespan armed"),
		Cutscene->GetLifeSpan(), 0.f);
	TestEqual(TEXT("The cutscene actor is still alive while it plays"),
		CountCutscenes(Fixture.World), 1);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneArmingBeforePlayback,
	"TerritoryFramework.Presentation.Cutscenes.ArmingBeforePlaybackDoesNotTearDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneArmingBeforePlayback::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	// The settings the event forces, reproduced here so this test is about the component's contract
	// rather than about the event that happens to call it.
	FNarrativeSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	ANarrativeLevelSequenceActor* Cutscene = SpawnCutscene(Fixture.World, Viewer, Settings);
	TestNotNull(TEXT("The vendor factory creates a cutscene actor"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;

	// A brand-new player reads as not playing and not paused, because ULevelSequencePlayer leaves its
	// status at its zero value until something plays it - not because this sequence has ended, which
	// the last test in this section measures. Read through the public predicates rather than
	// GetPlaybackStatus(), which is protected on the player and therefore not available to a caller at
	// all: IsPlaying/IsPaused are the whole of what arming could consult.
	TestFalse(TEXT("A never-played player is not playing"),
		Player->IsPlaying());
	TestFalse(TEXT("A never-played player is not paused"),
		Player->IsPaused());

	TestNotNull(TEXT("Arming a player that has not played yet succeeds"),
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Cutscene, 1.f));

	// The regression this pins, and the reason arming must not consult the playback status: arming
	// happens before playback starts, so a "status is Stopped" branch inside arming would destroy every
	// cutscene the instant it was armed, silently and universally.
	TestTrue(TEXT("Arming does not destroy a cutscene that has not started yet"),
		IsValid(Cutscene) && !Cutscene->IsActorBeingDestroyed());
	TestEqual(TEXT("Arming arms no lifespan of its own"), Cutscene->GetLifeSpan(), 0.f);

	// The control for the two assertions above: the player really was startable, so they are not a
	// dead player proving nothing.
	Player->Play();
	TestTrue(TEXT("The armed cutscene plays on request"), Player->IsPlaying());
	Player->Stop();
	TestTrue(TEXT("Stopping after arming arms the authored grace"),
		Cutscene->GetLifeSpan() > 0.f && Cutscene->GetLifeSpan() <= 1.f);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneReconcileAlreadyStopped,
	"TerritoryFramework.Presentation.Cutscenes.ReconcileTearsDownASequenceAlreadyStopped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneReconcileAlreadyStopped::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	FNarrativeSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	ANarrativeLevelSequenceActor* Cutscene = SpawnCutscene(Fixture.World, Viewer, Settings);
	TestNotNull(TEXT("The vendor factory creates a cutscene actor"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;

	// Play and stop *before* arming, so the ending is broadcast to a player that has no bindings yet.
	// That is the window an immediate cancellation opens - a StopTags tag, a zero-length sequence, a
	// producer that cancels the shot as it starts - and it is the window a stop binding alone cannot
	// cover, because the signal has already been sent by the time the binding exists.
	Player->Play();
	TestTrue(TEXT("The sequence plays before it is stopped"), Player->IsPlaying());
	Player->Stop();
	TestFalse(TEXT("The sequence has already stopped when teardown is armed"),
		Player->IsPlaying());

	UTerritoryCutsceneTeardownComponent* Teardown =
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Cutscene, 0.f);
	TestNotNull(TEXT("Arming on an already-stopped player still arms"), Teardown);
	if (!Teardown) return false;

	// Before the reconcile the actor is alive, deliberately. Arming cannot tell "already stopped" from
	// "not started yet" - the previous test measures why - so it must not guess, and the ending is only
	// recoverable by the caller that knows it just asked this player to play.
	TestTrue(TEXT("Arming alone leaves the already-ended cutscene alive"),
		IsValid(Cutscene) && !Cutscene->IsActorBeingDestroyed());

	Teardown->ReconcileAfterPlaybackRequest();

	TestTrue(TEXT("Reconciling destroys a cutscene whose sequence has already ended"),
		!IsValid(Cutscene) || Cutscene->IsActorBeingDestroyed());
	TestEqual(TEXT("No cutscene actor is left behind"), CountCutscenes(Fixture.World), 0);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutsceneReconcileKeepsGrace,
	"TerritoryFramework.Presentation.Cutscenes.ReconcileFromAnAlreadyStoppedPlayerKeepsTheGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutsceneReconcileKeepsGrace::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	FNarrativeSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	ANarrativeLevelSequenceActor* Cutscene = SpawnCutscene(Fixture.World, Viewer, Settings);
	TestNotNull(TEXT("The vendor factory creates a cutscene actor"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;

	Player->Play();
	Player->Stop();

	// The grace exists because this destruction replicates: a client whose own copy is a moment behind
	// must not have its cutscene cut off. A reconcile that short-circuited to Destroy() would satisfy
	// the previous test and quietly drop that guarantee for every already-ended sequence, which is what
	// this test separates.
	UTerritoryCutsceneTeardownComponent* Teardown =
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Cutscene, 1.f);
	TestNotNull(TEXT("Arming on an already-stopped player still arms"), Teardown);
	if (!Teardown) return false;

	Teardown->ReconcileAfterPlaybackRequest();

	TestTrue(TEXT("Reconciling an already-ended sequence applies the authored grace, not an immediate destroy"),
		Cutscene->GetLifeSpan() > 0.f && Cutscene->GetLifeSpan() <= 1.f);
	TestTrue(TEXT("The actor is still alive during that grace"),
		IsValid(Cutscene) && !Cutscene->IsActorBeingDestroyed());

	// The same timer drive the other grace tests use, so the destruction is what the grace would have
	// caused rather than a wait.
	Cutscene->LifeSpanExpired();
	TestTrue(TEXT("The reconciled cutscene is destroyed once its lifespan expires"),
		!IsValid(Cutscene) || Cutscene->IsActorBeingDestroyed());
	TestEqual(TEXT("No cutscene actor is left behind"), CountCutscenes(Fixture.World), 0);

	DestroyCutscenes(Fixture.World);
	return true;
}

/**
 * The engine facts the playback order rests on, measured rather than quoted from the engine source, so
 * an engine upgrade that invalidates the reasoning fails a test instead of silently reopening claim 8.
 *
 * Together they are why the order is "force the settings, spawn, arm, play, reconcile" and why arming
 * cannot perform the reconcile itself:
 *
 *   1. A player that was never played and a player that played and stopped answer the *same* to every
 *      question a caller can ask: not playing, not paused. ULevelSequencePlayer leaves its status at
 *      its zero value until something plays it, so "it is not playing" cannot be read as "this
 *      sequence has already ended".
 *   2. Stop() on a player that never played is a silent no-op, so a sequence is only "already ended"
 *      from Territory's point of view once something has played it.
 *
 * The predicates are IsPlaying and IsPaused rather than the status enum for a reason that is itself
 * part of the finding: GetPlaybackStatus() is protected on the player, so it is not an API a caller
 * could reach even if it did disambiguate. Nothing about this can be checked from outside the player
 * except the two predicates that cannot tell the two histories apart.
 *
 * The remaining leg - that the vendor's BeginPlay autoplays, creating the window in the first place -
 * cannot be measured the same way: it fires only inside the factory's FinishSpawning in a *begun*
 * world, and there is no engine hook between the factory's InitializePlayer() and that BeginPlay to
 * interpose a probe on. It is covered by construction instead: the event forces Auto Play off, which
 * is the value the vendor's BeginPlay branches on, and the test above asserts that forced value on the
 * actor the factory built.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCutscenePlaybackOrder,
	"TerritoryFramework.Presentation.Cutscenes.Regression.PlaybackOrderIsSettingsSpawnObserverPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCutscenePlaybackOrder::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	APlayerController* Viewer = MakeFactionViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A faction viewer can be built"), Viewer);
	if (!Viewer) return false;

	FNarrativeSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	ANarrativeLevelSequenceActor* Cutscene = SpawnCutscene(Fixture.World, Viewer, Settings);
	TestNotNull(TEXT("The vendor factory creates a cutscene actor"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	if (!Player) return false;

	TestFalse(TEXT("A never-played player is not playing"), Player->IsPlaying());
	TestFalse(TEXT("A never-played player is not paused"), Player->IsPaused());

	Player->Stop();
	TestFalse(TEXT("Stopping a never-played player changes nothing, because StopInternal only acts on a playing or paused player"),
		Player->IsPlaying());
	TestFalse(TEXT("and leaves it not paused"), Player->IsPaused());

	Player->Play();
	TestTrue(TEXT("A played player is playing"), Player->IsPlaying());

	Player->Stop();
	TestFalse(TEXT("A played-and-stopped player reads exactly as the never-played one did: not playing"),
		Player->IsPlaying());
	TestFalse(TEXT("and not paused"), Player->IsPaused());

	// Identical readings, different histories. That is the whole reason the ordering cannot be replaced
	// by a status check inside arming, and the reason ReconcileAfterPlaybackRequest is a method the
	// caller invokes rather than something arming decides for itself.
	DestroyCutscenes(Fixture.World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Claim 7: the authority's cinematic sweep is bounded by the resolved audience
//
// ULevelSequencePlayer::EnableCinematicMode walks every local controller in its own world and never
// consults OwnerControllers - the vendor applies that list to net relevancy only. On a listen server a
// cutscene staged for one player therefore suppresses movement and look on every local controller the
// world has, including someone watching a cutscene that is not theirs. These tests are the bound.
//
// The fixture world has no game loop, so the engine's sweep is reproduced verbatim rather than
// triggered, and the reconcile is driven through the test seam. Both are named as such in the test
// bodies: what they prove is the reconcile's logic, and the live run in the listen-server harness is
// what proves the engine really makes that call and that the world's delegate really invokes the
// handler. Neither leg substitutes for the other.
// ═══════════════════════════════════════════════════════════════════════════════════

namespace TerritoryCutsceneTests
{
	/**
	 * An event configured the way a real cutscene is: the four flags that make
	 * ULevelSequencePlayer::EnableCinematicMode suppress anything at all.
	 *
	 * Without at least one of them the engine never calls SetCinematicMode, so a fixture with default
	 * settings would prove nothing about the audience - it would be testing a sequence that cannot
	 * suppress anyone.
	 */
	UTerritoryPlayCutsceneEvent* MakeSuppressingEvent(UObject* Outer,
		const FGameplayTag& AudienceFaction, ULevelSequence* Sequence)
	{
		UTerritoryPlayCutsceneEvent* Event = MakeEvent(Outer, AudienceFaction, Sequence);
		Event->PlaybackSettings.bDisableMovementInput = true;
		Event->PlaybackSettings.bDisableLookAtInput = true;
		Event->PlaybackSettings.bHidePlayer = true;
		Event->PlaybackSettings.bHideHud = true;
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicAudienceReleased,
	"TerritoryFramework.Presentation.Cutscenes.CinematicModeIsReleasedOutsideTheAudience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicAudienceReleased::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;

	// The listen-server shape: two local controllers in one authority world, and a cutscene staged for
	// exactly one of them. This is the case the engine gets wrong, because its sweep has no audience.
	ATerritoryCinematicRecordingController* Watcher = MakeRecordingViewer(Fixture.World, Heroes);
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("The audience controller can be built"), Watcher);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Watcher || !Bystander) return false;

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	Event->ExecuteEvent(nullptr, Watcher, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts for the explicit audience"), Cutscene);
	if (!Cutscene) return false;
	TestEqual(TEXT("The audience is exactly the controller the transition named"),
		Cutscene->OwnerControllers.Num(), 1);

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Teardown) return false;

	TestEqual(TEXT("Exactly the one local controller outside the audience is resolved"),
		FTFTerritoryCutsceneTeardownTestAccess::UncoveredCount(Teardown), 1);
	TestTrue(TEXT("The reconcile subscribes to the world's sequence tick while it has something to bound"),
		FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(Teardown));

	const FMovieSceneSequencePlaybackSettings& Settings = Cutscene->PlaybackSettings;
	TestTrue(TEXT("The authored cutscene really does suppress input, so there is something to bound"),
		Settings.bDisableMovementInput || Settings.bDisableLookAtInput
			|| Settings.bHidePlayer || Settings.bHideHud);

	// The engine's sweep, reproduced verbatim because it is private and only reachable from the
	// player's first update - which a world with no game loop never produces.
	SuppressCinematics(Fixture.World, Settings);
	TestEqual(TEXT("The engine suppressed the audience controller"), Watcher->NumberEntering(), 1);
	TestEqual(TEXT("The engine suppressed the bystander too, which is the defect"),
		Bystander->NumberEntering(), 1);

	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);

	TestEqual(TEXT("The audience controller keeps the suppression its cutscene is for"),
		Watcher->Num(), 1);
	TestTrue(TEXT("and is still in cinematic mode"), Watcher->bCinematicMode);
	TestEqual(TEXT("The bystander is released"), Bystander->Num(), 2);
	if (const ATerritoryCinematicRecordingController::FCall* Last = Bystander->Last())
	{
		TestFalse(TEXT("The bystander's last word is control, not suppression"),
			Last->bCinematicMode);
		// Symmetry, not just "off": the release repeats the engine's own flags, so it is the inverse of
		// the call being undone rather than a second opinion about what cinematic mode means.
		TestTrue(TEXT("and the release repeats the engine's hide-player flag"),
			Last->bHidePlayer == Settings.bHidePlayer);
		TestTrue(TEXT("and the engine's hide-HUD flag"),
			Last->bHideHud == Settings.bHideHud);
		TestTrue(TEXT("and the engine's movement flag"),
			Last->bAffectsMovement == Settings.bDisableMovementInput);
		TestTrue(TEXT("and the engine's look flag"),
			Last->bAffectsTurning == Settings.bDisableLookAtInput);
	}
	TestFalse(TEXT("The bystander really is out of cinematic mode"), Bystander->bCinematicMode);

	// Idempotence, and it is not a nicety: APlayerController's own setter has no change guard and
	// calls the reliable ClientSetCinematicMode RPC unconditionally, so a reconcile that released
	// again on every frame would send that RPC for every bystander for the whole cutscene.
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("Later frames release nothing, because there is nothing left to release"),
		Bystander->Num(), 2);
	TestEqual(TEXT("and the audience controller is never touched again"), Watcher->Num(), 1);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicAudienceCovered,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileStaysInertWhenItCoversEveryLocalController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicAudienceCovered::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Only = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A local controller can be built"), Only);
	if (!Only) return false;

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	Event->ExecuteEvent(nullptr, Only, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Teardown) return false;

	// The single-player case, and the client's: the audience is the whole world, so the engine's sweep
	// is exactly right and the reconcile must hold no tick at all. This is the anti-regression guard
	// for the common path - a reconcile that armed here would be releasing the very controller the
	// cutscene is for.
	TestEqual(TEXT("An audience that covers every local controller leaves nothing uncovered"),
		FTFTerritoryCutsceneTeardownTestAccess::UncoveredCount(Teardown), 0);
	TestFalse(TEXT("so no per-frame hook is taken"),
		FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(Teardown));
	// The play-start binding is the second hook the reconcile takes, and it is taken for the same
	// reason - to incur a debt - so an inert reconcile must not have it either. A component that bound
	// it without a pool would leave a live binding on the player for the life of the actor with
	// nothing to do.
	TestFalse(TEXT("and no play-start binding is taken either"),
		FTFTerritoryCutsceneTeardownTestAccess::HasPlayStartBinding(Teardown));

	SuppressCinematics(Fixture.World, Cutscene->PlaybackSettings);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);

	TestEqual(TEXT("The only controller was suppressed once, by the engine, and never released"),
		Only->Num(), 1);
	TestTrue(TEXT("and is still in cinematic mode, because the cutscene is for it"),
		Only->bCinematicMode);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicBystanderAlreadyFrozen,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileLeavesAControllerAnotherSystemAlreadyFroze",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicBystanderAlreadyFrozen::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Watcher = MakeRecordingViewer(Fixture.World, Heroes);
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("The audience controller can be built"), Watcher);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Watcher || !Bystander) return false;

	// Something else already has the bystander in cinematic mode before this cutscene exists - another
	// system's own cinematic. Territory has no standing to take that back, and could not tell it apart
	// from its own suppression afterwards, so it must decide before it starts.
	Bystander->SetCinematicMode(true, true, true, true, true);
	TestEqual(TEXT("The bystander starts out suppressed by something that is not this cutscene"),
		Bystander->Num(), 1);
	TestTrue(TEXT("and is in cinematic mode before the cutscene is armed"),
		Bystander->bCinematicMode);

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	Event->ExecuteEvent(nullptr, Watcher, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Teardown) return false;

	TestEqual(TEXT("A controller another system already froze is not claimed by this reconcile"),
		FTFTerritoryCutsceneTeardownTestAccess::UncoveredCount(Teardown), 0);
	TestFalse(TEXT("so this cutscene takes no per-frame hook"),
		FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(Teardown));
	TestFalse(TEXT("and no play-start binding, so its pool can never be re-opened"),
		FTFTerritoryCutsceneTeardownTestAccess::HasPlayStartBinding(Teardown));

	// The engine sweeps it again, as it does in production - the freeze is re-applied with the
	// cutscene's flags and must survive the reconcile untouched.
	SuppressCinematics(Fixture.World, Cutscene->PlaybackSettings);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);

	TestEqual(TEXT("Territory never releases a suppression it did not make"), Bystander->Num(), 2);
	TestTrue(TEXT("and the bystander is still in cinematic mode"), Bystander->bCinematicMode);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicPausedSequence,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileReleasesABystanderOfAPausedSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicPausedSequence::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Watcher = MakeRecordingViewer(Fixture.World, Heroes);
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("The audience controller can be built"), Watcher);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Watcher || !Bystander) return false;

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	Event->ExecuteEvent(nullptr, Watcher, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	ULevelSequencePlayer* Player = Cutscene->GetSequencePlayer();
	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a sequence player"), Player);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Player || !Teardown) return false;

	Player->Pause();
	TestTrue(TEXT("The sequence is paused"), Player->IsPaused());
	TestFalse(TEXT("and no longer playing"), Player->IsPlaying());

	SuppressCinematics(Fixture.World, Cutscene->PlaybackSettings);
	TestEqual(TEXT("The pause did not stop the engine suppressing, which is what strands the bystander"),
		Bystander->NumberEntering(), 1);

	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);

	// A paused sequence has deliberately not stopped, and the engine releases cinematic mode only from
	// a real stop - so this bystander would hold movement and look taken away for the whole of the
	// pause, for a cutscene it is not in and may not even be able to see. The suppression being undone
	// belongs to this sequence's own sweep; the pause does not change whose it is. A playback-status
	// gate here is the defect, which is why the assertion is on the release and not on the hook.
	TestEqual(TEXT("A paused sequence still releases the controller it is not for"),
		Bystander->NumberLeaving(), 1);
	TestFalse(TEXT("so the bystander gets control back while the shot is still on screen"),
		Bystander->bCinematicMode);

	// And the audience is what the pause is protecting, so it must be untouched by that. It is not in
	// the pool at all: nothing this reconcile does can reach the controller the cutscene is for.
	TestEqual(TEXT("The audience controller is never touched, paused or not"), Watcher->Num(), 1);
	TestTrue(TEXT("and keeps the suppression its cutscene is for"), Watcher->bCinematicMode);

	// Resume. This is the engine's own play-start, not the seam: Play() reaches
	// StartTimeControllerAndBroadcastPlayState synchronously - PlayInternal's NeedsQueueLatentAction is
	// IsEvaluating(), false outside an evaluation callback - so this asserts the binding production
	// uses. The engine re-arms bPendingOnStartedPlaying there and sweeps the whole world again, so the
	// bystander is frozen a second time and is owed a second release.
	Player->Play();
	TestTrue(TEXT("The sequence resumes"), Player->IsPlaying());
	TestEqual(TEXT("and the resume is what re-opens the release, not the first play-start"),
		FTFTerritoryCutsceneTeardownTestAccess::OwedCount(Teardown), 1);

	SuppressCinematics(Fixture.World, Cutscene->PlaybackSettings);
	TestEqual(TEXT("The resumed sequence suppresses the bystander all over again"),
		Bystander->NumberEntering(), 2);

	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("So the resumed sequence releases it again"),
		Bystander->NumberLeaving(), 2);
	TestFalse(TEXT("and it is out of cinematic mode"), Bystander->bCinematicMode);

	// A third frame releases nothing: the debt was paid by the frame above, and a reconcile that
	// released whenever it saw the flag would send ClientSetCinematicMode for the rest of the shot.
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("and a frame with nothing owed releases nothing"), Bystander->Num(), 4);
	// The audience is swept by the engine on every play-start - twice here, once for the first run and
	// once for the resume - and released by this reconcile never. That is what makes the audience bound
	// structural rather than a matter of timing: the pool is decided before any suppression exists, and
	// the audience is not in it, so no frame this component ever sees can reach it.
	TestEqual(TEXT("The engine swept the audience controller on both play-starts"),
		Watcher->NumberEntering(), 2);
	TestEqual(TEXT("while the reconcile never released it, on any frame"),
		Watcher->NumberLeaving(), 0);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicLaterSuppressionSurvives,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileLeavesALaterSuppressionAlone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicLaterSuppressionSurvives::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Watcher = MakeRecordingViewer(Fixture.World, Heroes);
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("The audience controller can be built"), Watcher);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Watcher || !Bystander) return false;

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	Event->ExecuteEvent(nullptr, Watcher, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Teardown) return false;

	const FMovieSceneSequencePlaybackSettings& Settings = Cutscene->PlaybackSettings;

	SuppressCinematics(Fixture.World, Settings);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("The bystander is released while the cutscene plays"), Bystander->NumberLeaving(), 1);

	// A later suppression on the same controller, with the engine's own arguments
	// (LevelSequencePlayer.cpp:395). This is the shape of any cinematic that starts after ours - a
	// second cutscene staged for this controller, a vendor dialogue shot, another system's freeze - and
	// every one of them reaches this controller through this same call, which is the point: nothing in
	// the engine records *which* sequence put a controller in cinematic mode, so the only thing that
	// can tell this suppression from our own is when it arrived relative to our sweep.
	Bystander->SetCinematicMode(true, Settings.bHidePlayer, Settings.bHideHud,
		Settings.bDisableMovementInput, Settings.bDisableLookAtInput);
	TestTrue(TEXT("Something else freezes the bystander later in our own playback"),
		Bystander->bCinematicMode);

	// This is the finding in one assertion. The suppression above is not ours to undo - our own sweep was
	// answered by an earlier frame - and bCinematicMode is a bare bool that cannot say so. Only the debt
	// can, and it is paid: a reconcile that kept releasing whatever it found in cinematic mode would take
	// this controller's suppression back here and on every frame after it, for the whole of a cutscene
	// it is not in.
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);

	// Three calls and no fourth: the engine's suppression, this reconcile's release, and then the later
	// freeze - which is left standing. A released-then-refrozen controller is exactly the case the bare
	// bool cannot describe, so the count is the assertion rather than the flag.
	TestEqual(TEXT("The later suppression is left exactly as it was found"), Bystander->Num(), 3);
	TestTrue(TEXT("so the bystander is still in cinematic mode"), Bystander->bCinematicMode);
	TestEqual(TEXT("and the reconcile has nothing left to pay, so it is owed nothing"),
		FTFTerritoryCutsceneTeardownTestAccess::OwedCount(Teardown), 0);

	// The audience is untouched throughout, which is what makes the two bounds independent: this one is
	// about a controller that was released and then frozen again, and the audience was never released.
	TestEqual(TEXT("nor does any of this release the audience controller"), Watcher->NumberLeaving(), 0);
	TestTrue(TEXT("which keeps the suppression its own cutscene is for"), Watcher->bCinematicMode);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicUnansweredDebt,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileHoldsAReleaseUntilItsSweepLands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicUnansweredDebt::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Watcher = MakeRecordingViewer(Fixture.World, Heroes);
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("The audience controller can be built"), Watcher);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Watcher || !Bystander) return false;

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	Event->ExecuteEvent(nullptr, Watcher, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Teardown) return false;

	// What the debt has to be, for the two tests above to work. The engine's sweep is not synchronous
	// with Play(): bPendingOnStartedPlaying is consumed by the player's next position update, so there
	// is a frame where the play-start has happened and the suppression has not - and on that frame the
	// reconcile runs *before* the tick manager, so the frame is every frame in production rather than a
	// corner. A release that were spent on it would release nobody. It is owed across that frame and
	// paid on the first frame the suppression is actually there, which is what these assertions pin.
	// (This leg is a guard on the design, not a red leg: the previous two tests are the ones that fail
	// against the released code.)
	FTFTerritoryCutsceneTeardownTestAccess::SequenceStarted(Teardown);
	TestEqual(TEXT("A play-start owes the pool a release, before anything is suppressed"),
		FTFTerritoryCutsceneTeardownTestAccess::OwedCount(Teardown), 1);

	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("A frame where the sweep has not landed releases nobody"), Bystander->Num(), 0);
	TestEqual(TEXT("and stays owed rather than being spent on that frame"),
		FTFTerritoryCutsceneTeardownTestAccess::OwedCount(Teardown), 1);

	SuppressCinematics(Fixture.World, Cutscene->PlaybackSettings);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("So the debt is paid on the first frame the sweep has landed, not the first frame after the start"),
		Bystander->NumberLeaving(), 1);
	TestFalse(TEXT("and the bystander really is out of cinematic mode"), Bystander->bCinematicMode);

	// The other half of the same rule, and the one the first bound in ArmAudienceReconcile rests on:
	// nothing is owed before a play-start. Arming is not a start, so a sequence that has been armed but
	// never played holds no debt, takes the play-start binding that would incur one, and releases
	// nothing at all - including a controller that something else freezes while it waits.
	//
	// The bystander is free at this point, having just been released, which is what puts it in this
	// second cutscene's pool rather than being excluded the way the already-frozen test's controller is.
	FNarrativeSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.bPauseAtEnd = false;
	Settings.bDisableMovementInput = true;
	Settings.bDisableLookAtInput = true;
	Settings.bHidePlayer = true;
	Settings.bHideHud = true;
	ANarrativeLevelSequenceActor* Fresh = SpawnCutscene(Fixture.World, Watcher, Settings);
	TestNotNull(TEXT("A cutscene can be built and left unplayed"), Fresh);
	if (!Fresh) return false;

	UTerritoryCutsceneTeardownComponent* FreshTeardown =
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Fresh, 1.f);
	TestNotNull(TEXT("Teardown arms for a cutscene that has not started playing"), FreshTeardown);
	if (!FreshTeardown) return false;

	TestTrue(TEXT("Arming alone takes the play-start binding that will incur the debt"),
		FTFTerritoryCutsceneTeardownTestAccess::HasPlayStartBinding(FreshTeardown));
	TestTrue(TEXT("and takes the world hook, because it does have a controller to bound"),
		FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(FreshTeardown));
	TestEqual(TEXT("but owes nothing yet, because nothing of its has swept"),
		FTFTerritoryCutsceneTeardownTestAccess::OwedCount(FreshTeardown), 0);

	// Something else freezes the bystander while that sequence is still waiting for its first
	// play-start. The call is the engine's own, with the same arguments, because there is no other
	// shape it could have: this is what any later cinematic does to any controller, and the engine
	// keeps no record of which sequence made it.
	Bystander->SetCinematicMode(true, true, true, true, true);
	TestEqual(TEXT("The bystander is suppressed again by something outside this cutscene"),
		Bystander->Num(), 3);
	TestTrue(TEXT("and is in cinematic mode"), Bystander->bCinematicMode);

	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(FreshTeardown);
	TestEqual(TEXT("A sequence that has not started releases nothing somebody else froze"),
		Bystander->Num(), 3);
	TestTrue(TEXT("and leaves it in cinematic mode"), Bystander->bCinematicMode);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicSelfTerminating,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileStopsWhenTheSequenceEnds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicSelfTerminating::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Watcher = MakeRecordingViewer(Fixture.World, Heroes);
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("The audience controller can be built"), Watcher);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Watcher || !Bystander) return false;

	UTerritoryPlayCutsceneEvent* Event = MakeSuppressingEvent(Fixture.World, Heroes,
		MakeSequence(Fixture.World));
	// A grace, so the actor outlives its sequence and the reconcile is still in place afterwards.
	Event->TeardownGraceSeconds = 1.f;
	Event->ExecuteEvent(nullptr, Watcher, nullptr);

	ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
	TestNotNull(TEXT("The cutscene starts"), Cutscene);
	if (!Cutscene) return false;

	UTerritoryCutsceneTeardownComponent* Teardown = TeardownOn(Cutscene);
	TestNotNull(TEXT("The cutscene has a teardown observer"), Teardown);
	if (!Teardown) return false;
	TestTrue(TEXT("The reconcile is subscribed while the cutscene plays"),
		FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(Teardown));

	SuppressCinematics(Fixture.World, Cutscene->PlaybackSettings);
	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestEqual(TEXT("The bystander is released while the sequence plays"), Bystander->Num(), 2);

	// The sequence ends. The actor survives for the authored grace, so without self-termination the
	// reconcile would keep a world hook and keep sweeping every frame of it.
	FTFTerritoryCutsceneTeardownTestAccess::SequenceStopped(Teardown);
	TestTrue(TEXT("The ending arms the one teardown"), FTFTerritoryCutsceneTeardownTestAccess::IsArmed(Teardown));
	TestTrue(TEXT("and the actor is kept alive for the authored grace"),
		Cutscene->GetLifeSpan() > 0.f);

	FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(Teardown);
	TestFalse(TEXT("The reconcile hands the world tick back once it has nothing to bound"),
		FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(Teardown));
	TestEqual(TEXT("and the grace period releases nobody further"), Bystander->Num(), 2);
	TestEqual(TEXT("nor touches the audience"), Watcher->Num(), 1);

	DestroyCutscenes(Fixture.World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicDialogueShotReceipt,
	"TerritoryFramework.Presentation.Cutscenes.DialogueShotForcedFlagsAreRecordedNotBounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicDialogueShotReceipt::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	// The other half of the receipt: a cutscene Territory starts forces none of the four flags of its
	// own. That is what makes the audience bound above inert for default content - nothing suppresses,
	// so nothing needs releasing - and it is why the shot below is the residual rather than the rule.
	// It is the value the engine reads, on the actor the event actually spawned, not a reading of the
	// authored template.
	{
		FWorldFixture Fixture;
		PlayCutscene(Fixture, 1.f);

		ANarrativeLevelSequenceActor* Cutscene = SoleCutscene(Fixture.World);
		TestNotNull(TEXT("A cutscene starts from an event with no cinematic flags authored"), Cutscene);
		if (Cutscene)
		{
			const FMovieSceneSequencePlaybackSettings& Authored = Cutscene->PlaybackSettings;
			TestFalse(TEXT("A Territory cutscene leaves movement suppression to the designer"),
				Authored.bDisableMovementInput);
			TestFalse(TEXT("and look suppression"),
				Authored.bDisableLookAtInput);
			TestFalse(TEXT("and player hiding"),
				Authored.bHidePlayer);
			TestFalse(TEXT("and HUD hiding"),
				Authored.bHideHud);
		}

		DestroyCutscenes(Fixture.World);
	}

	// This test is a receipt, not coverage. UTerritoryDialogueShot forces three of the same four flags
	// the cutscene audience reconcile now bounds, so a Territory dialogue shot is the unconditional
	// instance of claim 7. It is deliberately left unbounded in this batch: a shot's audience is the
	// conversation's, the dialogue system stops the sequence itself, and bounding it is a
	// dialogue-design question with its own test surface. What this measures is the residual, so that
	// the next reader cannot mistake "cutscenes are bounded" for "Territory is bounded".
	//
	// The flag values themselves are already asserted by DialogueShotSuppressesInput. They are repeated
	// here because a receipt has to state the whole finding in one place; the leg that this test adds is
	// the reflection below, which is why the shot's values are Territory's decision and not an
	// inheritance from the vendor's defaults.
	UTerritoryDialogueShot* Shot = NewObject<UTerritoryDialogueShot>();
	TestNotNull(TEXT("A Territory dialogue shot can be built"), Shot);
	if (!Shot) return false;

	const FMovieSceneSequencePlaybackSettings ShotSettings = Shot->GetPlaybackSettings();
	TestTrue(TEXT("A Territory shot still forces HUD hiding"), ShotSettings.bHideHud);
	TestTrue(TEXT("A Territory shot still forces movement suppression"),
		ShotSettings.bDisableMovementInput);
	TestTrue(TEXT("A Territory shot still forces look suppression"),
		ShotSettings.bDisableLookAtInput);

	// The settings are the designer's, which is why the forced values above are a decision Territory
	// made rather than a value it inherited from the vendor's defaults.
	const FProperty* PlaybackProperty =
		UNarrativeDialogueSequence::StaticClass()->FindPropertyByName(TEXT("PlaybackSettings"));
	TestNotNull(TEXT("PlaybackSettings is declared on the vendor dialogue sequence"),
		PlaybackProperty);
	if (PlaybackProperty)
	{
		TestTrue(TEXT("A designer can author the shot's playback settings"),
			(PlaybackProperty->PropertyFlags & CPF_Edit) != 0);
		TestTrue(TEXT("and a Blueprint can read and write them"),
			(PlaybackProperty->PropertyFlags & CPF_BlueprintVisible) != 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicNothingToBound,
	"TerritoryFramework.Presentation.Cutscenes.AudienceReconcileHoldsNothingWhenThereIsNothingToBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicNothingToBound::RunTest(const FString&)
{
	using namespace TerritoryCutsceneTests;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	FWorldFixture Fixture;
	ATerritoryCinematicRecordingController* Viewer = MakeRecordingViewer(Fixture.World, Heroes);
	// A second local controller is what gives these legs their teeth. With only the audience local,
	// every one of the three inert guards below is masked: the audience covers the world, so nothing
	// is uncovered whatever the guard does, and a red leg that swapped the guard for a no-op would
	// still read green. With a bystander present, each guard is the only thing holding the reconcile
	// away from it - which is what these tests claim.
	ATerritoryCinematicRecordingController* Bystander = MakeRecordingViewer(Fixture.World, Heroes);
	TestNotNull(TEXT("A local controller can be built"), Viewer);
	TestNotNull(TEXT("A second local controller can be built"), Bystander);
	if (!Viewer || !Bystander) return false;

	// An empty audience is the vendor's own "everyone": OwnerControllers gates net relevancy only when
	// it has entries. The engine sweeping every local controller is exactly right for that content, so
	// the reconcile must stay completely out of the way rather than treating empty as "nobody".
	FNarrativeSequencePlaybackSettings EveryoneSettings;
	EveryoneSettings.bDisableMovementInput = true;
	EveryoneSettings.bDisableLookAtInput = true;
	EveryoneSettings.bHidePlayer = true;
	EveryoneSettings.bHideHud = true;

	ANarrativeLevelSequenceActor* Unbounded = SpawnCutscene(Fixture.World, Viewer, EveryoneSettings);
	TestNotNull(TEXT("A cutscene can be built directly from the vendor factory"), Unbounded);
	if (!Unbounded) return false;
	Unbounded->OwnerControllers.Empty();

	UTerritoryCutsceneTeardownComponent* EveryoneTeardown =
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Unbounded, 0.f);
	TestNotNull(TEXT("Teardown still arms for an unbounded audience"), EveryoneTeardown);
	if (EveryoneTeardown)
	{
		TestEqual(TEXT("An audience the vendor left empty covers everyone, so nothing is uncovered"),
			FTFTerritoryCutsceneTeardownTestAccess::UncoveredCount(EveryoneTeardown), 0);
		TestFalse(TEXT("and no per-frame hook is taken"),
			FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(EveryoneTeardown));
		TestFalse(TEXT("nor a play-start binding, so a later start cannot re-open it"),
			FTFTerritoryCutsceneTeardownTestAccess::HasPlayStartBinding(EveryoneTeardown));
	}

	// A sequence that suppresses nothing. EnableCinematicMode returns before touching a controller
	// unless one of the four flags is authored, so there is no suppression to undo and no reason to
	// sweep a world every frame for the length of a cutscene.
	FNarrativeSequencePlaybackSettings SilentSettings;
	SilentSettings.bDisableMovementInput = false;
	SilentSettings.bDisableLookAtInput = false;
	SilentSettings.bHidePlayer = false;
	SilentSettings.bHideHud = false;

	ANarrativeLevelSequenceActor* Silent = SpawnCutscene(Fixture.World, Viewer, SilentSettings);
	TestNotNull(TEXT("A non-suppressing cutscene can be built"), Silent);
	if (Silent)
	{
		UTerritoryCutsceneTeardownComponent* SilentTeardown =
			UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(Silent, 0.f);
		TestNotNull(TEXT("Teardown still arms for a non-suppressing cutscene"), SilentTeardown);
		if (SilentTeardown)
		{
			TestEqual(TEXT("A cutscene that cannot suppress anyone has no audience to bound"),
				FTFTerritoryCutsceneTeardownTestAccess::UncoveredCount(SilentTeardown), 0);
			TestFalse(TEXT("so it takes no per-frame hook either"),
				FTFTerritoryCutsceneTeardownTestAccess::HasTickSubscription(SilentTeardown));
			TestFalse(TEXT("and no play-start binding either"),
				FTFTerritoryCutsceneTeardownTestAccess::HasPlayStartBinding(SilentTeardown));

			// Nothing suppressed the bystander, so there is nothing for the reconcile to release - and
			// this is the half a count alone cannot show: a reconcile that resolved the bystander and
			// did nothing with it would pass the count assertion above on a quiet frame.
			FTFTerritoryCutsceneTeardownTestAccess::SequenceTick(SilentTeardown);
			TestEqual(TEXT("A non-suppressing sequence never touches a controller"), Bystander->Num(), 0);
			TestFalse(TEXT("and the bystander is not in cinematic mode"), Bystander->bCinematicMode);
		}
	}

	TestNull(TEXT("Teardown cannot be armed for a missing actor"),
		UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(nullptr, 0.f));

	DestroyCutscenes(Fixture.World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
