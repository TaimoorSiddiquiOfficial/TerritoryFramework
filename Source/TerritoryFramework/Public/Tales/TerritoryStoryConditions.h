#pragma once

#include "CoreMinimal.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Core/TerritoryTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryGarrisonCondition.h"
#include "Tales/TerritoryQuestRules.h"
#include "TerritoryStoryConditions.generated.h"

class UNarrativeItem;
class UQuest;

UENUM(BlueprintType)
enum class ETerritoryWaitTimeSource : uint8
{
	NarrativeCampaignElapsed UMETA(DisplayName="Narrative Campaign Elapsed Time (Saved)"),
	CurrentWorldElapsed UMETA(DisplayName="Current World Elapsed Time (Not Saved)")
};

/** Uses the explicit Narrative event's Tales component; no second quest state is stored. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Narrative Quest State Condition",
		ToolTip="Read the quest on the Tales component supplied by Narrative. This does not begin a quest. Example: Not Started shows an offer; In Progress shows a reminder."))
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
 * Deterministic non-latent time gate for State Configs, event Conditions, and dialogue.
 * It does not sleep or hold an Event: the calling rule passes on its next evaluation
 * after the selected clock reaches Wait Time.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Wait Time Condition",
		ToolTip="Check an elapsed clock. This does not pause a dialogue or wait after entering a node. Example: 600 campaign seconds means ten minutes since the campaign began. Use a quest condition task to poll until it passes."))
class TERRITORYFRAMEWORK_API UTerritoryWaitTimeCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryWaitTimeCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Narrative Campaign time is saved and recommended for story rules. Current World time restarts when the level/world starts."))
	ETerritoryWaitTimeSource TimeSource =
		ETerritoryWaitTimeSource::NarrativeCampaignElapsed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", Units="s",
			ToolTip="The condition passes when the selected elapsed clock reaches this value. Easy example: 600 means wait until ten minutes of campaign time have elapsed."))
	float WaitTimeSeconds = 0.f;

	static bool HasWaitFinished(double CurrentTimeSeconds,
		float RequiredWaitSeconds);

protected:
	virtual bool CheckCondition_Implementation(APawn* Target,
		APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Validates the explicit pawn/controller/Tales context before a Narrative event runs.
 * Use this on events such as Give XP that require a live player Ability System.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Event Context Condition",
		ToolTip="Require the exact pawn, controller, Tales component or Ability System needed by the next event. Each checkbox is independent. This never selects the first player in the world."))
class TERRITORYFRAMEWORK_API UTerritoryEventContextCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryEventContextCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Require the event's explicit Target pawn to be valid. Example: a world recovery with no player will not run a player reward."))
	bool bRequireTargetPawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Require Target to be controlled by a real player. Example: an AI recapturing a Place will not receive player XP."))
	bool bRequirePlayerControlledTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Require Target to expose a valid Gameplay Ability System Component. Enable this before Narrative events such as Give XP or Apply Gameplay Effect."))
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
	meta=(DisplayName="Territory Ownership Changed During Transition",
		ToolTip="Pass only during a Territory state callback that changes the owner. Use on capture rewards to avoid paying for same-owner recovery. Ordinary dialogue outside that callback fails."))
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

UENUM(BlueprintType)
enum class ETerritoryStateConditionQuery : uint8
{
	PoliticalState UMETA(DisplayName="Political State", ToolTip="Check Claimed, Contested, or Unclaimed. Existing Locked selections check the separate lock field for compatibility."),
	Availability UMETA(DisplayName="Local Lock State", ToolTip="Check whether this place itself is Locked or Unlocked. A parent may still block gameplay."),
	Known UMETA(DisplayName="Territory State Is Known", ToolTip="Check that the territory can be read. Use this before an inverted condition so missing data cannot look like success."),
	Loaded UMETA(DisplayName="Territory Actor Is Loaded", ToolTip="Check that the real territory actor is loaded and registered in this world.")
};

/** Reads political ownership and local story locks without changing either. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory State Condition",
		ToolTip="Check political state, local lock state, or whether territory data is known. Claimed and Locked can both be true. Old Locked selections now read the real lock field. Use Known before an inverted check when missing data must fail."))
class TERRITORYFRAMEWORK_API UTerritoryStateCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryStateCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Choose the fact to check. A place can be Claimed by Bandits and Locked at the same time. Use a Situation condition for gameplay availability through the whole parent hierarchy."))
	ETerritoryStateConditionQuery Query = ETerritoryStateConditionQuery::PoliticalState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Territory to inspect. Example: Territory.HavenReach.MarketSquare."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query == ETerritoryStateConditionQuery::PoliticalState", EditConditionHides,
			ToolTip="Political state to match. Claimed does not mean unlocked. Locked (Legacy) is supported and reads the local lock field; prefer Local Lock State for new conditions."))
	ETerritoryState RequiredState = ETerritoryState::Claimed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query == ETerritoryStateConditionQuery::Availability", EditConditionHides,
			ToolTip="Locked blocks this place while keeping its owner. Unlocked checks only this place, not its parent District or City."))
	ETerritoryAvailability RequiredAvailability = ETerritoryAvailability::Unlocked;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query != ETerritoryStateConditionQuery::Loaded", EditConditionHides,
			ToolTip="If the actor is streamed out, read its saved or replicated campaign directory entry. This does not load the actor. Missing entries fail. Leave off when the scene requires the actor to be present."))
	bool bAllowUnloadedTerritory = false;

	/** Compares a known live or campaign snapshot. Missing data must be rejected by the caller. */
	bool MatchesState(ETerritoryState State, ETerritoryAvailability Availability) const;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks Territory capture/control progress without changing capture authority. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Control Progress Condition",
		ToolTip="Compare real control progress from zero to one hundred percent. Optional faction means the exact faction applying pressure. Full control may also mean a stable Claimed place; add a Contested state condition for ongoing capture. Requires a loaded territory."))
class TERRITORYFRAMEWORK_API UTerritoryControlProgressCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryControlProgressCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Territory whose real capture progress is inspected."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(Categories="Narrative.Factions",
		ToolTip="Optional exact faction applying capture pressure. Empty checks the Place's progress without a faction filter. A different or missing contesting faction fails this condition."))
	FGameplayTag ContestingFaction;

	/** Numeric comparison used between the live value and the authored threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryFloatComparison Comparison = ETerritoryFloatComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent",
			ToolTip="Progress percentage used by the comparison. Example: 75 means capture pressure reached seventy-five percent."))
	float ProgressPercent = 75.f;

	/** Tolerance in percentage points when comparing two percentages for equality. */
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
	meta=(DisplayName="Territory Faction Reputation Condition",
		ToolTip="Compare saved campaign reputation for one faction. Explicit uses the selected tag; dynamic sources follow the current Narrative participant. This does not compare treaties or NPC attitude."))
class TERRITORYFRAMEWORK_API UTerritoryReputationCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryReputationCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Explicit uses the fixed Faction below. Narrative Target or Controller Pawn follows the current real faction, including a story faction change. Missing character context fails."))
	ETerritoryCaptureFactionSource FactionSource = ETerritoryCaptureFactionSource::ExplicitFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions", ToolTip="Faction whose saved reputation is inspected."))
	FGameplayTag Faction;

	/** Numeric comparison used between the live value and the authored threshold. */
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

/**
 * Counts the faction's current unlocked, stable Claimed Districts. Attach this
 * to any Narrative event, including Set Territory Diplomacy, to branch story
 * policy from territorial power without hard-coding the Heroes faction.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Faction Claimed District Count Condition",
		ToolTip="Count complete, unlocked Districts held by one faction using the campaign directory. Partial or Contested Districts do not count. Use a Situation condition to count individual Places instead."))
class TERRITORYFRAMEWORK_API UTerritoryFactionDistrictHoldingCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryFactionDistrictHoldingCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Where the faction comes from. Explicit keeps a fixed faction. Narrative Target follows the character who caused the quest/event. Controller Pawn follows the current possessed player, so a story faction change is respected."))
	ETerritoryCaptureFactionSource FactionSource =
		ETerritoryCaptureFactionSource::ExplicitFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions",
			EditCondition="FactionSource == ETerritoryCaptureFactionSource::ExplicitFaction",
			EditConditionHides,
			ToolTip="Fixed faction whose Claimed Districts are counted. Example: Narrative.Factions.Bandits."))
	FGameplayTag Faction;

	/** Numeric comparison used between the live value and the authored threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0",
			DisplayName="Claimed District Count",
			ToolTip="Number of unlocked Districts fully Claimed through their authored Places. Example: At Least 1 means this faction controls one complete District. At Least 2 can trigger a diplomacy reaction. World Partition Districts count through the replicated strategic directory. Locked, Contested, partial, and Unclaimed Districts do not count."))
	int32 DistrictCount = 1;

	/** Resolve the exact faction used by this condition for Blueprint debugging. */
	UFUNCTION(BlueprintPure, Category="Territory|Conditions",
		meta=(DisplayName="Resolve Claimed District Count Faction"))
	FGameplayTag ResolveFaction(APawn* NarrativeTarget,
		APlayerController* Controller) const;

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
	RemainingAttackers UMETA(DisplayName="Latest Living Plus Reserve Attackers"),
	AnyRecorded UMETA(DisplayName="Has A Matching Assault Record")
};

/** Reads durable counterattack records; it never schedules or resolves an assault. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Enemy Wave / Assault Condition",
		ToolTip="Read finite enemy-wave records. Filter by territory, optional exact attacking faction and optional story ID. Latest queries prefer an unfinished matching assault over old results. Missing records fail, including a zero-count check."))
class TERRITORYFRAMEWORK_API UTerritoryAssaultCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryAssaultCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Territory whose finite counterattack record is inspected."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(Categories="Narrative.Factions",
		ToolTip="Optional exact faction that sends the force. Empty checks forces from every faction. This is not the speaking NPC or requesting player."))
	FGameplayTag AttackingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(
		ToolTip="Optional exact story encounter ID. Use the same ID on the enemy wave event so an unrelated victory cannot unlock this handover."))
	FName ScenarioID;

	/** Select the live Territory fact this condition compares; related fields become relevant for that query. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryAssaultConditionQuery Query = ETerritoryAssaultConditionQuery::AnyPendingOrActive;

	/** Assault lifecycle state required by the selected counterattack query. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query == ETerritoryAssaultConditionQuery::LatestState", EditConditionHides))
	ETerritoryAssaultState RequiredState = ETerritoryAssaultState::Active;

	/** Terminal assault outcome required by the selected counterattack query. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query == ETerritoryAssaultConditionQuery::LatestResolution", EditConditionHides))
	ETerritoryAssaultResolution RequiredResolution = ETerritoryAssaultResolution::AllAttackersRemoved;

	/** Numeric comparison used between the live value and the authored threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(EditCondition="Query >= ETerritoryAssaultConditionQuery::PlannedAttackers && Query <= ETerritoryAssaultConditionQuery::RemainingAttackers", EditConditionHides))
	ETerritoryIntegerComparison Comparison = ETerritoryIntegerComparison::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0", EditCondition="Query >= ETerritoryAssaultConditionQuery::PlannedAttackers && Query <= ETerritoryAssaultConditionQuery::RemainingAttackers", EditConditionHides,
			ToolTip="Example: Killed Attackers At Least 3 can unlock a reinforcement objective."))
	int32 Value = 1;

	static const FTerritoryAssaultRecord* SelectLatestRecord(
		const TArray<FTerritoryAssaultRecord>& Records);
	bool MatchesRecord(const FTerritoryAssaultRecord& Record) const;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks whether the explicit Narrative target pawn is inside a Territory. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Narrative Target Is In Territory Condition",
		ToolTip="Check the Narrative target position, or the explicit controller pawn if Target is empty. Child inclusion follows authored parent links. Only loaded territory bounds can be checked."))
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
	meta=(DisplayName="Territory Production Status Condition",
		ToolTip="Read the last recorded production status for a place or one recipe. This does not run production. Missing production records fail; clients use the replicated economy view."))
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
	meta=(DisplayName="Territory Faction Resource Condition",
		ToolTip="Compare a faction storage account for one Narrative item. This is faction storage, not the speaking NPC inventory. A missing storage account fails even when checking for zero items."))
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

	/** Numeric comparison used between the live value and the authored threshold. */
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
