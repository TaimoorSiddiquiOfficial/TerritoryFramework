#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryDiplomacyCondition.generated.h"

/**
 * Checks the rich Territory diplomacy state between two Narrative factions.
 *
 * Easy example: require Heroes and Bandits to be at War before a Locked District
 * can open. Use Narrative's inherited "Not" option when the opposite result is needed.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Diplomacy Condition"))
class TERRITORYFRAMEWORK_API UTerritoryDiplomacyCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryDiplomacyCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions",
			ToolTip="First Narrative faction. Example: Narrative.Factions.Heroes."))
	FGameplayTag FactionA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions",
			ToolTip="Second Narrative faction. Example: Narrative.Factions.Bandits."))
	FGameplayTag FactionB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Relationship that must currently be true. Example: War allows hostile capture and physical counterattacks."))
	EDiplomacyState RequiredState = EDiplomacyState::War;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
