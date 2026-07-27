#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerritoryDistrictRowWidget.generated.h"

class ATerritoryDistrict;
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
	void SetGuardActionState(bool bCanAdd, bool bCanRemove, const FText& Status);

	UFUNCTION(BlueprintPure, Category="Territory|UI")
	ATerritoryDistrict* GetDistrict() const;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryDistrictRowSelected OnDistrictSelected;

	UPROPERTY(BlueprintAssignable, Category="Territory|UI")
	FOnTerritoryGuardActionRequested OnGuardActionRequested;

protected:
	virtual void NativeConstruct() override;

	private:
	TWeakObjectPtr<ATerritoryDistrict> District;
	TObjectPtr<UNarrativeCommonButtonBase> SelectButton;
	TObjectPtr<UNarrativeCommonButtonBase> AddGuardButton;
	TObjectPtr<UNarrativeCommonButtonBase> RemoveGuardButton;
	TObjectPtr<UNarrativeCommonTextBlock> NameText;
	TObjectPtr<UNarrativeCommonTextBlock> SummaryText;
	TObjectPtr<UNarrativeCommonTextBlock> StatusText;
	bool bCanAddGuard = false;
	bool bCanRemoveGuard = false;
	FText ActionStatus;

	void BuildNativeLayout();
	void RefreshRow();

	UFUNCTION()
	void HandleSelected();

	UFUNCTION()
	void HandleAddGuard();

	UFUNCTION()
	void HandleRemoveGuard();
};
