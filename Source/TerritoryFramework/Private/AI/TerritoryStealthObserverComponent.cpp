#include "AI/TerritoryStealthObserverComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryStealthTags.h"
#include "Core/TerritoryVolume.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"

UTerritoryStealthObserverComponent::UTerritoryStealthObserverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTerritoryStealthObserverComponent::BeginPlay()
{
	Super::BeginPlay();
	ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	if (!Guard || !Guard->HasAuthority()) return;
	if (!BindToCurrentPerception())
	{
		GetWorld()->GetTimerManager().SetTimer(BindingRetryTimer, this,
			&UTerritoryStealthObserverComponent::RetryPerceptionBinding,
			0.25f, true, 0.25f);
	}
	GetWorld()->GetTimerManager().SetTimer(SightRefreshTimer, this,
		&UTerritoryStealthObserverComponent::RefreshVisibleTargets,
		0.25f, true, 0.25f);
}

void UTerritoryStealthObserverComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindingRetryTimer);
		World->GetTimerManager().ClearTimer(SightRefreshTimer);
	}
	UnbindFromPerception();
	CurrentlySeenTargets.Empty();
	RecentGunshots.Empty();
	Super::EndPlay(EndPlayReason);
}

ATerritoryGuardCharacter* UTerritoryStealthObserverComponent::GetTerritoryGuard() const
{
	return Cast<ATerritoryGuardCharacter>(GetOwner());
}

const UTerritoryStealthProfile* UTerritoryStealthObserverComponent::GetActiveProfile() const
{
	const ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	const ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	return Territory ? Territory->GetActiveStealthProfile() : nullptr;
}

AActor* UTerritoryStealthObserverComponent::ResolvePlayerSource(AActor* SensedActor) const
{
	if (!IsValid(SensedActor) || SensedActor->IsActorBeingDestroyed()) return nullptr;
	if (const APawn* Pawn = Cast<APawn>(SensedActor))
	{
		return Pawn->IsPlayerControlled() ? SensedActor : nullptr;
	}
	if (APawn* InstigatorPawn = SensedActor->GetInstigator())
	{
		return InstigatorPawn->IsPlayerControlled() ? InstigatorPawn : nullptr;
	}
	return nullptr;
}

bool UTerritoryStealthObserverComponent::BindToCurrentPerception()
{
	ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	if (!Guard || !Guard->HasAuthority()) return false;
	AAIController* Controller = Cast<AAIController>(Guard->GetController());
	UAIPerceptionComponent* Perception = Controller
		? Controller->FindComponentByClass<UAIPerceptionComponent>() : nullptr;
	if (!Perception) return false;
	if (BoundPerception.Get() == Perception)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindingRetryTimer);
		}
		return true;
	}
	UnbindFromPerception();
	BoundPerception = Perception;
	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(
		this, &UTerritoryStealthObserverComponent::HandleTargetPerceptionUpdated);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindingRetryTimer);
	}
	return true;
}

void UTerritoryStealthObserverComponent::RetryPerceptionBinding()
{
	BindToCurrentPerception();
}

void UTerritoryStealthObserverComponent::UnbindFromPerception()
{
	if (UAIPerceptionComponent* Perception = BoundPerception.Get())
	{
		Perception->OnTargetPerceptionUpdated.RemoveDynamic(
			this, &UTerritoryStealthObserverComponent::HandleTargetPerceptionUpdated);
	}
	BoundPerception.Reset();
}

bool UTerritoryStealthObserverComponent::IsTargetFiring(AActor* Target) const
{
	const UAbilitySystemComponent* ASC = Target
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target) : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(
		FNarrativeGameplayTags::Get().State_Weapon_IsFiring);
}

float UTerritoryStealthObserverComponent::CalculateEffectiveSightStrength(
	AActor* Target, float RawSightStrength) const
{
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	if (!Profile || !Target) return FMath::Clamp(RawSightStrength, 0.f, 1.f);
	const UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (Profile->bRespectNarrativeInvisibleTag && ASC
		&& ASC->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_InvisibleToEnemies))
	{
		return 0.f;
	}

	float StealthRating = 0.f;
	if (const ANarrativeCharacter* NarrativeCharacter = Cast<ANarrativeCharacter>(Target))
	{
		StealthRating = NarrativeCharacter->GetStealthRating();
	}
	return ApplyStealthRatingToSight(RawSightStrength,
		Profile->GuardDetectionMultiplier, StealthRating,
		Profile->MaximumStealthRating);
}

float UTerritoryStealthObserverComponent::ApplyStealthRatingToSight(
	float RawSightStrength, float GuardDetectionMultiplier,
	float StealthRating, float MaximumStealthRating)
{
	const float StealthFraction = MaximumStealthRating > 0.f
		? FMath::Clamp(StealthRating / MaximumStealthRating, 0.f, 1.f)
		: 0.f;
	return FMath::Clamp(RawSightStrength * FMath::Max(0.f, GuardDetectionMultiplier)
		* (1.f - StealthFraction), 0.f, 1.f);
}

void UTerritoryStealthObserverComponent::HandleTargetPerceptionUpdated(
	AActor* SensedActor, FAIStimulus Stimulus)
{
	ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	AActor* Target = ResolvePlayerSource(SensedActor);
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	UWorld* World = GetWorld();
	UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Guard || !Guard->HasAuthority() || !Territory || !Target || !Profile
		|| !Control || !Territory->ContainsPoint(Target->GetActorLocation()))
	{
		return;
	}

	const TSubclassOf<UAISense> SenseClass =
		UAIPerceptionSystem::GetSenseClassForStimulus(this, Stimulus);
	if (SenseClass == UAISense_Sight::StaticClass())
	{
		if (!Stimulus.WasSuccessfullySensed())
		{
			CurrentlySeenTargets.Remove(Target);
			Control->ReportStealthEvidence(Territory, Target, Guard,
				ETerritoryStealthEvidence::Sight, 0.f,
				Stimulus.StimulusLocation, FVector::ZeroVector, false);
			return;
		}

		const float EffectiveStrength = CalculateEffectiveSightStrength(
			Target, Stimulus.Strength);
		FObservedSight& Seen = CurrentlySeenTargets.FindOrAdd(Target);
		Seen.RawStrength = Stimulus.Strength;
		Seen.LastLocation = Stimulus.StimulusLocation;
		if (IsTargetFiring(Target) && Profile->bFireWhileSeenExposes)
		{
			Control->ReportStealthEvidence(Territory, Target, Guard,
				ETerritoryStealthEvidence::FireSeen, 1.f,
				Stimulus.StimulusLocation, FVector::ZeroVector, true);
		}
		else
		{
			Control->ReportStealthEvidence(Territory, Target, Guard,
				ETerritoryStealthEvidence::Sight, EffectiveStrength,
				Stimulus.StimulusLocation, FVector::ZeroVector,
				EffectiveStrength >= Profile->ImmediateSightExposureThreshold);
		}
		return;
	}

	if (SenseClass == UAISense_Damage::StaticClass()
		&& Stimulus.WasSuccessfullySensed() && Profile->bDamageImmediatelyExposes)
	{
		Control->ReportStealthEvidence(Territory, Target, Guard,
			ETerritoryStealthEvidence::Damage, 1.f,
			Stimulus.StimulusLocation, FVector::ZeroVector, true);
		return;
	}

	if (SenseClass != UAISense_Hearing::StaticClass()
		|| !Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	const FName StimulusTag = Stimulus.Tag;
	if (StimulusTag == TerritoryStealthTags::DistractionThrowable.GetTag().GetTagName())
	{
		Control->ReportStealthEvidence(Territory, Target, Guard,
			ETerritoryStealthEvidence::ThrowableDistraction,
			Profile->ThrowableDistractionSuspicion, Stimulus.StimulusLocation,
			FVector::ZeroVector, false);
		return;
	}

	if (StimulusTag == FName(TEXT("Gunshot")))
	{
		RecentGunshots.FindOrAdd(Target) = {Now, Stimulus.StimulusLocation};
		const bool bCurrentlySeen = CurrentlySeenTargets.Contains(Target);
		Control->ReportStealthEvidence(Territory, Target, Guard,
			bCurrentlySeen ? ETerritoryStealthEvidence::FireSeen
				: ETerritoryStealthEvidence::Gunshot,
			bCurrentlySeen ? 1.f : Profile->GunshotSuspicion,
			Stimulus.StimulusLocation, FVector::ZeroVector, bCurrentlySeen);
		return;
	}

	// Narrative weapon impacts currently use an empty hearing tag. Treat one as a
	// missed-shot impact only when it can be correlated to a recent Gunshot from
	// the same player; unrelated untagged sounds remain Narrative-owned.
	if (StimulusTag.IsNone())
	{
		const FRecentGunshot* Shot = RecentGunshots.Find(Target);
		if (Shot && Now - Shot->WorldTime <= Profile->ShotCorrelationWindow)
		{
			const FVector Direction =
				(Stimulus.StimulusLocation - Shot->MuzzleLocation).GetSafeNormal();
			Control->ReportStealthEvidence(Territory, Target, Guard,
				ETerritoryStealthEvidence::BulletImpact,
				Profile->BulletImpactSuspicion, Stimulus.StimulusLocation,
				Direction, false);
		}
	}
}

void UTerritoryStealthObserverComponent::RefreshVisibleTargets()
{
	ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	UWorld* World = GetWorld();
	UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Guard || !Guard->HasAuthority() || !Territory || !Profile || !Control)
	{
		return;
	}
	if (!BoundPerception.IsValid()) BindToCurrentPerception();

	for (auto It = CurrentlySeenTargets.CreateIterator(); It; ++It)
	{
		AActor* Target = It->Key.Get();
		if (!Target || Target->IsActorBeingDestroyed()
			|| !Territory->ContainsPoint(Target->GetActorLocation()))
		{
			It.RemoveCurrent();
			continue;
		}
		const float EffectiveStrength = CalculateEffectiveSightStrength(
			Target, It->Value.RawStrength);
		if (IsTargetFiring(Target) && Profile->bFireWhileSeenExposes)
		{
			Control->ReportStealthEvidence(Territory, Target, Guard,
				ETerritoryStealthEvidence::FireSeen, 1.f,
				Target->GetActorLocation(), FVector::ZeroVector, true);
		}
		else
		{
			Control->ReportStealthEvidence(Territory, Target, Guard,
				ETerritoryStealthEvidence::Sight, EffectiveStrength,
				Target->GetActorLocation(), FVector::ZeroVector,
				EffectiveStrength >= Profile->ImmediateSightExposureThreshold);
		}
	}

	const double Now = World->GetTimeSeconds();
	for (auto It = RecentGunshots.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()
			|| Now - It->Value.WorldTime > Profile->ShotCorrelationWindow)
		{
			It.RemoveCurrent();
		}
	}
}
