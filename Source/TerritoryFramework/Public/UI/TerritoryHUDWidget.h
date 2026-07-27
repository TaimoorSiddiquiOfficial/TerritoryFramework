#pragma once

#include "CoreMinimal.h"
#include "UI/TerritoryInfoWidget.h"
#include "TerritoryHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** Live territory card for the Narrative gameplay HUD. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryHUDWidget : public UTerritoryInfoWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void RefreshTerritoryDisplay() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DistrictName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DistrictOwnerText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CaptureState;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DistrictDescriptionText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Capture;

private:
	bool bHasObservedState = false;
	ETerritoryState LastObservedState = ETerritoryState::Unclaimed;
	FGameplayTag LastObservedContestingFaction;
};
