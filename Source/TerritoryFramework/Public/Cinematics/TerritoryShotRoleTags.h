#pragma once

#include "Cinematics/TerritoryDialogueShot.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

/**
 * Native gameplay tags for dialogue shot roles.
 *
 * The role enum stays the authoring default so existing assets, the built-in shot table and the
 * shot guidance keep working. A tag lets a designer name a new editorial role in data instead of
 * extending the enum, and gives the role a stable semantic identity that quests and validation can
 * query. Use GetTerritoryShotRoleTag() to resolve either form.
 */
namespace TerritoryShotRoleTags
{
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Establishing);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TwoShot);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(OverShoulder);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MediumCloseUp);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CloseUp);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reaction);
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Insert);

	/** The role a legacy enum value maps to. Empty for an unrecognised value. */
	TERRITORYFRAMEWORK_API FGameplayTag FromLegacyRole(ETerritoryDialogueShotRole Role);
}
