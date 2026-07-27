#pragma once

#include "CoreMinimal.h"
#include "UI/TerritoryInfoWidget.h"
#include "TerritoryDistrictManagementWidget.generated.h"

class ATerritoryDistrict;
class ATerritoryDistrictManagementPoint;
class ATerritoryVolume;
class UNarrativeCommonButtonBase;
class UTerritoryPlayerManagementComponent;
class UTextBlock;

/** Native-backed District management panel. Layout is supplied by a project Widget Blueprint. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryDistrictManagementWidget : public UTerritoryInfoWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void InitializeManagement(ATerritoryDistrictManagementPoint* ManagementPoint);

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	ATerritoryDistrict* GetManagedDistrict() const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedFaction() const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	int32 GetDistrictIncome() const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	bool CanPurchaseGuard(FText& OutFailureReason) const;

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	bool CanRemoveGuard(FText& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RefreshManagementDisplay();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DistrictNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnerText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TreasuryText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> EarningsText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GuardCountText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GuardCostText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> AddGuardButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> RemoveGuardButton;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> CloseButton;

	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Management")
	void OnManagementRefreshed();

	private:
	TWeakObjectPtr<ATerritoryDistrictManagementPoint> ManagementPoint;
	TWeakObjectPtr<UTerritoryPlayerManagementComponent> ManagementComponent;
	FGameplayTag ManagedFaction;
	FTimerHandle RefreshTimerHandle;

	UFUNCTION()
	void HandleAddGuardClicked();

	UFUNCTION()
	void HandleRemoveGuardClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleGuardPurchaseResult(ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId);

	void BindManagementComponent();
};
