#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryOwnershipCondition.generated.h"

UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Ownership",
		ToolTip="Check the current owner. The special Locked, Contested and Unclaimed options are OR exceptions and can pass without matching the owner. Empty owner follows the Narrative participant and fails until their faction is ready. Only a call with no participant accepts any Claimed owner. Requires a loaded territory."))
class TERRITORYFRAMEWORK_API UTerritoryOwnershipCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	/** Stable tag of the Territory evaluated by this Narrative condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition",
		meta = (Categories = "Territory"))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition",
		meta = (Categories = "Narrative.Factions",
			ToolTip="Optional exact owner. Leave empty to use the Narrative target pawn/controller faction, including a participant supplied by Tales. A participant whose faction is not ready fails this check. Only a world-level call with no participant accepts any Claimed owner. Easy example: a locked Farm can require the Blacksmith to belong to whichever faction the player currently represents, without hardcoding Heroes."))
	FGameplayTag RequiredOwner;

	/** Allow this ownership condition to pass while the Territory is Contested, even before checking the required owner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition")
	bool bPassWhenContested = false;

	/** Allow this ownership condition to pass for an Unclaimed Territory, even without the required owner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition")
	bool bPassWhenUnclaimed = false;

	/** Pass for a story-locked Territory even if it does not match Required Owner. Leave off if ownership must always be checked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Condition")
	bool bPassWhenLocked = false;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
