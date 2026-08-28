#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryCounterAttackTypes.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;

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
	Cancelled
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
		ToolTip="The attacking faction no longer owns a loaded District in Claimed or story-Locked state, so an undeployed strategic counterattack cannot continue.")
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

/** Strategic territory holding required before this faction may launch a normal counterattack. */
UENUM(BlueprintType)
enum class ETerritoryAssaultStagingRequirement : uint8
{
	None UMETA(DisplayName="No Territory Holding Required",
		ToolTip="Use only for forces that should not depend on domination holdings."),
	OwnsSecureDistrict UMETA(DisplayName="Owns At Least One Secure District",
		ToolTip="The faction must own at least one loaded District in Claimed or story-Locked state. Example: a Locked Bandit base still supports normal counters, but a Contested or Unclaimed District does not.")
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
		meta=(ToolTip="Meaning of this route for UI and project rules. It does not replace navigation validation."))
	ETerritoryAttackApproachType Type = ETerritoryAttackApproachType::Road;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ToolTip="Spawn point relative to the Territory actor. Example: 2,000 cm west of the District center."))
	FTransform RelativeSpawnTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ClampMin="1", ToolTip="Largest part of one wave allowed to use this approach. This never creates infinite reserves."))
	int32 MaxWaveSize = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack",
		meta=(ToolTip="Disabled approaches are never selected or used for physical spawning."))
	bool bEnabled = true;
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
		meta=(ToolTip="Normal strategic admission rule. Recommended: require one loaded District in Claimed or story-Locked state so a defeated faction with no secure District cannot keep launching counters."))
	ETerritoryAssaultStagingRequirement StagingRequirement =
		ETerritoryAssaultStagingRequirement::OwnsSecureDistrict;

	/** Dual opt-in for a Tales story pursuit that intentionally has no staging District. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Story",
		meta=(ToolTip="Allows this force to be used by a Story Pursuit / Boss Chase event without owning a District. The event must also explicitly select Story Pursuit. Normal counters never use this exception."))
	bool bAllowStoryPursuitWithoutStagingDistrict = false;

	/** Enables repeated strategic responses after a previous response has ended. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Recurring",
		meta=(ToolTip="When enabled, this faction may counter the same still-hostile owner again after the cooldown, but only while its staging and all other admission rules still pass."))
	bool bEnableRecurringStrategicCounters = true;

	/** Campaign-time delay after a resolved response before another strategic response may be admitted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack|Recurring",
		meta=(ClampMin="1.0", ToolTip="Time between finite counterattack attempts. Example: 900 means a defeated wave cannot immediately reroll every subsystem update."))
	float RecurringCounterCooldownGameTime = 900.f;
};

/** Complete deterministic calculator input, also retained for debugging and saves. */
USTRUCT(BlueprintType)
struct FTerritoryAssaultEvaluationInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") int32 ActiveGuards = 0;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") int32 DesiredGuards = 0;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") int32 MaximumGuards = 0;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") int32 ReserveGuards = 0;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float GuardQuality = 1.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float Fortification = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float NearbyAlliedSupport = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float AttackingMilitaryPower = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float EconomyReadiness = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float SupplyReadiness = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float StrategicValue = 1.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float RecentMomentum = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Territory|Counter Attack") float FactionInfluence = 0.5f;
};

USTRUCT(BlueprintType)
struct FTerritoryAssaultEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack") float DistrictDefencePower = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack") float PowerRatio = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack") float AttackPriority = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack") float LaunchProbability = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|Counter Attack") float EstimatedSuccessProbability = 0.f;
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
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultState State = ETerritoryAssaultState::Grace;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultResolution Resolution = ETerritoryAssaultResolution::None;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 EvaluationCycle = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 DecisionSeed = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float DecisionRoll = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double CapturedGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double GraceEndsGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ScheduledGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ActivatedGameTime = 0.0;
	/** Durable terminal timestamp used by recurring strategic cooldowns. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ResolvedGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 PlannedForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 AliveForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 PendingReserveForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 KilledForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 WithdrawnForce = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 WaveSize = 1;
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
