#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Core/TerritoryDiplomacyTypes.h"

/** Dialogue states an NPC can expose to Narrative tagged dialogue entries. */
UENUM(BlueprintType)
enum class ETerritoryDialogueState : uint8
{
	Ally       UMETA(DisplayName = "Ally",       ToolTip = "Alliance, or the interactor shares the NPC's exact faction."),
	Neutral    UMETA(DisplayName = "Neutral",    ToolTip = "No rich treaty, or a trade agreement."),
	Wary       UMETA(DisplayName = "Wary",       ToolTip = "Ceasefire: temporary peace after hostility."),
	Hostile    UMETA(DisplayName = "Hostile",    ToolTip = "The factions are at war."),
	Conversing UMETA(DisplayName = "Conversing", ToolTip = "The NPC is inside a running dialogue; ambient lines stay quiet.")
};

/**
 * Territory dialogue-state tags.
 *
 * These tags are applied to a Territory NPC's Ability System while a Territory-side attitude
 * state machine (Logic Driver `SM_NPC_Attitude`, or any other server authority) is active, and
 * they are the filter that Narrative Pro's tagged dialogue entries read through
 * `FTaggedDialogue::RequiredTags` / `BlockedTags`. One Greet/Taunt/Flee tag can therefore resolve
 * to different lines per relationship without editing Narrative Pro and without a second faction
 * database.
 *
 * Authority: `UTerritoryDiplomacySubsystem` remains the owner of the relationship and
 * `ANarrativeGameState` remains the AI attitude authority. These tags are a read-model of that
 * state - they never change a treaty, an owner, or an attitude.
 */
namespace TerritoryDialogueTags
{
	/** Applied while the resolved relationship is an Alliance, or the interactor shares the NPC's exact faction. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateAlly);

	/** Applied while no rich treaty exists, or while a trade agreement is active. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateNeutral);

	/** Applied during a ceasefire: temporary peace after hostility, so the NPC stays cautious. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateWary);

	/** Applied while the NPC and interactor factions are at war. War always wins over friendly treaties. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateHostile);

	/** Applied while the NPC is inside a running dialogue, so ambient lines stay quiet. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateConversing);

	/**
	 * Ambient warning line for a player standing inside a story-bounds Place with a holstered
	 * weapon. It is not a dialogue state: the nearest guard plays it through the ordinary tagged
	 * dialogue gate, so cooldown and distance rules still apply.
	 */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WarnHolsteredWeapon);

	/**
	 * Maps the existing Territory diplomacy relationship onto the dialogue-state tag.
	 *
	 * This mapping is intentionally coarser than the seven `UTerritoryDiplomacyDialogueProfile`
	 * slots: the interaction dialogue keeps its fine-grained per-treaty selection, while the
	 * ambient tagged lines only need to know how warm the relationship is right now. A shared
	 * exact faction is treated as friendly unless the pair is at war, so a betrayal never reads
	 * as friendly.
	 */
	TERRITORYFRAMEWORK_API FGameplayTag GetDialogueStateTag(EDiplomacyState Relationship, bool bSameFaction);

	/** Maps the dialogue state enum onto its registered tag. Never returns an invalid tag. */
	TERRITORYFRAMEWORK_API FGameplayTag GetDialogueStateTagForState(ETerritoryDialogueState State);

	/** Inverse of GetDialogueStateTagForState. An unregistered tag maps to ETerritoryDialogueState::Neutral. */
	TERRITORYFRAMEWORK_API ETerritoryDialogueState GetDialogueStateForTag(const FGameplayTag& Tag);
}
