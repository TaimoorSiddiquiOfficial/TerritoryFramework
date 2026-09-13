#include "Tales/TerritoryDisguiseTask.h"

#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "GameFramework/Pawn.h"

void UTerritoryDisguiseTask::BeginTask()
{
	// Narrative may enter the same branch again. BeginTask immediately invokes the
	// first tick through UNarrativeTask, so stale inside-state must be cleared first.
	bWasInsideTarget = false;
	ObservedPawn.Reset();
	ObservedTerritory.Reset();
	// Event objectives also need a bounded retry when the player arrives later.
	TickInterval = 0.25f;
	Super::BeginTask();
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr;
	UTerritoryDisguiseSubsystem* Disguises = World
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	if (!Disguises) return;
	Disguises->OnDisguiseChanged.AddUniqueDynamic(
		this, &UTerritoryDisguiseTask::HandleDisguiseChanged);
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
	ObservedPawn.Reset();
	ObservedTerritory.Reset();
	Super::EndTask();
}

void UTerritoryDisguiseTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
	ATerritoryVolume* Territory = ResolveTerritory();
	if (ObservedPawn.Get() != Pawn || ObservedTerritory.Get() != Territory || !Pawn || !Territory)
	{
		bWasInsideTarget = false;
		ObservedPawn = Pawn;
		ObservedTerritory = Territory;
	}
	UWorld* World = GetWorld();
	const UTerritoryDisguiseSubsystem* Disguises = World
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	FTerritoryDisguiseSnapshot Snapshot;
	if (!Pawn || !Disguises
		|| !Disguises->GetDisguiseSnapshot(Pawn, Snapshot)
		|| !MatchesFaction(Snapshot, FGameplayTag()))
	{
		bWasInsideTarget = false;
		return;
	}
	if (Objective == ETerritoryDisguiseTaskObjective::EquipDisguise)
	{
		CompleteTask();
		return;
	}
	if (!Territory) return;
	const bool bInside = Territory->ContainsPoint(Pawn->GetActorLocation());
	FText Reason;
	const bool bAccepted = Disguises->IsDisguiseAccepted(
		Pawn, Territory, Faction, Reason);
	const bool bEntered = bInside && bAccepted;
	const bool bLeft = bWasInsideTarget && !bInside && bAccepted;
	// Losing cover or the observed subject ends this inside-to-outside evidence.
	// Restore while outside is not an undetected exit from an earlier visit.
	bWasInsideTarget = bInside && bAccepted;
	if ((Objective == ETerritoryDisguiseTaskObjective::EnterTerritoryAccepted && bEntered)
		|| (Objective == ETerritoryDisguiseTaskObjective::ExitTerritoryUndetected && bLeft)) CompleteTask();
}

void UTerritoryDisguiseTask::HandleDisguiseChanged(AActor* Target,
	ETerritoryDisguiseChange Change, FGameplayTag ObserverFaction,
	ATerritoryVolume* Territory, const FTerritoryDisguiseSnapshot& Snapshot)
{
	if (!bIsActive || !Target
		|| Target != TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController)
		|| CurrentProgress >= RequiredQuantity
		|| !MatchesFaction(Snapshot, ObserverFaction)
		|| TargetTerritory.IsValid() && Territory
			&& Territory->GetTerritoryTag() != TargetTerritory)
	{
		return;
	}
	if (Change == ETerritoryDisguiseChange::Removed || Change == ETerritoryDisguiseChange::Compromised)
	{
		bWasInsideTarget = false;
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
