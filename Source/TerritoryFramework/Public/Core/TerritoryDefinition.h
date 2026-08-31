#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryTypes.h"
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
class UTerritoryCounterAttackProfile;
class UTerritoryDistrictManagementWidget;
class UTerritoryGuardPostDefinition;
class UTerritoryPatrolGoal;
class UTerritoryProductionProfile;
class UTerritoryStealthProfile;
class UTriggerSet;

/** A patrol instruction stored relative to its guard-post actor. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGuardPatrolTemplateNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol",
		meta=(ClampMin="0.0", Units="s"))
	float WaitTime = 2.f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol|Crowd Avoidance",
		meta=(EditCondition="bEnablePatrolCrowdAvoidance", ClampMin="100.0", ClampMax="2000.0", Units="cm"))
	float PatrolAvoidanceConsiderationRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Patrol|Crowd Avoidance",
		meta=(EditCondition="bEnablePatrolCrowdAvoidance", ClampMin="0.0", ClampMax="1.0"))
	float PatrolAvoidanceWeight = 0.5f;

	/** Adds a small score preference for the nearest hostile player during a valid Territory war. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat")
	bool bPrioritizeClosestHostilePlayer = true;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Guard",
		meta=(ClampMin="0", UIMin="0", UIMax="100"))
	int32 Priority = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0", UIMin="0", UIMax="10"))
	int32 ReserveSlots = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve")
	bool bAutoSpawnReserves = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0.1", Units="s"))
	float ReserveSpawnDelay = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0.1", Units="s"))
	float ReserveSpawnRetryInterval = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="100.0", Units="cm"))
	float ReserveSpawnRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0.0", Units="cm"))
	float ReserveMinimumPlayerDistance = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="1", ClampMax="64"))
	int32 ReserveSpawnCandidateCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="0", ClampMax="20"))
	int32 ReserveCameraAvoidanceRetryLimit = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve",
		meta=(ClampMin="1", ClampMax="100"))
	int32 ReserveTotalRetryLimit = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post|Reserve")
	EReserveOwnershipPolicy ReserveOwnershipPolicy =
		EReserveOwnershipPolicy::RefillOnOwnerChange;

	/** Optional route relative to the guard-post actor. Empty uses the Guard Post Definition route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	TArray<FTerritoryGuardPatrolTemplateNode> PatrolRoute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard Post")
	bool bLoopPatrol = true;
};

/** Physical multiplayer capture-point Blueprint and settings for a Place. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCapturePointTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<ATerritoryCapturePoint> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides, ClampMin="100.0", Units="cm"))
	float CaptureRadius = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides,
			ToolTip="Normal domination/multiplayer progress. Story Capture From Bounds disables automatic progress."))
	bool bAutomaticCapture = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capture Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	bool bHideWhileUnavailable = true;
};

/** Narrative POI/interactable used to open Territory management. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryManagementPointTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<ATerritoryDistrictManagementPoint> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	/** Empty means this District, or the parent District when authored on a Place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides, Categories="Territory"))
	FGameplayTag ManagedDistrictOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<UTerritoryDistrictManagementWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides, Categories="UI.Layer"))
	FGameplayTag WidgetLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Management Point",
		meta=(EditCondition="bEnabled", EditConditionHides, ClampMin="100.0", Units="cm"))
	float InteractionDistance = 600.f;
};

/** Protected owner Blueprint and Narrative dialogue settings for story handover. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryStoryOwnerTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<ATerritoryStoryOwnerSpawner> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FTransform RelativeTransform = FTransform::Identity;

	/** Narrative Pro owns NPC identity, appearance, abilities, default dialogue, and spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TObjectPtr<UNPCDefinition> NPCDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	bool bBeginDialogueOnActivation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	TSoftClassPtr<UDialogue> DialogueOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Story Owner",
		meta=(EditCondition="bEnabled", EditConditionHides))
	FName DialogueStartFromID;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Identity",
		meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(Categories="Narrative.Factions"))
	FGameplayTag InitialOwningFaction;

	/** Story availability is independent from political control. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(DisplayName="Initial Availability",
			ToolTip="Locked keeps this Territory silent until its Narrative Locked exit conditions pass. Ownership is preserved."))
	ETerritoryAvailability InitialAvailability = ETerritoryAvailability::Unlocked;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign")
	ETerritoryInitialState InitialState = ETerritoryInitialState::Automatic;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(ToolTip="Place Definitions are Independent. City and District Definitions are Aggregate Only. The class fixes this value automatically."))
	ETerritoryControlMode ControlMode = ETerritoryControlMode::Independent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="03 New Campaign",
		meta=(ClampMin="1"))
	int32 MaxConcurrentAttackers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(ClampMin="0"))
	int32 PeriodicIncome = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(ClampMin="0"))
	int32 GuardUpkeepPerCycle = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="04 Economy",
		meta=(ClampMin="0"))
	int32 GuardRecruitmentCost = 50;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadOnly, Category="05 State Rules",
		meta=(DisplayName="State Rules (All Runtime States)",
			ToolTip="Always contains four rows: Locked availability, Unclaimed, Contested, and Claimed. Claimed is the stable ownership row after capture completes. Contested Entry Events run once whenever gameplay really enters Contested, not every capture tick."))
	TMap<ETerritoryState, FTerritoryStateConfig> StateConfigs;

	/** Default pre-conflict stealth policy. A State Config may override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="05 State Rules|Stealth",
		meta=(ToolTip="Optional reusable stealth policy. Empty preserves legacy story bounds: entering immediately starts Contested. Easy example: assign a Rescue Mission profile so the player can enter Claimed enemy bounds while undetected."))
	TObjectPtr<UTerritoryStealthProfile> DefaultStealthProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Capture")
	bool bStoryCaptureFromBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="06 Capture")
	FTerritoryCapturePointTemplate CapturePoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards")
	TObjectPtr<UNPCDefinition> DefaultGuardDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards",
		meta=(TitleProperty="Faction"))
	TArray<FTerritoryFactionGuardDefinition> FactionGuardDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards",
		meta=(ClampMin="0"))
	int32 InitialGuardCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards")
	ETerritoryPostCaptureGarrisonPolicy PostCaptureGarrisonPolicy =
		ETerritoryPostCaptureGarrisonPolicy::PlayerChooses;

	/** Shared patrol, crowd-avoidance, and target-priority policy for this Territory's guards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards")
	FTerritoryGuardBehaviorTemplate GuardBehavior;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="07 Guards",
		meta=(TitleProperty="GuardPostID"))
	TArray<FTerritoryGuardPostTemplate> GuardPosts;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="07 Guards|Narrative")
	TArray<TObjectPtr<UNarrativeEvent>> DefenderDiedEvents;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="07 Guards|Narrative")
	TArray<TObjectPtr<UNarrativeEvent>> AllDefendersDefeatedEvents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack")
	TObjectPtr<UTerritoryCounterAttackProfile> CounterAttackProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(TitleProperty="ApproachID"))
	TArray<FTerritoryAssaultApproach> CounterAttackApproaches;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float GuardQuality = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float FortificationStrength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float NearbyAlliedSupport = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="08 Counter Attack",
		meta=(ClampMin="0.0"))
	float StrategicValue = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="09 Management")
	FTerritoryManagementPointTemplate ManagementPoint;

	/** Copies this authoring asset into an actor. Saved owner/state/progress are never overwritten here. */
	UFUNCTION(BlueprintCallable, Category="Territory|Definition")
	bool ApplyToTerritory(ATerritoryVolume* Territory) const;

	const FTerritoryGuardPostTemplate* FindGuardPost(FName GuardPostID) const;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Production")
	TObjectPtr<UTerritoryProductionProfile> ProductionProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Upgrades",
		meta=(ClampMin="0"))
	int32 MaxUpgradeLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Upgrades",
		meta=(ClampMin="0"))
	int32 UpgradeCostPerLevel = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 Place|Upgrades",
		meta=(ClampMin="0"))
	int32 IncomeBonusPerLevel = 25;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 District|Hierarchy")
	TArray<TObjectPtr<UTerritoryPlaceDefinition>> Places;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 District|Economy")
	bool bIsCapital = false;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="10 City|Hierarchy")
	TArray<TObjectPtr<UTerritoryDistrictDefinition>> Districts;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void RefreshHierarchyLinks() override;
	virtual bool IsDefinitionCompatible(const ATerritoryVolume* Territory) const override;
};
