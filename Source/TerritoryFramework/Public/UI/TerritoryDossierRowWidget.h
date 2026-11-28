#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/TerritoryDossierTypes.h"
#include "TerritoryDossierRowWidget.generated.h"

class UTextBlock;
class UBorder;
class UNarrativeCommonButtonBase;

/** Asks the Dossier to focus the Territory a fact happened in. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryDossierTerritoryRequested, FGameplayTag, TerritoryTag);

/**
 * One read-only evidence row inside the Dossier History list.
 *
 * The row mirrors UTerritoryLiveEventRowWidget: a designer-authored Blueprint supplies the
 * layout, and this class only fills text and forwards a request. It never writes gameplay
 * state, so a row can be reused for any drawer without touching the ledger.
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryDossierRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Populate this row from a ledger-backed presentation snapshot. */
	UFUNCTION(BlueprintCallable, Category="Territory|Dossier")
	void InitializeDossierRecord(const FTerritoryDossierRecordView& InRecord);

	/** Presentation snapshot this row currently shows. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	FTerritoryDossierRecordView GetDossierRecord() const { return Record; }

	/** Raised when the player asks to focus the row's Territory. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Dossier")
	FOnTerritoryDossierTerritoryRequested OnTerritoryRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Fact text, rebuilt from the record on every refresh. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HeadlineText;

	/** Subject, faction, territory and campaign time line. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MetaText;

	/** Bounded weight readout. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> WeightText;

	/**
	 * Optional focus button. A row authored without one still renders; it simply stays
	 * informational instead of pretending to be clickable.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> FocusButton;

	/**
	 * Optional accent strip along the row, tinted with the record's category colour so a long
	 * list can be scanned for conflicts, treaties or stealth without reading every line.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> AccentBar;

	/**
	 * Optional row surface (normally the root border). Tinted with a faint wash of the category
	 * colour, which is what makes the list read as grouped rows instead of one grey slab.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> RowSurface;

private:
	UPROPERTY(Transient)
	FTerritoryDossierRecordView Record;

	/** True once this instance bound its click handler, so a rebuild cannot double-bind. */
	bool bFocusBound = false;

	void BuildNativeLayout();
	void RefreshRecord();
	void HandleFocusClicked();
};
