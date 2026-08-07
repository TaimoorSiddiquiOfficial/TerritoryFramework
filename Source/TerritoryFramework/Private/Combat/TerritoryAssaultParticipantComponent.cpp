#include "Combat/TerritoryAssaultParticipantComponent.h"

#include "AI/TerritoryAssaultActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UTerritoryAssaultParticipantComponent::UTerritoryAssaultParticipantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTerritoryAssaultParticipantComponent::Configure(
	const FGuid& InAssaultID, const FGameplayTag& InTargetTerritory,
	const FGameplayTag& InAttackingFaction)
{
	AssaultID = InAssaultID;
	TargetTerritory = InTargetTerritory;
	AttackingFaction = InAttackingFaction;
}

void UTerritoryAssaultParticipantComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Owner))
	{
		if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(
			AbilityOwner->GetAbilitySystemComponent()))
		{
			BoundASC = ASC;
			ASC->OnDied.AddUniqueDynamic(this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ParticipationTimer, this,
			&UTerritoryAssaultParticipantComponent::UpdateParticipation, 0.5f, true, 0.1f);
	}
}

void UTerritoryAssaultParticipantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipationTimer);
	}
	if (UNarrativeAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->OnDied.RemoveDynamic(this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
	}
	if (GetOwner() && GetOwner()->HasAuthority() && !bRemovalReported
		&& EndPlayReason != EEndPlayReason::EndPlayInEditor)
	{
		Retire(false);
	}
	Super::EndPlay(EndPlayReason);
}

void UTerritoryAssaultParticipantComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, AssaultID);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, TargetTerritory);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, AttackingFaction);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, bCaptureRegistered);
}

bool UTerritoryAssaultParticipantComponent::EnsureNarrativeActivityAndGoal()
{
	if (AssaultGoal) return true;
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	// Narrative applies the definition and appearance asynchronously. Do not start
	// goal selection while that authoritative initialization is incomplete. Equipment
	// visuals are intentionally not a movement prerequisite: unarmed NPC definitions
	// are valid, and a missing optional weapon visual must not deadlock the assault.
	if (!NPC || !NPC->IsNarrativeSpawnReady())
	{
		return false;
	}
	UNPCActivityComponent* ActivityComponent = NPC ? NPC->GetActivityComponent() : nullptr;
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	if (!ActivityComponent || !Controller)
	{
		return false;
	}

	UNPCActivity* Activity = ActivityComponent->GetActivity(UTerritoryAssaultActivity::StaticClass());
	if (!Activity)
	{
		Activity = ActivityComponent->AddActivity(UTerritoryAssaultActivity::StaticClass(), false);
	}
	if (!Activity) return false;

	ATerritoryVolume* Territory = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Territory = Registry->GetTerritoryByTag(TargetTerritory);
		}
	}
	if (!Territory) return false;

	UTerritoryAssaultGoal* NewGoal = NewObject<UTerritoryAssaultGoal>(ActivityComponent);
	NewGoal->GoalKey = this;
	NewGoal->AssaultID = AssaultID;
	NewGoal->TargetTerritoryTag = TargetTerritory;
	NewGoal->TargetTerritory = Territory;
	NewGoal->TargetLocation = Territory->GetTerritoryBounds().GetCenter();
	NewGoal->bSaveGoal = false;
	NewGoal->bRemoveOnSucceeded = false;
	AssaultGoal = Cast<UTerritoryAssaultGoal>(ActivityComponent->AddGoal(NewGoal, true));
	return AssaultGoal != nullptr;
}

void UTerritoryAssaultParticipantComponent::UpdateParticipation()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World || bRemovalReported) return;

	if (!EnsureNarrativeActivityAndGoal())
	{
		if (++GoalInitializationAttempts >= 40)
		{
			UE_LOG(LogTerritory, Error,
				TEXT("Assault participant %s could not initialize its Narrative goal/activity"),
				*GetNameSafe(Owner));
			Retire(false);
			Owner->Destroy();
		}
		return;
	}

	UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Counterattacks || !Counterattacks->IsAssaultActive(AssaultID))
	{
		Retire(false);
		Owner->Destroy();
		return;
	}

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	ATerritoryVolume* Territory = Registry ? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
	if (!Territory)
	{
		// The old weak-keyed capture state is pruned by the control subsystem when
		// a World Partition actor unloads. Clear our local read model so this NPC
		// can register against the replacement actor when it streams back in.
		bCaptureRegistered = false;
		return;
	}

	const bool bInsideTarget = Territory->ContainsPoint(Owner->GetActorLocation());
	if (!bInsideTarget)
	{
		UnregisterCapturePressure();
		return;
	}
	if (bCaptureRegistered) return;

	ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner);
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	UTerritoryCombatDirector* Director = World->GetSubsystem<UTerritoryCombatDirector>();
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	if (!Controller || !Director || !Control
		|| !Director->RequestAssaultSlot(Territory, Controller))
	{
		return;
	}

	bCaptureRegistered = Control->TryRegisterAttacker(Territory, Owner, AttackingFaction);
	if (!bCaptureRegistered)
	{
		Director->ReleaseAssaultSlot(Territory, Controller);
	}
}

void UTerritoryAssaultParticipantComponent::UnregisterCapturePressure()
{
	if (!bCaptureRegistered) return;
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	ATerritoryVolume* Territory = Registry ? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
	if (!Territory)
	{
		bCaptureRegistered = false;
		return;
	}
	if (Territory)
	{
		if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
		{
			Control->UnregisterAttacker(Territory, Owner, AttackingFaction);
		}
		if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			if (ANarrativeNPCController* Controller = NPC->GetNPCController())
			{
				if (UTerritoryCombatDirector* Director = World->GetSubsystem<UTerritoryCombatDirector>())
				{
					Director->ReleaseAssaultSlot(Territory, Controller);
				}
			}
		}
	}
	bCaptureRegistered = false;
}

void UTerritoryAssaultParticipantComponent::Retire(bool bKilled)
{
	if (bRemovalReported) return;
	bRemovalReported = true;
	UnregisterCapturePressure();

	if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		if (UNPCActivityComponent* ActivityComponent = NPC->GetActivityComponent())
		{
			if (AssaultGoal) ActivityComponent->RemoveGoal(AssaultGoal);
			ActivityComponent->RemoveActivity(UTerritoryAssaultActivity::StaticClass());
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryCounterAttackSubsystem* Counterattacks =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			Counterattacks->NotifyParticipantRemoved(
				AssaultID, Cast<ATerritoryAssaultCharacter>(GetOwner()), bKilled);
		}
	}
}

void UTerritoryAssaultParticipantComponent::HandleOwnerDied(
	AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC)
{
	(void)KilledActor;
	(void)KilledASC;
	Retire(true);
}
