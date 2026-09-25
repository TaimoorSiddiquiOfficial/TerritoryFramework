#include "Cinematics/TerritoryCutsceneTeardown.h"

#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

UTerritoryCutsceneTeardownComponent::UTerritoryCutsceneTeardownComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The teardown itself is never replicated: destroying the sequence actor is what reaches clients,
	// through the actor destruction that already replicates. A replicated copy of this component
	// would only add a client that is able to destroy an actor the server owns.
	SetIsReplicatedByDefault(false);
}

UTerritoryCutsceneTeardownComponent* UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(
	ANarrativeLevelSequenceActor* SequenceActor, float GraceSeconds)
{
	if (!IsValid(SequenceActor) || !SequenceActor->HasAuthority()) return nullptr;

	ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
	if (!Player) return nullptr;

	// Idempotent per actor. One cutscene must not be able to arm two teardowns, and a second call is
	// the expected shape when an event is authored on more than one of a floor's event arrays.
	TInlineComponentArray<UTerritoryCutsceneTeardownComponent*> Components(SequenceActor);
	for (auto* Existing : Components)
	{
		if (IsValid(Existing)) return Existing;
	}

	auto* Component = NewObject<UTerritoryCutsceneTeardownComponent>(
		SequenceActor, NAME_None, RF_Transient);
	Component->SequenceActor = SequenceActor;
	Component->SequencePlayer = Player;
	Component->GraceSeconds = FMath::Max(0.f, GraceSeconds);
	SequenceActor->AddInstanceComponent(Component);
	Component->RegisterComponent();

	Player->OnStop.AddUniqueDynamic(Component,
		&UTerritoryCutsceneTeardownComponent::HandleSequenceStopped);
	Player->OnFinished.AddUniqueDynamic(Component,
		&UTerritoryCutsceneTeardownComponent::HandleSequenceStopped);

	// After the bindings, so an already-running sequence cannot slip an ending past a reconcile that
	// is not yet listening.
	Component->ArmAudienceReconcile();

	UE_LOG(LogTerritory, Log,
		TEXT("[CutsceneTeardown] %s will be destroyed %.2fs after its sequence stops"),
		*GetNameSafe(SequenceActor), Component->GraceSeconds);
	return Component;
}

void UTerritoryCutsceneTeardownComponent::HandleSequenceStopped()
{
	// A natural finish reaches StopInternal through FinishPlaybackInternal, which broadcasts OnStop
	// and then OnFinished, so both bindings fire for one ending. Arming once is what keeps that from
	// starting two teardowns.
	if (bTeardownArmed) return;

	// A paused player has not stopped. Narrative's dialogue shots hold their final frame while a line
	// is still displayed and the engine broadcasts OnFinished after Pause(), so destroying the actor
	// here would cut a live shot short. StopInternal assigns Status = Stopped before it broadcasts
	// OnStop (MovieSceneSequencePlayer.cpp:466-529), so this guard only ever rejects the OnFinished
	// path and can never reject a real stop.
	if (const ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		if (Player->IsPaused()) return;
	}

	bTeardownArmed = true;

	ANarrativeLevelSequenceActor* Subject = SequenceActor;
	if (!IsValid(Subject)) return;

	if (GraceSeconds > 0.f)
	{
		// A grace delay rather than an immediate destroy, because this destruction replicates: a
		// client whose own copy is a fraction of a second behind must not have its cutscene cut off
		// mid-shot. SetLifeSpan is authority-gated, which is the same gate this component is created
		// behind, so the two cannot disagree about who owns the actor.
		Subject->SetLifeSpan(GraceSeconds);
		UE_LOG(LogTerritory, Log,
			TEXT("[CutsceneTeardown] %s stopped; destroying in %.2fs"),
			*GetNameSafe(Subject), GraceSeconds);
		return;
	}

	// Never SetLifeSpan(0) here. AActor::SetLifeSpan takes its clear-the-timer branch for any value
	// <= 0 (Actor.cpp:6584-6602) and would leave the actor alive with no timer at all, which is the
	// silent leak this component exists to prevent. An immediate teardown destroys directly.
	UE_LOG(LogTerritory, Log, TEXT("[CutsceneTeardown] %s stopped; destroying now"),
		*GetNameSafe(Subject));
	Subject->Destroy();
}

void UTerritoryCutsceneTeardownComponent::ReconcileAfterPlaybackRequest()
{
	// Playing, or held on its last frame by a dialogue shot. The binding armed above will deliver the
	// ending whenever it arrives, so there is nothing to reconcile. IsPaused covers the dialogue-shot
	// case for the same reason HandleSequenceStopped tests it.
	if (const ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		if (Player->IsPlaying() || Player->IsPaused()) return;
	}

	// Playback is already over, or never began. Both are the same answer to a caller that has just
	// asked for playback: there is nothing to wait for, so tear the actor down - through the one stop
	// path, so the authored grace still applies and the arming guard still holds.
	HandleSequenceStopped();
}

void UTerritoryCutsceneTeardownComponent::HandleSequenceStarted()
{
	// The teardown is armed, so this actor is already on its way out and there is no sweep left to
	// answer. Reachable: a Stop followed by a Play on the same player, inside the authored grace.
	if (bTeardownArmed) return;

	// Every play-start sweeps, so every controller in the pool is owed its release again. This is the
	// whole of what makes a resume work: ULevelSequencePlayer::PlayInternal's own guard is
	// `!IsPlaying()`, so a resume after a pause passes it and StartTimeControllerAndBroadcastPlayState
	// re-arms bPendingOnStartedPlaying from the same function that broadcasts OnPlay - the engine
	// suppresses the world all over again. Without this the reconcile would answer the first play-run
	// only, and a resumed cutscene would hold a bystander frozen for the rest of its playback.
	OwedReleases = UncoveredControllers;
}

void UTerritoryCutsceneTeardownComponent::ArmAudienceReconcile()
{
	// A client must never do this. Its own local controller is the audience for the server's
	// suppression, which arrived over the reliable ClientSetCinematicMode RPC along with the replicated
	// sequence, and releasing it here would fight the server with no authority to do so. The component
	// is already authority-only by construction - ScheduleAfterSequence refuses without it - but the
	// world's net mode is the thing that actually decides whether a local controller exists to release.
	ANarrativeLevelSequenceActor* Subject = SequenceActor;
	UWorld* World = Subject ? Subject->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client) return;

	// An empty audience is the vendor's own "everyone": OwnerControllers gates IsNetRelevantFor only
	// when it has entries, and falls back to standard relevancy when it does not. The engine sweeping
	// every local controller is therefore exactly right for that content and there is nothing to bound.
	const TArray<TObjectPtr<APlayerController>>& Audience = Subject->OwnerControllers;
	if (Audience.IsEmpty()) return;

	// EnableCinematicMode returns before touching any controller unless one of these four flags is
	// authored, so the default case has no suppression to undo and must not pay for a per-frame
	// handler. Read from the actor rather than the player: the player's copy is protected, and the
	// factory assigns both from the same settings, so this is the value the engine will read.
	const FMovieSceneSequencePlaybackSettings& Settings = Subject->PlaybackSettings;
	if (!Settings.bDisableMovementInput && !Settings.bDisableLookAtInput
		&& !Settings.bHidePlayer && !Settings.bHideHud)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		if (!Controller || !Controller->IsLocalController()) continue;

		const bool bInAudience = Audience.ContainsByPredicate(
			[Controller](const TObjectPtr<APlayerController>& Entry)
			{
				return Entry.Get() == Controller;
			});
		if (bInAudience) continue;

		// Something else already put this controller in cinematic mode. Releasing it would take back a
		// suppression no part of this sequence asked for, so it is left exactly as it was found. This is
		// the first of the two bounds on how far the release reaches, and the one that needs no evidence
		// to be decided: it is taken before this sequence has suppressed anything. The second is per
		// play-start, and is held in OwedReleases.
		if (Controller->bCinematicMode) continue;

		UncoveredControllers.Add(Controller);
	}

	// Nothing is owed yet. The debt is incurred by this player's own play-start, below - not by arming,
	// because a sweep that has not happened owes nothing and a controller frozen in the meantime was
	// frozen by somebody else. A player that is *already* playing is the one other case: Play() has
	// happened and the sweep it triggered has not, because bPendingOnStartedPlaying is consumed by the
	// player's next position update rather than by Play() itself. Its debt is already incurred, so it
	// is taken here rather than waited for on a binding that will never fire again.
	OwedReleases.Reset();
	if (const ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		if (Player->IsPlaying()) OwedReleases = UncoveredControllers;
	}

	if (UncoveredControllers.IsEmpty()) return;

	// Registered now, which is after UMovieSceneSequenceTickManager registered its own handler when
	// the factory built the player - and that ordering is what puts this handler *before* playback
	// ticks, not after. TMulticastDelegateBase::Broadcast calls its bound functions in reverse
	// registration order (MulticastDelegateBase.h:298-300, whose own comment gives the reason: an
	// instance added by a callee must not be called in the same broadcast), so the newest binding
	// runs first. The engine's suppression therefore lands after this handler has already looked, and
	// the release it owes arrives on the following frame - which the owed list absorbs without
	// noticing, because a release is not lost by arriving late, only by arriving twice.
	ReconciledWorld = World;
	SequenceTickHandle = World->AddMovieSceneSequenceTickHandler(
		FOnMovieSceneSequenceTick::FDelegate::CreateUObject(
			this, &UTerritoryCutsceneTeardownComponent::HandleSequenceTick));

	// Both starts, not just OnPlay: bReversePlayback picks which one PlayInternal broadcasts, and while
	// nothing in Territory asks for reverse playback, this component does not own the play call - it is
	// handed a player by ScheduleAfterSequence. Missing a start costs a bystander the whole play-run,
	// and the pair costs one line.
	if (ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		Player->OnPlay.AddUniqueDynamic(this,
			&UTerritoryCutsceneTeardownComponent::HandleSequenceStarted);
		Player->OnPlayReverse.AddUniqueDynamic(this,
			&UTerritoryCutsceneTeardownComponent::HandleSequenceStarted);
	}

	UE_LOG(LogTerritory, Log,
		TEXT("[CutsceneTeardown] %s is staged for %d controller(s) and will release %d local controller(s) outside that audience"),
		*GetNameSafe(Subject), Audience.Num(), UncoveredControllers.Num());
}

void UTerritoryCutsceneTeardownComponent::DisarmAudienceReconcile()
{
	// Both hooks, and the play-start binding first: the release is one-way, so a start that arrives
	// after the reconcile has handed its work back must not re-open a debt nothing will answer.
	if (ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		Player->OnPlay.RemoveAll(this);
		Player->OnPlayReverse.RemoveAll(this);
	}

	OwedReleases.Reset();

	// Reset the handle before the world removes it: the release is one-way, and a world mid-teardown
	// must not be asked to remove a handler it has already dropped with itself.
	const FDelegateHandle Handle = SequenceTickHandle;
	SequenceTickHandle.Reset();

	if (!Handle.IsValid()) return;

	if (UWorld* World = ReconciledWorld.Get())
	{
		World->RemoveMovieSceneSequenceTickHandler(Handle);
	}
	ReconciledWorld.Reset();
}

void UTerritoryCutsceneTeardownComponent::HandleSequenceTick(float DeltaSeconds)
{
	// The ending has been handled, which means the destruction is already scheduled and this reconcile
	// has nothing left to bound. It is reachable: the actor outlives its sequence by the whole grace
	// period, and a released controller must not be released again on every frame of it.
	if (bTeardownArmed)
	{
		DisarmAudienceReconcile();
		return;
	}

	const ULevelSequencePlayer* Player = SequencePlayer.Get();
	ANarrativeLevelSequenceActor* Subject = SequenceActor;
	if (!Player || !Subject)
	{
		DisarmAudienceReconcile();
		return;
	}

	// Playback status deliberately does not gate this. What is being undone is this sequence's own
	// sweep, and a sweep is not undone by a pause: the engine releases cinematic mode only from
	// OnStopped, so a bystander still owed a release when a pause lands between the sweep and this
	// handler would stay frozen for the whole pause. The audience - the controller the suppression is
	// actually for - is not in this list, so leaving the gate off cannot release it.
	const FMovieSceneSequencePlaybackSettings& Settings = Subject->PlaybackSettings;

	for (int32 Index = OwedReleases.Num() - 1; Index >= 0; --Index)
	{
		APlayerController* Controller = OwedReleases[Index].Get();
		if (!Controller)
		{
			// Destroyed mid-cutscene. Dropped rather than held, because a release must never resurrect
			// it and the debt can never be paid.
			OwedReleases.RemoveAt(Index);
			continue;
		}

		// This gate is what makes the loop affordable rather than a nicety. APlayerController's outer
		// setter has no change guard - it assigns bCinematicMode and calls ClientSetCinematicMode, a
		// reliable RPC, unconditionally - so an ungated release would send that RPC every frame for
		// every bystander.
		//
		// It is also the "this one has not been answered yet" test, and the reason an owed release is a
		// state rather than a one-shot: the frame between Play() and the sweep it triggers reads not
		// cinematic, and the release it owes must survive that frame rather than be spent on it.
		if (!Controller->bCinematicMode) continue;

		// The actor's settings are the engine's own input to EnableCinematicMode, so releasing with them
		// is the exact inverse of the call being undone - the same flags, the same argument order.
		Controller->SetCinematicMode(false, Settings.bHidePlayer, Settings.bHideHud,
			Settings.bDisableMovementInput, Settings.bDisableLookAtInput);

		// Dropped, not merely flagged: the suppression this controller carries has now been answered, so
		// anything that freezes it from here on - another cutscene's sweep, another system's cinematic -
		// is not this sequence's to undo. Releasing it again is the defect this list exists to prevent.
		OwedReleases.RemoveAt(Index);

		UE_LOG(LogTerritory, Log,
			TEXT("[CutsceneTeardown] Released %s, which is not in this cutscene's audience"),
			*GetNameSafe(Controller));
	}
}

void UTerritoryCutsceneTeardownComponent::UnbindFromPlayer()
{
	DisarmAudienceReconcile();

	if (ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		Player->OnStop.RemoveAll(this);
		Player->OnFinished.RemoveAll(this);
	}
	SequencePlayer.Reset();
}

void UTerritoryCutsceneTeardownComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	UnbindFromPlayer();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UTerritoryCutsceneTeardownComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UnbindFromPlayer();
	Super::EndPlay(Reason);
}
