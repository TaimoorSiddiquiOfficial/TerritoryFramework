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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryWaypointRequested, ATerritoryDistrict*, District);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryEspionageRequested, ATerritoryDistrict*, District);

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

	/** Selected-state hook used by the journal exactly like a Quest Journal entry button. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	bool IsSelected() const { return bSelected; }

	/** CommonUI focus target for controller and keyboard navigation. */
	UFUNCTION(BlueprintPure, Category="Territory|UI")
	UWidget* GetEntryFocusTarget() const;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryDistrictRowSelected OnDistrictSelected;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryGuardActionRequested OnGuardActionRequested;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryWaypointRequested OnWaypointRequested;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryEspionageRequested OnEspionageRequested;

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
	TObjectPtr<UNarrativeCommonButtonBase> SetWaypointButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> EspionageButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonTextBlock> DistrictName;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonTextBlock> DistrictSummary;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonTextBlock> DistrictStatus;

	/** Native fallbacks use these surfaces to communicate ownership and danger at a glance. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> DistrictRowSurface;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> DistrictAccentRail;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> DistrictStatusSurface;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlaceList;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PlaceProgressTrack;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> OwnedProgressSegment;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> KnownProgressSegment;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> HiddenProgressSegment;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaceProgressText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ExpandHintText;

	private:
	TWeakObjectPtr<ATerritoryDistrict> District;
	FTerritoryDistrictOperationsView OperationsView;
	bool bCanAddGuard = false;
	bool bCanRemoveGuard = false;
	bool bExpanded = false;
	bool bSelected = false;
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

	UFUNCTION()
	void HandleSetWaypoint();

	UFUNCTION()
	void HandleEspionage();
};
