#include "Core/TerritoryDialogueTags.h"

namespace TerritoryDialogueTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateAlly,
		"Territory.DialogueState.Ally",
		"Territory NPC dialogue state: alliance or shared exact faction. Ambient tagged lines may read as friendly.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateNeutral,
		"Territory.DialogueState.Neutral",
		"Territory NPC dialogue state: no rich treaty, or a trade agreement. Ambient tagged lines stay polite and distant.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateWary,
		"Territory.DialogueState.Wary",
		"Territory NPC dialogue state: ceasefire after hostility. Ambient tagged lines stay cautious.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateHostile,
		"Territory.DialogueState.Hostile",
		"Territory NPC dialogue state: the factions are at war. Ambient tagged lines select the hostile variants.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StateConversing,
		"Territory.DialogueState.Conversing",
		"Territory NPC dialogue state: the NPC is inside a running dialogue. Ambient tagged lines stay quiet.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WarnHolsteredWeapon,
		"Territory.TaggedDialogue.Warn.HolsteredWeapon",
		"Warning line played on a nearby guard while a player stays inside a story-bounds Place with a holstered weapon.");

	FGameplayTag GetDialogueStateTag(EDiplomacyState Relationship, bool bSameFaction)
	{
		// War always wins: a shared faction must never read as friendly while the pair is at war.
		if (Relationship == EDiplomacyState::War)
		{
			return StateHostile;
		}

		if (bSameFaction)
		{
			return StateAlly;
		}

		switch (Relationship)
		{
		case EDiplomacyState::Alliance:
			return StateAlly;

		case EDiplomacyState::Ceasefire:
			return StateWary;

		case EDiplomacyState::NonAggression:
		case EDiplomacyState::TradeAgreement:
		case EDiplomacyState::None:
		default:
			return StateNeutral;
		}
	}

	FGameplayTag GetDialogueStateTagForState(ETerritoryDialogueState State)
	{
		switch (State)
		{
		case ETerritoryDialogueState::Ally:       return StateAlly;
		case ETerritoryDialogueState::Wary:       return StateWary;
		case ETerritoryDialogueState::Hostile:    return StateHostile;
		case ETerritoryDialogueState::Conversing: return StateConversing;
		case ETerritoryDialogueState::Neutral:
		default:                                  return StateNeutral;
		}
	}

	ETerritoryDialogueState GetDialogueStateForTag(const FGameplayTag& Tag)
	{
		if (Tag == StateAlly)       return ETerritoryDialogueState::Ally;
		if (Tag == StateWary)       return ETerritoryDialogueState::Wary;
		if (Tag == StateHostile)    return ETerritoryDialogueState::Hostile;
		if (Tag == StateConversing) return ETerritoryDialogueState::Conversing;
		return ETerritoryDialogueState::Neutral;
	}
}
