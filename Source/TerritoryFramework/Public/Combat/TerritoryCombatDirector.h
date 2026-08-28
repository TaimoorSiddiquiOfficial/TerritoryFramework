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
 * AI should use both: RequestAssaultSlot (strategic gate) then TryClaimToken (tactical).
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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat", meta=(DisplayName="Request Slot"))
	bool RequestSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
	{
		return RequestAssaultSlot(Territory, Controller);
	}

	/** Release an assault slot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	void ReleaseAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat", meta=(DisplayName="Release Slot"))
	void ReleaseSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
	{
		ReleaseAssaultSlot(Territory, Controller);
	}

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

	/**
	 * Authored Territory limit after the optional Narrative difficulty cap.
	 * This is a strategic wave limit; Narrative attack tokens remain the tactical
	 * per-defender permission used by Narrative's attack behavior.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Combat",
		meta=(DisplayName="Get Effective Concurrent Attacker Limit"))
	int32 GetEffectiveMaxConcurrentAttackers(const ATerritoryVolume* Territory) const;

	/** True only for a configured physical assault participant targeting this Territory. */
	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	bool IsEligibleAssaultController(const ATerritoryVolume* Territory,
		const ANarrativeNPCController* Controller) const;

	/** Role predicate shared by the BT adapter and regression tests. */
	static bool RequiresStrategicAssaultSlot(const APawn* Pawn);

private:
	struct FPerTerritorySlots
	{
		TArray<TWeakObjectPtr<ANarrativeNPCController>> GrantedControllers;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritorySlots> SlotMap;

	void CleanupInvalidControllers(FPerTerritorySlots& Slots);

	/** Remove SlotMap entries whose territory weak pointer is no longer valid. */
	void CleanupStaleTerritoryKeys();

	/** Bind to Narrative's death-state delegate so slots are released if an NPC dies mid-assault. */
	void BindControllerDeath(ANarrativeNPCController* Controller);

	/** Unbind from Narrative's death-state delegate to prevent delegate leaks. */
	void UnbindControllerDeath(ANarrativeNPCController* Controller);

	UFUNCTION()
	void OnAssaultControllerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC,
		const bool bIsDead);

	/** Track which controllers we've already bound to avoid duplicate bindings. */
	TSet<TWeakObjectPtr<ANarrativeNPCController>> BoundControllers;
	TMap<TWeakObjectPtr<ANarrativeNPCController>, TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundControllerASCs;

	bool ControllerHasAnySlot(const ANarrativeNPCController* Controller) const;
	UNarrativeAbilitySystemComponent* ResolveControllerASC(ANarrativeNPCController* Controller) const;
};
