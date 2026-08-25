#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/TerritoryTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "TerritoryCounterAttackSubsystem.generated.h"

class ATerritoryAssaultCharacter;
class ATerritoryVolume;
class UTerritoryCounterAttackProfile;
struct FTerritoryFactionAssaultConfig;
enum class EDiplomacyState : uint8;

/**
 * Server-authoritative strategic scheduler for finite, physical counterattacks.
 * It never changes ownership directly; spawned Narrative NPCs enter the existing
 * UTerritoryControlSubsystem capture flow.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryCounterAttackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Counter Attack")
	bool ScheduleCounterAttack(ATerritoryVolume* Territory, FGameplayTag AttackingFaction);

	/**
	 * Explicit Tales path for a boss chase or betrayal pursuit. It may bypass the normal
	 * District staging rule only when the selected force config separately opts in.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Counter Attack|Story")
	bool ScheduleStoryPursuit(ATerritoryVolume* Territory, FGameplayTag AttackingFaction);

	/**
	 * Diplomacy-first automatic admission. Evaluates every configured, valid faction and
	 * schedules the strongest eligible candidate. PreferredFaction is only a stable tie-break,
	 * so a stronger hostile faction can supersede the former owner.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Counter Attack")
	bool ScheduleBestCounterAttack(ATerritoryVolume* Territory,
		FGameplayTag PreferredFaction = FGameplayTag());

	/** Read-only strategic preview used by District command UI. Does not reserve a cycle or roll. */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool GetBestEligibleAttackerPreview(const ATerritoryVolume* Territory,
		FGameplayTag PreferredFaction, FGameplayTag& OutAttackingFaction,
		FTerritoryAssaultEvaluationInput& OutInput,
		FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Counter Attack")
	bool CancelAssault(FGuid AssaultID,
		ETerritoryAssaultResolution Reason = ETerritoryAssaultResolution::ManuallyCancelled);

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool GetAssault(FGuid AssaultID, FTerritoryAssaultRecord& OutAssault) const;

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	TArray<FTerritoryAssaultRecord> GetAllAssaults() const;

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	TArray<FTerritoryAssaultRecord> GetAssaultsForTerritory(FGameplayTag TerritoryTag) const;

	/**
	 * Exact loaded-actor query. Stable GUID is authoritative; tag is used only for a
	 * legacy record that has not yet been rebound. Prefer this for UI and gameplay
	 * that already has a Territory actor.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack",
		meta=(DisplayName="Get Assaults for Territory Actor"))
	TArray<FTerritoryAssaultRecord> GetAssaultsForTerritoryActor(
		const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool IsAssaultActive(FGuid AssaultID) const;

	/** True for grace, warning, proximity-waiting, or physically active records. */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool IsAssaultPendingOrActive(FGuid AssaultID) const;

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	FString GetAssaultDebugString(FGuid AssaultID) const;

	/** Loaded, authoritative Districts securely held in Claimed or story-Locked state. */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack|Staging")
	int32 GetSecureDistrictCountForFaction(FGameplayTag Faction) const;

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack|Staging")
	bool CanFactionStageStrategicCounterAttack(FGameplayTag Faction) const;

	/** Pure deterministic calculator used by runtime evaluation and monotonicity tests. */
	static FTerritoryAssaultEvaluationResult CalculateEvaluation(
		const FTerritoryAssaultEvaluationInput& Input,
		const UTerritoryCounterAttackProfile* Profile);

	/** Raw post reserves only defend staffing slots the owner has authorized. */
	static int32 CalculateEffectiveReserveGuards(int32 RawReserveGuards,
		int32 DesiredGuardCount);

	/** Higher local influence shortens a delay without ever making it zero or negative. */
	static float CalculateInfluenceAdjustedDelay(float BaseDelay, float FactionInfluence,
		float MinimumTimingScale);

	/**
	 * Claimed is required before activation. Once physical attackers register through
	 * the capture authority, their own matching Contested state remains valid.
	 */
	static bool IsTerritoryControlStateValidForAssault(
		ETerritoryAssaultState AssaultState, ETerritoryState TerritoryState,
		const FGameplayTag& ContestingFaction, const FGameplayTag& AttackingFaction);

	/** State events are live transition notifications, never save/load replays or same-state updates. */
	static bool ShouldEmitCounterHappened(ETerritoryAssaultState PreviousState,
		ETerritoryAssaultState NewState, bool bIsRestoringState);

	/** Build the exact value payload emitted by OnCounterHappened. */
	static FTerritoryCounterAttackStateEvent MakeCounterHappenedEvent(
		const FTerritoryAssaultRecord& Assault, ETerritoryAssaultState PreviousState,
		double EventGameTime);

	/** Atomically commit proximity activation; a second nearby player returns false. */
	static bool TryCommitProximityActivation(FTerritoryAssaultRecord& Assault,
		double ActivatedGameTime);

	/** Apply one already-deduplicated finite participant removal to durable counts. */
	static bool ApplyParticipantRemoval(FTerritoryAssaultRecord& Assault,
		bool bKilled, bool& bOutForceExhausted);

	/** Pure policy used by runtime and regression tests for post-activation reserve waves. */
	static bool ShouldDeployActiveReserveWave(const FTerritoryAssaultRecord& Assault,
		bool bRelevantPlayerNearby, bool bContinueAfterActivation);

	/** Pure, save-stable recurring cooldown rule. */
	static bool IsRecurringCooldownComplete(const FTerritoryAssaultRecord& PreviousAssault,
		double CurrentGameTime, float CooldownGameTime);

	/** Deterministic three-column deployment formation facing the target Territory. */
	static FTransform CalculateParticipantDeploymentTransform(
		const FTransform& ApproachTransform, const FVector& TargetLocation,
		int32 FormationSlot, float ParticipantSpacing);

	/**
	 * Uses the exact runtime nav projection and complete-path rules used by physical assaults.
	 * Editor validation calls this same function so a route cannot pass validation and then
	 * fail only after the grace period. OutFailureReason is intended for logs and validation.
	 */
	static bool ValidateNavigationRoute(UWorld* World, const FVector& Start,
		const FVector& End, FString* OutFailureReason = nullptr);

	/** Global persistence/replication bridge owned by ATerritoryWorldState. */
	TArray<FTerritoryAssaultRecord> GetPersistentState() const;
	TArray<FTerritoryAssaultCycleRecord> GetPersistentCycleState() const;
	void RestorePersistentState(const TArray<FTerritoryAssaultRecord>& Records);
	void RestorePersistentState(const TArray<FTerritoryAssaultRecord>& Records,
		const TArray<FTerritoryAssaultCycleRecord>& CycleRecords);

	/** Participant lifecycle callbacks. Each physical NPC may report removal once. */
	void NotifyParticipantRemoved(FGuid AssaultID, ATerritoryAssaultCharacter* Participant, bool bKilled);

	UPROPERTY(BlueprintAssignable, Category="Territory|Counter Attack")
	FOnTerritoryAssaultChanged OnAssaultChanged;

	UPROPERTY(BlueprintAssignable, Category="Territory|Counter Attack")
	FOnTerritoryAssaultWarning OnAssaultWarning;

	/**
	 * Server-side global event fired once after each verified assault-state transition.
	 * Bind Tales/server orchestration here; owning-client UI uses the identically named
	 * event on UTerritoryPlayerManagementComponent or UTerritoryHUDWidget.
	 */
	UPROPERTY(BlueprintAssignable, Category="Territory|Counter Attack")
	FOnTerritoryCounterHappened OnCounterHappened;

private:
	TMap<FGuid, FTerritoryAssaultRecord> Assaults;
	TMap<FGuid, TMap<FGameplayTag, int32>> EvaluationCycleHighWater;
	TMap<FGuid, TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>> LiveParticipants;
	TMap<FGuid, TSet<TWeakObjectPtr<APlayerController>>> WarnedControllers;
	FTimerHandle UpdateTimer;
	bool bRestoringState = false;

	UFUNCTION()
	void HandleTerritoryControlChanged(ATerritoryVolume* Territory,
		FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void HandleDiplomacyChanged(FGameplayTag FactionA, FGameplayTag FactionB,
		EDiplomacyState NewState);

	UFUNCTION()
	void HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bIsNew);

	void UpdateAssaults();
	void TryScheduleRecurringStrategicAssaults();
	void AdvanceAssault(FTerritoryAssaultRecord& Assault);
	void EvaluateAssault(FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory);
	bool ActivateAssault(FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory);
	void SpawnNextWave(FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory);
	ATerritoryAssaultCharacter* SpawnParticipant(FTerritoryAssaultRecord& Assault,
		ATerritoryVolume* Territory, const FTerritoryFactionAssaultConfig& ForceConfig,
		const FTerritoryAssaultApproach& Approach, const FTransform& SpawnTransform);
	void ResolveAssault(FTerritoryAssaultRecord& Assault, ETerritoryAssaultState FinalState,
		ETerritoryAssaultResolution Reason);
	void RetireLiveParticipants(FTerritoryAssaultRecord& Assault, bool bDestroyActors);
	void BroadcastChanged(const FTerritoryAssaultRecord& Assault);
	void BroadcastStateTransition(const FTerritoryAssaultRecord& Assault,
		ETerritoryAssaultState PreviousState);
	void NotifyRelevantPlayers(FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory);
	void NotifyRelevantPlayersOfState(const FTerritoryCounterAttackStateEvent& Event,
		ATerritoryVolume* Territory);

	/** Stable GUID is authoritative; the tag is only a bounded fallback for legacy saves. */
	static bool DoesAssaultTargetTerritory(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory);
	/** Repairs a legacy missing GUID or an old tag after a GUID-preserving rename. */
	static bool ReconcileAssaultTargetIdentity(FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory);
	bool HasNonTerminalAssaultForTerritory(const ATerritoryVolume* Territory) const;
	ATerritoryVolume* ResolveTerritory(const FTerritoryAssaultRecord& Assault) const;
	bool HasRelevantPlayerNearby(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory, float Radius) const;
	bool IsRelevantPlayer(const APlayerController* Controller,
		const FTerritoryAssaultRecord& Assault, const UTerritoryCounterAttackProfile* Profile) const;
	bool IsDiplomacyBlocked(const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory) const;
	TArray<FName> SelectValidApproaches(const ATerritoryVolume* Territory,
		const UTerritoryCounterAttackProfile* Profile, float PowerRatio) const;
	bool ResolveApproach(const ATerritoryVolume* Territory, FName ApproachID,
		FTerritoryAssaultApproach& OutApproach, FTransform& OutWorldTransform) const;
	bool IsDeploymentLocationSeparated(const FVector& Location, float MinimumSpacing) const;
	bool HasNavigationRoute(const FVector& Start, const FVector& End) const;
	FTerritoryAssaultEvaluationInput BuildEvaluationInput(const ATerritoryVolume* Territory,
		const FTerritoryFactionAssaultConfig& ForceConfig) const;
	bool FindBestEligibleAttacker(const ATerritoryVolume* Territory,
		const FGameplayTag& PreferredFaction, FGameplayTag& OutAttackingFaction,
		FTerritoryAssaultEvaluationInput& OutInput,
		FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason,
		bool bRequireRecurringEligibility = false) const;
	bool ScheduleAssault(ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
		ETerritoryAssaultLaunchMode LaunchMode);
	bool DoesForceMeetStagingRequirement(const FTerritoryFactionAssaultConfig& ForceConfig,
		ETerritoryAssaultLaunchMode LaunchMode) const;
	bool FindReachableObjective(const ATerritoryVolume* Territory, const FVector& Start,
		FVector& OutObjective, FString* OutFailureReason = nullptr) const;
	double GetCampaignGameTime() const;
	int32 MakeDecisionSeed(const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction, int32 EvaluationCycle) const;
	int32 ReserveNextEvaluationCycle(const FGuid& TerritoryGUID,
		const FGameplayTag& AttackingFaction);
	int32 CountNonTerminalAssaults(const FGameplayTag* OptionalFaction = nullptr) const;
	int32 CountLiveParticipants() const;
	void TrimTerminalHistory();

	friend class FTFCounterAttackCycleHighWater;
	friend class FTFWorldStateAssaultPersistenceRoundTrip;
	friend class FTFCounterAttackWorldPartitionTargetRebind;
};
