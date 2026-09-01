#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryStoryEvents.generated.h"

class ANarrativePlayerState;

/**
 * Changes the exact Narrative quest player's saved faction membership.
 * This is political identity, not a disguise and not Territory ownership.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Narrative Player Factions"))
class TERRITORYFRAMEWORK_API UTerritorySetNarrativePlayerFactionsEvent
	: public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetNarrativePlayerFactionsEvent(
		const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Faction memberships applied to the exact Narrative target player. Easy example: replace Police with Heroes after the Regime betrays the player."))
	FGameplayTagContainer NewFactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="True replaces every existing player faction in one Narrative Player State update. False adds the selected memberships and preserves existing ones."))
	bool bReplaceExistingFactions = true;

	/** Native helper used by execution and regression tests. */
	bool ApplyToPlayerState(ANarrativePlayerState* PlayerState) const;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target,
		APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

UENUM(BlueprintType)
enum class ETerritoryHierarchyStoryOperation : uint8
{
	ClaimForFaction UMETA(DisplayName="Claim Entire Hierarchy For Faction"),
	ClearToUnclaimed UMETA(DisplayName="Clear Entire Hierarchy To Unclaimed"),
	Lock UMETA(DisplayName="Lock Entire Hierarchy"),
	Unlock UMETA(DisplayName="Unlock Entire Hierarchy")
};

/**
 * Applies one story decision to a loaded City, District, or Place hierarchy.
 * Ownership changes are committed only on independent leaf Places; the existing
 * unanimity reducer remains the sole authority that derives District and City owner.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Apply Territory Hierarchy Story Override"))
class TERRITORYFRAMEWORK_API UTerritoryHierarchyStoryOverrideEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryHierarchyStoryOverrideEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Root City, District, or Place. Easy example: choose Haven Reach to change every currently loaded District and Place below it after a betrayal quest."))
	FGameplayTag RootTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event")
	ETerritoryHierarchyStoryOperation Operation =
		ETerritoryHierarchyStoryOperation::ClaimForFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			EditCondition="Operation == ETerritoryHierarchyStoryOperation::ClaimForFaction",
			EditConditionHides,
			ToolTip="Exact Narrative faction that receives every independent Place. District and City ownership is then derived from their children."))
	FGameplayTag ClaimingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Recommended for a deliberate story override. Bypasses Place lock, diplomacy, and state conditions, but never bypasses server authority or the hierarchy reducer."))
	bool bForceStoryOverride = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(EditCondition="Operation == ETerritoryHierarchyStoryOperation::Lock",
			EditConditionHides,
			ToolTip="Reason shown by locked Territory UI. Example: Complete The Governor's Trial."))
	FText LockReason;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Schedules one finite physical enemy assault through the existing counterattack authority.
 * The inherited Narrative Event -> Conditions array is evaluated before scheduling. A
 * Strategic Counterattack may later repeat only when the selected force profile permits
 * a finite/unlimited schedule and its cooldown, Narrative time, quest, diplomacy, staging,
 * route, and budget rules still pass. Story Pursuit never repeats automatically.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Wave of Enemies (Schedule Territory Assault)"))
class TERRITORYFRAMEWORK_API UTerritoryScheduleEnemyWaveEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryScheduleEnemyWaveEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory", ToolTip="Claimed Territory the enemy will physically attack."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="When enabled, diplomacy, force power, supply, budgets, and deterministic priority choose the best configured attacker."))
	bool bChooseBestEligibleAttacker = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Exact attacker, or preferred tie-break when Best Eligible Attacker is enabled. Example: Narrative.Factions.Bandits."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Strategic Counterattack starts one finite battle and may later follow the force profile's One Assault, Finite Series, or Unlimited Schedule policy. Story Pursuit / Boss Chase is a deliberate Tales exception, requires force-profile permission, and never repeats automatically."))
	ETerritoryAssaultLaunchMode LaunchMode =
		ETerritoryAssaultLaunchMode::StrategicCounterattack;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Easy Tales event for a one-shot boss pursuit. Narrative owns mounting, driving, arrival,
 * dismount and combat. Territory supplies the finite scenario, route direction, optional
 * capture policy and outcome. Use it for hunters arriving by car or a capo escaping by car.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Start Territory Boss Chase"))
class TERRITORYFRAMEWORK_API UTerritoryStartBossChaseEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryStartBossChaseEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory",
			ToolTip="Claimed Place where the boss force will pursue the player. Its Territory Definition supplies the vehicle and foot approaches."))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Exact pursuing faction. Configure that faction's Attacker Definition as the boss or boss-force NPC Definition in the target's counterattack profile."))
	FGameplayTag PursuingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ShowOnlyInnerProperties,
			ToolTip="Reusable story options. Enemy Chases Player drives into the Place and hands off to combat. Player Chases Enemy reverses the authored vehicle route and records Target Escaped if the capo reaches the exit."))
	FTerritoryStoryPursuitOptions PursuitOptions;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Cancels matching durable assaults; active physical attackers are optional and never become a hidden ownership roll. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Cancel Territory Enemy Waves"))
class TERRITORYFRAMEWORK_API UTerritoryCancelEnemyWavesEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryCancelEnemyWavesEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions", ToolTip="Optional attacker filter. Leave empty to cancel waves from every faction."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="False cancels only grace, warning, and waiting records. True also retires living attackers from an active assault."))
	bool bIncludePhysicallyActiveAssaults = false;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Uses the existing atomic staffing/currency mutation; it does not grant free guards. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Territory Guard Assignment Target"))
class TERRITORYFRAMEWORK_API UTerritorySetGarrisonTargetEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetGarrisonTargetEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(Categories="Territory"))
	FGameplayTag TargetTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ClampMin="0", ToolTip="Exact desired staffing target. Increasing it charges the explicit Narrative target's inventory."))
	int32 DesiredGuards = 1;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Purchases exactly one existing Property upgrade through the normal Narrative currency path. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Purchase One Territory Property Upgrade"))
class TERRITORYFRAMEWORK_API UTerritoryUpgradePropertyEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryUpgradePropertyEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory", ToolTip="Loaded Property to upgrade by exactly one level."))
	FGameplayTag TargetProperty;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Executes one atomic input/output recipe against the explicit target's Narrative inventory. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Execute Territory Resource Recipe"))
class TERRITORYFRAMEWORK_API UTerritoryExecuteResourceRecipeEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryExecuteResourceRecipeEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions", ToolTip="Faction that owns the explicit Narrative inventory account."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Territory", ToolTip="Semantic source recorded in the production result. It does not bypass ownership or capture."))
	FGameplayTag SourceTerritory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Atomic Narrative inventory inputs and outputs. Example: consume medicine supplies and produce one relief package."))
	FTerritoryProductionRule Recipe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event", meta=(ClampMin="0"))
	int32 UpgradeLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ClampMin="1", ToolTip="Finite number of recipe batches executed in one validated transaction."))
	int32 BatchCount = 1;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
