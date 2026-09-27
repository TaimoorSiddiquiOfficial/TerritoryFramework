// Copyright Territory Framework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NarrativeActorProvider.h"
#include "NarrativeActorProvider_InteractableSpot.generated.h"

class AActor;
class UNarrativeInteractableComponent;
class UNarrativeInteractionComponent;

/**
 * Resolves the interactable prop a patrol spot refers to, at runtime, by query.
 *
 * Narrative Pro already owns seat and slot capacity: UNarrativeInteractableComponent holds the
 * authored slots, the replicated statuses, and the server-gated claim. This provider contributes
 * only *which prop* a spot names — never whether a slot is free, which stays with the vendor so
 * there remains a single authority for it.
 *
 * Patrol delivery uses this query once, then binds the resolved actor through Narrative's
 * LevelReference provider. Goal_Interact caches its interactable during Initialize, so the
 * selected prop and slot must describe the same resolution before the goal is admitted.
 *
 * The search parameters are deliberately unset by default: an unconfigured provider resolves
 * nothing rather than guessing, so an unauthored spot can never silently grab a nearby prop.
 */
UCLASS(BlueprintType, EditInlineNew, AutoExpandCategories = ("Provider", "TerritorySpot"),
	meta = (DisplayName = "Find Interactable Spot (Territory)"))
class TERRITORYFRAMEWORK_API UNarrativeActorProvider_InteractableSpot : public UNarrativeActorProvider
{
	GENERATED_BODY()

public:
	UNarrativeActorProvider_InteractableSpot();

	virtual class AActor* ProvideActor_Implementation(const UObject* WorldContextObject) override;
	virtual FText GetDescription() const override;

	/**
	 * Centre of the search, in world space. Territory sets this to the patrol node's world
	 * location before delivery; an author placing this provider by hand sets it explicitly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerritorySpot")
	FVector SearchOrigin = FVector::ZeroVector;

	/**
	 * Search radius in cm.
	 * 0 disables the query outright rather than falling back to a default, so a spot that was
	 * never configured resolves nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerritorySpot",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5000.0"))
	float SearchRadius = 0.f;

	/**
	 * Optional class filter.
	 * When set, the query searches that class directly, so an authored spot prop is found even
	 * when it has no collision. When null, the query is a physical sphere overlap and the prop
	 * must overlap it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerritorySpot")
	TSubclassOf<AActor> SpotActorClass;

	/**
	 * Optional slot filter. When set, a candidate must expose at least one free slot whose
	 * authored FInteractionSlotConfig::SlotTag matches.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerritorySpot",
		meta = (Categories = "Interaction"))
	FGameplayTag SpotTag;

	/**
	 * The NPC whose claim this resolution is for.
	 *
	 * Set by Territory at delivery time, and required: the vendor's availability rule evaluates
	 * IsUsable/IsStealable against a specific claimer, and resolving with a null claimer would
	 * exercise a call shape Narrative Pro itself never uses. With no claimer set, this provider
	 * resolves nothing.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "TerritorySpot")
	TObjectPtr<UNarrativeInteractionComponent> ClaimerComponent;

	/**
	 * The chosen spot actor, or null when nothing eligible is in range.
	 * Side-effect free, so the delivery path and tests can both call it.
	 */
	UFUNCTION(BlueprintCallable, Category = "TerritorySpot")
	AActor* ResolveSpotActor(const UObject* WorldContextObject);

	/**
	 * The free slots this provider accepts on the given interactable, per Narrative Pro's own
	 * availability rule. An empty result means the component is not an eligible spot.
	 */
	bool GetEligibleSlots(UNarrativeInteractableComponent* Interactable, TArray<int32>& OutSlots);
};
