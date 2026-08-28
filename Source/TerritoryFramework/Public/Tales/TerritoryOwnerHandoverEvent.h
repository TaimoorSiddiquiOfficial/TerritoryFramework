#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryOwnerHandoverEvent.generated.h"

class ATerritoryStoryOwnerSpawner;

/**
 * Reveals a Narrative property owner after a story capture prerequisite succeeds.
 * Add this to a Place's On All Defenders Defeated Events, or to Locked -> Exit Events
 * when an undefended story property becomes available after a quest.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory: Begin Owner Handover"))
class TERRITORYFRAMEWORK_API UTerritoryOwnerHandoverEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryOwnerHandoverEvent(const FObjectInitializer& ObjectInitializer);

	/** Direct level reference. Keep the spawner in the same World Partition cell/data layer as the Place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	TObjectPtr<ATerritoryStoryOwnerSpawner> OwnerSpawner;

	/** Stable fallback used to resolve the spawner when the direct reference has streamed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory"))
	FGameplayTag OwnerTerritoryTag;

	/** If false, the owner appears and waits for normal player interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	bool bBeginDialogueImmediately = true;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
