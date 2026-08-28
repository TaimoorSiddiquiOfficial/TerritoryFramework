#pragma once

#include "CoreMinimal.h"

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
		TArray<FTerritoryNarrativeGoalScoreOverride>& InOutOverrides);

	TERRITORYFRAMEWORK_API bool RestoreGoalScores(
		TArray<FTerritoryNarrativeGoalScoreOverride>& InOutOverrides);
}
