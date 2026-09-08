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

/** One physical guard slot and its reusable Narrative guard-post profile. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGuardPostTemplate
{
	GENERATED_BODY()

	/** Stable name used to connect an authored guard-post actor to this row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post",
		meta=(DisplayName="Guard Post ID"))
	FName GuardPostID;

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
			ToolTip="Locked keeps this Territory silent until its Narrative Locked exit conditions pass. Ownership is preserved."))
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

	/** Relative importance of this Territory to strategic assault planning; higher values make it a more valuable target. */
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

	/** Refresh derived child parent links after changing hierarchy arrays. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Territory|Definition")
	virtual void RefreshHierarchyLinks();

	virtual bool IsDefinitionCompatible(const ATerritoryVolume* Territory) const;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
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
