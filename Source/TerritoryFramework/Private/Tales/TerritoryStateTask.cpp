#include "Tales/TerritoryStateTask.h"

#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryVolume.h"
#include "Navigation/MapMarker.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "GameFramework/Pawn.h"

namespace
{
	bool IsPresenceObjective(ETerritoryStateTaskObjective Objective)
	{
		return Objective == ETerritoryStateTaskObjective::EnterTerritory
			|| Objective == ETerritoryStateTaskObjective::LeaveTerritory;
	}
}

void UTerritoryStateTask::BeginTask()
{
	bWasInsideTarget = false;
	ObservedPawn.Reset();
	bHasPresenceObservation = false;
	bHasObjectiveObservation = false;
	bObservedObjectiveSatisfied = false;
	if (IsPresenceObjective(Objective)
		|| Objective == ETerritoryStateTaskObjective::BecomeAvailable)
	{
		// Presence has no Territory delegate. Effective availability also depends on
		// ancestors, so a low-frequency check catches an ancestor unlocking.
		TickInterval = 0.25f;
	}

	Super::BeginTask();
	if (!bIsActive || !OwningComp || CurrentProgress >= RequiredQuantity
		|| !TargetTerritory.IsValid())
	{
		return;
	}

	UWorld* World = OwningComp->GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TerritoryStateTask] Territory Registry is unavailable"));
		return;
	}

	Registry->OnTerritoryRegistered.AddUniqueDynamic(
		this, &UTerritoryStateTask::HandleTerritoryRegistered);
	Registry->OnTerritoryUnregistered.AddUniqueDynamic(
		this, &UTerritoryStateTask::HandleTerritoryUnregistered);

	if (ATerritoryVolume* Territory =
		Registry->GetTerritoryByTag(TargetTerritory))
	{
		BindTerritory(Territory);
		EvaluateCurrent(true);
	}
	else if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugTales())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[TerritoryStateTask] Waiting for streamed Territory %s"),
			*TargetTerritory.ToString());
	}
}

void UTerritoryStateTask::EndTask()
{
	UnbindTerritory();
	if (UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr)
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryStateTask::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(
				this, &UTerritoryStateTask::HandleTerritoryUnregistered);
		}
	}
	bWasInsideTarget = false;
	Super::EndTask();
}

void UTerritoryStateTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;

	ATerritoryVolume* Territory = CachedTerritory.Get();
	if (!Territory)
	{
		Territory = ResolveTerritory();
		if (Territory) BindTerritory(Territory);
	}
	if (!Territory) return;

	if (Objective == ETerritoryStateTaskObjective::BecomeAvailable)
	{
		EvaluateCurrent(false);
		return;
	}
	if (IsPresenceObjective(Objective)) ObservePresence();
}

void UTerritoryStateTask::ObservePresence()
{
	ATerritoryVolume* Territory = CachedTerritory.Get();
	APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
	if (!Territory || !Pawn)
	{
		ObservedPawn.Reset();
		bHasPresenceObservation = false;
		bWasInsideTarget = false;
		return;
	}
	const bool bInside = Territory->ContainsPoint(Pawn->GetActorLocation());
	const bool bNewObservation = !bHasPresenceObservation || ObservedPawn.Get() != Pawn;
	const bool bEntered = bInside && (bNewObservation ? bCompleteIfAlreadySatisfied : !bWasInsideTarget);
	const bool bLeft = !bNewObservation && bWasInsideTarget && !bInside;
	ObservedPawn = Pawn;
	bHasPresenceObservation = true;
	bWasInsideTarget = bInside;
	if ((Objective == ETerritoryStateTaskObjective::EnterTerritory && bEntered)
		|| (Objective == ETerritoryStateTaskObjective::LeaveTerritory && bLeft)) CompleteTask();
}

const FTerritoryFloorSnapshot* UTerritoryStateTask::FindTargetFloor(
	const TArray<FTerritoryFloorSnapshot>& Floors) const
{
	if (!IsFloorFiltered()) return nullptr;
	return Floors.FindByPredicate(
		[this](const FTerritoryFloorSnapshot& Entry)
		{
			return Entry.FloorIndex == TargetFloor;
		});
}

bool UTerritoryStateTask::IsObjectiveSatisfiedBy(
	const ATerritoryVolume* Territory) const
{
	if (!Territory || Territory->GetTerritoryTag() != TargetTerritory)
	{
		return false;
	}

	if (IsFloorFiltered())
	{
		// GetGarrisonSnapshot() returns by value, so the snapshot has to be named before a floor
		// entry is taken out of it. Reading the entry straight from the accessor left the pointer
		// aimed at a temporary that died at the end of that statement, which a release allocator
		// happily serves intact and the stomp allocator turns into an access violation.
		const FTerritoryGarrisonSnapshot Garrison = Territory->GetGarrisonSnapshot();
		// An undeclared floor is never satisfied. Falling back to the whole Place here
		// would complete an objective the author never asked for.
		const FTerritoryFloorSnapshot* Floor = FindTargetFloor(Garrison.Floors);
		if (!Floor) return false;
		switch (Objective)
		{
		case ETerritoryStateTaskObjective::AllDefendersDefeated:
			// The cleared rule and the floor-cleared event share one definition, so a floor
			// cannot satisfy the quest while the framework still counts defenders on it.
			return Floor->IsCleared();
		case ETerritoryStateTaskObjective::ReachDesiredGarrison:
			return Floor->ActiveGuards >= FMath::Max(1, RequiredQuantity);
		default:
			break;
		}
	}

	switch (Objective)
	{
	case ETerritoryStateTaskObjective::BecomeAvailable:
		return Territory->IsAvailableForGameplay();
	case ETerritoryStateTaskObjective::BecomeLocked:
		return Territory->GetTerritoryAvailability()
			== ETerritoryAvailability::Locked;
	case ETerritoryStateTaskObjective::BecomeUnclaimed:
		return Territory->GetTerritoryState() == ETerritoryState::Unclaimed;
	case ETerritoryStateTaskObjective::BecomeContested:
		return Territory->GetTerritoryState() == ETerritoryState::Contested;
	case ETerritoryStateTaskObjective::BecomeClaimed:
		return Territory->GetTerritoryState() == ETerritoryState::Claimed;
	case ETerritoryStateTaskObjective::AllDefendersDefeated:
		return Territory->GetDefenderCount() == 0
			// Initial evaluation must agree with the authoritative defeat event.
			// A replacement delay is still part of the fight. Use the replicated
			// garrison view so Blueprint previews also work without client-side posts.
			&& Territory->GetGarrisonSnapshot().PendingDeployments == 0
			&& (Territory->GetDesiredGuardCount() > 0
				|| Territory->GetConfiguredGuardCount() > 0);
	case ETerritoryStateTaskObjective::ReachDesiredGarrison:
		return Territory->GetDesiredGuardCount() >= FMath::Max(1, RequiredQuantity);
	case ETerritoryStateTaskObjective::EnterTerritory:
	case ETerritoryStateTaskObjective::LeaveTerritory:
	{
		APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
		if (!Pawn) return false;
		const bool bInside = Territory->ContainsPoint(Pawn->GetActorLocation());
		return Objective == ETerritoryStateTaskObjective::EnterTerritory ? bInside
			: ObservedPawn.Get() == Pawn && bHasPresenceObservation && bWasInsideTarget && !bInside;
	}
	default:
		return false;
	}
}

void UTerritoryStateTask::BindTerritory(ATerritoryVolume* Territory)
{
	if (!Territory || Territory->GetTerritoryTag() != TargetTerritory) return;
	if (CachedTerritory.Get() == Territory) return;
	UnbindTerritory();
	CachedTerritory = Territory;
	Territory->OnTerritoryStateChangedDelegate.AddUniqueDynamic(
		this, &UTerritoryStateTask::HandleStateChanged);
	Territory->OnTerritoryAvailabilityChanged.AddUniqueDynamic(
		this, &UTerritoryStateTask::HandleAvailabilityChanged);
	Territory->OnAllGuardsDefeatedDelegate.AddUniqueDynamic(
		this, &UTerritoryStateTask::HandleAllDefendersDefeated);
	Territory->OnGarrisonChanged.AddUniqueDynamic(
		this, &UTerritoryStateTask::HandleGarrisonChanged);

}

void UTerritoryStateTask::UnbindTerritory()
{
	if (ATerritoryVolume* Territory = CachedTerritory.Get())
	{
		Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(
			this, &UTerritoryStateTask::HandleStateChanged);
		Territory->OnTerritoryAvailabilityChanged.RemoveDynamic(
			this, &UTerritoryStateTask::HandleAvailabilityChanged);
		Territory->OnAllGuardsDefeatedDelegate.RemoveDynamic(
			this, &UTerritoryStateTask::HandleAllDefendersDefeated);
		Territory->OnGarrisonChanged.RemoveDynamic(
			this, &UTerritoryStateTask::HandleGarrisonChanged);
	}
	CachedTerritory.Reset();
	ObservedPawn.Reset();
	bHasPresenceObservation = false;
	bWasInsideTarget = false;
	bHasObjectiveObservation = false;
	bObservedObjectiveSatisfied = false;
}

void UTerritoryStateTask::EvaluateCurrent(bool bInitialEvaluation)
{
	ATerritoryVolume* Territory = CachedTerritory.Get();
	if (!bIsActive || !Territory || CurrentProgress >= RequiredQuantity) return;
	if (IsPresenceObjective(Objective))
	{
		ObservePresence();
		return;
	}

	const bool bSatisfied = IsObjectiveSatisfiedBy(Territory);
	const bool bNewObservation = bInitialEvaluation || !bHasObjectiveObservation;
	const bool bWasSatisfied = bObservedObjectiveSatisfied;
	bHasObjectiveObservation = true;
	bObservedObjectiveSatisfied = bSatisfied;
	if (!bCompleteIfAlreadySatisfied && (bNewObservation || bWasSatisfied)) return;
	if (Objective == ETerritoryStateTaskObjective::ReachDesiredGarrison)
	{
		// Whole Place: progress is the owner's staffing target, which the management
		// screen raises. One floor: a floor has no staffing target of its own, so progress
		// is the guards physically standing there.
		const FTerritoryGarrisonSnapshot Garrison = Territory->GetGarrisonSnapshot();
		const FTerritoryFloorSnapshot* Floor = FindTargetFloor(Garrison.Floors);
		SetProgress(IsFloorFiltered()
			? (Floor ? Floor->ActiveGuards : 0)
			: Territory->GetDesiredGuardCount());
		return;
	}
	if (bSatisfied) CompleteTask();
}

void UTerritoryStateTask::HandleTerritoryRegistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)bWasUnregistered;
	if (!bIsActive || CurrentProgress >= RequiredQuantity
		|| !Territory || Territory->GetTerritoryTag() != TargetTerritory) return;
	BindTerritory(Territory);
	EvaluateCurrent(true);

	if (SpawnedMarker && CurrentProgress < RequiredQuantity)
	{
		SpawnedMarker->RemoveMarker();
		SpawnedMarker = nullptr;
		SpawnDefaultNavigationMarker();
	}
}

void UTerritoryStateTask::HandleTerritoryUnregistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)bWasUnregistered;
	if (Territory && Territory == CachedTerritory.Get())
	{
		UnbindTerritory();
		bWasInsideTarget = false;
	}
}

void UTerritoryStateTask::HandleStateChanged(
	ATerritoryVolume* Territory, ETerritoryState NewState)
{
	(void)NewState;
	if (Territory == CachedTerritory.Get()) EvaluateCurrent(false);
}

void UTerritoryStateTask::HandleAvailabilityChanged(
	ATerritoryVolume* Territory, ETerritoryAvailability NewAvailability)
{
	(void)NewAvailability;
	if (Territory == CachedTerritory.Get()) EvaluateCurrent(false);
}

void UTerritoryStateTask::HandleAllDefendersDefeated(
	ATerritoryVolume* Territory)
{
	// This delegate describes the whole Place. A floor-filtered task follows its own floor
	// and is evaluated from the per-floor snapshot in HandleGarrisonChanged instead.
	if (Territory == CachedTerritory.Get() && !IsFloorFiltered()
		&& Objective == ETerritoryStateTaskObjective::AllDefendersDefeated)
	{
		CompleteTask();
	}
}

void UTerritoryStateTask::HandleGarrisonChanged(
	ATerritoryVolume* Territory, FTerritoryGarrisonSnapshot Snapshot)
{
	if (Territory != CachedTerritory.Get()) return;
	if (Objective == ETerritoryStateTaskObjective::ReachDesiredGarrison)
	{
		// The delegate already hands us the snapshot by value, so it outlives the floor entry
		// read out of it and no second copy is needed here.
		const FTerritoryFloorSnapshot* Floor = FindTargetFloor(Snapshot.Floors);
		SetProgress(IsFloorFiltered()
			? (Floor ? Floor->ActiveGuards : 0)
			: Snapshot.DesiredGuards);
		return;
	}
	// Route a floor defeat through the normal evaluation so the task keeps the same
	// "must be seen to change" rule every other objective uses, rather than completing on
	// whatever the snapshot happened to say when the quest started.
	if (IsFloorFiltered()
		&& Objective == ETerritoryStateTaskObjective::AllDefendersDefeated)
	{
		EvaluateCurrent(false);
	}
}

ATerritoryVolume* UTerritoryStateTask::ResolveTerritory() const
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	return Registry && TargetTerritory.IsValid()
		? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
}

FText UTerritoryStateTask::ResolveTerritoryName() const
{
	if (const ATerritoryVolume* Territory = CachedTerritory.IsValid()
		? CachedTerritory.Get() : ResolveTerritory())
	{
		return Territory->GetTerritoryDisplayName();
	}

	FString Name = TargetTerritory.ToString();
	int32 Separator = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), Separator)) Name.RightChopInline(Separator + 1);
	return FText::FromString(FName::NameToDisplayString(Name, false));
}

FText UTerritoryStateTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	const FText Name = ResolveTerritoryName();
	switch (Objective)
	{
	case ETerritoryStateTaskObjective::BecomeAvailable:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Unlock", "Unlock {0}"), Name);
	case ETerritoryStateTaskObjective::BecomeLocked:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Lock", "Lock {0}"), Name);
	case ETerritoryStateTaskObjective::BecomeUnclaimed:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Unclaim", "Leave {0} unclaimed"), Name);
	case ETerritoryStateTaskObjective::BecomeContested:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Contest", "Contest {0}"), Name);
	case ETerritoryStateTaskObjective::BecomeClaimed:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Claim", "Secure {0}"), Name);
	case ETerritoryStateTaskObjective::AllDefendersDefeated:
		return IsFloorFiltered()
			? FText::Format(NSLOCTEXT("TerritoryTask", "ClearFloorDefenders",
				"Defeat the defenders on floor {0} of {1}"),
				FText::AsNumber(TargetFloor), Name)
			: FText::Format(NSLOCTEXT("TerritoryTask", "ClearDefenders", "Defeat the defenders at {0}"), Name);
	case ETerritoryStateTaskObjective::ReachDesiredGarrison:
		return IsFloorFiltered()
			? FText::Format(NSLOCTEXT("TerritoryTask", "AssignFloorGuards",
				"Assign {0} guards to floor {1} of {2}"),
				FMath::Max(1, RequiredQuantity), FText::AsNumber(TargetFloor), Name)
			: FText::Format(NSLOCTEXT("TerritoryTask", "AssignGuards", "Assign {0} guards to {1}"),
				FMath::Max(1, RequiredQuantity), Name);
	case ETerritoryStateTaskObjective::EnterTerritory:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Enter", "Enter {0}"), Name);
	case ETerritoryStateTaskObjective::LeaveTerritory:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Leave", "Leave {0}"), Name);
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FText UTerritoryStateTask::GetTaskProgressText_Implementation() const
{
	return Objective == ETerritoryStateTaskObjective::ReachDesiredGarrison
		? Super::GetTaskProgressText_Implementation() : FText::GetEmpty();
}

FVector UTerritoryStateTask::GetNavigationMarkerLocation_Implementation() const
{
	return ResolveTerritory() ? FVector::ZeroVector
		: MarkerSettings.ActorFallbackLocation;
}

AActor* UTerritoryStateTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return ResolveTerritory();
}
