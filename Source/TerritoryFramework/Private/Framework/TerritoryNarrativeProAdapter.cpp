#include "Framework/TerritoryNarrativeProAdapter.h"

#include "UnrealFramework/NarrativeGameState.h"

FString FTerritoryNarrativeProAdapter::MakeCanonicalFactionPairKey(
	const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	const FString NameA = FactionA.ToString();
	const FString NameB = FactionB.ToString();
	return NameA < NameB ? NameA + TEXT("|") + NameB : NameB + TEXT("|") + NameA;
}

TArray<FTerritoryNarrativeAttitudeSnapshot> FTerritoryNarrativeProAdapter::ReadFactionAttitudes(
	const ANarrativeGameState* GameState)
{
	TMap<FString, FTerritoryNarrativeAttitudeSnapshot> ByPair;
	if (!GameState) return {};

	// FactionAllianceMap is currently the only public API that enumerates every
	// configured relationship. Keep this vendor representation dependency here.
	for (const auto& SourcePair : GameState->FactionAllianceMap)
	{
		const FGameplayTag& SourceFaction = SourcePair.Key;
		for (const auto& TargetPair : SourcePair.Value.AttitudeMap)
		{
			const FGameplayTag& TargetFaction = TargetPair.Key;
			if (!SourceFaction.IsValid() || !TargetFaction.IsValid()
				|| SourceFaction == TargetFaction)
			{
				continue;
			}

			const FString Key = MakeCanonicalFactionPairKey(SourceFaction, TargetFaction);
			FTerritoryNarrativeAttitudeSnapshot* Existing = ByPair.Find(Key);
			const ETeamAttitude::Type DirectionalAttitude = TargetPair.Value.GetValue();
			if (!Existing)
			{
				FTerritoryNarrativeAttitudeSnapshot Snapshot;
				Snapshot.FactionA = SourceFaction;
				Snapshot.FactionB = TargetFaction;
				Snapshot.Attitude = DirectionalAttitude;
				ByPair.Add(Key, Snapshot);
				continue;
			}

			// Lossless conservative merge for asymmetric Narrative data: hostility wins,
			// then friendliness, otherwise neutral.
			if (Existing->Attitude == ETeamAttitude::Hostile
				|| DirectionalAttitude == ETeamAttitude::Hostile)
			{
				Existing->Attitude = ETeamAttitude::Hostile;
			}
			else if (Existing->Attitude == ETeamAttitude::Friendly
				|| DirectionalAttitude == ETeamAttitude::Friendly)
			{
				Existing->Attitude = ETeamAttitude::Friendly;
			}
			else
			{
				Existing->Attitude = ETeamAttitude::Neutral;
			}
		}
	}

	TArray<FString> Keys;
	ByPair.GetKeys(Keys);
	Keys.Sort();
	TArray<FTerritoryNarrativeAttitudeSnapshot> Result;
	Result.Reserve(Keys.Num());
	for (const FString& Key : Keys)
	{
		Result.Add(ByPair.FindChecked(Key));
	}
	return Result;
}

void FTerritoryNarrativeProAdapter::SetSymmetricFactionAttitude(
	ANarrativeGameState* GameState, const FGameplayTag& FactionA,
	const FGameplayTag& FactionB, ETeamAttitude::Type Attitude)
{
	if (!GameState || !FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;
	GameState->SetFactionAttitude(FactionA, FactionB, Attitude);
	GameState->SetFactionAttitude(FactionB, FactionA, Attitude);
}
