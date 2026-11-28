#include "UI/TerritoryDossierRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/TerritoryDossierEntryObject.h"
#include "UI/TerritoryDossierWidget.h"
#include "Widgets/NarrativeCommonButtonBase.h"

void UTerritoryDossierRowWidget::InitializeDossierRecord(const FTerritoryDossierRecordView& InRecord)
{
	Record = InRecord;
	BuildNativeLayout();
	RefreshRecord();
}

void UTerritoryDossierRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildNativeLayout();
}

void UTerritoryDossierRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	RefreshRecord();
}

void UTerritoryDossierRowWidget::NativeDestruct()
{
	if (FocusButton && bFocusBound)
	{
		FocusButton->OnClicked().RemoveAll(this);
		bFocusBound = false;
	}
	Super::NativeDestruct();
}

void UTerritoryDossierRowWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// The interface's own native hook is what routes this to the designer's Blueprint event, so it
	// has to be called explicitly (UUserWidget carries no such Super method).
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (const UTerritoryDossierEntryObject* Entry = Cast<UTerritoryDossierEntryObject>(ListItemObject))
	{
		if (Entry->HasDossierRecord())
		{
			InitializeDossierRecord(Entry->GetDossierRecord());
			return;
		}
	}

	// A recycled row must never keep showing the previous fact: an emptied entry clears the row
	// instead of leaving stale history on screen.
	InitializeDossierRecord(FTerritoryDossierRecordView());
}

void UTerritoryDossierRowWidget::BuildNativeLayout()
{
	if (!FocusButton && WidgetTree)
	{
		// An authored row normally names its button through BindWidgetOptional. A designer who
		// simply drops a CommonUI button into the row still gets a working row, because the
		// first common button found is adopted as the focus target.
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (FocusButton)
			{
				return;
			}
			if (UNarrativeCommonButtonBase* Candidate = Cast<UNarrativeCommonButtonBase>(Widget))
			{
				FocusButton = Candidate;
			}
		});
	}

	if (FocusButton && !bFocusBound)
	{
		// Same convention as the shell: the project labels territory buttons in C++, because the
		// authored button only carries the common "BUTTON TEXT" placeholder.
		FocusButton->SetButtonText(NSLOCTEXT("TerritoryDossierRow", "FocusButton", "Locate"));

		FocusButton->OnClicked().AddUObject(this, &UTerritoryDossierRowWidget::HandleFocusClicked);
		bFocusBound = true;
	}
}

void UTerritoryDossierRowWidget::RefreshRecord()
{
	if (HeadlineText)
	{
		HeadlineText->SetText(UTerritoryDossierWidget::GetEvidenceHeadlineText(Record));

		// Category identity in the text itself: the accent is brightened because a saturated
		// strip colour is too dark to read as body text on the panel background.
		const FLinearColor Accent = UTerritoryDossierWidget::GetDossierCategoryAccentColor(Record.Category);
		HeadlineText->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(Accent, FLinearColor::White, 0.35f)));
	}
	if (MetaText)
	{
		MetaText->SetText(UTerritoryDossierWidget::GetEvidenceMetaText(Record));
	}
	if (CategoryText)
	{
		// Same derived category the accent strip and the filter chips use, so the chip in the card
		// header and the chip in the filter row can never disagree.
		CategoryText->SetText(UTerritoryDossierWidget::GetDossierCategoryText(Record.Category));
		CategoryText->SetColorAndOpacity(FSlateColor(
			UTerritoryDossierWidget::GetDossierCategoryAccentColor(Record.Category)));
	}
	if (EntityText)
	{
		EntityText->SetText(UTerritoryDossierWidget::GetEvidenceEntityText(Record));
	}
	if (TimeText)
	{
		TimeText->SetText(UTerritoryDossierWidget::GetEvidenceTimeText(Record));
	}
	if (WeightText)
	{
		WeightText->SetText(FText::Format(
			NSLOCTEXT("TerritoryDossier", "EvidenceWeight", "Weight {0}"),
			FText::AsNumber(Record.Weight)));
	}
	if (AccentBar)
	{
		// The strip carries the category, so the list reads as a hierarchy instead of a grey column.
		AccentBar->SetBrushColor(UTerritoryDossierWidget::GetDossierCategoryAccentColor(Record.Category));
	}
	if (RowSurface)
	{
		// A faint wash of the same colour: enough to group rows at a glance, weak enough that the
		// body text keeps its contrast against the panel behind it.
		const FLinearColor Accent = UTerritoryDossierWidget::GetDossierCategoryAccentColor(Record.Category);
		RowSurface->SetBrushColor(FLinearColor(Accent.R, Accent.G, Accent.B, 0.18f));
	}
	if (FocusButton)
	{
		// A fact without a Territory cannot be focused anywhere, so the affordance is hidden
		// instead of left clickable and inert.
		FocusButton->SetVisibility(Record.SourceTerritory.IsValid()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
}

void UTerritoryDossierRowWidget::HandleFocusClicked()
{
	if (Record.SourceTerritory.IsValid())
	{
		OnTerritoryRequested.Broadcast(Record.SourceTerritory);
	}
}
