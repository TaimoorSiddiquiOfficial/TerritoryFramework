#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Navigation/TerritoryRoadTypes.h"
#include "TerritoryAssaultParticipantComponent.generated.h"

class ANarrativeNPCCharacter;
class ANarrativeVehicleBase;
class ATerritoryVolume;
class UNarrativeAbilitySystemComponent;
class UNPCActivity;
class UNPCActivityComponent;
class UNPCGoalItem;
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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Configure(const FGuid& InAssaultID, const FGuid& InTargetTerritoryGUID,
		const FGameplayTag& InTargetTerritory, const FGameplayTag& InAttackingFaction);
	void ConfigureAssaultRules(bool bInAllowsTerritoryCapture,
		bool bInPrioritizeTerritoryTakeover = false,
		float InDefendingPlayerEngagementPadding = 800.f,
		FGameplayTag InTakeoverStartedDialogueTag = FGameplayTag(),
		FGameplayTag InFinalFightDialogueTag = FGameplayTag());
	void ConfigureNarrativeVehicleIngress(ANarrativeVehicleBase* InVehicle,
		const TArray<FVector>& InRoutePoints,
		const FTransform& InParkDestination, const FTransform& InWalkDestination,
		float InMaximumDriveSpeed, float InTimeoutSeconds, bool bInEscapeOnArrival,
		const FTerritoryVehicleAwarenessSettings& InAwarenessSettings,
		float InMaximumChaseDistance = 0.f,
		float InChaseDistanceGraceSeconds = 0.f,
		bool bInAbandonDamagedVehicleForFinalFight = false,
		float InVehicleAbandonHealthFraction = 0.35f);

	/** Pure mission rules exposed for automation tests and Blueprint-independent tuning. */
	static float CalculateObstacleSpeedFactor(float ObstacleDistance,
		float EmergencyStopDistance, float BrakingDistance);
	static bool ShouldFailChaseDistance(float SecondsOutsideRange,
		float GraceSeconds, float ClosestPlayerDistance, float MaximumDistance);
	static bool ShouldAbandonVehicle(float HealthFraction, float HealthThreshold,
		float BlockedSeconds, float BlockedTimeout);

	/** Permanently retire this finite participant exactly once. */
	void Retire(bool bKilled);

	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGuid GetAssaultID() const { return AssaultID; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGuid GetTargetTerritoryGUID() const { return TargetTerritoryGUID; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGameplayTag GetTargetTerritoryTag() const { return TargetTerritory; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") FGameplayTag GetAttackingFaction() const { return AttackingFaction; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") ATerritoryVolume* GetTargetTerritory() const { return ResolveTargetTerritory(); }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") bool IsCaptureRegistered() const { return bCaptureRegistered; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault|Vehicle") bool IsVehicleIngressPending() const { return bVehicleIngressRequired && !bVehicleIngressComplete; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault|Vehicle") bool CanEngageCombat() const { return !IsVehicleIngressPending() && !bEscapeOnVehicleArrival; }
	UFUNCTION(BlueprintPure, Category="Territory|Assault") bool AllowsTerritoryCapture() const { return bAllowsTerritoryCapture; }
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
	UPROPERTY(Replicated) bool bAllowsTerritoryCapture = true;
	bool bPrioritizeTerritoryTakeover = false;
	float DefendingPlayerEngagementPadding = 800.f;
	FGameplayTag TakeoverStartedDialogueTag;
	FGameplayTag FinalFightDialogueTag;
	bool bTakeoverStartedDialoguePlayed = false;

	UPROPERTY(Transient) TObjectPtr<UTerritoryAssaultGoal> AssaultGoal;
	TWeakObjectPtr<ANarrativeVehicleBase> NarrativeIngressVehicle;
	TArray<FVector> VehicleRoutePoints;
	FTransform VehicleParkDestination;
	FTransform VehicleWalkDestination;
	float VehicleMaximumDriveSpeed = 0.f;
	float VehicleIngressTimeoutSeconds = 120.f;
	double VehicleIngressDeadline = 0.0;
	bool bVehicleIngressRequired = false;
	bool bVehicleIngressComplete = false;
	bool bVehicleIngressFailed = false;
	bool bVehicleMountRequested = false;
	bool bVehiclePossessionConfirmed = false;
	bool bVehicleDriveActive = false;
	bool bVehicleDismountRequested = false;
	int32 VehicleRoutePointIndex = 0;
	bool bEscapeOnVehicleArrival = false;
	bool bEscapeCompletionReported = false;
	FTerritoryVehicleAwarenessSettings VehicleAwareness;
	float MaximumChaseDistance = 0.f;
	float ChaseDistanceGraceSeconds = 0.f;
	float SecondsOutsideChaseRange = 0.f;
	float VehicleAbandonHealthFraction = 0.35f;
	float VehicleBlockedSeconds = 0.f;
	bool bAbandonDamagedVehicleForFinalFight = false;
	bool bVehicleAbandonmentRequested = false;
	bool bUseVehicleWalkDestination = false;
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
	bool EnsureNarrativeVehicleIngress();
	bool EnsureNarrativeActivityAndGoal();
	bool MaintainAssaultMovement(ATerritoryVolume* Territory);
	void ReconcileNarrativeDefenderTargeting(UNPCActivityComponent* ActivityComponent,
		TConstArrayView<AActor*> LiveHostileDefenders);
	TArray<AActor*> CollectTakeoverCombatants(ATerritoryVolume* Territory) const;
	void PlayMissionDialogue(const FGameplayTag& DialogueTag);
	void RestoreNarrativeDefenderTargeting(bool bReselectActivity);
	void UnregisterCapturePressure();
	void UpdateVehicleDriving(float DeltaTime);
	float GetClosestPlayerDistanceToVehicle() const;
	float GetNarrativeVehicleHealthFraction() const;
	bool QueryVehicleObstacleDistance(const FVector& LateralOffset,
		float& OutDistance) const;
	bool TryBeginVehicleDismount();
	void BeginVehicleAbandonment(const TCHAR* Reason);
	void StopVehicleInputs();
	void CompleteVehicleIngress();
	void WithdrawForVehicleIngressFailure(const TCHAR* Reason);

	UFUNCTION()
	void HandleOwnerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC,
		const bool bIsDead);
};
