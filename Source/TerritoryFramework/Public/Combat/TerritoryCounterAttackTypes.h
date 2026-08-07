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
	Road,
	Gate,
	Water,
	Rooftop,
	Tunnel,
	Air,
	Custom
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
	SpawnFailed
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack")
	ETerritoryAttackApproachType Type = ETerritoryAttackApproachType::Road;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack")
	FTransform RelativeSpawnTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack", meta=(ClampMin="1"))
	int32 MaxWaveSize = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Counter Attack")
	bool bEnabled = true;
};

/** Per-attacking-faction physical force configuration. */
USTRUCT(BlueprintType)
struct FTerritoryFactionAssaultConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack")
	TObjectPtr<UNPCDefinition> AttackerDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack")
	TObjectPtr<UNPCActivityConfiguration> ActivityConfigurationOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack")
	TArray<TSoftObjectPtr<UTriggerSet>> TriggerSetOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="0.0"))
	float MilitaryPower = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="0.0", ClampMax="1.0"))
	float EconomyReadiness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SupplyReadiness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float RecentMomentum = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="1"))
	int32 PlannedForce = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Counter Attack", meta=(ClampMin="1"))
	int32 WaveSize = 3;
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
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultState State = ETerritoryAssaultState::Grace;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") ETerritoryAssaultResolution Resolution = ETerritoryAssaultResolution::None;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 EvaluationCycle = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") int32 DecisionSeed = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") float DecisionRoll = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double CapturedGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double GraceEndsGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ScheduledGameTime = 0.0;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category="Territory|Counter Attack") double ActivatedGameTime = 0.0;
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
