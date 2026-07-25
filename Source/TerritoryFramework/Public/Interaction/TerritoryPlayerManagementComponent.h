#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryPlayerManagementComponent.generated.h"

class ATerritoryDistrictManagementPoint;
class ATerritoryVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTerritoryGuardPurchaseResult,
	ATerritoryVolume*, Territory, bool, bSuccess, FText, Message);

/** Owned client-to-server bridge for district management actions. */
UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryPlayerManagementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryPlayerManagementComponent();

	UPROPERTY(BlueprintAssignable, Category="Territory|Management")
	FOnTerritoryGuardPurchaseResult OnGuardPurchaseResult;

	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void RequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedFaction() const;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestPurchaseGuards(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count);

	UFUNCTION(Client, Reliable)
	void ClientReceiveGuardPurchaseResult(ATerritoryVolume* Territory, bool bSuccess, const FText& Message);

	void PerformPurchase(ATerritoryDistrictManagementPoint* ManagementPoint, int32 Count);
	APawn* GetManagingPawn() const;
};
