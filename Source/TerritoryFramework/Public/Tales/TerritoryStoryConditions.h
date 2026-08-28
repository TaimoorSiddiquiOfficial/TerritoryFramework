#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Core/TerritoryTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/TerritoryGarrisonCondition.h"
#include "Tales/TerritoryQuestRules.h"
#include "TerritoryStoryConditions.generated.h"

class UNarrativeItem;
class UQuest;

/** Uses the explicit Narrative event's Tales component; no second quest state is stored. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Narrative Quest State Condition"))
class TERRITORYFRAMEWORK_API UTerritoryQuestStateCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryQuestStateCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Narrative Quest to inspect. Easy example: select Stealth Investigation, then choose In Progress to allow an event only during that quest."))
	TSubclassOf<UQuest> QuestClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Required Narrative quest state. Use the inherited Not checkbox to invert it; for example Not + In Progress means do not run during this quest."))
	ETerritoryQuestStateRequirement RequiredState =
		ETerritoryQuestStateRequirement::InProgress;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Validates the explicit pawn/controller/Tales context before a Narrative event runs.
 * Use this on events such as Give XP that require a live player Ability System.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Event Context Condition"))
class TERRITORYFRAMEWORK_API UTerritoryEventContextCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryEventContextCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Require the event's explicit Target pawn to be valid. Example: a world recovery with no player will not run a player reward."))
	bool bRequireTargetPawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="bRequireTargetPawn", ToolTip="Require Target to be controlled by a real player. Example: an AI recapturing a Place will not receive player XP."))
	bool bRequirePlayerControlledTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="bRequireTargetPawn", ToolTip="Require Target to expose a valid Gameplay Ability System Component. Enable this before Narrative events such as Give XP or Apply Gameplay Effect."))
	bool bRequireAbilitySystemComponent = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Require the explicit Player Controller passed to the event. Enable this for player-only UI or controller actions."))
	bool bRequirePlayerController = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Require the explicit Tales Component passed to the event. Enable this for quest or dialogue actions that need Narrative story state."))
	bool bRequireTalesComponent = false;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Passes only while the containing Territory is changing to a different owner.
 * Add it to Claimed-state rewards or waves that must not run when an abandoned
 * contest simply returns to the same owner. Use Narrative's inherited Not option
 * when an event should run only for a state-only recovery.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Ownership Changed During Transition"))
class TERRITORYFRAMEWORK_API UTerritoryOwnershipTransitionCondition
	: public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryOwnershipTransitionCondition();

protected:
	virtual bool CheckCondition_Implementation(APawn* Target,
		APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

UENUM(BlueprintType)
enum class ETerritoryFloatComparison : uint8
{
	NearlyEqual UMETA(DisplayName="Nearly Equal To", ToolTip="Passes when the values differ by no more than the tolerance."),
	AtLeast UMETA(DisplayName="At Least"),
	AtMost UMETA(DisplayName="At Most"),
	GreaterThan UMETA(DisplayName="Greater Than"),
	LessThan UMETA(DisplayName="Less Than")
};

/** Checks the authoritative enum state of one loaded Territory. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory State Condition"))
class TERRITORYFRAMEWORK_API UTerritoryStateCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryStateCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Territory to inspect. Example: Territory.HavenReach.MarketSquare."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="State that must currently be true. Example: Contested opens battle dialogue."))
	ETerritoryState RequiredState = ETerritoryState::Claimed;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks Territory capture/control progress without changing capture authority. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Control Progress Condition"))
class TERRITORYFRAMEWORK_API UTerritoryControlProgressCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryControlProgressCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Territory whose real capture progress is inspected."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryFloatComparison Comparison = ETerritoryFloatComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent",
			ToolTip="Progress percentage used by the comparison. Example: 75 means capture pressure reached seventy-five percent."))
	float ProgressPercent = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", ClampMax="10.0", Units="Percent",
			EditCondition="Comparison == ETerritoryFloatComparison::NearlyEqual", EditConditionHides))
	float EqualityTolerancePercent = 0.5f;

	static bool CompareValues(float ActualValue, ETerritoryFloatComparison Operation,
		float RequiredValue, float Tolerance);

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks the campaign reputation stored by the Territory diplomacy authority. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Faction Reputation Condition"))
class TERRITORYFRAMEWORK_API UTerritoryReputationCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryReputationCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions", ToolTip="Faction whose saved reputation is inspected."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Reputation used by the comparison. Example: At Least 50 unlocks trusted-faction dialogue."))
	int32 Value = 0;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks the same secure-District holding used by normal strategic counterattacks. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Faction District Holdings Condition"))
class TERRITORYFRAMEWORK_API UTerritoryFactionDistrictHoldingCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryFactionDistrictHoldingCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions",
			ToolTip="Faction whose secure Districts are counted. Example: Narrative.Factions.Bandits."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0",
			ToolTip="Number of loaded Districts securely held in Claimed or story-Locked state. Example: At Least 1 means the faction still has a military base for normal counters. Contested and Unclaimed Districts do not count; 0 means a story pursuit needs an explicit exception."))
	int32 DistrictCount = 1;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

UENUM(BlueprintType)
enum class ETerritoryAssaultConditionQuery : uint8
{
	AnyPendingOrActive UMETA(DisplayName="Has Warning, Waiting, or Active Assault"),
	PhysicalAssaultActive UMETA(DisplayName="Has Physically Active Assault"),
	LatestState UMETA(DisplayName="Latest Assault State Is"),
	LatestResolution UMETA(DisplayName="Latest Assault Resolution Is"),
	PlannedAttackers UMETA(DisplayName="Latest Planned Attackers"),
	LivingAttackers UMETA(DisplayName="Latest Living Attackers"),
	PendingReserveAttackers UMETA(DisplayName="Latest Pending Reserve Attackers"),
	KilledAttackers UMETA(DisplayName="Latest Killed Attackers"),
	WithdrawnAttackers UMETA(DisplayName="Latest Withdrawn Attackers"),
	RemainingAttackers UMETA(DisplayName="Latest Living Plus Reserve Attackers")
};

/** Reads durable counterattack records; it never schedules or resolves an assault. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Enemy Wave / Assault Condition"))
class TERRITORYFRAMEWORK_API UTerritoryAssaultCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryAssaultCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Territory whose finite counterattack record is inspected."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryAssaultConditionQuery Query = ETerritoryAssaultConditionQuery::AnyPendingOrActive;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query == ETerritoryAssaultConditionQuery::LatestState", EditConditionHides))
	ETerritoryAssaultState RequiredState = ETerritoryAssaultState::Active;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query == ETerritoryAssaultConditionQuery::LatestResolution", EditConditionHides))
	ETerritoryAssaultResolution RequiredResolution = ETerritoryAssaultResolution::AllAttackersRemoved;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query >= ETerritoryAssaultConditionQuery::PlannedAttackers", EditConditionHides))
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0", EditCondition="Query >= ETerritoryAssaultConditionQuery::PlannedAttackers", EditConditionHides,
			ToolTip="Example: Killed Attackers At Least 3 can unlock a reinforcement objective."))
	int32 Value = 1;

	static const FTerritoryAssaultRecord* SelectLatestRecord(
		const TArray<FTerritoryAssaultRecord>& Records);

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks whether the explicit Narrative target pawn is inside a Territory. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Narrative Target Is In Territory Condition"))
class TERRITORYFRAMEWORK_API UTerritoryPresenceCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryPresenceCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Place the target pawn must be inside."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="When enabled, a pawn inside a child Property also counts as being inside its parent District or City."))
	bool bIncludeChildTerritories = true;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks the last durable production outcome for a Property, including while it is streamed out. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Production Status Condition"))
class TERRITORYFRAMEWORK_API UTerritoryProductionStatusCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryProductionStatusCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Producing Property to inspect."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Optional exact production rule. Leave empty to use the Property's overall last status."))
	FGameplayTag RuleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Example: Missing Input can start a supply quest."))
	ETerritoryProductionStatus RequiredStatus = ETerritoryProductionStatus::Produced;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks one Narrative inventory resource amount from the Territory read snapshot. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Faction Resource Condition"))
class TERRITORYFRAMEWORK_API UTerritoryResourceCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryResourceCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions", ToolTip="Faction whose registered Narrative resource inventory is inspected."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Exact Narrative item class used as the strategic resource."))
	TSubclassOf<UNarrativeItem> ResourceItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0", ToolTip="Example: At Least 10 medicine allows a hospital relief event."))
	int32 Quantity = 1;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
