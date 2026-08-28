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
	 * already-launched NPCs finish attacking the District and guards without requiring
	 * the player to remain present. It never bypasses the initial proximity gate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scheduling",
		meta=(ToolTip="Recommended. Player proximity is required once to activate the physical assault; after activation, finite reserve waves continue without the player. Disable to pause reserve deployment until a relevant player returns."))
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
