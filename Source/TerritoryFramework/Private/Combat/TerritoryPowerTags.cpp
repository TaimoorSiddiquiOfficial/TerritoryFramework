#include "Combat/TerritoryPowerTags.h"

namespace TerritoryPowerTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tier1, "Territory.Power.Tier.1",
		"First campaign power tier granted by a Narrative perk or story reward.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tier2, "Territory.Power.Tier.2",
		"Second campaign power tier granted by a Narrative perk or story reward.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tier3, "Territory.Power.Tier.3",
		"Third campaign power tier granted by a Narrative perk or story reward.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tier4, "Territory.Power.Tier.4",
		"Fourth campaign power tier granted by a Narrative perk or story reward.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tier5, "Territory.Power.Tier.5",
		"Fifth campaign power tier granted by a Narrative perk or story reward.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ReservesArrivingDialogue,
		"Territory.Dialogue.ReservesArriving",
		"Optional Narrative tagged dialogue line for a newly arrived reserve wave.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PowerAttackDamageMagnitude,
		"Territory.SetByCaller.PowerAttackDamage",
		"Additive Attack Damage magnitude supplied by adaptive enemy power scaling.");
}
