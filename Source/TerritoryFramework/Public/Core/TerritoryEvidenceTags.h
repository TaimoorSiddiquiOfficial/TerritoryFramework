#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags for Territory evidence.
 *
 * Evidence records what the campaign physically observed: a capture that committed,
 * a guard that died, a disguise that was seen through, a treaty that was signed or
 * broken, a story beat that completed. It is deliberately not an opinion, attitude,
 * trust, or relationship value.
 *
 * Ownership follows the story bible, which gives relationship variables, quest
 * history, and dialogue state to Narrative Pro and the physical consequences to
 * Territory (Docs/TDA_STORY_BIBLE_AND_VERTICAL_SLICE.md:162, :176, :503). A tag in
 * this family therefore names an observable fact, never a feeling about a subject.
 *
 * Evidence producers arrive with the gameplay systems that own the fact. This
 * family is declared now so the ledger, its save data, and its tests have stable
 * identity from the first commit.
 */
namespace TerritoryEvidenceTags
{
	/** A Territory changed hands through the existing capture flow. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CaptureCommitted);

	/** A Territory the recording faction held was taken from it. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(CaptureLost);

	/** A Territory guard was killed while defending its post or Territory. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GuardKilled);

	/** A registered counterattack attacker died during an assault. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackerKilled);

	/** An infiltrator's identity was confirmed by Territory stealth evidence. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StealthExposed);

	/** A running disguise was compromised in front of a Territory observer. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DisguiseCompromised);

	/** A treaty reached a signed state between two factions. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TreatySigned);

	/** A treaty was broken or cancelled while its obligations were still live. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TreatyBroken);

	/** A Narrative quest reached a completed state for a specific character. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuestCompleted);

	/** A Narrative dialogue finished between the recording subject and a listener. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DialogueCompleted);
}
