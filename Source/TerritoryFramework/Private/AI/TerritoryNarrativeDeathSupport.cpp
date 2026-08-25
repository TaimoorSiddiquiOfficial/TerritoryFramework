#include "AI/TerritoryNarrativeDeathSupport.h"

#include "AIController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

namespace TerritoryNarrativeDeathSupport
{
	bool ResolveDeathState(const UNarrativeAbilitySystemComponent* AbilitySystem,
		const bool bReportedIsDead)
	{
		// Narrative Pro 2.4.2 added bIsDead to the BlueprintNativeEvent. Existing
		// Blueprint-generated classes can dispatch a stale false value until every
		// dependent class is regenerated, while the ASC already owns the correct state.
		return AbilitySystem ? AbilitySystem->IsDead() : bReportedIsDead;
	}

	bool PrepareForRemoval(ANarrativeNPCCharacter& Character)
	{
		ANarrativeNPCController* Controller = Character.GetNPCController();
		UNPCActivityComponent* ActivityComponent = Character.GetActivityComponent();
		if (!IsValid(Controller) || !IsValid(ActivityComponent))
		{
			return false;
		}

		// Narrative's controller cleanup unpossesses immediately and destroys the
		// controller shortly afterwards. Deactivate first while Blueprint activities
		// can still safely run K2_EndActivity, then remove goals so target-death
		// delegates cannot rescore through that pending-kill controller later.
		ActivityComponent->Deactivate();
		ActivityComponent->RemoveAllGoals();
		Controller->StopMovement();
		return true;
	}

	void FinalizePhysicalDeath(ANarrativeNPCCharacter& Character)
	{
		if (Character.HasAuthority())
		{
			if (AAIController* Controller = Cast<AAIController>(Character.GetController()))
			{
				Controller->StopMovement();
			}
		}

		// Narrative replicates death and ragdoll independently. Clear the last
		// locomotion sample on simulated proxies as well so a dead NPC cannot keep
		// following its pre-death path while the ragdoll notification is applied.
		if (UCharacterMovementComponent* Movement = Character.GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		// Simulated proxies consume Narrative's replicated bIsRagdoll notification.
		// Calling SetRagdoll there would attempt ServerStartRagdoll without an owning
		// connection before applying the same local state.
		if (Character.HasAuthority() && !Character.IsRagdoll(false))
		{
			Character.SetRagdoll(true);
		}
	}
}
