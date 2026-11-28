#include "UI/TerritoryDossierWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
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
	Super::NativeDestruct();
}

void UTerritoryDossierWidget::BuildNativeLayout()
{
	BindTabButton(Btn_HistoryTab, ETerritoryDossierDrawer::History);
	BindTabButton(Btn_NotesTab, ETerritoryDossierDrawer::Notes);
	BindTabButton(Btn_CollectiblesTab, ETerritoryDossierDrawer::Collectibles);
	BindTabButton(Btn_RelationshipsTab, ETerritoryDossierDrawer::Relationships);
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
	case ETerritoryDossierDrawer::Notes:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleNotesTabClicked);
		break;
	case ETerritoryDossierDrawer::Collectibles:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleCollectiblesTabClicked);
		break;
	case ETerritoryDossierDrawer::Relationships:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleRelationshipsTabClicked);
		break;
	case ETerritoryDossierDrawer::History:
	default:
		Button->OnClicked().AddUObject(this, &UTerritoryDossierWidget::HandleHistoryTabClicked);
		break;
	}

	BoundTabButtons.Add(Button);
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

void UTerritoryDossierWidget::HandleNotesTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Notes);
}

void UTerritoryDossierWidget::HandleCollectiblesTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Collectibles);
}

void UTerritoryDossierWidget::HandleRelationshipsTabClicked()
{
	HandleDrawerButtonClicked(ETerritoryDossierDrawer::Relationships);
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
	if (InDrawer == ETerritoryDossierDrawer::History)
	{
		RefreshDossier();
	}
}

void UTerritoryDossierWidget::SetDossierCategory(ETerritoryDossierCategory InCategory)
{
	SelectedCategory = InCategory;
	RefreshDossier();
}

FText UTerritoryDossierWidget::GetDossierDrawerText(ETerritoryDossierDrawer Drawer)
{
	switch (Drawer)
	{
	case ETerritoryDossierDrawer::Notes:
		return LOCTEXT("DossierDrawerNotes", "Notes");
	case ETerritoryDossierDrawer::Collectibles:
		return LOCTEXT("DossierDrawerCollectibles", "Collectibles");
	case ETerritoryDossierDrawer::Relationships:
		return LOCTEXT("DossierDrawerRelationships", "Relationships");
	case ETerritoryDossierDrawer::History:
	default:
		return LOCTEXT("DossierDrawerHistory", "History");
	}
}

FText UTerritoryDossierWidget::GetDossierPlaceholderText(ETerritoryDossierDrawer Drawer)
{
	switch (Drawer)
	{
	case ETerritoryDossierDrawer::Notes:
		return LOCTEXT("DossierNotesPlaceholder",
			"Player notes are not tracked yet. When they are, they will be the player's own words and will never be written by the campaign.");
	case ETerritoryDossierDrawer::Collectibles:
		return LOCTEXT("DossierCollectiblesPlaceholder",
			"Collectibles are not tracked yet. This page stays empty rather than listing items the campaign never recorded.");
	case ETerritoryDossierDrawer::Relationships:
		return LOCTEXT("DossierRelationshipsPlaceholder",
			"Per-character standing is not tracked yet. Until a relationship authority exists, this page shows nothing.");
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
	if (!HistoryEntries)
	{
		return;
	}

	const TArray<FTerritoryDossierRecordView> Records = GetHistoryRecords();
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

	if (HistoryEmptyText)
	{
		HistoryEmptyText->SetText(GetHistoryEmptyStateText());
		HistoryEmptyText->SetVisibility(Records.Num() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// The counters move as facts are recorded, so the header is rewritten with the rows.
	WriteHeaderText();
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

void UTerritoryDossierWidget::HandleHistoryRowFocusRequested(FGameplayTag TerritoryTag)
{
	// The Dossier owns no map or selection state; the screen hosting it decides what focusing a
	// Territory means.
	OnTerritoryFocusRequested.Broadcast(TerritoryTag);
}

#undef LOCTEXT_NAMESPACE
