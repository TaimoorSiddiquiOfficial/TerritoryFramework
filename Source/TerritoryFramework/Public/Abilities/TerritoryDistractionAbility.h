#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Engine/EngineTypes.h"
#include "TerritoryDistractionAbility.generated.h"

class ATerritoryDistractionProjectile;
class UNarrativeItem;

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
		meta=(ClampMin="0.0", Units="cm", ToolTip="Forward distance from the avatar viewpoint. Used only when the launch socket below cannot be found."))
	float ForwardSpawnOffset = 75.f;

	/**
	 * Skeletal-mesh socket the stone leaves from. The camera is used only to find the crosshair
	 * aim point, never the spawn point: in a third-person setup the camera sits metres behind the
	 * character, so spawning along the view vector placed the projectile near the camera instead
	 * of in the throwing hand.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ToolTip="Socket on the avatar's skeletal mesh. Vendor GA_Attack_ThrowGrenade (AM_MM_Rifle_GrenadeToss) spawns at hand_l, so the default matches that toss animation; override it here for a different montage. Empty or missing falls back to the avatar origin plus Fallback Spawn Height."))
	FName LaunchSocketName = TEXT("hand_l");

	// ─── Animation (all optional; every value is editable in a Blueprint child) ───

	/**
	 * Optional throw montage. When it is set the server plays it, waits for the release event the
	 * montage's Territory Throw Release notify sends, and only then spawns. Leave it empty to keep
	 * the immediate spawn that AI pawns and automation tests use.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Animation",
		meta=(ToolTip="Throw montage. Move the Territory Throw Release notify to set the exact release frame; a Blueprint child can override the montage, the tag, the section and the play rate without a C++ change."))
	TObjectPtr<UAnimMontage> ThrowMontage;

	/**
	 * Gameplay Event the montage's release notify sends. The notify itself carries the tag, so one
	 * notify asset serves several abilities as long as both sides agree on the tag.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Animation",
		meta=(Categories="GameplayEvent",
			ToolTip="Must match the Event Tag on the montage's Territory Throw Release notify. Defaults to GameplayEvent.Distraction.Release."))
	FGameplayTag ThrowReleaseEventTag;

	/** Montage section to jump to before playing. Empty plays from the start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Animation",
		meta=(ToolTip="Optional montage section, for a montage that also contains an idle or a follow-up."))
	FName ThrowMontageSection;

	/** Montage play rate. Zero is treated as one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Animation",
		meta=(ClampMin="0.0"))
	float ThrowMontagePlayRate = 1.f;

	/**
	 * Safety release if the notify never arrives (a montage that lacks it, or an interrupted blend).
	 * The montage's own end also releases, so this only bounds the wait.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Animation",
		meta=(ClampMin="0.05", Units="s",
			ToolTip="Maximum time to wait for the release event before throwing anyway. Keep it slightly longer than the montage's release frame."))
	float ThrowReleaseFallbackSeconds = 0.65f;

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

	/**
	 * Follow the mouse pointer when the project actually shows one. With a hidden cursor (the TDA
	 * demo setup) the screen centre already is the pointer, so the camera view ray is used and this
	 * flag changes nothing. With a visible cursor the throw must leave along the ray through the
	 * pointer pixel instead of the screen centre.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ToolTip="When the owning player controller shows a mouse cursor, aim through the pointer position instead of the screen centre. Ignored for hidden-cursor setups and for remote clients, whose cursor state is not replicated."))
	bool bAimWithVisibleMouseCursor = true;

	/**
	 * Bend the throw upward just enough that the authored projectile speed and gravity still land
	 * the stone on the camera aim point. Without this the stone leaves along the crosshair but
	 * gravity drops it 0.9 m at 5 m and 3.4 m at 10 m, which reads as "it does not follow the
	 * crosshair". Unreachable targets fall back to the straight aim direction.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ToolTip="Compensate for projectile gravity so the stone lands on the aim point. Disable for a deliberately flat, physics-only throw."))
	bool bCompensateLaunchForGravity = true;

	/** Upper bound for the gravity-compensated launch elevation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Aim",
		meta=(ClampMin="0.0", ClampMax="89.0", Units="deg",
			ToolTip="Maximum upward launch angle the gravity compensation may use. Lower values lob less but cannot reach far targets."))
	float MaxCompensatedLaunchPitch = 55.f;

	/** Zero preserves the projectile movement component's authored Initial Speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction",
		meta=(ClampMin="0.0", Units="cm/s", ToolTip="Optional launch-speed override. Leave zero to use the projectile's authored Initial Speed (1200 by default)."))
	float LaunchSpeedOverride = 0.f;

	/**
	 * Require this ability to have been granted by the equipped Narrative item.
	 * This prevents a second Property-granted ability spec from throwing a second
	 * projectile for the same Narrative.Input.Throw press.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Inventory",
		meta=(ToolTip="Requires the source item to be equipped and still present in the throwing character's Narrative inventory. Keep enabled for equipped rocks and bottles. Disable this to use an unequipped consumable item source. Consumption still requires an item in the throwing character's inventory."))
	bool bRequireEquippedNarrativeItemSource = true;

	/** Consume one source-item unit only after the authoritative projectile finishes spawning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Distraction|Inventory",
		meta=(ToolTip="Consumes exactly one unit from the Narrative inventory after a successful server spawn. Enabling this also requires a valid item source. A failed activation or spawn consumes nothing."))
	bool bConsumeSourceItemOnSuccessfulThrow = true;

	/** Returns the server launch transform after camera/AI aim-point resolution. */
	UFUNCTION(BlueprintPure, Category="Territory|Distraction")
	bool GetDistractionLaunchTransform(FTransform& OutTransform) const;

	/**
	 * Aim math, exposed so automation tests can pin it without a world or a net driver.
	 *
	 * Launch direction that makes a projectile leave Start at Speed under GravityMagnitude and
	 * arrive on AimPoint, using the low arc and capping the elevation at MaxPitchDegrees.
	 * Returns false when no reachable arc exists (too far), when the target is directly overhead,
	 * or when gravity/speed are disabled; callers then keep their straight aim direction.
	 */
	static bool ComputeAimCompensatedDirection(
		const FVector& Start,
		const FVector& AimPoint,
		float Speed,
		float GravityMagnitude,
		float MaxPitchDegrees,
		FVector& OutDirection);

	/**
	 * True when the server must read the possessed pawn's eye location plus the replicated
	 * ControlRotation instead of the camera. A ServerOnly ability running on the server's copy of
	 * a remote client has no locally updated camera: GetPlayerViewPoint then falls back to the
	 * view-target actor rotation, which is yaw only, so the throw silently lost the aim pitch.
	 */
	static bool ShouldUseReplicatedClientAim(bool bIsLocalController, bool bHasCameraCache);

	/**
	 * True when the aim ray must be taken from the mouse pointer instead of the screen centre.
	 * A project that hides the cursor (the TDA demo player controller) aims with the crosshair, so
	 * the camera view ray stays authoritative; a project that shows the cursor aims where the
	 * pointer actually is, which is the camera ray through that pixel.
	 */
	static bool ShouldAimWithMouseCursor(bool bIsLocalController, bool bCursorVisible,
		bool bHasMousePosition);

	/** Pure inventory contract used by activation validation and automation tests. */
	UFUNCTION(BlueprintPure, Category="Territory|Distraction|Inventory")
	bool IsThrowableSourceItemReady(const UNarrativeItem* SourceItem) const;

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UNarrativeItem* GetThrowableSourceItem(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	bool IsThrowableSourceOwnedByAvatar(const UNarrativeItem* SourceItem,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	// Transient call identity only; never saved or replicated.
	uint64 ActivationSerial = 0;

	/** Plays ThrowMontage on the server (replicated through GAS) and starts the release wait. */
	void StartThrowMontage();

	/** Spawns and pays for the projectile. Guarded so one activation can only throw once. */
	void PerformThrow();

	/** Ends this activation after a verified throw or a verified failure. */
	void FinishActivation(bool bCancelled);

	/** Release notify (or montage end) accepted: run the throw. */
	UFUNCTION()
	void HandleReleaseEvent(FGameplayEventData Payload);

	/** Montage completed or blended out without a release notify: throw anyway rather than lose the input. */
	UFUNCTION()
	void HandleMontageFinished();

	/** Safety timer: bounded wait for the release notify. */
	UFUNCTION()
	void HandleReleaseFallback();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Source item captured at activation; consumed only after a verified spawn. */
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeItem> PendingSourceItem;

	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_WaitGameplayEvent> ReleaseWaitTask;

	UPROPERTY(Transient)
	TObjectPtr<class UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle ReleaseFallbackTimer;
	uint64 PendingActivationSerial = 0;
	bool bPendingConsumeSourceItem = false;
	bool bThrowPerformed = false;
	bool bThrowPending = false;

	/** Presentation hook. Gameplay has already committed and the replicated actor exists. */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Distraction",
		meta=(DisplayName="On Distraction Projectile Spawned"))
	void K2_OnDistractionProjectileSpawned(ATerritoryDistractionProjectile* Projectile);

	/**
	 * Presentation hook fired on the server when the release frame is accepted, before the projectile
	 * exists. Useful for a hand puff, a throw sound or a camera shake a designer adds in a child BP.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Distraction",
		meta=(DisplayName="On Distraction Release"))
	void K2_OnDistractionRelease();
};
