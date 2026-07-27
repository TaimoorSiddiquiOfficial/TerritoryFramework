#pragma once

#include "CoreMinimal.h"
#include "NarrativeActivatableWidget.h"
#include "TerritoryActivatableWidget.generated.h"

class APlayerController;

/** Common project-owned base for Territory tabs and modal screens. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryActivatableWidget : public UNarrativeActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void CloseTerritoryWidget();

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	APlayerController* GetTerritoryPlayerController() const;
};
