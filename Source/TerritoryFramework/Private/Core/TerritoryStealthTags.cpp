#include "Core/TerritoryStealthTags.h"

namespace TerritoryStealthTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Exposed,
		"Territory.Event.Stealth.Exposed",
		"Sent to an infiltrator when Territory evidence confirms their identity.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StealthAbility,
		"Territory.Ability.Stealth",
		"Ability tag for Territory-aware stealth abilities that end when an infiltrator is exposed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DistractionThrowable,
		"Territory.Distraction.Throwable",
		"Hearing stimulus produced by a nonlethal Territory distraction throwable.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InvestigationActive,
		"Territory.State.Investigating",
		"Granted to a Territory guard while its Narrative investigation activity is active.");
}
