#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerritoryNarrativeQuestStarter.generated.h"

class ANarrativePlayerController;
class UQuest;
class UTalesComponent;

/**
 * Server-authoritative bridge for safely starting a Narrative Quest from a level.
 *
 * Narrative creates each player's Tales component after level BeginPlay and may
 * restore a save before the player is ready for new Quest state. This actor waits
 * for that lifecycle instead of asking level Blueprints to call BeginQuest on a
 * possibly-null component. One placed actor can also initialize late joiners.
 *
 * Easy example: place this in Blacksmith, select NQ_CaptureBlacksmith, and remove
 * the unsafe Event BeginPlay -> Get Tales Component -> Begin Quest graph.
 */
UCLASS(BlueprintType, Blueprintable, Placeable,
	meta=(DisplayName="Territory Narrative Quest Starter",
		ToolTip="Safely starts one Narrative Quest after each selected player's pawn and Tales component are ready and Tales is not loading a save. Use this instead of calling Begin Quest directly from a Level Blueprint BeginPlay event."))
class TERRITORYFRAMEWORK_API ATerritoryNarrativeQuestStarter : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryNarrativeQuestStarter();

	/** Narrative Quest started for the configured player audience. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Narrative Quest",
		meta=(ToolTip="Narrative Quest class to start. Easy example: NQ_CaptureBlacksmith."))
	TSubclassOf<UQuest> QuestClass;

	/** Optional Narrative Quest state ID used instead of the normal start state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Narrative Quest",
		meta=(ToolTip="Usually leave None. Set a valid Narrative Quest State ID only when this story should resume from a specific authored state."))
	FName StartFromID = NAME_None;

	/** Start the Quest independently for every connected player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Narrative Quest|Multiplayer",
		meta=(ToolTip="Enable for personal story progress so every player receives the Quest. Disable when only the first ready player should own this Quest."))
	bool bStartForEveryPlayer = true;

	/** Continue watching for players who connect after the level has started. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Narrative Quest|Multiplayer",
		meta=(ToolTip="Keeps a light server timer active so late-joining players also receive the Quest. Disable only for a one-time session event that must ignore late joiners."))
	bool bKeepPollingForLateJoiningPlayers = true;

	/** Time allowed for Narrative player creation before the first check. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Narrative Quest|Readiness",
		meta=(ClampMin="0.0", UIMin="0.0", Units="s",
			ToolTip="Delay before the first readiness check. This is not relied on for correctness; the actor keeps retrying until Narrative is actually ready."))
	float InitialDelay = 0.25f;

	/** Server readiness retry interval. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Narrative Quest|Readiness",
		meta=(ClampMin="0.1", UIMin="0.1", Units="s",
			ToolTip="How often the server checks for a ready Tales component, completed save loading, and late joiners."))
	float RetryInterval = 0.25f;

	/** Immediately check all eligible server players. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Territory|Narrative Quest")
	void TryStartPendingPlayers();

	/** Number of Tales components already initialized or found to own this Quest. */
	UFUNCTION(BlueprintPure, Category="Territory|Narrative Quest|Debug")
	int32 GetHandledPlayerCount() const { return HandledTalesComponents.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryStartForController(ANarrativePlayerController* Controller,
		bool& bOutPlayerReady);
	void StopReadinessPolling();

	FTimerHandle ReadinessTimer;
	TSet<TWeakObjectPtr<UTalesComponent>> HandledTalesComponents;
	bool bSingleAudienceHandled = false;
};
