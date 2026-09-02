#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryNarrativeCheckpointEvent.generated.h"

/**
 * Commits the current Narrative Pro world and quest progress to disk.
 *
 * Narrative already remembers the current quest state, reached states, and
 * task progress. This event supplies the missing save moment. It prefers the
 * campaign save that Narrative is currently using and falls back to the
 * configured save name when a campaign has not been saved yet.
 *
 * Easy example: place this event on the Start runtime of State 2. When State 1
 * finishes, State 2 becomes the checkpoint. If the player dies and the game
 * reloads that save, the quest resumes at State 2.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Save Narrative Quest Checkpoint",
		ToolTip="Save Narrative Pro's current world and quest progress. Easy example: run at the Start of State 2 so death reloads State 2 instead of restarting State 1."))
class TERRITORYFRAMEWORK_API UTerritoryNarrativeCheckpointEvent final
	: public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryNarrativeCheckpointEvent(
		const FObjectInitializer& ObjectInitializer);

	/**
	 * Optional exact save-file name. Leave empty to reuse Narrative's active
	 * campaign. If there is no active campaign, Territory uses Narrative's
	 * Default Save Name plus Fallback Campaign Index.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint",
		meta=(ToolTip="Usually leave empty. Territory reuses the active Narrative save. Easy example: an empty value reuses NarrativeSave2 when the player loaded campaign slot 2."))
	FString SaveNameOverride;

	/** Campaign index appended only when no active save and no override exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint",
		meta=(ClampMin="0", UIMin="0",
			ToolTip="Fallback campaign number used only before Narrative has an active save. Easy example: 0 creates NarrativeSave0."))
	int32 FallbackCampaignIndex = 0;

	/** Platform user slot passed to Unreal's SaveGame API. Usually keep zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint",
		meta=(ClampMin="0", UIMin="0",
			ToolTip="Unreal platform user slot, not the visible Narrative campaign number. Keep 0 for Narrative Pro's standard save menu."))
	int32 PlatformUserSlot = 0;

	virtual void ExecuteEvent_Implementation(APawn* Target,
		APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;

	virtual FString GetGraphDisplayText_Implementation() override;
	virtual FText GetHintText_Implementation() override;
};
