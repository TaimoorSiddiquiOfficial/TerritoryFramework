#include "Abilities/TerritoryDistractionAbility.h"
#include "Engine/World.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Core/TerritoryStealthTags.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Items/InventoryComponent.h"
#include "Items/EquippableItem.h"
#include "Items/NarrativeItem.h"
#include "NarrativeGameplayTags.h"

UTerritoryDistractionAbility::UTerritoryDistractionAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Throw;
	ProjectileClass = ATerritoryDistractionProjectile::StaticClass();
	// Montage-driven throws wait for this tag; the Territory Throw Release notify defaults to it.
	ThrowReleaseEventTag = TerritoryStealthTags::DistractionReleaseEvent;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(TerritoryStealthTags::DistractionAbility);
	SetAssetTags(AssetTags);

	// Match Narrative's combat-input locks. Throwing while these states are active
	// produces invisible projectiles, animation conflicts, or mounted/falling exploits.
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_IsDead);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_DialogueControlled);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_OnMount);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Movement_Falling);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Movement_Climbing);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Weapon_BlockFiring);
}

UNarrativeItem* UTerritoryDistractionAbility::GetThrowableSourceItem(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<UNarrativeItem>(GetSourceObject(Handle, ActorInfo));
}

bool UTerritoryDistractionAbility::IsThrowableSourceItemReady(
	const UNarrativeItem* SourceItem) const
{
	if (!IsValid(SourceItem) || !IsValid(SourceItem->OwningInventory)
		|| SourceItem->GetQuantity() < 1
		|| !SourceItem->OwningInventory->GetItems().Contains(SourceItem))
	{
		return false;
	}

	if (bRequireEquippedNarrativeItemSource)
	{
		const UEquippableItem* EquippedItem = Cast<UEquippableItem>(SourceItem);
		if (!EquippedItem || !EquippedItem->IsEquipped()) return false;
	}

	return !bConsumeSourceItemOnSuccessfulThrow || SourceItem->CanBeRemoved();
}

bool UTerritoryDistractionAbility::IsThrowableSourceOwnedByAvatar(
	const UNarrativeItem* SourceItem, const FGameplayAbilityActorInfo* ActorInfo) const
{
	return IsThrowableSourceItemReady(SourceItem) && ActorInfo
		&& ActorInfo->AvatarActor.IsValid()
		&& SourceItem->OwningInventory->GetOwningPawn() == ActorInfo->AvatarActor.Get();
}

bool UTerritoryDistractionAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	const bool bNeedsSourceItem = bRequireEquippedNarrativeItemSource
		|| bConsumeSourceItemOnSuccessfulThrow;
	return !bNeedsSourceItem
		|| IsThrowableSourceOwnedByAvatar(GetThrowableSourceItem(Handle, ActorInfo), ActorInfo);
}

bool UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
	const FVector& Start,
	const FVector& AimPoint,
	float Speed,
	float GravityMagnitude,
	float MaxPitchDegrees,
	FVector& OutDirection)
{
	if (Speed <= KINDA_SMALL_NUMBER || GravityMagnitude <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector ToAim = AimPoint - Start;
	const float HorizontalDistance = ToAim.Size2D();
	if (HorizontalDistance <= 1.f)
	{
		return false;
	}

	// Ballistic solution: tan(theta) = (v^2 - sqrt(v^4 - g(g x^2 + 2 y v^2))) / (g x),
	// taking the low arc so the stone still looks like a throw rather than a mortar shot.
	const float SpeedSquared = Speed * Speed;
	const float Discriminant = SpeedSquared * SpeedSquared
		- GravityMagnitude * (GravityMagnitude * HorizontalDistance * HorizontalDistance
			+ 2.f * ToAim.Z * SpeedSquared);
	if (Discriminant < 0.f)
	{
		return false;
	}

	const float LowArcPitch = FMath::Atan(
		(SpeedSquared - FMath::Sqrt(Discriminant))
		/ (GravityMagnitude * HorizontalDistance));
	const float PitchCap = FMath::DegreesToRadians(FMath::Clamp(MaxPitchDegrees, 0.f, 89.f));
	const float ClampedPitch = FMath::Clamp(LowArcPitch, -PitchCap, PitchCap);
	const FVector HorizontalAim(ToAim.X, ToAim.Y, 0.f);
	if (HorizontalAim.IsNearlyZero())
	{
		return false;
	}

	OutDirection = (HorizontalAim.GetSafeNormal() * FMath::Cos(ClampedPitch)
		+ FVector::UpVector * FMath::Sin(ClampedPitch)).GetSafeNormal();
	return !OutDirection.IsNearlyZero();
}

bool UTerritoryDistractionAbility::ShouldUseReplicatedClientAim(
	bool bIsLocalController, bool bHasCameraCache)
{
	return !bIsLocalController && !bHasCameraCache;
}

bool UTerritoryDistractionAbility::ShouldAimWithMouseCursor(
	bool bIsLocalController, bool bCursorVisible, bool bHasMousePosition)
{
	// Only a locally controlled player can deproject the pointer, and only when the project
	// actually shows a cursor. Reading a mouse position without a visible cursor would aim at a
	// stale pixel while the player is looking down the crosshair.
	return bIsLocalController && bCursorVisible && bHasMousePosition;
}

bool UTerritoryDistractionAbility::GetDistractionLaunchTransform(
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!Avatar || !World)
	{
		return false;
	}

	// The camera supplies the crosshair aim point; it must never supply the spawn point. In a
	// third-person setup the camera sits metres behind the character, so spawning along the view
	// vector put the projectile near the camera instead of in the throwing hand.
	FVector ViewLocation = Avatar->GetActorLocation();
	FRotator ViewRotation = Avatar->GetActorRotation();
	if (const AController* Controller = GetOwningController())
	{
		const APlayerController* PlayerController = Cast<APlayerController>(Controller);
		const APawn* ControlledPawn = Controller->GetPawn();
		// A ServerOnly ability on the server's copy of a remote client has no locally updated camera:
		// GetPlayerViewPoint would fall back to the view-target actor rotation (yaw only) until the
		// first ServerUpdateCamera arrives, silently dropping the aim pitch. Use the pawn's
		// replicated eye location and the replicated control rotation so the throw follows that
		// client's crosshair.
		const bool bLocalController = !PlayerController || PlayerController->IsLocalController();
		const bool bHasCameraCache = PlayerController && PlayerController->PlayerCameraManager
			&& PlayerController->PlayerCameraManager->GetCameraCacheTime() > 0.f;
		if (PlayerController && ControlledPawn
			&& ShouldUseReplicatedClientAim(bLocalController, bHasCameraCache))
		{
			ViewLocation = ControlledPawn->GetPawnViewLocation();
			ViewRotation = PlayerController->GetControlRotation();
		}
		else
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}
	else if (const APawn* Pawn = Cast<APawn>(Avatar))
	{
		ViewRotation = Pawn->GetBaseAimRotation();
	}

	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return false;
	}

	// The ray that finds the aim point. Usually it is the camera view ray, which is what the
	// crosshair sits on. A project that shows a mouse cursor aims where the pointer is, so the
	// ray is rebuilt through the pointer pixel instead of the screen centre.
	FVector AimRayOrigin = ViewLocation;
	FVector AimRayDirection = ViewDirection;
	if (bAimWithVisibleMouseCursor)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetOwningController()))
		{
			float MouseX = 0.f;
			float MouseY = 0.f;
			const bool bGotMouse = PlayerController->GetMousePosition(MouseX, MouseY);
			if (ShouldAimWithMouseCursor(PlayerController->IsLocalController(),
				PlayerController->ShouldShowMouseCursor(), bGotMouse))
			{
				FVector DeprojectedLocation = FVector::ZeroVector;
				FVector DeprojectedDirection = FVector::ZeroVector;
				if (PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY,
					DeprojectedLocation, DeprojectedDirection))
				{
					const FVector PointerDirection = DeprojectedDirection.GetSafeNormal();
					// Guard against a stale pointer that resolved behind the camera: keep the
					// crosshair ray rather than throwing over the shoulder.
					if (!PointerDirection.IsNearlyZero()
						&& FVector::DotProduct(PointerDirection, ViewDirection) > 0.2f)
					{
						AimRayOrigin = DeprojectedLocation;
						AimRayDirection = PointerDirection;
					}
				}
			}
		}
	}

	// Launch from the throwing hand when the character exposes the socket; the avatar-origin
	// fallback keeps AI pawns and test characters working.
	FVector SpawnLocation = Avatar->GetActorLocation()
		+ FVector::UpVector * FMath::Max(0.f, FallbackSpawnHeight);
	if (!LaunchSocketName.IsNone())
	{
		if (const USkeletalMeshComponent* Mesh = Avatar->FindComponentByClass<USkeletalMeshComponent>())
		{
			if (Mesh->DoesSocketExist(LaunchSocketName))
			{
				SpawnLocation = Mesh->GetSocketLocation(LaunchSocketName);
			}
		}
	}
	if (SpawnLocation.Equals(Avatar->GetActorLocation() + FVector::UpVector * FMath::Max(0.f, FallbackSpawnHeight),
		KINDA_SMALL_NUMBER))
	{
		// No socket: keep the old viewpoint-relative offset so the stone still clears the capsule.
		SpawnLocation = ViewLocation + ViewDirection * FMath::Max(0.f, ForwardSpawnOffset);
	}

	const FVector TraceEnd = AimRayOrigin
		+ AimRayDirection * FMath::Max(100.f, AimTraceDistance);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TerritoryDistractionAim),
		true, Avatar);
	// Held items and weapons are separate actors sitting inside the camera trace. Turning the aim
	// point into one of them produced backwards throws, so ignore everything attached to the avatar.
	TArray<AActor*> AttachedActors;
	Avatar->GetAttachedActors(AttachedActors);
	QueryParams.AddIgnoredActors(AttachedActors);
	if (const AController* Controller = GetOwningController())
	{
		if (APawn* ControlledPawn = Controller->GetPawn())
		{
			QueryParams.AddIgnoredActor(ControlledPawn);
		}
	}
	FHitResult AimHit;
	World->LineTraceSingleByChannel(AimHit, AimRayOrigin, TraceEnd,
		AimTraceChannel, QueryParams);
	const FVector AimPoint = AimHit.bBlockingHit ? AimHit.ImpactPoint : TraceEnd;
	FVector LaunchDirection = (AimPoint - SpawnLocation).GetSafeNormal();
	// A hit closer than the hand (standing against a wall) would aim the throw backwards; fall
	// back to the aim ray instead of launching the stone over the shoulder.
	if (LaunchDirection.IsNearlyZero()
		|| FVector::DotProduct(LaunchDirection, AimRayDirection) < 0.2f)
	{
		LaunchDirection = AimRayDirection;
	}

	// A stone with the authored 1200 cm/s and gravity 1.0 leaves along the crosshair but sags
	// 0.9 m at 5 m and 3.4 m at 10 m. Bend the launch up just enough that the same speed and
	// gravity land on the aim point; unreachable targets keep the straight aim direction.
	if (bCompensateLaunchForGravity && ProjectileClass)
	{
		const ATerritoryDistractionProjectile* ProjectileDefaults =
			ProjectileClass->GetDefaultObject<ATerritoryDistractionProjectile>();
		const UProjectileMovementComponent* ProjectileMovement = ProjectileDefaults
			? ProjectileDefaults->ProjectileMovement.Get() : nullptr;
		const float Speed = LaunchSpeedOverride > 0.f
			? LaunchSpeedOverride
			: (ProjectileMovement ? ProjectileMovement->InitialSpeed : 0.f);
		const float GravityScale = ProjectileMovement
			? FMath::Max(0.f, ProjectileMovement->ProjectileGravityScale) : 1.f;
		const float GravityMagnitude = FMath::Max(0.f, -World->GetGravityZ()) * GravityScale;
		FVector CompensatedDirection;
		if (ComputeAimCompensatedDirection(SpawnLocation, AimPoint, Speed, GravityMagnitude,
			MaxCompensatedLaunchPitch, CompensatedDirection))
		{
			LaunchDirection = CompensatedDirection;
		}
	}

	OutTransform = FTransform(LaunchDirection.Rotation(), SpawnLocation);
	return true;
}

void UTerritoryDistractionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	(void)TriggerEventData;
	const uint64 ThisActivation = ++ActivationSerial;
	// Commit, construction, inventory and event callbacks can end this activation
	// and even start another one on the same InstancedPerActor ability.
	const auto EndThisActivation = [&](const bool bCancelled)
	{
		if (ActivationSerial == ThisActivation && IsActive())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, bCancelled);
		}
	};
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	UNarrativeItem* SourceItem = GetThrowableSourceItem(Handle, ActorInfo);
	const bool bNeedsSourceItem = bRequireEquippedNarrativeItemSource
		|| bConsumeSourceItemOnSuccessfulThrow;
	const auto CanFinishThrow = [&]()
	{
		const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
		return ActivationSerial == ThisActivation && IsActive()
			&& IsValid(Avatar) && !Avatar->IsActorBeingDestroyed()
			&& ActorInfo->AvatarActor.Get() == Avatar && Spec && !Spec->PendingRemove
			&& (!bNeedsSourceItem || IsThrowableSourceOwnedByAvatar(SourceItem, ActorInfo));
	};
	FTransform LaunchTransform;
	if (!Avatar || !Avatar->HasAuthority() || !World || !ProjectileClass
		|| (bNeedsSourceItem
			&& !IsThrowableSourceOwnedByAvatar(SourceItem, ActorInfo))
		|| !GetDistractionLaunchTransform(LaunchTransform)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo)
		|| !CanFinishThrow())
	{
		EndThisActivation(true);
		return;
	}

	// Capture the activation state a deferred release needs. The ability stays active until the
	// release frame, and one activation can only ever throw once.
	PendingSourceItem = SourceItem;
	bPendingConsumeSourceItem = bConsumeSourceItemOnSuccessfulThrow;
	PendingActivationSerial = ThisActivation;
	bThrowPerformed = false;
	bThrowPending = false;

	// Vendor pattern (GA_Attack_ThrowGrenade): play the montage through GAS so it replicates, wait
	// for the release Gameplay Event the montage's Territory Throw Release notify sends, and bound
	// the wait with a timer. Everything here is designer-configurable on a Blueprint child.
	const USkeletalMeshComponent* ThrowMesh = Avatar->FindComponentByClass<USkeletalMeshComponent>();
	if (ThrowMontage && ThrowMesh && ThrowMesh->GetAnimInstance())
	{
		if (!ThrowReleaseEventTag.IsValid())
		{
			ThrowReleaseEventTag = TerritoryStealthTags::DistractionReleaseEvent;
		}
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ThrowMontage, FMath::Max(0.f, ThrowMontagePlayRate),
			ThrowMontageSection, true, 1.f, 0.f);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
			MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageFinished);
			MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
			MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
			MontageTask->ReadyForActivation();
		}
		ReleaseWaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, ThrowReleaseEventTag, nullptr, true, false);
		if (ReleaseWaitTask)
		{
			ReleaseWaitTask->EventReceived.AddDynamic(this, &ThisClass::HandleReleaseEvent);
			ReleaseWaitTask->ReadyForActivation();
		}
		bThrowPending = true;
		World->GetTimerManager().SetTimer(ReleaseFallbackTimer, this,
			&ThisClass::HandleReleaseFallback, FMath::Max(0.05f, ThrowReleaseFallbackSeconds), false);
		return;
	}

	// No montage (AI pawns, test worlds) or no anim instance: keep the immediate authoritative throw.
	PerformThrow();
}

void UTerritoryDistractionAbility::PerformThrow()
{
	// One activation, one throw: the notify, the montage end and the fallback timer can race.
	if (bThrowPerformed)
	{
		return;
	}
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const uint64 ThisActivation = PendingActivationSerial;
	const bool bNeedsSourceItem = bRequireEquippedNarrativeItemSource || bPendingConsumeSourceItem;
	const auto CanFinishThrow = [this, ActorInfo, Avatar, ThisActivation, bNeedsSourceItem]()
	{
		const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(GetCurrentAbilitySpecHandle()) : nullptr;
		return ThisActivation == ActivationSerial && IsActive()
			&& IsValid(Avatar) && !Avatar->IsActorBeingDestroyed()
			&& ActorInfo && ActorInfo->AvatarActor.Get() == Avatar && Spec && !Spec->PendingRemove
			&& (!bNeedsSourceItem || IsThrowableSourceOwnedByAvatar(PendingSourceItem, ActorInfo));
	};

	if (World)
	{
		World->GetTimerManager().ClearTimer(ReleaseFallbackTimer);
	}
	if (ReleaseWaitTask)
	{
		ReleaseWaitTask->EndTask();
		ReleaseWaitTask = nullptr;
	}

	FTransform LaunchTransform;
	if (!Avatar || !Avatar->HasAuthority() || !World || !ProjectileClass
		|| (bNeedsSourceItem
			&& (!ActorInfo || !IsThrowableSourceOwnedByAvatar(PendingSourceItem, ActorInfo)))
		|| !GetDistractionLaunchTransform(LaunchTransform)
		|| !CanFinishThrow())
	{
		FinishActivation(true);
		return;
	}

	// Presentation hook at the release frame, before the projectile exists.
	K2_OnDistractionRelease();

	APawn* InstigatorPawn = Cast<APawn>(Avatar);
	ATerritoryDistractionProjectile* Projectile =
		World->SpawnActorDeferred<ATerritoryDistractionProjectile>(
			ProjectileClass, LaunchTransform, Avatar, InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Projectile) || Projectile->IsActorBeingDestroyed() || !CanFinishThrow())
	{
		if (IsValid(Projectile)) Projectile->Destroy();
		FinishActivation(true);
		return;
	}

	if (Projectile->ProjectileMovement)
	{
		const float Speed = LaunchSpeedOverride > 0.f
			? LaunchSpeedOverride
			: Projectile->ProjectileMovement->InitialSpeed;
		// UProjectileMovementComponent::InitializeComponent (run by FinishSpawning below) rotates
		// Velocity by the component transform when bInitialVelocityInLocalSpace is true, which is
		// the engine default. The direction computed above is already world space, so a second
		// rotation would throw the stone somewhere else entirely. Force the world-space contract
		// here as well so a Blueprint child cannot silently reintroduce the bug.
		Projectile->ProjectileMovement->bInitialVelocityInLocalSpace = false;
		Projectile->ProjectileMovement->Velocity =
			LaunchTransform.GetRotation().GetForwardVector() * FMath::Max(0.f, Speed);
	}
	Projectile->FinishSpawning(LaunchTransform);
	if (!IsValid(Projectile) || Projectile->IsActorBeingDestroyed() || !CanFinishThrow())
	{
		if (IsValid(Projectile)) Projectile->Destroy();
		FinishActivation(true);
		return;
	}

	if (bPendingConsumeSourceItem)
	{
		UNarrativeInventoryComponent* Inventory = PendingSourceItem
			? PendingSourceItem->OwningInventory : nullptr;
		if (!Inventory || Inventory->ConsumeItem(PendingSourceItem, 1) != 1)
		{
			// An inventory race must not create a free replicated distraction.
			Projectile->Destroy();
			FinishActivation(true);
			return;
		}
	}

	// Consuming the last equipped item may revoke/end this ability. The throw is
	// now paid for: like Narrative's SpawnProjectile task, it owns its own lifetime.
	bThrowPerformed = true;
	FGameplayEventData Payload;
	Payload.EventTag = TerritoryStealthTags::DistractionThrownEvent;
	Payload.Instigator = Avatar;
	Payload.Target = Projectile;
	Payload.OptionalObject = Projectile;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Avatar, Payload.EventTag, Payload);
	K2_OnDistractionProjectileSpawned(Projectile);
	FinishActivation(false);
}

void UTerritoryDistractionAbility::HandleReleaseEvent(FGameplayEventData Payload)
{
	(void)Payload;
	PerformThrow();
}

void UTerritoryDistractionAbility::HandleMontageFinished()
{
	// The montage ended without a release notify: throw rather than silently lose the input.
	PerformThrow();
}

void UTerritoryDistractionAbility::HandleReleaseFallback()
{
	PerformThrow();
}

void UTerritoryDistractionAbility::FinishActivation(const bool bCancelled)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->GetWorld() : nullptr;
	if (World)
	{
		World->GetTimerManager().ClearTimer(ReleaseFallbackTimer);
	}
	if (ReleaseWaitTask)
	{
		ReleaseWaitTask->EndTask();
		ReleaseWaitTask = nullptr;
	}
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	PendingSourceItem = nullptr;
	bThrowPending = false;
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(),
			true, bCancelled);
	}
}

void UTerritoryDistractionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(ReleaseFallbackTimer);
	}
	if (ReleaseWaitTask)
	{
		ReleaseWaitTask->EndTask();
		ReleaseWaitTask = nullptr;
	}
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	PendingSourceItem = nullptr;
	bThrowPending = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
