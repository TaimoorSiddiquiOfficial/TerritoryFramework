#include "Tales/TerritoryAIObservationTask.h"

#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "Tales/TerritoryTalesUtilities.h"

void UTerritoryAIObservationTask::BeginTask()
{
	// Transition objectives are edge-triggered. Reset their observation latches
	// before Super performs Narrative's immediate first tick on branch re-entry.
	ResetObservation();
	bObservedUnavailableTarget = false;
	TickInterval = 0.2f;
	Super::BeginTask();
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	if (TargetProvider)
	{
		TargetProvider->OnProviderActorReady.AddUniqueDynamic(
			this, &UTerritoryAIObservationTask::HandleTargetReady);
	}
	if (DestinationProvider)
	{
		DestinationProvider->OnProviderActorReady.AddUniqueDynamic(
			this, &UTerritoryAIObservationTask::HandleDestinationReady);
	}
	CachedDestination = ResolveDestination();
	if (AActor* Target = ResolveTarget()) BindTarget(Target);
	else bObservedUnsatisfiedState = true;
	Evaluate(true);
}

void UTerritoryAIObservationTask::EndTask()
{
	if (TargetProvider)
	{
		TargetProvider->OnProviderActorReady.RemoveDynamic(
			this, &UTerritoryAIObservationTask::HandleTargetReady);
	}
	if (DestinationProvider)
	{
		DestinationProvider->OnProviderActorReady.RemoveDynamic(
			this, &UTerritoryAIObservationTask::HandleDestinationReady);
	}
	UnbindTarget();
	CachedDestination.Reset();
	ResetObservation();
	bObservedUnavailableTarget = false;
	Super::EndTask();
}

void UTerritoryAIObservationTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	BindTarget(ResolveTarget());
	AActor* Destination = ResolveDestination();
	if (CachedDestination.Get() != Destination)
	{
		CachedDestination = Destination;
		if (Objective == ETerritoryAIObservationObjective::ReachActor) bObservedUnsatisfiedState = false;
	}
	Evaluate(false);
}

bool UTerritoryAIObservationTask::IsAIStateSatisfiedBy(
	const AActor* Target) const
{
	if (!IsValid(Target)) return false;
	ANarrativeNPCController* Controller = ResolveController(Target);
	AActor* SpatialActor = ResolveSpatialActor(Target);
	switch (Objective)
	{
	case ETerritoryAIObservationObjective::ActorAvailable:
		return true;
	case ETerritoryAIObservationObjective::NPCAlive:
	{
		UNarrativeAbilitySystemComponent* AbilitySystem =
			ResolveNarrativeAbilitySystem(const_cast<AActor*>(Target));
		return Controller ? Controller->IsAlive()
			: (AbilitySystem && !AbilitySystem->IsDead());
	}
	case ETerritoryAIObservationObjective::NPCDead:
	{
		UNarrativeAbilitySystemComponent* AbilitySystem =
			ResolveNarrativeAbilitySystem(const_cast<AActor*>(Target));
		return AbilitySystem && AbilitySystem->IsDead();
	}
	case ETerritoryAIObservationObjective::ReachQuestOwner:
	{
		APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
		return SpatialActor && Pawn && SpatialActor->GetDistanceTo(Pawn) <= DistanceTolerance;
	}
	case ETerritoryAIObservationObjective::ReachActor:
	{
		AActor* Destination = ResolveDestination();
		return SpatialActor && Destination
			&& SpatialActor->GetDistanceTo(Destination) <= DistanceTolerance;
	}
	case ETerritoryAIObservationObjective::ReachLocation:
		return SpatialActor
			&& FVector::Dist(SpatialActor->GetActorLocation(), DestinationLocation)
				<= DistanceTolerance;
	case ETerritoryAIObservationObjective::HasGoalClass:
		return Controller && GoalClass
			&& Controller->GetActivityComponent()
			&& Controller->GetActivityComponent()->HasGoal(GoalClass);
	case ETerritoryAIObservationObjective::RunsActivityClass:
	{
		const UNPCActivity* Activity = Controller && Controller->GetActivityComponent()
			? Controller->GetActivityComponent()->GetCurrentActivity() : nullptr;
		return Activity && ActivityClass && Activity->IsA(ActivityClass);
	}
	case ETerritoryAIObservationObjective::PerceivesQuestOwner:
		return IsPerceivingQuestOwner(Controller);
	case ETerritoryAIObservationObjective::LosesQuestOwner:
		return Controller && !IsPerceivingQuestOwner(Controller);
	case ETerritoryAIObservationObjective::EntersVehicle:
		return IsControllingVehicle(Controller);
	case ETerritoryAIObservationObjective::LeavesVehicle:
		return HasReturnedToOwnedNPC(Controller);
	case ETerritoryAIObservationObjective::ClaimsAttackToken:
		return HasAttackToken(Controller);
	case ETerritoryAIObservationObjective::ReleasesAttackToken:
		return Controller && !HasAttackToken(Controller);
	default:
		return false;
	}
}

AActor* UTerritoryAIObservationTask::ResolveTarget() const
{
	return TargetProvider ? TargetProvider->ProvideActor(this) : nullptr;
}

AActor* UTerritoryAIObservationTask::ResolveDestination() const
{
	return DestinationProvider
		? DestinationProvider->ProvideActor(this) : nullptr;
}

ANarrativeNPCController* UTerritoryAIObservationTask::ResolveController(
	const AActor* Target) const
{
	if (ANarrativeNPCController* Controller =
		Cast<ANarrativeNPCController>(const_cast<AActor*>(Target)))
	{
		return IsValid(Controller) ? Controller : nullptr;
	}
	const APawn* Pawn = Cast<APawn>(Target);
	ANarrativeNPCController* Controller = Pawn
		? Cast<ANarrativeNPCController>(Pawn->GetController()) : nullptr;
	return IsValid(Controller) ? Controller : nullptr;
}

AActor* UTerritoryAIObservationTask::ResolveSpatialActor(
	const AActor* Target) const
{
	if (ANarrativeNPCController* Controller = ResolveController(Target))
	{
		if (Controller->GetPawn()) return Controller->GetPawn();
		if (Controller->GetOwnedNPC()) return Controller->GetOwnedNPC();
	}
	return const_cast<AActor*>(Target);
}

UNarrativeAbilitySystemComponent*
UTerritoryAIObservationTask::ResolveNarrativeAbilitySystem(AActor* Target) const
{
	if (!Target) return nullptr;
	AActor* AbilityOwner = Target;
	if (ANarrativeNPCController* Controller = ResolveController(Target))
	{
		if (Controller->GetOwnedNPC()) AbilityOwner = Controller->GetOwnedNPC();
	}
	return FTerritoryNarrativeProAdapter::ResolveAbilitySystem(AbilityOwner);
}

bool UTerritoryAIObservationTask::IsPerceivingQuestOwner(
	const ANarrativeNPCController* Controller) const
{
	APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
	if (!Controller || !Pawn) return false;
	UAIPerceptionComponent* Perception =
		const_cast<ANarrativeNPCController*>(Controller)->GetAIPerceptionComponent();
	if (!IsValid(Perception)) return false;
	FActorPerceptionBlueprintInfo Info;
	if (!Perception->GetActorsPerception(Pawn, Info)) return false;
	for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
	{
		if (Stimulus.WasSuccessfullySensed()) return true;
	}
	return false;
}

bool UTerritoryAIObservationTask::IsControllingVehicle(
	const ANarrativeNPCController* Controller) const
{
	return Controller && Controller->GetOwnedNPC() && Controller->GetPawn()
		&& Controller->GetPawn() != Controller->GetOwnedNPC();
}

bool UTerritoryAIObservationTask::HasReturnedToOwnedNPC(
	const ANarrativeNPCController* Controller) const
{
	return Controller && Controller->GetOwnedNPC()
		&& Controller->GetPawn() == Controller->GetOwnedNPC();
}

bool UTerritoryAIObservationTask::HasAttackToken(
	const ANarrativeNPCController* Controller) const
{
	APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
	if (!Controller || !Pawn) return false;
	const UNarrativeAbilitySystemComponent* PlayerASC =
		FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Pawn);
	if (!PlayerASC) return false;
	return PlayerASC->GrantedAttackTokens.ContainsByPredicate(
		[Controller](const FAttackToken& Token)
		{
			return Token.Owner == Controller;
		});
}

void UTerritoryAIObservationTask::BindTarget(AActor* Target)
{
	UNarrativeAbilitySystemComponent* AbilitySystem = ResolveNarrativeAbilitySystem(Target);
	ANarrativeNPCController* Controller = ResolveController(Target);
	if (IsValid(Target) && CachedTarget.Get() == Target
		&& CachedAbilitySystem.Get() == AbilitySystem && CachedController.Get() == Controller) return;
	UnbindTarget();
	if (!IsValid(Target)) return;
	CachedTarget = Target;
	CachedController = Controller;
	CachedAbilitySystem = AbilitySystem;
	if (AbilitySystem)
	{
		AbilitySystem->OnDeathStateChanged.AddUniqueDynamic(
			this, &UTerritoryAIObservationTask::HandleDeathStateChanged);
	}
}

void UTerritoryAIObservationTask::UnbindTarget()
{
	if (UNarrativeAbilitySystemComponent* AbilitySystem =
		CachedAbilitySystem.Get())
	{
		AbilitySystem->OnDeathStateChanged.RemoveDynamic(
			this, &UTerritoryAIObservationTask::HandleDeathStateChanged);
	}
	CachedAbilitySystem.Reset();
	CachedTarget.Reset();
	CachedController.Reset();
	ResetObservation();
}

void UTerritoryAIObservationTask::ResetObservation()
{
	bObservedPerception = false;
	bObservedVehicle = false;
	bObservedAttackToken = false;
	bObservedUnsatisfiedState = false;
	ObservedQuestPawn.Reset();
}

void UTerritoryAIObservationTask::Evaluate(bool bInitialEvaluation)
{
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	APawn* Pawn = TerritoryTales::ResolveTaskPawn(OwningComp, OwningPawn, OwningController);
	if (ObservedQuestPawn.Get() != Pawn)
	{
		ObservedQuestPawn = Pawn;
		bObservedPerception = false;
		bObservedAttackToken = false;
		if (Objective == ETerritoryAIObservationObjective::ReachQuestOwner
			|| Objective == ETerritoryAIObservationObjective::PerceivesQuestOwner
			|| Objective == ETerritoryAIObservationObjective::ClaimsAttackToken) bObservedUnsatisfiedState = false;
	}
	AActor* Target = CachedTarget.Get();
	if (!Target)
	{
		bObservedUnavailableTarget = true;
		return;
	}
	if (Objective == ETerritoryAIObservationObjective::ActorAvailable && bObservedUnavailableTarget)
		bObservedUnsatisfiedState = true;
	ANarrativeNPCController* Controller = ResolveController(Target);
	const bool bPerceiving = IsPerceivingQuestOwner(Controller);
	const bool bInVehicle = IsControllingVehicle(Controller);
	const bool bHasToken = HasAttackToken(Controller);

	if (Objective == ETerritoryAIObservationObjective::LosesQuestOwner)
	{
		if (!Controller || !Pawn) return;
		const bool bLost = bObservedPerception && !bPerceiving;
		bObservedPerception |= bPerceiving;
		if (bLost) CompleteTask();
		return;
	}
	if (Objective == ETerritoryAIObservationObjective::LeavesVehicle)
	{
		if (!Controller) return;
		const bool bLeft = bObservedVehicle && HasReturnedToOwnedNPC(Controller);
		bObservedVehicle |= bInVehicle;
		if (bLeft) CompleteTask();
		return;
	}
	if (Objective == ETerritoryAIObservationObjective::ReleasesAttackToken)
	{
		if (!Controller || !Pawn) return;
		const bool bReleased = bObservedAttackToken && !bHasToken;
		bObservedAttackToken |= bHasToken;
		if (bReleased) CompleteTask();
		return;
	}
	const bool bSatisfied = IsAIStateSatisfiedBy(Target);
	if (bInitialEvaluation)
	{
		bObservedUnsatisfiedState |= !bSatisfied;
		if (bCompleteIfAlreadySatisfied && bSatisfied) CompleteTask();
		return;
	}
	if (!bSatisfied)
	{
		bObservedUnsatisfiedState = true;
		return;
	}
	if (bCompleteIfAlreadySatisfied || bObservedUnsatisfiedState) CompleteTask();
}

void UTerritoryAIObservationTask::HandleTargetReady(AActor* Actor)
{
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	BindTarget(Actor);
	Evaluate(true);
}

void UTerritoryAIObservationTask::HandleDestinationReady(AActor* Actor)
{
	if (!bIsActive || CurrentProgress >= RequiredQuantity) return;
	if (CachedDestination.Get() != Actor && Objective == ETerritoryAIObservationObjective::ReachActor)
		bObservedUnsatisfiedState = false;
	CachedDestination = Actor;
	Evaluate(false);
}

void UTerritoryAIObservationTask::HandleDeathStateChanged(
	AActor* ChangedActor, UNarrativeAbilitySystemComponent* ChangedASC,
	const bool bIsDead)
{
	if (bIsActive && Objective == ETerritoryAIObservationObjective::NPCDead
		&& ChangedASC == CachedAbilitySystem.Get() && bIsDead && CurrentProgress < RequiredQuantity)
	{
		CompleteTask();
	}
	(void)ChangedActor;
}

FText UTerritoryAIObservationTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	switch (Objective)
	{
	case ETerritoryAIObservationObjective::ActorAvailable:
		return NSLOCTEXT("CommunityTask", "AIAvailable", "Find the story character");
	case ETerritoryAIObservationObjective::NPCAlive:
		return NSLOCTEXT("CommunityTask", "AIAlive", "Keep the character alive");
	case ETerritoryAIObservationObjective::NPCDead:
		return NSLOCTEXT("CommunityTask", "AIDead", "Defeat the character");
	case ETerritoryAIObservationObjective::ReachQuestOwner:
		return NSLOCTEXT("CommunityTask", "AIReachPlayer", "Wait for the character to reach you");
	case ETerritoryAIObservationObjective::ReachActor:
		return NSLOCTEXT("CommunityTask", "AIReachActor", "Wait for the character to reach the destination");
	case ETerritoryAIObservationObjective::ReachLocation:
		return NSLOCTEXT("CommunityTask", "AIReachLocation", "Wait for the character to reach the location");
	case ETerritoryAIObservationObjective::HasGoalClass:
		return NSLOCTEXT("CommunityTask", "AIHasGoal", "Give the character its story goal");
	case ETerritoryAIObservationObjective::RunsActivityClass:
		return NSLOCTEXT("CommunityTask", "AIRunsActivity", "Wait for the character to begin its activity");
	case ETerritoryAIObservationObjective::PerceivesQuestOwner:
		return NSLOCTEXT("CommunityTask", "AISeesPlayer", "Let the character detect you");
	case ETerritoryAIObservationObjective::LosesQuestOwner:
		return NSLOCTEXT("CommunityTask", "AILosesPlayer", "Escape the character's senses");
	case ETerritoryAIObservationObjective::EntersVehicle:
		return NSLOCTEXT("CommunityTask", "AIEntersVehicle", "Wait for the character to enter the vehicle");
	case ETerritoryAIObservationObjective::LeavesVehicle:
		return NSLOCTEXT("CommunityTask", "AILeavesVehicle", "Force the character out of the vehicle");
	case ETerritoryAIObservationObjective::ClaimsAttackToken:
		return NSLOCTEXT("CommunityTask", "AIClaimsToken", "Draw the character into an attack");
	case ETerritoryAIObservationObjective::ReleasesAttackToken:
		return NSLOCTEXT("CommunityTask", "AIReleasesToken", "Break the character's attack turn");
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FText UTerritoryAIObservationTask::GetTaskProgressText_Implementation() const
{
	return FText::GetEmpty();
}

FVector UTerritoryAIObservationTask::GetNavigationMarkerLocation_Implementation() const
{
	if (Objective == ETerritoryAIObservationObjective::ReachLocation)
		return DestinationLocation;
	return MarkerSettings.ActorFallbackLocation;
}

AActor* UTerritoryAIObservationTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return ResolveSpatialActor(CachedTarget.IsValid()
		? CachedTarget.Get() : ResolveTarget());
}
