#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"

#include "AI/NPCDefinition.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GroomComponent.h"
#include "Tales/Dialogue.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/UObjectIterator.h"

void UTerritoryCinematicPresentationSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		BindToController(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void UTerritoryCinematicPresentationSubsystem::Deinitialize()
{
	UnbindFromTalesComponent();
	RestoreComponentLODs();
	ActiveDialogue = nullptr;
	Super::Deinitialize();
}

void UTerritoryCinematicPresentationSubsystem::PlayerControllerChanged(
	APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	BindToController(NewPlayerController);
}

UTerritoryCinematicPresentationSubsystem*
UTerritoryCinematicPresentationSubsystem::GetForPlayerController(
	const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController
		? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer
		? LocalPlayer->GetSubsystem<UTerritoryCinematicPresentationSubsystem>()
		: nullptr;
}

void UTerritoryCinematicPresentationSubsystem::BindToController(
	APlayerController* PlayerController)
{
	UnbindFromTalesComponent();
	RestoreComponentLODs();
	const bool bWasActive = ActiveDialogue != nullptr;
	ActiveDialogue = nullptr;

	const ANarrativePlayerController* NarrativeController =
		Cast<ANarrativePlayerController>(PlayerController);
	BoundTalesComponent = NarrativeController
		? NarrativeController->GetTalesComponent() : nullptr;
	if (BoundTalesComponent)
	{
		BoundTalesComponent->OnDialogueBegan.AddUniqueDynamic(
			this, &UTerritoryCinematicPresentationSubsystem::HandleDialogueBegan);
		BoundTalesComponent->OnDialogueFinished.AddUniqueDynamic(
			this, &UTerritoryCinematicPresentationSubsystem::HandleDialogueFinished);
		BoundTalesComponent->OnNPCDialogueLineStarted.AddUniqueDynamic(
			this, &UTerritoryCinematicPresentationSubsystem::HandleNPCDialogueLineStarted);
		BoundTalesComponent->OnPlayerDialogueLineStarted.AddUniqueDynamic(
			this, &UTerritoryCinematicPresentationSubsystem::HandlePlayerDialogueLineStarted);

		if (UDialogue* CurrentDialogue = BoundTalesComponent->GetCurrentDialogue())
		{
			HandleDialogueBegan(CurrentDialogue);
			return;
		}
	}

	if (bWasActive)
	{
		OnPresentationChanged.Broadcast(false);
	}
}

void UTerritoryCinematicPresentationSubsystem::UnbindFromTalesComponent()
{
	CancelDialogueReconciliation();
	if (!BoundTalesComponent) return;
	BoundTalesComponent->OnDialogueBegan.RemoveDynamic(
		this, &UTerritoryCinematicPresentationSubsystem::HandleDialogueBegan);
	BoundTalesComponent->OnDialogueFinished.RemoveDynamic(
		this, &UTerritoryCinematicPresentationSubsystem::HandleDialogueFinished);
	BoundTalesComponent->OnNPCDialogueLineStarted.RemoveDynamic(
		this, &UTerritoryCinematicPresentationSubsystem::HandleNPCDialogueLineStarted);
	BoundTalesComponent->OnPlayerDialogueLineStarted.RemoveDynamic(
		this, &UTerritoryCinematicPresentationSubsystem::HandlePlayerDialogueLineStarted);
	BoundTalesComponent = nullptr;
}

void UTerritoryCinematicPresentationSubsystem::HandleDialogueBegan(
	UDialogue* Dialogue)
{
	if (!Dialogue) return;
	CancelDialogueReconciliation();
	const bool bWasActive = ActiveDialogue != nullptr;
	if (ActiveDialogue != Dialogue)
	{
		// A chained conversation may use different actors. Release the previous
		// subjects before taking the new dialogue's temporary quality overrides.
		RestoreComponentLODs();
	}
	ActiveDialogue = Dialogue;
	RefreshDialogueSubjects(Dialogue);
	if (!bWasActive)
	{
		OnPresentationChanged.Broadcast(true);
	}
}

void UTerritoryCinematicPresentationSubsystem::HandleDialogueFinished(
	UDialogue* Dialogue, const bool bStartingNewDialogue,
	const EExitDialogueReason Reason)
{
	(void)Reason;
	if (Dialogue != ActiveDialogue) return;
	CancelDialogueReconciliation();
	if (bStartingNewDialogue)
	{
		// Tales broadcasts Finished before clearing CurrentDialogue and attempting
		// MakeDialogueInstance. A rejected replacement has no Began/Finished event.
		// Reconcile once after that attempt, preserving the HUD through valid chains.
		if (UWorld* World = BoundTalesComponent ? BoundTalesComponent->GetWorld() : nullptr)
		{
			DialogueReconciliationWorld = World;
			DialogueReconciliationTimer = World->GetTimerManager().SetTimerForNextTick(
				this, &UTerritoryCinematicPresentationSubsystem::ReconcileCurrentDialogue);
			return;
		}
	}
	ClearPresentation();
}

void UTerritoryCinematicPresentationSubsystem::CancelDialogueReconciliation()
{
	if (UWorld* World = DialogueReconciliationWorld.Get())
	{
		World->GetTimerManager().ClearTimer(DialogueReconciliationTimer);
	}
	DialogueReconciliationTimer.Invalidate();
	DialogueReconciliationWorld.Reset();
}

void UTerritoryCinematicPresentationSubsystem::ReconcileCurrentDialogue()
{
	DialogueReconciliationTimer.Invalidate();
	DialogueReconciliationWorld.Reset();
	if (UDialogue* Current = BoundTalesComponent ? BoundTalesComponent->GetCurrentDialogue() : nullptr)
	{
		HandleDialogueBegan(Current);
	}
	else
	{
		ClearPresentation();
	}
}

void UTerritoryCinematicPresentationSubsystem::ClearPresentation()
{
	const bool bWasActive = ActiveDialogue != nullptr;
	ActiveDialogue = nullptr;
	RestoreComponentLODs();
	if (bWasActive)
	{
		OnPresentationChanged.Broadcast(false);
	}
}

void UTerritoryCinematicPresentationSubsystem::HandleNPCDialogueLineStarted(
	UDialogue* Dialogue, UDialogueNode_NPC* Node,
	const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{
	(void)Node;
	(void)DialogueLine;
	(void)Speaker;
	RefreshDialogueSubjects(Dialogue);
}

void UTerritoryCinematicPresentationSubsystem::HandlePlayerDialogueLineStarted(
	UDialogue* Dialogue, UDialogueNode_Player* Node,
	const FDialogueLine& DialogueLine)
{
	(void)Node;
	(void)DialogueLine;
	RefreshDialogueSubjects(Dialogue);
}

void UTerritoryCinematicPresentationSubsystem::RefreshDialogueSubjects(
	UDialogue* Dialogue)
{
	if (!Dialogue || Dialogue != ActiveDialogue) return;
	RegisterCinematicSubject(Dialogue->GetPlayerAvatar());
	RegisterCinematicSubject(Dialogue->GetCurrentSpeakerAvatar());
	RegisterCinematicSubject(Dialogue->GetCurrentListenerAvatar());
	for (const FSpeakerInfo& Speaker : Dialogue->Speakers)
	{
		RegisterCinematicSubject(Dialogue->GetAvatar(Speaker.GetSpeakerID()));
	}
	for (const FPlayerSpeakerInfo& PartySpeaker : Dialogue->PartySpeakerInfo)
	{
		RegisterCinematicSubject(Dialogue->GetAvatar(PartySpeaker.GetSpeakerID()));
	}
}

void UTerritoryCinematicPresentationSubsystem::RegisterCinematicSubject(
	AActor* Subject)
{
	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	if (!Subject || !ActiveDialogue || !Settings
		|| !Settings->bUseCinematicParticipantLODDuringDialogue)
	{
		return;
	}

	TArray<AActor*> VisualActors = { Subject };
	if (Settings->bIncludeAttachedActorsInCinematicLOD)
	{
		Subject->GetAttachedActors(VisualActors, false, true);
	}

	for (AActor* VisualActor : VisualActors)
	{
		if (!VisualActor) continue;

		TInlineComponentArray<ULODSyncComponent*> LODSyncComponents(VisualActor);
		for (ULODSyncComponent* Component : LODSyncComponents)
		{
			if (!Component || HasOverrideFor(Component)) continue;
			const FComponentLODOverride* Shared = FindSharedOverride(Component);
			ComponentLODOverrides.Add({ Component,
				EComponentOverrideType::LODSync, Shared ? Shared->PreviousLOD : Component->ForcedLOD });
			Component->ForcedLOD = 0;
		}

		TInlineComponentArray<UGroomComponent*> GroomComponents(VisualActor);
		for (UGroomComponent* Component : GroomComponents)
		{
			if (!Component || HasOverrideFor(Component)) continue;
			const FComponentLODOverride* Shared = FindSharedOverride(Component);
			ComponentLODOverrides.Add({ Component,
				EComponentOverrideType::Groom, Shared ? Shared->PreviousLOD : Component->GetForcedLOD() });
			Component->SetForcedLOD(0);
		}

		TInlineComponentArray<USkinnedMeshComponent*> MeshComponents(VisualActor);
		for (USkinnedMeshComponent* Component : MeshComponents)
		{
			if (!Component || HasOverrideFor(Component)) continue;
			const FComponentLODOverride* Shared = FindSharedOverride(Component);
			ComponentLODOverrides.Add({ Component,
				EComponentOverrideType::SkinnedMesh, Shared ? Shared->PreviousLOD : Component->GetForcedLOD() });
			// USkinnedMeshComponent uses 1 for render LOD 0; 0 means automatic.
			Component->SetForcedLOD(1);
		}
	}
}

bool UTerritoryCinematicPresentationSubsystem::HasOverrideFor(
	const UActorComponent* Component) const
{
	return ComponentLODOverrides.ContainsByPredicate(
		[Component](const FComponentLODOverride& Override)
		{
			return Override.Component.Get() == Component;
		});
}

const UTerritoryCinematicPresentationSubsystem::FComponentLODOverride*
UTerritoryCinematicPresentationSubsystem::FindSharedOverride(const UActorComponent* Component) const
{
	// Local players can share the same rendered actor. Inspect only presentation
	// subsystem instances on acquisition/release, not world actors or every tick.
	// Exact component identity also separates independent PIE worlds.
	for (TObjectIterator<UTerritoryCinematicPresentationSubsystem> It; It; ++It)
	{
		if (*It == this || It->IsTemplate() || !IsValid(*It)) continue;
		if (const auto* Shared = It->ComponentLODOverrides.FindByPredicate(
			[Component](const FComponentLODOverride& Entry) { return Entry.Component.Get() == Component; }))
		{
			return Shared;
		}
	}
	return nullptr;
}

void UTerritoryCinematicPresentationSubsystem::RestoreComponentLODs()
{
	for (const FComponentLODOverride& Override : ComponentLODOverrides)
	{
		UActorComponent* Component = Override.Component.Get();
		if (!Component || FindSharedOverride(Component)) continue;
		switch (Override.Type)
		{
		case EComponentOverrideType::LODSync:
			if (ULODSyncComponent* LODSync = Cast<ULODSyncComponent>(Component))
			{
				if (LODSync->ForcedLOD == 0) LODSync->ForcedLOD = Override.PreviousLOD;
			}
			break;
		case EComponentOverrideType::Groom:
			if (UGroomComponent* Groom = Cast<UGroomComponent>(Component))
			{
				if (Groom->GetForcedLOD() == 0) Groom->SetForcedLOD(Override.PreviousLOD);
			}
			break;
		case EComponentOverrideType::SkinnedMesh:
			if (USkinnedMeshComponent* Mesh = Cast<USkinnedMeshComponent>(Component))
			{
				if (Mesh->GetForcedLOD() == 1) Mesh->SetForcedLOD(Override.PreviousLOD);
			}
			break;
		}
	}
	ComponentLODOverrides.Reset();
}
