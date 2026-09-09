#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryConditionGroup.generated.h"

UENUM(BlueprintType)
enum class ETerritoryConditionGroupMatch : uint8
{
	All UMETA(DisplayName="All Must Pass (AND)", ToolTip="Every listed condition must pass."),
	Any UMETA(DisplayName="Any May Pass (OR)", ToolTip="At least one listed condition must pass.")
};

/** Combines existing Narrative conditions. Stores rules only, never another copy of quest or territory state. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Condition Group (All / Any)",
		ToolTip="Combine any Narrative conditions. Example: All of Blacksmith Locked and Any of Player Trusted or Bandits Weak. Child Not and party options are respected. Empty groups, empty rows, and cycles fail. The group's own Not is applied by Narrative."))
class TERRITORYFRAMEWORK_API UTerritoryConditionGroup : public UNarrativeCondition
{
	GENERATED_BODY()
public:
	UTerritoryConditionGroup();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="All means every row must pass. Any means one passing row is enough. Use nested groups to mix AND and OR."))
	ETerritoryConditionGroupMatch Match = ETerritoryConditionGroupMatch::All;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Territory Condition",
		meta=(ToolTip="Add existing Territory or Narrative conditions here. A row's Not checkbox reverses that row. Add a Known State check before an inverted state check when missing data must block the dialogue."))
	TArray<TObjectPtr<UNarrativeCondition>> Conditions;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;

private:
	bool HasValidStructure(TSet<const UTerritoryConditionGroup*>& Path, int32 Depth) const;
	bool bEvaluating = false;
};
