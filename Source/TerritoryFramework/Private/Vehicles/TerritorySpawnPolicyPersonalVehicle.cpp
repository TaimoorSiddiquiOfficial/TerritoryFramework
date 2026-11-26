#include "Vehicles/TerritorySpawnPolicyPersonalVehicle.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnTypes.h"
#include "Vehicles/TerritorySpawnExecutorVehicle.h"

UTerritorySpawnPolicyPersonalVehicle::UTerritorySpawnPolicyPersonalVehicle()
{
}

FGameplayTag UTerritorySpawnPolicyPersonalVehicle::GetEntityTag_Implementation() const
{
	return EntityTag;
}

TSubclassOf<UTerritorySpawnExecutor> UTerritorySpawnPolicyPersonalVehicle::GetExecutorClass_Implementation() const
{
	return UTerritorySpawnExecutorVehicle::StaticClass();
}

TSoftClassPtr<AActor> UTerritorySpawnPolicyPersonalVehicle::GetDesiredActorClass_Implementation() const
{
	return VehicleClass;
}

bool UTerritorySpawnPolicyPersonalVehicle::ShouldEvaluate_Implementation(const FTerritorySpawnRequest& Request) const
{
	// An unclaimed bay does nothing at all. This is checked here rather than in EvaluateDecision because a
	// skipped slot costs one tag comparison, while an evaluated-then-refused slot costs a full policy pass
	// on every sweep for as long as the server is up.
	if (bRequireOwnerClaim)
	{
		const ATerritorySpawnAnchor* const Anchor = GetOwningAnchor();
		if (!Anchor || Anchor->GetClaimedByPlayerName().IsNone())
		{
			return false;
		}
	}

	return true;
}

bool UTerritorySpawnPolicyPersonalVehicle::WasWrecked(const FTerritorySpawnRequest& Request) const
{
	return Request.Record.bWasDestroyed;
}

FTerritorySpawnDecision UTerritorySpawnPolicyPersonalVehicle::EvaluateDecision_Implementation(
	const FTerritorySpawnRequest& Request) const
{
	FTerritorySpawnDecision Decision;

	// ─── A car is here ───
	if (Request.bHasEntity)
	{
		// The default answer is still "leave it alone". Retiring a car is opt-in per bay, because a car the
		// player parked and walked away from is a car they are coming back to.
		if (!bDespawnWhenAbandoned)
		{
			return Decision;
		}

		// The occupancy question is NOT asked here. The framework asks it via the executor before any
		// retire, and it uses the project's own predicate, which knows about mounted passengers as well as
		// a possessing controller. Asking here would be a second implementation of one rule.
		//
		// What only this policy can decide is whether the OWNER has left the area, which is a distance the
		// framework has no opinion about. The owner is the player who claimed the bay.
		const ATerritorySpawnAnchor* const Anchor = GetOwningAnchor();
		const FName ClaimedBy = Anchor ? Anchor->GetClaimedByPlayerName() : FName();
		if (ClaimedBy.IsNone())
		{
			// Nobody owns this bay, so there is no "player is far away" to measure. Leaving the car in the
			// world is correct: it is the only state that does not guess.
			return Decision;
		}

		const APawn* OwnerPawn = nullptr;
		if (UWorld* const World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* const PC = It->Get();
				if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == ClaimedBy)
				{
					OwnerPawn = PC->GetPawn();
					break;
				}
			}
		}

		if (!OwnerPawn)
		{
			// The owner is not in the world at all (logged out, or not yet spawned). That is NOT an
			// abandonment to act on: the record keeps the car, and the next session finds it where it was.
			// Treating a logout as abandonment would delete a car every time a player crashed.
			return Decision;
		}

		const double DistanceCm = FVector::Dist(OwnerPawn->GetActorLocation(), Request.EntityTransform.GetLocation());
		if (DistanceCm < static_cast<double>(AbandonDistanceCm))
		{
			return Decision;
		}

		// The owner being far away is necessary but not sufficient. Somebody else may be standing right
		// next to the car — the owner's passenger who has not walked off yet, a second player, a player who
		// just got out. Retiring then removes the car in front of them, and the framework's own interlock
		// will not catch it because that one asks whether somebody is INSIDE the vehicle.
		if (IsAnyPlayerNearby(Request.EntityTransform.GetLocation(), AbandonSafeRadiusCm))
		{
			return Decision;
		}

		Decision.Action = ESpawnAction::Retire;
		Decision.RetireMode = AbandonRetireMode;
		Decision.Reason = ESpawnTransitionReason::PolicyRequest;
		// Grace measured from the last transition would be wrong here: the car has been live for hours.
		// The framework's own occupancy interlock still runs after this answer, which is the guard that
		// matters.
		Decision.MinimumLiveSeconds = ReturnGraceSeconds;
		return Decision;
	}

	// Nothing tracked. Two reasons, and they differ in what should happen next.
	if (Request.Record.bHasInstance && !Request.bAwaitingRestore)
	{
		// The record believes a car exists but nothing is tracked. That is the reconcile case, and the
		// framework handles it before the policy is ever asked (the sweep turns it into a dormancy commit
		// with bWasDestroyed preserved). Returning None avoids racing that commit.
		//
		// bAwaitingRestore is excluded because a car restored from a save is exactly the case this policy
		// must SERVE: the record promises a car and the framework is asking where to put it. Treating that
		// as a reconcile would leave every personal car permanently missing after every load.
		return Decision;
	}

	const bool bWrecked = WasWrecked(Request);

	// A wrecked car waits out its replacement delay; a car that was simply put away waits out the return
	// grace. Two different clocks because they mean different things: one is a consequence the player
	// should feel, the other is a courtesy window so getting out does not instantly teleport the car home.
	const double WaitSeconds = bWrecked
		? (bReplaceVehicleWhenWrecked ? static_cast<double>(WreckReplacementDelaySeconds) : -1.0)
		: static_cast<double>(ReturnGraceSeconds);

	if (bWrecked && !bReplaceVehicleWhenWrecked)
	{
		// Wrecks are permanent by authoring. Nothing to do, ever, for this slot.
		return Decision;
	}

	// The grace is measured from the moment the slot last changed state, which is the moment the car
	// disappeared. A negative timestamp means nothing has ever happened here, so there is nothing to wait
	// for: this is a first spawn.
	if (Request.Record.LastTransitionGameTime >= 0.0)
	{
		const double Elapsed = Request.NowGameTime - Request.Record.LastTransitionGameTime;
		if (Elapsed < WaitSeconds)
		{
			return Decision;
		}
	}

	Decision.Action = ESpawnAction::Spawn;
	Decision.Reason = bWrecked
		? ESpawnTransitionReason::PolicyRequest
		: ESpawnTransitionReason::InitialSpawn;
	Decision.DesiredClass = VehicleClass;

	// The return-condition switch was documented here from the beginning and read by nobody, which made
	// a designer's "Fresh Health" choice do nothing at all. It is now the decision the framework obeys.
	//
	// The default is PreserveDamage, and that default is the interesting part: a personal car is the SAME
	// car coming back, so silently repairing it every time the player walks away would erase damage the
	// player caused and never notice. "Fresh Health" stays available because a project may want the bay
	// to service the car, which is a different game and a different answer.
	Decision.bRestoreCheckpoint = (ReturnCondition == ESpawnVehicleReturnCondition::PreserveDamage);

	// Placement. "As last left" is only meaningful when the record's transform survived and is usable; a
	// corrupt record must fall back to the authored pad rather than putting a car at NaN. The framework
	// already refuses an unusable transform at load, so this is a second guard at the point of use.
	if (ReturnPlacement == ESpawnVehicleReturnPlacement::AsLastLeft
		&& Request.Record.IsTransformUsable()
		&& Request.Record.Generation > 0)
	{
		Decision.DesiredTransform = Request.Record.Transform;
	}
	else
	{
		// Always the anchor's own authored pad. A personal car bay that has never held a car has no
		// "last left" to honour, and the pad is the one position a designer actually chose.
		Decision.DesiredTransform = Request.AnchorTransform;
	}

	return Decision;
}
