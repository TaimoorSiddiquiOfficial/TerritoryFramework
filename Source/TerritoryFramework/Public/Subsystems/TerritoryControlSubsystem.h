#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryMutationTypes.h"
#include "Core/TerritoryStealthProfile.h"
#include "TerritoryControlSubsystem.generated.h"

class ATerritoryVolume;
class UNarrativeAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnTerritoryStealthEvidenceReported,
	ATerritoryVolume*, Territory,
	AActor*, Target,
	ETerritoryStealthEvidence, Evidence,
	const FTerritoryInfiltrationSnapshot&, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnTerritoryExposureChanged,
	ATerritoryVolume*, Territory,
	AActor*, Target,
	ETerritoryExposureState, OldState,
	ETerritoryExposureState, NewState);

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

	/** Reset the Territory's active capture contest through the server's capture system. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void ResetCapture(ATerritoryVolume* Territory);

	/** P0-01: Remove only the runtime capture tracking map entry — does NOT mutate territory state. */
	void ClearCaptureTrackingOnly(ATerritoryVolume* Territory);

	/** Add progress through the existing capture system on the server. Capture must still satisfy its final checks. */
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

	/**
	 * Applies a Narrative unlock as one hierarchy-aware transaction.
	 * Place targets open only their City/District path. District and City targets
	 * cascade downward while every child keeps its own lock conditions.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Lock",
		meta=(DisplayName="Apply Territory Unlock"))
	FTerritoryUnlockCascadeResult ApplyUnlockCascade(ATerritoryVolume* Target,
		const FTerritoryTransitionContext& TransitionContext,
		ETerritoryUnlockScope Scope = ETerritoryUnlockScope::AutomaticHierarchy);

	/** Register an actor as an attacker for a faction. Identity-based — duplicates ignored. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	/** Register and report whether this exact actor was admitted to capture pressure. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	bool TryRegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	/**
	 * Register a physical contester without granting automatic capture progress.
	 *
	 * This is the story-confrontation path: entering the Place changes it to
	 * Contested so its guards can react, but defeating those guards still requires
	 * the authored owner dialogue/quest event to transfer ownership. Multiplayer
	 * capture points should continue to use Try Register Attacker.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture",
		meta = (DisplayName = "Try Register Contester (No Capture Progress)"))
	bool TryRegisterContester(ATerritoryVolume* Territory, AActor* Attacker,
		const FGameplayTag& Faction);

	// ─── Stealth infiltration API (authority-only evidence, read-only queries) ───

	/**
	 * Register physical presence without starting Contested. Returns false when the
	 * active profile does not allow stealth, so callers can keep legacy behavior.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	bool RegisterInfiltrator(ATerritoryVolume* Territory, AActor* Target,
		const FGameplayTag& Faction);

	/** Remove this actor from the Territory's tracked infiltrator state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	void UnregisterInfiltrator(ATerritoryVolume* Territory, AActor* Target);

	/** Narrative perception adapters submit evidence here; only the server mutates awareness. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	bool ReportStealthEvidence(ATerritoryVolume* Territory, AActor* Target,
		AActor* Observer, ETerritoryStealthEvidence Evidence, float Strength,
		const FVector& EvidenceLocation, const FVector& EstimatedSourceDirection,
		bool bConfirmedIdentity);

	/** Clear the target's confirmed exposure on the server, optionally resetting its suspicion. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	bool ClearInfiltratorExposure(ATerritoryVolume* Territory, AActor* Target,
		bool bResetSuspicion = true);

	/** Quest override. ClearOverride=true returns authoring control to the active Data Asset. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	void SetStealthInfiltrationOverride(ATerritoryVolume* Territory,
		bool bEnabled, bool bClearOverride = false);

	/** Check the effective infiltration setting, including the active profile and runtime override. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	bool IsStealthInfiltrationEnabled(const ATerritoryVolume* Territory) const;

	/** Read the target's current suspicion, evidence and exposure for this Territory. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	bool GetInfiltrationSnapshot(const ATerritoryVolume* Territory,
		const AActor* Target, FTerritoryInfiltrationSnapshot& OutSnapshot) const;

	/** Check whether the target has been confirmed as exposed in this Territory. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	bool IsInfiltratorExposed(const ATerritoryVolume* Territory,
		const AActor* Target) const;

	/** True only while one or more Territory guards currently confirm the target by sight. */
	bool IsTargetCurrentlySeen(const ATerritoryVolume* Territory,
		const AActor* Target) const;

	/** Removes one streamed, dead, or unpossessed observer from every target record. */
	void ForgetStealthObserver(ATerritoryVolume* Territory, AActor* Observer);

	/** Unregister an actor. Removes identity, decrements count only if actor was registered. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	// ─── Query API (read-only, no authority needed) ───

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool IsCaptureInProgress(const ATerritoryVolume* Territory) const;

	/** Return current capture progress from 0 to 1. A value of 0.5 means half complete. */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	float GetCaptureProgress(const ATerritoryVolume* Territory) const;

	/** Return the faction currently represented by this capture contest. */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	FGameplayTag GetContestingFaction(const ATerritoryVolume* Territory) const;

	/** Check whether another attacker fits the Territory's strategic attacker limit. */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool HasAttackBudget(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	/** Return the living attackers currently registered for this Territory's capture. */
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

	/**
	 * Side-effect-free admission for beginning a conflict. Unlike capture
	 * eligibility, living defenders are allowed: they are the fight that must be
	 * resolved before ownership can change.
	 */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	ECaptureResult GetContestEligibility(
		const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction) const;

	/** Native persistence bridge for resuming a saved contested territory. */
	void RestoreCaptureState(
		ATerritoryVolume* Territory,
		const FGameplayTag& ContestingFaction,
		float ControlProgress);

	/** Single capture-admission policy used by requests, progress ticks, and completion. */
	bool CanFactionCaptureTerritory(const ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction) const;

	/** Called after the Territory owner changes, with the old and new factions. */
	UPROPERTY(BlueprintAssignable, Category = "Territory|Capture")
	FOnTerritoryControlChanged OnTerritoryControlChanged;

	/** Reports the result of a capture attempt; inspect the result before showing success. */
	UPROPERTY(BlueprintAssignable, Category = "Territory|Capture")
	FOnCaptureAttempted OnCaptureAttempted;

	/** Called when Territory receives stealth evidence such as sight, sound or damage. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Stealth")
	FOnTerritoryStealthEvidenceReported OnStealthEvidenceReported;

	/** Called when the target's stealth exposure changes. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Stealth")
	FOnTerritoryExposureChanged OnExposureChanged;

private:
	friend class FTFCaptureAtomicContestTransition;

	/** Per-faction capture state — attacker tracking is actor-based, not count-based */
	struct FPerTerritoryState
	{
		/** Actor sets per faction — prevents count inflation from duplicate registrations */
		TMap<FGameplayTag, TSet<TWeakObjectPtr<AActor>>> AttackersByFaction;
		/** Subset of AttackersByFaction which holds Contested state but adds no pressure. */
		TMap<FGameplayTag, TSet<TWeakObjectPtr<AActor>>> NonCapturingAttackersByFaction;
		TMap<FGameplayTag, float> CaptureProgressByFaction;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritoryState> TerritoryCaptureState;

	/** Number of live capture registrations held by each actor across all territories. */
	TMap<TWeakObjectPtr<AActor>, int32> AttackerRegistrationCounts;

	/** ASC used for the one death binding owned by each registered attacker. */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundAttackerASCs;

	struct FInfiltrationRuntime
	{
		FTerritoryInfiltrationSnapshot Snapshot;
		FGameplayTag Faction;
		TSet<TWeakObjectPtr<AActor>> CurrentSightObservers;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>,
		TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>> TerritoryInfiltrationState;

	/** Explicit quest overrides; absence means use the active Definition profile. */
	TMap<TWeakObjectPtr<ATerritoryVolume>, bool> StealthInfiltrationOverrides;

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
	bool bCaptureTickInProgress = false;

	UFUNCTION()
	void OnCaptureTick();

	void EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime);
	void CompleteCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner);
	void EvaluateInfiltrationState(float DeltaTime);
	void RemoveInfiltratorFromAllTerritories(AActor* Target);
	void AssignClosestInvestigators(ATerritoryVolume* Territory, AActor* SuspectedSource,
		ETerritoryStealthEvidence Evidence, const FVector& Location,
		const FVector& EstimatedSourceDirection, bool bIdentityConfirmed,
		const UTerritoryStealthProfile& Profile);
	FTerritoryTransitionContext BuildTransitionContext(AActor* Attacker, const FGameplayTag& Faction) const;
	FTerritoryTransitionContext ResolveCaptureContext(
		const ATerritoryVolume* Territory,
		const FGameplayTag& Faction) const;
	FTerritoryTransitionContext ResolveFactionPlayerContext(const FGameplayTag& Faction) const;
	void AddAttackerRegistration(AActor* Attacker);
	void ReleaseAttackerRegistration(const TWeakObjectPtr<AActor>& Attacker);
	int32 PruneInvalidAttackers(TSet<TWeakObjectPtr<AActor>>& Attackers);
	int32 GetActiveCapturePressure(const ATerritoryVolume* Territory,
		const FGameplayTag& Faction) const;
	void ReleaseTerritoryAttackers(ATerritoryVolume* Territory);
	void RemoveAttackerFromAllCaptures(AActor* Attacker);
	UNarrativeAbilitySystemComponent* ResolveAttackerASC(AActor* Attacker) const;
	bool CommitCaptureReadModel(ATerritoryVolume* Territory, ETerritoryState NewState,
		const FGameplayTag& ContestingFaction, float ControlProgress,
		const FTerritoryTransitionContext& TransitionContext = FTerritoryTransitionContext()) const;

	UFUNCTION()
	void OnRegisteredAttackerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC,
		const bool bIsDead);

	/**
	 * P1-08: Pure validation — no side effects. Checks territory validity,
	 * aggregate-only, locked, already-owned, defenders-remain, and diplomacy.
	 * Returns the first rejection code, or Success if all checks pass.
	 * Does NOT mutate territory state, create capture entries, or broadcast.
	 */
	ECaptureResult ValidateCaptureAttempt(
		const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction) const;

	/** Conflict admission is deliberately separate from capture completion. */
	ECaptureResult ValidateContestAttempt(
		const ATerritoryVolume* Territory,
		const FGameplayTag& AttackingFaction) const;

	bool TryRegisterAttackerInternal(ATerritoryVolume* Territory, AActor* Attacker,
		const FGameplayTag& Faction, bool bContributesCaptureProgress);

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
