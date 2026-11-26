#include "Core/TerritoryEvidenceTags.h"

namespace TerritoryEvidenceTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CaptureCommitted,
		"Territory.Evidence.Capture.Committed",
		"Physical fact: a Territory changed hands through the existing capture flow.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CaptureLost,
		"Territory.Evidence.Capture.Lost",
		"Physical fact: a Territory the recorded subject held was taken from it.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GuardKilled,
		"Territory.Evidence.Combat.GuardKilled",
		"Physical fact: a Territory guard died while defending its post or Territory.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackerKilled,
		"Territory.Evidence.Combat.AttackerKilled",
		"Physical fact: a registered counterattack attacker died during an assault.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StealthExposed,
		"Territory.Evidence.Stealth.Exposed",
		"Physical fact: Territory stealth evidence confirmed an infiltrator's identity.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DisguiseCompromised,
		"Territory.Evidence.Stealth.DisguiseCompromised",
		"Physical fact: a running disguise was compromised in front of a Territory observer.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TreatySigned,
		"Territory.Evidence.Treaty.Signed",
		"Physical fact: a treaty reached a signed state between two factions.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TreatyBroken,
		"Territory.Evidence.Treaty.Broken",
		"Physical fact: a treaty was broken or cancelled while its obligations were still live.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(QuestCompleted,
		"Territory.Evidence.Tales.QuestCompleted",
		"Physical fact: a Narrative quest reached a completed state for a specific character.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DialogueCompleted,
		"Territory.Evidence.Tales.DialogueCompleted",
		"Physical fact: a Narrative dialogue finished between the recording subject and a listener.");
}
