#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "NarrativeActorProvider.h"
#include "Tales/QuestTask.h"
#include "TerritoryCombatProgressTask.generated.h"

/** Combat milestones exposed as reusable Narrative quest tasks. */
UENUM(BlueprintType)
enum class ETerritoryCombatProgressObjective : uint8
{
	DealDamageAmount UMETA(DisplayName="Deal Damage",
		ToolTip="Add the rounded Narrative damage dealt to matching targets."),
	TakeDamageAmount UMETA(DisplayName="Take Damage",
		ToolTip="Add the rounded Narrative damage received from matching sources."),
	ReceiveHealingAmount UMETA(DisplayName="Receive Healing",
		ToolTip="Add the rounded Narrative healing received from matching healers."),
	DealHitCount UMETA(DisplayName="Land Hits",
		ToolTip="Add one for every positive Narrative damage event dealt to a matching target."),
	SurviveHitCount UMETA(DisplayName="Survive Hits",
		ToolTip="Add one for every positive Narrative damage event that leaves the subject alive."),
	Die UMETA(DisplayName="Subject Dies",
		ToolTip="Complete when Narrative changes the subject to its dead state."),
	Revive UMETA(DisplayName="Subject Is Revived",
		ToolTip="Complete when Narrative changes the subject from dead back to alive.")
};

/**
 * Community Narrative Task backed by Narrative's authoritative ASC delegates.
 * It observes combat only; it never applies damage, healing, effects, or tags.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Combat Progress Task",
		ToolTip="Narrative Task for damage, healing, hit, death, and revive objectives using the real Narrative Ability System Component."))
class TERRITORYFRAMEWORK_API UTerritoryCombatProgressTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	/** Optional combat subject. Empty follows the pawn that owns this quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Community Task|Subject",
		meta=(ToolTip="Actor whose Narrative combat events are counted. Empty watches the quest player. Easy example: Find NPC watches a story boss die."))
	TObjectPtr<UNarrativeActorProvider> SubjectProvider;

	/** Optional other side of the combat event: target, damager, or healer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category="Community Task|Filter",
		meta=(ToolTip="Optional actor filter. For Deal Damage this is the damaged target; for Take Damage it is the attacker; for Healing it is the healer."))
	TObjectPtr<UNarrativeActorProvider> CounterpartyProvider;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Combat")
	ETerritoryCombatProgressObjective Objective =
		ETerritoryCombatProgressObjective::DealDamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Filter",
		meta=(ToolTip="Optional effect asset tags. Every configured tag must match the Gameplay Effect that produced the combat event. Leave empty to accept any effect."))
	FGameplayTagContainer RequiredEffectTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Community Task|Combat",
		meta=(EditCondition="Objective == ETerritoryCombatProgressObjective::Die", EditConditionHides,
			ToolTip="When enabled, an already-dead subject completes a Die task immediately. Disable it when the quest needs a new death after this task starts."))
	bool bCompleteIfAlreadyDead = true;

	/** Converts an event magnitude into safe integer Narrative task progress. */
	UFUNCTION(BlueprintPure, Category="Community Task|Preview",
		meta=(ToolTip="Rounds a positive damage or healing magnitude to the nearest task-progress unit. Zero and negative values add no progress."))
	static int32 MagnitudeToProgress(float Magnitude);

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;
	virtual void TickTask_Implementation() override;
	virtual FText GetTaskDescription_Implementation() const override;
	virtual FText GetTaskProgressText_Implementation() const override;
	virtual AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	AActor* ResolveSubject() const;
	AActor* ResolveCounterparty() const;
	UNarrativeAbilitySystemComponent* ResolveNarrativeAbilitySystem(AActor* Actor) const;
	void BindSubject(AActor* Subject);
	void UnbindSubject();
	bool MatchesCounterparty(const UNarrativeAbilitySystemComponent* Other) const;
	bool MatchesEffect(const FGameplayEffectSpec& Spec) const;
	void AddMagnitudeProgress(float Magnitude);

	UFUNCTION() void HandleSubjectReady(AActor* Actor);
	UFUNCTION() void HandleCounterpartyReady(AActor* Actor);
	UFUNCTION() void HandleDealtDamage(UNarrativeAbilitySystemComponent* DamagedASC,
		const float Damage, const FGameplayEffectSpec& Spec);
	UFUNCTION() void HandleDamagedBy(UNarrativeAbilitySystemComponent* DamagerASC,
		const float Damage, const FGameplayEffectSpec& Spec);
	UFUNCTION() void HandleHealedBy(UNarrativeAbilitySystemComponent* HealerASC,
		const float Amount, const FGameplayEffectSpec& Spec);
	UFUNCTION() void HandleDeathStateChanged(AActor* ChangedActor,
		UNarrativeAbilitySystemComponent* ChangedASC, const bool bIsDead);

	UPROPERTY() TWeakObjectPtr<AActor> CachedSubject;
	UPROPERTY() TWeakObjectPtr<AActor> CachedCounterparty;
	UPROPERTY() TWeakObjectPtr<UNarrativeAbilitySystemComponent> CachedAbilitySystem;
	bool bObservedDeadState = false;
};
