#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
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
	virtual void NativeDestruct() override;
	virtual void RefreshTerritoryDisplay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|UI")
	bool bCollapseWhenOutsideTerritory = true;

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

	/** Non-modal styling hook. Implement animations only; never open a pausing menu. */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|UI")
	void OnCounterAttackAlert(const FText& AlertText, float Duration);

	/**
	 * Owning-client state event. Switch on Event.NewState and call Narrative's
	 * Show Narrative HUD Notification (or project presentation) from Blueprint.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Counter Attack")
	void OnCounterHappened(const FTerritoryCounterAttackStateEvent& Event);

private:
	TWeakObjectPtr<class UTerritoryPlayerManagementComponent> ManagementComponent;
	bool bHasObservedState = false;
	ETerritoryState LastObservedState = ETerritoryState::Unclaimed;
	FGameplayTag LastObservedContestingFaction;
	FText ActiveCounterAttackAlert;
	double CounterAttackAlertExpiresAtRealTime = 0.0;

	UFUNCTION()
	void HandleAssaultNotification(const FTerritoryAssaultRecord& Assault);
	UFUNCTION()
	void HandleCounterHappened(const FTerritoryCounterAttackStateEvent& Event);
	void PresentCounterAttackAlert(const FText& AlertText, float Duration);
};
