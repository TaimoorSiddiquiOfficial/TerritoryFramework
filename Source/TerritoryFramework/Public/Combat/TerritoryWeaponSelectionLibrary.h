#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryWeaponSelectionLibrary.generated.h"

class ANarrativeNPCController;
class UNPCActivity;
class UNPCGoalItem;

/** Preferences only. Narrative activities still validate weapons, perception,
 * reachability, equip items, move, and execute attacks. */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryWeaponSelectionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Wrap the Native BPA_Attack_Melee ScoreGoalItem return after checking
	 * available weapons. Native zero/negative scores and its unreachable fallback
	 * (0.2) are preserved. Nearby eligible melee beats the Native ranged
	 * score; the larger exit radius prevents switching on every rescore. Invalid
	 * and negative Native scores are never turned into valid combat goals.
	 * Units are centimetres. No targets, weapons or perception are created here. */
	UFUNCTION(BlueprintPure, Category="Territory|AI|Combat")
	static float PreferMeleeAtCloseRange(const ANarrativeNPCController* OwnerController,
		const UNPCGoalItem* Goal, const UNPCActivity* MeleeActivity, float EligibleScore,
		float EnterMeleeDistance = 300.f, float LeaveMeleeDistance = 450.f);
};
