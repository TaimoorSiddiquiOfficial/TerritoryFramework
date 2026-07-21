#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeEvent.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryCaptureEvent.generated.h"

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryCaptureEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Territory"))
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag CapturingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Event")
	bool bForceCapture = false;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
};
