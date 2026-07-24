#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TerritoryCombatDirector.generated.h"

class ATerritoryVolume;
class ANarrativeNPCController;
class UNarrativeAbilitySystemComponent;

/**
 * Strategic assault budget manager — limits how many AI can simultaneously
 * attack within a territory. This is SEPARATE from Narrative Pro's per-target
 * attack tokens (UNarrativeAbilitySystemComponent::TryClaimToken):
 *
 * - Narrative tokens = tactical: limits how many AI gang up on ONE defender
 * - Assault slots = strategic: limits how many AI participate in a territory assault
 *
 * AI should use both: RequestAssaultSlot (strategic gate) → RequestAttackToken (tactical).
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryCombatDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Request a strategic assault slot in this territory.
	 * Does NOT claim a Narrative attack token — call RequestAttackToken separately.
	 * Returns true if a slot was granted (or controller already has one).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	bool RequestAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller);

	/** Release an assault slot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	void ReleaseAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller);

	/** Release all assault slots held by this controller across all territories. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	void ReleaseAllSlots(ANarrativeNPCController* Controller);

	/** Check if controller currently holds an assault slot in this territory. */
	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	bool HasAssaultSlot(const ATerritoryVolume* Territory, const ANarrativeNPCController* Controller) const;

	/** Number of assault slots currently granted in this territory. */
	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	int32 GetGrantedSlots(const ATerritoryVolume* Territory) const;

	/** Available assault slots remaining in this territory. */
	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	int32 GetAvailableSlots(const ATerritoryVolume* Territory) const;

private:
	struct FPerTerritorySlots
	{
		TArray<TWeakObjectPtr<ANarrativeNPCController>> GrantedControllers;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritorySlots> SlotMap;

	void CleanupInvalidControllers(FPerTerritorySlots& Slots);

	/** Remove SlotMap entries whose territory weak pointer is no longer valid. */
	void CleanupStaleTerritoryKeys();

	/** Bind to controller's ASC OnDied so slots are released if NPC dies mid-assault. */
	void BindControllerDeath(ANarrativeNPCController* Controller);

	/** Unbind from controller's ASC OnDied to prevent delegate leaks. */
	void UnbindControllerDeath(ANarrativeNPCController* Controller);

	UFUNCTION()
	void OnAssaultControllerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC);

	/** Track which controllers we've already bound to avoid duplicate bindings. */
	TSet<TWeakObjectPtr<ANarrativeNPCController>> BoundControllers;
};
