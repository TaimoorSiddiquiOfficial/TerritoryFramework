#include "Tales/TerritoryGameplayStateTask.h"

#include "AbilitySystemComponent.h"
#include "Framework/TerritoryNarrativeProAdapter.h"

void UTerritoryGameplayStateTask::BeginTask()
{
	if (SubjectProvider) TickInterval = 0.25f;
	Super::BeginTask();
	if (IsComplete()) return;
	if (SubjectProvider)
	{
		SubjectProvider->OnProviderActorReady.AddUniqueDynamic(
			this, &UTerritoryGameplayStateTask::HandleProviderActorReady);
	}
	if (AActor* Subject = ResolveSubject()) BindSubject(Subject);
	Evaluate(true);
}

void UTerritoryGameplayStateTask::EndTask()
{
	if (SubjectProvider)
	{
		SubjectProvider->OnProviderActorReady.RemoveDynamic(
			this, &UTerritoryGameplayStateTask::HandleProviderActorReady);
	}
	UnbindSubject();
	Super::EndTask();
}

void UTerritoryGameplayStateTask::TickTask_Implementation()
{
	Super::TickTask_Implementation();
	if (IsComplete()) return;
	AActor* Subject = CachedSubject.Get();
	if (!Subject)
	{
		Subject = ResolveSubject();
		if (Subject)
		{
			BindSubject(Subject);
			Evaluate(true);
		}
	}
}

bool UTerritoryGameplayStateTask::IsGameplayStateSatisfiedBy(
	const AActor* Subject) const
{
	const UAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Subject);
	if (!AbilitySystem) return false;
	switch (Objective)
	{
	case ETerritoryGameplayStateObjective::AllTagsPresent:
		if (RequiredTags.IsEmpty()) return false;
		for (const FGameplayTag& Tag : RequiredTags)
		{
			if (!HasConfiguredTag(AbilitySystem, Tag)) return false;
		}
		return true;
	case ETerritoryGameplayStateObjective::AnyTagPresent:
		if (RequiredTags.IsEmpty()) return false;
		for (const FGameplayTag& Tag : RequiredTags)
		{
			if (HasConfiguredTag(AbilitySystem, Tag)) return true;
		}
		return false;
	case ETerritoryGameplayStateObjective::AllTagsAbsent:
		if (RequiredTags.IsEmpty()) return false;
		for (const FGameplayTag& Tag : RequiredTags)
		{
			if (HasConfiguredTag(AbilitySystem, Tag)) return false;
		}
		return true;
	case ETerritoryGameplayStateObjective::AttributeAtLeast:
		return Attribute.IsValid()
			&& AbilitySystem->GetNumericAttribute(Attribute) >= Threshold;
	case ETerritoryGameplayStateObjective::AttributeAtMost:
		return Attribute.IsValid()
			&& AbilitySystem->GetNumericAttribute(Attribute) <= Threshold;
	default:
		return false;
	}
}

AActor* UTerritoryGameplayStateTask::ResolveSubject() const
{
	return SubjectProvider ? SubjectProvider->ProvideActor(this) : OwningPawn;
}

UAbilitySystemComponent* UTerritoryGameplayStateTask::ResolveAbilitySystem(
	const AActor* Subject) const
{
	return FTerritoryNarrativeProAdapter::ResolveAbilitySystem(
		const_cast<AActor*>(Subject));
}

void UTerritoryGameplayStateTask::BindSubject(AActor* Subject)
{
	UAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Subject);
	if (!Subject || !AbilitySystem
		|| (CachedSubject.Get() == Subject
			&& CachedAbilitySystem.Get() == AbilitySystem)) return;
	UnbindSubject();
	CachedSubject = Subject;
	CachedAbilitySystem = AbilitySystem;

	if (Objective == ETerritoryGameplayStateObjective::AttributeAtLeast
		|| Objective == ETerritoryGameplayStateObjective::AttributeAtMost)
	{
		if (Attribute.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddUObject(this,
					&UTerritoryGameplayStateTask::HandleAttributeChanged);
		}
	}
	else
	{
		for (const FGameplayTag& Tag : RequiredTags)
		{
			AbilitySystem->RegisterGameplayTagEvent(
				Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this,
					&UTerritoryGameplayStateTask::HandleGameplayTagChanged);
		}
	}
}

void UTerritoryGameplayStateTask::UnbindSubject()
{
	if (UAbilitySystemComponent* AbilitySystem = CachedAbilitySystem.Get())
	{
		if (Attribute.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute)
				.RemoveAll(this);
		}
		for (const FGameplayTag& Tag : RequiredTags)
		{
			AbilitySystem->RegisterGameplayTagEvent(
				Tag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		}
	}
	CachedAbilitySystem.Reset();
	CachedSubject.Reset();
}

void UTerritoryGameplayStateTask::Evaluate(bool bInitialEvaluation)
{
	if (IsComplete() || (bInitialEvaluation && !bCompleteIfAlreadySatisfied)) return;
	if (IsGameplayStateSatisfiedBy(CachedSubject.Get())) CompleteTask();
}

bool UTerritoryGameplayStateTask::HasConfiguredTag(
	const UAbilitySystemComponent* AbilitySystem, const FGameplayTag& Tag) const
{
	return AbilitySystem && Tag.IsValid()
		&& (bExactTagMatch ? AbilitySystem->GetTagCount(Tag) > 0
			: AbilitySystem->HasMatchingGameplayTag(Tag));
}

void UTerritoryGameplayStateTask::HandleProviderActorReady(AActor* Actor)
{
	BindSubject(Actor);
	Evaluate(true);
}

void UTerritoryGameplayStateTask::HandleGameplayTagChanged(
	FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	(void)NewCount;
	Evaluate(false);
}

void UTerritoryGameplayStateTask::HandleAttributeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	(void)ChangeData;
	Evaluate(false);
}

FText UTerritoryGameplayStateTask::GetTaskDescription_Implementation() const
{
	if (!DescriptionOverride.IsEmptyOrWhitespace()) return DescriptionOverride;
	switch (Objective)
	{
	case ETerritoryGameplayStateObjective::AllTagsPresent:
		return NSLOCTEXT("CommunityTask", "AllTags", "Gain all required gameplay states");
	case ETerritoryGameplayStateObjective::AnyTagPresent:
		return NSLOCTEXT("CommunityTask", "AnyTag", "Gain one required gameplay state");
	case ETerritoryGameplayStateObjective::AllTagsAbsent:
		return NSLOCTEXT("CommunityTask", "NoTags", "Remove the required gameplay states");
	case ETerritoryGameplayStateObjective::AttributeAtLeast:
		return FText::Format(NSLOCTEXT("CommunityTask", "AttributeMinimum", "Raise {0} to at least {1}"),
			FText::FromString(Attribute.GetName()), FText::AsNumber(Threshold));
	case ETerritoryGameplayStateObjective::AttributeAtMost:
		return FText::Format(NSLOCTEXT("CommunityTask", "AttributeMaximum", "Lower {0} to {1} or less"),
			FText::FromString(Attribute.GetName()), FText::AsNumber(Threshold));
	default:
		return Super::GetTaskDescription_Implementation();
	}
}

FText UTerritoryGameplayStateTask::GetTaskProgressText_Implementation() const
{
	return FText::GetEmpty();
}

AActor* UTerritoryGameplayStateTask::GetNavigationMarkerAttachActor_Implementation() const
{
	return CachedSubject.IsValid() ? CachedSubject.Get() : ResolveSubject();
}
