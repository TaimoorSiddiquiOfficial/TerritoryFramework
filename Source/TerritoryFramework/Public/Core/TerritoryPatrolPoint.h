#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "TerritoryPatrolPoint.generated.h"

/**
 * A single world-placed patrol waypoint for territory guards.
 *
 * Place these in the level and assign them to a TerritoryVolume's PatrolPoints array.
 * The actor's own world transform IS the patrol node — no manual transform entry needed.
 *
 * Quick usage:
 *   1. Place ATerritoryPatrolPoint actors in the level along the desired patrol path.
 *   2. On the owning TerritoryVolume, add them to the PatrolPoints array.
 *   3. Guards read them via TerritoryVolume::GetPatrolPoints() or
 *      ATerritoryPatrolPoint::GetPatrolTransform().
 *
 * Each point optionally carries a wait time and activity tag, matching the
 * FTerritoryPatrolNode struct on GuardSpawnPoint for consistency.
 */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Territory Patrol Point"))
class TERRITORYFRAMEWORK_API ATerritoryPatrolPoint : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryPatrolPoint();

	/** Which territory this patrol point belongs to. Resolved by tag, then by proximity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol",
		meta=(Categories="Territory", DisplayName="Owner Territory"))
	FGameplayTag OwnerTerritoryTag;

	/**
	 * Seconds the guard waits at this point before proceeding to the next.
	 * Use 0 for continuous patrol with no waiting. Typical: 2-5s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol",
		meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0", DisplayName="Wait Time"))
	float WaitTime = 2.f;

	/**
	 * Optional activity tag played at this node instead of standing idle.
	 * E.g., Guard.Activity.Inspect, Guard.Activity.Rest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol",
		meta=(Categories="Guard.Activity", DisplayName="Activity Tag"))
	FGameplayTag ActivityTag;

	/** Order index — guards visit points in ascending order. Auto-sorted by TerritoryVolume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Patrol",
		meta=(DisplayName="Patrol Order"))
	int32 PatrolOrder = 0;

	/** Returns this actor's world transform as a patrol node location+rotation. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	FTransform GetPatrolTransform() const;

	/** Converts this point to an FTerritoryPatrolNode struct for compatibility with existing patrol systems. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	FTerritoryPatrolNode ToPatrolNode() const;

	/** Bind to a specific territory volume (called by TerritoryVolume at BeginPlay). */
	void BindToTerritory(class ATerritoryVolume* Territory);

	/** The territory this point is bound to. */
	UFUNCTION(BlueprintPure, Category="Territory|Patrol")
	class ATerritoryVolume* GetOwningTerritory() const;

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<class ATerritoryVolume> CachedTerritory;
};
