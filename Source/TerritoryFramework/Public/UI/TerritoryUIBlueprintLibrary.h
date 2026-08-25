#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "TerritoryUIBlueprintLibrary.generated.h"

class APlayerController;
class ATerritoryCity;
class ATerritoryDistrict;
class ATerritoryVolume;
class UTerritoryActivatableWidget;

/** Viewer-relative operational scopes used by Territory menus and Blueprint lists. */
UENUM(BlueprintType)
enum class ETerritoryOperationsFilter : uint8
{
	All UMETA(ToolTip="Show every loaded registered District, including locked and enemy Districts."),
	Unlocked UMETA(ToolTip="Show Districts that are not Locked."),
	Available UMETA(ToolTip="Show Districts the viewer may currently capture or interact with under Territory rules."),
	Owned UMETA(ToolTip="Show Districts owned by the viewer's exact Narrative faction."),
	Manageable UMETA(ToolTip="Show owned Districts where the viewer may change guards or operations."),
	UnderAttack UMETA(ToolTip="Show Districts with an active or waiting physical counterattack."),
	Contested UMETA(ToolTip="Show Districts whose capture state is Contested."),
	Locked UMETA(ToolTip="Show story-locked Districts. They remain visible but read-only."),
	FinancialRisk UMETA(ToolTip="Show Districts whose upkeep is greater than income."),
	Producing UMETA(ToolTip="Show Districts with at least one production site currently producing."),
	ProductionBlocked UMETA(ToolTip="Show Districts with production stopped by any rule."),
	MissingInputs UMETA(ToolTip="Show production sites waiting for Narrative inventory items."),
	StorageFull UMETA(ToolTip="Show production sites that cannot store more output.")
};

/** Escalation level derived from replicated capture and assault state. */
UENUM(BlueprintType)
enum class ETerritoryThreatLevel : uint8
{
	None UMETA(ToolTip="No current capture or assault threat."),
	Watch UMETA(ToolTip="Early strategic risk. Example: a scheduled warning outside activation range."),
	Warning UMETA(ToolTip="A nearby or waiting assault needs attention."),
	Critical UMETA(ToolTip="Physical attackers are active or capture pressure is dangerous.")
};

/** Player-facing City -> District -> Place level used by hierarchy lists. */
UENUM(BlueprintType)
enum class ETerritoryHierarchyLevel : uint8
{
	City UMETA(ToolTip="A City groups Districts."),
	District UMETA(ToolTip="A District belongs to a City and groups Places."),
	Place UMETA(ToolTip="A capturable Property/Place inside a District.")
};

/**
 * One read-only hierarchy row. "Place" is the player-facing name for
 * ATerritoryProperty; gameplay authority remains on the original Territory actor.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryHierarchyOperationsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TObjectPtr<ATerritoryVolume> Territory = nullptr;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag TerritoryTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag ParentTerritoryTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ETerritoryHierarchyLevel HierarchyLevel = ETerritoryHierarchyLevel::Place;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ETerritoryState TerritoryState = ETerritoryState::Unclaimed;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag OwnerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag ViewerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bRegistered = false;
	/** True only when this actor and every required City/District ancestor are loaded, registered, and unlocked. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bVisibleToPlayer = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bOwnedByViewer = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bAvailableForCapture = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ECaptureResult CaptureEligibility = ECaptureResult::InvalidTerritory;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FText AvailabilityReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ActiveGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 DesiredGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 MaximumGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int64 PeriodicIncome = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int64 GuardUpkeep = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int64 NetIncome = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bHasProductionProfile = false;
};

/** One resource row shared by compact Territory, District, Journal, and Economy widgets. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryResourceOperationsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TSubclassOf<class UNarrativeItem> ItemClass;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TSoftObjectPtr<class UTexture2D> Thumbnail;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 StoredQuantity = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 InputPerCycle = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 OutputPerCycle = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 NetPerCycle = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") bool bSufficientForNextCycle = true;
};

/** One modular production-site row. It owns no gameplay state. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionSiteOperationsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag TerritoryTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag ParentTerritoryTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag OwnerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag ActiveRuleTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") ETerritoryProductionStatus Status = ETerritoryProductionStatus::NeverEvaluated;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FText StatusReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") int64 LastEvaluatedCycle = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") bool bHasProductionProfile = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") bool bProducing = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") bool bBlocked = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryProductionRuleState> RuleStates;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryResourceOperationsView> Resources;
};

/** One independently managed district/property garrison and its local P&L. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGarrisonOperationsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") TObjectPtr<ATerritoryVolume> Territory = nullptr;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FGameplayTag TerritoryTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bDistrictGarrison = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bOwnedByViewer = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bManageable = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ActiveGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 DesiredGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 MaximumGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ReserveGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 PendingDeployments = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 RecruitmentCostPerGuard = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 UpkeepPerGuard = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 PeriodicIncome = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 GuardUpkeep = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 NetIncome = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bCanIncreaseTarget = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bCanDecreaseTarget = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText IncreaseFailureReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText DecreaseFailureReason;
};

/** A read-only, viewer-relative projection of existing Territory authorities. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryDistrictOperationsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") TObjectPtr<ATerritoryDistrict> District = nullptr;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag DistrictTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TObjectPtr<ATerritoryCity> City = nullptr;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag CityTag;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FText CityDisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag OwnerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag ViewerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag ContestingFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") ETerritoryState TerritoryState = ETerritoryState::Unclaimed;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bRegistered = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bUnlocked = false;
	/** Locked City/District ancestors also hide this District from player-facing lists. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bHierarchyVisible = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bOwnedByViewer = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bManageable = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bAvailableForCapture = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") ECaptureResult CaptureEligibility = ECaptureResult::InvalidTerritory;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") FText AvailabilityReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") FText ManagementFailureReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") FText LockReason;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") bool bCaptureInProgress = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") float CaptureProgress = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") int32 ActiveAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") bool bAttackerCountKnown = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ActiveGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 DesiredGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 MaximumGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ReserveGuards = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bReserveCountKnown = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float GuardQuality = 1.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float Fortification = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float AlliedSupport = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float StrategicValue = 1.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bUnguarded = false;
	/** Total loaded, registered child Places, including story-locked Places. Only this aggregate count may reveal locked content. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 TotalProperties = 0;
	/** Places whose identity is visible to the player after City, District, and Place lock checks. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 KnownProperties = 0;
	/** Aggregate-only count. Names, tags, owners, and objectives of these Places are deliberately not exposed. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 HiddenProperties = 0;
	/** Child Places controlled by the viewing player's faction, used for the District completion display. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 OwnedProperties = 0;
	/** Known child Places the viewer can currently contest under authoritative capture rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ContestableProperties = 0;
	/** True only when every configured loaded Place is visible; hidden Places keep full District control unavailable. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bAllPlacesDiscovered = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ManageableGarrisonTargets = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 UnguardedGarrisonTargets = 0;
	/** City, selected District, and loaded unlocked Places in deterministic hierarchy order. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TArray<FTerritoryHierarchyOperationsView> Hierarchy;
	/** Loaded Places visible under the selected District. Locked Places are deliberately absent. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TArray<FTerritoryHierarchyOperationsView> VisiblePlaces;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 AvailableFunds = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 PeriodicIncome = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 GuardUpkeep = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 NetIncome = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 GuardPurchaseCost = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bFinancialRisk = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bCanAddGuard = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bCanRemoveGuard = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") FText AddGuardFailureReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") FText RemoveGuardFailureReason;

	/** District garrison plus every loaded, registered child Property garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") TArray<FTerritoryGarrisonOperationsView> GarrisonTargets;

	/** Production works for loaded and World Partition-unloaded child Properties. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryProductionSiteOperationsView> ProductionSites;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryResourceOperationsView> ResourceFlows;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") int32 ProducingSiteCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") int32 BlockedProductionSiteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") ETerritoryThreatLevel ThreatLevel = ETerritoryThreatLevel::None;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") bool bUnderAttack = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") bool bAttackScheduled = false;
	/** Planning projection only; it is never presented as a scheduled physical assault. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") bool bThreatPreviewAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 NonTerminalAssaultCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FGuid AssaultID;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FGameplayTag ThreatTargetTerritory;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") ETerritoryAssaultState AssaultState = ETerritoryAssaultState::Grace;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") ETerritoryAssaultResolution AssaultResolution = ETerritoryAssaultResolution::None;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FGameplayTag AttackingFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 PlannedAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 AliveAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 PendingReserveAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 KilledAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 WithdrawnAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float LaunchProbability = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float EstimatedSuccessProbability = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float AttackPriority = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float DistrictDefencePower = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float PowerRatio = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") TArray<FName> SelectedApproaches;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FText ThreatEvaluationReason;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FText ThreatSummary;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") EDiplomacyState ViewerOwnerDiplomacy = EDiplomacyState::None;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") int32 OwnerReputation = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") bool bViewerAtWarWithOwner = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") bool bViewerAlliedWithOwner = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") bool bViewerTradesWithOwner = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") FText DiplomacySummary;
};

/** Faction-level finance read model. Narrative inventory remains the balance authority. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryEconomyOperationsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") FGameplayTag Faction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 AvailableFunds = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 IncomePerTick = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 CostsPerTick = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 NetPerTick = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 TerritoryCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bDeficit = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 RecentCredits = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 RecentDebits = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") TArray<FTerritoryTransaction> RecentTransactions;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") bool bResourceStorageAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TArray<FTerritoryResourceOperationsView> ResourceStockpile;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TArray<FTerritoryProductionSiteOperationsView> ProductionSites;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 ProducingSiteCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 BlockedProductionSiteCount = 0;
};

/** Shared read-model builder for all Territory CommonUI widgets. It owns no gameplay state. */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Push a Territory Narrative-activatable screen onto a registered Narrative HUD layer. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|CommonUI",
		meta=(DeterminesOutputType="WidgetClass"))
	static UTerritoryActivatableWidget* OpenTerritoryMenu(
		APlayerController* PlayerController,
		TSubclassOf<UTerritoryActivatableWidget> WidgetClass,
		UPARAM(meta=(Categories="UI.Layer")) FGameplayTag LayerTag);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static bool BuildDistrictOperationsView(
		const UObject* WorldContextObject,
		ATerritoryDistrict* District,
		APlayerController* Viewer,
		FTerritoryDistrictOperationsView& OutView);

	/** Build a City, District, or Place row without creating gameplay state. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Hierarchy",
		meta=(WorldContext="WorldContextObject"))
	static bool BuildHierarchyOperationsView(
		const UObject* WorldContextObject,
		ATerritoryVolume* Territory,
		APlayerController* Viewer,
		FTerritoryHierarchyOperationsView& OutView);

	/**
	 * True when the actor and every required hierarchy ancestor are loaded,
	 * registered, and unlocked. Player menus use this instead of revealing story gates.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Hierarchy",
		meta=(WorldContext="WorldContextObject"))
	static bool IsTerritoryVisibleToPlayer(
		const UObject* WorldContextObject,
		ATerritoryVolume* Territory);

	/** City -> District -> loaded visible Places for a selected District. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Hierarchy",
		meta=(WorldContext="WorldContextObject"))
	static TArray<FTerritoryHierarchyOperationsView> GetDistrictHierarchyOperationsViews(
		const UObject* WorldContextObject,
		ATerritoryDistrict* District,
		APlayerController* Viewer);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static bool BuildGarrisonOperationsView(
		const UObject* WorldContextObject,
		ATerritoryVolume* Territory,
		APlayerController* Viewer,
		FTerritoryGarrisonOperationsView& OutView);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static TArray<FTerritoryGarrisonOperationsView> GetDistrictGarrisonOperationsViews(
		const UObject* WorldContextObject,
		ATerritoryDistrict* District,
		APlayerController* Viewer);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static TArray<FTerritoryDistrictOperationsView> GetDistrictOperationsViews(
		const UObject* WorldContextObject,
		APlayerController* Viewer,
		ETerritoryOperationsFilter Filter = ETerritoryOperationsFilter::All);

	/** Player journal list. Locked Districts and descendants of locked Cities are absent. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static TArray<FTerritoryDistrictOperationsView> GetPlayerVisibleDistrictOperationsViews(
		const UObject* WorldContextObject,
		APlayerController* Viewer,
		ETerritoryOperationsFilter Filter = ETerritoryOperationsFilter::All);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static bool DoesDistrictMatchFilter(
		const FTerritoryDistrictOperationsView& View,
		ETerritoryOperationsFilter Filter);

	/**
	 * Tokenized, case-insensitive search over the player-facing District read model.
	 * Every non-empty token must match at least one indexed field, including the
	 * display name, stable tag, owner, state, availability, threat, or child garrison.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static bool DoesDistrictMatchSearch(
		const FTerritoryDistrictOperationsView& View,
		const FString& SearchText);

	/** True for a registered, hierarchy-visible, unlocked district not already owned by the viewer. Capture gates remain visible as contextual reasons. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static bool IsDistrictAvailableUnlocked(const FTerritoryDistrictOperationsView& View);

	/** True when the viewer faction is the durable owner of a registered district. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static bool IsDistrictCapturedOwned(const FTerritoryDistrictOperationsView& View);

	/** Stable-enough UI revision used to avoid rebuilding an unchanged widget tree. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static int32 GetDistrictOperationsRevision(const FTerritoryDistrictOperationsView& View);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Finance",
		meta=(WorldContext="WorldContextObject"))
	static FTerritoryEconomyOperationsView BuildEconomyOperationsView(
		const UObject* WorldContextObject,
		APlayerController* Viewer,
		FGameplayTag Faction,
		int32 MaxRecentTransactions = 10);

	/** Build one production-site read model from the server subsystem or client WorldState projection. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Production",
		meta=(WorldContext="WorldContextObject"))
	static bool BuildProductionSiteOperationsView(
		const UObject* WorldContextObject,
		FGameplayTag TerritoryTag,
		FTerritoryProductionSiteOperationsView& OutView);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Production")
	static FText GetProductionStatusText(ETerritoryProductionStatus Status);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetThreatLevelText(ETerritoryThreatLevel ThreatLevel);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetAssaultStateText(ETerritoryAssaultState AssaultState);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Diplomacy")
	static FText GetDiplomacyStateText(EDiplomacyState DiplomacyState);
};
