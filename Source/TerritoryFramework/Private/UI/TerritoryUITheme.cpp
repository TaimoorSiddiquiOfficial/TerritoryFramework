#include "UI/TerritoryUITheme.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/TerritoryDeveloperSettings.h"
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

		if (UNarrativeCommonTextBlock* NarrativeText =
			Cast<UNarrativeCommonTextBlock>(Text))
		{
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
			if (Style)
			{
				NarrativeText->SetStyle(Style);
			}
		}

		// Narrative theme styles own the font face, but the Territory layout owns
		// responsive scale. Always restore the requested compact size after SetStyle.
		FSlateFontInfo Font = Text->GetFont();
		const float TextScale = Settings
			? FMath::Clamp(Settings->TerritoryTextScale, 0.75f, 2.f) : 1.f;
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
			if (Role == ETerritoryButtonRole::Tab
				|| Role == ETerritoryButtonRole::ToggleAction)
			{
				// CommonButton ignores SetIsSelected while selection is disabled.
				// Tabs and stateful commands (for example a tracked Waypoint)
				// therefore opt into selection without making every action a toggle.
				Button->SetIsSelectable(true);
			}
		}
	}
}
