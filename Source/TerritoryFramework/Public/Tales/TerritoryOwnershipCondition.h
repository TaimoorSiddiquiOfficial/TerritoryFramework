#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryOwnershipCondition.generated.h"

UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Ownership"))
class TERRITORYFRAMEWORK_API UTerritoryOwnershipCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition",
		meta = (Categories = "Territory"))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition",
		meta = (Categories = "Narrative.Factions",
			ToolTip="Optional exact owner. Leave empty to use the Narrative target pawn/controller faction when available; if the event has no faction context, any Captured / Claimed owner passes. Easy example: a locked Farm can require the Blacksmith to belong to whichever faction the player currently represents, without hardcoding Heroes."))
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
