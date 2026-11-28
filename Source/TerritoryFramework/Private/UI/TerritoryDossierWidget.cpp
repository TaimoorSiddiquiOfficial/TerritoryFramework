#include "UI/TerritoryDossierWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Subsystems/TerritoryCollectibleSubsystem.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "UI/TerritoryCollectibleTileObject.h"
#include "UI/TerritoryCollectibleTileWidget.h"
#include "UI/TerritoryDossierEntryObject.h"
#include "UI/TerritoryDossierRowWidget.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "TerritoryDossierWidget"

namespace
{
	/** Resolves a tag the ledger is expected to have registered; a missing tag is not fatal here. */
	FGameplayTag DossierRequestTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), /*ErrorIfNotFound=*/false);
	}

	/**
	 * Short player-readable label for a tag: the tag's own leaf, e.g.
	 * Territory.HavenReach.MarketSquare.Blacksmith -> Blacksmith.
	 *
	 * The Dossier is a client-side read model and has no world of its own, so it cannot ask the
	 * Territory actor for the authored display name; the leaf segment is the authored short name
	 * and is documented as the fallback rather than a fabricated string.
	 */
	FString DossierTagLeaf(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return FString();
		}
		const FString FullName = Tag.GetTagName().ToString();
		int32 SeparatorIndex = INDEX_NONE;
		if (FullName.FindLastChar(TEXT('.'), SeparatorIndex))
		{
			return FullName.RightChop(SeparatorIndex + 1);
		}
		return FullName;
	}

	/** Ledger for this widget's world, or null when the world has no evidence authority yet. */
	const UTerritoryEvidenceLedgerSubsystem* ResolveDossierLedger(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}
		const UWorld* World = WorldContextObject->GetWorld();
		return World ? World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>() : nullptr;
	}
}

void UTerritoryDossierWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildNativeLayout();
	RefreshDossier();
}

void UTerritoryDossierWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	RefreshDossier();
	if (DossierReveal)
	{
		PlayAnimation(DossierReveal);
	}
}

void UTerritoryDossierWidget::NativeDestruct()
{
	for (const TObjectPtr<UNarrativeCommonButtonBase>& Button : BoundTabButtons)
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	BoundTabButtons.Reset();
	for (const TObjectPtr<UNarrativeCommonButtonBase>& Chip : BoundFilterChips)
	{
		if (Chip)
		{
			Chip->OnClicked().RemoveAll(this);
		}
	}
	BoundFilterChips.Reset();
	HistoryListEntries.Reset();
	CollectibleTileEntries.Reset();
	Super::NativeDestruct();
}

void UTerritoryDossierWidget::BuildNativeLayout()
{
	BindTabButton(Btn_HistoryTab, ETerritoryDossierDrawer::History);
	BindTabButton(Btn_CollectiblesTab, ETerritoryDossierDrawer::Collectibles);
	BindTabButton(Btn_RelationshipsTab, ETerritoryDossierDrawer::Relationships);
	BindTabButton(Btn_LogsTab, ETerritoryDossierDrawer::Logs);
	BindTabButton(Btn_StatisticsTab, ETerritoryDossierDrawer::Statistics);
	BindTabButton(Btn_AchievementsTab, ETerritoryDossierDrawer::Achievements);
	BindTabButton(Btn_LoreTab, ETerritoryDossierDrawer::Lore);

	BindFilterChip(Btn_FilterAll, ETerritoryDossierCategory::All);
	BindFilterChip(Btn_FilterControl, ETerritoryDossierCategory::Control);
	BindFilterChip(Btn_FilterConflict, ETerritoryDossierCategory::Conflict);
	BindFilterChip(Btn_FilterStealth, ETerritoryDossierCategory::Stealth);
	BindFilterChip(Btn_FilterDiplomacy, ETerritoryDossierCategory::Diplomacy);
	BindFilterChip(Btn_FilterTales, ETerritoryDossierCategory::Tales);

	// UListViewBase keeps EntryWidgetClass protected and only exposes GetEntryWidgetClass() to the
	// editor, so the entry widget is authored on the list itself. The shell cannot complete that
	// wiring at runtime; a list without an entry class simply renders no rows, and the hand-built
	// HistoryEntries column stays available as the fallback the designer can lean on.
	if (HistoryList && !RecordRowWidgetClass)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Dossier] %s exposes a history ListView but no row widget class, so a designer ")
			TEXT("cannot know which entry WBP the list must use."),
			*GetNameSafe(this));
	}

	RefreshFilterChips();
	WriteHeaderText();
}

void UTerritoryDossierWidget::BindTabButton(UNarrativeCommonButtonBase* Button, ETerritoryDossierDrawer Drawer)
{
	if (!Button || BoundTabButtons.Contains(Button))
	{
		return;
	}

	// The project writes every territory button label in C++ (the authored WBP only carries the
	// placeholder), so each tab gets its localised drawer name here rather than in the asset.
	Button->SetButtonText(GetDossierDrawerText(Drawer));

	switch (Drawer)
	{
	case ETerritoryDossierDrawer::Collectibles:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleCollectiblesTabClicked);
		break;
	case ETerritoryDossierDrawer::Relationships:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleRelationshipsTabClicked);
		break;
	case ETerritoryDossierDrawer::Logs:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleLogsTabClicked);
		break;
	case ETerritoryDossierDrawer::Statistics:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleStatisticsTabClicked);
		break;
	case ETerritoryDossierDrawer::Achievements:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleAchievementsTabClicked);
		break;
	case ETerritoryDossierDrawer::Lore:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleLoreTabClicked);
		break;
	case ETerritoryDossierDrawer::History:
	default:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleHistoryTabClicked);
		break;
	}

	BoundTabButtons.Add(Button);
}

void UTerritoryDossierWidget::BindFilterChip(UNarrativeCommonButtonBase* Button, ETerritoryDossierCategory Category)
{
	if (!Button || BoundFilterChips.Contains(Button))
	{
		return;
	}

	// Same convention as the drawer tabs: the label is written here, so the authored chip only has
	// to exist with the right name and the localisation stays in one file.
	Button->SetButtonText(GetDossierCategoryText(Category));
	Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleFilterChipClicked, Category);
	BoundFilterChips.Add(Button);
}

void UTerritoryDossierWidget::RefreshFilterChips()
{
	// The active chip is disabled rather than restyled: the shell reads every filter state from
	// one place, and a disabled chip is the one clear "already applied" signal available to any
	// CommonUI button style without this class reaching into a designer's brush.
	static constexpr ETerritoryDossierCategory ChipOrder[] =
	{
		ETerritoryDossierCategory::All,
		ETerritoryDossierCategory::Control,
		ETerritoryDossierCategory::Conflict,
		ETerritoryDossierCategory::Stealth,
		ETerritoryDossierCategory::Diplomacy,
		ETerritoryDossierCategory::Tales
	};

	for (int32 Index = 0; Index < BoundFilterChips.Num(); ++Index)
	{
		UNarrativeCommonButtonBase* Chip = BoundFilterChips[Index];
		if (!Chip)
		{
			continue;
		}

		const ETerritoryDossierCategory ChipCategory = ChipOrder[Index < UE_ARRAY_COUNT(ChipOrder) ? Index : 0];
		Chip->SetIsEnabled(ChipCategory != SelectedCategory);
	}
}

void UTerritoryDossierWidget::HandleFilterChipClicked(ETerritoryDossierCategory Category)
{
	SetDossierCategory(Category);
}

void UTerritoryDossierWidget::FillHistoryList(const TArray<FTerritoryDossierRecordView>& Records)
{
	if (!HistoryList)
	{
		return;
	}

	HistoryListEntries.Reset();
	TArray<UObject*> Items;
	Items.Reserve(Records.Num());

	for (const FTerritoryDossierRecordView& Record : Records)
	{
		// Outer is this widget so the entries live exactly as long as the screen that shows them.
		UTerritoryDossierEntryObject* Entry = NewObject<UTerritoryDossierEntryObject>(this);
		Entry->SetDossierRecord(Record);
		HistoryListEntries.Add(Entry);
		Items.Add(Entry);
	}

	// SetListItems clears first, so a refresh never stacks a second copy of the history.
	HistoryList->SetListItems(Items);
}

void UTerritoryDossierWidget::FillCollectiblesGrid()
{
	if (!CollectiblesGrid && !CollectiblesProgressText)
	{
		return;
	}

	const FTerritoryCollectibleProgress Progress = GetCollectibleProgressSnapshot(this);

	if (CollectiblesProgressText)
	{
		CollectiblesProgressText->SetText(GetCollectibleProgressText(Progress));
	}

	if (!CollectiblesGrid)
	{
		return;
	}

	if (CollectiblesGrid->GetEntryWidgetClass() == nullptr)
	{
		// A list with no entry class renders nothing at all, and C++ cannot set it
		// (UListViewBase::EntryWidgetClass is protected, and there is no setter), so name the
		// asset the designer is expected to assign instead of leaving a silently empty grid.
		UE_LOG(LogTerritory, Warning,
			TEXT("[Dossier] %s exposes a collectibles TileView with no entry widget class. ")
			TEXT("Assign %s to it, or the grid renders nothing."),
			*GetNameSafe(this),
			CollectibleTileWidgetClass
				? *CollectibleTileWidgetClass->GetPathName()
				: TEXT("/Game/TerritoryFramework/UI/WBP_CollectibleTile"));
		return;
	}

	CollectibleTileEntries.Reset();
	TArray<UObject*> Items;

	const UTerritoryCollectibleSubsystem* Collectibles = GetCollectibleSubsystem(this);
	const TArray<FTerritoryCollectibleView> Views = Collectibles
		? Collectibles->BuildCollectibleViews()
		: TArray<FTerritoryCollectibleView>();

	Items.Reserve(Views.Num());
	for (const FTerritoryCollectibleView& View : Views)
	{
		// Same ownership rule as the history entries: the tile view holds them only while they
		// are in its item array, so the shell keeps the strong reference.
		UTerritoryCollectibleTileObject* Entry = NewObject<UTerritoryCollectibleTileObject>(this);
		Entry->SetCollectibleView(View);
		CollectibleTileEntries.Add(Entry);
		Items.Add(Entry);
	}

	CollectiblesGrid->SetListItems(Items);
}

FText UTerritoryDossierWidget::GetCollectibleProgressText(const FTerritoryCollectibleProgress& Progress)
{
	if (!Progress.bProgressKnown)
	{
		// The page says it does not know rather than claiming an empty collection.
		return LOCTEXT("DossierCollectiblesUnknown", "Collection progress is not known on this client.");
	}

	if (Progress.UnknownTagCount > 0)
	{
		return FText::Format(
			LOCTEXT("DossierCollectiblesWithUnknown", "Found {0} of {1}  |  {2} found item(s) no longer in the authored set"),
			FText::AsNumber(Progress.FoundCount),
			FText::AsNumber(Progress.TotalCount),
			FText::AsNumber(Progress.UnknownTagCount));
	}

	return FText::Format(
		LOCTEXT("DossierCollectiblesProgress", "Found {0} of {1}"),
		FText::AsNumber(Progress.FoundCount),
		FText::AsNumber(Progress.TotalCount));
}

FTerritoryCollectibleProgress UTerritoryDossierWidget::GetCollectibleProgressSnapshot(const UObject* WorldContextObject)
{
	const UTerritoryCollectibleSubsystem* Collectibles = GetCollectibleSubsystem(WorldContextObject);
	return Collectibles ? Collectibles->GetCollectibleProgress() : FTerritoryCollectibleProgress();
}

UTerritoryCollectibleSubsystem* UTerritoryDossierWidget::GetCollectibleSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTerritoryCollectibleSubsystem>() : nullptr;
}

void UTerritoryDossierWidget::WriteHeaderText()
{
	if (TitleText)
	{
		TitleText->SetText(LOCTEXT("DossierTitle", "Dossier"));
	}

	if (!StatusText)
	{
		return;
	}

	const FTerritoryDossierLedgerState State = GetDossierLedgerState();
	if (!State.bLedgerReady)
	{
		// "Not known yet" is not the same claim as "nothing happened" (AGENTS.md §4.2).
		StatusText->SetText(LOCTEXT("DossierStatusSyncing", "History is still syncing with the server"));
		return;
	}

	StatusText->SetText(State.bHistoryTruncated
		? FText::Format(LOCTEXT("DossierStatusTrimmed",
			"{0} facts recorded  |  {1} kept  |  {2} trimmed"),
			FText::AsNumber(State.TotalRecordedCount),
			FText::AsNumber(State.RetainedCount),
			FText::AsNumber(State.EvictedCount))
		: FText::Format(LOCTEXT("DossierStatusCounts", "{0} facts recorded  |  {1} kept"),
			FText::AsNumber(State.TotalRecordedCount),
			FText::AsNumber(State.RetainedCount)));
}

FLinearColor UTerritoryDossierWidget::GetDossierCategoryAccentColor(ETerritoryDossierCategory Category)
{
	// Linear values, in step with the territory palette the journal and district rows use.
	switch (Category)
	{
	case ETerritoryDossierCategory::Control:
		return FLinearColor(0.588f, 0.391f, 0.061f);
	case ETerritoryDossierCategory::Conflict:
		return FLinearColor(0.549f, 0.086f, 0.070f);
	case ETerritoryDossierCategory::Stealth:
		return FLinearColor(0.247f, 0.131f, 0.420f);
	case ETerritoryDossierCategory::Diplomacy:
		return FLinearColor(0.090f, 0.251f, 0.502f);
	case ETerritoryDossierCategory::Tales:
		return FLinearColor(0.130f, 0.400f, 0.200f);
	case ETerritoryDossierCategory::All:
	default:
		return FLinearColor(0.451f, 0.478f, 0.502f);
	}
}

void UTerritoryDossierWidget::HandleHistoryTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::History);
}

void UTerritoryDossierWidget::HandleCollectiblesTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Collectibles);
}

void UTerritoryDossierWidget::HandleRelationshipsTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Relationships);
}

void UTerritoryDossierWidget::HandleLogsTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Logs);
}

void UTerritoryDossierWidget::HandleStatisticsTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Statistics);
}

void UTerritoryDossierWidget::HandleAchievementsTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Achievements);
}

void UTerritoryDossierWidget::HandleLoreTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Lore);
}

void UTerritoryDossierWidget::HandleDrawerButtonClicked(ETerritoryDossierDrawer Drawer)
{
	SetDossierDrawer(Drawer);
}

int32 UTerritoryDossierWidget::GetSwitcherIndexForDrawer(ETerritoryDossierDrawer Drawer)
{
	return static_cast<int32>(Drawer);
}

void UTerritoryDossierWidget::SetDossierDrawer(ETerritoryDossierDrawer InDrawer)
{
	SelectedDrawer = InDrawer;
	if (DossierSwitcher)
	{
		DossierSwitcher->SetActiveWidgetIndex(GetSwitcherIndexForDrawer(InDrawer));
	}

	// Only the page being opened is rebuilt; the Data pages are cheap, the row pages are not.
	switch (InDrawer)
	{
	case ETerritoryDossierDrawer::Collectibles:
		FillCollectiblesGrid();
		break;
	case ETerritoryDossierDrawer::Logs:
		RefreshLogs();
		break;
	case ETerritoryDossierDrawer::Statistics:
		RefreshStatistics();
		break;
	case ETerritoryDossierDrawer::History:
	default:
		RefreshDossier();
		break;
	}
}

void UTerritoryDossierWidget::SetDossierCategory(ETerritoryDossierCategory InCategory)
{
	SelectedCategory = InCategory;

	// The chips are the visible half of the filter: without this the list would look arbitrarily
	// short with no indication of which category is in force.
	RefreshFilterChips();
	RefreshDossier();
}

FText UTerritoryDossierWidget::GetDossierDrawerText(ETerritoryDossierDrawer Drawer)
{
	switch (Drawer)
	{
	case ETerritoryDossierDrawer::Collectibles:
		return LOCTEXT("DossierDrawerCollectibles", "Collectibles");
	case ETerritoryDossierDrawer::Relationships:
		return LOCTEXT("DossierDrawerRelationships", "NPC Relationships");
	case ETerritoryDossierDrawer::Logs:
		return LOCTEXT("DossierDrawerLogs", "Game Logs");
	case ETerritoryDossierDrawer::Statistics:
		return LOCTEXT("DossierDrawerStatistics", "Statistics");
	case ETerritoryDossierDrawer::Achievements:
		return LOCTEXT("DossierDrawerAchievements", "Achievements");
	case ETerritoryDossierDrawer::Lore:
		return LOCTEXT("DossierDrawerLore", "Lore & Notes");
	case ETerritoryDossierDrawer::History:
	default:
		return LOCTEXT("DossierDrawerHistory", "Game History");
	}
}

FText UTerritoryDossierWidget::GetDossierPlaceholderText(ETerritoryDossierDrawer Drawer)
{
	switch (Drawer)
	{
	case ETerritoryDossierDrawer::Collectibles:
		return LOCTEXT("DossierCollectiblesPlaceholder",
			"Collectibles are not tracked yet. This page stays empty rather than listing items the campaign never recorded.");
	case ETerritoryDossierDrawer::Relationships:
		return LOCTEXT("DossierRelationshipsPlaceholder",
			"Per-character standing is not tracked yet. Until a relationship authority exists, this page shows nothing.");
	case ETerritoryDossierDrawer::Achievements:
		return LOCTEXT("DossierAchievementsPlaceholder",
			"Achievements are not tracked yet. They must be awarded by gameplay, never by this screen.");
	case ETerritoryDossierDrawer::Lore:
		return LOCTEXT("DossierLorePlaceholder",
			"Notes and lore readings are not tracked yet. When they are, they will be the player's own words and will never be written by the campaign.");
	case ETerritoryDossierDrawer::Logs:
		return LOCTEXT("DossierLogsPlaceholder",
			"Committed facts about this campaign appear here as a timeline.");
	case ETerritoryDossierDrawer::Statistics:
		return LOCTEXT("DossierStatisticsPlaceholder",
			"No statistic can be derived from the campaign ledger yet.");
	case ETerritoryDossierDrawer::History:
	default:
		return LOCTEXT("DossierHistoryPlaceholder",
			"Committed facts about this campaign appear here.");
	}
}

FTerritoryDossierLedgerState UTerritoryDossierWidget::GetDossierLedgerState() const
{
	FTerritoryDossierLedgerState State;
	const UTerritoryEvidenceLedgerSubsystem* Ledger = ResolveDossierLedger(this);
	if (!Ledger)
	{
		return State;
	}

	State.bLedgerReady = Ledger->IsEvidenceLedgerReady();
	if (!State.bLedgerReady)
	{
		// An unready replica has no rows to count, and reporting zeros would read as "nothing
		// happened" instead of "not known yet" (AGENTS.md §4.2).
		return State;
	}

	State.RetainedCount = Ledger->GetRetainedEvidenceCount();
	State.EvictedCount = Ledger->GetEvictedEvidenceCount();
	State.TotalRecordedCount = Ledger->GetTotalRecordedEvidenceCount();
	State.bHistoryTruncated = State.EvictedCount > 0;
	State.FilteredCount = GetHistoryRecords().Num();
	return State;
}

TArray<FTerritoryDossierRecordView> UTerritoryDossierWidget::GetHistoryRecords() const
{
	TArray<FTerritoryDossierRecordView> Views;

	const UTerritoryEvidenceLedgerSubsystem* Ledger = ResolveDossierLedger(this);
	if (!Ledger || !Ledger->IsEvidenceLedgerReady())
	{
		return Views;
	}

	const TArray<FTerritoryEvidenceRecord> Records = Ledger->GetEvidenceRecords();
	Views.Reserve(Records.Num());

	// The ledger keeps campaign order; the Dossier reads newest first.
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		const FTerritoryEvidenceRecord& Record = Records[Index];
		const ETerritoryDossierCategory Category = ResolveDossierCategory(Record.EvidenceTag);
		if (!MatchesDossierFilter(SelectedCategory, Category))
		{
			continue;
		}

		FTerritoryDossierRecordView View;
		View.ContextId = Record.ContextId;
		View.EvidenceTag = Record.EvidenceTag;
		View.Category = Category;
		View.SubjectId = Record.SubjectId;
		View.InstigatorFaction = Record.InstigatorFaction;
		View.SourceTerritory = Record.SourceTerritory;
		View.Weight = Record.Weight;
		View.GameTimeSeconds = Record.GameTimeSeconds;
		Views.Add(View);
	}

	return Views;
}

void UTerritoryDossierWidget::RefreshDossier()
{
	// Each page refreshes on its own so a WBP that only authors one of them still works.
	const TArray<FTerritoryDossierRecordView> Records = GetHistoryRecords();

	if (HistoryEntries)
	{
		HistoryEntries->ClearChildren();

		if (RecordRowWidgetClass)
		{
			APlayerController* OwningPlayer = GetOwningPlayer();
			for (const FTerritoryDossierRecordView& View : Records)
			{
				UTerritoryDossierRowWidget* Row = OwningPlayer
					? CreateWidget<UTerritoryDossierRowWidget>(OwningPlayer, RecordRowWidgetClass)
					: CreateWidget<UTerritoryDossierRowWidget>(this, RecordRowWidgetClass);
				if (!Row)
				{
					continue;
				}

				Row->OnTerritoryRequested.AddUniqueDynamic(this, &UTerritoryDossierWidget::HandleHistoryRowFocusRequested);
				Row->InitializeDossierRecord(View);
				HistoryEntries->AddChildToVerticalBox(Row);
			}
		}
	}

	else if (HistoryList)
	{
		// The virtualised list wins when it exists: filling both would list every fact twice. The
		// VerticalBox stays the fallback for a WBP that authors plain rows instead of a ListView.
		FillHistoryList(Records);
	}

	if (HistoryEmptyText)
	{
		HistoryEmptyText->SetText(GetHistoryEmptyStateText());
		HistoryEmptyText->SetVisibility(Records.Num() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// The counters move as facts are recorded, so the header is rewritten with the rows.
	WriteHeaderText();
	RefreshLogs();
	RefreshStatistics();
	FillCollectiblesGrid();
}

void UTerritoryDossierWidget::RefreshLogs()
{
	if (!LogsEntries)
	{
		return;
	}

	LogsEntries->ClearChildren();

	const TArray<FTerritoryDossierRecordView> Records = GetHistoryRecords();
	if (Records.Num() == 0)
	{
		if (LogsEmptyText)
		{
			LogsEmptyText->SetText(GetHistoryEmptyStateText());
			LogsEmptyText->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	if (LogsEmptyText)
	{
		LogsEmptyText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// The log lines are written from the same ledger rows the History page lists, so a player who
	// reads the log and a player who reads the history can never be told two different stories.
	for (const FTerritoryDossierRecordView& View : Records)
	{
		UTextBlock* Line = WidgetTree ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()) : nullptr;
		if (!Line)
		{
			continue;
		}

		Line->SetText(FText::Format(
			LOCTEXT("DossierLogLineFmt", "[{0}] {1}: {2}"),
			FText::AsNumber(FMath::RoundToInt(View.GameTimeSeconds)),
			GetDossierCategoryText(View.Category),
			GetEvidenceHeadlineText(View)));
		if (LogsEmptyText)
		{
			Line->SetFont(LogsEmptyText->GetFont());
		}
		LogsEntries->AddChildToVerticalBox(Line);
	}
}

void UTerritoryDossierWidget::RefreshStatistics()
{
	if (!StatisticsEntries)
	{
		return;
	}

	StatisticsEntries->ClearChildren();

	const FTerritoryDossierLedgerState State = GetDossierLedgerState();
	if (!State.bLedgerReady)
	{
		// Showing zeros here would read as "nothing ever happened" instead of "not known yet".
		if (StatisticsEmptyText)
		{
			StatisticsEmptyText->SetText(GetDossierPlaceholderText(ETerritoryDossierDrawer::Statistics));
			StatisticsEmptyText->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	if (StatisticsEmptyText)
	{
		StatisticsEmptyText->SetVisibility(ESlateVisibility::Collapsed);
	}

	TMap<FGameplayTag, int32> CountsByTag;
	int64 TotalWeight = 0;
	if (const UTerritoryEvidenceLedgerSubsystem* Ledger = ResolveDossierLedger(this))
	{
		for (const FTerritoryEvidenceRecord& Record : Ledger->GetEvidenceRecords())
		{
			CountsByTag.FindOrAdd(Record.EvidenceTag)++;
			TotalWeight += Record.Weight;
		}
	}

	if (UWidget* Row = BuildStatisticRow(LOCTEXT("DossierStatRecorded", "Facts recorded"), FText::AsNumber(State.TotalRecordedCount)))
	{
		StatisticsEntries->AddChildToVerticalBox(Row);
	}
	if (UWidget* Row = BuildStatisticRow(LOCTEXT("DossierStatKept", "Facts kept in this window"), FText::AsNumber(State.RetainedCount)))
	{
		StatisticsEntries->AddChildToVerticalBox(Row);
	}
	if (UWidget* Row = BuildStatisticRow(LOCTEXT("DossierStatTrimmed", "Facts trimmed by the retention limit"), FText::AsNumber(State.EvictedCount)))
	{
		StatisticsEntries->AddChildToVerticalBox(Row);
	}
	if (UWidget* Row = BuildStatisticRow(LOCTEXT("DossierStatWeight", "Recorded weight"), FText::AsNumber(TotalWeight)))
	{
		StatisticsEntries->AddChildToVerticalBox(Row);
	}

	// One row per fact type that actually happened. Nothing is invented for a type with no rows.
	CountsByTag.KeySort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });
	for (const TPair<FGameplayTag, int32>& Pair : CountsByTag)
	{
		FTerritoryDossierRecordView Sample;
		Sample.EvidenceTag = Pair.Key;
		Sample.Category = ResolveDossierCategory(Pair.Key);
		if (UWidget* Row = BuildStatisticRow(GetEvidenceHeadlineText(Sample), FText::AsNumber(Pair.Value)))
		{
			StatisticsEntries->AddChildToVerticalBox(Row);
		}
	}
}

UWidget* UTerritoryDossierWidget::BuildStatisticRow(const FText& Label, const FText& Value)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!Row || !LabelText || !ValueText)
	{
		return nullptr;
	}

	LabelText->SetText(Label);
	ValueText->SetText(Value);
	ValueText->SetJustification(ETextJustify::Right);
	if (StatisticsEmptyText)
	{
		LabelText->SetFont(StatisticsEmptyText->GetFont());
		ValueText->SetFont(StatisticsEmptyText->GetFont());
	}
	ValueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.902f, 0.824f, 0.478f, 1.f)));

	Row->AddChildToHorizontalBox(LabelText);
	Row->AddChildToHorizontalBox(ValueText);
	return Row;
}

FText UTerritoryDossierWidget::GetHistoryEmptyStateText() const
{
	const FTerritoryDossierLedgerState State = GetDossierLedgerState();
	if (!State.bLedgerReady)
	{
		return LOCTEXT("DossierLedgerUnavailable",
			"Evidence for this campaign is not available on this machine yet.");
	}
	if (State.bHistoryTruncated && State.RetainedCount == 0)
	{
		return LOCTEXT("DossierHistoryTrimmed",
			"Every recorded fact has already been trimmed out of this campaign's history window.");
	}
	if (State.TotalRecordedCount == 0)
	{
		return LOCTEXT("DossierHistoryEmpty", "Nothing has been recorded in this campaign yet.");
	}
	return LOCTEXT("DossierHistoryFiltered", "No recorded fact matches the selected filter.");
}

ETerritoryDossierCategory UTerritoryDossierWidget::ResolveDossierCategory(FGameplayTag EvidenceTag)
{
	if (!EvidenceTag.IsValid())
	{
		return ETerritoryDossierCategory::All;
	}

	if (EvidenceTag.MatchesTag(DossierRequestTag(TEXT("Territory.Evidence.Capture"))))
	{
		return ETerritoryDossierCategory::Control;
	}
	if (EvidenceTag.MatchesTag(DossierRequestTag(TEXT("Territory.Evidence.Combat"))))
	{
		return ETerritoryDossierCategory::Conflict;
	}
	if (EvidenceTag.MatchesTag(DossierRequestTag(TEXT("Territory.Evidence.Stealth"))))
	{
		return ETerritoryDossierCategory::Stealth;
	}
	if (EvidenceTag.MatchesTag(DossierRequestTag(TEXT("Territory.Evidence.Treaty"))))
	{
		return ETerritoryDossierCategory::Diplomacy;
	}
	if (EvidenceTag.MatchesTag(DossierRequestTag(TEXT("Territory.Evidence.Tales"))))
	{
		return ETerritoryDossierCategory::Tales;
	}
	return ETerritoryDossierCategory::All;
}

bool UTerritoryDossierWidget::MatchesDossierFilter(ETerritoryDossierCategory Filter, ETerritoryDossierCategory Category)
{
	return Filter == ETerritoryDossierCategory::All || Filter == Category;
}

FText UTerritoryDossierWidget::GetDossierCategoryText(ETerritoryDossierCategory Category)
{
	switch (Category)
	{
	case ETerritoryDossierCategory::Control:
		return LOCTEXT("DossierCategoryControl", "Control");
	case ETerritoryDossierCategory::Conflict:
		return LOCTEXT("DossierCategoryConflict", "Conflict");
	case ETerritoryDossierCategory::Stealth:
		return LOCTEXT("DossierCategoryStealth", "Stealth");
	case ETerritoryDossierCategory::Diplomacy:
		return LOCTEXT("DossierCategoryDiplomacy", "Diplomacy");
	case ETerritoryDossierCategory::Tales:
		return LOCTEXT("DossierCategoryTales", "Tales");
	case ETerritoryDossierCategory::All:
	default:
		return LOCTEXT("DossierCategoryAll", "All");
	}
}

FText UTerritoryDossierWidget::GetEvidenceHeadlineText(const FTerritoryDossierRecordView& Record)
{
	const FGameplayTag Tag = Record.EvidenceTag;

	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Capture.Committed"))))
	{
		return LOCTEXT("EvidenceCaptureCommitted", "Territory captured");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Capture.Lost"))))
	{
		return LOCTEXT("EvidenceCaptureLost", "Territory lost");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Combat.GuardKilled"))))
	{
		return LOCTEXT("EvidenceGuardKilled", "Guard killed in the defence");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Combat.AttackerKilled"))))
	{
		return LOCTEXT("EvidenceAttackerKilled", "Attacker killed by the defence");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Stealth.Exposed"))))
	{
		return LOCTEXT("EvidenceStealthExposed", "Exposed inside a Territory");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Stealth.DisguiseCompromised"))))
	{
		return LOCTEXT("EvidenceDisguiseCompromised", "Disguise compromised");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Treaty.Signed"))))
	{
		return LOCTEXT("EvidenceTreatySigned", "Treaty signed");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Treaty.Broken"))))
	{
		return LOCTEXT("EvidenceTreatyBroken", "Treaty broken");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Tales.QuestCompleted"))))
	{
		return LOCTEXT("EvidenceQuestCompleted", "Quest completed");
	}
	if (Tag.MatchesTagExact(DossierRequestTag(TEXT("Territory.Evidence.Tales.DialogueCompleted"))))
	{
		return LOCTEXT("EvidenceDialogueCompleted", "Conversation finished");
	}

	if (!Tag.IsValid())
	{
		return LOCTEXT("EvidenceUnknown", "Unrecorded fact");
	}

	// A tag this build does not know about still shows its own leaf name instead of a
	// generic label, so a newer producer is visible rather than silently flattened.
	return FText::FromString(DossierTagLeaf(Tag));
}

FText UTerritoryDossierWidget::GetEvidenceMetaText(const FTerritoryDossierRecordView& Record)
{
	TArray<FString> Parts;

	if (Record.InstigatorFaction.IsValid())
	{
		Parts.Add(DossierTagLeaf(Record.InstigatorFaction));
	}
	if (Record.SourceTerritory.IsValid())
	{
		Parts.Add(DossierTagLeaf(Record.SourceTerritory));
	}
	if (Record.SubjectId.IsValid())
	{
		Parts.Add(Record.SubjectId.ToString(EGuidFormats::Short));
	}
	Parts.Add(FString::Printf(TEXT("%.1f s"), Record.GameTimeSeconds));

	return FText::FromString(FString::Join(Parts, TEXT("  |  ")));
}

FText UTerritoryDossierWidget::GetEvidenceEntityText(const FTerritoryDossierRecordView& Record)
{
	if (!Record.SubjectId.IsValid())
	{
		// A producer may record a fact about no particular subject (a treaty, a wave). Saying so is
		// honest; printing an empty GUID would read as corrupted data.
		return LOCTEXT("EvidenceNoSubject", "No subject");
	}

	// Short form on purpose: the card shows this next to the headline, and the full GUID stays
	// traceable in the meta line and in the ledger itself.
	return FText::FromString(Record.SubjectId.ToString(EGuidFormats::Short));
}

FText UTerritoryDossierWidget::GetEvidenceTimeText(const FTerritoryDossierRecordView& Record)
{
	// Campaign time, never wall-clock: a save that is reloaded months later must still read in the
	// order the campaign happened (AGENTS.md §7 — saved state, not real time).
	return FText::FromString(FString::Printf(TEXT("T+%.0f s"), Record.GameTimeSeconds));
}

void UTerritoryDossierWidget::HandleHistoryRowFocusRequested(FGameplayTag TerritoryTag)
{
	// The Dossier owns no map or selection state; the screen hosting it decides what focusing a
	// Territory means.
	OnTerritoryFocusRequested.Broadcast(TerritoryTag);
}

#undef LOCTEXT_NAMESPACE
