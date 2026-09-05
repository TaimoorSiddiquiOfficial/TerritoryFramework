#include "UI/TerritoryUITheme.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"

namespace TerritoryUITheme
{
	void ApplyText(UTextBlock* Text, int32 FontSize, const FLinearColor& Color,
		ETerritoryTextRole Role, bool bAutoWrap)
	{
		if (!Text)
		{
			return;
		}
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();

		TSubclassOf<UCommonTextStyle> Style;
		if (Settings)
		{
			switch (Role)
			{
			case ETerritoryTextRole::Title:
				Style = Settings->TerritoryTitleTextStyle.LoadSynchronous();
				break;
			case ETerritoryTextRole::Heading:
				Style = Settings->TerritoryHeadingTextStyle.LoadSynchronous();
				break;
			case ETerritoryTextRole::Muted:
				Style = Settings->TerritoryMutedTextStyle.LoadSynchronous();
				break;
			case ETerritoryTextRole::Body:
			default:
				Style = Settings->DefaultTerritoryTextStyle.LoadSynchronous();
				break;
			}
		}

		// CommonText styles are applied again during SynchronizeProperties. The RPG
		// theme's base body style is intentionally a 26 px decorative Cinzel face,
		// which silently replaces the compact size requested by Territory after a
		// generated widget joins the tree. Copy the useful style font first, then
		// detach the live CommonText style so this adapter remains authoritative.
		FSlateFontInfo Font = Text->GetFont();
		if (Style)
		{
			if (const UCommonTextStyle* StyleDefaults =
				Style->GetDefaultObject<UCommonTextStyle>())
			{
				StyleDefaults->GetFont(Font);
			}
		}
		if (UCommonTextBlock* CommonText = Cast<UCommonTextBlock>(Text))
		{
			CommonText->SetStyle(nullptr);
			CommonText->SetTextCase(false);
			CommonText->SetLineHeightPercentage(
				Role == ETerritoryTextRole::Title ? 1.f : 1.08f);
			CommonText->SetApplyLineHeightToBottomLine(false);
		}

		const float TextScale = Settings
			? FMath::Clamp(Settings->TerritoryTextScale, 0.75f, 2.f) : 1.f;
		if (Role != ETerritoryTextRole::Title && Settings)
		{
			if (UFont* InterfaceFont = Settings->TerritoryInterfaceFont.LoadSynchronous())
			{
				Font.FontObject = InterfaceFont;
				Font.TypefaceFontName = Role == ETerritoryTextRole::Heading
					? FName(TEXT("Bold")) : FName(TEXT("Regular"));
			}
		}
		Font.Size = FMath::Max(1.f, FontSize * TextScale);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(bAutoWrap);
	}

	void ApplySurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius, float OutlineWidth,
		bool bUseThemeTexture, ETerritorySurfaceRole Role)
	{
		if (!Border)
		{
			return;
		}

		FSlateBrush Brush;
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		UTexture2D* Texture = nullptr;
		if (bUseThemeTexture && Settings)
		{
			Texture = Role == ETerritorySurfaceRole::Screen
				? Settings->TerritoryScreenBackgroundTexture.LoadSynchronous()
				: Settings->TerritoryPanelTexture.LoadSynchronous();
		}

		if (Texture)
		{
			Brush.DrawAs = Role == ETerritorySurfaceRole::Screen
				? ESlateBrushDrawType::Image
				: ESlateBrushDrawType::Box;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			if (Role == ETerritorySurfaceRole::Panel)
			{
				Brush.Margin = FMargin(0.035f, 0.22f);
				Brush.TintColor = FSlateColor(FLinearColor(
					0.72f + Outline.R * 0.28f,
					0.72f + Outline.G * 0.28f,
					0.72f + Outline.B * 0.28f, Fill.A));
			}
			else
			{
				Brush.TintColor = FSlateColor(Fill);
			}
			Border->SetBrush(Brush);
			return;
		}

		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius, FSlateColor(Outline), OutlineWidth);
		Border->SetBrush(Brush);
	}

	bool ApplyProgress(UProgressBar* ProgressBar, bool bUseAuthoredFill)
	{
		if (!ProgressBar)
		{
			return false;
		}

		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		UTexture2D* FrameTexture = Settings
			? Settings->TerritoryProgressFrameTexture.LoadSynchronous() : nullptr;
		UTexture2D* FillTexture = bUseAuthoredFill && Settings
			? Settings->TerritoryProgressFillTexture.LoadSynchronous() : nullptr;
		if (!FrameTexture && !FillTexture)
		{
			return false;
		}

		FProgressBarStyle Style = ProgressBar->GetWidgetStyle();
		if (FrameTexture)
		{
			FSlateBrush Frame;
			Frame.DrawAs = ESlateBrushDrawType::Box;
			Frame.SetResourceObject(FrameTexture);
			Frame.ImageSize = FVector2D(
				FrameTexture->GetSizeX(), FrameTexture->GetSizeY());
			Frame.Margin = FMargin(0.04f, 0.28f);
			Frame.TintColor = FSlateColor(FLinearColor::White);
			Style.BackgroundImage = Frame;
		}
		if (FillTexture)
		{
			FSlateBrush FillBrush;
			FillBrush.DrawAs = ESlateBrushDrawType::Box;
			FillBrush.SetResourceObject(FillTexture);
			FillBrush.ImageSize = FVector2D(
				FillTexture->GetSizeX(), FillTexture->GetSizeY());
			FillBrush.Margin = FMargin(0.02f, 0.24f);
			FillBrush.TintColor = FSlateColor(FLinearColor::White);
			Style.FillImage = FillBrush;
		}
		ProgressBar->SetWidgetStyle(Style);
		return FillTexture != nullptr;
	}

	void ApplyButton(UNarrativeCommonButtonBase* Button,
		ETerritoryButtonRole Role)
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		TSubclassOf<UCommonButtonStyle> Style;
		if (Settings)
		{
			Style = Role == ETerritoryButtonRole::Tab
				? Settings->TerritoryTabButtonStyle.LoadSynchronous()
				: Settings->TerritoryActionButtonStyle.LoadSynchronous();
			if (!Style)
			{
				Style = Settings->DefaultTerritoryButtonStyle.LoadSynchronous();
			}
		}
		if (Button && Style)
		{
			Button->SetStyle(Style);
			// Narrative's generic button template forces all-caps. Territory tab names
			// and actions use sentence/title case so their casing remains consistent
			// with the information beneath them. Font weight still comes from the
			// configured Normal/Hovered/Selected/Disabled button text styles.
			if (Button->WidgetTree)
			{
				Button->WidgetTree->ForEachWidgetAndDescendants([](UWidget* Widget)
				{
					if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(Widget))
					{
						Label->SetTextCase(false);
					}
				});
			}
			if (Role == ETerritoryButtonRole::Tab
				|| Role == ETerritoryButtonRole::ToggleAction)
			{
				// CommonButton ignores SetIsSelected while selection is disabled.
				// Tabs and stateful commands (for example a tracked Waypoint)
				// therefore opt into selection without making every action a toggle.
				Button->SetIsSelectable(true);
				if (Role == ETerritoryButtonRole::Tab)
				{
					// The selected tab remains hoverable so SelectedHovered is a real,
					// visible state rather than an unreachable style slot.
					Button->SetIsInteractableWhenSelected(true);
				}
			}
		}
	}

	void SetTabSelected(UNarrativeCommonButtonBase* Button, bool bSelected)
	{
		if (!Button || Button->GetSelected() == bSelected)
		{
			return;
		}

		if (bSelected)
		{
			Button->SetIsSelected(true, false);
		}
		else
		{
			// CommonUI ignores SetIsSelected(false) when bToggleable is false.
			// ClearSelection is the supported explicit group-coordination path.
			Button->ClearSelection();
		}
	}
}
