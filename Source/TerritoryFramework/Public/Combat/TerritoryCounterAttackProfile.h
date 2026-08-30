#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Tales/TerritoryQuestRules.h"
#include "TerritoryCounterAttackProfile.generated.h"

class UQuest;

UENUM(BlueprintType)
enum class ETerritoryCounterQuestPlayerScope : uint8
{
	AnyOnlinePlayer UMETA(DisplayName="Any Online Player"),
	DefendingFactionPlayers UMETA(DisplayName="Defending Faction Players"),
	AttackingFactionPlayers UMETA(DisplayName="Attacking Faction Players")
};

UENUM(BlueprintType)
enum class ETerritoryCounterQuestRuleAction : uint8
{
	BlockWhenMatched UMETA(DisplayName="Block Counter When Matched"),
	RequireMatch UMETA(DisplayName="Require Match Before Counter")
};

/** One reusable Narrative quest gate for automatic strategic counterattacks. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCounterAttackQuestRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Rule",
		meta=(ToolTip="Narrative Quest read from each scoped player's Tales component."))
	TSubclassOf<UQuest> QuestClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Rule")
	ETerritoryQuestStateRequirement QuestState =
		ETerritoryQuestStateRequirement::InProgress;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Rule",
		meta=(ToolTip="Which online players may satisfy this rule. Exact Narrative faction tags are used."))
	ETerritoryCounterQuestPlayerScope PlayerScope =
		ETerritoryCounterQuestPlayerScope::DefendingFactionPlayers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Rule",
		meta=(ToolTip="Easy example: Block + In Progress prevents normal counters during a stealth quest. Require + Succeeded unlocks counters only after a story consequence."))
	ETerritoryCounterQuestRuleAction Action =
		ETerritoryCounterQuestRuleAction::BlockWhenMatched;
};

/** Reusable balance and Narrative NPC configuration for counterattacks. */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryCounterAttackProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Grace duration in Narrative Pro accumulated time-of-day units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="0.0"))
	float GracePeriodGameTime = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="100.0"))
	float ActivationRadius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="100.0"))
	float NotificationRadius = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling")
	bool bNotifyDefendingFactionOnly = true;

	/**
	 * When false (recommended for domination), the warned physical force deploys and
	 * attacks the Territory/guards even when no player is nearby. When true, the old
	 * first-player proximity gate is retained for story encounters that must wait.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling|Recapture",
		meta=(DisplayName="Require Player Proximity To Activate",
			ToolTip="Disable for autonomous counterattacks. Attackers will deploy after the warning and fight Territory guards without waiting for the player."))
	bool bRequirePlayerProximityForActivation = false;

	/** Begin a visible, save-safe handover countdown after attackers clear all defenders and the player is absent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling|Recapture",
		meta=(DisplayName="Allow Unattended Recapture Countdown",
			ToolTip="Recommended. If the defence is cleared while no living defending player is inside the Place or its District, start a countdown instead of capturing instantly."))
	bool bUseUnattendedRecaptureHandover = true;

	/** Campaign-time duration before an unattended cleared Place is handed to the attacking faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling|Recapture",
		meta=(EditCondition="bUseUnattendedRecaptureHandover", ClampMin="1.0",
			DisplayName="Unattended Recapture Time",
			ToolTip="Time available for a defending player to return. Entering the Place or parent District stops the countdown and requires a fight."))
	float UnattendedRecaptureDelayGameTime = 30.f;

	/** Immediately hand over a cleared Place when the defending player dies before respawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling|Recapture",
		meta=(DisplayName="Concede Cleared Place On Defending Player Death",
			ToolTip="If attackers have cleared the defenders and a defending player inside the Place or District dies, ownership immediately transfers before respawn."))
	bool bConcedeWhenDefendingPlayerDies = true;

	/**
	 * Narrative quest gates for automatic strategic counterattacks. Every rule must
	 * pass. Explicit Story Pursuit events use their inherited Narrative Conditions
	 * instead, so a boss chase can deliberately start during a quest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling|Quest Rules",
		meta=(TitleProperty="QuestClass"))
	TArray<FTerritoryCounterAttackQuestRule> QuestRules;

	/**
	 * After the first relevant player has physically activated an assault, deploy its
	 * remaining finite reserve waves even if that player leaves the radius. This lets
	 * already-launched NPCs finish attacking the target Place and guards without requiring
	 * the player to remain present. The separate activation option decides whether an
	 * initial proximity gate exists.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling",
		meta=(ToolTip="Recommended. After activation, finite reserve waves continue without the player. Disable to pause reserve deployment until a relevant player returns."))
	bool bContinueFiniteWavesAfterActivation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling", meta=(ClampMin="1", ClampMax="8"))
	int32 MaximumApproaches = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BaseLaunchProbability = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumLaunchProbability = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaximumLaunchProbability = 0.95f;

	/**
	 * Launch probability when the complete local defence cascade has no active guards.
	 * Diplomacy, territory validity, cooldown/budget admission, and route validation remain
	 * hard gates. A value of 1 makes a genuinely empty District/property front certain to
	 * launch after grace without directly changing ownership.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0", ClampMax="1.0"))
	float UnguardedLaunchProbability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float DefenceDeterrenceWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float AttackerPowerWeight = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float StrategicValueWeight = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float ReadinessWeight = 0.10f;

	/** Contribution of the attacking faction's authored local influence to launch pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Probability", meta=(ClampMin="0.0"))
	float InfluenceWeight = 0.20f;

	/**
	 * Smallest time scale reached at influence 1.0 for grace and contested reserve timing.
	 * Example: 0.25 turns a 300-unit grace period into 75 units at maximum influence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling",
		meta=(ClampMin="0.05", ClampMax="1.0"))
	float MinimumInfluenceTimingScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force")
	TArray<FTerritoryFactionAssaultConfig> FactionForces;

	/** Center-to-center spacing between deterministic formation slots at an approach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment",
		meta=(ClampMin="100.0", UIMin="100.0"))
	float ParticipantSpacing = 220.f;

	/** Bounded formation slots tested for each participant before a wave is deferred. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment",
		meta=(ClampMin="1", ClampMax="16"))
	int32 SpawnPlacementAttemptsPerParticipant = 4;

	/** Reorders only valid authored approaches around a relevant player; it never invents a spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment|Presentation",
		meta=(ToolTip="Recommended. Prefer valid authored approaches near the player, on the same floor, and near the left or right edge of the camera. Navigation and route validation still decide whether spawning is legal."))
	bool bUsePlayerRelativeReserveStaging = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment|Presentation",
		meta=(EditCondition="bUsePlayerRelativeReserveStaging", ClampMin="100.0", ToolTip="Authored approaches closer than this are strongly discouraged to avoid unfair pop-in."))
	float ReserveMinimumPlayerDistance = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment|Presentation",
		meta=(EditCondition="bUsePlayerRelativeReserveStaging", ClampMin="100.0", ToolTip="Ideal distance for an arriving reserve wave. Example: 2600 cm leaves time for the alert line."))
	float ReservePreferredPlayerDistance = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment|Presentation",
		meta=(EditCondition="bUsePlayerRelativeReserveStaging", ClampMin="100.0", ToolTip="Authored approaches farther than this receive a presentation penalty but may still be used when they are the only valid route."))
	float ReserveMaximumPlayerDistance = 5500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment|Presentation",
		meta=(EditCondition="bUsePlayerRelativeReserveStaging", ClampMin="0.0", ClampMax="1.0", ToolTip="Preferred absolute view dot. Around 0.55 places arrivals near the left or right camera edge instead of directly in front or behind."))
	float PreferredCameraEdgeDot = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Deployment|Presentation",
		meta=(EditCondition="bUsePlayerRelativeReserveStaging", ClampMin="0.0", ToolTip="Maximum height difference considered the same floor. Use authored Rooftop, Stair, or Custom approaches plus Nav Links for multi-floor Places."))
	float SameFloorHeightTolerance = 500.f;

	/**
	 * Cap Territory strategic assault slots with Narrative Pro's attack-token count
	 * for the current difficulty. Example with Territory Max=6: Easy=1, Medium=2,
	 * Hard=4, Insane=6 using Narrative's default Combat Settings.
	 * Narrative still grants the real per-defender tactical tokens during combat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Difficulty",
		meta=(ToolTip="Recommended. Effective concurrent participants are min(Territory Max Concurrent Attackers, Narrative attack tokens for the current difficulty)."))
	bool bCapConcurrentAttackersToNarrativeDifficulty = true;

	/** Use complete NavMesh paths instead of flat XY distance when selecting objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Movement",
		meta=(ToolTip="Recommended for multi-floor Places. Attackers choose only complete reachable NavMesh paths when one exists; stairs or Nav Links must connect floors."))
	bool bUseNavigationAwareObjectives = true;

	/** Spread participants across reachable guards, posts, and patrol nodes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Movement",
		meta=(ToolTip="Uses each participant's stable save GUID to distribute a wave across reachable defence objectives instead of making every attacker crowd the same point."))
	bool bDistributeParticipantsAcrossObjectives = true;

	/** Delay before retrying an idle assault move that stopped outside the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Movement",
		meta=(ClampMin="0.25", ClampMax="10.0"))
	float StalledMovementRetryInterval = 1.5f;

	/** A participant withdraws after this many consecutive failed movement restarts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force|Movement",
		meta=(ClampMin="1", ClampMax="100"))
	int32 MaxStalledMovementRetries = 8;

	/** Cancel an activated assault after this many consecutive zero-spawn wave attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Force", meta=(ClampMin="1", ClampMax="100"))
	int32 MaxConsecutiveSpawnFailures = 5;

	const FTerritoryFactionAssaultConfig* FindFactionForce(const FGameplayTag& Faction) const;

	/** Pure all-player reduction used by runtime and native regression tests. */
	static bool DoesQuestRulePass(ETerritoryCounterQuestRuleAction Action,
		bool bAnyScopedPlayerMatches)
	{
		return Action == ETerritoryCounterQuestRuleAction::BlockWhenMatched
			? !bAnyScopedPlayerMatches : bAnyScopedPlayerMatches;
	}
};
