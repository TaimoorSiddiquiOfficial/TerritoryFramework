#include "Tales/TerritoryNarrativeDataTask.h"

#include "Engine/World.h"
#include "Tales/NarrativeDataTask.h"
#include "Tales/Quest.h"
#include "Tales/TalesComponent.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "TerritoryNarrativeDataTask"

UTerritoryNarrativeDataTask::UTerritoryNarrativeDataTask()
{
	TickInterval = 0.f;
	MarkerSettings.bAddNavigationMarker = false;
}

void UTerritoryNarrativeDataTask::BeginTask()
{
	Unbind();
	TickInterval = 0.f;
	Super::BeginTask();
	if (!bIsActive || !IsValid(OwningComp) || !OwningComp->HasAuthority()
		|| !IsValid(DataTask) || DataTask->TaskName.IsEmpty() || Argument.TrimStartAndEnd().IsEmpty()) return;
	BoundTales = OwningComp;
	bListening = true;
	StartingCount = OwningComp->GetNumberOfTimesTaskWasCompleted(DataTask, Argument);
	BaselineCount = bCountPreviousCompletions ? 0 : StartingCount;
	BaselineRecordKey = MakeBaselineRecordKey();
	if (!bCountPreviousCompletions)
	{
		// Store the cursor in Native's existing SaveGame ledger. It is a starting
		// count, not a second progress authority. This also covers a save taken after
		// the producer commits but before our deferred progress callback runs.
		if (!OwningComp->bIsLoading)
			OwningComp->MasterTaskList.Add(BaselineRecordKey, static_cast<int32>(StartingCount));
		else if (const int32* SavedStart = OwningComp->MasterTaskList.Find(BaselineRecordKey))
			BaselineCount = *SavedStart;
		else bAwaitingSavedProgress = true; // Bounded migration from a legacy task.
	}
	OwningComp->OnNarrativeDataTaskCompleted.AddUniqueDynamic(this,
		&UTerritoryNarrativeDataTask::OnDataTaskRecorded);
	if (OwningComp->bIsLoading) QueueRefresh();
	else RefreshFromRecords();
}

void UTerritoryNarrativeDataTask::EndTask()
{
	Unbind();
	Super::EndTask();
}

void UTerritoryNarrativeDataTask::BeginDestroy()
{
	Unbind();
	Super::BeginDestroy();
}

void UTerritoryNarrativeDataTask::Unbind()
{
	bListening = false;
	bAwaitingSavedProgress = false;
	if (UTalesComponent* Tales = BoundTales.Get())
		Tales->OnNarrativeDataTaskCompleted.RemoveDynamic(this,
			&UTerritoryNarrativeDataTask::OnDataTaskRecorded);
	BoundTales.Reset();
	// Native ends the old quest instances during load after restoring the ledger.
	// Do not erase a saved cursor here. A fresh BeginTask overwrites the same key.
	if (UWorld* World = GetWorld(); IsValid(World) && !World->bIsTearingDown)
		World->GetTimerManager().ClearTimer(RefreshTimer);
}

void UTerritoryNarrativeDataTask::OnDataTaskRecorded(
	const UNarrativeDataTask* RecordedTask, const FString& RecordedArgument)
{
	if (!bListening || !IsValid(RecordedTask) || !IsValid(DataTask)
		|| RecordedTask->MakeTaskString(RecordedArgument) != DataTask->MakeTaskString(Argument)) return;
	// Native broadcasts before writing MasterTaskList and does not include quantity.
	// Read its verified final count after that stack unwinds; never guess +1.
	QueueRefresh();
}

void UTerritoryNarrativeDataTask::QueueRefresh()
{
	UWorld* World = GetWorld();
	if (!bListening || !World || World->bIsTearingDown || World->GetNetMode() == NM_Client) return;
	if (!World->GetTimerManager().IsTimerActive(RefreshTimer))
		RefreshTimer = World->GetTimerManager().SetTimerForNextTick(this,
			&UTerritoryNarrativeDataTask::RefreshFromRecords);
}

void UTerritoryNarrativeDataTask::RefreshFromRecords()
{
	// Clear a queued callback also when a test or Blueprint lifecycle invokes this
	// path synchronously. Reentrant completion may end/restart the same task.
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
	UTalesComponent* Tales = BoundTales.Get();
	if (!bListening || !bIsActive || !IsValid(Tales) || Tales != OwningComp
		|| !Tales->HasAuthority() || !IsValid(DataTask)) return;
	if (Tales->bIsLoading) { QueueRefresh(); return; }
	if (bAwaitingSavedProgress)
	{
		BaselineCount = StartingCount - CurrentProgress;
		Tales->MasterTaskList.Add(BaselineRecordKey,
			static_cast<int32>(FMath::Clamp<int64>(BaselineCount, MIN_int32, MAX_int32)));
		bAwaitingSavedProgress = false;
	}
	const int64 Count = Tales->GetNumberOfTimesTaskWasCompleted(DataTask, Argument);
	const int32 Progress = static_cast<int32>(FMath::Clamp<int64>(
		Count - BaselineCount, 0, FMath::Max(1, RequiredQuantity)));
	SetProgress(Progress);
}

FString UTerritoryNarrativeDataTask::MakeBaselineRecordKey() const
{
	// Native quest templates preserve branch IDs and instanced task names when
	// duplicated. Never use the live quest instance name or an actor pointer.
	const UQuestBranch* Branch = GetOwningBranch();
	FString Key = FString::Printf(TEXT("__territory_datatask_start|%s|%s|%s|%s|%s"),
		OwningQuest ? *OwningQuest->GetClass()->GetPathName() : TEXT("standalone"),
		Branch ? *Branch->GetID().ToString() : TEXT("standalone"), *GetName(),
		DataTask ? *DataTask->GetPathName() : TEXT("missing"),
		DataTask ? *DataTask->MakeTaskString(Argument) : TEXT("missing"));
	return Key;
}

FText UTerritoryNarrativeDataTask::GetTaskDescription_Implementation() const
{
	return DescriptionOverride.IsEmpty()
		? FText::Format(LOCTEXT("Description", "Complete {0}: {1}"),
			FText::FromString(DataTask ? DataTask->TaskName : TEXT("Narrative action")),
			FText::FromString(Argument)) : DescriptionOverride;
}

FText UTerritoryNarrativeDataTask::GetTaskNodeDescription_Implementation() const
{
	return GetTaskDescription_Implementation();
}

#undef LOCTEXT_NAMESPACE
