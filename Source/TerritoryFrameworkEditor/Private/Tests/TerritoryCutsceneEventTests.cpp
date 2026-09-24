#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/NarrativeLevelSequencePlayer.h"
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
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TerritoryStoryEvents.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/UnrealType.h"

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
	TestTrue(TEXT("Auto Play is forced on, because an event that never plays is not a cutscene"),
		Cutscene->PlaybackSettings.bAutoPlay);
	TestFalse(TEXT("The authored pausing configuration did not survive to the player"),
		Cutscene->NarrativeSequenceParams.bPauseAtEnd);

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

#endif // WITH_DEV_AUTOMATION_TESTS
