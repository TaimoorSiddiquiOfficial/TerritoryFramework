#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeEvent.h"
#include "GameplayTagContainer.h"
#include "TerritoryLockEvent.generated.h"

/**
 * Locks a territory from being captured.
 * Use in quests/dialogues when story requires a territory to be inaccessible.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryLockEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Territory"))
	FGameplayTag TargetTerritoryTag;

	/** Reason shown to UI/debug. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	FText LockReason;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Unlocks a territory so it can be captured.
 * Use in quests/dialogues when story permits territory capture (e.g., after quest completion).
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryUnlockEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Territory"))
	FGameplayTag TargetTerritoryTag;

	/** If true, bypasses LockConditions check (force unlock). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	bool bForceUnlock = true;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
