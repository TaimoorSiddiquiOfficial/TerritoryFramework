#include "Interaction/TerritoryStoryOwnerSpawner.h"

#include "AI/NPCDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Spawners/NPCSpawnComponent.h"
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

void ATerritoryStoryOwnerSpawner::BeginPlay()
{
	// Narrative loads the spawner's SaveGame properties in its BeginPlay. Because
	// bActivateOnBeginPlay is false, no owner is spawned before the saved activation
	// flag has been restored.
	Super::BeginPlay();

	if (HasAuthority() && bHandoverActivated)
	{
		EnsureOwnerSpawned();
	}
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

	if (!EnsureOwnerSpawned())
	{
		return false;
	}

	// Save and replicate activation only after Narrative has produced a valid NPC.
	// Otherwise a missing definition could permanently save a completed handover
	// whose owner never existed.
	bHandoverActivated = true;
	ForceNetUpdate();

	if (bBeginDialogueImmediately && bBeginDialogueOnActivation)
	{
		return BeginOwnerDialogue(NarrativeTarget, Controller, NarrativeComponent);
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
	return IsValid(GetStoryOwner());
}

bool ATerritoryStoryOwnerSpawner::BeginOwnerDialogue(APawn* NarrativeTarget,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	ANarrativeNPCCharacter* StoryOwner = GetStoryOwner();
	if (!IsValid(StoryOwner))
	{
		return false;
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
