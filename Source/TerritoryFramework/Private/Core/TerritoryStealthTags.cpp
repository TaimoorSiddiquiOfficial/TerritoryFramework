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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseActive,
		"Territory.State.Disguise.Active",
		"The actor has an equipped Territory disguise. This does not change its true Narrative faction.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseCompromised,
		"Territory.State.Disguise.Compromised",
		"At least one faction has identified the disguised actor.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseActivatedEvent,
		"Territory.Event.Disguise.Activated",
		"Sent to the wearer's Ability System when a Territory disguise becomes active.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseRemovedEvent,
		"Territory.Event.Disguise.Removed",
		"Sent when the active Territory disguise is removed.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseCompromisedEvent,
		"Territory.Event.Disguise.Compromised",
		"Sent when an observer faction identifies the disguised actor.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseRestoredEvent,
		"Territory.Event.Disguise.Restored",
		"Sent when a burned identity is restored for an observer faction.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseCheckPassedEvent,
		"Territory.Event.Disguise.IdentityCheckPassed",
		"Sent when a Territory identity or clearance check accepts the disguise.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseCheckFailedEvent,
		"Territory.Event.Disguise.IdentityCheckFailed",
		"Sent when a Territory identity or clearance check rejects the disguise.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseOfficerClearance,
		"Territory.Disguise.Clearance.Officer",
		"Example clearance tag for a restricted officer-only Territory area.");
}
