#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryTypes.h"
#include "Tales/TerritoryQuestRules.h"
#include "TerritoryDefinition.generated.h"

class AActor;
class ATerritoryCapturePoint;
class ATerritoryDistrictManagementPoint;
class ATerritoryGuardSpawnPoint;
class ATerritoryStoryOwnerSpawner;
class ATerritoryVolume;
class UDialogue;
class UNarrativeEvent;
class UNPCDefinition;
class UNPCActivityConfiguration;
class UNarrativeGameplayAbility;
class UGameplayEffect;
class UWeaponItem;
class UTerritoryCounterAttackProfile;
class UTerritoryDistrictManagementWidget;
class UTerritoryFloorCombatPolicy;
class UTerritoryGuardPostDefinition;
class UTerritoryPatrolGoal;
class UTerritoryProductionProfile;
class UTerritoryStealthProfile;
class UTriggerSet;

/**
 * One ownership benefit unlocked by a Place/Property upgrade level.
 * Narrative remains authoritative for abilities, effects, weapons, and input.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryPropertyGameplayBenefit
{
	GENERATED_BODY()

	/** Zero grants this tier as soon as the viewer's faction owns the claimed Property. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit",
		meta=(ClampMin="0", ToolTip="Minimum Property upgrade level required. Use zero for an ownership benefit and one or higher for a purchased upgrade."))
	int32 RequiredUpgradeLevel = 0;

	/** Replicated through Narrative GAS while this tier is active; use it in abilities, effects, conditions, and UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit",
		meta=(Categories="Territory.Property.Benefit", ToolTip="Stable capability tag granted by Narrative's Ability System while this Property benefit is active."))
	FGameplayTag BenefitTag;

	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit",
		meta=(MultiLine="true", ToolTip="Player-facing explanation shown in the Territory BENEFITS tab."))
	FText Description;

	/** Narrative abilities are granted once even if several owned Properties provide the same class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit|Narrative GAS",
		meta=(ToolTip="Narrative Gameplay Abilities granted to the owning player's character. Use each ability's Input Tag (for example Narrative.Input.Throw) for input binding."))
	TArray<TSubclassOf<UNarrativeGameplayAbility>> GrantedAbilities;

	/** Use Infinite effects for persistent upgrades. Instant effects cannot be revoked when ownership is lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit|Narrative GAS",
		meta=(ToolTip="Persistent Narrative Gameplay Effects applied while the Property remains owned. Infinite effects are recommended so Territory can remove them on loss."))
	TArray<TSubclassOf<UGameplayEffect>> GrantedGameplayEffects;

	/** Catalog entries shown in the Territory tab; Narrative inventory/vendor logic still owns purchase and equipment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Property Benefit|Weapons",
		meta=(ToolTip="Weapon item classes unlocked or serviced by this tier. Territory exposes them in UI; Narrative inventory/vendor assets remain purchase authority."))
	TArray<TSubclassOf<UWeaponItem>> UnlockedWeaponItems;
};

/** A patrol instruction stored relative to its guard-post actor. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGuardPatrolTemplateNode
{
	GENERATED_BODY()

	/** Patrol node position and facing relative to its guard-post actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol")
	FTransform RelativeTransform = FTransform::Identity;

	/** Seconds the guard waits after reaching this patrol node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol",
		meta=(ClampMin="0.0", Units="s"))
	float WaitTime = 2.f;

	/** Optional Guard.Activity tag identifying the activity requested at this patrol node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol",
		meta=(Categories="Guard.Activity"))
	FGameplayTag ActivityTag;
};

/** Territory-wide behavior applied to every stationary defender spawned for this asset. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGuardBehaviorTemplate
{
	GENERATED_BODY()

	FTerritoryGuardBehaviorTemplate();

	/** Narrative goal created for guards that have a patrol route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol")
	TSubclassOf<UTerritoryPatrolGoal> PatrolGoalClass;

	/** Server-side CharacterMovement RVO helps physical guards avoid blocking one another. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol|Crowd Avoidance")
	bool bEnablePatrolCrowdAvoidance = true;

	/** Radius in centimetres in which a guard considers nearby agents for movement avoidance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol|Crowd Avoidance",
		meta=(EditCondition="bEnablePatrolCrowdAvoidance", ClampMin="100.0", ClampMax="2000.0", Units="cm"))
	float PatrolAvoidanceConsiderationRadius = 500.f;

	/** Relative RVO avoidance weight from 0 to 1; affects how guards share avoidance responsibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol|Crowd Avoidance",
		meta=(EditCondition="bEnablePatrolCrowdAvoidance", ClampMin="0.0", ClampMax="1.0"))
	float PatrolAvoidanceWeight = 0.5f;

	/** Adds a small score preference for the nearest hostile player during a valid Territory war. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
	bool bPrioritizeClosestHostilePlayer = true;

	/** Let defenders respond to Narrative personal hostility, including real damage. Does not declare War or unlock capture. Same-faction actors and protective treaties remain protected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
	bool bAllowPersonalRetaliation = true;

	/** Let a defender fight an exposed enemy faction even while a quest keeps the Place Claimed. Local Alarm and hidden players still require personal hostility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
	bool bDefendAgainstExposedEnemies = true;

	/**
	 * Let a defender attack a faction it is at War with even when the Place is Claimed
	 * rather than Contested.
	 *
	 * Easy example: the Regime sends you to take Bandit outposts, then turns on you
	 * mid-game. With this off (default) the Regime's own guards keep ignoring you in
	 * every Place you captured for them, because those Places are Claimed and peaceful.
	 * With this on, the war has teeth — their guards defend the territory you won for
	 * them the moment the alliance breaks.
	 *
	 * Off by default: making every Claimed Place lethal at War changes stealth and quest
	 * content that deliberately runs inside hostile territories.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
	bool bEngageAtWarInClaimedTerritory = false;

	/** Optional exact Narrative faction filter for combat targets. Empty allows every faction that passes diplomacy, stealth and quest rules. Uses the perceived faction while a disguise is accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(Categories="Narrative.Factions"))
	FGameplayTagContainer CombatTargetFactions;

	/** Extra Narrative goal score given to the closest eligible hostile player; does not make a friendly player hostile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat",
		meta=(EditCondition="bPrioritizeClosestHostilePlayer", ClampMin="0.0", ClampMax="10.0"))
	float ClosestHostilePlayerGoalScoreBonus = 0.75f;

	/** Optional fallback relationship dialogue. Empty keeps the Narrative NPC Definition dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue")
	TObjectPtr<UTerritoryDiplomacyDialogueProfile> DialogueProfile;

	/** Exact owning-faction mappings for a guard class reused by several factions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue",
		meta=(TitleProperty="Faction"))
	TArray<FTerritoryFactionDialogueProfile> FactionDialogueProfiles;
};

/**
 * One authored floor of a Place, used to stage defenders and to drive per-floor story
 * objectives. Declaring floors is optional: a Definition with no floor rows keeps the
 * whole-Territory behaviour exactly, and guard posts then need no floor assignment.
 *
 * Floor state itself is never stored. The guard posts that stand on a floor own the live
 * counts, and the replicated per-floor read model is regrouped from them, so renumbering a
 * floor is an authoring-only change with no save migration.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryFloorTemplate
{
	GENERATED_BODY()

	/** Canonical floor identity within this Definition. Zero is ground; upper floors are positive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor",
		meta=(DisplayName="Floor Index", ClampMin="0"))
	int32 FloorIndex = 0;

	/** Designer-facing label for UI and objective text. Display only; never used as identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor",
		meta=(DisplayName="Display Name"))
	FText DisplayName;

	/**
	 * Authored defender quota for this floor. Zero means "every post on this floor", so a
	 * floor may be declared without restating its post count. The quota cannot exceed the
	 * number of guard posts assigned to the floor, because one post holds exactly one
	 * active guard; validation reports the mismatch instead of silently clamping it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor",
		meta=(ClampMin="0"))
	int32 DesiredGuards = 0;

	/**
	 * The one rule for "how many guards does this floor want", given that floor's own ceiling.
	 *
	 * Two callers share it so they cannot disagree about what a zero quota means: the garrison
	 * snapshot, which reports it as the floor's staffing target, and SpawnGuardsToCount, which now
	 * reserves it out of the Territory's target before the surplus is filled in Priority order.
	 *
	 * Capacity is a parameter rather than something this reads, because the two callers measure the
	 * ceiling differently and both are right to: the snapshot counts authored slot identities union
	 * the slots it can actually see standing (so a streamed-out post still contributes), while the
	 * fill counts the posts it can reach. Passing it keeps this a pure statement of the quota rule.
	 */
	static int32 ResolveGuardQuota(int32 AuthoredQuota, int32 FloorCapacity)
	{
		return AuthoredQuota > 0 ? AuthoredQuota : FloorCapacity;
	}

	/**
	 * Whether this row claims a share of the Territory's staffing target ahead of the global
	 * Priority order. Only an authored count does: a zero quota means "every post on this floor" for
	 * reporting, which is not the same as claiming the whole budget. Reading zero as a claim would
	 * make every declared floor reserve its full ceiling, which reorders deployment for content that
	 * never asked for it - the one thing this feature must not do.
	 */
	bool ClaimsFloorQuota() const { return DesiredGuards > 0; }

	/** Narrative events executed when this floor's last living defender is defeated, before any ownership change. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="Floor|Narrative")
	TArray<TObjectPtr<UNarrativeEvent>> FloorClearedEvents;

	/**
	 * Separation rule for this floor alone. Empty falls back to the Place's DefaultFloorCombatPolicy.
	 *
	 * The policy of the DEFENDER's floor is the one that applies, so this is "may this floor's
	 * defenders engage across floors", not "may anyone engage this floor". Overriding a single
	 * floor is how one stairwell can be held strictly while the rest of the building answers
	 * normally.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floor|Combat",
		meta=(ToolTip="Leave empty to inherit the Place's Default Floor Combat Policy. Set it to give this one floor a different rule."))
	TObjectPtr<UTerritoryFloorCombatPolicy> CombatPolicy;
};

/** One physical guard slot and its reusable Narrative guard-post profile. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGuardPostTemplate
{
	GENERATED_BODY()

	/** Stable name used to connect an authored guard-post actor to this row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post",
		meta=(DisplayName="Guard Post ID"))
	FName GuardPostID;

	/**
	 * Which authored floor this post stands on. Must name a row in the Place's Floors array
	 * once that array is non-empty; with no declared floors this value is ignored.
	 * Zero is ground; upper floors are positive.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post",
		meta=(DisplayName="Floor Index", ClampMin="0"))
	int32 FloorIndex = 0;

	/** Stable save identity for the physical post instance. Duplicating the Territory asset regenerates it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard Post")
	FGuid StableGuardPostGUID;

	/** Blueprint used when an editor builder creates this post. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	TSoftClassPtr<ATerritoryGuardSpawnPoint> ActorClass;

	/** Placement relative to the Territory actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	FTransform RelativeTransform = FTransform::Identity;

	/** Guard type, Narrative activity, TriggerSets, reserves, and default patrol policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	TObjectPtr<UTerritoryGuardPostDefinition> GuardPostDefinition;

	/** Optional per-post NPC. Empty uses the Territory's faction-aware guard definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Narrative")
	TObjectPtr<UNPCDefinition> NPCDefinitionOverride;

	/** Optional Narrative Pro activity configuration for this physical post. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Narrative")
	TObjectPtr<UNPCActivityConfiguration> ActivityConfigurationOverride;

	/** Optional Narrative Pro TriggerSets for guards created by this post. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Narrative")
	TArray<TSoftObjectPtr<UTriggerSet>> TriggerSetOverrides;

	/** Empty means guards inherit the current Territory owner at spawn time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Guard",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag FactionOverride;

	/** Guard-post filling priority; higher-priority posts are considered before lower-priority posts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Guard",
		meta=(ClampMin="0", UIMin="0", UIMax="100"))
	int32 Priority = 50;

	/** Finite replacement guards available at this post, separate from the active guard slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0", UIMin="0", UIMax="10"))
	int32 ReserveSlots = 1;

	/** Allow this post to deploy its remaining reserves after a vacancy, subject to ownership and spawn checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve")
	bool bAutoSpawnReserves = true;

	/** Seconds to wait before attempting an automatic reserve deployment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0.1", Units="s"))
	float ReserveSpawnDelay = 3.f;

	/** Seconds between failed automatic reserve placement attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0.1", Units="s"))
	float ReserveSpawnRetryInterval = 2.f;

	/** Radius in centimetres around the post in which reserve placement searches for valid navigation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="100.0", Units="cm"))
	float ReserveSpawnRadius = 600.f;

	/** Minimum distance in centimetres between a reserve spawn candidate and player cameras. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0.0", Units="cm"))
	float ReserveMinimumPlayerDistance = 500.f;

	/** Maximum navigation candidates checked during one reserve placement attempt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="1", ClampMax="64"))
	int32 ReserveSpawnCandidateCount = 12;

	/** Failed attempts before camera avoidance may relax. The total retry limit still bounds deployment attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0", ClampMax="20"))
	int32 ReserveCameraAvoidanceRetryLimit = 3;

	/** Maximum failed automatic placement attempts before this reserve deployment stops retrying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="1", ClampMax="100"))
	int32 ReserveTotalRetryLimit = 10;

	/** Choose whether ownership changes refill the post's reserves or preserve their remaining finite count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve")
	EReserveOwnershipPolicy ReserveOwnershipPolicy =
		EReserveOwnershipPolicy::RefillOnOwnerChange;

	/** Optional route relative to the guard-post actor. Empty uses the Guard Post Definition route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	TArray<FTerritoryGuardPatrolTemplateNode> PatrolRoute;

	/** Return to the first patrol node after the last node instead of ending the route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	bool bLoopPatrol = true;
};

/** Physical multiplayer capture-point Blueprint and settings for a Place. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCapturePointTemplate
{
	GENERATED_BODY()

	/** Include a physical flag or hold-zone helper when building this Place. Automatic Capture separately controls whether it generates progress. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point")
	bool bEnabled = false;

	/** Blueprint class used by the editor builder to create this helper actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<ATerritoryCapturePoint> ActorClass;

	/** Position, rotation and scale relative to the owning actor or template anchor, rather than absolute world coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	/** Radius of the physical capture zone in centimetres; 350 means 3.5 metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides, ClampMin="100.0", Units="cm"))
	float CaptureRadius = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides,
			ToolTip="Normal domination/multiplayer progress. Story Capture From Bounds disables automatic progress."))
	bool bAutomaticCapture = true;

	/** Hide this helper's presentation while its linked Territory is unavailable under story rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	bool bHideWhileUnavailable = true;
};

/** Narrative POI/interactable used to open Territory management. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryManagementPointTemplate
{
	GENERATED_BODY()

	/** Include a Narrative management interaction helper when building this Territory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point")
	bool bEnabled = false;

	/** Blueprint class used by the editor builder to create this helper actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<ATerritoryDistrictManagementPoint> ActorClass;

	/** Position, rotation and scale relative to the owning actor or template anchor, rather than absolute world coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	/** Empty means this District, or the parent District when authored on a Place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides, Categories="Territory"))
	FGameplayTag ManagedDistrictOverride;

	/** Territory management widget opened by this interaction through Narrative's UI system. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<UTerritoryDistrictManagementWidget> WidgetClass;

	/** Existing CommonUI layer tag where the management widget is opened. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides, Categories="UI.Layer"))
	FGameplayTag WidgetLayer;

	/** Maximum interaction distance in centimetres; 300 means three metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides, ClampMin="100.0", Units="cm"))
	float InteractionDistance = 600.f;
};

/** Protected owner Blueprint and Narrative dialogue settings for story handover. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryStoryOwnerTemplate
{
	GENERATED_BODY()

	/** Include a protected Narrative owner-spawner helper for explicit story handover. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner")
	bool bEnabled = false;

	/** Blueprint class used by the editor builder to create this helper actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<ATerritoryStoryOwnerSpawner> ActorClass;

	/** Position, rotation and scale relative to the owning actor or template anchor, rather than absolute world coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	/** Narrative Pro owns NPC identity, appearance, abilities, default dialogue, and spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TObjectPtr<UNPCDefinition> NPCDefinition;

	/** Attempt to begin the owner's dialogue when the handover activation supplies a valid player context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	bool bBeginDialogueOnActivation = true;

	/** Optional Narrative Dialogue Blueprint replacing the NPC Definition's default handover conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<UDialogue> DialogueOverride;

	/** Optional Native dialogue node ID to start from. Empty starts at the dialogue root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FName DialogueStartFromID;

	/** Maximum interaction distance in centimetres; 300 means three metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides, ClampMin="100.0", ClampMax="1000.0", Units="cm"))
	float InteractionDistance = 300.f;
};

/**
 * Single authoring source shared by City, District, and Place definitions.
 * Runtime ownership/state/progress still belongs to ATerritoryVolume and its subsystems.
 */
UCLASS(Abstract, BlueprintType, Const)
class TERRITORYFRAMEWORK_API UTerritoryDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	friend class UTerritoryDistrictDefinition;
	friend class UTerritoryCityDefinition;

public:
	UTerritoryDefinition();

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity",
		meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity")
	FText DisplayName;

	/** Persistent identity shared with the placed runtime actor and saved campaign records. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="01 Identity")
	FGuid StableTerritoryGUID;

	/** Default placement relative to the parent definition. City placement is relative to its level anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity")
	FTransform RelativeTransform = FTransform::Identity;

	/** Lightweight Blueprint class used for bounds/presentation; gameplay configuration comes from this asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity")
	TSoftClassPtr<ATerritoryVolume> TerritoryActorClass;

	/** Filled from City -> District -> Place arrays. Designers do not type parent tags twice. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="02 Hierarchy",
		meta=(Categories="Territory"))
	FGameplayTag DerivedParentTerritoryTag;

	/** Narrative faction that owns this Territory in a new campaign. Loading saved ownership takes precedence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag InitialOwningFaction;

	/** Story availability is independent from political control. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(DisplayName="Initial Availability",
			ToolTip="Locked keeps this Territory silent until its Narrative Locked exit conditions pass. Ownership is preserved. Applied only when a brand-new campaign starts; an existing save keeps its saved availability, so changing this appears to do nothing while testing on a save you already have. Easy example: lock the Farm Place until the story unlocks it, then test on a new campaign."))
	ETerritoryAvailability InitialAvailability = ETerritoryAvailability::Unlocked;

	/** Political state used for a new campaign. Automatic derives the starting state from the initial owner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign")
	ETerritoryInitialState InitialState = ETerritoryInitialState::Automatic;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(ToolTip="Place Definitions are Independent. City and District Definitions are Aggregate Only. The class fixes this value automatically."))
	ETerritoryControlMode ControlMode = ETerritoryControlMode::Independent;

	/** Strategic attacker slots available for this Territory. Narrative's per-target combat tokens remain separate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(ClampMin="1"))
	int32 MaxConcurrentAttackers = 3;

	/** Base currency income per economy cycle before state, faction, upgrade and hierarchy modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(ClampMin="0"))
	int32 PeriodicIncome = 100;

	/** Currency upkeep rate per guard each economy cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(ClampMin="0"))
	int32 GuardUpkeepPerCycle = 50;

	/** Base Narrative currency cost to recruit one guard before applicable modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(ClampMin="0"))
	int32 GuardRecruitmentCost = 50;

	/**
	 * Let the buyer's standing with the owner faction change guard recruitment prices.
	 * Easy example: you betray the Regime. With this on, staffing a garrison in Regime land
	 * you once served costs more, while an allied faction is cheaper.
	 * Off by default, because turning it on changes a price every project has already
	 * balanced around. Turn it on per Territory Definition where the betrayal should hurt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(DisplayName="Attitude Affects Prices"))
	bool bAttitudeAffectsPrices = false;

	/**
	 * Price multiplier when the buyer's faction is Allied or holds a Trade Agreement.
	 * Easy example: 0.85 gives an ally a 15% discount.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(EditCondition="bAttitudeAffectsPrices", ClampMin="0.0"))
	float FriendlyPriceMultiplier = 0.85f;

	/**
	 * Price multiplier for Neutral, Non-Aggression and Ceasefire relations.
	 * Easy example: leave at 1.0 so only friendship and war move the price.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(EditCondition="bAttitudeAffectsPrices", ClampMin="0.0"))
	float NeutralPriceMultiplier = 1.f;

	/**
	 * Price multiplier when the buyer's faction is at War with the owner.
	 * Easy example: 1.5 means a faction you are fighting charges you half again as much.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(EditCondition="bAttitudeAffectsPrices", ClampMin="0.0"))
	float WarPriceMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadOnly, Category="05 State Rules",
		meta=(DisplayName="State Rules (All Runtime States)",
			ToolTip="Always contains four rows: Locked availability, Unclaimed, Contested, and Claimed. Claimed is the stable ownership row after capture completes. Contested Entry Events run once whenever gameplay really enters Contested, not every capture tick."))
	TMap<ETerritoryState, FTerritoryStateConfig> StateConfigs;

	/**
	 * Optional Quest-owned runtime phases. While a matching Quest is active, only
	 * the selected primary/automatic rules pause; explicit Narrative Territory
	 * events and the live City -> District -> Place ownership reducer still work.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 State Rules|Quest Override",
		meta=(TitleProperty="QuestClass",
			DisplayName="Quest Runtime Overrides",
			ToolTip="Assign Quests that temporarily control this Territory. When the Quest ends, the normal rules continue from the current live state; skipped state rewards/events are not replayed."))
	TArray<FTerritoryQuestRuntimeOverrideRule> QuestRuntimeOverrides;

	/** Default pre-conflict stealth policy. A State Config may override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 State Rules|Stealth",
		meta=(ToolTip="Optional reusable stealth policy. Empty preserves legacy story bounds: entering immediately starts Contested. Easy example: assign a Rescue Mission profile so the player can enter Claimed enemy bounds while undetected."))
	TObjectPtr<UTerritoryStealthProfile> DefaultStealthProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Capture",
		meta=(DisplayName="Story Capture From Whole Place Bounds",
			ToolTip="Story mode uses the full multi-floor Place volume for contesting and explicit owner handover. Enabling this automatically turns off physical Capture Point progress, because both modes must never compete for capture authority."))
	bool bStoryCaptureFromBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Capture",
		meta=(ToolTip="Optional physical point, flag, or interaction actor. Automatic progress is for domination/multiplayer mode and is automatically turned off when Story Capture From Whole Place Bounds is enabled."))
	FTerritoryCapturePointTemplate CapturePoint;

	/** Fallback Narrative NPC Definition for guards when no matching faction or post override is supplied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards")
	TObjectPtr<UNPCDefinition> DefaultGuardDefinition;

	/** Exact owning-faction mappings to Narrative guard definitions, allowing one Place to support different occupiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards",
		meta=(TitleProperty="Faction"))
	TArray<FTerritoryFactionGuardDefinition> FactionGuardDefinitions;

	/** Desired initial garrison count for a new campaign, bounded by the available guard posts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards",
		meta=(ClampMin="0"))
	int32 InitialGuardCount = 3;

	/** Choose how the new owner's garrison is populated after a verified capture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards")
	ETerritoryPostCaptureGarrisonPolicy PostCaptureGarrisonPolicy =
		ETerritoryPostCaptureGarrisonPolicy::PlayerChooses;

	/** Shared patrol, crowd-avoidance, and target-priority policy for this Territory's guards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards")
	FTerritoryGuardBehaviorTemplate GuardBehavior;

	/** Physical guard-post templates, stable identities, reserve settings and optional per-post patrol overrides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards",
		meta=(TitleProperty="GuardPostID"))
	TArray<FTerritoryGuardPostTemplate> GuardPosts;

	/** Narrative events executed when a registered defender dies, using the live Territory transition context. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="07 Guards|Narrative")
	TArray<TObjectPtr<UNarrativeEvent>> DefenderDiedEvents;

	/** Narrative events executed when the last relevant defender is defeated; useful for revealing a story owner. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="07 Guards|Narrative")
	TArray<TObjectPtr<UNarrativeEvent>> AllDefendersDefeatedEvents;

	/** Reusable strategic scheduling, finite-force and Narrative attacker configuration for this Territory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack")
	TObjectPtr<UTerritoryCounterAttackProfile> CounterAttackProfile;

	/** Typed entry routes attackers may use. Vehicle entries need a valid road route; spawning must pass approach validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(TitleProperty="ApproachID"))
	TArray<FTerritoryAssaultApproach> CounterAttackApproaches;

	/** Relative quality multiplier used when estimating garrison defence power for strategic planning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float GuardQuality = 1.f;

	/** Additional authored defence strength used by strategic counterattack planning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float FortificationStrength = 0.f;

	/** Authored contribution from nearby allied support to the strategic defence estimate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float NearbyAlliedSupport = 0.f;

	/**
	 * Relative importance of this Territory to strategic assault planning; higher values make it a
	 * more valuable target. Aggregated by *maximum*, not by sum: a District or a defence front is as
	 * valuable as its single most valuable member, so raising one Place's value raises the whole
	 * District and adding another trivial Place does not inflate the number.
	 * See TerritoryAssaultTargetPolicy::AggregateStrategicValue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float StrategicValue = 1.f;

	/**
	 * Controls only the compact passive Territory card inside Narrative's gameplay
	 * HUD. Notifications, POIs, map/compass markers, menus, and management remain
	 * available. Broad City volumes default to off; Places and Districts default on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="09 Presentation",
		meta=(DisplayName="Show Passive Gameplay HUD Card",
			ToolTip="Show the compact Territory location/capture card while the player is inside this exact Territory. Turn this off for broad ambient City or District volumes. Live notifications, POIs, map markers, Command Center, and management are not hidden."))
	bool bShowGameplayHUD = true;

	/** Optional Narrative interaction and UI helper used to manage this Place or its District. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="09 Management")
	FTerritoryManagementPointTemplate ManagementPoint;

	/** Copies this authoring asset into an actor. Saved owner/state/progress are never overwritten here. */
	UFUNCTION(BlueprintCallable, Category="Territory|Definition")
	bool ApplyToTerritory(ATerritoryVolume* Territory) const;

	const FTerritoryGuardPostTemplate* FindGuardPost(FName GuardPostID) const;

	/** Find a guard-post row by its stable post ID. False means no matching row was found. */
	UFUNCTION(BlueprintPure, Category="Territory|Definition",
		meta=(DisplayName="Get Guard Post Template"))
	bool GetGuardPostTemplate(FName GuardPostID,
		FTerritoryGuardPostTemplate& OutGuardPost) const;

	/**
	 * Floors are authored data on UTerritoryPlaceDefinition only: a City or a District owns no
	 * defenders, so it has nothing to stage by floor. These three queries stay declared on the
	 * shared base so existing Blueprints that call them keep compiling, and each one resolves
	 * through the Place subclass and reports the honest answer for an aggregate: no floor row,
	 * zero posts, no authored floors. That is a true answer, not a compatibility stub - a City
	 * genuinely has no floors - and it is the migration path AGENTS.md requires for a breaking
	 * change. Any new authoring surface must live on the Place.
	 */
	const FTerritoryFloorTemplate* FindFloor(int32 FloorIndex) const;

	/** Find an authored floor row by its index. False means this definition declares no such floor, which a City or a District never does. */
	UFUNCTION(BlueprintPure, Category="Territory|Definition",
		meta=(DisplayName="Get Floor Template"))
	bool GetFloorTemplate(int32 FloorIndex,
		FTerritoryFloorTemplate& OutFloor) const;

	/** Number of guard posts assigned to an authored floor. This is that floor's physical guard ceiling. */
	UFUNCTION(BlueprintPure, Category="Territory|Definition",
		meta=(DisplayName="Get Floor Guard Post Count"))
	int32 GetFloorGuardPostCount(int32 FloorIndex) const;

	/** True when this definition declares at least one authored floor. Only a Place can. */
	UFUNCTION(BlueprintPure, Category="Territory|Definition",
		meta=(DisplayName="Has Authored Floors"))
	bool HasAuthoredFloors() const;

	/** Refresh derived child parent links after changing hierarchy arrays. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Territory|Definition")
	virtual void RefreshHierarchyLinks();

	virtual bool IsDefinitionCompatible(const ATerritoryVolume* Territory) const;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	void SetDerivedParentTag(const FGameplayTag& ParentTag);
};

/** One independently capturable Place. */
UCLASS(BlueprintType, meta=(DisplayName="Territory Place Definition"))
class TERRITORYFRAMEWORK_API UTerritoryPlaceDefinition : public UTerritoryDefinition
{
	GENERATED_BODY()

public:
	UTerritoryPlaceDefinition();

	/**
	 * Optional authored floors for staging defenders and driving per-floor story beats.
	 * Empty disables floor grouping entirely. When non-empty, each post's FloorIndex must
	 * name a row here.
	 *
	 * Authored on the Place and nowhere else. A City or a District is an aggregate over
	 * Places and registers no defenders of its own, so floors could never do anything there;
	 * declaring them on the shared base only made an aggregate's floor rows a permanent
	 * validation error whose own advice (add guard posts) named an array the City/District
	 * panel hides. GuardPosts stays on the base so the shared floor-quota check can still
	 * count a Place's posts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards|Floor",
		meta=(TitleProperty="FloorIndex"))
	TArray<FTerritoryFloorTemplate> Floors;

	/**
	 * Separation rule applied to every floor row of this Place that names no policy of its own.
	 * Empty means floors restrict nothing, which is the behaviour of every Place until this is
	 * authored, so an existing project is unaffected by the feature.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards|Floor",
		meta=(ToolTip="Optional. Leave empty to keep floors as pure staging groups with no effect on combat. Set it to separate this Place's floors, then override individual floors in the Floors list if one needs a different rule."))
	TObjectPtr<UTerritoryFloorCombatPolicy> DefaultFloorCombatPolicy;

	/** Optional semantic role used by Territory/Narrative tags, filters, conditions, and UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Benefits",
		meta=(Categories="Territory.Property.Role", ToolTip="What this Property provides. Example: Territory.Property.Role.ArmsShop for a blacksmith or gunsmith."))
	FGameplayTag PropertyRoleTag;

	/** Ownership and upgrade-level benefits reconciled onto each owning Narrative character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Benefits",
		meta=(TitleProperty="DisplayName", ToolTip="Abilities, persistent Gameplay Effects, benefit tags, and weapon catalog entries unlocked by owning and upgrading this Property."))
	TArray<FTerritoryPropertyGameplayBenefit> GameplayBenefits;

	/** Reusable input/output recipes processed by the existing Territory economy and Narrative inventory systems. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Production")
	TObjectPtr<UTerritoryProductionProfile> ProductionProfile;

	/** Highest purchasable upgrade level for this Place; zero disables further upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Upgrades",
		meta=(ClampMin="0"))
	int32 MaxUpgradeLevel = 3;

	/** Base currency cost used when calculating a Place upgrade purchase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Upgrades",
		meta=(ClampMin="0"))
	int32 UpgradeCostPerLevel = 500;

	/** Additional base income earned per purchased Place upgrade level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Upgrades",
		meta=(ClampMin="0"))
	int32 IncomeBonusPerLevel = 25;

	/** Optional protected Narrative NPC and dialogue configuration for explicit story handover. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="11 Place|Story")
	FTerritoryStoryOwnerTemplate StoryOwner;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual bool IsDefinitionCompatible(const ATerritoryVolume* Territory) const override;

	/**
	 * The floor-separation policy that governs a defender standing on FloorIndex.
	 *
	 * Resolution order is the floor row's own CombatPolicy, then DefaultFloorCombatPolicy, then
	 * null (which every caller reads as "floors separate nothing"). INDEX_NONE names no row and so
	 * resolves to the Place default: a defender whose floor could not be resolved is not a defender
	 * on a floor.
	 */
	const UTerritoryFloorCombatPolicy* GetEffectiveFloorCombatPolicy(int32 FloorIndex) const;

#if WITH_EDITOR
	/** Validates the floor rows against this Place's own guard posts, then defers to the base checks. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** One District and the complete list of Places that determine its control. */
UCLASS(BlueprintType, meta=(DisplayName="Territory District Definition"))
class TERRITORYFRAMEWORK_API UTerritoryDistrictDefinition : public UTerritoryDefinition
{
	GENERATED_BODY()

public:
	UTerritoryDistrictDefinition();

	/** Place Definitions belonging to this District. Refreshing hierarchy links derives their parent tags from this list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 District|Hierarchy")
	TArray<TObjectPtr<UTerritoryPlaceDefinition>> Places;

	/** Mark this District as a capital for the authored capital reward and income rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 District|Economy")
	bool bIsCapital = false;

	/** Authored currency reward for capturing this capital, subject to the current reward and faction rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 District|Economy", meta=(ClampMin="0"))
	int32 CapitalCaptureReward = 500;

	/** Multiplier applied to qualifying capital income; 2 means twice the base rate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 District|Economy",
		meta=(ClampMin="1.0"))
	float CapitalIncomeMultiplier = 2.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void RefreshHierarchyLinks() override;
	virtual bool IsDefinitionCompatible(const ATerritoryVolume* Territory) const override;
};

/** One City and the complete list of District assets that determine its control. */
UCLASS(BlueprintType, meta=(DisplayName="Territory City Definition"))
class TERRITORYFRAMEWORK_API UTerritoryCityDefinition : public UTerritoryDefinition
{
	GENERATED_BODY()

public:
	UTerritoryCityDefinition();

	/** Authored currency reward for capturing this capital, subject to the current reward and faction rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 City|Economy", meta=(ClampMin="0"))
	int32 CapitalCaptureReward = 1000;

	/** District Definitions belonging to this City. Their Place lists define the rest of the hierarchy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 City|Hierarchy")
	TArray<TObjectPtr<UTerritoryDistrictDefinition>> Districts;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void RefreshHierarchyLinks() override;
	virtual bool IsDefinitionCompatible(const ATerritoryVolume* Territory) const override;
};
