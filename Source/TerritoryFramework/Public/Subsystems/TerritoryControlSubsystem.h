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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnTerritoryAlertChanged,
	ATerritoryVolume*, Territory,
	AActor*, Target,
	ETerritoryAlertLevel, OldLevel,
	ETerritoryAlertLevel, NewLevel);

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

	/** Recover through normal state conditions. Rejected recovery stays pending for retry. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	FTerritoryMutationResponse TryResetCapture(ATerritoryVolume* Territory);

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

	/** Bounds own only this registration source; leaving bounds never removes explicit capture participation. */
	bool TryRegisterStoryBoundsContester(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);
	void UnregisterStoryBoundsContester(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

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

	/** Narrative perception adapters submit evidence here; only the server mutates awareness. Sight Evidence Seconds is elapsed observation time, capped at one second per report. Use zero for a visibility-change callback and elapsed time for a continuing observation. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	bool ReportStealthEvidence(ATerritoryVolume* Territory, AActor* Target,
		AActor* Observer, ETerritoryStealthEvidence Evidence, float Strength,
		const FVector& EvidenceLocation, const FVector& EstimatedSourceDirection,
		bool bConfirmedIdentity, float SightEvidenceSeconds = 0.25f);

	/**
	 * Tag-addressed evidence path for external systems (quests, abilities, vehicles, pets).
	 *
	 * A tag mapped by a profile Evidence Rule reports that rule's evidence channel and weight;
	 * Territory's native distraction tag maps without any rule. Unknown tags are rejected, so a new
	 * evidence source is authored in the stealth profile rather than in C++.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth",
		meta=(DisplayName="Report Stealth Evidence By Tag"))
	bool ReportStealthEvidenceByTag(ATerritoryVolume* Territory, AActor* Target,
		AActor* Observer, FGameplayTag EvidenceTag, float Strength,
		const FVector& EvidenceLocation, const FVector& EstimatedSourceDirection,
		bool bConfirmedIdentity, float SightEvidenceSeconds = 0.25f);

	/** Clear confirmed exposure and its exposure/bounds contest registrations. Explicit story and flag-capture registrations, Narrative personal hostility and burned disguises are retained. */
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
	UFUNCTION(BlueprintPure, Category = "Territory|Capture", meta = (DeprecatedFunction,
		DeprecationMessage = "Use Try Get Capture Participants and check readiness before calculating remaining capacity."))
	bool HasAttackBudget(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	/** Return the living attackers currently registered for this Territory's capture. */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture", meta = (DeprecatedFunction,
		DeprecationMessage = "Use Try Get Capture Participants. Exact counts are server-only; -1 means unknown."))
	int32 GetActiveAttackers(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	/** True means exact server data, including a known zero. False sets both counts to -1 (unknown). */
	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool TryGetCaptureParticipants(const ATerritoryVolume* Territory, const FGameplayTag& Faction,
		int32& OutLivingAttackers, int32& OutCapturePressure) const;

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

	/** Called when the Calm -> Suspicious -> Searching -> Alarmed -> Lockdown ladder changes. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Stealth")
	FOnTerritoryAlertChanged OnTerritoryAlertChanged;

	/** Hiding spot registration: while hidden the stealth observer treats the target as unseen. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Stealth")
	void SetTargetHidden(ATerritoryVolume* Territory, AActor* Target, bool bHidden);

	/** True while a hiding spot in this Territory holds the target hidden. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	bool IsTargetHidden(const ATerritoryVolume* Territory, const AActor* Target) const;

	/** Deterministic alert ladder used by the snapshot and the alarm delegate. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	static ETerritoryAlertLevel ComputeAlertLevel(const UTerritoryStealthProfile* Profile,
		ETerritoryExposureState ExposureState, float Suspicion);

	/** Called when the target's stealth exposure changes. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Stealth")
	FOnTerritoryExposureChanged OnExposureChanged;

	/**
	 * Resolve one explicit transition context for a faction's player, or an empty context when no
	 * player is connected as that faction.
	 *
	 * Public because it is the one rule that decides which player a faction's context belongs to,
	 * and story-driven presentation legitimately needs the same answer when a Territory event runs
	 * with no player context of its own - a defender defeat with no instigator. Duplicating the
	 * identity match (controller, pawn and player state) elsewhere would let a cutscene reach a
	 * different player than a capture does. Selection is path-sorted, so it stays stable when
	 * several players share one Narrative faction.
	 */
	FTerritoryTransitionContext ResolveFactionPlayerContext(const FGameplayTag& Faction) const;

private:
	friend class FTFCaptureAtomicContestTransition;
	friend class UTerritoryCounterAttackSubsystem;
	// The exposure producer is private because a report must cross the exposure latch; these two
	// tests drive the record itself, one for the shape it writes and one for the client refusal.
	friend class FTFEvidenceStealthExposedProducer;
	friend class FTFEvidenceStealthExposedRefusedOnClient;
	FTerritoryMutationResponse ApplyTerritoryMutationValidated(const FTerritoryMutationRequest& Request,
		TFunctionRef<bool()> FinalAdmission);

	/**
	 * Records the physical fact of a committed ownership change in the evidence ledger.
	 *
	 * Called after the final-state check and before OnTerritoryControlChanged, so a listener
	 * woken by that delegate already sees the fact that caused its call. The context id is
	 * derived from the transition itself (Place, old owner, new owner, campaign time), which
	 * makes a replayed commit idempotent without suppressing a later, legitimate recapture.
	 */
	void RecordCaptureEvidence(ATerritoryVolume& Territory, const FGameplayTag& OldOwner,
		const FGameplayTag& NewOwner);

	/**
	 * Records one physical exposure fact for an infiltrator that just crossed into Exposed.
	 *
	 * Called as soon as that state is committed and before any stealth broadcast, so a listener
	 * woken by OnExposureChanged already sees the fact that caused its call. The exposure latch
	 * keeps this to one row per episode; repeated sight updates on an already exposed target add
	 * nothing. The infiltrator is the subject only when it carries a durable identity - a player
	 * character has none, and the row then stands on the acting observer faction instead.
	 */
	void RecordStealthExposedEvidence(ATerritoryVolume& Territory, AActor& Target, AActor* Observer);

	/** Campaign clock used for evidence stamps. Zero when no Narrative game state is ticking. */
	double GetCampaignGameTime() const;

	enum class EParticipationSource : uint8
	{
		Capture = 1, ExplicitContest = 2, StoryBounds = 4, Exposure = 8
	};

	/** Per-faction capture state — attacker tracking is actor-based, not count-based */
	struct FPerTerritoryState
	{
		/** Actor sets per faction — prevents count inflation from duplicate registrations */
		TMap<FGameplayTag, TSet<TWeakObjectPtr<AActor>>> AttackersByFaction;
		/** Why each identity is admitted. Only Capture adds pressure; each caller releases its own bits. Transient, like the actor registrations. */
		TMap<FGameplayTag, TMap<TWeakObjectPtr<AActor>, uint8>> ParticipationSourcesByFaction;
		TMap<FGameplayTag, float> CaptureProgressByFaction;
		bool bRecoveryPending = false;
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
	/** Hiding spot registrations. A target is hidden while at least one spot holds it. */
	TMap<TWeakObjectPtr<ATerritoryVolume>, TSet<TWeakObjectPtr<AActor>>> HiddenTargets;

	/** Explicit quest overrides; absence means use the active Definition profile. */
	TMap<TWeakObjectPtr<ATerritoryVolume>, bool> StealthInfiltrationOverrides;

	/** Deferred commands to apply AFTER iteration to avoid map mutation during range-for */
	struct FDeferredCommand
	{
		enum EType { Complete, Reset };
		EType Type;
		TWeakObjectPtr<ATerritoryVolume> Territory;
		FGameplayTag Faction;
		uint64 LoadGeneration = 0;
	};
	TArray<FDeferredCommand> DeferredCommands;

	FTimerHandle CaptureTickTimerHandle;
	bool bCaptureTickInProgress = false;

	UFUNCTION()
	void OnCaptureTick();

	void EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime);
	void SelectCaptureLeader(const ATerritoryVolume* Territory, FGameplayTag& OutFaction, float& OutProgress) const;
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
		const FGameplayTag& Faction, EParticipationSource Source);
	void ReleaseParticipationSource(ATerritoryVolume* Territory, AActor* Attacker,
		const FGameplayTag& Faction, EParticipationSource Source);
	void ReleaseExposureParticipation(ATerritoryVolume* Territory, AActor* Target);

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
