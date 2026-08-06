#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "TerritoryUIBlueprintLibrary.generated.h"

class APlayerController;
class ATerritoryDistrict;
class ATerritoryVolume;
class UTerritoryActivatableWidget;

/** Viewer-relative operational scopes used by Territory menus and Blueprint lists. */
UENUM(BlueprintType)
enum class ETerritoryOperationsFilter : uint8
{
	All,
	Unlocked,
	Available,
	Owned,
	Manageable,
	UnderAttack,
	Contested,
	Locked,
	FinancialRisk
};

/** Escalation level derived from replicated capture and assault state. */
UENUM(BlueprintType)
enum class ETerritoryThreatLevel : uint8
{
	None,
	Watch,
	Warning,
	Critical
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
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag OwnerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag ViewerFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") FGameplayTag ContestingFaction;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI") ETerritoryState TerritoryState = ETerritoryState::Unclaimed;

	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bRegistered = false;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Availability") bool bUnlocked = false;
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
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 TotalProperties = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 OwnedProperties = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 ManageableGarrisonTargets = 0;
	UPROPERTY(BlueprintReadOnly, Category="Territory|UI|Hierarchy") int32 UnguardedGarrisonTargets = 0;

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

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static bool DoesDistrictMatchFilter(
		const FTerritoryDistrictOperationsView& View,
		ETerritoryOperationsFilter Filter);

	/** True only for a registered, unlocked, currently actionable district not already owned by the viewer. */
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

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetThreatLevelText(ETerritoryThreatLevel ThreatLevel);

	UFUNCTION(BlueprintPure, Category="Territory|UI|Operations")
	static FText GetAssaultStateText(ETerritoryAssaultState AssaultState);
};
