#include "Abilities/TerritoryDistractionAbility.h"
#include "Engine/World.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/TerritoryStealthTags.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
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

	FVector ViewLocation = Avatar->GetActorLocation()
		+ FVector::UpVector * FallbackSpawnHeight;
	FRotator ViewRotation = Avatar->GetActorRotation();
	if (const AController* Controller = GetOwningController())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
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

	const FVector TraceEnd = ViewLocation
		+ ViewDirection * FMath::Max(100.f, AimTraceDistance);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TerritoryDistractionAim),
		true, Avatar);
	FHitResult AimHit;
	World->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd,
		AimTraceChannel, QueryParams);
	const FVector AimPoint = AimHit.bBlockingHit ? AimHit.ImpactPoint : TraceEnd;
	const FVector SpawnLocation = ViewLocation
		+ ViewDirection * FMath::Max(0.f, ForwardSpawnOffset);
	const FVector LaunchDirection = (AimPoint - SpawnLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		return false;
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

	APawn* InstigatorPawn = Cast<APawn>(Avatar);
	ATerritoryDistractionProjectile* Projectile =
		World->SpawnActorDeferred<ATerritoryDistractionProjectile>(
			ProjectileClass, LaunchTransform, Avatar, InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Projectile) || Projectile->IsActorBeingDestroyed() || !CanFinishThrow())
	{
		if (IsValid(Projectile)) Projectile->Destroy();
		EndThisActivation(true);
		return;
	}

	if (Projectile->ProjectileMovement)
	{
		const float Speed = LaunchSpeedOverride > 0.f
			? LaunchSpeedOverride
			: Projectile->ProjectileMovement->InitialSpeed;
		Projectile->ProjectileMovement->Velocity =
			LaunchTransform.GetRotation().GetForwardVector() * FMath::Max(0.f, Speed);
	}
	Projectile->FinishSpawning(LaunchTransform);
	if (!IsValid(Projectile) || Projectile->IsActorBeingDestroyed() || !CanFinishThrow())
	{
		if (IsValid(Projectile)) Projectile->Destroy();
		EndThisActivation(true);
		return;
	}

	if (bConsumeSourceItemOnSuccessfulThrow)
	{
		UNarrativeInventoryComponent* Inventory = SourceItem
			? SourceItem->OwningInventory : nullptr;
		if (!Inventory || Inventory->ConsumeItem(SourceItem, 1) != 1)
		{
			// An inventory race must not create a free replicated distraction.
			Projectile->Destroy();
			EndThisActivation(true);
			return;
		}
	}

	// Consuming the last equipped item may revoke/end this ability. The throw is
	// now paid for: like Narrative's SpawnProjectile task, it owns its own lifetime.
	FGameplayEventData Payload;
	Payload.EventTag = TerritoryStealthTags::DistractionThrownEvent;
	Payload.Instigator = Avatar;
	Payload.Target = Projectile;
	Payload.OptionalObject = Projectile;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Avatar, Payload.EventTag, Payload);
	K2_OnDistractionProjectileSpawned(Projectile);
	EndThisActivation(false);
}
