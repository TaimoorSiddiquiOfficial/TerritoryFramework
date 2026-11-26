#include "Core/TerritoryUpgradeDefinition.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	AActor* ResolveInventoryOwner(AActor* Requester)
	{
		if (const AController* Controller = Cast<AController>(Requester))
		{
			return Controller->GetPawn();
		}
		return Requester;
	}

	UNarrativeInventoryComponent* GetRequesterInventory(AActor* Requester)
	{
		if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(ResolveInventoryOwner(Requester)))
		{
			return Character->GetInventoryComponent();
		}
		return nullptr;
	}

	bool AreEntryConditionsMet(const FTerritoryUpgradeCostEntry& Entry, APawn* Target,
		APlayerController* Controller, UTalesComponent* Tales)
	{
		for (UNarrativeCondition* Condition : Entry.Conditions)
		{
			if (!IsValid(Condition))
			{
				continue;
			}
			if (!TerritoryTales::DoesConditionPass(Condition, Target, Controller, Tales))
			{
				return false;
			}
		}
		return true;
	}

	bool CanAffordEntry(const FTerritoryUpgradeCostEntry& Entry, AActor* Requester,
		UTerritoryEconomySubsystem* Economy, UNarrativeInventoryComponent* Inventory)
	{
		switch (Entry.Payment)
		{
		case ETerritoryUpgradePayment::Currency:
			if (Entry.CurrencyAmount <= 0)
			{
				return true;
			}
			return Economy && Requester && Economy->CanActorAfford(Requester, Entry.CurrencyAmount);
		case ETerritoryUpgradePayment::Item:
			return Inventory && !Entry.ItemClass.IsNull()
				&& Inventory->HasItemExact(Entry.ItemClass, FMath::Max(1, Entry.ItemQuantity));
		case ETerritoryUpgradePayment::ConditionOnly:
		case ETerritoryUpgradePayment::Free:
		default:
			return true;
		}
	}

	void AppendEntryToPlan(const FTerritoryUpgradeCostEntry& Entry,
		TerritoryUpgrade::FTerritoryUpgradePaymentPlan& Plan)
	{
		switch (Entry.Payment)
		{
		case ETerritoryUpgradePayment::Currency:
			if (Entry.CurrencyAmount > 0)
			{
				Plan.CurrencyTotal = static_cast<int32>(FMath::Min<int64>(
					static_cast<int64>(Plan.CurrencyTotal) + Entry.CurrencyAmount,
					static_cast<int64>(TNumericLimits<int32>::Max())));
				Plan.bFree = false;
			}
			break;
		case ETerritoryUpgradePayment::Item:
		{
			UClass* ItemClass = Entry.ItemClass.LoadSynchronous();
			if (ItemClass)
			{
				Plan.Items.Emplace(ItemClass, FMath::Max(1, Entry.ItemQuantity));
				Plan.bFree = false;
			}
			break;
		}
		default:
			break;
		}
	}

	void ReturnConsumedItems(UNarrativeInventoryComponent* Inventory,
		const TArray<TPair<UClass*, int32>>& Consumed)
	{
		if (!Inventory)
		{
			return;
		}
		for (const TPair<UClass*, int32>& Pair : Consumed)
		{
			if (Pair.Key && Pair.Value > 0)
			{
				Inventory->TryAddItemFromClass(Pair.Key, Pair.Value, false);
			}
		}
	}
}

const FTerritoryUpgradeTier* UTerritoryUpgradeDefinition::FindTierConst(int32 Level) const
{
	for (const FTerritoryUpgradeTier& Tier : Tiers)
	{
		if (Tier.Level == Level)
		{
			return &Tier;
		}
	}
	return nullptr;
}

bool UTerritoryUpgradeDefinition::FindTier(int32 Level, FTerritoryUpgradeTier& OutTier) const
{
	if (const FTerritoryUpgradeTier* Tier = FindTierConst(Level))
	{
		OutTier = *Tier;
		return true;
	}
	return false;
}

FPrimaryAssetId UTerritoryUpgradeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryUpgradeDefinition"), GetFName());
}

FPrimaryAssetId UTerritoryUpgradeBalanceProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryUpgradeBalanceProfile"), GetFName());
}

bool TerritoryUpgrade::BuildPaymentPlan(const FTerritoryUpgradeTier& Tier, AActor* Requester,
	APawn* Target, APlayerController* Controller, UTalesComponent* Tales, FString& OutFailure,
	FTerritoryUpgradePaymentPlan& OutPlan)
{
	OutPlan = FTerritoryUpgradePaymentPlan();

	UTerritoryEconomySubsystem* Economy = nullptr;
	if (const AActor* InventoryOwner = ResolveInventoryOwner(Requester))
	{
		if (UWorld* World = InventoryOwner->GetWorld())
		{
			Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
		}
	}
	else if (Target)
	{
		Economy = Target->GetWorld() ? Target->GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	}
	UNarrativeInventoryComponent* Inventory = GetRequesterInventory(Requester);

	bool bHasOptional = false;
	bool bOptionalSatisfied = false;

	for (const FTerritoryUpgradeCostEntry& Entry : Tier.Costs)
	{
		if (!AreEntryConditionsMet(Entry, Target, Controller, Tales))
		{
			if (Entry.bOptional)
			{
				bHasOptional = true;
				continue;
			}
			OutFailure = FString::Printf(TEXT("Upgrade requirement failed on route %s."),
				Entry.PathTag.IsValid() ? *Entry.PathTag.ToString() : TEXT("(untagged)"));
			return false;
		}

		if (!CanAffordEntry(Entry, Requester, Economy, Inventory))
		{
			if (Entry.bOptional)
			{
				bHasOptional = true;
				continue;
			}
			OutFailure = Entry.Payment == ETerritoryUpgradePayment::Item
				? FString::Printf(TEXT("Missing %d x %s for the upgrade."),
					FMath::Max(1, Entry.ItemQuantity),
					Entry.ItemClass.IsNull() ? TEXT("item") : *Entry.ItemClass.GetAssetName())
				: FString::Printf(TEXT("Not enough currency for the %d cost."), Tier.Costs.Num() > 0
					? Entry.CurrencyAmount : 0);
			return false;
		}

		if (Entry.bOptional)
		{
			bHasOptional = true;
			if (bOptionalSatisfied)
			{
				continue;
			}
			bOptionalSatisfied = true;
		}
		AppendEntryToPlan(Entry, OutPlan);
	}

	if (bHasOptional && !bOptionalSatisfied)
	{
		OutFailure = TEXT("No optional upgrade route is currently satisfied.");
		return false;
	}

	if (OutPlan.CurrencyTotal > 0)
	{
		if (!Economy || !Requester || !Economy->CanActorAfford(Requester, OutPlan.CurrencyTotal))
		{
			OutFailure = FString::Printf(TEXT("Not enough currency for the %d total cost."),
				OutPlan.CurrencyTotal);
			return false;
		}
	}

	if (!OutPlan.Items.IsEmpty() && !Inventory)
	{
		OutFailure = TEXT("The upgrade requires items but the requester has no Narrative inventory.");
		return false;
	}

	return true;
}

bool TerritoryUpgrade::ExecutePaymentPlan(const FTerritoryUpgradePaymentPlan& Plan, AActor* Requester,
	const FGameplayTag& Faction, const FString& Reason, FString& OutFailure)
{
	if (Plan.bFree)
	{
		return true;
	}

	UNarrativeInventoryComponent* Inventory = GetRequesterInventory(Requester);
	TArray<TPair<UClass*, int32>> Consumed;
	Consumed.Reserve(Plan.Items.Num());

	for (const TPair<TSubclassOf<UNarrativeItem>, int32>& Pair : Plan.Items)
	{
		UClass* ItemClass = Pair.Key.Get();
		if (!Inventory || !ItemClass || Pair.Value <= 0)
		{
			ReturnConsumedItems(Inventory, Consumed);
			OutFailure = TEXT("The upgrade item payment is invalid.");
			return false;
		}
		const int32 ConsumedCount = Inventory->ConsumeItemsOfClass(ItemClass, Pair.Value);
		if (ConsumedCount < Pair.Value)
		{
			if (ConsumedCount > 0)
			{
				Inventory->TryAddItemFromClass(ItemClass, ConsumedCount, false);
			}
			ReturnConsumedItems(Inventory, Consumed);
			OutFailure = TEXT("The upgrade item payment could not be completed.");
			return false;
		}
		Consumed.Emplace(ItemClass, Pair.Value);
	}

	if (Plan.CurrencyTotal > 0)
	{
		UTerritoryEconomySubsystem* Economy = Requester && Requester->GetWorld()
			? Requester->GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
		if (!Economy || !Requester)
		{
			ReturnConsumedItems(Inventory, Consumed);
			OutFailure = TEXT("The upgrade currency payment is unavailable.");
			return false;
		}
		const FTerritoryCurrencyMutationResult Payment = Economy->DebitCurrencyWithResult(
			Requester, Plan.CurrencyTotal, Faction, Reason, ETerritoryTransactionType::UpgradeCost);
		if (Payment.Status == ETerritoryCurrencyMutationStatus::Rejected
			|| Payment.Status == ETerritoryCurrencyMutationStatus::Superseded)
		{
			ReturnConsumedItems(Inventory, Consumed);
			OutFailure = TEXT("The upgrade currency payment was rejected.");
			return false;
		}
	}

	return true;
}

void TerritoryUpgrade::RefundPaymentPlan(const FTerritoryUpgradePaymentPlan& Plan, AActor* Requester,
	const FGameplayTag& Faction, const FString& Reason)
{
	if (Plan.bFree || !Requester)
	{
		return;
	}
	if (UNarrativeInventoryComponent* Inventory = GetRequesterInventory(Requester))
	{
		for (const TPair<TSubclassOf<UNarrativeItem>, int32>& Pair : Plan.Items)
		{
			if (UClass* ItemClass = Pair.Key.Get())
			{
				Inventory->TryAddItemFromClass(ItemClass, Pair.Value, false);
			}
		}
	}
	if (Plan.CurrencyTotal > 0)
	{
		if (UTerritoryEconomySubsystem* Economy = Requester->GetWorld()
			? Requester->GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr)
		{
			Economy->CreditCurrencyWithResult(Requester, Plan.CurrencyTotal, Faction, Reason,
				ETerritoryTransactionType::UpgradeRefund);
		}
	}
}
