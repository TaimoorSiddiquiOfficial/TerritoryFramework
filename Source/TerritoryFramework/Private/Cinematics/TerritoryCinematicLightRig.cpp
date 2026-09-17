#include "Cinematics/TerritoryCinematicLightRig.h"
#include "Cinematics/TerritoryCinematicLightRigAdapter.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/TerritoryDialogueShot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TerritoryTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Tales/Dialogue.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UTerritoryCinematicLightRigProfile::HasValidConfiguration(FString& Reason) const
{
	if (!RigClass || RigClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)
		|| (ElementAdapter ? !ElementAdapter->SupportsRigClass(RigClass)
			: !RigClass->ImplementsInterface(UTerritoryCinematicLightRig::StaticClass())))
	{
		Reason = TEXT("Select a whole rig implementing Territory Cinematic Light Rig, or select a matching Element Adapter for the original pack element class.");
		return false;
	}
	if (ElementAdapter && ElementAdapter->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		Reason = TEXT("Select a concrete, supported Element Adapter.");
		return false;
	}
	const AActor* Defaults = RigClass->GetDefaultObject<AActor>();
	if (!Defaults || Defaults->GetIsReplicated() || Defaults->IsEditorOnly())
	{
		Reason = TEXT("The optional rig must be a non-replicated runtime actor.");
		return false;
	}
	if ((MeshRequirements.IsEmpty() && (!ElementAdapter || ElementAdapter->bRequiresCharacterMeshes))
		|| !FMath::IsFinite(ReadyTimeout) || ReadyTimeout < 0.1f || ReadyTimeout > 60.f)
	{
		Reason = TEXT("Add the required meshes and use a ready timeout between 0.1 and 60 seconds.");
		return false;
	}
	TSet<FName> Names;
	for (const auto& Requirement : MeshRequirements)
	{
		if (Requirement.ComponentName.IsNone() || Names.Contains(Requirement.ComponentName)
			|| Requirement.RequiredSockets.Contains(NAME_None))
		{
			Reason = TEXT("Mesh names must be unique and nonempty; socket names cannot be empty.");
			return false;
		}
		Names.Add(Requirement.ComponentName);
	}
	Reason.Reset();
	return true;
}

bool UTerritoryCinematicLightRigProfile::IsVisualReady(AActor* Visual) const
{
	if (!IsValid(Visual)) return false;
	if (ElementAdapter && !ElementAdapter->bRequiresCharacterMeshes) return true;
	if (MeshRequirements.IsEmpty()) return false;
	if (const auto* NativeVisual = Cast<ANarrativeCharacterVisual>(Visual))
		if (!NativeVisual->bBaseAppearanceLoaded) return false;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Visual);
	for (const auto& Requirement : MeshRequirements)
	{
		USkeletalMeshComponent* Match = nullptr;
		for (auto* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->GetFName() == Requirement.ComponentName)
			{
				if (Match) return false;
				Match = Mesh;
			}
		}
		if (!Match || !Match->IsRegistered() || !Match->GetSkeletalMeshAsset()) return false;
		for (const FName Socket : Requirement.RequiredSockets)
			if (!Match->DoesSocketExist(Socket)) return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UTerritoryCinematicLightRigProfile::IsDataValid(FDataValidationContext& Context) const
{
	const auto SuperResult = Super::IsDataValid(Context);
	FString Reason;
	if (!HasValidConfiguration(Reason))
	{
		Context.AddError(FText::FromString(Reason));
		return EDataValidationResult::Invalid;
	}
	return SuperResult == EDataValidationResult::Invalid ? SuperResult : EDataValidationResult::Valid;
}
#endif

UTerritoryCinematicLightRigComponent::UTerritoryCinematicLightRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(false);
}

bool UTerritoryCinematicLightRigComponent::CanUseLights(const APlayerController* Viewer, const UWorld* World)
{
	return IsValid(Viewer) && World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer
		&& Viewer->GetWorld() == World && Viewer->IsLocalPlayerController()
		&& World->GetGameInstance() && World->GetGameInstance()->GetNumLocalPlayers() == 1;
}

UTerritoryCinematicLightRigComponent* UTerritoryCinematicLightRigComponent::FindOrCreate(
	ALevelSequenceActor* Actor, APlayerController* Viewer)
{
	if (!IsValid(Actor) || !CanUseLights(Viewer, Actor->GetWorld()) || !Actor->GetSequencePlayer()) return nullptr;
	TInlineComponentArray<UTerritoryCinematicLightRigComponent*> Components(Actor);
	for (auto* Component : Components)
		if (IsValid(Component) && Component->LocalViewer == Viewer) return Component;
	auto* Component = NewObject<UTerritoryCinematicLightRigComponent>(Actor, NAME_None, RF_Transient);
	Component->LocalViewer = Viewer;
	Component->SequencePlayer = Actor->GetSequencePlayer();
	Actor->AddInstanceComponent(Component);
	Component->RegisterComponent();
	auto* Player = Actor->GetSequencePlayer();
	Player->OnCameraCut.AddUniqueDynamic(Component, &UTerritoryCinematicLightRigComponent::CameraCut);
	Player->OnStop.AddUniqueDynamic(Component, &UTerritoryCinematicLightRigComponent::PlaybackEnded);
	Player->OnFinished.AddUniqueDynamic(Component, &UTerritoryCinematicLightRigComponent::PlaybackEnded);
	return Component;
}

UTerritoryCinematicLightRigComponent* UTerritoryCinematicLightRigComponent::FollowNarrativeSequence(
	ANarrativeLevelSequenceActor* Actor, APlayerController* Viewer, AActor* Subject, UTerritoryCinematicLightRigProfile* Profile)
{
	if (!IsValid(Subject) || !IsValid(Profile) || !Actor || Subject->GetWorld() != Actor->GetWorld()) return nullptr;
	auto* Component = FindOrCreate(Actor, Viewer);
	if (Component)
	{
		if (Component->ActiveProfile != Profile || Component->SubjectActor != Subject || Component->bFollowingDialogue)
		{
			Component->ClearRig();
			Component->bFailed = false;
			Component->WaitingTime = 0.f;
		}
		Component->bFollowingDialogue = false;
		Component->SourceDialogue.Reset();
		Component->ActiveProfile = Profile;
		Component->SubjectActor = Subject;
		Component->RefreshRig(0.f);
	}
	return Component;
}

void UTerritoryCinematicLightRigComponent::FollowDialogueSequence(ALevelSequenceActor* Actor, UDialogue* Dialogue)
{
	if (!IsValid(Dialogue)) return;
	if (auto* Component = FindOrCreate(Actor, Dialogue->OwningController))
	{
		if (Component->SourceDialogue != Dialogue)
		{
			Component->ClearRig();
			Component->ActiveProfile = nullptr;
			Component->SubjectActor.Reset();
		}
		Component->SourceDialogue = Dialogue;
		Component->bFollowingDialogue = true;
		// Native assigns CurrentDialogueSequence after BeginPlaySequence returns.
		// Reconcile on the component tick so same-shot reuse follows Native's result.
	}
}

void UTerritoryCinematicLightRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	RefreshRig(DeltaTime);
}

void UTerritoryCinematicLightRigComponent::RefreshRig(float DeltaTime)
{
	if (!CanUseLights(LocalViewer.Get(), GetWorld()) || !SequencePlayer.IsValid())
	{
		StopLightRig();
		return;
	}
	if (bFollowingDialogue)
	{
		UDialogue* Dialogue = SourceDialogue.Get();
		auto* Controller = Cast<ANarrativePlayerController>(LocalViewer.Get());
		UTalesComponent* Tales = Controller ? Controller->GetTalesComponent() : nullptr;
		if (!Dialogue || !Dialogue->IsInitialized() || !Tales || Tales->GetCurrentDialogue() != Dialogue)
		{
			StopLightRig();
			return;
		}
		auto* Shot = Cast<UTerritoryDialogueShot>(Dialogue->GetCurrentDialogueSequence());
		auto* Profile = Shot ? Shot->LightRigProfile.Get() : nullptr;
		AActor* Subject = Shot && Shot->bLightRigUsesListener ? Dialogue->GetCurrentListenerAvatar() : Dialogue->GetCurrentSpeakerAvatar();
		if (ActiveProfile != Profile || SubjectActor != Subject)
		{
			ClearRig();
			ActiveProfile = Profile;
			SubjectActor = Subject;
			bFailed = false;
			WaitingTime = 0.f;
		}
	}
	if (!IsValid(ActiveProfile) || !SubjectActor.IsValid() || SubjectActor->GetWorld() != GetWorld())
	{
		ClearRig();
		return;
	}
	if (bFailed) return;
	if (SequencePlayer->GetDisableCameraCuts() || (!SequencePlayer->IsPlaying() && !SequencePlayer->IsPaused()))
	{
		ClearRig();
		return;
	}
	FString Reason;
	if (!ActiveProfile->HasValidConfiguration(Reason)) { Fail(Reason); return; }
	UCameraComponent* CameraComponent = SequencePlayer->GetActiveCameraComponent();
	ACameraActor* Camera = CameraComponent ? Cast<ACameraActor>(CameraComponent->GetOwner()) : nullptr;
	if (!IsValid(Camera) || Camera->GetWorld() != GetWorld()) { ClearRig(); return; }
	// A paused/older sequence may retain a camera after another cinematic or
	// gameplay takes the view. Only the displayed camera (or its blend target) owns lights.
	APlayerController* Viewer = LocalViewer.Get();
	if (Viewer->GetViewTarget() != Camera && (!Viewer->PlayerCameraManager
		|| Viewer->PlayerCameraManager->PendingViewTarget.Target != Camera)) { ClearRig(); return; }
	AActor* Visual = SubjectActor.Get();
	if (!ActiveProfile->ElementAdapter || ActiveProfile->ElementAdapter->bRequiresCharacterMeshes)
		if (auto* Character = Cast<ANarrativeCharacter>(Visual)) Visual = Character->GetCharacterVisual();
	if (SpawnedRig && BoundVisual != Visual) ClearRig();
	if (!ActiveProfile->IsVisualReady(Visual))
	{
		ClearRig();
		WaitingTime += DeltaTime;
		if (WaitingTime >= ActiveProfile->ReadyTimeout) Fail(TEXT("The character visual or required bones did not become ready."));
		return;
	}
	if (!IsValid(SpawnedRig) && RuntimeAdapter) ClearRig();
	if (IsValid(SpawnedRig))
	{
		UTerritoryCinematicLightRigAdapter* Adapter = RuntimeAdapter;
		if (!IsValid(Adapter)) { Fail(TEXT("The runtime light adapter was lost.")); return; }
		if (BoundCamera != Camera)
		{
			if (!Adapter->ChangeCamera(Camera) || RuntimeAdapter != Adapter || !IsValid(SpawnedRig))
			{ Fail(TEXT("The runtime rig could not update its camera.")); return; }
			BoundCamera = Camera;
		}
		if (!Adapter->UpdateRig(DeltaTime) || RuntimeAdapter != Adapter || !IsValid(SpawnedRig))
			Fail(Adapter->FailureReason.IsEmpty() ? TEXT("The runtime element could not update its lights.") : Adapter->FailureReason);
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.ObjectFlags |= RF_Transient;
	Params.bDeferConstruction = true;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedRig = GetWorld()->SpawnActor<AActor>(ActiveProfile->RigClass, FTransform::Identity, Params);
	if (!IsValid(SpawnedRig)) { Fail(TEXT("The runtime rig could not be spawned.")); return; }
	UTerritoryCinematicLightRigAdapter* Adapter = ActiveProfile->ElementAdapter
		? DuplicateObject<UTerritoryCinematicLightRigAdapter>(ActiveProfile->ElementAdapter, this)
		: NewObject<UTerritoryCinematicLightRigAdapter>(this);
	RuntimeAdapter = Adapter;
	Adapter->ClearFlags(RF_ArchetypeObject | RF_Public | RF_Standalone);
	Adapter->SetFlags(RF_Transient);
	SetComponentTickInterval(Adapter->bUpdateEveryFrame ? 0.f : 0.1f);
	if (!Adapter->Initialize(SpawnedRig, Visual, Camera) || RuntimeAdapter != Adapter || !IsValid(SpawnedRig))
	{ Fail(Adapter->FailureReason.IsEmpty() ? TEXT("The runtime rig rejected its visual or camera.") : Adapter->FailureReason); return; }
	SpawnedRig->FinishSpawning(FTransform::Identity);
	if (!IsValid(SpawnedRig) || RuntimeAdapter != Adapter || !Adapter->ActivateRig()
		|| RuntimeAdapter != Adapter || !IsValid(SpawnedRig) || !Adapter->IsRigReady()
		|| RuntimeAdapter != Adapter || !IsValid(SpawnedRig))
	{ Fail(Adapter->FailureReason.IsEmpty() ? TEXT("The runtime rig did not initialize its lights.") : Adapter->FailureReason); return; }
	BoundVisual = Visual;
	BoundCamera = Camera;
	WaitingTime = 0.f;
}

void UTerritoryCinematicLightRigComponent::CameraCut(UCameraComponent*) { RefreshRig(0.f); }
void UTerritoryCinematicLightRigComponent::PlaybackEnded()
{
	// Native dialogue shots pause on their last frame while a line or reply is
	// still displayed. UE broadcasts OnFinished after Pause() in that case.
	if (SequencePlayer.IsValid() && SequencePlayer->IsPaused()) return;
	StopLightRig();
}

void UTerritoryCinematicLightRigComponent::ClearRig()
{
	AActor* ReleasedRig = SpawnedRig;
	UTerritoryCinematicLightRigAdapter* ReleasedAdapter = RuntimeAdapter;
	SpawnedRig = nullptr;
	RuntimeAdapter = nullptr;
	BoundVisual.Reset();
	BoundCamera.Reset();
	if (IsValid(ReleasedAdapter)) ReleasedAdapter->Shutdown();
	if (IsValid(ReleasedRig)) ReleasedRig->Destroy(); // ChildActorComponents own the individual lights.
	SetComponentTickInterval(0.1f);
}

void UTerritoryCinematicLightRigComponent::Fail(const FString& Reason)
{
	ClearRig();
	bFailed = true;
	UE_LOG(LogTerritory, Warning, TEXT("Optional cinematic lights skipped for %s (%s): %s"),
		*GetNameSafe(SubjectActor.Get()), *GetNameSafe(ActiveProfile), *Reason);
}

void UTerritoryCinematicLightRigComponent::StopLightRig() { ClearRig(); DestroyComponent(); }

void UTerritoryCinematicLightRigComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (auto* Player = SequencePlayer.Get())
	{
		Player->OnCameraCut.RemoveAll(this);
		Player->OnStop.RemoveAll(this);
		Player->OnFinished.RemoveAll(this);
	}
	ClearRig();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UTerritoryCinematicLightRigComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Player = SequencePlayer.Get())
	{
		Player->OnCameraCut.RemoveAll(this);
		Player->OnStop.RemoveAll(this);
		Player->OnFinished.RemoveAll(this);
	}
	ClearRig();
	Super::EndPlay(Reason);
}
