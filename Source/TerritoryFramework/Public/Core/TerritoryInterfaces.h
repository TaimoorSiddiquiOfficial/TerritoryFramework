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
	/** Return the implementing Territory's current Narrative owner faction. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	FGameplayTag GetTerritoryOwner() const;

	/** Read the implementing Territory's control progress through the ownership interface. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	float GetTerritoryControlProgress() const;

	/** Check whether the implementing Territory currently has a capture contest. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	bool IsTerritoryContested() const;

	/** Return the faction currently represented by this capture contest. */
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
	/** Read currency from the faction's relevant Narrative account or replicated account view. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	int32 GetTreasury(FGameplayTag Faction) const;

	/** Return the current periodic income rate from the relevant Territory or faction economy view. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	int32 GetPeriodicIncome(FGameplayTag Faction) const;

	/** Check whether the relevant faction account has enough Narrative currency for this cost. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	bool CanAfford(FGameplayTag Faction, int32 Cost) const;

	/** Read the supplied actor's currency from its Narrative inventory/account. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	int32 GetActorCurrency(AActor* Requester) const;

	/** Check the supplied actor's Narrative currency against the requested cost. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory|Economy")
	bool CanActorAfford(AActor* Requester, int32 Cost) const;
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
	/** Called after the Territory owner changes, with the old and new factions. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryControlChanged(FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner);

	/** Interface event notifying a listener that this Territory entered a capture contest. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryContested(FGameplayTag TerritoryTag, FGameplayTag ContestingFaction);

	/** Interface event notifying a listener that this Territory's contest ended. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryUncontested(FGameplayTag TerritoryTag);

	/** Interface event notifying a listener of a Territory political-state change. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryStateChanged(FGameplayTag TerritoryTag, ETerritoryState NewState);
};
