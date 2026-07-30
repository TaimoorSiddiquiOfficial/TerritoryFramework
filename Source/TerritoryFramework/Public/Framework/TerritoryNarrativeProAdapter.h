#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"

class ANarrativeGameState;

/** One unordered Narrative faction-pair attitude, normalized for Territory use. */
struct TERRITORYFRAMEWORK_API FTerritoryNarrativeAttitudeSnapshot
{
	FGameplayTag FactionA;
	FGameplayTag FactionB;
	ETeamAttitude::Type Attitude = ETeamAttitude::Neutral;
};

/**
 * Narrow, Territory-owned compatibility seam around Narrative Pro faction APIs.
 * Vendor representation access is intentionally isolated here so monthly Narrative
 * upgrades cannot leak implementation coupling throughout TerritoryFramework.
 */
class TERRITORYFRAMEWORK_API FTerritoryNarrativeProAdapter final
{
public:
	static TArray<FTerritoryNarrativeAttitudeSnapshot> ReadFactionAttitudes(
		const ANarrativeGameState* GameState);

	static void SetSymmetricFactionAttitude(ANarrativeGameState* GameState,
		const FGameplayTag& FactionA, const FGameplayTag& FactionB,
		ETeamAttitude::Type Attitude);

	static FString MakeCanonicalFactionPairKey(
		const FGameplayTag& FactionA, const FGameplayTag& FactionB);
};
