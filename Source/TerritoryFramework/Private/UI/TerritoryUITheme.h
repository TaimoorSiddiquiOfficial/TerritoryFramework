#pragma once

#include "CoreMinimal.h"

class UBorder;
class UProgressBar;
class UTextBlock;
class UNarrativeCommonButtonBase;

enum class ETerritoryTextRole : uint8
{
	Body,
	Title,
	Heading,
	Muted
};

enum class ETerritorySurfaceRole : uint8
{
	Panel,
	Screen
};

/** Navigation selectors and gameplay commands intentionally use different emphasis. */
enum class ETerritoryButtonRole : uint8
{
	Action,
	ToggleAction,
	Tab
};

/** One compact type scale shared by authored and runtime-created Territory UI. */
namespace TerritoryTypography
{
	inline constexpr int32 Caption = 11;
	inline constexpr int32 Metadata = 12;
	inline constexpr int32 Body = 14;
	inline constexpr int32 CardTitle = 16;
	inline constexpr int32 SectionTitle = 18;
	inline constexpr int32 PanelTitle = 22;
	inline constexpr int32 ScreenTitle = 30;
}

/** Shared Territory presentation adapter over Narrative CommonUI styles. */
namespace TerritoryUITheme
{
	void ApplyText(UTextBlock* Text, int32 FontSize, const FLinearColor& Color,
		ETerritoryTextRole Role = ETerritoryTextRole::Body,
		bool bAutoWrap = true);

	void ApplySurface(UBorder* Border, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius = 10.f,
		float OutlineWidth = 1.f, bool bUseThemeTexture = true,
		ETerritorySurfaceRole Role = ETerritorySurfaceRole::Panel);

	bool ApplyProgress(UProgressBar* ProgressBar, bool bUseAuthoredFill);

	void ApplyButton(UNarrativeCommonButtonBase* Button,
		ETerritoryButtonRole Role = ETerritoryButtonRole::Action);

	/**
	 * Apply an explicit selected state without CommonUI's non-toggleable
	 * deselection trap. Tabs and stateful commands such as waypoint buttons must
	 * clear their previous selection through ClearSelection().
	 */
	void SetTabSelected(UNarrativeCommonButtonBase* Button, bool bSelected);
}
