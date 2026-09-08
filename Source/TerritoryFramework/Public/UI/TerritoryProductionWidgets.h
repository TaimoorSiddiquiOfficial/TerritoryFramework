#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "TerritoryProductionWidgets.generated.h"

class UImage;
class UVerticalBox;
class UNarrativeCommonTextBlock;

/** Reusable read-only resource row for Territory, District, Journal, and Economy screens. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryResourceRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Populate this resource row from an existing resource view. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|Resources")
	void InitializeResourceView(const FTerritoryResourceOperationsView& InView);

	/** Return the read-only resource data currently displayed by this row. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Resources")
	FTerritoryResourceOperationsView GetResourceView() const { return ResourceView; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> ResourceIcon;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UNarrativeCommonTextBlock> ResourceName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UNarrativeCommonTextBlock> StoredQuantityText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UNarrativeCommonTextBlock> ResourceFlowText;

	/** Blueprint presentation hook called after the row's resource data changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|UI|Resources")
	void OnResourceViewChanged(const FTerritoryResourceOperationsView& View);

private:
	FTerritoryResourceOperationsView ResourceView;
	void BuildNativeLayout();
	void RefreshResourceDisplay();
};

/** Reusable production-site row which composes resource rows and owns no gameplay state. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryProductionSiteRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Populate this row from an existing production-site view. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|Production")
	void InitializeProductionSiteView(const FTerritoryProductionSiteOperationsView& InView);

	/** Return the read-only production-site data currently displayed by this row. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Production")
	FTerritoryProductionSiteOperationsView GetProductionSiteView() const { return ProductionSiteView; }

	/** Widget class used to display one production resource input or output row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|UI|Production")
	TSubclassOf<UTerritoryResourceRowWidget> ResourceRowClass;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UNarrativeCommonTextBlock> ProductionSiteName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UNarrativeCommonTextBlock> ProductionStatusText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UNarrativeCommonTextBlock> ProductionReasonText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> ResourceRows;

	/** Blueprint presentation hook called after the production-site data changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|UI|Production")
	void OnProductionSiteViewChanged(const FTerritoryProductionSiteOperationsView& View);

private:
	FTerritoryProductionSiteOperationsView ProductionSiteView;
	void BuildNativeLayout();
	void RefreshProductionDisplay();
};
