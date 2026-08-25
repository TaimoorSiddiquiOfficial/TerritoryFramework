#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "TerritoryDistrictRowWidget.generated.h"

class ATerritoryDistrict;
class UBorder;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UNarrativeCommonButtonBase;
class UNarrativeCommonTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryDistrictRowSelected, ATerritoryDistrict*, District);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerritoryGuardActionRequested, ATerritoryDistrict*, District, int32, Delta);

/** Runtime-built row used by the district journal list. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryDistrictRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void InitializeDistrict(ATerritoryDistrict* InDistrict);

	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void InitializeOperationsView(const FTerritoryDistrictOperationsView& InView);

	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void SetGuardActionState(bool bCanAdd, bool bCanRemove, const FText& Status);

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	ATerritoryDistrict* GetDistrict() const;

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	FTerritoryDistrictOperationsView GetOperationsView() const { return OperationsView; }

	/** Opens or closes the compact known-Place list without exposing locked Place identities. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void SetExpanded(bool bInExpanded);

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	bool IsExpanded() const { return bExpanded; }

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryDistrictRowSelected OnDistrictSelected;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryGuardActionRequested OnGuardActionRequested;

	/** Inline guard buttons are disabled by default; the command panel owns staffing mutations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|UI")
	bool bShowInlineGuardActions = false;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> SelectDistrictButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> AddGuardButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> RemoveGuardButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonTextBlock> DistrictName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonTextBlock> DistrictSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonTextBlock> DistrictStatus;

	/** Native fallbacks use these surfaces to communicate ownership and danger at a glance. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DistrictRowSurface;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DistrictAccentRail;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DistrictStatusSurface;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PlaceList;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PlaceProgressTrack;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OwnedProgressSegment;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> KnownProgressSegment;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HiddenProgressSegment;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlaceProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExpandHintText;

	private:
	TWeakObjectPtr<ATerritoryDistrict> District;
	FTerritoryDistrictOperationsView OperationsView;
	bool bCanAddGuard = false;
	bool bCanRemoveGuard = false;
	bool bExpanded = false;
	FText ActionStatus;

	void BuildNativeLayout();
	void RefreshRow();
	void RefreshPlaceProgress();
	void RebuildPlaceList();

	UFUNCTION()
	void HandleSelected();

	UFUNCTION()
	void HandleAddGuard();

	UFUNCTION()
	void HandleRemoveGuard();
};
