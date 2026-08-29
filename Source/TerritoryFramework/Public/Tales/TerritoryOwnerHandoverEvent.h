#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryOwnerHandoverEvent.generated.h"

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

	/** Exact Place tag used to find the streamed owner spawner without a level-actor reference. */
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
