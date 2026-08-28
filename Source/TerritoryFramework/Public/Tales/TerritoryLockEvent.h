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
	UTerritoryLockEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Territory",
			ToolTip = "Exact City, District, or Place to lock. Easy example: to lock a Farm Place, choose its complete Place tag, not only its parent District tag."))
	FGameplayTag TargetTerritoryTag;

	/** Reason shown to UI/debug. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	FText LockReason;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Unlocks a territory's replicated runtime state so it can be captured.
 * Use in quests/dialogues when story permits territory capture (e.g., after quest completion).
 * The New Campaign Initial State property is an authoring seed and intentionally
 * remains Locked so a future new campaign still begins with the same story gate.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryUnlockEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryUnlockEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Territory",
			ToolTip = "Exact locked City, District, or Place to unlock at runtime. Easy example: choose Territory.MyCity.OldTown.Farm to unlock the Farm Place; choosing Territory.MyCity.OldTown only targets its parent District."))
	FGameplayTag TargetTerritoryTag;

	/** If true, bypasses Locked Exit Conditions. Use for an explicit trusted story command. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	bool bForceUnlock = true;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
