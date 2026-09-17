#include "Cinematics/TerritoryCinematicLightRigAdapter.h"
#include "Cinematics/TerritoryCinematicLightRig.h"
#include "Camera/CameraActor.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	// Cosmetic leases only. Weak keys/owners never keep streamed actors or ended sessions alive.
	TMap<TWeakObjectPtr<ULightComponent>, TWeakObjectPtr<UTerritoryCinematicLightRigAdapter>> SceneLightOwners;
}

UTerritoryCinematicLightRigAdapter::UTerritoryCinematicLightRigAdapter()
{
	RequiredRigInterface = UTerritoryCinematicLightRig::StaticClass();
	SupportedRigBaseClass = AActor::StaticClass();
}

UWorld* UTerritoryCinematicLightRigAdapter::GetWorld() const
{
	if (IsTemplate()) return nullptr;
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

bool UTerritoryCinematicLightRigAdapter::SupportsRigClass(UClass* Class) const
{
	return Class && SupportedRigBaseClass && Class->IsChildOf(SupportedRigBaseClass)
		&& RequiredRigInterface && RequiredRigInterface->HasAnyClassFlags(CLASS_Interface)
		&& Class->ImplementsInterface(RequiredRigInterface);
}

bool UTerritoryCinematicLightRigAdapter::Initialize(AActor* InRig, AActor* InVisual, ACameraActor* InCamera)
{
	Shutdown();
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer
		|| !IsValid(InRig) || !IsValid(InVisual) || !IsValid(InCamera)
		|| InRig->GetWorld() != World || InVisual->GetWorld() != World || InCamera->GetWorld() != World
		|| !SupportsRigClass(InRig->GetClass())) return false;
	Rig = InRig;
	Rig->OnDestroyed.AddUniqueDynamic(this, &UTerritoryCinematicLightRigAdapter::RigDestroyed);
	Visual = InVisual;
	Camera = InCamera;
	FailureReason.Reset();
	return PrepareRig();
}

bool UTerritoryCinematicLightRigAdapter::PrepareRig_Implementation()
{
	return IsValid(Rig) && Rig->Implements<UTerritoryCinematicLightRig>()
		&& ITerritoryCinematicLightRig::Execute_PrepareLightRig(Rig, Visual, Camera);
}

bool UTerritoryCinematicLightRigAdapter::IsRigReady_Implementation() const
{
	return IsValid(Rig) && Rig->Implements<UTerritoryCinematicLightRig>()
		&& ITerritoryCinematicLightRig::Execute_IsLightRigReady(Rig);
}

bool UTerritoryCinematicLightRigAdapter::ActivateRig_Implementation() { return IsRigReady(); }

bool UTerritoryCinematicLightRigAdapter::ChangeCamera(ACameraActor* InCamera)
{
	if (!IsValid(InCamera) || InCamera->GetWorld() != GetWorld()) return false;
	Camera = InCamera;
	return UpdateCamera();
}

bool UTerritoryCinematicLightRigAdapter::UpdateCamera_Implementation()
{
	return IsValid(Rig) && Rig->Implements<UTerritoryCinematicLightRig>()
		&& ITerritoryCinematicLightRig::Execute_SetLightRigCamera(Rig, Camera);
}

bool UTerritoryCinematicLightRigAdapter::UpdateRig_Implementation(float DeltaSeconds)
{
	// Whole-rig actors own their own Tick. Element adapters override this with their pack's UpdateLight call.
	return IsValid(Rig) && !Rig->IsActorBeingDestroyed();
}

void UTerritoryCinematicLightRigAdapter::ReleaseRig_Implementation() { RestoreSceneLights(); }

void UTerritoryCinematicLightRigAdapter::RigDestroyed(AActor*) { Shutdown(); }

void UTerritoryCinematicLightRigAdapter::Shutdown()
{
	if (bShuttingDown) return;
	TGuardValue<bool> Guard(bShuttingDown, true);
	if (Rig)
	{
		Rig->OnDestroyed.RemoveAll(this);
		ReleaseRig();
	}
	RestoreSceneLights();
	Rig = nullptr;
	Visual = nullptr;
	Camera = nullptr;
}

USkeletalMeshComponent* UTerritoryCinematicLightRigAdapter::FindVisualMesh(FName ComponentName) const
{
	if (!IsValid(Visual) || ComponentName.IsNone()) return nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Visual);
	USkeletalMeshComponent* Match = nullptr;
	for (auto* Mesh : Meshes)
	{
		if (IsValid(Mesh) && Mesh->GetFName() == ComponentName && Mesh->IsRegistered() && Mesh->GetSkeletalMeshAsset())
		{
			if (Match) return nullptr;
			Match = Mesh;
		}
	}
	return Match;
}

bool UTerritoryCinematicLightRigAdapter::RefreshTaggedSceneLights(FName ActorTag, float DeltaSeconds, bool bForce)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer || !IsValid(Rig) || ActorTag.IsNone())
	{
		FailureReason = TEXT("Background elements need a nonempty scene-light Actor Tag and an active local rig.");
		return false;
	}
	RefreshRemaining -= FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.f, DeltaSeconds) : 0.f;
	if (!bForce && ActorTag == CapturedTag && RefreshRemaining > 0.f) return true;
	for (auto It = SceneLightOwners.CreateIterator(); It; ++It)
		if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
	TMap<ULightComponent*, ALight*> Wanted;
	for (TActorIterator<ALight> It(World); It; ++It)
	{
		ALight* Light = *It;
		ULightComponent* Component = Light->GetLightComponent();
		if (!IsValid(Light) || Light->IsActorBeingDestroyed() || !Light->ActorHasTag(ActorTag)
			|| !IsValid(Component) || !Component->IsRegistered()) continue;
		if (Component->Mobility == EComponentMobility::Static)
		{
			FailureReason = FString::Printf(TEXT("Tagged scene light %s is Static. Use a Stationary or Movable light."), *Light->GetName());
			return false;
		}
		const auto* Existing = SceneLightOwners.Find(Component);
		if (Existing && Existing->IsValid() && Existing->Get() != this)
		{
			FailureReason = FString::Printf(TEXT("Another cinematic is already changing scene light %s."), *Light->GetName());
			return false;
		}
		Wanted.Add(Component, Light);
	}
	// Validate the complete target set first. A refused session never partly takes over scene lights.
	for (int32 Index = CapturedLights.Num() - 1; Index >= 0; --Index)
	{
		if (!Wanted.Contains(CapturedLights[Index].Component.Get()))
		{
			RestoreLight(CapturedLights[Index]);
			CapturedLights.RemoveAtSwap(Index);
		}
	}
	for (const auto& Pair : Wanted)
	{
		if (CapturedLights.ContainsByPredicate([&Pair](const FLightState& State) { return State.Component == Pair.Key; })) continue;
		ULightComponent* Component = Pair.Key;
		FLightState& State = CapturedLights.AddDefaulted_GetRef();
		State.Actor = Pair.Value;
		State.Component = Component;
		State.Intensity = Component->Intensity;
		State.Color = Component->LightColor;
		State.Temperature = Component->Temperature;
		State.bUseTemperature = Component->bUseTemperature;
		State.RayTracedShadows = static_cast<uint8>(Component->CastRaytracedShadow.GetValue());
		State.Samples = Component->SamplesPerPixel;
		SceneLightOwners.Add(Component, this);
	}
	CapturedTag = ActorTag;
	RefreshRemaining = 0.5f;
	FailureReason.Reset();
	return true;
}

TArray<ALight*> UTerritoryCinematicLightRigAdapter::GetCapturedSceneLights() const
{
	TArray<ALight*> Result;
	for (const auto& State : CapturedLights)
		if (State.Actor.IsValid() && !State.Actor->IsActorBeingDestroyed()
			&& State.Component.IsValid() && State.Component->IsRegistered()) Result.Add(State.Actor.Get());
	return Result;
}

void UTerritoryCinematicLightRigAdapter::RestoreLight(const FLightState& State)
{
	const auto* Owner = SceneLightOwners.Find(State.Component);
	if (!Owner || Owner->Get() != this) return;
	if (ULightComponent* Component = State.Component.Get())
	{
		Component->SetIntensity(State.Intensity);
		Component->SetLightFColor(State.Color);
		Component->SetTemperature(State.Temperature);
		Component->SetUseTemperature(State.bUseTemperature);
		Component->SetCastRaytracedShadows(static_cast<ECastRayTracedShadow::Type>(State.RayTracedShadows));
		Component->SetSamplesPerPixel(State.Samples);
	}
	SceneLightOwners.Remove(State.Component);
}

void UTerritoryCinematicLightRigAdapter::RestoreSceneLights()
{
	for (const auto& State : CapturedLights) RestoreLight(State);
	CapturedLights.Reset();
	CapturedTag = NAME_None;
	RefreshRemaining = 0.f;
}
