#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Spawners/NPCSpawner.h"
#include "TerritoryStoryOwnerSpawner.generated.h"

class ANarrativeNPCCharacter;
class UDialogue;
class UNPCSpawnComponent;
class UTalesComponent;

/**
 * Narrative-owned NPC spawner used by story capture handovers.
 *
 * The owner does not exist before activation. After the final defender is defeated,
 * a Territory Narrative event activates this spawner, Narrative Pro spawns the NPC,
 * and the owner can begin the authored surrender dialogue. The dialogue remains the
 * authority for accepting or refusing the handover; Territory only performs the
 * atomic capture when its normal Narrative capture event is reached.
 */
UCLASS(Blueprintable, Placeable, meta=(DisplayName="Territory Story Owner Spawner"))
class TERRITORYFRAMEWORK_API ATerritoryStoryOwnerSpawner : public ANPCSpawner
{
	GENERATED_BODY()

public:
	ATerritoryStoryOwnerSpawner();

	/** Narrative spawn component that owns the single property-owner NPC. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Story Capture")
	TObjectPtr<UNPCSpawnComponent> OwnerSpawn;

	/** Place served by this owner. Also provides a stable fallback when actor references stream. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Story Capture",
		meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

	/** Begin the NPC definition's dialogue immediately after the owner appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Capture")
	bool bBeginDialogueOnActivation = true;

	/** Optional dialogue override. Empty uses Owner Spawn -> NPC To Spawn -> Dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Capture")
	TSubclassOf<UDialogue> OverrideDialogue;

	/** Optional node ID for beginning at a dedicated surrender branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Capture")
	FName DialogueStartFromID;

	/** Saved and replicated so a completed defeat does not recreate a silent owner state. */
	UPROPERTY(SaveGame, ReplicatedUsing=OnRep_HandoverActivated, BlueprintReadOnly,
		Category="Territory|Story Capture")
	bool bHandoverActivated = false;

	/** Server-only activation called by UTerritoryOwnerHandoverEvent. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Story Capture")
	bool ActivateHandover(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* NarrativeComponent, bool bBeginDialogueImmediately = true);

	UFUNCTION(BlueprintPure, Category="Territory|Story Capture")
	ANarrativeNPCCharacter* GetStoryOwner() const;

	UFUNCTION(BlueprintPure, Category="Territory|Story Capture")
	bool IsHandoverActivated() const { return bHandoverActivated; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_HandoverActivated();

private:
	bool EnsureOwnerSpawned();
	bool BeginOwnerDialogue(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* NarrativeComponent);
};
