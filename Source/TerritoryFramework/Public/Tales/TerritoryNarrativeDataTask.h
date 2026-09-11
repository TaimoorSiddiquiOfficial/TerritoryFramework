#pragma once

#include "CoreMinimal.h"
#include "Tales/QuestTask.h"
#include "TerritoryNarrativeDataTask.generated.h"

class UNarrativeDataTask;

/** Watches Narrative's saved data-task records. Narrative still owns quest progress, saving and replication. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Wait For Narrative Data Task",
		ToolTip="Wait for a named Narrative record, such as overhearing a conversation. Uses the full recorded quantity and keeps other quest listeners connected when this task ends."))
class TERRITORYFRAMEWORK_API UTerritoryNarrativeDataTask : public UNarrativeTask
{
	GENERATED_BODY()
public:
	UTerritoryNarrativeDataTask();

	/** Data Task asset passed to Complete Narrative Data Task on this quest's Tales component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Narrative Data Task")
	TObjectPtr<UNarrativeDataTask> DataTask;

	/** Argument passed with the record. For example: BlacksmithPatrolConversation. Narrative matches without spaces or letter case. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Narrative Data Task")
	FString Argument;

	/** Include records from before this task started. Disable when the player must perform the action again for this quest step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Narrative Data Task")
	bool bCountPreviousCompletions = false;

	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskNodeDescription_Implementation() const override;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void BeginDestroy() override;

private:
	friend class FTFNarrativeDataTaskRecords;
	TWeakObjectPtr<UTalesComponent> BoundTales;
	FTimerHandle RefreshTimer;
	int64 StartingCount = 0;
	int64 BaselineCount = 0;
	FString BaselineRecordKey;
	bool bAwaitingSavedProgress = false;
	bool bListening = false;
	void Unbind();
	void QueueRefresh();
	void RefreshFromRecords();
	FString MakeBaselineRecordKey() const;
	UFUNCTION()
	void OnDataTaskRecorded(const UNarrativeDataTask* RecordedTask, const FString& RecordedArgument);
};
