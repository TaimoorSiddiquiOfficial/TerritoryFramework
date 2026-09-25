#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "TerritoryCutsceneTeardown.generated.h"

class ANarrativeLevelSequenceActor;
class APlayerController;
class ULevelSequencePlayer;

/**
 * Destroys the cutscene sequence actor Territory created, once its sequence has genuinely stopped.
 *
 * The vendor factory spawns the sequence actor and hands it back through OutActor with no
 * self-cleanup of its own, so the actor's lifetime is the caller's contract, and Territory's caller
 * is UTerritoryPlayCutsceneEvent. This component is that caller's discharge of it.
 *
 * It is attached to the actor it will destroy rather than kept anywhere else, so it cannot outlive
 * its subject, and it is created on the authority only: a client must never locally destroy a
 * replicated actor, and the client's copy is removed by the same destruction replicating.
 *
 * Teardown is driven by OnStop, which is the only signal covering all three ways a cutscene ends. A
 * natural finish reaches StopInternal through FinishPlaybackInternal, an explicit Stop() calls
 * StopInternal directly, and a skip-to-end - GoToEndAndStop - also calls StopInternal directly and
 * therefore never broadcasts OnFinished at all. OnFinished is bound as well but behind a paused
 * guard, because StopInternal skips the whole OnStop broadcast when the entity system runner
 * declines to queue its final update (MovieSceneSequencePlayer.cpp:540), and a sequence that
 * genuinely stopped would otherwise leak. Pausing is deliberately not treated as a stop: Narrative's
 * dialogue shots pause on their last frame while a line is still displayed and the engine broadcasts
 * OnFinished after Pause(), so a paused player is left alone.
 *
 * Both bindings can fire for one finish, so arming is idempotent.
 *
 * It also bounds what the sequence does to the *authority's* local controllers, which is a separate
 * problem from replication. ULevelSequencePlayer::EnableCinematicMode walks every local controller in
 * its own world with no audience input at all - it never consults OwnerControllers, which the vendor
 * only applies to net relevancy. On a listen server, a cutscene staged for one player therefore
 * suppresses movement and look on every local controller that world has, including the host watching
 * someone else's cutscene. This component releases exactly the controllers the audience does not name.
 *
 * That release is Territory-owned rather than a change to the flags, because PlaybackSettings
 * replicates: stripping the flags on the authority would strip them on the audience client too, which
 * is the one client that should keep them.
 *
 * It is bounded twice, and both bounds are needed because APlayerController::bCinematicMode is a bare
 * bool: it reports that a controller is suppressed and never reports by whom.
 *
 * The first bound is decided before any suppression exists. A controller already in cinematic mode
 * when this is armed is not in the pool at all - something else put it there deliberately.
 *
 * The second is per sweep. The engine does not sweep once per player, it sweeps once per *play-start*:
 * PlayInternal's own guard is `!IsPlaying()`, so a resume from a pause passes it, and
 * StartTimeControllerAndBroadcastPlayState re-arms bPendingOnStartedPlaying from the same function
 * that broadcasts OnPlay. So each play-start owes the pool one release, and a controller leaves that
 * debt the moment it is paid. A controller suppressed while it is still owed was suppressed by this
 * sequence's most recent sweep; one suppressed after its release was suppressed by something else,
 * and is left alone. Without that second bound the reconcile would take back any cinematic call in
 * the world, its own or not, for the whole of playback.
 *
 * No playback-status gate belongs on the release, for the same reason. The engine's own release comes
 * only from OnStopped, so a bystander still owed when a pause lands between the sweep and the handler
 * would stay frozen for the whole pause - movement and look taken from a player the cutscene is not
 * for, for as long as somebody else is watching one. Releasing it cannot touch the audience, which is
 * what the suppression is for and is not in the pool.
 *
 * The per-frame hook is the world's MovieSceneSequenceTick delegate, not this component's tick. The
 * sequence player is ticked by UMovieSceneSequenceTickManager, which registers on that same delegate,
 * and this component cannot subscribe the usual way: AActor::PrimaryActorTick.bCanEverTick is false on
 * ALevelSequenceActor (FTickFunction's own constructor initialises to disabled, and the actor leaves
 * even the explicit assignment commented out - LevelSequenceActor.cpp:97), AActor::SetActorTickEnabled
 * is gated on that flag so it returns without a word (Actor.cpp:1754-1760), and
 * AActor::RegisterActorTickFunctions is protected so no component can re-register it.
 *
 * Being on that delegate is what makes the reconcile possible, and it is worth being precise about why,
 * because the ordering is not the obvious one. A multicast delegate invokes in *reverse* registration
 * order, so this handler - registered after the tick manager - runs *before* playback ticks, not after.
 * The release therefore lands on the frame following the engine's suppression rather than inside the
 * same one, and an owed release is what makes that lag harmless: the first frame the controller reads
 * cinematic is the frame it is released on, whenever that frame arrives. That is also why the
 * bCinematicMode gate is load-bearing rather than an optimisation - it is the test for "this one has
 * not been answered yet".
 *
 * Authority note: the destruction is server-authoritative and replicates like any other actor
 * removal. Nothing here is saved - the sequence actor is spawned RF_Transient - so this changes no
 * durable state and needs no migration.
 */
UCLASS(BlueprintType, NotBlueprintable, Transient)
class TERRITORYFRAMEWORK_API UTerritoryCutsceneTeardownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryCutsceneTeardownComponent();

	/**
	 * Arm teardown for a sequence actor Territory just created from the vendor factory.
	 *
	 * Server-only: returns null on a client. Idempotent per actor, so calling it twice for one
	 * cutscene cannot arm two teardowns.
	 *
	 * GraceSeconds is the delay between the sequence stopping and the actor being destroyed. The
	 * destruction replicates, so the grace is what keeps a client whose own copy is a moment behind
	 * from having its cutscene cut short. Zero or less destroys as soon as the sequence stops.
	 */
	static UTerritoryCutsceneTeardownComponent* ScheduleAfterSequence(
		ANarrativeLevelSequenceActor* SequenceActor, float GraceSeconds);

	/**
	 * Reconcile teardown against the status the sequence player was left in, for a caller that has just
	 * asked it to play.
	 *
	 * Arming subscribes to OnStop and OnFinished, so an ending that arrives *after* arming is always
	 * caught. An ending that has already happened is not, and one that happened before arming existed
	 * is lost forever: a StopTags tag, a zero-length sequence or an immediate cancellation can end
	 * playback inside the Play() call itself, broadcasting both signals to a player whose bindings did
	 * not exist yet. The caller that started playback is the only party positioned to notice, which is
	 * why this is called by the caller and not from Arming.
	 *
	 * A player that is playing, or held by a dialogue shot, is left alone - the binding will deliver
	 * its ending. Anything else is handed to the same stop path the bindings use, so the grace, the
	 * paused rule and the idempotence guard all stay in one place.
	 *
	 * Note carefully what this is not: a general "already stopped, so tear down" test that arming
	 * could perform on its own. A never-played player is indistinguishable from a played-and-stopped
	 * one through the player's public API - both answer not-playing and not-paused, because
	 * ULevelSequencePlayer leaves its status at its zero value until something plays it, and
	 * StopInternal is a no-op unless the player is playing or paused - and the status itself is
	 * protected, so no caller can read it at all. Arming on that reading would destroy every cutscene
	 * the instant it was armed. This is only sound because its caller knows it has just asked this
	 * exact player to play.
	 */
	void ReconcileAfterPlaybackRequest();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	// The paused guard is the one branch a future refactor is most likely to drop, and it can only be
	// reached behaviourally by holding a real player on its final frame. The seam is how the test
	// does that, the same way Territory's light rig test reaches its own end-of-playback handler.
	// It also reaches the audience reconcile, whose entry point is the world's sequence tick delegate:
	// the reconcile is only correct when it runs after playback has ticked, and a test that called it
	// by hand would be asserting its own ordering rather than the engine's.
	friend class FTFTerritoryCutsceneTeardownTestAccess;

	/** One sequence has ended. Arms the teardown exactly once for this actor. */
	UFUNCTION() void HandleSequenceStopped();

	/**
	 * One play-start, which is one sweep of the engine's to answer.
	 *
	 * Bound to the player's own OnPlay and OnPlayReverse, because they are the last thing
	 * StartTimeControllerAndBroadcastPlayState does before the engine re-arms the suppression it is
	 * about to apply - so this is the exact frame at which the debt is incurred, rather than a frame
	 * later when a poll of IsPlaying() would notice it.
	 */
	UFUNCTION() void HandleSequenceStarted();

	/** Release the player bindings. Safe to call more than once. */
	void UnbindFromPlayer();

	/**
	 * Resolve the local controllers this sequence would wrongly suppress, and take the two hooks the
	 * reconcile needs: the world's sequence tick, and the player's play-start.
	 *
	 * Does nothing - and takes neither hook - when there is nothing to bound: no authority, an audience
	 * the vendor left empty (its own "everyone", which makes the engine's sweep correct), a sequence
	 * with none of the four cinematic flags authored (so EnableCinematicMode never touches a
	 * controller), or a world whose local controllers the audience already covers. Those are the
	 * single-player, client and dedicated-server cases, and they are inert by construction rather than
	 * by a test.
	 */
	void ArmAudienceReconcile();

	/** Drop both reconcile hooks. Safe to call more than once, and with no world left. */
	void DisarmAudienceReconcile();

	/**
	 * The world's per-frame sequence tick. Pays the releases the last play-start owes.
	 *
	 * Runs whether or not the sequence is still playing, deliberately: see the class comment on why no
	 * playback-status gate belongs here.
	 */
	void HandleSequenceTick(float DeltaSeconds);

	UPROPERTY(Transient) TObjectPtr<ANarrativeLevelSequenceActor> SequenceActor;
	TWeakObjectPtr<ULevelSequencePlayer> SequencePlayer;
	float GraceSeconds = 0.f;
	bool bTeardownArmed = false;

	/**
	 * The local controllers this sequence is not for, snapshotted at arming.
	 *
	 * Weak, because a controller can be destroyed while the cutscene plays and a release must never
	 * resurrect it. A controller that was already in cinematic mode when this was armed is excluded:
	 * something else put it there deliberately, and Territory has no standing to take it back.
	 *
	 * This is the pool - it decides which controllers this component may *ever* release, once, before
	 * there is any suppression to misread. What is currently owed a release is OwedReleases.
	 */
	TArray<TWeakObjectPtr<APlayerController>> UncoveredControllers;

	/**
	 * Of that pool, the controllers still owed a release for the current play-run.
	 *
	 * Filled from the pool on every play-start and emptied as each release lands. That is what makes
	 * the release attributable without an owner field on bCinematicMode: the engine applies its
	 * suppression to every local controller on each play-start, so a controller that reads cinematic
	 * while it is still owed was suppressed by this sequence's most recent sweep, and one that reads
	 * cinematic after its release was suppressed by somebody else.
	 *
	 * Empty is also the "no sweep of ours can exist yet" state. Nothing is owed until this player
	 * reaches a play-start, so a controller another system freezes while this sequence is still arming
	 * - or one that never plays at all - is never released.
	 */
	TArray<TWeakObjectPtr<APlayerController>> OwedReleases;

	TWeakObjectPtr<UWorld> ReconciledWorld;
	FDelegateHandle SequenceTickHandle;
};
