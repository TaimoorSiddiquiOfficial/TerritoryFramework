#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Navigation/TerritoryRoadTypes.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "TerritoryCounterAttackTypes.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;
class ANarrativeVehicleBase;

UENUM(BlueprintType)
enum class ETerritoryAttackApproachType : uint8
{
	Road UMETA(ToolTip="Ground approach that follows a road or open street."),
	Gate UMETA(ToolTip="Fortified entrance approach. Example: attackers gather outside the city gate."),
	Water UMETA(ToolTip="Boat, shore, river, or sewer-water approach."),
	Rooftop UMETA(ToolTip="High route across roofs. Use only when the Narrative NPC can navigate it."),
	Tunnel UMETA(ToolTip="Underground or hidden passage approach."),
	Air UMETA(ToolTip="Air insertion approach. Use only with a matching NPC/activity/navigation setup."),
	Custom UMETA(ToolTip="Project-defined approach whose meaning is explained by its Approach ID and assets.")
};

UENUM(BlueprintType)
enum class ETerritoryAssaultState : uint8
{
	Grace,
	Evaluating,
	ScheduledWarning,
	WaitingForPlayerProximity,
	Active,
	Succeeded,
	Defeated,
	Cancelled,
	/**
	 * Physical attackers hold the cleared Place while no living defending player is
	 * present. Ownership changes only when the saved deadline expires. Appended to
	 * preserve the serialized values of all existing states.
	 */
	RecaptureCountdown UMETA(DisplayName="Recapture Countdown")
};

UENUM(BlueprintType)
enum class ETerritoryAssaultResolution : uint8
{
	None,
	DecisionRollFailed,
	DiplomacyBlocked,
	InvalidTerritory,
	InvalidApproachOrRoute,
	BudgetBlocked,
	ConfigurationInvalid,
	OwnershipChanged,
	AllAttackersRemoved,
	CaptureCompleted,
	ManuallyCancelled,
	SpawnFailed,
	QuestRuleBlocked UMETA(DisplayName="Narrative Quest Rule Blocked",
		ToolTip="A pending strategic counterattack was cancelled because its Narrative quest rules no longer passed."),
	StagingDistrictUnavailable UMETA(DisplayName="No Secure Staging District",
		ToolTip="The attacking faction no longer owns a loaded, unlocked District whose complete authored Place set is secure, so an undeployed strategic counterattack cannot continue."),
	TargetEscaped UMETA(DisplayName="Story Target Escaped",
		ToolTip="A player-chases-enemy story target completed Narrative's authored vehicle exit route before the finite target was killed."),
	ChaseDistanceLost UMETA(DisplayName="Story Chase Distance Lost",
		ToolTip="Every player remained beyond the configured chase distance for the full grace period, so the finite story target escaped."),
	ReinforcementCapabilityLost UMETA(DisplayName="Reinforcement Capability Lost",
		ToolTip="The faction lost the Territory.Capability.Reinforcements perk before its physical counterattack deployed. Already deployed attackers are never erased by this rule.")
};

/** Why an assault was admitted. Story pursuit is explicit and never selected by normal strategy. */
UENUM(BlueprintType)
enum class ETerritoryAssaultLaunchMode : uint8
{
	StrategicCounterattack UMETA(DisplayName="Strategic Counterattack",
		ToolTip="Normal domination response. The configured staging rule, diplomacy, budgets, route, grace, warning, and first-player activation gate all apply."),
	StoryPursuit UMETA(DisplayName="Story Pursuit / Boss Chase",
		ToolTip="Tales-triggered pursuit. It may bypass the staging-District rule only when the force profile also allows that exception; it never bypasses diplomacy, finite force, route, or physical capture rules.")
};

/** Direction of one explicit Tales-driven pursuit. Normal strategic counters ignore this. */
UENUM(BlueprintType)
enum class ETerritoryStoryPursuitDirection : uint8
{
	EnemyChasesPlayer UMETA(DisplayName="Enemy Chases Player",
		ToolTip="The hostile driver enters at the authored vehicle spawn, parks at the Place drop-off, dismounts, then Narrative combat takes over. Example: regime hunters arrive after a betrayal."),
	PlayerChasesEnemy UMETA(DisplayName="Player Chases Enemy",
		ToolTip="The capo or underboss starts at the Place drop-off and drives toward the authored route exit. Killing the finite target defeats the pursuit; reaching the exit records Target Escaped.")
};

/** Strategic territory holding required before this faction may launch a normal counterattack. */
UENUM(BlueprintType)
enum class ETerritoryAssaultStagingRequirement : uint8
{
	None UMETA(DisplayName="No Territory Holding Required",
		ToolTip="Use only for forces that should not depend on domination holdings."),
	OwnsSecureDistrict UMETA(DisplayName="Owns At Least One Secure District",
		ToolTip="The faction must own at least one loaded, unlocked District whose complete authored Place set is secure. Example: Bandits may counter from Castle Hill only after every unlocked Place there is securely Bandit-owned. Locked, partial, Contested, and Unclaimed Districts do not count.")
};

/** How many separate finite battles one ownership-response schedule may create. */
UENUM(BlueprintType)
enum class ETerritoryCounterScheduleMode : uint8
{
	SingleAssault UMETA(DisplayName="One Assault",
		ToolTip="Schedules one finite battle and never repeats it automatically."),
	FiniteSeries UMETA(DisplayName="Finite Series",
		ToolTip="Repeats after the cooldown until Maximum Scheduled Assaults is reached. Every battle still has its own finite Planned Force."),
	UnlimitedSeries UMETA(DisplayName="Unlimited Schedule",
		ToolTip="May schedule future finite battles for as long as ownership, war, staging, quest, route, and budget rules continue to pass. It never creates infinite attackers inside one battle.")
};

/** Narrative Pro campaign-clock policy used before a strategic battle is evaluated. */
UENUM(BlueprintType)
enum class ETerritoryCounterTimePolicy : uint8
{
	AnyTime UMETA(DisplayName="Any Narrative Time",
		ToolTip="The grace/cooldown is the only time gate."),
	NarrativeTimeWindow UMETA(DisplayName="Narrative / Ultra Dynamic Sky Time Window",
		ToolTip="After grace, wait until Narrative Game State enters the configured time-of-day window. A Narrative Pro Ultra Dynamic Sky setup follows the same clock.")
};

/** Physical arrival used by one authored ingress route. */
UENUM(BlueprintType)
enum class ETerritoryAssaultEntryType : uint8
{
	OnFoot UMETA(DisplayName="On Foot",
		ToolTip="Spawn finite Territory assault NPCs at this approach and use Narrative NPC navigation to enter the Place."),
	NarrativeVehicle UMETA(DisplayName="Narrative Vehicle",
		ToolTip="The first configured deployments arrive with a Narrative vehicle. Territory follows the validated ZoneGraph road after Narrative mounts and possesses it; remaining force can continue on foot from the authored drop-off.")
};

/** Editor-authored, typed ingress point stored relative to its Territory actor. */
USTRUCT(BlueprintType)
struct FTerritoryAssaultApproach
{
	GENERATED_BODY()

	/** Stable per-territory route identity. Example: Blacksmith_WestRoad. Never localize or rename after release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(DisplayName="Approach ID", ToolTip="Stable unique ID on this Territory, for example Blacksmith_WestRoad. Blank IDs are auto-filled in the editor."))
	FName ApproachID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ToolTip="Meaning of this Place ingress route for UI and project rules. It does not replace navigation validation."))
	ETerritoryAttackApproachType Type = ETerritoryAttackApproachType::Road;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Entry",
		meta=(ToolTip="Choose On Foot for a normal Narrative NPC route, or Narrative Vehicle for a road arrival that reuses Narrative's vehicle, seat, interaction ability, controller possession, and ZoneGraph road network."))
	ETerritoryAssaultEntryType EntryType = ETerritoryAssaultEntryType::OnFoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ToolTip="Spawn point relative to the Place actor. Example: 2,000 cm west of the Blacksmith entrance."))
	FTransform RelativeSpawnTransform;

	/** Existing Narrative vehicle Blueprint. Example: /Game/HOPTRENDY/BPV_Sedan_Mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Entry",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides,
			ToolTip="Narrative Vehicle class spawned on the server. It must derive from ANarrativeVehicleBase and provide usable mount seats."))
	TSoftClassPtr<ANarrativeVehicleBase> VehicleClass;

	/** Roadside park/dismount point relative to the Territory actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Entry",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides,
			ToolTip="Vehicle park and dismount transform relative to this Place. Put it on a ZoneGraph road with a NavMesh walk route into the Place."))
	FTransform RelativeVehicleDropOffTransform;

	/** Optional placed ATerritoryRoadGuide. Blank uses Approach ID by convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Road Guide",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides,
			ToolTip="Stable ID of a placed Territory Road Guide. Blank looks for a guide whose ID matches this Approach ID. When found, its spline start/end replace the fallback spawn/drop-off transforms."))
	FName RoadGuideID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Road Guide",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides,
			ToolTip="Directional lane on the Road Guide. Reverse story pursuits automatically mirror left/right so both directions stay on the correct side."))
	ETerritoryRoadLaneSide RoadLaneSide = ETerritoryRoadLaneSide::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Road Guide",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides,
			ToolTip="Centre/left/right collision probes used by the possessed Narrative mission car. Narrative Mass traffic continues using its own obstacle grid."))
	FTerritoryVehicleAwarenessSettings VehicleAwareness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Road Guide",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides,
			ToolTip="Safe cleanup policy for this temporary reinforcement or pursuit car."))
	FTerritoryVehicleRetirementSettings VehicleRetirement;

	/** Bounded number of cars created from this route in one finite assault. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Entry",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides, ClampMin="1", ClampMax="8",
			ToolTip="Maximum Narrative vehicles deployed by this approach during one assault. Later finite attackers use the drop-off as an on-foot entry."))
	int32 MaximumVehicleDeployments = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Entry",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides, ClampMin="0.0",
			ToolTip="Desired AI road speed in cm/s. Zero uses Territory's safe default of 1,400 cm/s. The vehicle still uses Narrative possession and its Chaos movement component."))
	float VehicleMaximumDriveSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack|Entry",
		meta=(EditCondition="EntryType == ETerritoryAssaultEntryType::NarrativeVehicle",
			EditConditionHides, ClampMin="5.0", ClampMax="600.0",
			ToolTip="Maximum real seconds allowed to mount, follow the ZoneGraph road, park and dismount. A timed-out driver withdraws safely instead of leaving a broken controller or car."))
	float VehicleIngressTimeoutSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ClampMin="1", ToolTip="Largest part of one wave allowed to use this approach. This never creates infinite reserves."))
	int32 MaxWaveSize = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ToolTip="Disabled approaches are never selected or used for physical spawning."))
	bool bEnabled = true;
};

/** Optional overrides for one deliberate story pursuit. No live actor pointer is persisted. */
USTRUCT(BlueprintType)
struct FTerritoryStoryPursuitOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit")
	ETerritoryStoryPursuitDirection Direction =
		ETerritoryStoryPursuitDirection::EnemyChasesPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ToolTip="When false, this story encounter may fight and chase but can never register capture pressure or hand the Place to the hostile faction. Useful for an optional capo encounter during District capture."))
	bool bAllowsTerritoryCapture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ToolTip="When false, the explicit Narrative Event always launches after validation. Enable only when the authored story intentionally wants the profile's strategic probability roll."))
	bool bUseStrategicDecisionRoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ToolTip="Optional player or story focus captured when the event starts. Zero uses the normal Place objective. This is a value, not a saved actor reference."))
	FVector StoryFocusLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ToolTip="Optional Narrative NPC Definition for the capo, underboss, hunter or escort. Empty reuses the attacking faction definition from the Counter Attack Profile."))
	TSoftObjectPtr<UNPCDefinition> AttackerDefinitionOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ClampMin="0", ClampMax="64", ToolTip="Zero reuses Planned Force from the profile. One creates a single kill-or-escape target; larger values create a finite escort or hunter group."))
	int32 PlannedForceOverride = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ClampMin="0", ClampMax="32", ToolTip="Zero reuses the profile Wave Size. The effective value is always clamped to the finite planned force."))
	int32 WaveSizeOverride = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ClampMin="-1.0", ToolTip="Negative uses the profile grace period. Zero begins evaluation immediately. Positive values provide an authored story delay in Narrative campaign time."))
	float GracePeriodOverrideGameTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit",
		meta=(ToolTip="Optional stable, non-localized identifier used by saves, logs and story outcome handling. Example: CastleHill_UnderbossEscape."))
	FName ScenarioID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit|Chase",
		meta=(ClampMin="0.0", ToolTip="Maximum distance from the closest participating player to a fleeing target vehicle. Zero disables the distance failure rule."))
	float MaximumChaseDistance = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit|Chase",
		meta=(ClampMin="0.0", ToolTip="How long every player may remain outside Maximum Chase Distance before the target escapes. This protects the mission from a single short road separation."))
	float ChaseDistanceGraceSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit|Final Fight",
		meta=(ToolTip="When enabled, a fleeing target whose Narrative vehicle falls below the health threshold dismounts and moves to the Road Guide final-fight point instead of being declared escaped."))
	bool bAbandonDamagedVehicleForFinalFight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit|Final Fight",
		meta=(ClampMin="0.01", ClampMax="0.95", ToolTip="Narrative vehicle health fraction that triggers the authored abandonment/final-fight handoff."))
	float VehicleAbandonHealthFraction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit|Traffic",
		meta=(ToolTip="Activate the Road Guide's referenced Narrative Quest Road Controls while this pursuit is active. Use it for authored slow traffic, intersections, and chase pressure."))
	bool bActivateRoadMissionTraffic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Story Pursuit|Traffic",
		meta=(ClampMin="-1", ClampMax="200", ToolTip="Negative uses the Road Guide traffic count. Zero clears controlled ambient traffic; positive values request that many Narrative Mass vehicles."))
	int32 MissionTrafficVehicleCountOverride = -1;
};

/** Maps one replicated player power tag to a simple campaign power level. */
USTRUCT(BlueprintType)
struct FTerritoryPlayerPowerTier
{
	GENERATED_BODY()

	/** Exact tag granted by a Narrative perk Gameplay Effect. Example: Territory.Power.Tier.3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(ToolTip="Exact replicated Gameplay Tag granted to the player by a Narrative perk or story reward. Example: Territory.Power.Tier.3."))
	FGameplayTag PlayerPowerTag;

	/** Enemy level associated with this tag before the configured enemy offset is added. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(ClampMin="1", ToolTip="Campaign power level represented by the tag. Example: tier 3 can map to player power level 6."))
	int32 PlayerPowerLevel = 1;
};

/** One designer override for how many signature cars a faction receives at a Narrative difficulty. */
USTRUCT(BlueprintType)
struct FTerritoryDifficultyVehicleCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(ToolTip="Narrative Pro gameplay difficulty selected in Game User Settings. Easy example: Hard can send two Bandit cars while Easy sends one."))
	ENarrativeGameplayDifficulty Difficulty = ENarrativeGameplayDifficulty::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(ClampMin="0", ClampMax="8",
			ToolTip="Maximum cars in the complete finite assault, not per wave. Zero makes this faction arrive on foot at this difficulty."))
	int32 MaximumCars = 1;
};

/** Per-attacking-faction physical force configuration. */
USTRUCT(BlueprintType)
struct FTerritoryFactionAssaultConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(Categories="Narrative.Factions", ToolTip="Exact Narrative faction allowed to launch this force. Example: Narrative.Factions.Bandits."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ToolTip="Narrative Pro NPC Definition used for every physical attacker. Territory never saves the spawned pawn pointer."))
	TObjectPtr<UNPCDefinition> AttackerDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ToolTip="Optional Narrative activity configuration override. Leave empty to use the NPC Definition's normal configuration."))
	TObjectPtr<UNPCActivityConfiguration> ActivityConfigurationOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ToolTip="Optional Narrative TriggerSets added to the spawned attackers."))
	TArray<TSoftObjectPtr<UTriggerSet>> TriggerSetOverrides;

	/** Faction identity shown to the player before the occupants are visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle",
		meta=(DisplayName="Faction Signature Vehicle",
			ToolTip="Narrative Vehicle Blueprint used by this faction on every vehicle approach. Easy example: Bandits use a rusty pickup while the Regime uses a black sedan, so the player recognizes the attacker from far away. Leave empty to use the vehicle stored on each Approach."))
	TSoftClassPtr<ANarrativeVehicleBase> SignatureVehicleClass;

	/** Narrative difficulty chooses a bounded car budget without changing the finite attacker count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle|Difficulty",
		meta=(DisplayName="Scale Car Count With Narrative Difficulty",
			ToolTip="Recommended. Uses Narrative Pro's current gameplay difficulty when the assault is evaluated. Empty overrides use Easy 1, Medium 1, Hard 2, Insane 3, always clamped by the authored road approaches and finite force."))
	bool bScaleVehicleCountByNarrativeDifficulty = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle|Difficulty",
		meta=(EditCondition="bScaleVehicleCountByNarrativeDifficulty", EditConditionHides,
			TitleProperty="Difficulty",
			ToolTip="Optional per-difficulty replacement values. Add only the difficulties you want to change; missing rows use the safe built-in values."))
	TArray<FTerritoryDifficultyVehicleCount> VehicleCountsByDifficulty;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="0.0", ToolTip="Strategic strength used for planning probability. Example: 200 is twice a baseline force of 100; it never captures by itself."))
	float MilitaryPower = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="0 means the faction cannot afford a response; 1 means fully ready."))
	float EconomyReadiness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="0 means no supply/proximity support; 1 means fully supplied."))
	float SupplyReadiness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="-1.0", ClampMax="1.0", ToolTip="Recent performance: -1 losing badly, 0 neutral, 1 winning strongly."))
	float RecentMomentum = 0.f;

	/** Durable local political/logistical reach. Higher values accelerate finite responses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Local political/logistics reach: 0 distant and weak, 1 deeply established. Higher values shorten response timing."))
	float TerritorialInfluence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="1", ToolTip="Total finite attackers in one assault. Example: 6 means at most six lives, never automatic infinite replacement."))
	int32 PlannedForce = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(ClampMin="1", ToolTip="Maximum attackers attempted per reserve wave, still limited by Planned Force and approach capacity."))
	int32 WaveSize = 3;

	/** Domination holding needed for ordinary strategic counters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Staging",
		meta=(ToolTip="Normal strategic admission rule. Recommended: require one loaded, unlocked District fully secured through its authored Places, so a defeated faction with no operational holding cannot keep launching counters."))
	ETerritoryAssaultStagingRequirement StagingRequirement =
		ETerritoryAssaultStagingRequirement::OwnsSecureDistrict;

	/** Dual opt-in for a Tales story pursuit that intentionally has no staging District. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Story",
		meta=(ToolTip="Allows this force to be used by a Story Pursuit / Boss Chase event without owning a District. The event must also explicitly select Story Pursuit. Normal counters never use this exception."))
	bool bAllowStoryPursuitWithoutStagingDistrict = false;

	/** Select one battle, a bounded series, or an unlimited schedule of finite battles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Recurring",
		meta=(ToolTip="Easy example: Finite Series + 3 allows the faction to attempt at most three separate battles during this response series. Unlimited Schedule keeps trying after cooldowns while every gameplay rule still passes."))
	ETerritoryCounterScheduleMode ScheduleMode =
		ETerritoryCounterScheduleMode::UnlimitedSeries;

	/** Total battles in one finite series, including the first response. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Recurring",
		meta=(EditCondition="ScheduleMode == ETerritoryCounterScheduleMode::FiniteSeries",
			EditConditionHides, ClampMin="1", ClampMax="100",
			DisplayName="Maximum Scheduled Assaults",
			ToolTip="Example: 3 means the first finite battle plus at most two later finite counterattacks."))
	int32 MaximumScheduledAssaults = 3;

	/** Campaign-time delay after a resolved response before another strategic response may be admitted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Recurring",
		meta=(ClampMin="1.0", ToolTip="Time between finite counterattack attempts. Example: 900 means a defeated wave cannot immediately reroll every subsystem update."))
	float RecurringCounterCooldownGameTime = 900.f;

	/** Optional launch window driven by Narrative Pro's authoritative campaign clock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Time Of Day",
		meta=(ToolTip="Use Narrative / Ultra Dynamic Sky Time Window when attacks should begin only at night, dawn, or another story-friendly period."))
	ETerritoryCounterTimePolicy TimePolicy = ETerritoryCounterTimePolicy::AnyTime;

	/** Inclusive window start in Narrative's 0000-2400 time format. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Time Of Day",
		meta=(EditCondition="TimePolicy == ETerritoryCounterTimePolicy::NarrativeTimeWindow",
			EditConditionHides, ClampMin="0.0", ClampMax="2400.0",
			DisplayName="Window Start (Narrative Time)",
			ToolTip="Example: 1800 starts the window at 6 PM."))
	float TimeWindowStart = 1800.f;

	/** Exclusive window end. An end earlier than start wraps across midnight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Time Of Day",
		meta=(EditCondition="TimePolicy == ETerritoryCounterTimePolicy::NarrativeTimeWindow",
			EditConditionHides, ClampMin="0.0", ClampMax="2400.0",
			DisplayName="Window End (Narrative Time)",
			ToolTip="Example: Start 1800 and End 0500 permits night attacks across midnight. Equal values mean all day."))
	float TimeWindowEnd = 500.f;

	/** Optional Narrative tagged line played by the first reserve attacker in each successful wave. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Presentation",
		meta=(ToolTip="Optional Narrative dialogue tag used to alert a nearby player as a reserve wave arrives. Example: Territory.Dialogue.ReservesArriving."))
	FGameplayTag ReserveWaveAlertDialogueTag;

	/** Optional combat line played when the first attacker physically begins holding the Place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Presentation",
		meta=(ToolTip="Optional Narrative tagged dialogue played once by the first attacker that enters the target and registers takeover pressure. Easy example: 'The market belongs to us now!'"))
	FGameplayTag TakeoverStartedDialogueTag;

	/** Optional line for the damaged-car to on-foot boss-fight transition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Presentation",
		meta=(ToolTip="Optional Narrative tagged dialogue played when a chase target abandons a damaged or blocked car and begins the final on-foot fight."))
	FGameplayTag FinalFightDialogueTag;

	/** Enemy level follows the strongest relevant player's Narrative level or mapped power-tier tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(ToolTip="Off by default. When enabled, newly spawned attackers use the strongest nearby player's Narrative level or configured power-tier tag, then add Enemy Level Offset. Narrative difficulty and attack-token counts are unchanged."))
	bool bScaleLevelToRelevantPlayerPower = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(EditCondition="bScaleLevelToRelevantPlayerPower", ClampMin="-20", ClampMax="20", ToolTip="Added after player power is resolved. Use 1 for an enemy one level above the player."))
	int32 EnemyLevelOffset = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(EditCondition="bScaleLevelToRelevantPlayerPower", ClampMin="1", ToolTip="Smallest Narrative NPC level produced by adaptive scaling."))
	int32 MinimumScaledEnemyLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(EditCondition="bScaleLevelToRelevantPlayerPower", ClampMin="1", ToolTip="Largest Narrative NPC level produced by adaptive scaling."))
	int32 MaximumScaledEnemyLevel = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(EditCondition="bScaleLevelToRelevantPlayerPower", TitleProperty="PlayerPowerTag", ToolTip="Optional tag-to-level mappings. Narrative perk Gameplay Effects can grant these replicated tags when skills unlock."))
	TArray<FTerritoryPlayerPowerTier> PlayerPowerTiers;

	/** Optional scalable effect for NPC configurations whose base attributes are constants. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(EditCondition="bScaleLevelToRelevantPlayerPower", ToolTip="Optional Gameplay Effect whose scalable modifiers use the resolved enemy level. Leave empty when the Narrative NPC configuration already has level curves."))
	TSubclassOf<UGameplayEffect> PowerScalingEffect;

	/** Magnitude added for every enemy level above level one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Difficulty",
		meta=(EditCondition="bScaleLevelToRelevantPlayerPower && PowerScalingEffect != nullptr", ClampMin="0.0", ToolTip="Value sent through Narrative Pro's existing SetByCaller.AttackDamage tag for each level above one. Example: 1.5 gives a level 6 enemy +7.5 Attack Damage. Narrative's AttackDamage, AttackRating, Armor, material and friendly-fire rules remain authoritative."))
	float PowerScalingMagnitudePerEnemyLevel = 0.f;
};

/** Complete deterministic calculator input, also retained for debugging and saves. */
USTRUCT(BlueprintType)
struct FTerritoryAssaultEvaluationInput
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") int32 ActiveGuards = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") int32 DesiredGuards = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") int32 MaximumGuards = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") int32 ReserveGuards = 0;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float GuardQuality = 1.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float Fortification = 0.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float NearbyAlliedSupport = 0.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float AttackingMilitaryPower = 0.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float EconomyReadiness = 0.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float SupplyReadiness = 0.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float StrategicValue = 1.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float RecentMomentum = 0.f;
	UPROPERTY(SaveGame, BlueprintReadWrite, Category="Territory|Counter Attack") float FactionInfluence = 0.5f;
};

USTRUCT(BlueprintType)
struct FTerritoryAssaultEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float DistrictDefencePower = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float PowerRatio = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float AttackPriority = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float LaunchProbability = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float EstimatedSuccessProbability = 0.f;
};

/** Durable high-water mark preventing a trimmed assault history from reusing a deterministic decision cycle. */
USTRUCT(BlueprintType)
struct FTerritoryAssaultCycleRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack")
	FGuid TargetTerritoryGUID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag AttackingFaction;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="1"))
	int32 HighestEvaluationCycle = 0;
};

/** Save/RPC-safe per-road car count; Unreal replicated payloads do not support TMap fields. */
USTRUCT(BlueprintType)
struct FTerritoryVehicleDeploymentCount
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle")
	FName ApproachID;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle")
	int32 Count = 0;
};

/** Durable decision/casualty record. Contains no live UObject pointers. */
USTRUCT(BlueprintType)
struct FTerritoryAssaultRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") FGuid AssaultID;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") FGuid TargetTerritoryGUID;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(Categories="Territory")) FGameplayTag TargetTerritory;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(Categories="Narrative.Factions")) FGameplayTag AttackingFaction;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(Categories="Narrative.Factions")) FGameplayTag DefendingFaction;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultLaunchMode LaunchMode = ETerritoryAssaultLaunchMode::StrategicCounterattack;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") ETerritoryStoryPursuitDirection StoryPursuitDirection = ETerritoryStoryPursuitDirection::EnemyChasesPlayer;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bAllowsTerritoryCapture = true;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bUseStrategicDecisionRoll = true;
	/**
	 * True for an explicit Narrative Event request. It may run during a Quest-owned
	 * phase and does not require the automatic strategic staging/capability gates.
	 */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bQuestOverrideAuthorized = false;
	/** Skips grace, time window, warning delay, and player-proximity wait after admission. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bImmediateDeployment = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") FVector StoryFocusLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") TSoftObjectPtr<UNPCDefinition> StoryAttackerDefinitionOverride;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") int32 StoryPlannedForceOverride = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") int32 StoryWaveSizeOverride = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") FName StoryScenarioID;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") float StoryMaximumChaseDistance = 9000.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") float StoryChaseDistanceGraceSeconds = 10.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bStoryAbandonDamagedVehicleForFinalFight = true;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") float StoryVehicleAbandonHealthFraction = 0.35f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bStoryActivateRoadMissionTraffic = true;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") int32 StoryMissionTrafficVehicleCountOverride = -1;
	/** True after the driver has left a disabled/blocked vehicle and the encounter has become a Narrative on-foot final fight. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Story") bool bStoryTargetAbandonedVehicle = false;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultState State = ETerritoryAssaultState::Grace;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultResolution Resolution = ETerritoryAssaultResolution::None;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 EvaluationCycle = 0;
	/** Groups the first response and all of its later scheduled battles. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") FGuid ScheduleSeriesID;
	/** One-based battle number within Schedule Series ID. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="1")) int32 ScheduleOccurrence = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 DecisionSeed = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float DecisionRoll = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double CapturedGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double GraceEndsGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ScheduledGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ActivatedGameTime = 0.0;
	/** Absolute campaign-time deadline for an unattended physical recapture. Zero means inactive. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double RecaptureEndsGameTime = 0.0;
	/** Durable terminal timestamp used by recurring strategic cooldowns. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ResolvedGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 PlannedForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 AliveForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 PendingReserveForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 KilledForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 WithdrawnForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 WaveSize = 1;
	/** Narrative difficulty is snapshotted so loading cannot silently change this assault's car budget. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle") ENarrativeGameplayDifficulty NarrativeDifficultyAtLaunch = ENarrativeGameplayDifficulty::Medium;
	/** Maximum signature/fallback cars across every road approach in this finite assault. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle") int32 MaximumVehicleDeployments = 0;
	/** Durable total and per-road usage prevent save/load from creating extra cars. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle") int32 VehicleDeploymentsUsed = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack|Vehicle") TArray<FTerritoryVehicleDeploymentCount> VehicleDeploymentsByApproach;
	/** Bounded physical deployment failure count; reset after any successful spawn. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 ConsecutiveSpawnFailures = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") TArray<FName> SelectedApproaches;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") FTerritoryAssaultEvaluationInput EvaluationInput;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") FTerritoryAssaultEvaluationResult EvaluationResult;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") bool bNotificationSent = false;

	bool IsTerminal() const
	{
		return State == ETerritoryAssaultState::Succeeded
			|| State == ETerritoryAssaultState::Defeated
			|| State == ETerritoryAssaultState::Cancelled;
	}

	int32 GetAccountedForce() const
	{
		return AliveForce + PendingReserveForce + KilledForce + WithdrawnForce;
	}
};

/**
 * Ephemeral, post-commit notification for one authoritative assault-state transition.
 * The embedded record is a complete value snapshot and contains no live UObject pointers,
 * so the same payload can be delivered reliably to an owning client.
 */
USTRUCT(BlueprintType)
struct FTerritoryCounterAttackStateEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack")
	FTerritoryAssaultRecord Assault;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack")
	ETerritoryAssaultState PreviousState = ETerritoryAssaultState::Grace;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack")
	ETerritoryAssaultState NewState = ETerritoryAssaultState::Grace;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack")
	ETerritoryAssaultResolution Resolution = ETerritoryAssaultResolution::None;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack")
	double EventGameTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryAssaultChanged,
	const FTerritoryAssaultRecord&, Assault);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerritoryAssaultWarning,
	APlayerController*, PlayerController, const FTerritoryAssaultRecord&, Assault);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryCounterHappened,
	const FTerritoryCounterAttackStateEvent&, Event);
