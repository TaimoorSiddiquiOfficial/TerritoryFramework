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
 * someone else's cutscene. This component releases exactly the controllers the audience does not name,
 * one frame after the engine suppresses them, and only while the sequence is genuinely playing.
 *
 * That release is Territory-owned rather than a change to the flags, because PlaybackSettings
 * replicates: stripping the flags on the authority would strip them on the audience client too, which
 * is the one client that should keep them.
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
 * same one. That is sound only because the reconcile repeats: the engine suppresses once, on the
 * player's first update after Play(), from a flag that never re-arms, so a release that arrives one
 * frame later is the last word on that controller. Repeating is also why the bCinematicMode gate is
 * load-bearing rather than an optimisation.
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

	/** Release the player bindings. Safe to call more than once. */
	void UnbindFromPlayer();

	/**
	 * Resolve the local controllers this sequence would wrongly suppress and subscribe the reconcile.
	 *
	 * Does nothing - and holds no tick - when there is nothing to bound: no authority, an audience the
	 * vendor left empty (its own "everyone", which makes the engine's sweep correct), a sequence with
	 * none of the four cinematic flags authored (so EnableCinematicMode never touches a controller),
	 * or a world whose local controllers the audience already covers. Those are the single-player,
	 * client and dedicated-server cases, and they are inert by construction rather than by a test.
	 */
	void ArmAudienceReconcile();

	/** Drop the world tick subscription. Safe to call more than once, and with no world left. */
	void DisarmAudienceReconcile();

	/** The world's per-frame sequence tick. Releases uncovered controllers while playback runs. */
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
	 */
	TArray<TWeakObjectPtr<APlayerController>> UncoveredControllers;

	TWeakObjectPtr<UWorld> ReconciledWorld;
	FDelegateHandle SequenceTickHandle;
};
