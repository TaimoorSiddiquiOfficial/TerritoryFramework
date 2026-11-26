#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "TerritoryStealthObserverComponent.generated.h"

class ATerritoryGuardCharacter;
class ATerritoryVolume;
class UAIPerceptionComponent;
class UTerritoryGuardPerceptionProfile;
class UTerritoryStealthProfile;

/**
 * Thin adapter on Territory guards. It reads Narrative's assigned controller and
 * AI Perception component, then submits server-only evidence to Territory authority.
 */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryStealthObserverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryStealthObserverComponent();

	/** Effective sight after Narrative Stealth Rating and invisibility are applied. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	float CalculateEffectiveSightStrength(AActor* Target, float RawSightStrength) const;

	/**
	 * True while this guard must not create its normal attack goal for Target: the target is inside
	 * the Territory, not exposed, not point-blank, and effective sight is below the profile's
	 * MinimumSightEvidence. Fails open (false) whenever stealth does not apply, so a non-stealth
	 * Place or a missing profile never disables guard combat.
	 *
	 * Called from the attack goal generator so a hidden player is investigated instead of attacked.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Should Suppress Attack Goal"))
	bool ShouldSuppressAttackGoal(AActor* Target) const;

	/**
	 * Optional guard-quality perception tier. Applied once to the controller's AIPerception when the
	 * observer binds, so a rookie/veteran/elite guard can see and hear differently without editing
	 * Narrative's controller Blueprint.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Stealth")
	TObjectPtr<UTerritoryGuardPerceptionProfile> PerceptionProfile;

	/**
	 * Deterministic half of ShouldSuppressAttackGoal, exposed for tests and UI previews.
	 * A target that the shared contest rule does not admit to combat (holstered weapon, no war,
	 * no recent aggression) is never given an attack goal; an admitted target keeps one unless it is
	 * still hidden (effective sight below the profile's MinimumSightEvidence).
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	static bool ShouldSuppressAttackGoalForState(ETerritoryExposureState ExposureState,
		bool bPointBlank, bool bFiringWhileSeen, bool bAdmittedToCombat,
		float EffectiveSight, float MinimumSightEvidence);

	/** Deterministic formula exposed for equipment previews, UI, and automation tests. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth")
	static float ApplyStealthRatingToSight(float RawSightStrength,
		float GuardDetectionMultiplier, float StealthRating,
		float MaximumStealthRating = 100.f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FObservedSight
	{
		float RawStrength = 0.f;
		FVector LastLocation = FVector::ZeroVector;
	};

	struct FRecentGunshot
	{
		double WorldTime = -1.0;
		FVector MuzzleLocation = FVector::ZeroVector;
	};

	TWeakObjectPtr<UAIPerceptionComponent> BoundPerception;
	/** Perception component this tier was already written to, so it is applied once per bind. */
	TWeakObjectPtr<UAIPerceptionComponent> PerceptionProfileAppliedTo;
	TWeakObjectPtr<ATerritoryVolume> ObservedTerritory;
	TMap<TWeakObjectPtr<AActor>, FObservedSight> CurrentlySeenTargets;
	TMap<TWeakObjectPtr<AActor>, FRecentGunshot> RecentGunshots;
	/** Last time each target's footsteps became evidence, so one walk pass is not ten reports. */
	TMap<TWeakObjectPtr<AActor>, double> RecentFootsteps;
	FTimerHandle BindingRetryTimer;
	FTimerHandle SightRefreshTimer;
	double LastSightRefreshWorldTime = -1.0;

	ATerritoryGuardCharacter* GetTerritoryGuard() const;
	AActor* ResolvePlayerSource(AActor* SensedActor) const;
	const UTerritoryStealthProfile* GetActiveProfile() const;
	bool IsTargetFiring(AActor* Target) const;
	bool IsNarrativeInvisible(AActor* Target) const;
	bool ShouldForcePointBlankExposure(AActor* Target) const;
	bool BindToCurrentPerception();
	void RetryPerceptionBinding();
	void UnbindFromPerception();
	void RefreshVisibleTargets();
	bool ReadCurrentSight(AActor* Target, FAIStimulus& OutStimulus) const;
	void ReportSight(AActor* Target, const FAIStimulus& Stimulus, float ElapsedSeconds);

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* SensedActor, FAIStimulus Stimulus);
};
