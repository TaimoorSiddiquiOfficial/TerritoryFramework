#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Tales/QuestTask.h"
#include "TerritoryAssaultAdmissionTask.generated.h"

class UTerritoryScheduleEnemyWaveEvent;

/**
 * Persistent pre-admission intent owned by Native quest progress. Completes when
 * the existing scheduler has a matching durable record, never when combat wins.
 * Pair with a Territory Assault Task for the required outcome.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Wait for Territory Assault Admission"))
class TERRITORYFRAMEWORK_API UTerritoryAssaultAdmissionTask : public UNarrativeTask
{
	GENERATED_BODY()
public:
	UTerritoryAssaultAdmissionTask();

	/** Uses the Wave's native scheduling fields and conditions. Scenario ID is required.
	 * Custom Execute Event overrides are not invoked by this admission task. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Territory Task")
	TObjectPtr<UTerritoryScheduleEnemyWaveEvent> Request;

	/** Server diagnostic. Task completion itself uses Native's replicated progress. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory Task")
	FText LastAdmissionFailure;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;

private:
	friend class FTFAssaultAdmissionLifecycle;
	UFUNCTION()
	void HandleAssaultChanged(const FTerritoryAssaultRecord& Record);
	bool bAttemptingAdmission = false;
	bool HasAdmittedRecord() const;
};
