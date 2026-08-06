#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "TerritoryPlayerManagementComponent.generated.h"

class ATerritoryDistrictManagementPoint;
class ATerritoryDistrict;
class ATerritoryVolume;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTerritoryGuardPurchaseResult,
	ATerritoryVolume*, Territory, bool, bSuccess, FText, Message, int32, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryAssaultNotification,
	const FTerritoryAssaultRecord&, Assault);

/** Owned client-to-server bridge for district management actions. */
UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryPlayerManagementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryPlayerManagementComponent();

	/** Ensures the owned bridge exists on a player controller for framework-only projects. */
	static UTerritoryPlayerManagementComponent* FindOrCreateForPlayerController(APlayerController* PlayerController);

	UPROPERTY(BlueprintAssignable, Category="Territory|Management")
	FOnTerritoryGuardPurchaseResult OnGuardPurchaseResult;

	UPROPERTY(BlueprintAssignable, Category="Territory|Counter Attack")
	FOnTerritoryAssaultNotification OnAssaultNotification;

	/** Server-side targeted notification route for this owning controller only. */
	void SendAssaultNotification(const FTerritoryAssaultRecord& Assault);

	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="0"))
	float PurchaseCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="1"))
	int32 MaxGuardPurchaseCount = 10;

	/** Hard RPC input bound; the selected territory still enforces its authored capacity. */
	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="1"))
	int32 MaxGuardTargetCount = 100;

	/** Sets an absolute garrison staffing target through a nearby district command point. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestSetGuardTarget(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount);

	/** Sets an absolute target remotely from the journal for an owned district or child property. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestSetGuardTargetForTerritory(ATerritoryVolume* Territory, int32 NewDesiredGuardCount);

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count = 1);

	/** Remote management action used by the territory journal for owned districts. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestPurchaseGuardsForDistrict(ATerritoryDistrict* District, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestRemoveGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count = 1);

	/** Remote management action used by the territory journal for owned districts. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestRemoveGuardsForDistrict(ATerritoryDistrict* District, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedFaction() const;

private:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSetGuardTarget(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);
	bool ServerRequestSetGuardTarget_Validate(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSetGuardTargetForTerritory(ATerritoryVolume* Territory,
		int32 NewDesiredGuardCount, int32 RequestId);
	bool ServerRequestSetGuardTargetForTerritory_Validate(ATerritoryVolume* Territory,
		int32 NewDesiredGuardCount, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	bool ServerRequestPurchaseGuards_Validate(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPurchaseGuardsForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	bool ServerRequestPurchaseGuardsForDistrict_Validate(ATerritoryDistrict* District, int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRemoveGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	bool ServerRequestRemoveGuards_Validate(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRemoveGuardsForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	bool ServerRequestRemoveGuardsForDistrict_Validate(ATerritoryDistrict* District, int32 Count, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveGuardPurchaseResult(ATerritoryVolume* Territory, bool bSuccess, const FText& Message, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveAssaultNotification(const FTerritoryAssaultRecord& Assault);

	void PerformPurchase(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	void PerformPurchaseForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	void PerformRemove(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	void PerformRemoveForDistrict(ATerritoryDistrict* District, int32 Count, int32 RequestId);
	void PerformSetGuardTarget(ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);
	void PerformSetGuardTargetAtManagementPoint(ATerritoryDistrictManagementPoint* ManagementPoint,
		ATerritoryVolume* Territory, int32 NewDesiredGuardCount, int32 RequestId);
	bool CanManageDistrict(ATerritoryDistrict* District, APawn* Pawn, FText& OutFailureReason) const;
	bool CanManageTerritory(ATerritoryVolume* Territory, APawn* Pawn, FText& OutFailureReason) const;
	bool IsTerritoryManagedByDistrict(ATerritoryDistrict* District, ATerritoryVolume* Territory) const;
	APawn* GetManagingPawn() const;

	float LastPurchaseRequestTime = -BIG_NUMBER;
	int32 NextRequestId = 0;
	int32 LastServerRequestId = 0;
};
