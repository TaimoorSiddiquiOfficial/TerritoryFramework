#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "TerritoryStealthObserverComponent.generated.h"

class ATerritoryGuardCharacter;
class UAIPerceptionComponent;
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
	TMap<TWeakObjectPtr<AActor>, FObservedSight> CurrentlySeenTargets;
	TMap<TWeakObjectPtr<AActor>, FRecentGunshot> RecentGunshots;
	FTimerHandle BindingRetryTimer;
	FTimerHandle SightRefreshTimer;

	ATerritoryGuardCharacter* GetTerritoryGuard() const;
	AActor* ResolvePlayerSource(AActor* SensedActor) const;
	const UTerritoryStealthProfile* GetActiveProfile() const;
	bool IsTargetFiring(AActor* Target) const;
	bool BindToCurrentPerception();
	void RetryPerceptionBinding();
	void UnbindFromPerception();
	void RefreshVisibleTargets();

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* SensedActor, FAIStimulus Stimulus);
};
