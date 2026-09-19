#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class ATerritoryVolume;
class UNPCGoalItem;
struct FNPCGoalContainer;

/** One reversible, server-only score adjustment applied to a Narrative goal. */
struct TERRITORYFRAMEWORK_API FTerritoryNarrativeGoalScoreOverride
{
	TWeakObjectPtr<UNPCGoalItem> Goal;
	float OriginalScore = 0.f;
	float AppliedScore = 0.f;
};

/** Result of reconciling Narrative attack goals with the live Territory defenders. */
struct TERRITORYFRAMEWORK_API FTerritoryDefenderGoalPreferenceResult
{
	bool bScoresChanged = false;
	bool bHasRegisteredDefenderGoal = false;
	int32 SuppressedNonDefenderGoals = 0;
};

/**
 * Narrow adapter policy for Narrative attack goals during a physical assault.
 *
 * Narrative remains responsible for perception, hostility, attack activities,
 * behavior trees, GAS combat, and tactical attack tokens. This policy only keeps
 * hostile non-defender goals from outranking the Territory's live registered
 * defenders, and restores every transient score when that preference ends.
 */
namespace TerritoryAssaultTargetPolicy
{
	/** Exact target plus its same-owner District defence cascade, in stable order. */
	TERRITORYFRAMEWORK_API TArray<ATerritoryVolume*> BuildDefenceFront(
		ATerritoryVolume* TargetTerritory);

	/**
	 * Relative importance of a set of Territories: the highest authored value, or 0 when the set is
	 * empty or every value is zero.
	 *
	 * Relative importance is *intensive* — a District or a defence front is as valuable as its single
	 * most valuable member, not as valuable as the sum of them. Summing made a District of six trivial
	 * Places outrank one vital Place and made the number grow with how the map happened to be carved
	 * into Places rather than with how important the place actually is, which contradicts the
	 * property's own meaning ("higher values make it a more valuable target").
	 *
	 * Both the Command Center display and strategic assault planning call this, with the set each one
	 * owns: the display passes a District plus its contributing Places, planning passes a defence
	 * front. The *sets* legitimately differ — `BuildDefenceFront` excludes the District because a
	 * District is never a physical defender or assault objective — but the *rule* is stated once here
	 * so the two can never aggregate the same concept two different ways.
	 *
	 * Negative values are clamped away rather than folded into the maximum: the property declares
	 * ClampMin=0, so a negative is corrupt data, and letting it win would be worse than ignoring it.
	 */
	TERRITORYFRAMEWORK_API float AggregateStrategicValue(
		TConstArrayView<ATerritoryVolume*> Territories);

	/** Unique registered defenders from the complete local defence front. */
	TERRITORYFRAMEWORK_API TArray<AActor*> CollectRegisteredDefenders(
		ATerritoryVolume* TargetTerritory);

	/**
	 * Physical objective candidates: defence-front guards, patrol nodes or posts that
	 * overlap the exact target, then the target center. Strategic routes and physical
	 * participants consume this same view so planning cannot count guards that AI ignores.
	 */
	TERRITORYFRAMEWORK_API TArray<FVector> BuildObjectiveLocations(
		ATerritoryVolume* TargetTerritory, bool bIncludeRegisteredDefenders = true);

	/**
	 * Select a complete NavMesh-reachable objective. A stable slot can distribute
	 * participants across floors/posts. If no complete path exists, falls back to
	 * full 3D distance (never flat XY distance).
	 */
	TERRITORYFRAMEWORK_API bool SelectObjectiveLocation(
		AActor* Participant, TConstArrayView<FVector> Objectives,
		int32 StableSlot, bool bUseNavigation, bool bDistribute,
		FVector& OutObjective);

	TERRITORYFRAMEWORK_API bool IsGoalTargetingRegisteredDefender(
		const UNPCGoalItem* Goal, TConstArrayView<AActor*> RegisteredDefenders);

	TERRITORYFRAMEWORK_API FTerritoryDefenderGoalPreferenceResult ApplyDefenderPreference(
		const FNPCGoalContainer& NarrativeAttackGoals,
		TConstArrayView<AActor*> RegisteredDefenders,
		TArray<FTerritoryNarrativeGoalScoreOverride>& InOutOverrides,
		bool bSuppressNonDefenderGoalsWhenNoDefender = false);

	TERRITORYFRAMEWORK_API bool RestoreGoalScores(
		TArray<FTerritoryNarrativeGoalScoreOverride>& InOutOverrides);
}
