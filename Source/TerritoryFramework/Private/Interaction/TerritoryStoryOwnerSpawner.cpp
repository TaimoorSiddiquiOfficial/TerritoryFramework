#include "Interaction/TerritoryStoryOwnerSpawner.h"

#include "AI/NPCDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Spawners/NPCSpawnComponent.h"
#include "Interaction/InteractableComponent.h"
#include "Tales/Dialogue.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

ATerritoryStoryOwnerSpawner::ATerritoryStoryOwnerSpawner()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bActivateOnBeginPlay = false;

	OwnerSpawn = CreateDefaultSubobject<UNPCSpawnComponent>(TEXT("StoryOwnerSpawn"));
	OwnerSpawn->SetupAttachment(GetRootComponent());
	OwnerSpawn->bDontSpawnIfPreviouslyKilled = true;
}

void ATerritoryStoryOwnerSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPlaceDefinition();
}

bool ATerritoryStoryOwnerSpawner::ApplyPlaceDefinition()
{
	if (!PlaceDefinition) return false;
	const FTerritoryStoryOwnerTemplate& Template = PlaceDefinition->StoryOwner;
	TerritoryTag = PlaceDefinition->TerritoryTag;
	bBeginDialogueOnActivation = Template.bBeginDialogueOnActivation;
	OverrideDialogue = Template.DialogueOverride.LoadSynchronous();
	DialogueStartFromID = Template.DialogueStartFromID;
	OwnerInteractionDistance = FMath::Clamp(
		Template.InteractionDistance, 100.f, 1000.f);
	if (OwnerSpawn)
	{
		OwnerSpawn->NPCToSpawn = Template.NPCDefinition;
	}
	return Template.bEnabled;
}

void ATerritoryStoryOwnerSpawner::BeginPlay()
{
	// Narrative loads the spawner's SaveGame properties in its BeginPlay. Because
	// bActivateOnBeginPlay is false, no owner is spawned before the saved activation
	// flag has been restored.
	Super::BeginPlay();
	if (!ApplyPlaceDefinition())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Territory Story Owner %s has no enabled Place Definition. Legacy Blueprint configuration is disabled."),
			*GetPathName());
		return;
	}

	if (HasAuthority() && bHandoverActivated)
	{
		PendingNarrativeTarget.Reset();
		PendingController.Reset();
		PendingNarrativeComponent.Reset();
		bPendingBeginDialogue = false;
		HandoverSpawnRetryAttempts = 0;
		if (!TryCompletePendingHandover())
		{
			SchedulePendingHandoverRetry();
		}
	}
}

void ATerritoryStoryOwnerSpawner::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearPendingHandover();
	Super::EndPlay(EndPlayReason);
}

void ATerritoryStoryOwnerSpawner::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerritoryStoryOwnerSpawner, bHandoverActivated);
}

bool ATerritoryStoryOwnerSpawner::ActivateHandover(APawn* NarrativeTarget,
	APlayerController* Controller, UTalesComponent* NarrativeComponent,
	const bool bBeginDialogueImmediately)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!PlaceDefinition || !PlaceDefinition->StoryOwner.bEnabled)
	{
		return false;
	}

	PendingNarrativeTarget = NarrativeTarget;
	PendingController = Controller;
	PendingNarrativeComponent = NarrativeComponent;
	bPendingBeginDialogue = bBeginDialogueImmediately;
	HandoverSpawnRetryAttempts = 0;

	// Narrative NPC spawning can complete one or more ticks after SpawnActors().
	// Accept the valid request now, then finish activation and dialogue only when
	// Narrative exposes the spawned NPC. This avoids a silent owner when guards are
	// defeated immediately after BeginPlay or after a streamed cell becomes ready.
	if (!TryCompletePendingHandover())
	{
		SchedulePendingHandoverRetry();
	}
	return true;
}

ANarrativeNPCCharacter* ATerritoryStoryOwnerSpawner::GetStoryOwner() const
{
	return IsValid(OwnerSpawn) ? OwnerSpawn->GetSpawnedNPC() : nullptr;
}

void ATerritoryStoryOwnerSpawner::OnRep_HandoverActivated()
{
	// Narrative replicates the server-spawned NPC. This notification deliberately
	// does not create a second client-side copy.
}

bool ATerritoryStoryOwnerSpawner::EnsureOwnerSpawned()
{
	if (IsValid(GetStoryOwner()))
	{
		ApplyOwnerInteractionDistance();
		return true;
	}

	if (!IsValid(OwnerSpawn) || !IsValid(OwnerSpawn->NPCToSpawn))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Territory story owner spawner %s has no Narrative NPC definition for %s."),
			*GetName(), *TerritoryTag.ToString());
		return false;
	}

	SpawnActors();
	const bool bSpawned = IsValid(GetStoryOwner());
	if (bSpawned)
	{
		ApplyOwnerInteractionDistance();
	}
	return bSpawned;
}

void ATerritoryStoryOwnerSpawner::ApplyOwnerInteractionDistance()
{
	ANarrativeNPCCharacter* StoryOwner = GetStoryOwner();
	UNarrativeInteractableComponent* Interactable = StoryOwner
		? StoryOwner->FindComponentByClass<UNarrativeInteractableComponent>() : nullptr;
	if (Interactable)
	{
		Interactable->InteractionDistance = FMath::Clamp(
			OwnerInteractionDistance, 100.f, 1000.f);
	}
}

bool ATerritoryStoryOwnerSpawner::BeginOwnerDialogue(APawn* NarrativeTarget,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	ANarrativeNPCCharacter* StoryOwner = GetStoryOwner();
	if (!IsValid(StoryOwner))
	{
		return false;
	}

	// Defender-death events are also raised by environment damage, GAS effects, and
	// server-side scripted kills. Those paths may not carry the damaging pawn or its
	// Tales component. Resolve the authoritative player here so an automatic story
	// handover behaves the same as a normal player weapon kill.
	if (!IsValid(Controller) && IsValid(NarrativeTarget))
	{
		Controller = Cast<APlayerController>(NarrativeTarget->GetController());
	}
	if (!IsValid(Controller) && GetWorld())
	{
		Controller = GetWorld()->GetFirstPlayerController();
	}
	if (!IsValid(NarrativeTarget) && IsValid(Controller))
	{
		NarrativeTarget = Controller->GetPawn();
	}
	if (!IsValid(NarrativeComponent) && IsValid(NarrativeTarget))
	{
		NarrativeComponent = UNarrativeFunctionLibrary::GetTalesComponent(NarrativeTarget);
	}

	if (!IsValid(NarrativeComponent))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("Story owner %s appeared for %s; no Tales component was available, so dialogue remains manually interactable."),
			*GetNameSafe(StoryOwner), *TerritoryTag.ToString());
		return true;
	}

	TSubclassOf<UDialogue> DialogueClass = OverrideDialogue;
	if (!DialogueClass && IsValid(OwnerSpawn->NPCToSpawn))
	{
		DialogueClass = OwnerSpawn->NPCToSpawn->Dialogue.LoadSynchronous();
	}

	if (!DialogueClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Story owner %s appeared for %s but has no surrender dialogue."),
			*GetNameSafe(StoryOwner), *TerritoryTag.ToString());
		return true;
	}

	FDialoguePlayParams PlayParams;
	PlayParams.StartFromID = DialogueStartFromID;
	PlayParams.Speakers.Add(StoryOwner);
	PlayParams.bOverride_bStopMovement = true;
	PlayParams.bStopMovement = true;
	PlayParams.bOverride_bCanBeExited = true;
	PlayParams.bCanBeExited = true;

	return NarrativeComponent->BeginDialogue(DialogueClass, PlayParams);
}

bool ATerritoryStoryOwnerSpawner::TryCompletePendingHandover()
{
	if (!EnsureOwnerSpawned())
	{
		return false;
	}

	// Save and replicate activation only after Narrative has produced a valid NPC.
	// Otherwise a missing definition could permanently save a completed handover
	// whose owner never existed.
	if (!bHandoverActivated)
	{
		bHandoverActivated = true;
		ForceNetUpdate();
	}

	if (bPendingBeginDialogue && bBeginDialogueOnActivation)
	{
		return BeginOwnerDialogue(PendingNarrativeTarget.Get(),
			PendingController.Get(), PendingNarrativeComponent.Get());
	}

	return true;
}

void ATerritoryStoryOwnerSpawner::SchedulePendingHandoverRetry()
{
	if (!HasAuthority()
		|| GetWorldTimerManager().IsTimerActive(HandoverSpawnRetryTimer))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(HandoverSpawnRetryTimer, this,
		&ATerritoryStoryOwnerSpawner::RetryPendingHandover, 0.1f, true);
}

void ATerritoryStoryOwnerSpawner::RetryPendingHandover()
{
	++HandoverSpawnRetryAttempts;
	if (TryCompletePendingHandover())
	{
		ClearPendingHandover();
		return;
	}

	if (HandoverSpawnRetryAttempts >= MaxHandoverSpawnRetryAttempts)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Territory story owner %s did not become ready for %s after %.1f seconds; handover dialogue remains manually recoverable."),
			*GetPathName(), *TerritoryTag.ToString(),
			MaxHandoverSpawnRetryAttempts * 0.1f);
		ClearPendingHandover();
	}
}

void ATerritoryStoryOwnerSpawner::ClearPendingHandover()
{
	GetWorldTimerManager().ClearTimer(HandoverSpawnRetryTimer);
	PendingNarrativeTarget.Reset();
	PendingController.Reset();
	PendingNarrativeComponent.Reset();
	bPendingBeginDialogue = false;
	HandoverSpawnRetryAttempts = 0;
}
