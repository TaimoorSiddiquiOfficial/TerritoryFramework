#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "TerritoryStealthProfile.generated.h"

class UTerritoryInvestigationActivity;

/** Per-player awareness inside a stealth-enabled Territory. Presence is not exposure. */
UENUM(BlueprintType)
enum class ETerritoryExposureState : uint8
{
	Undetected UMETA(DisplayName="Undetected",
		ToolTip="The player is inside, but no guard has enough evidence to identify them."),
	Suspicious UMETA(DisplayName="Suspicious / Investigating",
		ToolTip="Guards have weak evidence and may investigate, but Territory conflict has not begun."),
	Exposed UMETA(DisplayName="Exposed",
		ToolTip="A guard has confirmed the player. Story bounds may now register them as a contester.")
};

/** Evidence understood by the Territory layer. Narrative remains the source of perception data. */
UENUM(BlueprintType)
enum class ETerritoryStealthEvidence : uint8
{
	None UMETA(DisplayName="None"),
	Sight UMETA(DisplayName="Guard Sight"),
	FireSeen UMETA(DisplayName="Fired While Seen"),
	Gunshot UMETA(DisplayName="Unseen Gunshot"),
	BulletImpact UMETA(DisplayName="Unseen Bullet Impact"),
	Damage UMETA(DisplayName="Confirmed Damage"),
	DefenderKilledSeen UMETA(DisplayName="Defender Kill Seen"),
	Corpse UMETA(DisplayName="Unseen Defender Death / Corpse"),
	ThrowableDistraction UMETA(DisplayName="Throwable Distraction"),
	Scripted UMETA(DisplayName="Story Event"),
	Footstep UMETA(DisplayName="Footsteps Heard"),
	Disguise UMETA(DisplayName="Disguise Doubt")
};

/** Escalation ladder exposed to the HUD, Tales conditions and the alarm readers. */
UENUM(BlueprintType)
enum class ETerritoryAlertLevel : uint8
{
	Calm UMETA(DisplayName="Calm"),
	Suspicious UMETA(DisplayName="Suspicious"),
	Searching UMETA(DisplayName="Searching"),
	Alarmed UMETA(DisplayName="Alarmed"),
	Lockdown UMETA(DisplayName="Lockdown")
};

/** How far confirmed exposure should escalate. Faction War preserves the existing contest flow. */
UENUM(BlueprintType)
enum class ETerritoryStealthEscalationScope : uint8
{
	LocalAlarm UMETA(DisplayName="Local Alarm Only",
		ToolTip="Guards investigate, but the Territory is not registered as Contested."),
	TerritoryConflict UMETA(DisplayName="Territory Conflict",
		ToolTip="Register the exposed player as a Territory contester. Diplomacy is decided by State Events."),
	FactionWar UMETA(DisplayName="Faction War Through State Event",
		ToolTip="Register the player as a contester, then let the Contested State Event declare War.")
};

/** Read-only result for UI, Narrative conditions, debugging, and tests. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryInfiltrationSnapshot
{
	GENERATED_BODY()

	/** Current hidden, suspicious or confirmed state of the target. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryExposureState ExposureState = ETerritoryExposureState::Undetected;

	/** Most recently recorded kind of stealth evidence. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryStealthEvidence LastEvidence = ETerritoryStealthEvidence::None;

	/** Accumulated suspicion value for the target, on the system's 0 to 1 scale. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Suspicion = 0.f;

	/** World-space location of the most recent stealth evidence. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector LastEvidenceLocation = FVector::ZeroVector;

	/** Estimated direction toward the source of the reported evidence. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector EstimatedSourceDirection = FVector::ZeroVector;

	/** World time in seconds when the most recent evidence was recorded. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	float LastEvidenceWorldTime = -1.f;

	/** Observers that currently confirm the target's identity. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	int32 ConfirmingObserverCount = 0;

	/** Whether the viewer is currently inside the reported Territory. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	bool bInsideTerritory = false;

	/** Current escalation ladder for this target in this Territory. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryAlertLevel AlertLevel = ETerritoryAlertLevel::Calm;
};

/**
 * One data-driven evidence rule. Lets a project tune or extend evidence handling from a data asset
 * instead of C++: override the suspicion weight of any existing evidence channel, make it force
 * exposure, and give it a semantic tag so external systems can report it by tag.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryEvidenceRule
{
	GENERATED_BODY()

	/** Optional tag used by Report Stealth Evidence By Tag. Leave invalid for an enum-only override. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rule")
	FGameplayTag Tag;

	/** Evidence channel this rule tunes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rule")
	ETerritoryStealthEvidence Evidence = ETerritoryStealthEvidence::Footstep;

	/** Suspicion added by one report on this channel, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rule",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float Suspicion = 0.1f;

	/** Confirms exposure immediately, like damage or a seen defender kill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rule",
		meta=(DisplayName="Forces Exposure"))
	bool bForcesExposure = false;
};

/**
 * Reusable stealth policy selected by a Territory Definition and optionally overridden per State Config.
 * It adapts Narrative sight/hearing/damage into Territory suspicion; it does not replace Narrative AI.
 */
UCLASS(BlueprintType, Const, meta=(DisplayName="Territory Stealth Profile"))
class TERRITORYFRAMEWORK_API UTerritoryStealthProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTerritoryStealthProfile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Infiltration",
		meta=(ToolTip="When enabled, entering story bounds registers an undetected infiltrator instead of immediately starting Contested. Easy example: use this for a rescue quest inside an enemy Place."))
	bool bAllowStealthInfiltration = true;

	/** Choose which Territory or hierarchy scope becomes contested when the infiltrator is confirmed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Infiltration")
	ETerritoryStealthEscalationScope EscalationScope =
		ETerritoryStealthEscalationScope::FactionWar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="Effective Narrative sight at or above this value immediately confirms the player."))
	float ImmediateSightExposureThreshold = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="Sight below this value is ignored. Narrative's normal attack goal uses 0.2 as its useful minimum."))
	float MinimumSightEvidence = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ToolTip="Multiplies Narrative's sight strength before Stealth Rating is applied."))
	float GuardDetectionMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ToolTip="Suspicion added per second while partial sight remains active."))
	float SightSuspicionGainPerSecond = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ToolTip="Suspicion removed per second after every guard loses sight."))
	float SuspicionDecayPerSecond = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", ClampMax="100.0",
			ToolTip="Narrative Stealth Rating is interpreted on this scale. A rating of 50 on a scale of 100 halves effective sight."))
	float MaximumStealthRating = 100.f;

	/** Ignore sight-based detection while Narrative marks the target invisible; other reported evidence still follows its own rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight")
	bool bRespectNarrativeInvisibleTag = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ToolTip="When enabled, a guard that already has Narrative sight of the player exposes them inside Point Blank Exposure Distance even when Stealth Rating would otherwise reduce sight below the evidence threshold. This prevents walking directly in front of a guard while remaining hidden."))
	bool bPointBlankSightAlwaysExposes = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Sight",
		meta=(ClampMin="0.0", Units="cm",
			ToolTip="Maximum guard-to-player distance for unavoidable direct-sight exposure. Narrative AI Perception must still have valid sight and the Narrative Invisible tag is still respected. Easy example: 300 means three metres."))
	float PointBlankSightExposureDistance = 300.f;

	/** Immediately expose an infiltrator who fires while a defender has valid sight of them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bFireWhileSeenExposes = true;

	/** Treat unseen gunfire as investigation evidence instead of automatically identifying the shooter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bFireWhileUnseenStartsInvestigation = true;

	/** Confirm the identified damage source as exposed when the damage evidence is reported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bDamageImmediatelyExposes = true;

	/** Expose an infiltrator when a defender kill is witnessed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bSeenDefenderKillExposes = true;

	/** Start an anonymous investigation when a defender dies without a confirmed visible attacker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bUnseenDefenderDeathStartsInvestigation = true;

	/** By default, sounds, corpses and thrown distractions raise suspicion without identifying the hidden player. Enable only when the story intentionally allows clues alone to expose them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	bool bAnonymousEvidenceCanExpose = false;

	/** Accept real damage and heard gunshots or impacts from outside the Place. Ordinary sight outside does not expose a passer-by. An exposed shooter can be followed by sight outside. Native senses still decide what a guard actually hears or sees. This controls stealth evidence; Allow Personal Retaliation controls self-defence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence", meta=(DisplayName="Accept Outside Threat Evidence"))
	bool bRespondToOutsideThreats = true;

	/** How much a gunshot clue raises suspicion, from 0 to 1. For example, 0.35 adds 35 percent of a full suspicion meter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float GunshotSuspicion = 0.35f;

	/** How much a bullet-impact clue raises suspicion, from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float BulletImpactSuspicion = 0.2f;

	/** How much finding a corpse raises suspicion, from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float CorpseSuspicion = 0.5f;

	/** How much a thrown distraction raises suspicion, from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float ThrowableDistractionSuspicion = 0.25f;

	/**
	 * Narrative's foley notify reports each footstep as a hearing stimulus whose loudness and range
	 * are already scaled by the character's StealthRating (100 = silent). Turn those stimuli into
	 * anonymous suspicion so crouching and the Sneak perks actually change how guards react.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(DisplayName="Footsteps Raise Suspicion",
			ToolTip="Consume Narrative's Footstep hearing stimuli as evidence. Off ignores footsteps entirely."))
	bool bFootstepsRaiseSuspicion = true;

	/** How much one heard footstep adds to suspicion, from 0 to 1. Steps repeat, so keep this small. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0", DisplayName="Footstep Suspicion",
			ToolTip="Scaled by the stimulus loudness Narrative already reduced for high StealthRating. 0.06 adds six percent of a full suspicion meter per heard step."))
	float FootstepSuspicion = 0.06f;

	/** Seconds before the same guard counts another footstep from the same target as evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", Units="s", DisplayName="Footstep Evidence Cooldown",
			ToolTip="Narrative reports one stimulus per step. This keeps a walking player from filling the meter on one pass."))
	float FootstepEvidenceCooldown = 1.5f;

	/**
	 * How long an aggressive clue keeps admitting the player to combat and to the story-bounds
	 * contest. A gunshot heard ten minutes ago must not make a holstered visitor attackable forever;
	 * with the default, aggression expires and the guards fall back to investigation and warning.
	 * Zero disables the window and keeps the legacy "last evidence wins" behaviour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", Units="s", DisplayName="Aggression Evidence Window",
			ToolTip="Age after which a gunshot/impact/damage clue stops counting as aggression. 0 keeps the legacy behaviour."))
	float AggressionEvidenceWindow = 20.f;

	/**
	 * A recognised disguise still raises suspicion when it does not pass this Place's uniform policy
	 * (wrong faction, quality too low, missing clearance). Guards doubt the uniform instead of
	 * accepting it outright, so a bad disguise is a slow burn rather than a free pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(DisplayName="Disguise Raises Suspicion",
			ToolTip="Wrong or low-quality uniforms add suspicion while the disguised player stays visible."))
	bool bDisguiseRaisesSuspicion = true;

	/** Suspicion per second while a rejected disguise is visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence",
		meta=(ClampMin="0.0", ClampMax="1.0", DisplayName="Disguise Suspicion Per Second"))
	float DisguiseSuspicionPerSecond = 0.05f;

	/**
	 * Optional rules that override the evidence weights above (and may force exposure) without a C++
	 * change. A rule for an evidence channel replaces the matching property; a rule carrying a Tag
	 * lets external systems report that evidence by tag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 Evidence")
	TArray<FTerritoryEvidenceRule> EvidenceRules;

	/** Find the first rule for an evidence channel. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth|Evidence")
	bool FindEvidenceRule(ETerritoryStealthEvidence Evidence,
		FTerritoryEvidenceRule& OutRule) const;

	/** Find the first rule carrying this tag. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth|Evidence")
	bool FindEvidenceRuleForTag(FGameplayTag Tag, FTerritoryEvidenceRule& OutRule) const;

	/** Rule suspicion when one exists, otherwise the caller's built-in fallback. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth|Evidence",
		meta=(DisplayName="Get Evidence Suspicion"))
	float GetEvidenceSuspicion(ETerritoryStealthEvidence Evidence, float Fallback) const;

	/** True when a rule makes this evidence confirm exposure immediately. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth|Evidence")
	bool IsEvidenceForcedExposure(ETerritoryStealthEvidence Evidence) const;

	/** Maximum guards assigned to investigate one evidence response; zero prevents investigation assignments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0", ClampMax="8"))
	int32 MaximumInvestigators = 2;

	/**
	 * Minimum suspicion before any clue dispatches an investigator. A crouched player's footsteps add
	 * a little suspicion but stay below this, so sneaking does not send guards to their exact position;
	 * a distraction, gunshot, impact or real sight passes it immediately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0.0", ClampMax="1.0", DisplayName="Investigation Suspicion Threshold",
			ToolTip="Footsteps and other tiny clues build suspicion quietly below this value. 0 sends investigators for any suspicion at all."))
	float InvestigationSuspicionThreshold = 0.15f;

	/** Search radius in centimetres for selecting nearby guards to investigate evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="100.0", Units="cm"))
	float InvestigationRadius = 5000.f;

	/** Seconds an investigation goal remains valid before it expires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0.5", Units="s"))
	float InvestigationDuration = 12.f;

	/** Distance in centimetres at which an investigator considers the evidence location reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="10.0", Units="cm"))
	float InvestigationAcceptanceRadius = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation",
		meta=(ClampMin="0.0", ClampMax="2.0", Units="s",
			ToolTip="Muzzle and impact sounds from the same actor inside this window are treated as one missed shot."))
	float ShotCorrelationWindow = 0.3f;

	/** Optional Blueprint child can add Narrative dialogue or a custom search Behaviour Tree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Investigation")
	TSubclassOf<UTerritoryInvestigationActivity> InvestigationActivityClass;

	// ─── 08 Alarm ───

	/** Suspicion at which the Place reports Searching (guards keep investigating). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Alarm",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float SearchingSuspicionThreshold = 0.3f;

	/** Suspicion at which the Place reports Alarmed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Alarm",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float AlarmedSuspicionThreshold = 0.6f;

	/** Suspicion at which a confirmed target puts the Place into Lockdown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Alarm",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float LockdownSuspicionThreshold = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration",
		meta=(Categories="Territory.Event.Stealth",
			ToolTip="Sent to the exposed player's Ability System. A temporary stealth ability may listen for this event and cancel itself; passive equipment is not removed."))
	FGameplayTag BreakStealthGameplayEventTag;

	/** Runtime-safe event tag. Legacy assets that serialized an empty value fall back to Territory.Event.Stealth.Exposed. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth|Ability Integration")
	FGameplayTag GetEffectiveBreakStealthGameplayEventTag() const;

	/** Send the configured GAS event to the exposed target so abilities can respond to detection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration")
	bool bSendBreakStealthGameplayEvent = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration",
		meta=(ToolTip="Immediately cancels active Gameplay Abilities matching Stealth Ability Tags To Cancel when a guard confirms the player. Narrative Pro uses Abilities.Crouch for its built-in crouch stealth; Territory.Ability.Stealth is provided for a dedicated project stealth ability."))
	bool bCancelActiveStealthAbilitiesOnExposure = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration",
		meta=(ToolTip="Ability tags canceled on confirmed exposure. Defaults cover Narrative Pro crouch stealth and any Territory-aware stealth ability. Add your own ability tag here when using a custom stealth ability."))
	FGameplayTagContainer StealthAbilityTagsToCancel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration",
		meta=(ToolTip="Removes the configured temporary stealth Gameplay Effects when exposure is confirmed. This is needed for instant toggle abilities, such as Narrative Pro crouch, whose ability has already ended while its infinite stealth effect remains active."))
	bool bRemoveActiveStealthEffectsOnExposure = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 Ability Integration",
		meta=(ToolTip="Temporary Gameplay Effect classes removed on confirmed exposure. Easy example: add Narrative Pro GE_CrouchStealth here. Do not add permanent Sneak perk or equipment effects; those are capability bonuses and should survive detection."))
	TArray<TSubclassOf<UGameplayEffect>> StealthGameplayEffectsToRemove;

	/** Allow an equipped Territory disguise to replace only the perceived faction used by guards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Disguise",
		meta=(ToolTip="Enables faction uniforms in this Territory. The player's real Narrative faction and diplomacy are never changed."))
	bool bAllowFactionDisguises = true;

	/** The uniform must represent this Place's owning faction, not merely any friendly faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Disguise",
		meta=(ToolTip="Recommended for enemy bases. A Bandit Place accepts a Bandit uniform; a Civilian uniform does not automatically pass."))
	bool bRequireOwningFactionDisguise = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Disguise",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="Minimum profile quality accepted here. Easy example: use 0.5 for public streets and 0.9 for a guarded headquarters."))
	float MinimumDisguiseQuality = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Disguise",
		meta=(ToolTip="All of these access tags must exist on the uniform. Easy example: an officer-only floor can require Territory.Disguise.Clearance.Officer."))
	FGameplayTagContainer RequiredDisguiseClearanceTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Disguise",
		meta=(ToolTip="A failed scripted identity check burns the disguise for the checking faction. Disable this for a warning-only checkpoint."))
	bool bCompromiseDisguiseOnFailedIdentityCheck = true;

	// ─── 07 Contest Gate ───

	/**
	 * Story-bounds Places only. While true, a player who is inside the Place bounds with a
	 * holstered weapon does not start the story confrontation: the Place stays Claimed, guards
	 * keep patrolling, and a nearby guard delivers a warning line instead. Drawing the weapon
	 * contests immediately on the next evaluation.
	 *
	 * A faction already at war with this Place's owning faction is exempt: hostility is already
	 * declared, so the weapon is never checked. Capture Points and automatic capture ignore this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="07 Contest Gate",
		meta=(DisplayName="Require Wielded Weapon For Contest",
			ToolTip="Holstered weapon inside a story-bounds Place warns instead of contesting. A faction at war is always contested."))
	bool bContestRequiresWieldedWeapon = true;

	/**
	 * When true, an existing war no longer waives the wielded-weapon requirement for the contest:
	 * an unarmed visitor is warned even in a war. Guards still treat a faction at war as an enemy
	 * to fight, because that decision passes the war in as aggression.
	 *
	 * Default is on: a Place must not be flipped by an unarmed visitor, and a project that wants an
	 * authored war to admit a weaponless contest turns this off on the profile.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="07 Contest Gate",
		meta=(DisplayName="Ignore War For Weapon Requirement",
			ToolTip="On: a faction at war still has to show a weapon before the Place is contested. Guards still fight enemies at war."))
	bool bContestRequiresWieldedWeaponEvenAtWar = true;

	/**
	 * Line played on the nearest guard while a holstered player stays inside. Empty uses the native
	 * Territory.TaggedDialogue.Warn.HolsteredWeapon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="07 Contest Gate",
		meta=(Categories="Territory.TaggedDialogue",
			DisplayName="Holstered Weapon Warning Tag",
			ToolTip="Default is Territory.TaggedDialogue.Warn.HolsteredWeapon. Author the matching row in the guard's Tagged Dialogue Set."))
	FGameplayTag HolsteredWeaponWarningTag;

	/** Minimum seconds between warning lines for the same player. Zero warns every evaluation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="07 Contest Gate",
		meta=(ClampMin="0.0", DisplayName="Holstered Weapon Warning Cooldown"))
	float HolsteredWeaponWarningCooldown = 20.f;

	/** The warning tag actually used: the authored value, or the native Territory default. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Get Effective Holstered Weapon Warning Tag"))
	FGameplayTag GetEffectiveHolsteredWeaponWarningTag() const;

	/** The warning cooldown actually used: authored, clamped to zero. */
	UFUNCTION(BlueprintPure, Category="Territory|Stealth",
		meta=(DisplayName="Get Effective Holstered Weapon Warning Cooldown"))
	float GetEffectiveHolsteredWeaponWarningCooldown() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
