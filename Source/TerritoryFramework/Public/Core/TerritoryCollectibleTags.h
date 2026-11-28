#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags for Territory collectibles.
 *
 * A collectible is content the campaign physically found: a landmark photo, a
 * document, an artefact, a recording. The tags here declare only the two roots the
 * framework itself needs, because each collectible and each collectible family is
 * authored content:
 *
 *   Territory.Collectible.*          one tag per authored collectible definition
 *   Territory.CollectibleCategory.*  one tag per authored family, for grouping
 *
 * The two roots are separate on purpose. If families lived under
 * Territory.Collectible.Category.* then a collectible named after its family could
 * collide with a family tag, and the "is this row in the collectible family" check
 * would silently accept a category as a collectible.
 *
 * Ownership: Narrative Pro has no collectible, discovery, codex, or lore authority
 * (verified against its 2.5 source), and Territory had none either before this
 * family, so this is a new Territory-owned read model rather than a duplicate. It
 * records the finding only; names, descriptions, icons and families stay in the
 * authored UTerritoryCollectibleDefinition.
 */
namespace TerritoryCollectibleTags
{
	/** Root of the collectible family. One child tag per authored collectible. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Collectible);

	/** Root of the collectible family tags, used to group the Dossier grid. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Category);
}
