#include "UI/TerritoryCollectibleTileWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/TerritoryCollectibleTileObject.h"
#include "Widgets/SNullWidget.h"

void UTerritoryCollectibleTileWidget::InitializeCollectibleView(const FTerritoryCollectibleView& InView)
{
	View = InView;
	RefreshView();
}

void UTerritoryCollectibleTileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RefreshView();
}

void UTerritoryCollectibleTileWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshView();
}

void UTerritoryCollectibleTileWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// The interface's own native hook routes this to the designer's Blueprint event, so it
	// has to be called explicitly (UUserWidget carries no such Super method).
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (const UTerritoryCollectibleTileObject* Entry = Cast<UTerritoryCollectibleTileObject>(ListItemObject))
	{
		if (Entry->HasCollectibleView())
		{
			InitializeCollectibleView(Entry->GetCollectibleView());
			return;
		}
	}

	// A recycled tile must never keep showing the previous collectible.
	InitializeCollectibleView(FTerritoryCollectibleView());
}

FText UTerritoryCollectibleTileWidget::GetCollectibleNameText(const FTerritoryCollectibleView& InView)
{
	if (!InView.DisplayName.IsEmpty())
	{
		return InView.DisplayName;
	}

	if (InView.CollectibleTag.IsValid())
	{
		// A definition without a name is an authoring gap; showing the last tag segment is
		// more useful than showing nothing, and it tells the designer which row to fix.
		TArray<FString> Segments;
		InView.CollectibleTag.ToString().ParseIntoArray(Segments, TEXT("."));
		return FText::FromString(Segments.Num() > 0 ? Segments.Last() : InView.CollectibleTag.ToString());
	}

	return NSLOCTEXT("TerritoryCollectibleTile", "UnknownName", "Unknown collectible");
}

FText UTerritoryCollectibleTileWidget::GetCollectibleStatusText(const FTerritoryCollectibleView& InView)
{
	if (InView.bFound)
	{
		if (InView.FoundTerritory.IsValid())
		{
			// Campaign clock, same as the History rows: a wall-clock stamp would reorder
			// itself after a load.
			TArray<FString> Segments;
			InView.FoundTerritory.ToString().ParseIntoArray(Segments, TEXT("."));
			const FString Where = Segments.Num() > 0 ? Segments.Last() : InView.FoundTerritory.ToString();
			return FText::Format(
				NSLOCTEXT("TerritoryCollectibleTile", "FoundInFmt", "Found in {0} at T+{1} s"),
				FText::FromString(Where),
				FText::AsNumber(FMath::RoundToInt(InView.FoundGameTimeSeconds)));
		}

		return FText::Format(
			NSLOCTEXT("TerritoryCollectibleTile", "FoundAtFmt", "Found at T+{0} s"),
			FText::AsNumber(FMath::RoundToInt(InView.FoundGameTimeSeconds)));
	}

	// Unfound is a real state, not an error: the tile says so instead of looking broken.
	return NSLOCTEXT("TerritoryCollectibleTile", "NotFound", "Not found yet");
}

FText UTerritoryCollectibleTileWidget::GetCollectibleDescriptionText(const FTerritoryCollectibleView& InView)
{
	if (!InView.Description.IsEmpty())
	{
		return InView.Description;
	}

	if (!InView.CollectibleTag.IsValid())
	{
		return FText::GetEmpty();
	}

	return NSLOCTEXT("TerritoryCollectibleTile", "NoDescription",
		"No description has been authored for this collectible yet.");
}

FText UTerritoryCollectibleTileWidget::GetCollectibleCategoryText(const FTerritoryCollectibleView& InView)
{
	if (!InView.CategoryTag.IsValid())
	{
		return FText::GetEmpty();
	}

	TArray<FString> Segments;
	InView.CategoryTag.ToString().ParseIntoArray(Segments, TEXT("."));
	return FText::FromString(Segments.Num() > 0 ? Segments.Last() : InView.CategoryTag.ToString());
}

FLinearColor UTerritoryCollectibleTileWidget::GetCollectibleTileColor(const FTerritoryCollectibleView& InView)
{
	// Found tiles read brighter than hidden ones, so a wall of tiles shows progress at a
	// glance without a second icon or a counter per row.
	return InView.bFound
		? FLinearColor(0.28f, 0.30f, 0.33f, 1.0f)
		: FLinearColor(0.16f, 0.17f, 0.19f, 1.0f);
}

void UTerritoryCollectibleTileWidget::RefreshView()
{
	const bool bHasView = View.CollectibleTag.IsValid();

	if (NameText)
	{
		NameText->SetText(GetCollectibleNameText(View));
	}
	if (StatusText)
	{
		StatusText->SetText(GetCollectibleStatusText(View));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(GetCollectibleDescriptionText(View));
	}
	if (CategoryText)
	{
		const FText CategoryText2 = GetCollectibleCategoryText(View);
		CategoryText->SetText(CategoryText2);
		CategoryText->SetVisibility(CategoryText2.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TileSurface)
	{
		TileSurface->SetBrushColor(GetCollectibleTileColor(View));
	}
	if (HiddenOverlay)
	{
		// The veil is shown only for a real, unfound collectible: an empty recycled tile
		// must not claim "not found yet" for a collectible that does not exist.
		const bool bHidden = bHasView && !View.bFound;
		HiddenOverlay->SetVisibility(bHidden
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IconImage)
	{
		// Authored icons are soft references: a grid must not force every collectible icon
		// into memory to draw the first page. A missing icon keeps the authored brush.
		if (bHasView && !View.Icon.IsNull())
		{
			if (UTexture2D* IconTexture = View.Icon.LoadSynchronous())
			{
				IconImage->SetBrushFromTexture(IconTexture, true);
			}
		}

		// An unfound collectible is drawn dimmed so the found ones stand out.
		IconImage->SetColorAndOpacity(bHasView && View.bFound
			? FLinearColor::White
			: FLinearColor(0.35f, 0.35f, 0.38f, 1.0f));
	}
}
