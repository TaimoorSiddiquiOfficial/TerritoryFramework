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
}
