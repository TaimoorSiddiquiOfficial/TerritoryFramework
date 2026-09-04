#include "Abilities/TerritoryDistractionAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Core/TerritoryStealthTags.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
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

	// Throwing during death, dialogue, or a sequencer camera cut produces invisible
	// projectiles and breaks Narrative's input-lock contract.
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_IsDead);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_DialogueControlled);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
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
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	FTransform LaunchTransform;
	if (!Avatar || !Avatar->HasAuthority() || !World || !ProjectileClass
		|| !GetDistractionLaunchTransform(LaunchTransform)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	APawn* InstigatorPawn = Cast<APawn>(Avatar);
	ATerritoryDistractionProjectile* Projectile =
		World->SpawnActorDeferred<ATerritoryDistractionProjectile>(
			ProjectileClass, LaunchTransform, Avatar, InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
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

	FGameplayEventData Payload;
	Payload.EventTag = TerritoryStealthTags::DistractionThrownEvent;
	Payload.Instigator = Avatar;
	Payload.Target = Projectile;
	Payload.OptionalObject = Projectile;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Avatar, Payload.EventTag, Payload);
	K2_OnDistractionProjectileSpawned(Projectile);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
