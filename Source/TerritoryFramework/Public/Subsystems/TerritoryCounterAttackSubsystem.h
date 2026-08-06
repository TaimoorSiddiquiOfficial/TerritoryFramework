#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool IsAssaultActive(FGuid AssaultID) const;

	/** True for grace, warning, proximity-waiting, or physically active records. */
	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	bool IsAssaultPendingOrActive(FGuid AssaultID) const;

	UFUNCTION(BlueprintPure, Category="Territory|Counter Attack")
	FString GetAssaultDebugString(FGuid AssaultID) const;

	/** Pure deterministic calculator used by runtime evaluation and monotonicity tests. */
	static FTerritoryAssaultEvaluationResult CalculateEvaluation(
		const FTerritoryAssaultEvaluationInput& Input,
		const UTerritoryCounterAttackProfile* Profile);

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
	void NotifyRelevantPlayers(FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory);

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
	bool HasNavigationRoute(const FVector& Start, const FVector& End) const;
	FTerritoryAssaultEvaluationInput BuildEvaluationInput(const ATerritoryVolume* Territory,
		const FTerritoryFactionAssaultConfig& ForceConfig) const;
	bool FindBestEligibleAttacker(const ATerritoryVolume* Territory,
		const FGameplayTag& PreferredFaction, FGameplayTag& OutAttackingFaction,
		FTerritoryAssaultEvaluationInput& OutInput,
		FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason) const;
	double GetCampaignGameTime() const;
	int32 MakeDecisionSeed(const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction, int32 EvaluationCycle) const;
	int32 ReserveNextEvaluationCycle(const FGuid& TerritoryGUID,
		const FGameplayTag& AttackingFaction);
	int32 CountNonTerminalAssaults(const FGameplayTag* OptionalFaction = nullptr) const;
	int32 CountLiveParticipants() const;
	void TrimTerminalHistory();

	friend class FTFCounterAttackCycleHighWater;
};
