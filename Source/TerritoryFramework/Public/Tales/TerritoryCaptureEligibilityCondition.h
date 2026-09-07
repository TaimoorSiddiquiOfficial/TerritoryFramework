#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "TerritoryCaptureEligibilityCondition.generated.h"

/**
 * Dialogue/quest condition for a safe territory handover.
 *
 * Easy example: attach this to the owner's "I surrender the Blacksmith" dialogue
 * option. It passes only after the Place is unlocked, all defenders are defeated,
 * and the player's current faction is diplomatically allowed to capture it.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Capture Eligibility Condition"))
class TERRITORYFRAMEWORK_API UTerritoryCaptureEligibilityCondition
	: public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryCaptureEligibilityCondition();

	/** Optional shared Place/faction binding; empty preserves existing authored fields. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	TObjectPtr<class UTerritorySituationProfile> SituationProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Independent Place that the owner NPC may hand over."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryCaptureFactionSource CapturingFactionSource =
		ETerritoryCaptureFactionSource::NarrativeTargetFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions", EditCondition="CapturingFactionSource == ETerritoryCaptureFactionSource::ExplicitFaction", EditConditionHides))
	FGameplayTag ExplicitCapturingFaction;

	/** Recommended for a defeat-then-dialogue handover. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	bool bRequireNoLivingDefenders = true;

	/** Restrict a story handover to Places that explicitly disable automatic flag capture.
	 * False preserves existing authored conditions; new story examples enable this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	bool bRequireStoryCaptureFlow = false;

	/** Enable when the conversation is only valid during active contesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	bool bRequireContestedState = false;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
