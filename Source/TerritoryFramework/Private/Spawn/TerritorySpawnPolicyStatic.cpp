#include "Spawn/TerritorySpawnPolicyStatic.h"

#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnExecutorActor.h"
#include "Spawn/TerritorySpawnTags.h"

UTerritorySpawnPolicyStatic::UTerritorySpawnPolicyStatic()
{
}

FGameplayTag UTerritorySpawnPolicyStatic::GetEntityTag_Implementation() const
{
	// Zero tag when unset, which makes the anchor refuse to run rather than spawn into a bucket
	// nobody can query. The base class treats an invalid tag as "not configured".
	return EntityTag;
}

TSubclassOf<UTerritorySpawnExecutor> UTerritorySpawnPolicyStatic::GetExecutorClass_Implementation() const
{
	return UTerritorySpawnExecutorActor::StaticClass();
}

TSoftClassPtr<AActor> UTerritorySpawnPolicyStatic::GetDesiredActorClass_Implementation() const
{
	return ActorClass;
}

FTerritorySpawnDecision UTerritorySpawnPolicyStatic::EvaluateDecision_Implementation(
	const FTerritorySpawnRequest& Request) const
{
	FTerritorySpawnDecision Decision;

	// bAwaitingRestore is the load case: the record promises an entity that was never materialised this
	// session. It is not a "holding" state, or a restored prop would be treated as already present and
	// never created again. A record that believes it holds something the framework is not tracking, and
	// that the framework has not flagged as owed, was reconciled before this policy ran.
	const bool bHoldingEntity = Request.Record.bHasInstance && !Request.bAwaitingRestore;

	// Nothing is supposed to be here yet, or something should be here and is not. Either way the
	// answer is the same first move: create at the slot's authored transform.
	if (!bHoldingEntity)
	{
		// A slot whose entity was destroyed by gameplay must respect the recovery mode. A one-shot
		// prop stays gone; a rebuild-if-lost slot waits out its delay so the player does not see the
		// same crate pop back into existence on the next frame.
		if (Request.Record.bWasDestroyed && RecoveryMode == ESpawnPolicyRecoveryMode::InitialSpawn)
		{
			return Decision;
		}

		if (Request.Record.bWasDestroyed && Request.Record.LastTransitionGameTime >= 0.0)
		{
			const double Elapsed = Request.NowGameTime - Request.Record.LastTransitionGameTime;
			if (Elapsed < static_cast<double>(RespawnDelaySeconds))
			{
				// Not yet. Returning None is not a failure: the slot simply has nothing to do this
				// sweep, and the framework does not count a no-op as a retry.
				return Decision;
			}
		}

		Decision.Action = ESpawnAction::Spawn;
		Decision.Reason = Request.Record.bWasDestroyed
			? ESpawnTransitionReason::PolicyRequest
			: ESpawnTransitionReason::InitialSpawn;

		// Always the anchor's own transform. A static slot has exactly one intended place, and reading
		// it from the request rather than from the record is what stops a saved transform (which the
		// framework only writes for entities that move) from quietly overriding an authored pad.
		Decision.DesiredTransform = Request.AnchorTransform;
		Decision.DesiredClass = ActorClass;
		Decision.RetryDelaySeconds = 0.f;
		return Decision;
	}

	// Something is supposed to be here. This policy never asks for a retire: a static slot's entity
	// leaves through gameplay (a destroyed prop) or through the framework (a load reconcile), never
	// because the policy changed its mind.
	return Decision;
}
