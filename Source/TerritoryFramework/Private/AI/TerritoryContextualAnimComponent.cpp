#include "AI/TerritoryContextualAnimComponent.h"

#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

void UTerritoryContextualAnimComponent::OnLeaveScene(const FContextualAnimSceneBinding& Binding)
{
	Super::OnLeaveScene(Binding);
	ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner());
	if (!IsValid(NPC) || !NPC->HasAuthority() || NPC->IsActorBeingDestroyed()
		|| !GetWorld() || GetWorld()->bIsTearingDown || !NPC->IsAlive() || NPC->GetController()) return;
	// Narrative's melee execution unpossesses the target. An interrupted execution
	// must resume the original activities, without creating a controller or stealing
	// one that has since possessed a vehicle/another pawn.
	ANarrativeNPCController* Controller = NPC->GetNPCController();
	if (IsValid(Controller) && Controller->GetOwnedNPC() == NPC && !Controller->GetPawn())
	{
		Controller->Possess(NPC);
	}
}
