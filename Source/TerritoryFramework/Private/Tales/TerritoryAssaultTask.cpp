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
	return Assault.TargetTerritory == TargetTerritory
		&& (!AttackingFaction.IsValid()
			|| Assault.AttackingFaction == AttackingFaction)
		&& (ScenarioID.IsNone() || Assault.StoryScenarioID == ScenarioID);
}

void UTerritoryAssaultTask::EvaluateRecord(const FTerritoryAssaultRecord& Assault)
{
	if (!Matches(Assault) || IsComplete()) return;
	switch (Objective)
	{
	case ETerritoryAssaultTaskObjective::RepelAttack:
		if (Assault.State == ETerritoryAssaultState::Defeated
			&& Assault.Resolution ==
				ETerritoryAssaultResolution::AllAttackersRemoved)
		{
			CompleteTask();
		}
		break;
	case ETerritoryAssaultTaskObjective::TerritoryTaken:
		if (Assault.State == ETerritoryAssaultState::Succeeded
			&& Assault.Resolution == ETerritoryAssaultResolution::CaptureCompleted)
		{
			CompleteTask();
		}
		break;
	case ETerritoryAssaultTaskObjective::KillAttackers:
		SetProgress(FMath::Max(CurrentProgress, Assault.KilledForce));
		break;
	case ETerritoryAssaultTaskObjective::AssaultActivated:
		if (Assault.State == ETerritoryAssaultState::Active
			|| Assault.State == ETerritoryAssaultState::RecaptureCountdown
			|| (Assault.IsTerminal() && Assault.ActivatedGameTime > 0.0))
		{
			CompleteTask();
		}
		break;
	case ETerritoryAssaultTaskObjective::FinalFightStarted:
		if (Assault.bStoryTargetAbandonedVehicle) CompleteTask();
		break;
	case ETerritoryAssaultTaskObjective::TargetEscaped:
		if (Assault.Resolution == ETerritoryAssaultResolution::TargetEscaped
			|| Assault.Resolution == ETerritoryAssaultResolution::ChaseDistanceLost)
		{
			CompleteTask();
		}
		break;
	default:
		break;
	}
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
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FText UTerritoryAssaultTask::GetTaskProgressText_Implementation() const
{
	return Objective == ETerritoryAssaultTaskObjective::KillAttackers
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
