#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "TerritoryPatrolGoal.generated.h"

/** Narrative goal instance populated from a territory guard's assigned spawn point. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryPatrolGoal : public UNPCGoalItem
{
	GENERATED_BODY()

public:
	UTerritoryPatrolGoal(const FObjectInitializer& ObjectInitializer);

	/** Ordered patrol stops copied from the guard's assigned post; each stop provides a location, wait time and optional activity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Territory|Patrol")
	TArray<FTerritoryPatrolNode> TerritoryPatrol;

	/**
	 * Score returned while the owning guard is outside its authored leash.
	 *
	 * Above the patrol's default 1.0 so the patrol activity reclaims a guard that chased something
	 * out of the Place, and low enough that a deliberate activity score can still outrank it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Patrol",
		meta=(ClampMin="0.0", UIMax="20.0",
			ToolTip="Patrol goal score while the guard is farther from its post than the Place's Maximum Chase Distance From Home."))
	float LeashReturnScore = 6.f;

	virtual float GetGoalScore_Implementation() const override;
	virtual FString GetDebugString_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;

	/**
	 * Delivers the Spot authored on one of this goal's patrol nodes to the owning guard.
	 *
	 * The patrol activity calls this after completing a node's move, rotation and wait, passing
	 * the completed node's index captured before selecting its next stop. The node is selected from THIS
	 * array rather than from a guard-side accessor. The two agree only while a route has a single
	 * node; once it is staggered they are different arrays in a different order, and reading the
	 * wrong one delivers the wrong node's spot, silently.
	 *
	 * Returns true only when a goal was actually delivered. False is the ordinary answer for a node
	 * that authors no spot, or for a goal whose pawn has already gone.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Patrol")
	bool DeliverPatrolSpot(int32 PatrolIdx);
};
