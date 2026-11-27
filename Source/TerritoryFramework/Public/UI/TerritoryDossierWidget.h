#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryDossierTypes.h"
#include "TerritoryDossierWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UWidgetAnimation;
class UNarrativeCommonButtonBase;
class UTerritoryDossierRowWidget;

/** Asks the hosting screen to focus one Territory (the Dossier itself owns no map state). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryDossierFocusRequested, FGameplayTag, TerritoryTag);

/**
 * The Dossier: one screen for the player's history in this campaign.
 *
 * Batch 1 ships the shell plus the History drawer, which is a read-only projection of the
 * authoritative evidence ledger. Notes, Collectibles and Relationships are authored pages
 * with no data behind them yet, and the shell says so instead of inventing rows: nothing here
 * writes gameplay state, and nothing here invents a second authority for what happened.
 *
 * The widget stays Abstract so the project authors the layout; this class only binds, reads
 * and formats.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryDossierWidget : public UTerritoryActivatableWidget
{
	GENERATED_BODY()

public:
	/** Rebuilds the drawer list from the current ledger projection. */
	UFUNCTION(BlueprintCallable, Category="Territory|Dossier")
	void RefreshDossier();

	/** Selects a drawer and shows its page. */
	UFUNCTION(BlueprintCallable, Category="Territory|Dossier")
	void SetDossierCategory(ETerritoryDossierCategory InCategory);

	/** Drawer currently shown. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	ETerritoryDossierDrawer GetDossierDrawer() const { return SelectedDrawer; }

	/** Shows a drawer. Drawers without data show their authored placeholder text. */
	UFUNCTION(BlueprintCallable, Category="Territory|Dossier")
	void SetDossierDrawer(ETerritoryDossierDrawer InDrawer);

	/** Localised drawer name. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetDossierDrawerText(ETerritoryDossierDrawer Drawer);

	/**
	 * What a drawer without backing data says. Batch 1 explains the gap instead of drawing
	 * placeholder rows that a player would read as real history.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetDossierPlaceholderText(ETerritoryDossierDrawer Drawer);

	/** Drawer currently selected. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	ETerritoryDossierCategory GetDossierCategory() const { return SelectedCategory; }

	/**
	 * Newest-first view of the facts this client can actually see, narrowed to the selected
	 * drawer. An empty array means "no visible records", which is why readiness is reported
	 * separately by GetDossierLedgerState().
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	TArray<FTerritoryDossierRecordView> GetHistoryRecords() const;

	/** Readiness and retention counters, so the screen can say when history is incomplete. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	FTerritoryDossierLedgerState GetDossierLedgerState() const;

	/** Why the list is empty right now: unavailable, trimmed, nothing recorded, or filtered out. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	FText GetHistoryEmptyStateText() const;

	/** Localised drawer name. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetDossierCategoryText(ETerritoryDossierCategory Category);

	/** Drawer a fact belongs to. Derived from the tag hierarchy; never stored. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static ETerritoryDossierCategory ResolveDossierCategory(FGameplayTag EvidenceTag);

	/** True when a record's category passes the active History filter. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static bool MatchesDossierFilter(ETerritoryDossierCategory Filter, ETerritoryDossierCategory Category);

	/** One-line fact summary for a record row. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetEvidenceHeadlineText(const FTerritoryDossierRecordView& Record);

	/** Subject, faction, territory and campaign-time line for a record row. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetEvidenceMetaText(const FTerritoryDossierRecordView& Record);

	/** Raised when a row asks to focus its Territory. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Dossier")
	FOnTerritoryDossierFocusRequested OnTerritoryFocusRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** History | Notes | Collectibles | Relationships, in that order. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> DossierSwitcher;

	/** Hosts the built record rows. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> HistoryEntries;

	/** Explains an empty list instead of leaving it blank. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HistoryEmptyText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_HistoryTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_NotesTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CollectiblesTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_RelationshipsTab;

	/** Row widget class built into HistoryEntries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Dossier")
	TSubclassOf<UTerritoryDossierRowWidget> RecordRowWidgetClass;

	/** Optional reveal animation, same authoring contract as the Territory journal. */
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DossierReveal;

private:
	UPROPERTY(Transient)
	ETerritoryDossierCategory SelectedCategory = ETerritoryDossierCategory::All;

	UPROPERTY(Transient)
	ETerritoryDossierDrawer SelectedDrawer = ETerritoryDossierDrawer::History;

	/** Buttons this instance bound, so NativeDestruct unbinds exactly those. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNarrativeCommonButtonBase>> BoundTabButtons;

	void BuildNativeLayout();
	void BindTabButton(UNarrativeCommonButtonBase* Button);
	void HandleDrawerButtonClicked(ETerritoryDossierDrawer Drawer);
	void HandleHistoryTabClicked();
	void HandleNotesTabClicked();
	void HandleCollectiblesTabClicked();
	void HandleRelationshipsTabClicked();

	UFUNCTION()
	void HandleHistoryRowFocusRequested(FGameplayTag TerritoryTag);

	/** Switcher page index for a drawer. */
	static int32 GetSwitcherIndexForDrawer(ETerritoryDossierDrawer Drawer);
};
