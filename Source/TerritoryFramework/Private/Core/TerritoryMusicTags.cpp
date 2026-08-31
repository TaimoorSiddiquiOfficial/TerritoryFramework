#include "Core/TerritoryMusicTags.h"

namespace TerritoryMusicTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Locked,
		"Music.Territory.State.Locked",
		"Optional Narrative music theme for an unavailable Territory.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unclaimed,
		"Music.Territory.State.Unclaimed",
		"Optional Narrative music theme for an unclaimed Territory.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Contested,
		"Music.Territory.State.Contested",
		"Optional Narrative music theme for an active Territory conflict.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Claimed,
		"Music.Territory.State.Claimed",
		"Optional Narrative music theme for a stable claimed Territory.");
}
