#include "Tales/TerritoryStateTask.h"

#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryVolume.h"
#include "Navigation/MapMarker.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"

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
	if (IsPresenceObjective(Objective)
		|| Objective == ETerritoryStateTaskObjective::BecomeAvailable)
	{
		// Presence has no Territory delegate. Effective availability also depends on
		// ancestors, so a low-frequency check catches an ancestor unlocking.
		TickInterval = 0.25f;
	}

	Super::BeginTask();
	if (!OwningComp || CurrentProgress >= RequiredQuantity
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
	if (CurrentProgress >= RequiredQuantity) return;

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
	if (!IsPresenceObjective(Objective) || !OwningPawn) return;

	const bool bInside = Territory->ContainsPoint(OwningPawn->GetActorLocation());
	if (Objective == ETerritoryStateTaskObjective::EnterTerritory && bInside)
	{
		CompleteTask();
	}
	else if (Objective == ETerritoryStateTaskObjective::LeaveTerritory
		&& bWasInsideTarget && !bInside)
	{
		CompleteTask();
	}
	bWasInsideTarget = bInside;
}

bool UTerritoryStateTask::IsObjectiveSatisfiedBy(
	const ATerritoryVolume* Territory) const
{
	if (!Territory || Territory->GetTerritoryTag() != TargetTerritory)
	{
		return false;
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
			&& (Territory->GetDesiredGuardCount() > 0
				|| Territory->GetConfiguredGuardCount() > 0);
	case ETerritoryStateTaskObjective::ReachDesiredGarrison:
		return Territory->GetDesiredGuardCount() >= FMath::Max(1, RequiredQuantity);
	case ETerritoryStateTaskObjective::EnterTerritory:
		return OwningPawn
			&& Territory->ContainsPoint(OwningPawn->GetActorLocation());
	case ETerritoryStateTaskObjective::LeaveTerritory:
		return OwningPawn && bWasInsideTarget
			&& !Territory->ContainsPoint(OwningPawn->GetActorLocation());
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

	if (IsPresenceObjective(Objective) && OwningPawn)
	{
		// Treat a World Partition replacement as a fresh observation. Streaming
		// must never fake an inside-to-outside quest transition.
		bWasInsideTarget = Territory->ContainsPoint(
			OwningPawn->GetActorLocation());
	}
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
}

void UTerritoryStateTask::EvaluateCurrent(bool bInitialEvaluation)
{
	ATerritoryVolume* Territory = CachedTerritory.Get();
	if (!Territory || CurrentProgress >= RequiredQuantity) return;

	if (bInitialEvaluation && !bCompleteIfAlreadySatisfied) return;
	if (Objective == ETerritoryStateTaskObjective::ReachDesiredGarrison)
	{
		SetProgress(Territory->GetDesiredGuardCount());
		return;
	}
	if (Objective == ETerritoryStateTaskObjective::LeaveTerritory) return;
	if (IsObjectiveSatisfiedBy(Territory)) CompleteTask();
}

void UTerritoryStateTask::HandleTerritoryRegistered(
	ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)bWasUnregistered;
	if (!Territory || Territory->GetTerritoryTag() != TargetTerritory) return;
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
	if (Territory == CachedTerritory.Get()
		&& Objective == ETerritoryStateTaskObjective::AllDefendersDefeated)
	{
		CompleteTask();
	}
}

void UTerritoryStateTask::HandleGarrisonChanged(
	ATerritoryVolume* Territory, FTerritoryGarrisonSnapshot Snapshot)
{
	if (Territory == CachedTerritory.Get()
		&& Objective == ETerritoryStateTaskObjective::ReachDesiredGarrison)
	{
		SetProgress(Snapshot.DesiredGuards);
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
		return FText::Format(NSLOCTEXT("TerritoryTask", "ClearDefenders", "Defeat the defenders at {0}"), Name);
	case ETerritoryStateTaskObjective::ReachDesiredGarrison:
		return FText::Format(NSLOCTEXT("TerritoryTask", "AssignGuards", "Assign {0} guards to {1}"),
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
