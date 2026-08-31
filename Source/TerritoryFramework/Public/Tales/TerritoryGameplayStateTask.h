#pragma once

#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NarrativeActorProvider.h"
#include "Tales/QuestTask.h"
#include "TerritoryGameplayStateTask.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/** GAS state queries not covered by Narrative's single Wait Gameplay Tag Added task. */
UENUM(BlueprintType)
enum class ETerritoryGameplayStateObjective : uint8
{
	AllTagsPresent UMETA(DisplayName="All Gameplay Tags Are Present",
		ToolTip="Complete when the subject owns every Required Tag."),
	AnyTagPresent UMETA(DisplayName="Any Gameplay Tag Is Present",
		ToolTip="Complete when the subject owns at least one Required Tag."),
	AllTagsAbsent UMETA(DisplayName="Gameplay Tags Are Removed",
		ToolTip="Complete when the subject owns none of the Required Tags."),
	AttributeAtLeast UMETA(DisplayName="Gameplay Attribute Reaches Minimum",
		ToolTip="Complete when the selected GAS attribute is greater than or equal to Threshold."),
	AttributeAtMost UMETA(DisplayName="Gameplay Attribute Falls to Maximum",
		ToolTip="Complete when the selected GAS attribute is less than or equal to Threshold.")
};

/** Community Narrative Task for replicated GAS tags and attributes. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Gameplay State Task",
		ToolTip="Narrative Task that observes an Ability System Component's replicated tags or attributes without applying effects."))
class TERRITORYFRAMEWORK_API UTerritoryGameplayStateTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	/** Optional provider. Empty follows the quest owner's pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Community Task|Subject",
		meta=(ToolTip="Actor whose Ability System Component is observed. Easy example: Find NPC watches a boss; empty watches the player pawn."))
	TObjectPtr<UNarrativeActorProvider> SubjectProvider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|GAS")
	ETerritoryGameplayStateObjective Objective =
		ETerritoryGameplayStateObjective::AllTagsPresent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|GAS",
		meta=(EditCondition="Objective == ETerritoryGameplayStateObjective::AllTagsPresent || Objective == ETerritoryGameplayStateObjective::AnyTagPresent || Objective == ETerritoryGameplayStateObjective::AllTagsAbsent",
			EditConditionHides,
			ToolTip="Tags read from the subject's real ASC. Easy example: Narrative.State.Weapon.IsAiming and Narrative.State.Weapon.IsFiring."))
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|GAS",
		meta=(EditCondition="Objective == ETerritoryGameplayStateObjective::AllTagsPresent || Objective == ETerritoryGameplayStateObjective::AnyTagPresent || Objective == ETerritoryGameplayStateObjective::AllTagsAbsent",
			EditConditionHides,
			ToolTip="When enabled, parent tags do not satisfy child tags and child tags do not satisfy parent tags."))
	bool bExactTagMatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|GAS",
		meta=(EditCondition="Objective == ETerritoryGameplayStateObjective::AttributeAtLeast || Objective == ETerritoryGameplayStateObjective::AttributeAtMost",
			EditConditionHides,
			ToolTip="Replicated GAS attribute to observe. Easy example: Narrative Health."))
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|GAS",
		meta=(EditCondition="Objective == ETerritoryGameplayStateObjective::AttributeAtLeast || Objective == ETerritoryGameplayStateObjective::AttributeAtMost",
			EditConditionHides,
			ToolTip="Value compared with the selected attribute."))
	float Threshold = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|GAS",
		meta=(ToolTip="Complete immediately when the state is already true. Disable it when the story requires a new tag or attribute transition after this task begins."))
	bool bCompleteIfAlreadySatisfied = true;

	UFUNCTION(BlueprintPure, Category="Community Task|Preview",
		meta=(ToolTip="Checks the subject's current ASC state without adding tags, effects, attributes, or quest progress."))
	bool IsGameplayStateSatisfiedBy(const AActor* Subject) const;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskProgressText_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	AActor* ResolveSubject() const;
	UAbilitySystemComponent* ResolveAbilitySystem(const AActor* Subject) const;
	void BindSubject(AActor* Subject);
	void UnbindSubject();
	void Evaluate(bool bInitialEvaluation);
	bool HasConfiguredTag(const UAbilitySystemComponent* AbilitySystem,
		const FGameplayTag& Tag) const;

	UFUNCTION() void HandleProviderActorReady(AActor* Actor);
	void HandleGameplayTagChanged(FGameplayTag Tag, int32 NewCount);
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY() TWeakObjectPtr<AActor> CachedSubject;
	UPROPERTY() TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystem;
};
