#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryMutationTypes.h"
#include "TerritoryControlSubsystem.generated.h"

class ATerritoryVolume;
class UNarrativeAbilitySystemComponent;

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
	 * Explicit-context force capture for quests, editor utilities, and scripted transitions.
	 * RequestingFaction is normalized to NewOwner before the atomic mutation is committed.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	bool ForceCaptureWithContext(ATerritoryVolume* Territory, const FGameplayTag& NewOwner,
		const FTerritoryTransitionContext& TransitionContext);

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

	/** Register and report whether this exact actor was admitted to capture pressure. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	bool TryRegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

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

	/**
	 * Side-effect-free capture admission result for planning UI and AI.
	 * This query is safe on clients because it only reads replicated Territory state,
	 * diplomacy, and Narrative faction attitude. It never starts capture progress.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	ECaptureResult GetCaptureEligibility(
		const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction) const;

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
	friend class FTFCaptureAtomicContestTransition;

	/** Per-faction capture state — attacker tracking is actor-based, not count-based */
	struct FPerTerritoryState
	{
		/** Actor sets per faction — prevents count inflation from duplicate registrations */
		TMap<FGameplayTag, TSet<TWeakObjectPtr<AActor>>> AttackersByFaction;
		TMap<FGameplayTag, float> CaptureProgressByFaction;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritoryState> TerritoryCaptureState;

	/** Number of live capture registrations held by each actor across all territories. */
	TMap<TWeakObjectPtr<AActor>, int32> AttackerRegistrationCounts;

	/** ASC used for the one death binding owned by each registered attacker. */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundAttackerASCs;

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
	FTerritoryTransitionContext BuildTransitionContext(AActor* Attacker, const FGameplayTag& Faction) const;
	FTerritoryTransitionContext ResolveCaptureContext(
		const ATerritoryVolume* Territory,
		const FGameplayTag& Faction) const;
	FTerritoryTransitionContext ResolveFactionPlayerContext(const FGameplayTag& Faction) const;
	void AddAttackerRegistration(AActor* Attacker);
	void ReleaseAttackerRegistration(const TWeakObjectPtr<AActor>& Attacker);
	int32 PruneInvalidAttackers(TSet<TWeakObjectPtr<AActor>>& Attackers);
	void ReleaseTerritoryAttackers(ATerritoryVolume* Territory);
	void RemoveAttackerFromAllCaptures(AActor* Attacker);
	UNarrativeAbilitySystemComponent* ResolveAttackerASC(AActor* Attacker) const;
	bool CommitCaptureReadModel(ATerritoryVolume* Territory, ETerritoryState NewState,
		const FGameplayTag& ContestingFaction, float ControlProgress,
		const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext()) const;

	UFUNCTION()
	void OnRegisteredAttackerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC);

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
