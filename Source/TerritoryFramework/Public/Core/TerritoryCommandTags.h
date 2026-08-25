#pragma once

#include "NativeGameplayTags.h"

/**
 * Built-in Territory command capabilities. Projects may add their own children
 * below Territory.Capability and use the same State Config grant system.
 */
namespace TerritoryCommandTags
{
	/** Allows the faction to raise persistent garrison staffing targets. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GuardStaffing);

	/** Allows the faction to immediately deploy available reserve guards. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reinforcements);
}
