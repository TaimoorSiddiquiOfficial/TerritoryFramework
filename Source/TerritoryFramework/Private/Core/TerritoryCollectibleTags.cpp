#include "Core/TerritoryCollectibleTags.h"

namespace TerritoryCollectibleTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Collectible,
		"Territory.Collectible",
		"Root of the collectible family. One child tag per authored collectible definition.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Category,
		"Territory.CollectibleCategory",
		"Root of the collectible family tags used to group the Dossier grid.");
}
