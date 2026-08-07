#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryAssaultParticipantComponent.generated.h"

class ANarrativeNPCCharacter;
class UNarrativeAbilitySystemComponent;
class UTerritoryAssaultGoal;

/** Runtime bridge from one physical Narrative NPC to one durable assault record. */
UCLASS(ClassGroup=(Territory), BlueprintType, meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryAssaultParticipantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryAssaultParticipantComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(const FGuid& InAssaultID, const FGameplayTag& InTargetTerritory,
		const FGameplayTag& InAttackingFaction);

	/** Permanently retire this finite participant exactly once. */
	void Retire(bool bKilled);

	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGuid GetAssaultID() const { return AssaultID; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") bool IsCaptureRegistered() const { return bCaptureRegistered; }

private:
	UPROPERTY(Replicated) FGuid AssaultID;
	UPROPERTY(Replicated) FGameplayTag TargetTerritory;
	UPROPERTY(Replicated) FGameplayTag AttackingFaction;
	UPROPERTY(Replicated) bool bCaptureRegistered = false;

	UPROPERTY(Transient) TObjectPtr<UTerritoryAssaultGoal> AssaultGoal;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	FTimerHandle ParticipationTimer;
	int32 GoalInitializationAttempts = 0;
	bool bRemovalReported = false;

	void UpdateParticipation();
	bool BindNarrativeDeathAfterSpawnReady();
	bool EnsureNarrativeActivityAndGoal();
	void UnregisterCapturePressure();

	UFUNCTION()
	void HandleOwnerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC);
};
