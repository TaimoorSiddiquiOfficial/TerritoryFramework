#include "AI/TerritoryNPCController.h"
#include "AI/TerritoryNPCActivityComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

ATerritoryNPCController::ATerritoryNPCController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTerritoryNPCActivityComponent>(TEXT("NPCActivityComponent")))
{
}

ETeamAttitude::Type ATerritoryNPCController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// GetNarrativeCharacter(), not GetPawn(): it is the NPC this controller owns even while that NPC is
	// driving a vehicle, and it is the same character GetFactions() already answers for. A vehicle pawn
	// forwards attitude back to its controller (NarrativeVehicleBase.cpp:344), so consulting GetPawn()
	// here would recurse for a driving guard.
	//
	// The other direction cannot recurse: ANarrativeNPCCharacter::GetTeamAttitudeTowards computes from
	// its own factions and never delegates to a controller (NarrativeNPCCharacter.cpp:138).
	if (const ANarrativeCharacter* ControlledCharacter = GetNarrativeCharacter())
	{
		if (const INarrativeTeamAgentInterface* CharacterTeam = Cast<const INarrativeTeamAgentInterface>(ControlledCharacter))
		{
			return CharacterTeam->GetTeamAttitudeTowards(Other);
		}
	}

	// Nothing to defer to. The fallback is named fully qualified on purpose: AAIController inherits
	// GetTeamAttitudeTowards through IGenericTeamAgentInterface as well as through the Narrative
	// interface, so an unqualified call would be ambiguous - and this states which one is meant.
	return INarrativeTeamAgentInterface::GetTeamAttitudeTowards(Other);
}
