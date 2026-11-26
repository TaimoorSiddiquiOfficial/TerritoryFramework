#include "AI/TerritoryStealthObserverComponent.h"

#include "AbilitySystemComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AIController.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPerceptionProfile.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryStealthTags.h"
#include "Core/TerritoryVolume.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
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
	// Guard quality tier: write this profile onto the vendor controller's existing sense configs.
	if (PerceptionProfile && PerceptionProfileAppliedTo.Get() != Perception
		&& PerceptionProfile->ApplyTo(Perception))
	{
		PerceptionProfileAppliedTo = Perception;
	}
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
	if (UWorld* World = GetWorld(); World && !World->bIsTearingDown)
	{
		if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
			Control->ForgetStealthObserver(ObservedTerritory.Get(), GetOwner());
	}
	ObservedTerritory.Reset();
	CurrentlySeenTargets.Reset();
	RecentGunshots.Reset();
	LastSightRefreshWorldTime = -1.0;
}

bool UTerritoryStealthObserverComponent::IsTargetFiring(AActor* Target) const
{
	const UAbilitySystemComponent* ASC =
		FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Target);
	return ASC && ASC->HasMatchingGameplayTag(
		FNarrativeGameplayTags::Get().State_Weapon_IsFiring);
}

bool UTerritoryStealthObserverComponent::IsNarrativeInvisible(AActor* Target) const
{
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	if (!Profile || !Profile->bRespectNarrativeInvisibleTag || !Target)
	{
		return false;
	}
	const UAbilitySystemComponent* ASC =
		FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Target);
	return ASC && ASC->HasMatchingGameplayTag(
		FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
}

bool UTerritoryStealthObserverComponent::ShouldForcePointBlankExposure(
	AActor* Target) const
{
	const ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	return Guard && Target && Profile && Profile->bPointBlankSightAlwaysExposes
		&& Profile->PointBlankSightExposureDistance > 0.f
		&& !IsNarrativeInvisible(Target)
		&& FVector::DistSquared(Guard->GetActorLocation(), Target->GetActorLocation())
			<= FMath::Square(Profile->PointBlankSightExposureDistance);
}

float UTerritoryStealthObserverComponent::CalculateEffectiveSightStrength(
	AActor* Target, const float RawSightStrength) const
{
	const ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	const ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	// A hiding spot removes the sight channel entirely until the player leaves it.
	if (Territory && IsValid(Target))
	{
		if (const UTerritoryControlSubsystem* Control = GetWorld()
			? GetWorld()->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
			Control && Control->IsTargetHidden(Territory, Target))
		{
			return 0.f;
		}
	}
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	if (!Profile || !Target) return FMath::Clamp(RawSightStrength, 0.f, 1.f);
	if (IsNarrativeInvisible(Target))
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

bool UTerritoryStealthObserverComponent::ShouldSuppressAttackGoal(AActor* Target) const
{
	const ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	const ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	const UTerritoryControlSubsystem* Control = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;

	// Fail open: any missing piece must leave the guard's normal combat untouched.
	if (!Guard || !Guard->HasAuthority() || !Guard->IsAlive()
		|| !IsValid(Target) || !Territory || !Profile || !Control
		|| !Control->IsStealthInfiltrationEnabled(Territory))
	{
		return false;
	}

	// A hidden target keeps no attack goal while the hiding spot holds it.
	if (Control->IsTargetHidden(Territory, Target))
	{
		return true;
	}

	FTerritoryInfiltrationSnapshot Snapshot;
	if (!Control->GetInfiltrationSnapshot(Territory, Target, Snapshot))
	{
		return false;
	}

	FAIStimulus Sight;
	const float Raw = ReadCurrentSight(Target, Sight) ? FMath::Max(0.f, Sight.Strength) : 0.f;
	// Same admission rule the Place and the guard's combat check use, so the goal can never
	// outlive the permission to attack (that mismatch made guards chase a holstered player).
	const bool bAdmittedToCombat = UTerritoryBlueprintLibrary::DoesPawnAdmitStoryContest(
		Target, Territory, Profile,
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidence(
			this, Snapshot, Profile->AggressionEvidenceWindow));
	return ShouldSuppressAttackGoalForState(
		Snapshot.ExposureState,
		ShouldForcePointBlankExposure(Target),
		Profile->bFireWhileSeenExposes && IsTargetFiring(Target),
		bAdmittedToCombat,
		CalculateEffectiveSightStrength(Target, Raw),
		Profile->MinimumSightEvidence);
}

bool UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
	const ETerritoryExposureState ExposureState, const bool bPointBlank,
	const bool bFiringWhileSeen, const bool bAdmittedToCombat,
	const float EffectiveSight, const float MinimumSightEvidence)
{
	// Not a physical threat yet (holstered, no war, no recent aggression): investigate and warn,
	// never chase. This is what stops the follow-out-and-return loop.
	if (!bAdmittedToCombat) return true;
	// A hidden player standing in the guard's face is caught, never ignored.
	if (bPointBlank) return false;
	if (ExposureState == ETerritoryExposureState::Exposed) return false;
	// Firing while the guard has real sight is aggression, not stealth.
	if (bFiringWhileSeen) return false;
	return EffectiveSight < MinimumSightEvidence;
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
	if (!Guard || !Guard->HasAuthority() || !Guard->IsAlive() || !FTerritoryNarrativeProAdapter::IsCharacterReady(Guard)
		|| !Territory || !Target || !Profile || !Control
		|| !Control->IsStealthInfiltrationEnabled(Territory)
		|| (!Territory->ContainsPoint(Target->GetActorLocation()) && !Profile->bRespondToOutsideThreats))
	{
		return;
	}

	const TSubclassOf<UAISense> SenseClass =
		UAIPerceptionSystem::GetSenseClassForStimulus(this, Stimulus);
	if (SenseClass == UAISense_Sight::StaticClass())
	{
		// Seeing an owned projectile or distraction is not seeing its hidden owner.
		if (Target != SensedActor) return;
		// Visibility callbacks add no elapsed sight time. The timer samples Native's
		// current store, including strength changes that do not produce another edge.
		ReportSight(Target, Stimulus, 0.f);
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

	// Narrative's foley notify reports footsteps with tag "Footstep" and a loudness/range that
	// already scale with the character's StealthRating (100 = silent). Consume them as anonymous
	// evidence so crouching and the Sneak perks change how guards react.
	if (StimulusTag == FName(TEXT("Footstep")))
	{
		if (!Profile->bFootstepsRaiseSuspicion) return;
		if (const double* Last = RecentFootsteps.Find(Target))
		{
			if (Now - *Last < Profile->FootstepEvidenceCooldown) return;
		}
		RecentFootsteps.Add(Target, Now);
		const float Loudness = FMath::Clamp(Stimulus.Strength, 0.f, 1.f);
		const float Weight = Profile->FootstepSuspicion * (Loudness > 0.f ? Loudness : 1.f);
		Control->ReportStealthEvidence(Territory, Target, Guard,
			ETerritoryStealthEvidence::Footstep, Weight,
			Stimulus.StimulusLocation, FVector::ZeroVector, false);
		return;
	}

	if (StimulusTag == FName(TEXT("Gunshot")))
	{
		RecentGunshots.FindOrAdd(Target) = {Now, Stimulus.StimulusLocation};
		FAIStimulus CurrentSight;
		const bool bCurrentlySeen = ReadCurrentSight(Target, CurrentSight)
			&& CalculateEffectiveSightStrength(Target, CurrentSight.Strength) > 0.f
			&& (CalculateEffectiveSightStrength(Target, CurrentSight.Strength) >= Profile->MinimumSightEvidence
				|| ShouldForcePointBlankExposure(Target));
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

bool UTerritoryStealthObserverComponent::ReadCurrentSight(AActor* Target, FAIStimulus& OutStimulus) const
{
	UAIPerceptionComponent* Perception = BoundPerception.Get();
	FActorPerceptionBlueprintInfo Info;
	if (!Perception || !IsValid(Target) || !Perception->GetActorsPerception(Target, Info)) return false;
	for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
	{
		if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>()
			&& Stimulus.WasSuccessfullySensed() && !Stimulus.IsExpired()
			&& FMath::IsFinite(Stimulus.Strength))
		{
			OutStimulus = Stimulus;
			return true;
		}
	}
	return false;
}

void UTerritoryStealthObserverComponent::ReportSight(AActor* Target,
	const FAIStimulus& Stimulus, float ElapsedSeconds)
{
	ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	UTerritoryControlSubsystem* Control = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Guard || !Guard->HasAuthority() || !Guard->IsAlive() || !FTerritoryNarrativeProAdapter::IsCharacterReady(Guard)
		|| !IsValid(Target) || !Territory || !Profile || !Control) return;
	ObservedTerritory = Territory;
	const bool bValidSight = Stimulus.WasSuccessfullySensed() && !Stimulus.IsExpired()
		&& FMath::IsFinite(Stimulus.Strength)
		&& (Territory->ContainsPoint(Target->GetActorLocation())
			|| (Profile->bRespondToOutsideThreats && Control->IsInfiltratorExposed(Territory, Target)));
	const float Strength = bValidSight ? CalculateEffectiveSightStrength(Target, Stimulus.Strength) : 0.f;
	const bool bPointBlank = bValidSight && Stimulus.Strength > 0.f && ShouldForcePointBlankExposure(Target);
	if (bValidSight)
		CurrentlySeenTargets.Add(Target, {Stimulus.Strength, Stimulus.StimulusLocation});
	else CurrentlySeenTargets.Remove(Target);
	const bool bFireSeen = (bPointBlank || (Strength > 0.f && Strength >= Profile->MinimumSightEvidence))
		&& IsTargetFiring(Target) && Profile->bFireWhileSeenExposes;
	Control->ReportStealthEvidence(Territory, Target, Guard,
		bFireSeen ? ETerritoryStealthEvidence::FireSeen : ETerritoryStealthEvidence::Sight,
		bFireSeen ? 1.f : Strength, Target->GetActorLocation(), FVector::ZeroVector,
		bFireSeen || bPointBlank, ElapsedSeconds);
}

void UTerritoryStealthObserverComponent::RefreshVisibleTargets()
{
	ATerritoryGuardCharacter* Guard = GetTerritoryGuard();
	ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	const UTerritoryStealthProfile* Profile = GetActiveProfile();
	UWorld* World = GetWorld();
	UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Guard || !Guard->HasAuthority() || !Guard->IsAlive() || !FTerritoryNarrativeProAdapter::IsCharacterReady(Guard)
		|| !Territory || !Profile || !Control || !Control->IsStealthInfiltrationEnabled(Territory))
	{
		UnbindFromPerception();
		return;
	}
	if (ObservedTerritory.IsValid() && ObservedTerritory.Get() != Territory) UnbindFromPerception();
	// Controller replacement, reserve spawn, and late definition loading all use
	// the same binding path. Seed from the current store, not only future callbacks.
	if (!BindToCurrentPerception()) { UnbindFromPerception(); return; }
	ObservedTerritory = Territory;
	const double Now = World->GetTimeSeconds();
	const float ElapsedSeconds = LastSightRefreshWorldTime >= 0.0
		? FMath::Clamp(static_cast<float>(Now - LastSightRefreshWorldTime), 0.f, 1.f) : 0.f;
	LastSightRefreshWorldTime = Now;
	TArray<AActor*> VisibleActors;
	BoundPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);
	TSet<TWeakObjectPtr<AActor>> Targets;
	for (const auto& Pair : CurrentlySeenTargets) Targets.Add(Pair.Key);
	for (AActor* Actor : VisibleActors)
	{
		if (AActor* Target = ResolvePlayerSource(Actor); Target && Target == Actor
			&& (Territory->ContainsPoint(Actor->GetActorLocation())
				|| (Profile->bRespondToOutsideThreats && Control->IsInfiltratorExposed(Territory, Target)))) Targets.Add(Target);
	}
	// Report from a copy: exposure events may remove targets or destroy this guard.
	for (const TWeakObjectPtr<AActor>& Target : Targets)
	{
		if (!Guard->IsAlive() || Guard->IsActorBeingDestroyed()
			|| Guard->GetOwningTerritory() != Territory || !BoundPerception.IsValid()) break;
		if (!Target.IsValid() || Target->IsActorBeingDestroyed())
		{ CurrentlySeenTargets.Remove(Target); continue; }
		FAIStimulus Stimulus;
		ReadCurrentSight(Target.Get(), Stimulus);
		ReportSight(Target.Get(), Stimulus, ElapsedSeconds);

		// A uniform that fails this Place's policy is doubt, not proof: it accumulates slowly while
		// the disguised player stays visible, instead of granting a free pass or instant exposure.
		if (Profile->bDisguiseRaisesSuspicion && Profile->DisguiseSuspicionPerSecond > 0.f)
		{
			if (const UTerritoryDisguiseSubsystem* Disguises =
				World->GetSubsystem<UTerritoryDisguiseSubsystem>())
			{
				FTerritoryDisguiseSnapshot Disguise;
				FText Reason;
				if (Disguises->GetDisguiseSnapshot(Target.Get(), Disguise) && Disguise.bActive
					&& !Disguises->IsDisguiseAccepted(Target.Get(), Territory,
						Territory->GetOwningFaction(), Reason))
				{
					Control->ReportStealthEvidence(Territory, Target.Get(), Guard,
						ETerritoryStealthEvidence::Disguise,
						Profile->DisguiseSuspicionPerSecond * FMath::Clamp(ElapsedSeconds, 0.f, 1.f),
						Target->GetActorLocation(), FVector::ZeroVector, false, 0.f);
				}
			}
		}
	}

	for (auto It = RecentGunshots.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()
			|| Now - It->Value.WorldTime > Profile->ShotCorrelationWindow)
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = RecentFootsteps.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid()
			|| Now - It->Value > FMath::Max(Profile->FootstepEvidenceCooldown, 1.0f))
		{
			It.RemoveCurrent();
		}
	}
}
