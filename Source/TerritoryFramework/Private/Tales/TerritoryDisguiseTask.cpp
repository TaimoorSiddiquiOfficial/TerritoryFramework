#include "Tales/TerritoryDisguiseTask.h"

#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"

void UTerritoryDisguiseTask::BeginTask()
{
	// Narrative may enter the same branch again. BeginTask immediately invokes the
	// first tick through UNarrativeTask, so stale inside-state must be cleared first.
	bWasInsideTarget = false;
	if (Objective == ETerritoryDisguiseTaskObjective::EnterTerritoryAccepted
		|| Objective == ETerritoryDisguiseTaskObjective::ExitTerritoryUndetected)
	{
		TickInterval = 0.25f;
	}
	Super::BeginTask();
	UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr;
	UTerritoryDisguiseSubsystem* Disguises = World
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	if (!Disguises || !OwningPawn) return;
	Disguises->OnDisguiseChanged.AddUniqueDynamic(
		this, &UTerritoryDisguiseTask::HandleDisguiseChanged);

	FTerritoryDisguiseSnapshot Snapshot;
	if (Disguises->GetDisguiseSnapshot(OwningPawn, Snapshot)
		&& Objective == ETerritoryDisguiseTaskObjective::EquipDisguise
		&& MatchesFaction(Snapshot, FGameplayTag()))
	{
		CompleteTask();
		return;
	}
	ATerritoryVolume* Territory = ResolveTerritory();
	bWasInsideTarget = Territory && Territory->ContainsPoint(
		OwningPawn->GetActorLocation());
}

void UTerritoryDisguiseTask::EndTask()
{
	if (UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr)
	{
		if (UTerritoryDisguiseSubsystem* Disguises =
			World->GetSubsystem<UTerritoryDisguiseSubsystem>())
		{
			Disguises->OnDisguiseChanged.RemoveDynamic(
				this, &UTerritoryDisguiseTask::HandleDisguiseChanged);
		}
	}
	bWasInsideTarget = false;
	Super::EndTask();
}

void UTerritoryDisguiseTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (IsComplete() || !OwningPawn) return;
	ATerritoryVolume* Territory = ResolveTerritory();
	UWorld* World = OwningPawn->GetWorld();
	const UTerritoryDisguiseSubsystem* Disguises = World
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	FTerritoryDisguiseSnapshot Snapshot;
	if (!Territory || !Disguises
		|| !Disguises->GetDisguiseSnapshot(OwningPawn, Snapshot)
		|| !MatchesFaction(Snapshot, FGameplayTag()))
	{
		return;
	}
	const bool bInside = Territory->ContainsPoint(OwningPawn->GetActorLocation());
	FText Reason;
	const bool bAccepted = Disguises->IsDisguiseAccepted(
		OwningPawn, Territory, Faction, Reason);
	if (Objective == ETerritoryDisguiseTaskObjective::EnterTerritoryAccepted
		&& bInside && bAccepted)
	{
		CompleteTask();
	}
	else if (Objective == ETerritoryDisguiseTaskObjective::ExitTerritoryUndetected
		&& bWasInsideTarget && !bInside && bAccepted)
	{
		CompleteTask();
	}
	bWasInsideTarget |= bInside;
}

void UTerritoryDisguiseTask::HandleDisguiseChanged(AActor* Target,
	ETerritoryDisguiseChange Change, FGameplayTag ObserverFaction,
	ATerritoryVolume* Territory, const FTerritoryDisguiseSnapshot& Snapshot)
{
	if (Target != OwningPawn || IsComplete()
		|| !MatchesFaction(Snapshot, ObserverFaction)
		|| TargetTerritory.IsValid() && Territory
			&& Territory->GetTerritoryTag() != TargetTerritory)
	{
		return;
	}
	if (Objective == ETerritoryDisguiseTaskObjective::EquipDisguise
		&& Change == ETerritoryDisguiseChange::Activated
		|| Objective == ETerritoryDisguiseTaskObjective::PassIdentityCheck
			&& Change == ETerritoryDisguiseChange::IdentityCheckPassed
		|| Objective == ETerritoryDisguiseTaskObjective::CoverCompromised
			&& Change == ETerritoryDisguiseChange::Compromised
		|| Objective == ETerritoryDisguiseTaskObjective::RestoreCover
			&& Change == ETerritoryDisguiseChange::Restored
		|| Objective == ETerritoryDisguiseTaskObjective::RemoveDisguise
			&& Change == ETerritoryDisguiseChange::Removed)
	{
		CompleteTask();
	}
}

ATerritoryVolume* UTerritoryDisguiseTask::ResolveTerritory() const
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	return Registry && TargetTerritory.IsValid()
		? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
}

bool UTerritoryDisguiseTask::MatchesFaction(
	const FTerritoryDisguiseSnapshot& Snapshot,
	FGameplayTag ObserverFaction) const
{
	return !Faction.IsValid() || Snapshot.PerceivedFaction == Faction
		|| ObserverFaction == Faction;
}

FText UTerritoryDisguiseTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	switch (Objective)
	{
	case ETerritoryDisguiseTaskObjective::EquipDisguise:
		return NSLOCTEXT("TerritoryTask", "EquipDisguise", "Equip the disguise");
	case ETerritoryDisguiseTaskObjective::EnterTerritoryAccepted:
		return NSLOCTEXT("TerritoryTask", "EnterDisguised", "Enter the target area in disguise");
	case ETerritoryDisguiseTaskObjective::PassIdentityCheck:
		return NSLOCTEXT("TerritoryTask", "PassIdentity", "Pass the identity checkpoint");
	case ETerritoryDisguiseTaskObjective::CoverCompromised:
		return NSLOCTEXT("TerritoryTask", "CoverCompromised", "Allow your cover to be exposed");
	case ETerritoryDisguiseTaskObjective::RestoreCover:
		return NSLOCTEXT("TerritoryTask", "RestoreCover", "Restore your cover identity");
	case ETerritoryDisguiseTaskObjective::RemoveDisguise:
		return NSLOCTEXT("TerritoryTask", "RemoveDisguise", "Remove the disguise");
	case ETerritoryDisguiseTaskObjective::ExitTerritoryUndetected:
		return NSLOCTEXT("TerritoryTask", "ExitUndetected", "Leave the area without losing your cover");
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FVector UTerritoryDisguiseTask::GetNavigationMarkerLocation_Implementation() const
{
	return ResolveTerritory() ? FVector::ZeroVector
		: MarkerSettings.ActorFallbackLocation;
}

AActor* UTerritoryDisguiseTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return ResolveTerritory();
}
