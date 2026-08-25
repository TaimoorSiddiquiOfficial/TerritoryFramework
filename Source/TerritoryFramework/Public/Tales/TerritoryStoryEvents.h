#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryStoryEvents.generated.h"

/**
 * Schedules one finite physical enemy assault through the existing counterattack authority.
 * The inherited Narrative Event -> Conditions array is evaluated before scheduling.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Wave of Enemies (Schedule Finite Territory Assault)"))
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
		meta=(ToolTip="Strategic Counterattack requires the force's normal District staging rule. Story Pursuit / Boss Chase is a deliberate Tales exception and also requires permission in the selected force profile."))
	ETerritoryAssaultLaunchMode LaunchMode =
		ETerritoryAssaultLaunchMode::StrategicCounterattack;

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
