#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryMutationTypes.h"
#include "TerritoryControlSubsystem.generated.h"

class ATerritoryVolume;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryControlSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ─── Capture API (authority-only mutations) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	ECaptureResult AttemptCapture(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void ResetCapture(ATerritoryVolume* Territory);

	/** P0-01: Remove only the runtime capture tracking map entry — does NOT mutate territory state. */
	void ClearCaptureTrackingOnly(ATerritoryVolume* Territory);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void AddCaptureProgress(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction, float ProgressDelta);

	/** Force-sets ownership. Returns true if the territory actually changed to the requested state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	bool ForceCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner);

	/**
	 * Atomic territory mutation — validates, commits, reconciles, and fires events in one call.
	 * Replaces loosely coupled setter sequences with a single transaction.
	 *
	 * Steps: validate authority/target → validate diplomacy/locks → capture old state →
	 * apply ownership/state/progress → reconcile guards → fire Narrative events →
	 * broadcast result → return structured response.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture",
		meta = (DisplayName = "Apply Territory Mutation"))
	FTerritoryMutationResponse ApplyTerritoryMutation(const FTerritoryMutationRequest& Request);

	/** Register an actor as an attacker for a faction. Identity-based — duplicates ignored. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	/** Unregister an actor. Removes identity, decrements count only if actor was registered. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	// ─── Query API (read-only, no authority needed) ───

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool IsCaptureInProgress(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	float GetCaptureProgress(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	FGameplayTag GetContestingFaction(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool HasAttackBudget(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	int32 GetActiveAttackers(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	/** Native persistence bridge for resuming a saved contested territory. */
	void RestoreCaptureState(
		ATerritoryVolume* Territory,
		const FGameplayTag& ContestingFaction,
		float ControlProgress);

	/** Single capture-admission policy used by requests, progress ticks, and completion. */
	bool CanFactionCaptureTerritory(const ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction) const;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Capture")
	FOnTerritoryControlChanged OnTerritoryControlChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Capture")
	FOnCaptureAttempted OnCaptureAttempted;

private:
	/** Per-faction capture state — attacker tracking is actor-based, not count-based */
	struct FPerTerritoryState
	{
		/** Actor sets per faction — prevents count inflation from duplicate registrations */
		TMap<FGameplayTag, TSet<TWeakObjectPtr<AActor>>> AttackersByFaction;
		TMap<FGameplayTag, float> CaptureProgressByFaction;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritoryState> TerritoryCaptureState;

	/** Deferred commands to apply AFTER iteration to avoid map mutation during range-for */
	struct FDeferredCommand
	{
		enum EType { Complete, Reset };
		EType Type;
		TWeakObjectPtr<ATerritoryVolume> Territory;
		FGameplayTag Faction;
	};
	TArray<FDeferredCommand> DeferredCommands;

	FTimerHandle CaptureTickTimerHandle;

	UFUNCTION()
	void OnCaptureTick();

	void EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime);
	void CompleteCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner);

	/**
	 * P1-08: Pure validation — no side effects. Checks territory validity,
	 * aggregate-only, locked, already-owned, defenders-remain, and diplomacy.
	 * Returns the first rejection code, or Success if all checks pass.
	 * Does NOT mutate territory state, create capture entries, or broadcast.
	 */
	ECaptureResult ValidateCaptureAttempt(
		const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction) const;

	/**
	 * Validate + optionally commit contested state and broadcast.
	 * Calls ValidateCaptureAttempt internally, then applies side effects if requested.
	 */
	ECaptureResult ValidateAndBeginCapture(
		ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction,
		bool bBroadcastAttempt,
		bool bCommitContestState = true);
};
