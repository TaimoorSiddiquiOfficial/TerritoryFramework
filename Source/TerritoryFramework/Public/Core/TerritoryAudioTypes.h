#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryAudioTypes.generated.h"

class USoundBase;
class UTaggedMusicSet;

/**
 * Optional local audio presentation for one Territory state row.
 *
 * Narrative Music remains the soundtrack authority. Territory only selects a
 * Narrative music set/theme from replicated state and may play local one-shot
 * sounds when the observed state changes.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryStateAudioConfig
{
	GENERATED_BODY()

	/** Enables this row as a Territory override for Narrative Music. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|Music",
		meta=(DisplayName="Override Narrative Music",
			ToolTip="When enabled, a local player inside this Territory uses the Music Theme below. The most specific configured Place wins over its District and City. This is cosmetic and never changes replicated gameplay state."))
	bool bOverrideNarrativeMusic = false;

	/**
	 * Optional music library for this state. Empty keeps the active Narrative
	 * music set and only changes its theme.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|Music",
		meta=(EditCondition="bOverrideNarrativeMusic",
			ToolTip="Optional Narrative Tagged Music Set for this Territory state. Easy example: Blacksmith Claimed can use a forge ambience set. Empty uses the current world/default Narrative music set."))
	TSoftObjectPtr<UTaggedMusicSet> MusicSetOverride;

	/** Theme that Narrative Music cross-fades to while this state is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|Music",
		meta=(EditCondition="bOverrideNarrativeMusic", Categories="Music",
			ToolTip="Theme inside the selected Narrative Tagged Music Set. Easy examples: Music.Ambient for calm control, Music.Combat for Contested, or a custom Music.Territory.* theme."))
	FGameplayTag MusicTheme;

	/** Ignores the authored track fade only for this state switch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|Music",
		meta=(EditCondition="bOverrideNarrativeMusic",
			ToolTip="Immediate is useful for a sudden ambush. Leave disabled for professional cross-fades between ambient and combat music."))
	bool bImmediateThemeChange = false;

	/** One-shot local sound when this state begins while the player is inside. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|State Effects",
		meta=(ToolTip="Optional local state-start sound. Easy examples: alarm when Contested begins, victory cue when Claimed begins, or a lock mechanism when Locked begins."))
	TSoftObjectPtr<USoundBase> StateEnteredSound;

	/** One-shot local sound when this state ends while the player is inside. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|State Effects",
		meta=(ToolTip="Optional local state-end sound. Easy example: play an all-clear cue when Contested ends."))
	TSoftObjectPtr<USoundBase> StateExitedSound;

	/** Also play State Entered Sound when the player walks into an existing state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|State Effects",
		meta=(ToolTip="Disabled avoids replaying capture or alarm cues every time the player returns. Enable for an arrival cue that should play on every entry."))
	bool bPlayEnteredSoundOnPlayerArrival = false;

	/** Also play State Exited Sound when the player physically leaves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|State Effects",
		meta=(ToolTip="Disabled means the sound represents a real state transition only. Enable when it should also act as a departure cue."))
	bool bPlayExitedSoundOnPlayerDeparture = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|State Effects",
		meta=(ClampMin="0.0", ClampMax="4.0",
			ToolTip="Volume multiplier for this row's state-entered and state-exited sounds."))
	float StateEffectVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Audio|State Effects",
		meta=(ClampMin="0.25", ClampMax="4.0",
			ToolTip="Pitch multiplier for this row's state-entered and state-exited sounds."))
	float StateEffectPitch = 1.f;

	bool HasUsableMusicOverride() const
	{
		return bOverrideNarrativeMusic && MusicTheme.IsValid();
	}

	bool HasStateEffects() const
	{
		return !StateEnteredSound.IsNull() || !StateExitedSound.IsNull();
	}
};
