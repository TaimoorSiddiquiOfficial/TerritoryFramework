#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "UObject/Interface.h"
#include "TerritoryInterfaces.generated.h"

UINTERFACE(BlueprintType)
class UTerritoryOwnershipInterface : public UInterface
{
	GENERATED_BODY()
};

class TERRITORYFRAMEWORK_API ITerritoryOwnershipInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	FGameplayTag GetTerritoryOwner() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	float GetTerritoryControlProgress() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	bool IsTerritoryContested() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	FGameplayTag GetContestingFaction() const;
};

UINTERFACE(BlueprintType)
class UTerritoryEconomyInterface : public UInterface
{
	GENERATED_BODY()
};

class TERRITORYFRAMEWORK_API ITerritoryEconomyInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	int32 GetTreasury(FGameplayTag Faction) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	int32 GetPeriodicIncome(FGameplayTag Faction) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	bool CanAfford(FGameplayTag Faction, int32 Cost) const;
};

UINTERFACE(BlueprintType)
class UTerritoryEventReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

class TERRITORYFRAMEWORK_API ITerritoryEventReceiverInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryControlChanged(FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryContested(FGameplayTag TerritoryTag, FGameplayTag ContestingFaction);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryUncontested(FGameplayTag TerritoryTag);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryStateChanged(FGameplayTag TerritoryTag, ETerritoryState NewState);
};
