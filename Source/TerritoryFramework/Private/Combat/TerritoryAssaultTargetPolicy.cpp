#include "Combat/TerritoryAssaultTargetPolicy.h"

#include "AI/Activities/NPCGoalItem.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "GameFramework/Actor.h"

namespace
{
	bool ContainsGoal(const FNPCGoalContainer& Container, const UNPCGoalItem* Goal)
	{
		return Container.Goals.Contains(Goal);
	}

	void RestoreOverride(FTerritoryNarrativeGoalScoreOverride& Override)
	{
		if (UNPCGoalItem* Goal = Override.Goal.Get())
		{
			// Preserve a newer Narrative-authored score if its generator changed the
			// goal while the Territory preference was active.
			if (FMath::IsNearlyEqual(Goal->DefaultScore, Override.AppliedScore))
			{
				Goal->DefaultScore = Override.OriginalScore;
			}
		}
	}
}

TArray<ATerritoryVolume*> TerritoryAssaultTargetPolicy::BuildDefenceFront(
	ATerritoryVolume* TargetTerritory)
{
	TArray<ATerritoryVolume*> Result;
	if (!TargetTerritory) return Result;
	Result.Add(TargetTerritory);

	TArray<ATerritoryVolume*> Additional;
	if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(TargetTerritory))
	{
		Additional = District->GetProperties();
	}
	else if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(TargetTerritory))
	{
		if (ATerritoryDistrict* OwningDistrict = Property->GetOwningDistrict())
		{
			Additional.Add(OwningDistrict);
			Additional.Append(OwningDistrict->GetProperties());
		}
	}

	Additional.Sort([](const ATerritoryVolume& A, const ATerritoryVolume& B)
	{
		return A.GetTerritoryTag().ToString() < B.GetTerritoryTag().ToString();
	});
	for (ATerritoryVolume* Candidate : Additional)
	{
		Result.AddUnique(Candidate);
	}
	return Result;
}

TArray<AActor*> TerritoryAssaultTargetPolicy::CollectRegisteredDefenders(
	ATerritoryVolume* TargetTerritory)
{
	TArray<AActor*> Result;
	if (!TargetTerritory) return Result;
	const FGameplayTag DefendingFaction = TargetTerritory->GetOwningFaction();
	for (ATerritoryVolume* Defence : BuildDefenceFront(TargetTerritory))
	{
		if (!Defence || (Defence != TargetTerritory
			&& Defence->GetOwningFaction() != DefendingFaction))
		{
			continue;
		}
		for (AActor* Defender : Defence->GetRegisteredDefenders())
		{
			if (IsValid(Defender) && !Defender->IsActorBeingDestroyed())
			{
				Result.AddUnique(Defender);
			}
		}
	}
	Result.Sort([](const AActor& A, const AActor& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	return Result;
}

TArray<FVector> TerritoryAssaultTargetPolicy::BuildObjectiveLocations(
	ATerritoryVolume* TargetTerritory, bool bIncludeRegisteredDefenders)
{
	TArray<FVector> Result;
	if (!TargetTerritory) return Result;

	if (bIncludeRegisteredDefenders)
	{
		for (const AActor* Defender : CollectRegisteredDefenders(TargetTerritory))
		{
			if (Defender) Result.AddUnique(Defender->GetActorLocation());
		}
	}

	const FGameplayTag DefendingFaction = TargetTerritory->GetOwningFaction();
	TArray<ATerritoryGuardSpawnPoint*> Posts;
	for (ATerritoryVolume* Defence : BuildDefenceFront(TargetTerritory))
	{
		if (!Defence || (Defence != TargetTerritory
			&& Defence->GetOwningFaction() != DefendingFaction))
		{
			continue;
		}
		for (ATerritoryGuardSpawnPoint* Post : Defence->GetGuardSpawnPoints())
		{
			if (IsValid(Post)) Posts.AddUnique(Post);
		}
	}
	Posts.Sort([](const ATerritoryGuardSpawnPoint& A,
		const ATerritoryGuardSpawnPoint& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	for (const ATerritoryGuardSpawnPoint* Post : Posts)
	{
		for (const FTerritoryPatrolNode& Node : Post->GetEffectivePatrolRoute())
		{
			if (TargetTerritory->ContainsPoint(Node.Location))
			{
				Result.AddUnique(Node.Location);
			}
		}
		if (TargetTerritory->ContainsPoint(Post->GetActorLocation()))
		{
			Result.AddUnique(Post->GetActorLocation());
		}
	}

	Result.AddUnique(TargetTerritory->GetTerritoryBounds().GetCenter());
	return Result;
}

bool TerritoryAssaultTargetPolicy::IsGoalTargetingRegisteredDefender(
	const UNPCGoalItem* Goal, TConstArrayView<AActor*> RegisteredDefenders)
{
	const AActor* Target = Goal ? Cast<AActor>(Goal->GetGoalKey()) : nullptr;
	return IsValid(Target) && RegisteredDefenders.Contains(Target);
}

FTerritoryDefenderGoalPreferenceResult
TerritoryAssaultTargetPolicy::ApplyDefenderPreference(
	const FNPCGoalContainer& NarrativeAttackGoals,
	TConstArrayView<AActor*> RegisteredDefenders,
	TArray<FTerritoryNarrativeGoalScoreOverride>& InOutOverrides)
{
	FTerritoryDefenderGoalPreferenceResult Result;
	if (RegisteredDefenders.IsEmpty())
	{
		Result.bScoresChanged = RestoreGoalScores(InOutOverrides);
		return Result;
	}

	// Restore goals that disappeared from Narrative's container or became valid
	// Territory defender targets since the preceding reconciliation.
	for (int32 Index = InOutOverrides.Num() - 1; Index >= 0; --Index)
	{
		FTerritoryNarrativeGoalScoreOverride& Override = InOutOverrides[Index];
		UNPCGoalItem* Goal = Override.Goal.Get();
		if (!Goal || !ContainsGoal(NarrativeAttackGoals, Goal)
			|| IsGoalTargetingRegisteredDefender(Goal, RegisteredDefenders))
		{
			RestoreOverride(Override);
			InOutOverrides.RemoveAtSwap(Index);
			Result.bScoresChanged = true;
		}
	}

	for (UNPCGoalItem* Goal : NarrativeAttackGoals.Goals)
	{
		if (!IsValid(Goal)) continue;
		if (IsGoalTargetingRegisteredDefender(Goal, RegisteredDefenders))
		{
			Result.bHasRegisteredDefenderGoal = true;
			continue;
		}

		++Result.SuppressedNonDefenderGoals;
		FTerritoryNarrativeGoalScoreOverride* Existing =
			InOutOverrides.FindByPredicate([Goal](const FTerritoryNarrativeGoalScoreOverride& Entry)
			{
				return Entry.Goal.Get() == Goal;
			});
		if (!Existing)
		{
			FTerritoryNarrativeGoalScoreOverride& Added = InOutOverrides.AddDefaulted_GetRef();
			Added.Goal = Goal;
			Added.OriginalScore = Goal->DefaultScore;
			Added.AppliedScore = 0.f;
			Goal->DefaultScore = Added.AppliedScore;
			Result.bScoresChanged = true;
		}
		else if (!FMath::IsNearlyEqual(Goal->DefaultScore, Existing->AppliedScore))
		{
			// Narrative refreshed this goal. Preserve its new authored value for the
			// eventual restore, then reapply the temporary defender preference.
			Existing->OriginalScore = Goal->DefaultScore;
			Goal->DefaultScore = Existing->AppliedScore;
			Result.bScoresChanged = true;
		}
	}

	return Result;
}

bool TerritoryAssaultTargetPolicy::RestoreGoalScores(
	TArray<FTerritoryNarrativeGoalScoreOverride>& InOutOverrides)
{
	const bool bHadOverrides = !InOutOverrides.IsEmpty();
	for (FTerritoryNarrativeGoalScoreOverride& Override : InOutOverrides)
	{
		RestoreOverride(Override);
	}
	InOutOverrides.Reset();
	return bHadOverrides;
}
