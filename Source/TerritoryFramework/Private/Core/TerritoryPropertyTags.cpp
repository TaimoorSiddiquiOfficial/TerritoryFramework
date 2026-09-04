#include "Core/TerritoryPropertyTags.h"

namespace TerritoryPropertyTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ArmsShopRole,
		"Territory.Property.Role.ArmsShop",
		"Property role for a blacksmith, gunsmith, armoury, or other weapon-upgrade business.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponUpgradesBenefit,
		"Territory.Property.Benefit.WeaponUpgrades",
		"Granted to a Narrative character while its faction controls an active arms-shop benefit tier.");
}
