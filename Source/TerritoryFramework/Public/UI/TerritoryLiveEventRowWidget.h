#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/TerritoryLiveEventTypes.h"
#include "TerritoryLiveEventRowWidget.generated.h"

class UBorder;
class UTextBlock;
class UNarrativeCommonButtonBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTerritoryLiveEventWaypointRequested, FGameplayTag, TerritoryTag);

/** Compact authored-style row for the Territory activity feed. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryLiveEventRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|Live Events")
	void InitializeLiveEvent(const FTerritoryLiveEvent& InEvent);

	UPROPERTY(BlueprintAssignable, Category="Territory|Live Events")
	FOnTerritoryLiveEventWaypointRequested OnWaypointRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	FTerritoryLiveEvent LiveEvent;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EventSurface;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusDot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadlineText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReportMetaText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ImpactText;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeCommonButtonBase> WaypointButton;

	void BuildNativeLayout();
	void RefreshEvent();

	UFUNCTION()
	void HandleWaypointClicked();
};
