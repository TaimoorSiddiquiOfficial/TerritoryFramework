#include "Core/TerritoryCommandTags.h"

namespace TerritoryCommandTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GuardStaffing,
		"Territory.Capability.GuardStaffing",
		"Allows a faction to add guards or raise a garrison staffing target while it holds a configured Territory source.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Reinforcements,
		"Territory.Capability.Reinforcements",
		"Allows a faction to send an available reserve guard immediately to an owned garrison while it holds a configured Territory source.");
}
