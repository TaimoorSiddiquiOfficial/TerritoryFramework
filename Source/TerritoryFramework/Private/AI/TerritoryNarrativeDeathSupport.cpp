#include "AI/TerritoryNarrativeDeathSupport.h"

#include "AIController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

namespace TerritoryNarrativeDeathSupport
{
	void DetachTargetGoals(ANarrativeNPCCharacter& Character)
	{
		if (!Character.HasAuthority() || !Character.GetWorld() || Character.GetWorld()->bIsTearingDown) return;
		UNarrativeAbilitySystemComponent* ASC = Character.GetNarrativeAbilitySystemComponent();
		// Real death already notifies these Native goals. This adapter is only for
		// removal while alive; do not force an extra selection inside a death broadcast.
		if (!IsValid(ASC) || ASC->IsDead()) return;

		// Native Goal_Attack binds the target's death delegate and exposes the target
		// through GetGoalKey. Retirement/stream-out is not a death, so detach those
		// goals explicitly without fabricating an ASC death or clearing unrelated AI.
		// Copy the binding list before RemoveGoal invokes Blueprint OnRemoved.
		const TArray<UObject*> Listeners = ASC->OnDeathStateChanged.GetAllObjects();
		for (UObject* Listener : Listeners)
		{
			UNPCGoalItem* Goal = Cast<UNPCGoalItem>(Listener);
			if (!IsValid(Goal) || Goal->GetGoalKey() != &Character) continue;
			ANarrativeNPCController* Owner = Goal->OwnerController;
			if (IsValid(Owner) && Owner->HasAuthority() && Owner->GetWorld() == Character.GetWorld())
			{
				if (UNPCActivityComponent* Activity = Owner->GetActivityComponent(); IsValid(Activity))
				{
					bool bFound = false;
					// Old removed goals can still have a weak death binding. Never let
					// one erase the keyed entry of a newer goal for this same target.
					if (Activity->GetGoalByKey(Goal->GetClass(), &Character, bFound) == Goal && bFound)
						Activity->RemoveGoal(Goal);
				}
			}
			// Native OnRemoved clears timers, but retains this death binding.
			if (IsValid(ASC)) ASC->OnDeathStateChanged.RemoveAll(Goal);
		}
	}

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
		DetachTargetGoals(Character);
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
		if (IsValid(ActivityComponent)) ActivityComponent->RemoveAllGoals();
		if (IsValid(Controller) && !Controller->IsActorBeingDestroyed()) Controller->StopMovement();
		return true;
	}

	void ScheduleRemoval(ANarrativeNPCCharacter& Character,
		const float LatentCleanupGraceSeconds)
	{
		PrepareForRemoval(Character);
		if (!IsValid(&Character) || Character.IsActorBeingDestroyed()) return;
		Character.SetActorEnableCollision(false);
		Character.SetActorHiddenInGame(true);
		if (UCharacterMovementComponent* Movement = Character.GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}

		// Narrative goal-generator Blueprints may still own short Delay nodes. Keeping
		// the actor/component valid for one bounded grace window lets those continuations
		// query the now-empty goal set instead of dereferencing a pending-kill component.
		Character.SetLifeSpan(FMath::Max(0.1f, LatentCleanupGraceSeconds));
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
