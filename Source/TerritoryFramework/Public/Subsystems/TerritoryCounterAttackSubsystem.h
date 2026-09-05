#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/TerritoryTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "TerritoryCounterAttackSubsystem.generated.h"

class ATerritoryAssaultCharacter;
class ATerritoryVolume;
class ANarrativeVehicleBase;
class ATerritoryRoadGuide;
class APawn;
class UNPCGoalItem;
class UTerritoryCounterAttackProfile;
struct FTerritoryFactionAssaultConfig;
enum class EDiplomacyState : uint8;

/**
 * Server-authoritative strategic scheduler for finite, physical counterattacks.
 * A probability roll never changes ownership. Spawned Narrative NPCs must enter the
 * target and defeat its defence front. The subsystem then uses the shared Territory
 * mutation authority only for the configured unattended countdown or defending-player
 * death handover; player capture remains UTerritoryControlSubsystem's responsibility.
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

	/** Reusable story contract for hunter arrival, capo escape, optional capture and finite boss/escort overrides. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Counter Attack|Story")
	bool ScheduleStoryPursuitWithOptions(ATerritoryVolume* Territory,
		FGameplayTag AttackingFaction, const FTerritoryStoryPursuitOptions& Options);

	/**
	 * Diplomacy-first automatic admission. Evaluates every configured, valid faction and
	 * schedules the strongest eligible candidate. PreferredFaction is only a stable tie-break,
	 * so a stronger hostile faction can supersede the former owner.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Counter Attack")
	bool ScheduleBestCounterAttack(ATerritoryVolume* Territory,
		FGameplayTag PreferredFaction = FGameplayTag());

	/**
	 * Diagnostic scheduling path used by Narrative Events and Blueprint tools. It runs
	 * the same authoritative admission as Schedule Counter Attack / Story Pursuit and
	 * explains the first rejected rule instead of returning an unexplained false.
	 * Story Options are ignored for Strategic Counterattack.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Territory|Counter Attack|Diagnostics",
		meta=(DisplayName="Try Schedule Territory Assault (With Reason)"))
	bool TryScheduleAssaultWithReason(ATerritoryVolume* Territory,
		FGameplayTag AttackingFaction, ETerritoryAssaultLaunchMode LaunchMode,
		const FTerritoryStoryPursuitOptions& StoryOptions,
		FText& OutFailureReason);

	/**
	 * Explicit Narrative path with optional immediate physical deployment. An authored
	 * Wave does not require the automatic strategy layer's secure-District,
	 * Reinforcements-capability, or counter-Quest gates. It still validates authority,
	 * opposing ownership, diplomacy, finite force, spawn definitions, budget, and route.
	 * Immediate also skips grace, time window, chance, warning delay, and proximity.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Territory|Counter Attack|Diagnostics",
		meta=(DisplayName="Try Schedule Territory Assault Advanced (With Reason)"))
	bool TryScheduleAssaultAdvancedWithReason(ATerritoryVolume* Territory,
		FGameplayTag AttackingFaction, ETerritoryAssaultLaunchMode LaunchMode,
		const FTerritoryStoryPursuitOptions& StoryOptions,
		bool bStartImmediately, FText& OutFailureReason);

	/** Best-attacker admission with a player/developer-readable rejection reason. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly,
		Category="Territory|Counter Attack|Diagnostics",
		meta=(DisplayName="Try Schedule Best Counterattack (With Reason)"))
	bool TryScheduleBestCounterAttackWithReason(ATerritoryVolume* Territory,
		FGameplayTag PreferredFaction, FText& OutFailureReason);

	/** Read-only strategic preview used by District command UI. Does not reserve a cycle or roll. */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool GetBestEligibleAttackerPreview(const ATerritoryVolume* Territory,
		FGameplayTag PreferredFaction, FGameplayTag& OutAttackingFaction,
		FTerritoryAssaultEvaluationInput& OutInput,
		FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason) const;

	/** Best hostile configured force for one explicit Narrative Wave. */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack|Story")
	bool GetBestAuthoredWaveAttackerPreview(const ATerritoryVolume* Territory,
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

	/** Loaded, unlocked Districts whose complete authored Place set is securely held. */
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

	/**
	 * Waiting-state activation policy. Strategic counterattacks require a claimed
	 * target because they may recapture it; story pursuits that explicitly disable
	 * capture may activate in any available Territory state.
	 */
	static bool ShouldActivateWaitingAssault(bool bAllowsTerritoryCapture,
		ETerritoryState TerritoryState, bool bRequirePlayerProximity,
		bool bRelevantPlayerNearby);

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

	/**
	 * Pure policy used by runtime and regression tests for post-activation reserve waves.
	 * Vehicle-only forces set bWaitForCurrentWaveToEnd so one casualty cannot consume
	 * the next authored car as a one-seat top-up.
	 */
	static bool ShouldDeployActiveReserveWave(const FTerritoryAssaultRecord& Assault,
		bool bRelevantPlayerNearby, bool bContinueAfterActivation,
		bool bWaitForCurrentWaveToEnd = false);

	/** Pure recapture decision shared by runtime and regression tests. */
	enum class ERecaptureDecision : uint8
	{
		ContinueFight,
		StartCountdown,
		CompleteHandover,
		NoAction
	};
	static ERecaptureDecision EvaluateRecaptureDecision(bool bDefendersRemain,
		bool bPhysicalAttackerInside, bool bAliveDefendingPlayerInside,
		bool bDeadDefendingPlayerInside, bool bCountdownActive,
		bool bCountdownExpired, bool bAllowUnattendedCountdown,
		bool bConcedeOnPlayerDeath);

	/** Pure, save-stable recurring cooldown rule. */
	static bool IsRecurringCooldownComplete(const FTerritoryAssaultRecord& PreviousAssault,
		double CurrentGameTime, float CooldownGameTime);

	/**
	 * Narrative time-window rule. A wrapped range such as 1800 -> 0500 includes
	 * evening and early morning; equal start/end values mean the full day.
	 */
	static bool IsNarrativeTimeInWindow(float TimeOfDay, float WindowStart,
		float WindowEnd);

	/** True when another battle is permitted by the selected schedule mode. */
	static bool CanContinueSchedule(ETerritoryCounterScheduleMode ScheduleMode,
		int32 PreviousOccurrence, int32 MaximumScheduledAssaults);

	/** Deterministic three-column deployment formation facing the target Territory. */
	static FTransform CalculateParticipantDeploymentTransform(
		const FTransform& ApproachTransform, const FVector& TargetLocation,
		int32 FormationSlot, float ParticipantSpacing);

	/** Pure presentation score. Higher means an authored route is safer and more readable to this player. */
	static float CalculatePlayerRelativeApproachScore(
		const FVector& ApproachLocation, const FVector& PlayerLocation,
		const FVector& PlayerViewForward, float MinimumDistance,
		float PreferredDistance, float MaximumDistance,
		float PreferredCameraEdgeDot, float SameFloorHeightTolerance);

	/**
	 * Uses the exact runtime nav projection and complete-path rules used by physical assaults.
	 * Editor validation calls this same function so a route cannot pass validation and then
	 * fail only after the grace period. OutFailureReason is intended for logs and validation.
	 */
	static bool ValidateNavigationRoute(UWorld* World, const FVector& Start,
		const FVector& End, FString* OutFailureReason = nullptr);

	/**
	 * Mirrors Narrative Pro's default ZoneGraph vehicle-route admission (1,000 cm
	 * lane search, unfiltered lanes) without depending on a project vehicle Blueprint.
	 */
	static bool ValidateNarrativeVehicleRoute(UWorld* World, const FVector& Start,
		const FVector& End, FString* OutFailureReason = nullptr);

	/** Builds sampled, ordered ZoneGraph road points for Territory's possessed Narrative driver. */
	static bool BuildNarrativeVehicleRoute(UWorld* World, const FVector& Start,
		const FVector& End, TArray<FVector>& OutRoutePoints,
		FString* OutFailureReason = nullptr);

	/** Pure finite-force planner: returns the number of Territory participants one car may carry. */
	static int32 ResolveVehicleOccupantCount(int32 RemainingWaveSlots,
		int32 ApproachWaveLimit, int32 ConfiguredVehicleCapacity,
		int32 NarrativeMountSeatCount);

	/**
	 * Caps a vehicle-only finite force to the seats that the current Narrative
	 * difficulty is allowed to deliver. Each entry represents one authored car.
	 */
	static int32 ResolveVehicleOnlyPlannedForce(int32 RequestedForce,
		int32 MaximumVehicleDeployments,
		const TArray<int32>& VehicleDeploymentCapacities);

	/** Global persistence/replication bridge owned by ATerritoryWorldState. */
	TArray<FTerritoryAssaultRecord> GetPersistentState() const;
	TArray<FTerritoryAssaultCycleRecord> GetPersistentCycleState() const;
	void RestorePersistentState(const TArray<FTerritoryAssaultRecord>& Records);
	void RestorePersistentState(const TArray<FTerritoryAssaultRecord>& Records,
		const TArray<FTerritoryAssaultCycleRecord>& CycleRecords);

	/** Participant lifecycle callbacks. Each physical NPC may report removal once. */
	void NotifyParticipantRemoved(FGuid AssaultID, ATerritoryAssaultCharacter* Participant, bool bKilled);
	/** Called after the possessed Narrative vehicle reaches the authored exit for a fleeing story target. */
	void NotifyVehicleStoryTargetEscaped(FGuid AssaultID,
		ATerritoryAssaultCharacter* Participant);
	/** The player lost the fleeing target for longer than the authored grace period. */
	void NotifyVehicleStoryTargetLostByDistance(FGuid AssaultID,
		ATerritoryAssaultCharacter* Participant);
	/** Non-terminal handoff: the damaged/blocked target left its car for the final fight. */
	void NotifyVehicleStoryTargetAbandoned(FGuid AssaultID,
		ATerritoryAssaultCharacter* Participant);

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
	/** Runtime-only Narrative vehicles; durable saves retain route IDs, never actor pointers. */
	TMap<FGuid, TSet<TWeakObjectPtr<ANarrativeVehicleBase>>> LiveAssaultVehicles;
	TMap<TWeakObjectPtr<ANarrativeVehicleBase>, FTerritoryVehicleRetirementSettings>
		LiveVehicleRetirementRules;
	TMap<FGuid, TSet<TWeakObjectPtr<ATerritoryRoadGuide>>> LiveAssaultRoadGuides;
	struct FRetiringVehicle
	{
		TWeakObjectPtr<ANarrativeVehicleBase> Vehicle;
		double EarliestRetirementTime = 0.0;
		double HardRetirementTime = 0.0;
		float PlayerKeepAliveDistance = 0.f;
	};
	TArray<FRetiringVehicle> RetiringVehicles;
	TMap<FGuid, TSet<TWeakObjectPtr<APlayerController>>> WarnedControllers;
	FTimerHandle UpdateTimer;
	bool bRestoringState = false;
	bool bUpdatingAssaults = false;
	bool bSpawningAssaultWave = false;
	uint64 RestoreGeneration = 0;
	/** Transient access token; detects map relocation or replacement across synchronous callbacks. */
	struct FAssaultAccess
	{
		FGuid ID;
		const FTerritoryAssaultRecord* Address = nullptr;
		uint64 Generation = 0;
	};
	FAssaultAccess CaptureAssaultAccess(const FTerritoryAssaultRecord& Assault) const;
	bool IsAssaultCurrent(const FAssaultAccess& Access) const;
	bool IsAssaultCurrent(const FAssaultAccess& Access, ETerritoryAssaultState ExpectedState) const;

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
		UNPCDefinition* AttackerDefinition,
		const FTerritoryAssaultApproach& Approach, const FTransform& SpawnTransform,
		int32 OverrideNarrativeLevel);
	TArray<ATerritoryAssaultCharacter*> SpawnNarrativeVehicleParticipants(
		FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory,
		const FTerritoryFactionAssaultConfig& ForceConfig,
		UNPCDefinition* AttackerDefinition,
		const FTerritoryAssaultApproach& Approach,
		const FTransform& VehicleSpawnTransform,
		const FTransform& DriverSpawnTransform,
		const FTransform& DropOffTransform, const FVector& WalkDestination,
		int32 RequestedOccupants, int32 OverrideNarrativeLevel);
	void ResolveAssault(FTerritoryAssaultRecord& Assault, ETerritoryAssaultState FinalState,
		ETerritoryAssaultResolution Reason);
	void RetireLiveParticipants(FTerritoryAssaultRecord& Assault, bool bDestroyActors);
	void RetireLiveVehicles(const FGuid& AssaultID, bool bDestroyActors,
		bool bForCampaignRestore = false);
	void UpdateRetiringVehicles();
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
	void GetDefendingPlayerPresence(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory, bool& bOutAliveInside,
		bool& bOutDeadInside) const;
	bool HasPhysicalAttackerInside(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory) const;
	bool CompleteRecapture(FTerritoryAssaultRecord& Assault,
		ATerritoryVolume* Territory);
	APawn* FindNearestRelevantPlayer(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory, float Radius) const;
	int32 ResolveScaledEnemyLevel(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory,
		const FTerritoryFactionAssaultConfig& ForceConfig) const;
	bool IsRelevantPlayer(const APlayerController* Controller,
		const FTerritoryAssaultRecord& Assault, const UTerritoryCounterAttackProfile* Profile) const;
	bool IsDiplomacyBlocked(const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory) const;
	TArray<FName> SelectValidApproaches(const ATerritoryVolume* Territory,
		const UTerritoryCounterAttackProfile* Profile, float PowerRatio) const;
	bool ResolveApproach(const ATerritoryVolume* Territory, FName ApproachID,
		FTerritoryAssaultApproach& OutApproach, FTransform& OutWorldTransform) const;
	ATerritoryRoadGuide* ResolveRoadGuide(
		const FTerritoryAssaultApproach& Approach) const;
	bool ResolveApproachObjective(const ATerritoryVolume* Territory,
		const FTerritoryAssaultApproach& Approach,
		const FTransform& ApproachWorldTransform, FVector& OutObjective,
		FTransform& OutFootDeploymentTransform,
		FTransform* OutVehicleDropOffTransform = nullptr,
		FString* OutFailureReason = nullptr) const;
	bool IsDeploymentLocationSeparated(const FVector& Location, float MinimumSpacing) const;
	bool HasNavigationRoute(const FVector& Start, const FVector& End) const;
	FTerritoryAssaultEvaluationInput BuildEvaluationInput(const ATerritoryVolume* Territory,
		const FTerritoryFactionAssaultConfig& ForceConfig) const;
	bool FindBestEligibleAttacker(const ATerritoryVolume* Territory,
		const FGameplayTag& PreferredFaction, FGameplayTag& OutAttackingFaction,
		FTerritoryAssaultEvaluationInput& OutInput,
		FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason,
		bool bRequireRecurringEligibility = false,
		bool bExplicitNarrativeRequest = false) const;
	bool ScheduleAssault(ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
		ETerritoryAssaultLaunchMode LaunchMode,
		bool bContinueExistingSchedule = false,
		const FTerritoryStoryPursuitOptions* StoryOptions = nullptr,
		FText* OutFailureReason = nullptr,
		bool bQuestOverrideAuthorized = false,
		bool bStartImmediately = false);
	bool StartAssaultImmediately(FTerritoryAssaultRecord& Assault,
		ATerritoryVolume* Territory, FText* OutFailureReason = nullptr);
	bool HasPendingVehicleIngress(const FGuid& AssaultID) const;
	bool DoesForceMeetStagingRequirement(const FTerritoryFactionAssaultConfig& ForceConfig,
		ETerritoryAssaultLaunchMode LaunchMode) const;
	bool DoesFactionMeetReinforcementCapabilityRequirement(
		const UTerritoryCounterAttackProfile* Profile,
		const FGameplayTag& Faction, ETerritoryAssaultLaunchMode LaunchMode,
		FText* OutFailureReason = nullptr) const;
	bool DoStrategicQuestRulesPass(const UTerritoryCounterAttackProfile* Profile,
		const FGameplayTag& AttackingFaction, const FGameplayTag& DefendingFaction,
		FText* OutFailureReason = nullptr) const;
	bool FindReachableObjective(const ATerritoryVolume* Territory, const FVector& Start,
		FVector& OutObjective, FString* OutFailureReason = nullptr) const;
	double GetCampaignGameTime() const;
	float GetNarrativeTimeOfDay() const;
	bool IsForceTimeWindowOpen(const FTerritoryFactionAssaultConfig& ForceConfig) const;
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
	friend class FTFAssaultWarningCallback;
	friend class FTFAssaultEvaluationResume;
	friend class FTFAssaultSpawnCallbacks;
	friend class FTFAssaultVehicleRestoreCleanup;
};
