#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Engine/LatentActionManager.h"
#include "TerritoryUIBlueprintLibrary.generated.h"

class APlayerController;
class ATerritoryCity;
class ATerritoryDistrict;
class ATerritoryVolume;
class UTerritoryActivatableWidget;
class UGameplayEffect;
class UNarrativeGameplayAbility;
class UWeaponItem;

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

/**
 * One read-only hierarchy row. "Place" is the player-facing name for
 * ATerritoryProperty; gameplay authority remains on the original Territory actor.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryHierarchyOperationsView
{
	GENERATED_BODY()

	/** Loaded Territory actor represented by this entry; may be empty while streamed out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TObjectPtr<ATerritoryVolume> Territory = nullptr;
	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag TerritoryTag;
	/** Stable tag of this entry's parent District or City. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag ParentTerritoryTag;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FText DisplayName;
	/** Whether this entry represents a City, District or Place. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ETerritoryHierarchyLevel HierarchyLevel = ETerritoryHierarchyLevel::Place;
	/** Availability is a story gate. It is intentionally separate from political State. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;
	/** Current political state, separate from story availability. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ETerritoryState TerritoryState = ETerritoryState::Unclaimed;
	/** Narrative faction owning the Territory represented by this row. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag OwnerFaction;
	/** Narrative faction of the player for whom this UI result was built. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag ViewerFaction;
	/** Whether the existing Territory registry currently knows this actor. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bRegistered = false;
	/** True only when this actor and every required City/District ancestor are loaded, registered, and unlocked. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bVisibleToPlayer = false;
	/** Whether the Territory owner exactly matches the viewer's Narrative faction. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bOwnedByViewer = false;
	/** Whether the viewer currently passes this Territory's capture requirements. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bAvailableForCapture = false;
	/** Result of checking whether the requested capture is allowed. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") ECaptureResult CaptureEligibility = ECaptureResult::InvalidTerritory;
	/** Explains why this Territory is available or blocked for the viewer. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FText AvailabilityReason;
	/** Guards currently alive and assigned to this garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ActiveGuards = 0;
	/** Requested garrison size; it may exceed the currently living guards while replacements are pending. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 DesiredGuards = 0;
	/** Maximum guard capacity supported by the assigned posts and current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 MaximumGuards = 0;
	/** Base currency income per economy cycle before state, faction, upgrade and hierarchy modifiers. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int64 PeriodicIncome = 0;
	/** Currency upkeep charged or projected for these guards. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int64 GuardUpkeep = 0;
	/** Income remaining after the reported upkeep or costs are subtracted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int64 NetIncome = 0;
	/** Whether this Territory has a production configuration assigned. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bHasProductionProfile = false;
};

/** One resource row shared by compact Territory, District, Journal, and Economy widgets. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryResourceOperationsView
{
	GENERATED_BODY()

	/** Narrative item class represented by this resource entry; settlement matches the exact class. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TSubclassOf<class UNarrativeItem> ItemClass;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") FText DisplayName;
	/** Image used to represent this entry in the UI. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TSoftObjectPtr<class UTexture2D> Thumbnail;
	/** Item quantity currently available in the relevant Narrative inventory. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 StoredQuantity = 0;
	/** Item quantity required for one production cycle. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 InputPerCycle = 0;
	/** Item quantity produced by one successful production cycle. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 OutputPerCycle = 0;
	/** Output minus input for one production cycle. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 NetPerCycle = 0;
	/** Whether available inputs cover the next production cycle's requirements. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") bool bSufficientForNextCycle = true;
};

/** One modular production-site row. It owns no gameplay state. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryProductionSiteOperationsView
{
	GENERATED_BODY()

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag TerritoryTag;
	/** Stable tag of this entry's parent District or City. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag ParentTerritoryTag;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FText DisplayName;
	/** Narrative faction owning the Territory represented by this row. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag OwnerFaction;
	/** Stable tag of the rule represented by this production result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FGameplayTag ActiveRuleTag;
	/** Current result or blocking state of this operation. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") ETerritoryProductionStatus Status = ETerritoryProductionStatus::NeverEvaluated;
	/** Readable explanation of the current status. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") FText StatusReason;
	/** Most recent campaign cycle in which this entry was evaluated. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") int64 LastEvaluatedCycle = INDEX_NONE;
	/** Whether this Territory has a production configuration assigned. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") bool bHasProductionProfile = false;
	/** Whether this production site currently reports active production. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") bool bProducing = false;
	/** Whether a rule currently prevents this operation from proceeding. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") bool bBlocked = false;
	/** Latest outcome and cycle information for each production rule at this site. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryProductionRuleState> RuleStates;
	/** Resource rows included in this result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryResourceOperationsView> Resources;
};

/** One independently managed district/property garrison and its local P&L. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryGarrisonOperationsView
{
	GENERATED_BODY()

	/** Loaded Territory actor represented by this entry; may be empty while streamed out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") TObjectPtr<ATerritoryVolume> Territory = nullptr;
	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FGameplayTag TerritoryTag;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText DisplayName;
	/** Whether this result describes a District garrison rather than an individual Place post. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bDistrictGarrison = false;
	/** Whether the Territory owner exactly matches the viewer's Narrative faction. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bOwnedByViewer = false;
	/** Whether this viewer may manage the reported Territory under current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bManageable = false;
	/** Guards currently alive and assigned to this garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ActiveGuards = 0;
	/** Requested garrison size; it may exceed the currently living guards while replacements are pending. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 DesiredGuards = 0;
	/** Maximum guard capacity supported by the assigned posts and current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 MaximumGuards = 0;
	/** Finite replacement guards still available beyond the active garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ReserveGuards = 0;
	/** Troops or groups still waiting for their allowed physical deployment. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 PendingDeployments = 0;
	/** Currency required to recruit one guard under the current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 RecruitmentCostPerGuard = 0;
	/** Currency upkeep for one guard per economy cycle. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 UpkeepPerGuard = 0;
	/** Base currency income per economy cycle before state, faction, upgrade and hierarchy modifiers. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 PeriodicIncome = 0;
	/** Currency upkeep charged or projected for these guards. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 GuardUpkeep = 0;
	/** Income remaining after the reported upkeep or costs are subtracted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 NetIncome = 0;
	/** Whether the viewer may currently raise the desired garrison size. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bCanIncreaseTarget = false;
	/** Whether the viewer may currently lower the desired garrison size. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bCanDecreaseTarget = false;
	/** Whether the viewer may currently request reinforcements for this target. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bCanSendReinforcements = false;
	/** Explains why the desired guard count cannot currently be increased. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText IncreaseFailureReason;
	/** Explains why the desired guard count cannot currently be reduced. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText DecreaseFailureReason;
	/** Explains why reinforcements cannot currently be requested. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText ReinforcementFailureReason;
};

/** One ownership/upgrade benefit row for the Territory Command Center. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryPropertyBenefitOperationsView
{
	GENERATED_BODY()

	/** Loaded Place actor represented by this row; may be empty while streamed out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") TObjectPtr<class ATerritoryProperty> Property = nullptr;
	/** Stable Territory tag identifying the Place. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") FGameplayTag PropertyTag;
	/** Readable name of the Place represented by this row. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") FText PropertyName;
	/** Semantic role of this Place, such as a production or service role. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") FGameplayTag PropertyRoleTag;
	/** Stable tag identifying this ownership or upgrade benefit. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") FGameplayTag BenefitTag;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") FText DisplayName;
	/** Readable explanation shown for this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") FText Description;
	/** The Place's current purchased upgrade level. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") int32 CurrentUpgradeLevel = 0;
	/** Lowest Place upgrade level required by this benefit or rule. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") int32 RequiredUpgradeLevel = 0;
	/** Highest upgrade level available for this Place. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") int32 MaximumUpgradeLevel = 0;
	/** Currency needed for the next available Place upgrade. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") int32 NextUpgradeCost = 0;
	/** Whether the Territory owner exactly matches the viewer's Narrative faction. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") bool bOwnedByViewer = false;
	/** Whether this record or feature is currently active. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") bool bActive = false;
	/** Whether the viewer may currently request the next Place upgrade. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") bool bCanRequestUpgrade = false;
	/** Narrative gameplay ability classes supplied by this benefit. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") TArray<TSubclassOf<UNarrativeGameplayAbility>> GrantedAbilities;
	/** Narrative gameplay effect classes supplied by this benefit. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") TArray<TSubclassOf<UGameplayEffect>> GrantedGameplayEffects;
	/** Narrative weapon item classes exposed by the active ownership or upgrade benefit. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Benefits") TArray<TSubclassOf<UWeaponItem>> UnlockedWeaponItems;
};

/** One player-facing strategic control and the currently held sources that grant it. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCommandCapabilityView
{
	GENERATED_BODY()

	/** Stable gameplay tag identifying the capability being checked or reported. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") FGameplayTag Capability;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") FText DisplayName;
	/** Readable explanation shown for this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") FText Description;
	/** False means no project Territory uses this gate, so legacy action availability is retained. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") bool bConfigured = false;
	/** Whether the requested permission or benefit was granted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") bool bGranted = false;
	/** Only active, currently held sources are exposed; locked enemy source names are never leaked. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") TArray<FText> ActiveSourceNames;
	/** Explains why this Territory is available or blocked for the viewer. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command") FText AvailabilityReason;
};

/** A read-only, viewer-relative projection of existing Territory authorities. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryDistrictOperationsView
{
	GENERATED_BODY()

	/** Loaded District actor associated with this result; may be empty while streamed out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") TObjectPtr<ATerritoryDistrict> District = nullptr;
	/** Stable Territory tag identifying the District. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag DistrictTag;
	/** Readable name shown to designers or players; stable tags and IDs still identify this entry. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FText DisplayName;
	/** Loaded City actor associated with this result; may be empty while the actor is streamed out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TObjectPtr<ATerritoryCity> City = nullptr;
	/** Stable Territory tag identifying the City. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FGameplayTag CityTag;
	/** Readable name of the City shown in this row. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") FText CityDisplayName;
	/** Narrative faction owning the Territory represented by this row. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag OwnerFaction;
	/** Narrative faction of the player for whom this UI result was built. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag ViewerFaction;
	/** Faction currently represented by this capture contest. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag ContestingFaction;
	/** Explicit story availability. UI must render Locked before political State. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;
	/** Current political state, separate from story availability. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") ETerritoryState TerritoryState = ETerritoryState::Unclaimed;

	/** Whether the existing Territory registry currently knows this actor. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bRegistered = false;
	/** False means the stable directory row exists but its runtime actor is streamed out. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bRuntimeLoaded = false;
	/** Whether the story availability gate is currently open. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bUnlocked = false;
	/** Locked City/District ancestors also hide this District from player-facing lists. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bHierarchyVisible = false;
	/** Whether this entry is available under the current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bAvailable = false;
	/** Whether the Territory owner exactly matches the viewer's Narrative faction. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bOwnedByViewer = false;
	/** Whether this viewer may manage the reported Territory under current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bManageable = false;
	/** Whether the viewer currently passes this Territory's capture requirements. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bAvailableForCapture = false;
	/** Result of checking whether the requested capture is allowed. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") ECaptureResult CaptureEligibility = ECaptureResult::InvalidTerritory;
	/** Explains why this Territory is available or blocked for the viewer. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") FText AvailabilityReason;
	/** Explains why Territory management is currently unavailable to this viewer. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") FText ManagementFailureReason;
	/** Explains the story rule currently keeping this Territory locked. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") FText LockReason;

	/** Whether the existing capture system currently reports an active contest. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") bool bCaptureInProgress = false;
	/** Current capture progress from 0 to 1; 1 represents complete progress. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") float CaptureProgress = 0.f;
	/** Living attackers currently registered for capture pressure. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") int32 ActiveAttackers = 0;
	/** Whether the reported attacker count is known; false means unknown, not zero attackers. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Capture") bool bAttackerCountKnown = false;

	/** Guards currently alive and assigned to this garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ActiveGuards = 0;
	/** Requested garrison size; it may exceed the currently living guards while replacements are pending. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 DesiredGuards = 0;
	/** Maximum guard capacity supported by the assigned posts and current rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 MaximumGuards = 0;
	/** Finite replacement guards still available beyond the active garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") int32 ReserveGuards = 0;
	/** Whether the reported reserve count is known; false means unknown, not zero reserves. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bReserveCountKnown = false;
	/** Relative quality multiplier used when estimating garrison defence power for strategic planning. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float GuardQuality = 1.f;
	/** Fortification strength included in the defence estimate. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float Fortification = 0.f;
	/** Allied support included in the defence estimate. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float AlliedSupport = 0.f;
	/** Relative importance of this Territory to strategic assault planning; higher values make it a more valuable target. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") float StrategicValue = 1.f;
	/** Whether the reported garrison currently has no active guards. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bUnguarded = false;
	/** Total authored child Places from the District Definition, including locked or streamed-out Places. Only this aggregate count may reveal locked content. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 TotalProperties = 0;
	/** Places whose identity is visible to the player after City, District, and Place lock checks. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 KnownProperties = 0;
	/** Aggregate-only count. Names, tags, owners, and objectives of these Places are deliberately not exposed. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 HiddenProperties = 0;
	/** Child Places controlled by the viewing player's faction, used for the District completion display. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 OwnedProperties = 0;
	/** Known child Places the viewer can currently contest under authoritative capture rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ContestableProperties = 0;
	/** True only when every authored Place is visible; locked or streamed-out Places keep full District control unavailable. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") bool bAllPlacesDiscovered = false;
	/** Garrisons this viewer may manage under the current ownership and availability rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ManageableGarrisonTargets = 0;
	/** Garrison targets with no currently active guards. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 UnguardedGarrisonTargets = 0;
	/** City, selected District, and loaded unlocked Places in deterministic hierarchy order. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TArray<FTerritoryHierarchyOperationsView> Hierarchy;
	/** Loaded Places visible under the selected District. Locked Places are deliberately absent. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") TArray<FTerritoryHierarchyOperationsView> VisiblePlaces;

	/** Currency currently available through the relevant Narrative account. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 AvailableFunds = 0;
	/** Base currency income per economy cycle before state, faction, upgrade and hierarchy modifiers. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 PeriodicIncome = 0;
	/** Currency upkeep charged or projected for these guards. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 GuardUpkeep = 0;
	/** Income remaining after the reported upkeep or costs are subtracted. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 NetIncome = 0;
	/** Calculated currency cost for the proposed guard purchase. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 GuardPurchaseCost = 0;
	/** Whether current upkeep or production conditions require financial attention. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bFinancialRisk = false;
	/** Whether the viewer may currently request another guard here. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bCanAddGuard = false;
	/** Whether the viewer may currently request a guard removal. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bCanRemoveGuard = false;
	/** Whether the viewer may currently request reinforcements for this target. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") bool bCanSendReinforcements = false;
	/** Explains why the viewer cannot currently add a guard. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") FText AddGuardFailureReason;
	/** Explains why a guard cannot currently be removed. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") FText RemoveGuardFailureReason;
	/** Explains why reinforcements cannot currently be requested. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") FText ReinforcementFailureReason;

	/** State-driven faction controls. Built-ins include guard staffing and reserve reinforcement. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Command")
	TArray<FTerritoryCommandCapabilityView> CommandCapabilities;

	/** District garrison plus every loaded, registered child Property garrison. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Security") TArray<FTerritoryGarrisonOperationsView> GarrisonTargets;

	/** Production works for loaded and World Partition-unloaded child Properties. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryProductionSiteOperationsView> ProductionSites;
	/** Per-resource input, output and net production information. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") TArray<FTerritoryResourceOperationsView> ResourceFlows;
	/** Production sites currently reporting active production. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") int32 ProducingSiteCount = 0;
	/** Production sites currently unable to produce under the reported rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Production") int32 BlockedProductionSiteCount = 0;

	/** UI importance level derived from the current capture and assault state. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") ETerritoryThreatLevel ThreatLevel = ETerritoryThreatLevel::None;
	/** Whether capture or assault state currently marks this Territory as under attack. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") bool bUnderAttack = false;
	/** Whether an assault has already been scheduled for the reported target. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") bool bAttackScheduled = false;
	/** Planning projection only; it is never presented as a scheduled physical assault. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") bool bThreatPreviewAvailable = false;
	/** Assaults that have not yet completed or been cancelled. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 NonTerminalAssaultCount = 0;
	/** Unique ID linking troops, waves and saved records to the same assault. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FGuid AssaultID;
	/** Stable tag of the Territory that the reported threat targets. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FGameplayTag ThreatTargetTerritory;
	/** Current stage of the assault, such as waiting, active or resolved. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") ETerritoryAssaultState AssaultState = ETerritoryAssaultState::Grace;
	/** Outcome recorded when this assault finishes or is cancelled. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") ETerritoryAssaultResolution AssaultResolution = ETerritoryAssaultResolution::None;
	/** Narrative faction launching or participating in this assault. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FGameplayTag AttackingFaction;
	/** Total attackers admitted to this finite assault plan. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 PlannedAttackers = 0;
	/** Attackers from the finite assault force that are still alive. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 AliveAttackers = 0;
	/** Unspent reserve attackers remaining in the finite assault budget. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 PendingReserveAttackers = 0;
	/** Attackers permanently lost from the finite assault force through death. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 KilledAttackers = 0;
	/** Attackers permanently removed from this assault through withdrawal rather than death. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") int32 WithdrawnAttackers = 0;
	/** Chance from 0 to 1 that an eligible assault is launched. Troops must still complete physical capture. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float LaunchProbability = 0.f;
	/** Planning estimate of attacker strength against defence. This never directly changes ownership. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float EstimatedSuccessProbability = 0.f;
	/** Relative target score used to choose between eligible assaults; it is not a capture chance. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float AttackPriority = 0.f;
	/** Estimated defence strength for the target owner's District. Check whether this value is known before using it. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float DistrictDefencePower = 0.f;
	/** Attacker power divided by estimated defence power; used for planning rather than direct capture. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") float PowerRatio = 0.f;
	/** Authored entry routes selected for this finite assault. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") TArray<FName> SelectedApproaches;
	/** Explains the current strategic threat estimate or why it could not be calculated. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FText ThreatEvaluationReason;
	/** Readable summary of the threat facing this Territory. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Threat") FText ThreatSummary;

	/** Relationship between the viewer's faction and this Territory's owner. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") EDiplomacyState ViewerOwnerDiplomacy = EDiplomacyState::None;
	/** Reputation value relevant to the current owner relationship. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") int32 OwnerReputation = 0;
	/** Whether the viewer's faction is at war with the current owner. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") bool bViewerAtWarWithOwner = false;
	/** Whether the viewer's faction is allied with the current owner. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") bool bViewerAlliedWithOwner = false;
	/** Whether the viewer's faction has a trade agreement with the current owner. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") bool bViewerTradesWithOwner = false;
	/** Readable summary of the relevant faction relationship. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Diplomacy") FText DiplomacySummary;
};

/** Faction-level finance read model. Narrative inventory remains the balance authority. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryEconomyOperationsView
{
	GENERATED_BODY()

	/** Exact Narrative faction represented by this setting or result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") FGameplayTag Faction;
	/** Currency currently available through the relevant Narrative account. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 AvailableFunds = 0;
	/** Currency income projected for one economy update. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 IncomePerTick = 0;
	/** Currency costs projected for one economy update. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 CostsPerTick = 0;
	/** Income minus costs for one economy update. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 NetPerTick = 0;
	/** Number of Territories included in this summary. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int32 TerritoryCount = 0;
	/** Whether the reported costs exceed available income or funds. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") bool bDeficit = false;
	/** Recent currency amounts credited through the reported account. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 RecentCredits = 0;
	/** Recent currency amounts deducted through the reported account. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") int64 RecentDebits = 0;
	/** Recent verified economy transactions retained for display. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Finance") TArray<FTerritoryTransaction> RecentTransactions;
	/** Whether an eligible Narrative resource inventory is available. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") bool bResourceStorageAvailable = false;
	/** Resource quantities read from the relevant Narrative storage account. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TArray<FTerritoryResourceOperationsView> ResourceStockpile;
	/** Production-site summaries visible in this query. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") TArray<FTerritoryProductionSiteOperationsView> ProductionSites;
	/** Production sites currently reporting active production. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 ProducingSiteCount = 0;
	/** Production sites currently unable to produce under the reported rules. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Resources") int32 BlockedProductionSiteCount = 0;
};

/** Shared read-model builder for all Territory CommonUI widgets. It owns no gameplay state. */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Continue only after Narrative Pro has created the local GameplayHUD through its
	 * normal possession/PlayerState path. Use before calling a Narrative controller
	 * parent BeginPlay graph that reads GameplayHUD or LoadingMenu.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|Narrative Pro",
		meta=(Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject",
			DefaultToSelf="PlayerController", AdvancedDisplay="TimeoutSeconds"))
	static void WaitForNarrativeGameplayHUD(
		const UObject* WorldContextObject,
		FLatentActionInfo LatentInfo,
		APlayerController* PlayerController,
		float TimeoutSeconds = 15.f);

	/** Push a Territory Narrative-activatable screen onto a Narrative HUD layer. Leave Layer Tag empty to use Narrative's standard Menu layer. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|CommonUI",
		meta=(DeterminesOutputType="WidgetClass", AdvancedDisplay="LayerTag"))
	static UTerritoryActivatableWidget* OpenTerritoryMenu(
		APlayerController* PlayerController,
		TSubclassOf<UTerritoryActivatableWidget> WidgetClass,
		UPARAM(meta=(Categories="UI.Layer")) FGameplayTag LayerTag);

	/**
	 * Resolve the physical Narrative Navigation destination for a Territory command.
	 *
	 * A District or City is an aggregate control area, not a point of interest, so
	 * it resolves to a visible child Place. A legacy flat Territory resolves to
	 * itself. Locked or structurally hidden hierarchies return null.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Navigation")
	static ATerritoryVolume* ResolveTerritoryWaypointTarget(
		APlayerController* PlayerController, ATerritoryVolume* Territory);

	/** Track exactly one visible Place (or legacy flat Territory) using Narrative Navigation. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|Navigation")
	static bool SetTerritoryWaypoint(
		APlayerController* PlayerController, ATerritoryVolume* Territory);

	/** Clear the player's tracked Territory through Narrative navigation. */
	UFUNCTION(BlueprintCallable, Category="Territory|UI|Navigation")
	static void ClearTerritoryWaypoint(APlayerController* PlayerController);

	/** Return the Territory currently tracked by the player's Narrative navigation. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Navigation")
	static ATerritoryVolume* GetTrackedTerritory(
		APlayerController* PlayerController);

	/** True when Territory is the tracked Place or an aggregate ancestor of it. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Navigation")
	static bool IsTerritoryWaypointTracked(
		APlayerController* PlayerController, ATerritoryVolume* Territory);

	/**
	 * Return the District containing the player's pawn.
	 *
	 * A Place is resolved through its owning District, so a player standing in
	 * Castle Hill Farm receives Castle Hill just as a player in the Blacksmith
	 * receives Market Square. Returns null outside a loaded District hierarchy.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Hierarchy",
		meta=(WorldContext="WorldContextObject"))
	static ATerritoryDistrict* GetDistrictAtPlayerLocation(
		const UObject* WorldContextObject,
		APlayerController* PlayerController);

	/**
	 * Most-specific player-visible Territory at a point. If the spatial Place is
	 * locked, this walks to its first unlocked parent without revealing the Place.
	 * Example: locked Farm -> unlocked Castle Hill District.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Availability",
		meta=(WorldContext="WorldContextObject"))
	static ATerritoryVolume* GetVisibleTerritoryAtLocation(
		const UObject* WorldContextObject, FVector WorldLocation);

	/** Build read-only District information for this viewer using current ownership, garrison and assault data. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static bool BuildDistrictOperationsView(
		const UObject* WorldContextObject,
		ATerritoryDistrict* District,
		APlayerController* Viewer,
		FTerritoryDistrictOperationsView& OutView);

	/** Build a read-only District row when its actor is unloaded under World Partition. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject", DisplayName="Build District Operations View From Summary"))
	static bool BuildDistrictOperationsViewFromSummary(
		const UObject* WorldContextObject,
		const FReplicatedCaptureSummary& Summary,
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

	/** Build read-only guard counts, costs and allowed management actions for this viewer and target. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static bool BuildGarrisonOperationsView(
		const UObject* WorldContextObject,
		ATerritoryVolume* Territory,
		APlayerController* Viewer,
		FTerritoryGarrisonOperationsView& OutView);

	/** Build the garrison rows for the selected District and viewer. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations",
		meta=(WorldContext="WorldContextObject"))
	static TArray<FTerritoryGarrisonOperationsView> GetDistrictGarrisonOperationsViews(
		const UObject* WorldContextObject,
		ATerritoryDistrict* District,
		APlayerController* Viewer);

	/** Build the District rows available to this viewer under the current query. */
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

	/** Check whether this District view belongs in the selected operations filter. */
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

	/** Build read-only economy information for this viewer from Narrative accounts and Territory production. */
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

	/** Return a readable label for a production outcome or blocking status. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Production")
	static FText GetProductionStatusText(ETerritoryProductionStatus Status);

	/** Returns authored active and locked benefit tiers for loaded Places in one District. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Benefits",
		meta=(WorldContext="WorldContextObject"))
	static TArray<FTerritoryPropertyBenefitOperationsView>
	GetPropertyBenefitOperationsViews(const UObject* WorldContextObject,
		ATerritoryDistrict* District, APlayerController* Viewer);

	/** Return a readable label for the current UI threat level. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetThreatLevelText(ETerritoryThreatLevel ThreatLevel);

	/**
	 * Player-facing status with the correct precedence.
	 * Example: Locked + Contested is shown as "Locked", because the story gate
	 * decides whether the political state is currently actionable.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Availability")
	static FText GetTerritoryStatusText(
		ETerritoryAvailability Availability,
		ETerritoryState PoliticalState);

	/** Return a readable label for an assault lifecycle state. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetAssaultStateText(ETerritoryAssaultState AssaultState);

	/** Localizable player-facing outcome; never returns the C++ enum identifier. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetAssaultResolutionText(
		ETerritoryAssaultResolution AssaultResolution);

	/** Return a readable label for a faction relationship. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Diplomacy")
	static FText GetDiplomacyStateText(EDiplomacyState DiplomacyState);

	/** Localizable player-facing diplomacy history action. */
	UFUNCTION(BlueprintPure, Category="Territory|UI|Diplomacy")
	static FText GetDiplomacyEventTypeText(EDiplomacyEventType EventType);
};
