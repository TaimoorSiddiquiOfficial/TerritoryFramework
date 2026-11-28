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
class UListView;
class UTileView;
class UNarrativeCommonButtonBase;
class UTerritoryDossierRowWidget;
class UTerritoryDossierEntryObject;
class UTerritoryCollectibleTileWidget;
class UTerritoryCollectibleTileObject;
class UTerritoryCollectibleSubsystem;

/** Asks the hosting screen to focus one Territory (the Dossier itself owns no map state). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryDossierFocusRequested, FGameplayTag, TerritoryTag);

/**
 * The Dossier: one screen for the player's history in this campaign.
 *
 * The shell owns seven drawers (History, Collectibles, NPC Relationships, Game Logs, Statistics,
 * Achievements, Lore) and a category chip row for the timeline. History, Logs and Statistics are
 * read-only projections of the authoritative evidence ledger; the other four pages have no data
 * behind them yet and say so instead of inventing rows. Nothing here writes gameplay state, and
 * nothing here invents a second authority for what happened.
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

	/**
	 * Subject line on its own, for the reference card where the subject is its own column.
	 * A fact recorded without a subject says so instead of printing an empty GUID.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetEvidenceEntityText(const FTerritoryDossierRecordView& Record);

	/** Campaign-time readout on its own, so a card can print the moment next to the text. */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FText GetEvidenceTimeText(const FTerritoryDossierRecordView& Record);

	/**
	 * Accent colour for a drawer. The palette lives in one place so the list reads as
	 * categories (control, conflict, stealth, diplomacy, tales) instead of one grey column.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Dossier")
	static FLinearColor GetDossierCategoryAccentColor(ETerritoryDossierCategory Category);

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

	/**
	 * Optional virtualised History list. The reference dashboard wants a real ListView, so when a
	 * designer authors one the shell fills it with UTerritoryDossierEntryObject entries and leaves
	 * HistoryEntries alone; the plain VerticalBox stays as the fallback for hand-built rows.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UListView> HistoryList;

	/** Hosts the session-log lines built from the ledger. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> LogsEntries;

	/** Explains an empty log instead of leaving it blank. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LogsEmptyText;

	/** Hosts the aggregate rows derived from the ledger. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> StatisticsEntries;

	/** Explains why a statistic cannot be shown yet instead of printing a zero. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatisticsEmptyText;

	/** Optional headline. The class writes it so the screen is localised in one place. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Optional status line: the real retention counters, or the reason they are unknown. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_HistoryTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_CollectiblesTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_RelationshipsTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_LogsTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_StatisticsTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_AchievementsTab;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_LoreTab;

	/** All Evidence chip, the default filter. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_FilterAll;

	/** Control chip: captures and lost territories. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_FilterControl;

	/** Conflict chip: guards and attackers killed. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_FilterConflict;

	/** Stealth chip: exposures and compromised disguises. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_FilterStealth;

	/** Diplomacy chip: treaties signed and broken. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_FilterDiplomacy;

	/** Tales chip: quests and dialogues completed. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UNarrativeCommonButtonBase> Btn_FilterTales;

	/** Row widget class built into HistoryEntries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Dossier")
	TSubclassOf<UTerritoryDossierRowWidget> RecordRowWidgetClass;

	/**
	 * Optional collectible grid. The reference design calls for a tile wall, so this is a
	 * UTileView; the authored placeholder text stays visible when no grid is authored.
	 */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTileView> CollectiblesGrid;

	/** Optional "x of y found" line above the grid. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CollectiblesProgressText;

	/**
	 * Tile widget class for the collectibles grid, and for a hand-built tile wall. Configured
	 * with the tile class by the designer, the same way RecordRowWidgetClass is.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Dossier")
	TSubclassOf<UTerritoryCollectibleTileWidget> CollectibleTileWidgetClass;

	/** Optional reveal animation, same authoring contract as the Territory journal. */
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DossierReveal;

private:
	/** Reads the drawer navigation contract (name, empty-state copy, switcher index). */
	friend class FTFDossierDrawerContract;

	UPROPERTY(Transient)
	ETerritoryDossierCategory SelectedCategory = ETerritoryDossierCategory::All;

	UPROPERTY(Transient)
	ETerritoryDossierDrawer SelectedDrawer = ETerritoryDossierDrawer::History;

	/** Buttons this instance bound, so NativeDestruct unbinds exactly those. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNarrativeCommonButtonBase>> BoundTabButtons;

	/** Filter chips this instance bound, in category order, so the active one can be marked. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNarrativeCommonButtonBase>> BoundFilterChips;

	/**
	 * Entries handed to HistoryList. Kept as a strong reference because the list view owns them
	 * only for as long as they are in its item array.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTerritoryDossierEntryObject>> HistoryListEntries;

	/**
	 * Entries handed to CollectiblesGrid, kept alive for the same reason as the History
	 * entries: the tile view holds them only while they are in its item array.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTerritoryCollectibleTileObject>> CollectibleTileEntries;

	void BuildNativeLayout();
	void BindTabButton(UNarrativeCommonButtonBase* Button, ETerritoryDossierDrawer Drawer);
	void BindFilterChip(UNarrativeCommonButtonBase* Button, ETerritoryDossierCategory Category);
	void WriteHeaderText();

	/**
	 * Labels every bound chip and marks the active one, so the History page always shows which
	 * filter is in force instead of letting the list look arbitrarily short.
	 */
	void RefreshFilterChips();

	/**
	 * Fills HistoryList with one entry per record. A ListView rebuilds its rows on demand, so the
	 * entries — not widgets — are the thing this class hands over.
	 */
	void FillHistoryList(const TArray<FTerritoryDossierRecordView>& Records);

	/**
	 * Fills CollectiblesGrid from the collectible authority: the authored set joined with the
	 * campaign's findings. Reports "unknown" instead of "0 of 0" when the set is not authored.
	 */
	void FillCollectiblesGrid();

	/** "Found x of y" line, or the honest reason the total is unknown. */
	static FText GetCollectibleProgressText(const FTerritoryCollectibleProgress& Progress);

	/** Progress snapshot from the collectible authority, for the Collectibles page header. */
	static FTerritoryCollectibleProgress GetCollectibleProgressSnapshot(const UObject* WorldContextObject);

	/** The collectible authority for this screen's world, if that world has one. */
	static UTerritoryCollectibleSubsystem* GetCollectibleSubsystem(const UObject* WorldContextObject);

	/** Builds the session-log lines from the same ledger the History page reads. */
	void RefreshLogs();
	/** Builds the aggregate rows from ledger counters; a missing number is explained, not faked. */
	void RefreshStatistics();

	/** One labelled row in the Statistics page. */
	UWidget* BuildStatisticRow(const FText& Label, const FText& Value);
	void HandleDrawerButtonClicked(ETerritoryDossierDrawer Drawer);
	void HandleFilterChipClicked(ETerritoryDossierCategory Category);
	void HandleHistoryTabClicked();
	void HandleCollectiblesTabClicked();
	void HandleRelationshipsTabClicked();
	void HandleLogsTabClicked();
	void HandleStatisticsTabClicked();
	void HandleAchievementsTabClicked();
	void HandleLoreTabClicked();

	UFUNCTION()
	void HandleHistoryRowFocusRequested(FGameplayTag TerritoryTag);

	/** Switcher page index for a drawer. */
	static int32 GetSwitcherIndexForDrawer(ETerritoryDossierDrawer Drawer);
};
