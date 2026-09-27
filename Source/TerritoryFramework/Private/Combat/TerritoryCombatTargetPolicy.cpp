#include "Combat/TerritoryCombatTargetPolicy.h"

#include "AI/Activities/NPCGoalItem.h"
#include "AI/NarrativeNPCController.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryVolume.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

const AActor* TerritoryCombatTargetPolicy::ResolveEligibleTarget(
	const ANarrativeNPCController* Controller, const UNPCGoalItem* Goal)
{
	if (!IsValid(Controller) || !Controller->HasAuthority() || Controller->IsActorBeingDestroyed()
		|| !IsValid(Goal) || Goal->OwnerController != Controller) return nullptr;
	const ANarrativeNPCCharacter* NPC = Controller->GetControlledNPC();
	// GetGoalKey can execute project Blueprint. Never call it twice for one admission.
	const AActor* Target = Cast<AActor>(Goal->GetGoalKey());
	const auto StillValid = [&]()
	{
		return IsValid(Controller) && !Controller->IsActorBeingDestroyed() && Controller->HasAuthority()
			&& IsValid(Goal) && Goal->OwnerController == Controller
			&& IsValid(NPC) && NPC->HasAuthority() && NPC->IsAlive() && !NPC->IsActorBeingDestroyed()
			&& Controller->GetControlledNPC() == NPC && IsValid(Target) && Target != NPC
			&& !Target->IsActorBeingDestroyed() && Target->GetWorld() == NPC->GetWorld()
			&& (!Cast<ANarrativeCharacter>(Target) || CastChecked<ANarrativeCharacter>(Target)->IsAlive());
	};
	if (!StillValid()) return nullptr;
	bool bAllowed = true;
	if (const auto* Assault = Cast<ATerritoryAssaultCharacter>(NPC))
	{
		const UTerritoryAssaultParticipantComponent* Participant = Assault->AssaultParticipant;
		bAllowed = IsValid(Participant) && !Participant->HasRetired()
			&& IsValid(Participant->GetTargetTerritory()) && Assault->CanEngageAssaultTarget(Target);
	}
	else if (const auto* Guard = Cast<ATerritoryGuardCharacter>(NPC))
	{
		bAllowed = Guard->CanEngageTerritoryTarget(Target);
	}
	return bAllowed && StillValid() ? Target : nullptr;
}
