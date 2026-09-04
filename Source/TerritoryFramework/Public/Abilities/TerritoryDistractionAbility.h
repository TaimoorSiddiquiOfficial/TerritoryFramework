#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "TerritoryDistractionAbility.generated.h"

class ATerritoryDistractionProjectile;

/**
 * Narrative Gameplay Ability that owns a Territory distraction throw.
 *
 * The projectile remains an actor because collision, replication, movement, and
 * hearing stimuli require a world object. This ability owns the GAS contract:
 * Narrative.Input.Throw, activation rules, cost/cooldown commit, authoritative
 * spawning, and GameplayEvent.Distraction.* events.
 */
UCLASS(BlueprintType, Blueprintable,
	meta=(DisplayName="Territory Distraction Throw Ability"))
class TERRITORYFRAMEWORK_API UTerritoryDistractionAbility
	: public UNarrativeGameplayAbility
{
	GENERATED_BODY()

public:
	UTerritoryDistractionAbility();

	/** Actor payload spawned after CommitAbility succeeds. A Blueprint child may select BP_TerritoryDistractionProjectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction",
		meta=(ToolTip="Narrative projectile actor spawned by this ability. Use BP_TerritoryDistractionProjectile for the supplied mesh, bounce, and hearing stimulus."))
	TSubclassOf<ATerritoryDistractionProjectile> ProjectileClass;

	/** Distance in front of the avatar used for the spawn point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ClampMin="0.0", Units="cm", ToolTip="Forward distance from the avatar viewpoint. Increase this if a large character capsule catches the projectile."))
	float ForwardSpawnOffset = 75.f;

	/** Vertical offset from the avatar origin when no controller viewpoint exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(Units="cm", ToolTip="Fallback height above the avatar origin for AI or pawns without a camera viewpoint."))
	float FallbackSpawnHeight = 60.f;

	/** Camera trace distance used to turn the camera aim point into a launch direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ClampMin="100.0", Units="cm", ToolTip="Maximum camera aim trace. The projectile launches toward the first blocking hit, otherwise toward the trace end."))
	float AimTraceDistance = 5000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ToolTip="Trace channel used only to find the aim point. Projectile collision still uses the projectile actor's collision profile."))
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	/** Zero preserves the projectile movement component's authored Initial Speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction",
		meta=(ClampMin="0.0", Units="cm/s", ToolTip="Optional launch-speed override. Leave zero to use the projectile's authored Initial Speed (1200 by default)."))
	float LaunchSpeedOverride = 0.f;

	/** Returns the server launch transform after camera/AI aim-point resolution. */
	UFUNCTION(BlueprintPure, Category="Territory|Distraction")
	bool GetDistractionLaunchTransform(FTransform& OutTransform) const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Presentation hook. Gameplay has already committed and the replicated actor exists. */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Distraction",
		meta=(DisplayName="On Distraction Projectile Spawned"))
	void K2_OnDistractionProjectileSpawned(ATerritoryDistractionProjectile* Projectile);
};
