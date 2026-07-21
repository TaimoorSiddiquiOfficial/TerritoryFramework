#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryOwnershipCondition.generated.h"

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryOwnershipCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition",
		meta = (Categories = "Territory"))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag RequiredOwner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition")
	bool bPassWhenContested = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition")
	bool bPassWhenUnclaimed = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition")
	bool bPassWhenLocked = false;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
