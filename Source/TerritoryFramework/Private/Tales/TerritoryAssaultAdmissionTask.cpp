#include "Tales/TerritoryAssaultAdmissionTask.h"

#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryStoryEvents.h"
#include "TimerManager.h"

UTerritoryAssaultAdmissionTask::UTerritoryAssaultAdmissionTask()
{
	TickInterval = 1.f;
}

bool UTerritoryAssaultAdmissionTask::HasAdmittedRecord() const
{
	UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr;
	const auto* Counter = World ? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
	if (!Counter || !Request || Request->ScenarioID.IsNone()) return false;
	const auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	const ATerritoryVolume* Territory = Registry ? Registry->GetTerritoryByTag(Request->TargetTerritory) : nullptr;
	const TArray<FTerritoryAssaultRecord> Records = Territory
		? Counter->GetAssaultsForTerritoryActor(Territory)
		: Counter->GetAssaultsForTerritory(Request->TargetTerritory);
	return Records.ContainsByPredicate([this](const FTerritoryAssaultRecord& Record)
	{
		return Record.StoryScenarioID == Request->ScenarioID
			&& Record.LaunchMode == Request->LaunchMode
			&& (Request->bChooseBestEligibleAttacker || Record.AttackingFaction == Request->AttackingFaction);
	});
}

void UTerritoryAssaultAdmissionTask::BeginTask()
{
	Super::BeginTask();
	if (!bIsActive || CurrentProgress >= RequiredQuantity || !OwningComp) return;
	if (UWorld* World = OwningComp->GetWorld())
		if (auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
			Counter->OnAssaultChanged.AddUniqueDynamic(this, &UTerritoryAssaultAdmissionTask::HandleAssaultChanged);
}

void UTerritoryAssaultAdmissionTask::EndTask()
{
	if (UWorld* World = OwningComp ? OwningComp->GetWorld() : nullptr)
		if (auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
			Counter->OnAssaultChanged.RemoveDynamic(this, &UTerritoryAssaultAdmissionTask::HandleAssaultChanged);
	Super::EndTask();
}

void UTerritoryAssaultAdmissionTask::HandleAssaultChanged(const FTerritoryAssaultRecord& Record)
{
	if (Request && Record.TargetTerritory == Request->TargetTerritory
		&& Record.StoryScenarioID == Request->ScenarioID)
		TickTask_Implementation();
}

void UTerritoryAssaultAdmissionTask::TickTask_Implementation()
{
	if (bAttemptingAdmission || !bIsActive || !OwningComp || !OwningComp->HasAuthority()
		|| OwningComp->bIsLoading || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
	TGuardValue<bool> AdmissionGuard(bAttemptingAdmission, true);
	if (CurrentProgress >= RequiredQuantity)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_TickTask);
		return;
	}
	if (!Request || !Request->TargetTerritory.IsValid() || Request->ScenarioID.IsNone())
	{
		LastAdmissionFailure = NSLOCTEXT("TerritoryTask", "InvalidAdmission", "Configure a Wave request with a target Territory and stable Scenario ID.");
		return;
	}
	if (!HasAdmittedRecord())
	{
		// Native quest progress persists the pending intent; CounterAttack remains
		// the sole scheduler. Resolve current context after possession/load changes.
		Request->TryScheduleWave(OwningComp->GetOwningPawn(),
			OwningComp->GetOwningController(), OwningComp, LastAdmissionFailure);
	}
	// Re-read the authority after callbacks, including an immediate cancellation.
	// A recorded failed attempt requires an explicit story retry, never a reroll.
	if (bIsActive && !OwningComp->bIsLoading && HasAdmittedRecord())
	{
		LastAdmissionFailure = FText::GetEmpty();
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_TickTask);
		CompleteTask();
	}
}

FText UTerritoryAssaultAdmissionTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	return NSLOCTEXT("TerritoryTask", "AwaitAdmission", "Wait for the counterattack to be scheduled");
}
