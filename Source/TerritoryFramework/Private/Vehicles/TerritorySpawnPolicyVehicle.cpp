#include "Vehicles/TerritorySpawnPolicyVehicle.h"

#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnTypes.h"
#include "Vehicles/TerritorySpawnExecutorVehicle.h"

UTerritorySpawnPolicyVehicle::UTerritorySpawnPolicyVehicle()
{
}

FGameplayTag UTerritorySpawnPolicyVehicle::GetEntityTag_Implementation() const
{
	return EntityTag;
}

TSubclassOf<UTerritorySpawnExecutor> UTerritorySpawnPolicyVehicle::GetExecutorClass_Implementation() const
{
	return UTerritorySpawnExecutorVehicle::StaticClass();
}

TSoftClassPtr<AActor> UTerritorySpawnPolicyVehicle::GetDesiredActorClass_Implementation() const
{
	return VehicleClass;
}

bool UTerritorySpawnPolicyVehicle::AllowsLease_Implementation(FGameplayTag Purpose) const
{
	if (bAllowMissionLeases)
	{
		return true;
	}

	// Refusing a lease does not just refuse the mission tag: it refuses every pin. A slot that cannot
	// be leased is a slot the framework may retire at any moment, which is only correct for a bay
	// nothing borrows. The interaction lease is included because a player holding an entity is exactly
	// the case that must not be interrupted, so a policy that forbids leases is really saying "this bay
	// is decorative and nothing depends on it".
	return false;
}

FTerritorySpawnDecision UTerritorySpawnPolicyVehicle::EvaluateDecision_Implementation(
	const FTerritorySpawnRequest& Request) const
{
	FTerritorySpawnDecision Decision;

	// ─── A vehicle is here ───
	//
	// Being away from the bay is not the same as being gone, so the default answer is "leave it alone".
	// Only the reclaim below ever asks for a retire.
	if (Request.bHasEntity)
	{
		// ─── Is it still where it belongs? ───
		//
		// The reclaim exists because a bay is where every consumer looks. A player who drives the
		// district's vehicle across the city leaves the bay believing it still holds one, so the next
		// call finds an empty pad. Nothing else in the system notices, because from the record's point of
		// view the slot is perfectly healthy.
		if (!bReclaimWhenAbandoned)
		{
			return Decision;
		}

		// Distance is measured from the ENTITY, not from the record. The record still says "at home" for
		// a vehicle that was driven away; only the tracked transform knows where it actually is.
		const double DistanceFromBay = FVector::Dist2D(
			Request.EntityTransform.GetLocation(), Request.AnchorTransform.GetLocation());
		if (DistanceFromBay < static_cast<double>(ReclaimDistanceCm))
		{
			// Parked where it belongs. Nothing to fix.
			return Decision;
		}

		// A player standing next to the vehicle is loading a patient, changing a tyre, or about to get
		// back in. Retiring it now would remove it from under their hands, which is the exact failure the
		// occupancy interlock exists to prevent — and that interlock only covers a player INSIDE the
		// vehicle. This covers the player who just stepped out.
		if (IsAnyPlayerNearby(Request.EntityTransform.GetLocation(), ReclaimSafeRadiusCm))
		{
			return Decision;
		}

		// Settle time. Measured from the transition that produced this entity, so a job that just spawned
		// and immediately released a vehicle is not reclaimed out from under its own arrival.
		if (ReclaimDelaySeconds > 0.f && Request.Record.LastTransitionGameTime >= 0.0)
		{
			const double LiveSeconds = Request.NowGameTime - Request.Record.LastTransitionGameTime;
			if (LiveSeconds < static_cast<double>(ReclaimDelaySeconds))
			{
				return Decision;
			}
		}

		Decision.Action = ESpawnAction::Retire;
		Decision.RetireMode = RetireMode;
		Decision.Reason = ESpawnTransitionReason::PolicyRequest;

		// The framework still runs its own occupancy test after this answer, which is what protects a
		// driver who is inside the cab. This policy's radius test is the weaker case: a player beside it.
		Decision.MinimumLiveSeconds = ReclaimDelaySeconds;

		// A reclaim is a relocation, so nothing about it should read as destruction. That is also why the
		// condition is captured and restored across it regardless of bPersistDamage: the same physical
		// vehicle drives home, it is not serviced and re-issued.
		Decision.bRestoreCheckpoint = true;
		return Decision;
	}

	if (Request.Record.bHasInstance && !Request.bAwaitingRestore)
	{
		// The record believes a vehicle exists but nothing is tracked. The framework reconciles this
		// before the policy runs, so answering None keeps the two from racing to opposite conclusions.
		//
		// bAwaitingRestore is the load case and must NOT return here: a district whose vehicle was
		// parked when the save was written has to get it back on load, and returning None would leave
		// every bay empty for the whole session.
		return Decision;
	}

	// The replacement delay only applies to a rebuild, never to the first spawn: a district that loads
	// for the first time should have its vehicle immediately, not 45 seconds later.
	if (Request.Record.bWasDestroyed && Request.Record.LastTransitionGameTime >= 0.0)
	{
		const double Elapsed = Request.NowGameTime - Request.Record.LastTransitionGameTime;
		if (Elapsed < static_cast<double>(ReplacementDelaySeconds))
		{
			return Decision;
		}
	}

	Decision.Action = ESpawnAction::Spawn;
	Decision.Reason = Request.Record.bWasDestroyed
		? ESpawnTransitionReason::PolicyRequest
		: ESpawnTransitionReason::InitialSpawn;
	Decision.DesiredClass = VehicleClass;

	// THIS is where bPersistDamage stops being a decoration. A destroyed vehicle is rebuilt by a depot
	// that services it, so the default is a fresh one; a designer who wants damage to be a lasting
	// consequence ticks the box and the record's captured health is restored instead.
	//
	// The distinction that makes this correct rather than arbitrary: a REBUILD is a new vehicle, and a
	// RECLAIM is the same vehicle driving home. The reclaim above always restores; this only governs
	// replacement after a loss.
	Decision.bRestoreCheckpoint = !Request.Record.bWasDestroyed || bPersistDamage;

	if (ReplacementPlacement == ESpawnVehicleReturnPlacement::AsLastLeft
		&& Request.Record.IsTransformUsable()
		&& Request.Record.Generation > 0)
	{
		Decision.DesiredTransform = Request.Record.Transform;
	}
	else
	{
		Decision.DesiredTransform = Request.AnchorTransform;
	}

	return Decision;
}
