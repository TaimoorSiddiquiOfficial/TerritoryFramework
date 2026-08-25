#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryAssaultParticipantComponent.generated.h"

class ANarrativeNPCCharacter;
class ATerritoryVolume;
class UNarrativeAbilitySystemComponent;
class UNPCActivityComponent;
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

	void Configure(const FGuid& InAssaultID, const FGuid& InTargetTerritoryGUID,
		const FGameplayTag& InTargetTerritory, const FGameplayTag& InAttackingFaction);

	/** Permanently retire this finite participant exactly once. */
	void Retire(bool bKilled);

	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGuid GetAssaultID() const { return AssaultID; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGuid GetTargetTerritoryGUID() const { return TargetTerritoryGUID; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGameplayTag GetTargetTerritoryTag() const { return TargetTerritory; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGameplayTag GetAttackingFaction() const { return AttackingFaction; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") bool IsCaptureRegistered() const { return bCaptureRegistered; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") bool IsConfigured() const
	{
		return AssaultID.IsValid() && TargetTerritoryGUID.IsValid()
			&& TargetTerritory.IsValid() && AttackingFaction.IsValid();
	}

	/** Stable GUID is authoritative; tag matching is only a bounded legacy fallback. */
	bool MatchesTargetTerritory(const ATerritoryVolume* Territory) const;

private:
	UPROPERTY(Replicated) FGuid AssaultID;
	UPROPERTY(Replicated) FGuid TargetTerritoryGUID;
	UPROPERTY(Replicated) FGameplayTag TargetTerritory;
	UPROPERTY(Replicated) FGameplayTag AttackingFaction;
	UPROPERTY(Replicated) bool bCaptureRegistered = false;

	UPROPERTY(Transient) TObjectPtr<UTerritoryAssaultGoal> AssaultGoal;
	TWeakObjectPtr<UClass> NarrativeAttackGoalClass;
	TArray<FTerritoryNarrativeGoalScoreOverride> NarrativeGoalScoreOverrides;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	FTimerHandle ParticipationTimer;
	int32 GoalInitializationAttempts = 0;
	int32 ConsecutiveMovementRestartFailures = 0;
	double NextMovementRestartTime = 0.0;
	bool bRemovalReported = false;

	void UpdateParticipation();
	ATerritoryVolume* ResolveTargetTerritory() const;
	bool BindNarrativeDeathAfterSpawnReady();
	bool EnsureNarrativeActivityAndGoal();
	bool MaintainAssaultMovement(ATerritoryVolume* Territory);
	void ReconcileNarrativeDefenderTargeting(UNPCActivityComponent* ActivityComponent,
		TConstArrayView<AActor*> LiveHostileDefenders);
	void RestoreNarrativeDefenderTargeting(bool bReselectActivity);
	void UnregisterCapturePressure();

	UFUNCTION()
	void HandleOwnerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC,
		const bool bIsDead);
};
