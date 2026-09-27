#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryCutsceneCondition.generated.h"

class ULevelSequence;

/** Narrative movie playback condition for the explicit story player. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Is In Cutscene", ToolTip="Check whether this Narrative target player is playing a Narrative movie sequence. Ambient Level Sequence animations and dialogue alone do not count. Missing player context fails both true and false checks."))
class TERRITORYFRAMEWORK_API UTerritoryCutsceneCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryCutsceneCondition();

	/** Require a playing movie when true, or no matching movie when false. Missing player context always fails. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cutscene")
	bool bIsInCutscene = true;

	/** Empty checks any Narrative movie for the player. Does not load the asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cutscene")
	TSoftObjectPtr<ULevelSequence> Sequence;

	/** Also count a paused Narrative movie for this player as being in a cutscene. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cutscene")
	bool bIncludePaused = false;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
