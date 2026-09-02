#include "Tales/TerritoryCombatProgressTask.h"

#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeAttributeSetBase.h"

void UTerritoryCombatProgressTask::BeginTask()
{
	bObservedDeadState = false;
	if (SubjectProvider || CounterpartyProvider) TickInterval = 0.25f;
	Super::BeginTask();
	if (IsComplete()) return;

	if (SubjectProvider)
	{
		SubjectProvider->OnProviderActorReady.AddUniqueDynamic(
			this, &UTerritoryCombatProgressTask::HandleSubjectReady);
	}
	if (CounterpartyProvider)
	{
		CounterpartyProvider->OnProviderActorReady.AddUniqueDynamic(
			this, &UTerritoryCombatProgressTask::HandleCounterpartyReady);
	}
	CachedCounterparty = ResolveCounterparty();
	if (AActor* Subject = ResolveSubject()) BindSubject(Subject);
}

void UTerritoryCombatProgressTask::EndTask()
{
	if (SubjectProvider)
	{
		SubjectProvider->OnProviderActorReady.RemoveDynamic(
			this, &UTerritoryCombatProgressTask::HandleSubjectReady);
	}
	if (CounterpartyProvider)
	{
		CounterpartyProvider->OnProviderActorReady.RemoveDynamic(
			this, &UTerritoryCombatProgressTask::HandleCounterpartyReady);
	}
	UnbindSubject();
	CachedCounterparty.Reset();
	bObservedDeadState = false;
	Super::EndTask();
}

void UTerritoryCombatProgressTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (IsComplete()) return;
	if (!CachedSubject.IsValid())
	{
		if (AActor* Subject = ResolveSubject()) BindSubject(Subject);
	}
	if (CounterpartyProvider && !CachedCounterparty.IsValid())
	{
		CachedCounterparty = ResolveCounterparty();
	}
}

int32 UTerritoryCombatProgressTask::MagnitudeToProgress(float Magnitude)
{
	return Magnitude > 0.f
		? FMath::Max(1, FMath::RoundToInt(Magnitude)) : 0;
}

AActor* UTerritoryCombatProgressTask::ResolveSubject() const
{
	return SubjectProvider ? SubjectProvider->ProvideActor(this) : OwningPawn;
}

AActor* UTerritoryCombatProgressTask::ResolveCounterparty() const
{
	return CounterpartyProvider
		? CounterpartyProvider->ProvideActor(this) : nullptr;
}

UNarrativeAbilitySystemComponent*
UTerritoryCombatProgressTask::ResolveNarrativeAbilitySystem(AActor* Actor) const
{
	return FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Actor);
}

void UTerritoryCombatProgressTask::BindSubject(AActor* Subject)
{
	UNarrativeAbilitySystemComponent* AbilitySystem =
		ResolveNarrativeAbilitySystem(Subject);
	if (!Subject || !AbilitySystem
		|| (CachedSubject.Get() == Subject
			&& CachedAbilitySystem.Get() == AbilitySystem)) return;

	UnbindSubject();
	CachedSubject = Subject;
	CachedAbilitySystem = AbilitySystem;
	bObservedDeadState = AbilitySystem->IsDead();
	AbilitySystem->OnDealtDamage.AddUniqueDynamic(
		this, &UTerritoryCombatProgressTask::HandleDealtDamage);
	AbilitySystem->OnDamagedBy.AddUniqueDynamic(
		this, &UTerritoryCombatProgressTask::HandleDamagedBy);
	AbilitySystem->OnHealedBy.AddUniqueDynamic(
		this, &UTerritoryCombatProgressTask::HandleHealedBy);
	AbilitySystem->OnDeathStateChanged.AddUniqueDynamic(
		this, &UTerritoryCombatProgressTask::HandleDeathStateChanged);

	if (Objective == ETerritoryCombatProgressObjective::Die
		&& bCompleteIfAlreadyDead && AbilitySystem->IsDead()) CompleteTask();
}

void UTerritoryCombatProgressTask::UnbindSubject()
{
	if (UNarrativeAbilitySystemComponent* AbilitySystem =
		CachedAbilitySystem.Get())
	{
		AbilitySystem->OnDealtDamage.RemoveDynamic(
			this, &UTerritoryCombatProgressTask::HandleDealtDamage);
		AbilitySystem->OnDamagedBy.RemoveDynamic(
			this, &UTerritoryCombatProgressTask::HandleDamagedBy);
		AbilitySystem->OnHealedBy.RemoveDynamic(
			this, &UTerritoryCombatProgressTask::HandleHealedBy);
		AbilitySystem->OnDeathStateChanged.RemoveDynamic(
			this, &UTerritoryCombatProgressTask::HandleDeathStateChanged);
	}
	CachedAbilitySystem.Reset();
	CachedSubject.Reset();
}

bool UTerritoryCombatProgressTask::MatchesCounterparty(
	const UNarrativeAbilitySystemComponent* Other) const
{
	if (!CounterpartyProvider) return true;
	const AActor* Expected = CachedCounterparty.IsValid()
		? CachedCounterparty.Get() : ResolveCounterparty();
	if (!Expected || !Other) return false;
	const AActor* Avatar = Other->GetAvatarActor();
	const AActor* Owner = Other->GetOwnerActor();
	return Expected == Avatar || Expected == Owner;
}

bool UTerritoryCombatProgressTask::MatchesEffect(
	const FGameplayEffectSpec& Spec) const
{
	if (RequiredEffectTags.IsEmpty()) return true;
	FGameplayTagContainer EffectTags;
	Spec.GetAllAssetTags(EffectTags);
	return EffectTags.HasAll(RequiredEffectTags);
}

void UTerritoryCombatProgressTask::AddMagnitudeProgress(float Magnitude)
{
	const int32 Progress = MagnitudeToProgress(Magnitude);
	if (!IsComplete() && Progress > 0) AddProgress(Progress);
}

void UTerritoryCombatProgressTask::HandleSubjectReady(AActor* Actor)
{
	BindSubject(Actor);
}

void UTerritoryCombatProgressTask::HandleCounterpartyReady(AActor* Actor)
{
	CachedCounterparty = Actor;
}

void UTerritoryCombatProgressTask::HandleDealtDamage(
	UNarrativeAbilitySystemComponent* DamagedASC, const float Damage,
	const FGameplayEffectSpec& Spec)
{
	if (IsComplete() || !MatchesCounterparty(DamagedASC)
		|| !MatchesEffect(Spec) || Damage <= 0.f) return;
	if (Objective == ETerritoryCombatProgressObjective::DealDamageAmount)
		AddMagnitudeProgress(Damage);
	else if (Objective == ETerritoryCombatProgressObjective::DealHitCount)
		AddProgress(1);
}

void UTerritoryCombatProgressTask::HandleDamagedBy(
	UNarrativeAbilitySystemComponent* DamagerASC, const float Damage,
	const FGameplayEffectSpec& Spec)
{
	if (IsComplete() || !MatchesCounterparty(DamagerASC)
		|| !MatchesEffect(Spec) || Damage <= 0.f) return;
	if (Objective == ETerritoryCombatProgressObjective::TakeDamageAmount)
		AddMagnitudeProgress(Damage);
	else if (Objective == ETerritoryCombatProgressObjective::SurviveHitCount
		&& CachedAbilitySystem.IsValid()
		&& !CachedAbilitySystem->IsDead()
		&& CachedAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f) AddProgress(1);
}

void UTerritoryCombatProgressTask::HandleHealedBy(
	UNarrativeAbilitySystemComponent* HealerASC, const float Amount,
	const FGameplayEffectSpec& Spec)
{
	if (Objective == ETerritoryCombatProgressObjective::ReceiveHealingAmount
		&& !IsComplete() && MatchesCounterparty(HealerASC)
		&& MatchesEffect(Spec)) AddMagnitudeProgress(Amount);
}

void UTerritoryCombatProgressTask::HandleDeathStateChanged(
	AActor* ChangedActor, UNarrativeAbilitySystemComponent* ChangedASC,
	const bool bIsDead)
{
	if (IsComplete() || ChangedASC != CachedAbilitySystem.Get()) return;
	const bool bWasDead = bObservedDeadState;
	bObservedDeadState = bIsDead;
	if (Objective == ETerritoryCombatProgressObjective::Die && bIsDead)
		CompleteTask();
	else if (Objective == ETerritoryCombatProgressObjective::Revive
		&& bWasDead && !bIsDead) CompleteTask();
	(void)ChangedActor;
}

FText UTerritoryCombatProgressTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	switch (Objective)
	{
	case ETerritoryCombatProgressObjective::DealDamageAmount:
		return NSLOCTEXT("CommunityTask", "DealDamage", "Deal damage");
	case ETerritoryCombatProgressObjective::TakeDamageAmount:
		return NSLOCTEXT("CommunityTask", "TakeDamage", "Endure damage");
	case ETerritoryCombatProgressObjective::ReceiveHealingAmount:
		return NSLOCTEXT("CommunityTask", "ReceiveHealing", "Receive healing");
	case ETerritoryCombatProgressObjective::DealHitCount:
		return NSLOCTEXT("CommunityTask", "LandHits", "Land successful hits");
	case ETerritoryCombatProgressObjective::SurviveHitCount:
		return NSLOCTEXT("CommunityTask", "SurviveHits", "Survive enemy hits");
	case ETerritoryCombatProgressObjective::Die:
		return NSLOCTEXT("CommunityTask", "SubjectDies", "Wait for the subject to fall");
	case ETerritoryCombatProgressObjective::Revive:
		return NSLOCTEXT("CommunityTask", "SubjectRevives", "Revive the subject");
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FText UTerritoryCombatProgressTask::GetTaskProgressText_Implementation() const
{
	return Objective == ETerritoryCombatProgressObjective::Die
		|| Objective == ETerritoryCombatProgressObjective::Revive
		? FText::GetEmpty() : Super::GetTaskProgressText_Implementation();
}

AActor* UTerritoryCombatProgressTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return CachedSubject.IsValid() ? CachedSubject.Get() : ResolveSubject();
}
