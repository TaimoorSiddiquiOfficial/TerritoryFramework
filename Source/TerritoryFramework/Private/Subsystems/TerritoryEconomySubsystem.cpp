#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTerritoryEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings)
	{
		TickIntervalSeconds = Settings->EconomyTickIntervalSeconds;
	}

	// Register for territory events
	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->OnTerritoryRegistered.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryRegistered);
		Registry->OnTerritoryUnregistered.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryUnregistered);
	}

	// Start economy tick timer (server-only — economy state is server-authoritative)
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		World->GetTimerManager().SetTimer(
			EconomyTickTimerHandle,
			this,
			&UTerritoryEconomySubsystem::OnEconomyTick,
			TickIntervalSeconds,
			true);
	}

	UE_LOG(LogTerritory, Log, TEXT("TerritoryEconomySubsystem initialized (tick: %.0fs, server-only: %s)"),
		TickIntervalSeconds, World && World->GetNetMode() != NM_Client ? TEXT("true") : TEXT("false"));
}

void UTerritoryEconomySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EconomyTickTimerHandle);
	}

	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->OnTerritoryRegistered.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryRegistered);
		Registry->OnTerritoryUnregistered.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryUnregistered);
	}

	FactionTreasuries.Empty();
	Super::Deinitialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Economy Timer
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryEconomySubsystem::OnEconomyTick()
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTicks = Settings && Settings->ShouldDebugEconomy();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	for (auto& Pair : FactionTreasuries)
	{
		FTerritoryTreasury& Treasury = Pair.Value;
		int32 NetIncome = Treasury.IncomePerTick - Treasury.CostsPerTick;
		Treasury.Gold += NetIncome;

		if (Treasury.Gold < 0) Treasury.Gold = 0;

		// Record ledger entries for this tick
		if (Treasury.IncomePerTick > 0)
		{
			FTerritoryTransaction IncomeTx;
			IncomeTx.TransactionID = FGuid::NewGuid();
			IncomeTx.Faction = Pair.Key;
			IncomeTx.Type = ETerritoryTransactionType::Income;
			IncomeTx.Amount = Treasury.IncomePerTick;
			IncomeTx.BalanceAfter = Treasury.Gold;
			IncomeTx.Reason = TEXT("Periodic income");
			if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
			{
				IncomeTx.GameTime = GS->GetAccumulatedTime();
			}
			TransactionLedger.Add(IncomeTx);
		}
		if (Treasury.CostsPerTick > 0)
		{
			FTerritoryTransaction UpkeepTx;
			UpkeepTx.TransactionID = FGuid::NewGuid();
			UpkeepTx.Faction = Pair.Key;
			UpkeepTx.Type = ETerritoryTransactionType::GuardUpkeep;
			UpkeepTx.Amount = -Treasury.CostsPerTick;
			UpkeepTx.BalanceAfter = Treasury.Gold;
			UpkeepTx.Reason = TEXT("Guard upkeep");
			if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
			{
				UpkeepTx.GameTime = GS->GetAccumulatedTime();
			}
			TransactionLedger.Add(UpkeepTx);
		}
		while (TransactionLedger.Num() > MaxTransactionHistory)
		{
			TransactionLedger.RemoveAt(0);
		}

		if (bDebugTicks)
		{
			UE_LOG(LogTerritory, Log, TEXT("[EconomyTick] %s: gold=%d, income=%d, costs=%d, net=%d, territories=%d"),
				*Pair.Key.ToString(), Treasury.Gold, Treasury.IncomePerTick,
				Treasury.CostsPerTick, NetIncome, Treasury.TerritoryCount);
		}

		FTerritoryEconomySnapshot Snapshot;
		Snapshot.Treasury = Treasury.Gold;
		Snapshot.TotalIncome = Treasury.IncomePerTick;
		Snapshot.TotalCosts = Treasury.CostsPerTick;
		Snapshot.TerritoryCount = Treasury.TerritoryCount;

		OnEconomyTickFired.Broadcast(Pair.Key, Snapshot);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Query API (Read-Only)
// ═══════════════════════════════════════════════════════════════════════════════

int32 UTerritoryEconomySubsystem::GetTreasury(const FGameplayTag& Faction) const
{
	const FTerritoryTreasury* Treasury = FactionTreasuries.Find(Faction);
	return Treasury ? Treasury->Gold : 0;
}

int32 UTerritoryEconomySubsystem::GetIncome(const FGameplayTag& Faction) const
{
	const FTerritoryTreasury* Treasury = FactionTreasuries.Find(Faction);
	return Treasury ? Treasury->IncomePerTick : 0;
}

int32 UTerritoryEconomySubsystem::GetCosts(const FGameplayTag& Faction) const
{
	const FTerritoryTreasury* Treasury = FactionTreasuries.Find(Faction);
	return Treasury ? Treasury->CostsPerTick : 0;
}

bool UTerritoryEconomySubsystem::CanAfford(const FGameplayTag& Faction, int32 Cost) const
{
	return Cost >= 0 && GetTreasury(Faction) >= Cost;
}

FTerritoryTreasury UTerritoryEconomySubsystem::GetFactionEconomy(const FGameplayTag& Faction) const
{
	const FTerritoryTreasury* Treasury = FactionTreasuries.Find(Faction);
	return Treasury ? *Treasury : FTerritoryTreasury();
}

TArray<FGameplayTag> UTerritoryEconomySubsystem::GetAllFactionsWithTreasury() const
{
	TArray<FGameplayTag> Result;
	FactionTreasuries.GetKeys(Result);
	return Result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Mutation API (Authority-Only)
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryEconomySubsystem::AddToTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason, ETerritoryTransactionType Type)
{
	if (!Faction.IsValid() || PositiveAmount <= 0) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	FTerritoryTreasury& Treasury = FactionTreasuries.FindOrAdd(Faction);
	Treasury.Gold += PositiveAmount;

	// Record transaction
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = PositiveAmount;
	Tx.BalanceAfter = Treasury.Gold;
	Tx.Reason = Reason;

	// Set game time from NarrativeGameState
	if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
	{
		Tx.GameTime = GS->GetAccumulatedTime();
	}

	TransactionLedger.Add(Tx);

	// Trim ledger if over limit
	while (TransactionLedger.Num() > MaxTransactionHistory)
	{
		TransactionLedger.RemoveAt(0);
	}

	OnTransactionRecorded.Broadcast(Tx);

	if (bDebugTx)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Transaction] CREDIT %s: +%d (%s) balance=%d"),
			*Faction.ToString(), PositiveAmount, *Reason, Tx.BalanceAfter);
	}
}

bool UTerritoryEconomySubsystem::TryDebitTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason, ETerritoryTransactionType Type)
{
	if (!Faction.IsValid() || PositiveAmount <= 0) return false;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	FTerritoryTreasury* Treasury = FactionTreasuries.Find(Faction);
	if (!Treasury || Treasury->Gold < PositiveAmount) return false;

	Treasury->Gold -= PositiveAmount;

	// Record transaction (negative amount for debits)
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = -PositiveAmount;
	Tx.BalanceAfter = Treasury->Gold;
	Tx.Reason = Reason;

	// Set game time from NarrativeGameState
	if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
	{
		Tx.GameTime = GS->GetAccumulatedTime();
	}

	TransactionLedger.Add(Tx);

	while (TransactionLedger.Num() > MaxTransactionHistory)
	{
		TransactionLedger.RemoveAt(0);
	}

	OnTransactionRecorded.Broadcast(Tx);

	if (bDebugTx)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Transaction] DEBIT %s: -%d (%s) balance=%d"),
			*Faction.ToString(), PositiveAmount, *Reason, Tx.BalanceAfter);
	}
	return true;
}

TArray<FTerritoryTransaction> UTerritoryEconomySubsystem::GetTransactionHistory(const FGameplayTag& Faction, int32 MaxEntries) const
{
	TArray<FTerritoryTransaction> Result;
	for (int32 i = TransactionLedger.Num() - 1; i >= 0 && Result.Num() < MaxEntries; --i)
	{
		if (TransactionLedger[i].Faction == Faction)
		{
			Result.Add(TransactionLedger[i]);
		}
	}
	return Result;
}

void UTerritoryEconomySubsystem::SetFactionTreasury(const FGameplayTag& Faction, const FTerritoryTreasury& Treasury)
{
	if (!Faction.IsValid()) return;
	FactionTreasuries.Add(Faction, Treasury);
}

void UTerritoryEconomySubsystem::RecalculateIncome(const FGameplayTag& Faction)
{
	if (!Faction.IsValid()) return;

	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	TArray<ATerritoryVolume*> Territories = Registry->GetTerritoriesOwnedByFaction(Faction);

	FTerritoryTreasury& Treasury = FactionTreasuries.FindOrAdd(Faction);
	Treasury.IncomePerTick = 0;
	Treasury.CostsPerTick = 0;
	Treasury.TerritoryCount = Territories.Num();

	for (const ATerritoryVolume* Territory : Territories)
	{
		// Use GetEffectiveIncome for properties (includes upgrade bonus)
		// Fall back to GetPeriodicIncome for non-property territories
		const ATerritoryProperty* Property = Cast<const ATerritoryProperty>(Territory);
		if (Property)
		{
			Treasury.IncomePerTick += Property->GetEffectiveIncome();
		}
		else
		{
			Treasury.IncomePerTick += Territory->GetPeriodicIncome();
		}
		Treasury.CostsPerTick += Territory->GetGuardCost();
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Event Handlers
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryEconomySubsystem::OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (OldOwner.IsValid())
	{
		RecalculateIncome(OldOwner);
	}
	if (NewOwner.IsValid())
	{
		RecalculateIncome(NewOwner);
	}
}

void UTerritoryEconomySubsystem::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (!Territory || bWasUnregistered) return;

	// When a territory registers, bind its control-changed delegate
	Territory->OnTerritoryControlChanged.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);

	// Recalculate income for the owning faction
	FGameplayTag Owner = Territory->GetOwningFaction();
	if (Owner.IsValid())
	{
		RecalculateIncome(Owner);
	}
}

void UTerritoryEconomySubsystem::OnTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (!Territory || !bWasUnregistered) return;

	// Unbind the control-changed delegate to prevent dangling references
	Territory->OnTerritoryControlChanged.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);

	// Recalculate income for the owning faction (territory removed from their count)
	FGameplayTag Owner = Territory->GetOwningFaction();
	if (Owner.IsValid())
	{
		RecalculateIncome(Owner);
	}
}
