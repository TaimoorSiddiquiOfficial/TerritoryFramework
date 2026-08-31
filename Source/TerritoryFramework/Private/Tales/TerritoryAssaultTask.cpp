#include "Tales/TerritoryAssaultTask.h"

#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"

void UTerritoryAssaultTask::BeginTask()
{
	Super::BeginTask();
	UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr;
	UTerritoryCounterAttackSubsystem* Counter = World
		? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
	if (!Counter || !TargetTerritory.IsValid()) return;
	Counter->OnAssaultChanged.AddUniqueDynamic(
		this, &UTerritoryAssaultTask::HandleAssaultChanged);
	for (const FTerritoryAssaultRecord& Assault :
		Counter->GetAssaultsForTerritory(TargetTerritory))
	{
		EvaluateRecord(Assault);
		if (IsComplete()) break;
	}
}

void UTerritoryAssaultTask::EndTask()
{
	if (UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr)
	{
		if (UTerritoryCounterAttackSubsystem* Counter =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			Counter->OnAssaultChanged.RemoveDynamic(
				this, &UTerritoryAssaultTask::HandleAssaultChanged);
		}
	}
	Super::EndTask();
}

void UTerritoryAssaultTask::HandleAssaultChanged(
	const FTerritoryAssaultRecord& Assault)
{
	EvaluateRecord(Assault);
}

bool UTerritoryAssaultTask::Matches(const FTerritoryAssaultRecord& Assault) const
{
	return TargetTerritory.IsValid()
		&& Assault.TargetTerritory == TargetTerritory
		&& (!AttackingFaction.IsValid()
			|| Assault.AttackingFaction == AttackingFaction)
		&& (ScenarioID.IsNone() || Assault.StoryScenarioID == ScenarioID);
}

void UTerritoryAssaultTask::EvaluateRecord(const FTerritoryAssaultRecord& Assault)
{
	if (!Matches(Assault) || IsComplete()) return;
	const int32 RecordProgress = GetObjectiveProgressFromRecord(Assault);
	if (RecordProgress > 0)
	{
		SetProgress(FMath::Max(CurrentProgress, RecordProgress));
	}
	else if (IsObjectiveSatisfiedByRecord(Assault)) CompleteTask();
}

bool UTerritoryAssaultTask::IsObjectiveSatisfiedByRecord(
	const FTerritoryAssaultRecord& Assault) const
{
	if (!Matches(Assault)) return false;
	const bool bStoryPursuit =
		Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit;
	switch (Objective)
	{
	case ETerritoryAssaultTaskObjective::RepelAttack:
		return Assault.State == ETerritoryAssaultState::Defeated
			&& Assault.Resolution ==
				ETerritoryAssaultResolution::AllAttackersRemoved;
	case ETerritoryAssaultTaskObjective::TerritoryTaken:
		return Assault.State == ETerritoryAssaultState::Succeeded
			&& Assault.Resolution == ETerritoryAssaultResolution::CaptureCompleted;
	case ETerritoryAssaultTaskObjective::AssaultActivated:
		return Assault.State == ETerritoryAssaultState::Active
			|| Assault.State == ETerritoryAssaultState::RecaptureCountdown
			|| (Assault.IsTerminal() && Assault.ActivatedGameTime > 0.0);
	case ETerritoryAssaultTaskObjective::FinalFightStarted:
		return bStoryPursuit && Assault.bStoryTargetAbandonedVehicle;
	case ETerritoryAssaultTaskObjective::TargetEscaped:
		return bStoryPursuit
			&& (Assault.Resolution == ETerritoryAssaultResolution::TargetEscaped
				|| Assault.Resolution == ETerritoryAssaultResolution::ChaseDistanceLost);
	case ETerritoryAssaultTaskObjective::WarningIssued:
		return Assault.bNotificationSent;
	case ETerritoryAssaultTaskObjective::RecaptureCountdownStarted:
		return Assault.State == ETerritoryAssaultState::RecaptureCountdown;
	case ETerritoryAssaultTaskObjective::StoryPursuitStarted:
		return bStoryPursuit && (Assault.State == ETerritoryAssaultState::Active
			|| Assault.State == ETerritoryAssaultState::RecaptureCountdown
			|| (Assault.IsTerminal() && Assault.ActivatedGameTime > 0.0));
	case ETerritoryAssaultTaskObjective::StoryBossDefeated:
		return bStoryPursuit
			&& Assault.State == ETerritoryAssaultState::Defeated
			&& Assault.Resolution == ETerritoryAssaultResolution::AllAttackersRemoved
			&& Assault.KilledForce >= FMath::Max(1, RequiredQuantity);
	case ETerritoryAssaultTaskObjective::StoryEncounterResolved:
		return bStoryPursuit && Assault.IsTerminal();
	case ETerritoryAssaultTaskObjective::StoryTargetReachedExit:
		return bStoryPursuit
			&& Assault.Resolution == ETerritoryAssaultResolution::TargetEscaped;
	case ETerritoryAssaultTaskObjective::StoryChaseDistanceLost:
		return bStoryPursuit
			&& Assault.Resolution == ETerritoryAssaultResolution::ChaseDistanceLost;
	case ETerritoryAssaultTaskObjective::AssaultCancelled:
		return Assault.State == ETerritoryAssaultState::Cancelled;
	default:
		return false;
	}
}

int32 UTerritoryAssaultTask::GetObjectiveProgressFromRecord(
	const FTerritoryAssaultRecord& Assault) const
{
	if (!Matches(Assault)) return 0;
	if (Objective == ETerritoryAssaultTaskObjective::KillAttackers)
	{
		return FMath::Max(0, Assault.KilledForce);
	}
	if (Objective == ETerritoryAssaultTaskObjective::WithdrawnAttackers)
	{
		return FMath::Max(0, Assault.WithdrawnForce);
	}
	return 0;
}

ATerritoryVolume* UTerritoryAssaultTask::ResolveTerritory() const
{
	const UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	return Registry ? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
}

FText UTerritoryAssaultTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	const ATerritoryVolume* Territory = ResolveTerritory();
	const FText Name = Territory ? Territory->GetTerritoryDisplayName()
		: FText::FromName(TargetTerritory.GetTagName());
	switch (Objective)
	{
	case ETerritoryAssaultTaskObjective::RepelAttack:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Repel", "Repel the attack on {0}"), Name);
	case ETerritoryAssaultTaskObjective::TerritoryTaken:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Taken", "Help the faction take {0}"), Name);
	case ETerritoryAssaultTaskObjective::KillAttackers:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Kill", "Defeat reinforcements attacking {0}"), Name);
	case ETerritoryAssaultTaskObjective::AssaultActivated:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Arrive", "Wait for reinforcements at {0}"), Name);
	case ETerritoryAssaultTaskObjective::FinalFightStarted:
		return NSLOCTEXT("TerritoryTask", "FinalFight", "Force the target into a final fight");
	case ETerritoryAssaultTaskObjective::TargetEscaped:
		return NSLOCTEXT("TerritoryTask", "Escaped", "Let the chase target escape");
	case ETerritoryAssaultTaskObjective::WarningIssued:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Warning", "Receive the attack warning for {0}"), Name);
	case ETerritoryAssaultTaskObjective::RecaptureCountdownStarted:
		return FText::Format(NSLOCTEXT("TerritoryTask", "RecaptureCountdown", "Stop the takeover countdown at {0}"), Name);
	case ETerritoryAssaultTaskObjective::StoryPursuitStarted:
		return NSLOCTEXT("TerritoryTask", "PursuitStarted", "Begin the boss fight or chase");
	case ETerritoryAssaultTaskObjective::StoryBossDefeated:
		return NSLOCTEXT("TerritoryTask", "BossDefeated", "Defeat the story boss");
	case ETerritoryAssaultTaskObjective::StoryEncounterResolved:
		return NSLOCTEXT("TerritoryTask", "StoryResolved", "Resolve the boss fight or chase");
	case ETerritoryAssaultTaskObjective::StoryTargetReachedExit:
		return NSLOCTEXT("TerritoryTask", "ReachedExit", "Let the chase target reach the road exit");
	case ETerritoryAssaultTaskObjective::StoryChaseDistanceLost:
		return NSLOCTEXT("TerritoryTask", "DistanceLost", "Lose the chase target");
	case ETerritoryAssaultTaskObjective::WithdrawnAttackers:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Withdraw", "Force attackers to withdraw from {0}"), Name);
	case ETerritoryAssaultTaskObjective::AssaultCancelled:
		return FText::Format(NSLOCTEXT("TerritoryTask", "Cancelled", "Cancel the attack on {0}"), Name);
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FText UTerritoryAssaultTask::GetTaskProgressText_Implementation() const
{
	return Objective == ETerritoryAssaultTaskObjective::KillAttackers
		|| Objective == ETerritoryAssaultTaskObjective::WithdrawnAttackers
		? Super::GetTaskProgressText_Implementation() : FText::GetEmpty();
}

FVector UTerritoryAssaultTask::GetNavigationMarkerLocation_Implementation() const
{
	return ResolveTerritory() ? FVector::ZeroVector
		: MarkerSettings.ActorFallbackLocation;
}

AActor* UTerritoryAssaultTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return ResolveTerritory();
}
