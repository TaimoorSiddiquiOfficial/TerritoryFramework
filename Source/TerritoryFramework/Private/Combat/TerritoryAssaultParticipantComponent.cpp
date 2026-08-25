#include "Combat/TerritoryAssaultParticipantComponent.h"

#include "AI/TerritoryAssaultActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

UTerritoryAssaultParticipantComponent::UTerritoryAssaultParticipantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTerritoryAssaultParticipantComponent::Configure(
	const FGuid& InAssaultID, const FGuid& InTargetTerritoryGUID,
	const FGameplayTag& InTargetTerritory, const FGameplayTag& InAttackingFaction)
{
	AssaultID = InAssaultID;
	TargetTerritoryGUID = InTargetTerritoryGUID;
	TargetTerritory = InTargetTerritory;
	AttackingFaction = InAttackingFaction;
}

void UTerritoryAssaultParticipantComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ParticipationTimer, this,
			&UTerritoryAssaultParticipantComponent::UpdateParticipation, 0.5f, true, 0.1f);
	}
}

void UTerritoryAssaultParticipantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreNarrativeDefenderTargeting(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipationTimer);
	}
	if (UNarrativeAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->OnDeathStateChanged.RemoveDynamic(
			this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
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
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, TargetTerritoryGUID);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, TargetTerritory);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, AttackingFaction);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, bCaptureRegistered);
}

bool UTerritoryAssaultParticipantComponent::EnsureNarrativeActivityAndGoal()
{
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	// Narrative applies the definition and appearance asynchronously. Do not start
	// goal selection while that authoritative initialization is incomplete. Equipment
	// visuals are intentionally not a movement prerequisite: unarmed NPC definitions
	// are valid, and a missing optional weapon visual must not deadlock the assault.
	if (!NPC || !NPC->IsNarrativeSpawnReady())
	{
		return false;
	}
	// Narrative binds the character/controller death handlers while applying the
	// definition. Bind Territory casualty accounting only after that contract is
	// ready so ragdoll/death presentation runs before assault resolution.
	if (!BindNarrativeDeathAfterSpawnReady())
	{
		return false;
	}
	if (AssaultGoal) return true;
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

	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory) return false;

	UTerritoryAssaultGoal* NewGoal = NewObject<UTerritoryAssaultGoal>(ActivityComponent);
	NewGoal->GoalKey = this;
	NewGoal->AssaultID = AssaultID;
	NewGoal->TargetTerritoryGUID = TargetTerritoryGUID;
	NewGoal->TargetTerritoryTag = TargetTerritory;
	NewGoal->TargetTerritory = Territory;
	const TArray<FVector> InitialObjectives =
		TerritoryAssaultTargetPolicy::BuildObjectiveLocations(Territory, false);
	NewGoal->TargetLocation = InitialObjectives.IsEmpty()
		? Territory->GetTerritoryBounds().GetCenter() : InitialObjectives[0];
	NewGoal->bSaveGoal = false;
	NewGoal->bRemoveOnSucceeded = false;
	AssaultGoal = Cast<UTerritoryAssaultGoal>(ActivityComponent->AddGoal(NewGoal, true));
	return AssaultGoal != nullptr;
}

bool UTerritoryAssaultParticipantComponent::BindNarrativeDeathAfterSpawnReady()
{
	IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner());
	UNarrativeAbilitySystemComponent* ASC = AbilityOwner
		? Cast<UNarrativeAbilitySystemComponent>(AbilityOwner->GetAbilitySystemComponent())
		: nullptr;
	if (!ASC) return false;

	if (BoundASC.Get() != ASC)
	{
		if (UNarrativeAbilitySystemComponent* Previous = BoundASC.Get())
		{
			Previous->OnDeathStateChanged.RemoveDynamic(
				this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
		}
		BoundASC = ASC;
		ASC->OnDeathStateChanged.AddUniqueDynamic(
			this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
	}

	if (ASC->IsDead())
	{
		HandleOwnerDied(GetOwner(), ASC, true);
		return false;
	}
	return true;
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
			if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
			{
				TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
			}
			Owner->Destroy();
		}
		return;
	}

	UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Counterattacks || !Counterattacks->IsAssaultActive(AssaultID))
	{
		Retire(false);
		if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
		}
		Owner->Destroy();
		return;
	}

	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory)
	{
		// The old weak-keyed capture state is pruned by the control subsystem when
		// a World Partition actor unloads. Clear our local read model so this NPC
		// can register against the replacement actor when it streams back in.
		if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			if (ANarrativeNPCController* Controller = NPC->GetNPCController())
			{
				if (UTerritoryCombatDirector* Director =
					World->GetSubsystem<UTerritoryCombatDirector>())
				{
					Director->ReleaseAllSlots(Controller);
				}
			}
		}
		RestoreNarrativeDefenderTargeting(true);
		bCaptureRegistered = false;
		return;
	}

	const bool bInsideTarget = Territory->ContainsPoint(Owner->GetActorLocation());
	// Keep closing on a registered hostile defender after crossing the boundary.
	// Narrative combat remains authoritative; the Territory adapter only prevents a
	// non-defender combat goal from outranking a still-live registered defender.
	if (!MaintainAssaultMovement(Territory)) return;
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

bool UTerritoryAssaultParticipantComponent::MaintainAssaultMovement(
	ATerritoryVolume* Territory)
{
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	UNPCActivityComponent* ActivityComponent = NPC ? NPC->GetActivityComponent() : nullptr;
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	if (!NPC || !Territory || !ActivityComponent || !Controller || !AssaultGoal)
	{
		return true;
	}

	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const float RetryInterval = FMath::Clamp(
		Profile ? Profile->StalledMovementRetryInterval : 1.5f, 0.25f, 10.f);
	const int32 MaximumFailures = FMath::Clamp(
		Profile ? Profile->MaxStalledMovementRetries : 8, 1, 100);
	FVector DesiredTarget = Territory->GetTerritoryBounds().GetCenter();
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FVector& Objective :
		TerritoryAssaultTargetPolicy::BuildObjectiveLocations(Territory, false))
	{
		const float DistanceSquared = FVector::DistSquared2D(
			NPC->GetActorLocation(), Objective);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			DesiredTarget = Objective;
		}
	}
	TArray<AActor*> LiveHostileDefenders;
	float BestDefenderDistanceSquared = TNumericLimits<float>::Max();
	for (AActor* Defender :
		TerritoryAssaultTargetPolicy::CollectRegisteredDefenders(Territory))
	{
		if (!IsValid(Defender) || Defender == NPC || Defender->IsActorBeingDestroyed()) continue;
		if (IAbilitySystemInterface* AbilityDefender = Cast<IAbilitySystemInterface>(Defender))
		{
			if (UNarrativeAbilitySystemComponent* DefenderASC =
				Cast<UNarrativeAbilitySystemComponent>(AbilityDefender->GetAbilitySystemComponent()))
			{
				if (DefenderASC->IsDead()) continue;
			}
		}
		const INarrativeTeamAgentInterface* NarrativeTeam =
			Cast<INarrativeTeamAgentInterface>(NPC);
		if (!NarrativeTeam
			|| NarrativeTeam->GetTeamAttitudeTowards(*Defender) != ETeamAttitude::Hostile)
		{
			continue;
		}
		LiveHostileDefenders.Add(Defender);

		const float DistanceSquared = FVector::DistSquared2D(
			NPC->GetActorLocation(), Defender->GetActorLocation());
		if (DistanceSquared < BestDefenderDistanceSquared)
		{
			BestDefenderDistanceSquared = DistanceSquared;
			DesiredTarget = Defender->GetActorLocation();
		}
	}
	ReconcileNarrativeDefenderTargeting(ActivityComponent, LiveHostileDefenders);
	const bool bMovementTargetChanged = !AssaultGoal->TargetLocation.Equals(DesiredTarget, 200.f);
	AssaultGoal->TargetLocation = DesiredTarget;
	auto RecordFailedRestart = [this, NPC, Territory, MaximumFailures]()
	{
		if (++ConsecutiveMovementRestartFailures < MaximumFailures) return true;
		UE_LOG(LogTerritory, Error,
			TEXT("Assault participant %s withdrew after %d failed movement restarts toward %s"),
			*GetNameSafe(NPC), ConsecutiveMovementRestartFailures,
			*Territory->GetTerritoryTag().ToString());
		Retire(false);
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
		NPC->Destroy();
		return false;
	};

	UNPCGoalItem* CurrentGoal = ActivityComponent->GetCurrentActivityGoal();
	if (!CurrentGoal && Now >= NextMovementRestartTime)
	{
		NextMovementRestartTime = Now + RetryInterval;
		ActivityComponent->PerformActivitySelection(true);
		CurrentGoal = ActivityComponent->GetCurrentActivityGoal();
		if (!CurrentGoal) return RecordFailedRestart();
	}
	// A different current goal is Narrative combat or another deliberate activity.
	// Registered-defender combat remains authoritative and interrupts movement.
	if (CurrentGoal && CurrentGoal != AssaultGoal)
	{
		ConsecutiveMovementRestartFailures = 0;
		return true;
	}
	if (!CurrentGoal) return true;
	if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle && !bMovementTargetChanged)
	{
		ConsecutiveMovementRestartFailures = 0;
		return true;
	}

	if (Now < NextMovementRestartTime) return true;
	NextMovementRestartTime = Now + RetryInterval;

	const EPathFollowingRequestResult::Type Result = Controller->MoveToLocation(
		AssaultGoal->TargetLocation, 150.f, true, true, true, false, nullptr, true);
	if (Result != EPathFollowingRequestResult::Failed)
	{
		ConsecutiveMovementRestartFailures = 0;
		return true;
	}

	return RecordFailedRestart();
}

void UTerritoryAssaultParticipantComponent::ReconcileNarrativeDefenderTargeting(
	UNPCActivityComponent* ActivityComponent,
	TConstArrayView<AActor*> LiveHostileDefenders)
{
	if (!ActivityComponent)
	{
		RestoreNarrativeDefenderTargeting(false);
		return;
	}
	if (LiveHostileDefenders.IsEmpty())
	{
		RestoreNarrativeDefenderTargeting(true);
		return;
	}

	UNPCGoalItem* CurrentGoal = ActivityComponent->GetCurrentActivityGoal();
	if (!NarrativeAttackGoalClass.IsValid() && CurrentGoal && CurrentGoal != AssaultGoal)
	{
		const AActor* GoalTarget = Cast<AActor>(CurrentGoal->GetGoalKey());
		const ANarrativeCharacter* NarrativeTarget = Cast<ANarrativeCharacter>(GoalTarget);
		const INarrativeTeamAgentInterface* TeamAgent =
			Cast<INarrativeTeamAgentInterface>(GetOwner());
		if (NarrativeTarget && TeamAgent
			&& TeamAgent->GetTeamAttitudeTowards(*NarrativeTarget) == ETeamAttitude::Hostile)
		{
			// Narrative's attack goal exposes its target through GetGoalKey(). Cache
			// the configured concrete class instead of hard-coding a Narrative asset path.
			NarrativeAttackGoalClass = CurrentGoal->GetClass();
		}
	}

	UClass* GoalClass = NarrativeAttackGoalClass.Get();
	if (!GoalClass || !GoalClass->IsChildOf(UNPCGoalItem::StaticClass())) return;

	const FNPCGoalContainer AttackGoals = ActivityComponent->GetGoals(GoalClass);
	const FTerritoryDefenderGoalPreferenceResult Preference =
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(
			AttackGoals, LiveHostileDefenders, NarrativeGoalScoreOverrides);
	if (Preference.bScoresChanged)
	{
		// Reselect through Narrative so its configured activity, behavior tree,
		// perception goal, GAS combat, and tactical tokens remain authoritative.
		ActivityComponent->PerformActivitySelection(true);
	}
}

void UTerritoryAssaultParticipantComponent::RestoreNarrativeDefenderTargeting(
	bool bReselectActivity)
{
	if (!TerritoryAssaultTargetPolicy::RestoreGoalScores(NarrativeGoalScoreOverrides)) return;
	if (!bReselectActivity) return;

	if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		if (UNPCActivityComponent* ActivityComponent = NPC->GetActivityComponent())
		{
			ActivityComponent->PerformActivitySelection(true);
		}
	}
}

void UTerritoryAssaultParticipantComponent::UnregisterCapturePressure()
{
	if (!bCaptureRegistered) return;
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;
	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory)
	{
		if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			if (ANarrativeNPCController* Controller = NPC->GetNPCController())
			{
				if (UTerritoryCombatDirector* Director =
					World->GetSubsystem<UTerritoryCombatDirector>())
				{
					Director->ReleaseAllSlots(Controller);
				}
			}
		}
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

ATerritoryVolume* UTerritoryAssaultParticipantComponent::ResolveTargetTerritory() const
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;
	if (TargetTerritoryGUID.IsValid())
	{
		// Never let a streamed replacement inherit physical capture pressure merely
		// because it reused the old gameplay tag.
		return Registry->GetTerritoryByGUID(TargetTerritoryGUID);
	}
	return TargetTerritory.IsValid()
		? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
}

bool UTerritoryAssaultParticipantComponent::MatchesTargetTerritory(
	const ATerritoryVolume* Territory) const
{
	if (!Territory) return false;
	if (TargetTerritoryGUID.IsValid())
	{
		return TargetTerritoryGUID == Territory->GetTerritoryGUID();
	}
	return TargetTerritory.IsValid()
		&& TargetTerritory == Territory->GetTerritoryTag();
}

void UTerritoryAssaultParticipantComponent::Retire(bool bKilled)
{
	if (bRemovalReported) return;
	bRemovalReported = true;
	RestoreNarrativeDefenderTargeting(false);
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
	AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC, const bool bIsDead)
{
	if (!bIsDead)
	{
		return;
	}
	(void)KilledActor;
	(void)KilledASC;
	// Preserve Narrative's physical death presentation even if a project or test
	// changes delegate registration order. This call is idempotent and still uses
	// Narrative's replicated ragdoll authority.
	if (ATerritoryAssaultCharacter* NPC =
		Cast<ATerritoryAssaultCharacter>(GetOwner()))
	{
		NPC->ReconcileNarrativeDeathState(KilledASC, bIsDead);
	}
	Retire(true);
}
