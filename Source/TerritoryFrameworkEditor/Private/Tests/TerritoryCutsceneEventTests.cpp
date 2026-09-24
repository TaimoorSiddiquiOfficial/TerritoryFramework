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
	 */
	APlayerController* MakeFactionViewer(UWorld* World, const FGameplayTag& Faction)
	{
		if (!World) return nullptr;
		UGameInstance* Instance = World->GetGameInstance();
		APlayerController* Viewer = World->SpawnActor<APlayerController>();
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

#endif // WITH_DEV_AUTOMATION_TESTS
