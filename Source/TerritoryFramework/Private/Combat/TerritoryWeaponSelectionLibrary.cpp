#include "Combat/TerritoryWeaponSelectionLibrary.h"

#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/NarrativeNPCController.h"
#include "Combat/TerritoryCombatTargetPolicy.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

float UTerritoryWeaponSelectionLibrary::PreferMeleeAtCloseRange(
	const ANarrativeNPCController* OwnerController, const UNPCGoalItem* Goal,
	const UNPCActivity* MeleeActivity, float EligibleScore,
	float EnterMeleeDistance, float LeaveMeleeDistance)
{
	if (!FMath::IsFinite(EligibleScore)) return 0.f;
	if (EligibleScore <= 0.f) return EligibleScore;
	if (!IsValid(MeleeActivity)) return 0.f;
	const AActor* Target = TerritoryCombatTargetPolicy::ResolveEligibleTarget(OwnerController, Goal);
	if (!Target) return 0.f;
	// Installed Narrative BPA_Attack_Melee returns 0.2 for an unreachable
	// target. That is a low-priority fallback, not eligible close combat.
	// Preserve it when wrapping the Native parent instead of copying its graph.
	if (EligibleScore <= 0.2f) return EligibleScore;
	if (!FMath::IsFinite(EnterMeleeDistance) || !FMath::IsFinite(LeaveMeleeDistance)
		|| EnterMeleeDistance <= 0.f || LeaveMeleeDistance < EnterMeleeDistance) return EligibleScore;

	const UNPCActivityComponent* Activities = OwnerController->GetActivityComponent();
	const bool bContinuingMelee = Activities
		&& Activities->GetCurrentActivity() == MeleeActivity
		&& Activities->GetCurrentActivityGoal() == Goal;
	const double Radius = bContinuingMelee ? LeaveMeleeDistance : EnterMeleeDistance;
	const ANarrativeNPCCharacter* NPC = OwnerController->GetControlledNPC();
	const double DistanceSquared = FVector::DistSquared(NPC->GetActorLocation(), Target->GetActorLocation());
	// Native melee's eligible score is attack score - 0.1. Keep its normal
	// fallback everywhere else; add only the bounded near-target preference.
	return FMath::IsFinite(DistanceSquared) && DistanceSquared <= FMath::Square(Radius)
		? EligibleScore + 0.2f : EligibleScore;
}
