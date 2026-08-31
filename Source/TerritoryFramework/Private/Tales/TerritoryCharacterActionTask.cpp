#include "Tales/TerritoryCharacterActionTask.h"

#include "Character/NarrativeCharacterMovement.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	bool RequiresPolling(ETerritoryCharacterActionObjective Objective)
	{
		return Objective == ETerritoryCharacterActionObjective::Crouch
			|| Objective == ETerritoryCharacterActionObjective::Uncrouch
			|| Objective == ETerritoryCharacterActionObjective::StartSprint
			|| Objective == ETerritoryCharacterActionObjective::StopSprint
			|| Objective == ETerritoryCharacterActionObjective::StartSlowWalk
			|| Objective == ETerritoryCharacterActionObjective::StopSlowWalk
			|| Objective == ETerritoryCharacterActionObjective::Hurdle
			|| Objective == ETerritoryCharacterActionObjective::Mantle
			|| Objective == ETerritoryCharacterActionObjective::Vault;
	}

	bool CanCountInitialState(ETerritoryCharacterActionObjective Objective)
	{
		return Objective == ETerritoryCharacterActionObjective::Crouch
			|| Objective == ETerritoryCharacterActionObjective::StartSprint
			|| Objective == ETerritoryCharacterActionObjective::StartSlowWalk
			|| Objective == ETerritoryCharacterActionObjective::StartSwimming
			|| Objective == ETerritoryCharacterActionObjective::StartFalling
			|| Objective == ETerritoryCharacterActionObjective::StartClimbing
			|| Objective == ETerritoryCharacterActionObjective::EnterCover;
	}
}

void UTerritoryCharacterActionTask::BeginTask()
{
	if (RequiresPolling(Objective) || SubjectProvider) TickInterval = 0.1f;
	Super::BeginTask();
	if (IsComplete()) return;

	if (SubjectProvider)
	{
		SubjectProvider->OnProviderActorReady.AddUniqueDynamic(
			this, &UTerritoryCharacterActionTask::HandleProviderActorReady);
	}
	if (ACharacter* Character = ResolveCharacter()) BindCharacter(Character);
}

void UTerritoryCharacterActionTask::EndTask()
{
	if (SubjectProvider)
	{
		SubjectProvider->OnProviderActorReady.RemoveDynamic(
			this, &UTerritoryCharacterActionTask::HandleProviderActorReady);
	}
	UnbindCharacter();
	Super::EndTask();
}

void UTerritoryCharacterActionTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (IsComplete()) return;

	ACharacter* Character = CachedCharacter.Get();
	if (!Character)
	{
		Character = ResolveCharacter();
		if (Character) BindCharacter(Character);
	}
	if (!Character || !RequiresPolling(Objective)) return;

	const bool bCrouched = Character->bIsCrouched;
	const UNarrativeCharacterMovement* Movement =
		Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement());
	const bool bSprinting = Movement && Movement->IsSprinting();
	const bool bSlowWalking = Movement && Movement->IsSlowWalking();
	const ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(Character);
	const bool bTraversalPlaying = NarrativeCharacter
		&& NarrativeCharacter->IsPlayingAttachWarpMontage;

	if (Objective == ETerritoryCharacterActionObjective::Crouch
		&& !bPreviousCrouched && bCrouched) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::Uncrouch
		&& bPreviousCrouched && !bCrouched) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StartSprint
		&& !bPreviousSprinting && bSprinting) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StopSprint
		&& bPreviousSprinting && !bSprinting) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StartSlowWalk
		&& !bPreviousSlowWalking && bSlowWalking) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StopSlowWalk
		&& bPreviousSlowWalking && !bSlowWalking) CountAction();
	else if (!bPreviousTraversalPlaying && bTraversalPlaying)
	{
		const ETraversalActionType Action =
			NarrativeCharacter->AttachWarpProps.ActionType;
		if ((Objective == ETerritoryCharacterActionObjective::Hurdle
				&& Action == ETraversalActionType::Hurdle)
			|| (Objective == ETerritoryCharacterActionObjective::Mantle
				&& Action == ETraversalActionType::Mantle)
			|| (Objective == ETerritoryCharacterActionObjective::Vault
				&& Action == ETraversalActionType::Vault)) CountAction();
	}

	bPreviousCrouched = bCrouched;
	bPreviousSprinting = bSprinting;
	bPreviousSlowWalking = bSlowWalking;
	bPreviousTraversalPlaying = bTraversalPlaying;
}

bool UTerritoryCharacterActionTask::IsActionStateSatisfiedBy(
	const ACharacter* Character) const
{
	if (!Character) return false;
	const UCharacterMovementComponent* BaseMovement = Character->GetCharacterMovement();
	const UNarrativeCharacterMovement* Movement =
		Cast<UNarrativeCharacterMovement>(BaseMovement);
	switch (Objective)
	{
	case ETerritoryCharacterActionObjective::Crouch:
		return Character->bIsCrouched;
	case ETerritoryCharacterActionObjective::Uncrouch:
		return !Character->bIsCrouched;
	case ETerritoryCharacterActionObjective::StartSprint:
		return Movement && Movement->IsSprinting();
	case ETerritoryCharacterActionObjective::StopSprint:
		return Movement && !Movement->IsSprinting();
	case ETerritoryCharacterActionObjective::StartSlowWalk:
		return Movement && Movement->IsSlowWalking();
	case ETerritoryCharacterActionObjective::StopSlowWalk:
		return Movement && !Movement->IsSlowWalking();
	case ETerritoryCharacterActionObjective::StartSwimming:
		return BaseMovement && BaseMovement->IsSwimming();
	case ETerritoryCharacterActionObjective::StopSwimming:
		return BaseMovement && !BaseMovement->IsSwimming();
	case ETerritoryCharacterActionObjective::StartFalling:
		return BaseMovement && BaseMovement->IsFalling();
	case ETerritoryCharacterActionObjective::StartClimbing:
		return Movement && Movement->IsClimbing();
	case ETerritoryCharacterActionObjective::StopClimbing:
		return Movement && !Movement->IsClimbing();
	case ETerritoryCharacterActionObjective::EnterCover:
		return Movement && Movement->HasCover();
	case ETerritoryCharacterActionObjective::ExitCover:
		return Movement && !Movement->HasCover();
	default:
		return false;
	}
}

ACharacter* UTerritoryCharacterActionTask::ResolveCharacter() const
{
	AActor* Subject = SubjectProvider
		? SubjectProvider->ProvideActor(this) : OwningPawn;
	return Cast<ACharacter>(Subject);
}

void UTerritoryCharacterActionTask::BindCharacter(ACharacter* Character)
{
	if (!Character || CachedCharacter.Get() == Character) return;
	UnbindCharacter();
	CachedCharacter = Character;
	Character->LandedDelegate.AddUniqueDynamic(
		this, &UTerritoryCharacterActionTask::HandleLanded);
	Character->MovementModeChangedDelegate.AddUniqueDynamic(
		this, &UTerritoryCharacterActionTask::HandleMovementModeChanged);
	Character->OnReachedJumpApex.AddUniqueDynamic(
		this, &UTerritoryCharacterActionTask::HandleReachedJumpApex);

	if (ANarrativeCharacter* NarrativeCharacter = Cast<ANarrativeCharacter>(Character))
	{
		NarrativeCharacter->OnJumpedDelegate.AddUniqueDynamic(
			this, &UTerritoryCharacterActionTask::HandleJumped);
	}
	if (UNarrativeCharacterMovement* Movement =
		Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()))
	{
		Movement->OnEnterCover.AddUniqueDynamic(
			this, &UTerritoryCharacterActionTask::HandleEnteredCover);
		Movement->OnExitCover.AddUniqueDynamic(
			this, &UTerritoryCharacterActionTask::HandleExitedCover);
	}

	SnapshotPolledStates(Character);
	if (bCountInitialState && CanCountInitialState(Objective)
		&& IsActionStateSatisfiedBy(Character)) CountAction();
}

void UTerritoryCharacterActionTask::UnbindCharacter()
{
	if (ACharacter* Character = CachedCharacter.Get())
	{
		Character->LandedDelegate.RemoveDynamic(
			this, &UTerritoryCharacterActionTask::HandleLanded);
		Character->MovementModeChangedDelegate.RemoveDynamic(
			this, &UTerritoryCharacterActionTask::HandleMovementModeChanged);
		Character->OnReachedJumpApex.RemoveDynamic(
			this, &UTerritoryCharacterActionTask::HandleReachedJumpApex);
		if (ANarrativeCharacter* NarrativeCharacter = Cast<ANarrativeCharacter>(Character))
		{
			NarrativeCharacter->OnJumpedDelegate.RemoveDynamic(
				this, &UTerritoryCharacterActionTask::HandleJumped);
		}
		if (UNarrativeCharacterMovement* Movement =
			Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()))
		{
			Movement->OnEnterCover.RemoveDynamic(
				this, &UTerritoryCharacterActionTask::HandleEnteredCover);
			Movement->OnExitCover.RemoveDynamic(
				this, &UTerritoryCharacterActionTask::HandleExitedCover);
		}
	}
	CachedCharacter.Reset();
}

void UTerritoryCharacterActionTask::SnapshotPolledStates(const ACharacter* Character)
{
	bPreviousCrouched = Character && Character->bIsCrouched;
	const UNarrativeCharacterMovement* Movement = Character
		? Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()) : nullptr;
	bPreviousSprinting = Movement && Movement->IsSprinting();
	bPreviousSlowWalking = Movement && Movement->IsSlowWalking();
	const ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(Character);
	bPreviousTraversalPlaying = NarrativeCharacter
		&& NarrativeCharacter->IsPlayingAttachWarpMontage;
}

void UTerritoryCharacterActionTask::CountAction()
{
	if (!IsComplete()) AddProgress(1);
}

void UTerritoryCharacterActionTask::HandleProviderActorReady(AActor* Actor)
{
	BindCharacter(Cast<ACharacter>(Actor));
}

void UTerritoryCharacterActionTask::HandleJumped()
{
	if (Objective == ETerritoryCharacterActionObjective::Jump) CountAction();
}

void UTerritoryCharacterActionTask::HandleReachedJumpApex()
{
	if (Objective == ETerritoryCharacterActionObjective::ReachJumpApex) CountAction();
}

void UTerritoryCharacterActionTask::HandleLanded(const FHitResult& Hit)
{
	(void)Hit;
	if (Objective == ETerritoryCharacterActionObjective::Land) CountAction();
}

void UTerritoryCharacterActionTask::HandleMovementModeChanged(
	ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
	if (!Character || Character != CachedCharacter.Get()) return;
	const UCharacterMovementComponent* BaseMovement = Character->GetCharacterMovement();
	const UNarrativeCharacterMovement* Movement =
		Cast<UNarrativeCharacterMovement>(BaseMovement);
	if (Objective == ETerritoryCharacterActionObjective::StartSwimming
		&& BaseMovement && BaseMovement->IsSwimming()) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StopSwimming
		&& PreviousMode == MOVE_Swimming
		&& BaseMovement && !BaseMovement->IsSwimming()) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StartFalling
		&& BaseMovement && BaseMovement->IsFalling()) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StartClimbing
		&& Movement && Movement->IsClimbing()) CountAction();
	else if (Objective == ETerritoryCharacterActionObjective::StopClimbing
		&& PreviousMode == MOVE_Custom
		&& PreviousCustomMode == CMOVE_Climb
		&& Movement && !Movement->IsClimbing()) CountAction();
}

void UTerritoryCharacterActionTask::HandleEnteredCover()
{
	if (Objective == ETerritoryCharacterActionObjective::EnterCover) CountAction();
}

void UTerritoryCharacterActionTask::HandleExitedCover()
{
	if (Objective == ETerritoryCharacterActionObjective::ExitCover) CountAction();
}

FText UTerritoryCharacterActionTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	static const TMap<ETerritoryCharacterActionObjective, FText> Descriptions = {
		{ ETerritoryCharacterActionObjective::Jump, NSLOCTEXT("CommunityTask", "Jump", "Jump") },
		{ ETerritoryCharacterActionObjective::ReachJumpApex, NSLOCTEXT("CommunityTask", "Apex", "Reach the top of a jump") },
		{ ETerritoryCharacterActionObjective::Land, NSLOCTEXT("CommunityTask", "Land", "Land safely") },
		{ ETerritoryCharacterActionObjective::Crouch, NSLOCTEXT("CommunityTask", "Crouch", "Crouch") },
		{ ETerritoryCharacterActionObjective::Uncrouch, NSLOCTEXT("CommunityTask", "Uncrouch", "Stand up") },
		{ ETerritoryCharacterActionObjective::StartSprint, NSLOCTEXT("CommunityTask", "Sprint", "Start sprinting") },
		{ ETerritoryCharacterActionObjective::StopSprint, NSLOCTEXT("CommunityTask", "StopSprint", "Stop sprinting") },
		{ ETerritoryCharacterActionObjective::StartSlowWalk, NSLOCTEXT("CommunityTask", "SlowWalk", "Start slow walking") },
		{ ETerritoryCharacterActionObjective::StopSlowWalk, NSLOCTEXT("CommunityTask", "StopSlowWalk", "Stop slow walking") },
		{ ETerritoryCharacterActionObjective::StartSwimming, NSLOCTEXT("CommunityTask", "Swim", "Enter the water and swim") },
		{ ETerritoryCharacterActionObjective::StopSwimming, NSLOCTEXT("CommunityTask", "StopSwim", "Leave the water") },
		{ ETerritoryCharacterActionObjective::StartFalling, NSLOCTEXT("CommunityTask", "Fall", "Begin falling") },
		{ ETerritoryCharacterActionObjective::StartClimbing, NSLOCTEXT("CommunityTask", "Climb", "Start climbing") },
		{ ETerritoryCharacterActionObjective::StopClimbing, NSLOCTEXT("CommunityTask", "StopClimb", "Finish climbing") },
		{ ETerritoryCharacterActionObjective::EnterCover, NSLOCTEXT("CommunityTask", "Cover", "Enter cover") },
		{ ETerritoryCharacterActionObjective::ExitCover, NSLOCTEXT("CommunityTask", "ExitCover", "Leave cover") },
		{ ETerritoryCharacterActionObjective::Hurdle, NSLOCTEXT("CommunityTask", "Hurdle", "Hurdle an obstacle") },
		{ ETerritoryCharacterActionObjective::Mantle, NSLOCTEXT("CommunityTask", "Mantle", "Mantle an obstacle") },
		{ ETerritoryCharacterActionObjective::Vault, NSLOCTEXT("CommunityTask", "Vault", "Vault an obstacle") }
	};
	if (const FText* Description = Descriptions.Find(Objective)) return *Description;
	return Super::GetTaskDescription_Implementation();
}

FText UTerritoryCharacterActionTask::GetTaskProgressText_Implementation() const
{
	return RequiredQuantity > 1
		? Super::GetTaskProgressText_Implementation() : FText::GetEmpty();
}

AActor* UTerritoryCharacterActionTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return CachedCharacter.IsValid() ? CachedCharacter.Get() : ResolveCharacter();
}
