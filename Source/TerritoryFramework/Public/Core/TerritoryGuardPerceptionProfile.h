#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TerritoryGuardPerceptionProfile.generated.h"

class UAIPerceptionComponent;

/**
 * Guard-quality perception tier.
 *
 * Narrative ships one AI Perception setup on BP_NarrativeNPCController (sight 6000 / lose 7000 /
 * 180 degrees / hearing 10000 / MaxAge 0). Every guard therefore perceives exactly like every
 * other guard. This data asset lets a project author "rookie / veteran / elite" tiers and apply
 * one to a guard, without editing the vendor controller Blueprint.
 *
 * The values are applied to the controller's existing AIPerception sense configs when the Territory
 * stealth observer binds, so reserve spawns, late definitions and controller replacement all get
 * the tier. Defaults equal Narrative's shipped values, so an untouched tier changes nothing.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Territory Guard Perception Profile"))
class TERRITORYFRAMEWORK_API UTerritoryGuardPerceptionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTerritoryGuardPerceptionProfile();

	/** Designer-facing tier name, for journals, debugging and quest conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="00 Tier")
	FText TierName;

	// ─── Sight ───

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Sight",
		meta=(ClampMin="0.0", Units="cm",
			ToolTip="How far the guard can see. Narrative's controller default is 6000."))
	float SightRadius = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Sight",
		meta=(ClampMin="0.0", Units="cm",
			ToolTip="Radius at which the guard loses sight. Clamped to at least Sight Radius. Default 7000."))
	float LoseSightRadius = 7000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Sight",
		meta=(ClampMin="1.0", ClampMax="360.0",
			ToolTip="Half-angle peripheral vision. 180 is Narrative's shipped value (a half sphere); 120 gives a narrower cone."))
	float PeripheralVisionAngleDegrees = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Sight",
		meta=(ClampMin="0.0", Units="s",
			ToolTip="How long a sight stimulus survives. 0 means the guard never forgets, which is Narrative's default; a positive value makes stealth viable."))
	float SightMaxAge = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Sight",
		meta=(Units="cm",
			ToolTip="Distance at which the target is sensed even without line of sight. -1 disables it (Narrative default)."))
	float AutoSuccessRangeFromLastSeenLocation = -1.f;

	// ─── Hearing ───

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Hearing",
		meta=(ClampMin="0.0", Units="cm",
			ToolTip="Maximum hearing range. Narrative's controller default is 10000. Footstep and distraction noise ranges are capped by this."))
	float HearingRange = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Hearing",
		meta=(ClampMin="0.0", Units="s"))
	float HearingMaxAge = 0.f;

	// ─── Damage ───

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Damage",
		meta=(ClampMin="0.0", Units="s"))
	float DamageMaxAge = 20.f;

	// ─── Affiliation ───

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Affiliation")
	bool bDetectEnemies = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Affiliation")
	bool bDetectNeutrals = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Affiliation")
	bool bDetectFriendlies = true;

	/**
	 * Applies this tier to every sense config the perception component already owns. Returns true
	 * when at least one config changed. The component is the vendor controller's, so nothing here
	 * edits Narrative content.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Perception",
		meta=(ToolTip="Writes this tier onto an AIPerceptionComponent's sight, hearing and damage configs and requests a listener update."))
	bool ApplyTo(UAIPerceptionComponent* Perception) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
