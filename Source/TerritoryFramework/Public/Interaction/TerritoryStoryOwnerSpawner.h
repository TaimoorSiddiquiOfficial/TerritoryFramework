#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Spawners/NPCSpawner.h"
#include "TerritoryStoryOwnerSpawner.generated.h"

class ANarrativeNPCCharacter;
class UDialogue;
class UNPCSpawnComponent;
class UTalesComponent;
class UTerritoryPlaceDefinition;

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
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Internal/editor synchronization hook. OnConstruction applies the serialized binding. */
	bool ApplyPlaceDefinition();
	UTerritoryPlaceDefinition* GetPlaceDefinition() const { return PlaceDefinition; }
	void SetPlaceDefinition(UTerritoryPlaceDefinition* NewDefinition)
	{
		PlaceDefinition = NewDefinition;
	}

	/** Narrative spawn component that owns the single property-owner NPC. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Story Capture")
	TObjectPtr<UNPCSpawnComponent> OwnerSpawn;

	/** Place served by this owner. Also provides a stable fallback when actor references stream. */
	UPROPERTY(Transient)
	FGameplayTag TerritoryTag;

	/** Begin the NPC definition's dialogue immediately after the owner appears. */
	UPROPERTY(Transient)
	bool bBeginDialogueOnActivation = true;

	/** Optional dialogue override. Empty uses Owner Spawn -> NPC To Spawn -> Dialogue. */
	UPROPERTY(Transient)
	TSubclassOf<UDialogue> OverrideDialogue;

	/** Optional node ID for beginning at a dedicated surrender branch. */
	UPROPERTY(Transient)
	FName DialogueStartFromID;

	/**
	 * Maximum distance for manually speaking to the owner after they appear.
	 * Narrative's player trace is 1000 cm by default, so values above that still
	 * require the project's Narrative Player Interaction setting to be increased.
	 */
	UPROPERTY(Transient)
	float OwnerInteractionDistance = 300.f;

	/** Saved and replicated so a completed defeat does not recreate a silent owner state. */
	UPROPERTY(SaveGame, ReplicatedUsing=OnRep_HandoverActivated)
	bool bHandoverActivated = false;

	/** Server-only activation called by UTerritoryOwnerHandoverEvent. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Story Capture")
	bool ActivateHandover(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* NarrativeComponent, bool bBeginDialogueImmediately = true);

	UFUNCTION(BlueprintPure, Category="Territory|Story Capture")
	ANarrativeNPCCharacter* GetStoryOwner() const;

	UFUNCTION(BlueprintPure, Category="Territory|Story Capture")
	bool IsHandoverActivated() const { return bHandoverActivated; }

	UFUNCTION(BlueprintPure, Category="Territory|Story Capture")
	FGameplayTag GetTerritoryTag() const { return TerritoryTag; }

	UFUNCTION(BlueprintPure, Category="Territory|Story Capture")
	float GetOwnerInteractionDistance() const { return OwnerInteractionDistance; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_HandoverActivated();

private:
	/** Hidden serialized binding maintained by the Definition synchronizer. */
	UPROPERTY()
	TObjectPtr<UTerritoryPlaceDefinition> PlaceDefinition;

	bool EnsureOwnerSpawned();
	void ApplyOwnerInteractionDistance();
	bool BeginOwnerDialogue(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* NarrativeComponent);
};
