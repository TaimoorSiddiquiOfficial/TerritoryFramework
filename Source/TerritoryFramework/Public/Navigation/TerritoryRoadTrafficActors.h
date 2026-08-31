#pragma once

#include "CoreMinimal.h"
#include "Vehicles/Mass/QuestRoadControls.h"
#include "TerritoryRoadTrafficActors.generated.h"

class UBoxComponent;

/** Narrative QuestRoadControls with one explicit, designer-visible mission traffic area. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryRoadTrafficControls : public AQuestRoadControls
{
	GENERATED_BODY()

public:
	ATerritoryRoadTrafficControls();

	/** Fits the traffic spawn/filter box around all authored Territory Road Guides. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Territory|Road|Traffic")
	void SetMissionTrafficWorldBounds(const FBox& WorldBounds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Road|Traffic")
	TObjectPtr<UBoxComponent> MissionTrafficBounds;
};
