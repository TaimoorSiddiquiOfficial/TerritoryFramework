#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryPlayerManagementComponent.generated.h"

class ATerritoryDistrictManagementPoint;
class ATerritoryVolume;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTerritoryGuardPurchaseResult,
	ATerritoryVolume*, Territory, bool, bSuccess, FText, Message, int32, RequestId);

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

	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="0"))
	float PurchaseCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Territory|Management", meta=(ClampMin="1"))
	int32 MaxGuardPurchaseCount = 10;

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedFaction() const;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveGuardPurchaseResult(ATerritoryVolume* Territory, bool bSuccess, const FText& Message, int32 RequestId);

	void PerformPurchase(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count, int32 RequestId);
	APawn* GetManagingPawn() const;

	float LastPurchaseRequestTime = 0.f;
	int32 NextRequestId = 0;
	int32 LastServerRequestId = 0;
};
