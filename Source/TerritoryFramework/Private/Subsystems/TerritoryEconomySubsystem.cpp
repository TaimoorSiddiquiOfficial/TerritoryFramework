#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Items/InventoryComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

void UTerritoryEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings)
	{
		TickIntervalSeconds = Settings->EconomyTickIntervalSeconds;
	}

	UWorld* World = GetWorld();

	// Register for territory events
	if (World)
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryRegistered);
			Registry->OnTerritoryUnregistered.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryUnregistered);

			// Also bind delegates for territories already registered before this subsystem initialized.
			// Without this, if EconomySubsystem initializes after RegistrySubsystem, we'd miss
			// territories that registered between RegistrySubsystem::Initialize and now.
			for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (Territory)
				{
					Territory->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);
					FGameplayTag Owner = Territory->GetOwningFaction();
					if (Owner.IsValid())
					{
						RecalculateIncome(Owner);
					}
				}
			}
		}
	}

	// Start economy tick timer (server-only — economy state is server-authoritative)
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

		// Unbind per-territory ownership delegates to prevent dangling references
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryUnregistered);

			for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (Territory)
				{
					Territory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);
				}
			}
		}
	}

	FactionTreasuries.Empty();
	DirtyFactions.Empty();
	Super::Deinitialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Faction Member Bridge — reads/writes NarrativePro UInventoryComponent::Currency
// ═══════════════════════════════════════════════════════════════════════════════

TArray<ANarrativeCharacter*> UTerritoryEconomySubsystem::GetFactionMembers(const FGameplayTag& Faction) const
{
	TArray<ANarrativeCharacter*> Members;
	if (!Faction.IsValid()) return Members;

	UWorld* World = GetWorld();
	if (!World) return Members;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		ANarrativePlayerState* PS = PC->GetPlayerState<ANarrativePlayerState>();
		if (!PS || !PS->GetFactions().HasTagExact(Faction)) continue;

		APawn* Pawn = PC->GetPawn();
		ANarrativeCharacter* Char = Cast<ANarrativeCharacter>(Pawn);
		if (!Char) continue;

		if (UNarrativeInventoryComponent* Inv = Char->GetInventoryComponent())
		{
			Members.Add(Char);
		}
	}
	return Members;
}

int32 UTerritoryEconomySubsystem::GetFactionAggregateCurrency(const FGameplayTag& Faction) const
{
	int32 Total = 0;
	for (const ANarrativeCharacter* Member : GetFactionMembers(Faction))
	{
		if (UNarrativeInventoryComponent* Inv = Member->GetInventoryComponent())
		{
			Total += Inv->GetCurrency();
		}
	}
	return Total;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Economy Timer
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryEconomySubsystem::OnEconomyTick()
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTicks = Settings && Settings->ShouldDebugEconomy();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	// Process deferred income recalculations — factions marked dirty by capture/ownership
	// events get recalculated once per tick instead of O(N) times per capture cascade.
	for (const FGameplayTag& DirtyFaction : DirtyFactions)
	{
		if (DirtyFaction.IsValid())
		{
			RecalculateIncome(DirtyFaction);
		}
	}
	DirtyFactions.Empty();

	for (auto& Pair : FactionTreasuries)
	{
		const FGameplayTag& Faction = Pair.Key;
		FTerritoryTreasury& Treasury = Pair.Value;

		int32 NetIncome = Treasury.IncomePerTick - Treasury.CostsPerTick;

		// Distribute to faction members' UInventoryComponent::Currency
		TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);

		if (Members.Num() > 0 && NetIncome != 0)
		{
			// Split evenly; remainder (from integer division) goes to first member
			int32 BaseShare = NetIncome / Members.Num();
			int32 Remainder = NetIncome - (BaseShare * Members.Num());

			for (int32 i = 0; i < Members.Num(); ++i)
			{
				ANarrativeCharacter* Member = Members[i];
				UNarrativeInventoryComponent* Inv = Member->GetInventoryComponent();
				if (!Inv) continue;

				int32 Share = BaseShare + (i == 0 ? Remainder : 0);
				if (Share != 0)
				{
					Inv->AddCurrency(Share);
				}
			}
		}

		// Record transactions for audit trail (only if actual distribution occurred)
		// Gate on Members.Num() > 0 to prevent ghost transactions
		if (Members.Num() > 0)
		{
			int32 Aggregate = GetFactionAggregateCurrency(Faction);

			if (Treasury.IncomePerTick > 0)
			{
				FTerritoryTransaction IncomeTx;
				IncomeTx.TransactionID = FGuid::NewGuid();
				IncomeTx.Faction = Faction;
				IncomeTx.Type = ETerritoryTransactionType::Income;
				IncomeTx.Amount = Treasury.IncomePerTick;
				IncomeTx.BalanceAfter = Aggregate;
				IncomeTx.Reason = FString::Printf(TEXT("Periodic income (%d members)"), Members.Num());
				if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
				{
					IncomeTx.GameTime = GS->GetAccumulatedTime();
				}
				TransactionLedger.Add(IncomeTx);
				OnTransactionRecorded.Broadcast(IncomeTx);
			}

			if (Treasury.CostsPerTick > 0)
			{
				FTerritoryTransaction UpkeepTx;
				UpkeepTx.TransactionID = FGuid::NewGuid();
				UpkeepTx.Faction = Faction;
				UpkeepTx.Type = ETerritoryTransactionType::GuardUpkeep;
				UpkeepTx.Amount = -Treasury.CostsPerTick;
				UpkeepTx.BalanceAfter = Aggregate;
				UpkeepTx.Reason = TEXT("Guard upkeep (deducted from faction members)");
				if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
				{
					UpkeepTx.GameTime = GS->GetAccumulatedTime();
				}
				TransactionLedger.Add(UpkeepTx);
				OnTransactionRecorded.Broadcast(UpkeepTx);
			}
		}

		if (bDebugTicks)
		{
			UE_LOG(LogTerritory, Log, TEXT("[EconomyTick] %s: aggregate=%d, income=%d, costs=%d, net=%d, members=%d, territories=%d"),
				*Faction.ToString(), Aggregate, Treasury.IncomePerTick,
				Treasury.CostsPerTick, NetIncome, Members.Num(), Treasury.TerritoryCount);

			if (Settings->IsDebugEnabled())
			{
				const FString Msg = FString::Printf(TEXT("[Economy] %s: $%d (+%d/-%d) [%d members]"),
					*Faction.ToString(), Aggregate, Treasury.IncomePerTick, Treasury.CostsPerTick, Members.Num());
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Msg);
			}
		}

		FTerritoryEconomySnapshot Snapshot;
		Snapshot.Treasury = Aggregate;
		Snapshot.TotalIncome = Treasury.IncomePerTick;
		Snapshot.TotalCosts = Treasury.CostsPerTick;
		Snapshot.TerritoryCount = Treasury.TerritoryCount;

		OnEconomyTickFired.Broadcast(Faction, Snapshot);
	}

	// Trim ledger once after all factions processed (not per-faction)
	while (TransactionLedger.Num() > MaxTransactionHistory)
	{
		TransactionLedger.RemoveAt(0);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Query API (Read-Only)
// ═══════════════════════════════════════════════════════════════════════════════

int32 UTerritoryEconomySubsystem::GetTreasury(const FGameplayTag& Faction) const
{
	// Faction wealth = aggregate of all online members' UInventoryComponent::Currency
	return GetFactionAggregateCurrency(Faction);
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
	if (!GetWorld()->GetAuthGameMode()) return;
	if (!Faction.IsValid() || PositiveAmount <= 0) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	// Distribute to faction members' UInventoryComponent::Currency
	TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);
	if (Members.Num() > 0)
	{
		int32 BaseShare = PositiveAmount / Members.Num();
		int32 Remainder = PositiveAmount - (BaseShare * Members.Num());

		for (int32 i = 0; i < Members.Num(); ++i)
		{
			ANarrativeCharacter* Member = Members[i];
			UNarrativeInventoryComponent* Inv = Member->GetInventoryComponent();
			if (!Inv) continue;

			int32 Share = BaseShare + (i == 0 ? Remainder : 0);
			Inv->AddCurrency(Share);
		}
	}

	// Record transaction
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = PositiveAmount;
	Tx.BalanceAfter = GetFactionAggregateCurrency(Faction);
	Tx.Reason = Reason;

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
		UE_LOG(LogTerritory, Log, TEXT("[Transaction] CREDIT %s: +%d (%s) aggregate=%d (members=%d)"),
			*Faction.ToString(), PositiveAmount, *Reason, Tx.BalanceAfter, Members.Num());
	}
}

bool UTerritoryEconomySubsystem::TryDebitTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason, ETerritoryTransactionType Type)
{
	if (!GetWorld()->GetAuthGameMode()) return false;
	if (!Faction.IsValid() || PositiveAmount <= 0) return false;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	// Check aggregate faction wealth
	int32 Aggregate = GetFactionAggregateCurrency(Faction);
	if (Aggregate < PositiveAmount) return false;

	// Get faction members
	TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);
	if (Members.Num() == 0) return false;

	// Track actual debits per member for rollback if needed
	TArray<int32> ActualDebits;
	ActualDebits.Init(0, Members.Num());

	int32 DebitPerMember = PositiveAmount / Members.Num();
	int32 Remainder = PositiveAmount - (DebitPerMember * Members.Num());
	int32 Debited = 0;

	// First pass: attempt proportional debits
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		ANarrativeCharacter* Member = Members[i];
		UNarrativeInventoryComponent* Inv = Member->GetInventoryComponent();
		if (!Inv) continue;

		int32 MemberDebit = DebitPerMember + (i == 0 ? Remainder : 0);
		int32 CurrentCurrency = Inv->GetCurrency();

		// Clamp to what they can afford
		int32 ActualDebit = FMath::Min(MemberDebit, CurrentCurrency);
		if (ActualDebit > 0)
		{
			ActualDebits[i] = ActualDebit;
			Debited += ActualDebit;
		}
	}

	// Only succeed if we debited the full requested amount
	// If individual members couldn't afford their proportional share, fail the entire transaction
	if (Debited < PositiveAmount)
	{
		return false;
	}

	// Second pass: actually apply the debits
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		if (ActualDebits[i] > 0)
		{
			if (UNarrativeInventoryComponent* Inv = Members[i]->GetInventoryComponent())
			{
				Inv->AddCurrency(-ActualDebits[i]);
			}
		}
	}

	// Record transaction
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = -Debited;
	Tx.BalanceAfter = GetFactionAggregateCurrency(Faction);
	Tx.Reason = Reason;

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
		UE_LOG(LogTerritory, Log, TEXT("[Transaction] DEBIT %s: -%d (%s) aggregate=%d (members=%d)"),
			*Faction.ToString(), Debited, *Reason, Tx.BalanceAfter, Members.Num());
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
		// Only count leaf-level (Property) income to avoid hierarchy double-counting.
		// Cities and Districts are containers — their PeriodicIncome is metadata
		// for UI display, not a separate income source.
		if (Territory->IsA<ATerritoryProperty>())
		{
			const ATerritoryProperty* Property = Cast<const ATerritoryProperty>(Territory);
			Treasury.IncomePerTick += Property->GetEffectiveIncome();
		}

		// Only count guard costs for territories that are configured to spawn guards.
		// Use GuardSpawnCount (configured max) rather than GetSpawnedGuardCount() (currently alive)
		// to avoid undercounting upkeep for one tick after a guard wipe.
		if (Territory->GetConfiguredGuardCount() > 0)
		{
			Treasury.CostsPerTick += Territory->GetGuardCost();
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Event Handlers
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryEconomySubsystem::OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	// Mark factions dirty — actual recalculation deferred to next economy tick
	// to avoid O(3N) redundant scans per capture cascade.
	if (OldOwner.IsValid()) DirtyFactions.Add(OldOwner);
	if (NewOwner.IsValid()) DirtyFactions.Add(NewOwner);
}

void UTerritoryEconomySubsystem::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (!Territory || bWasUnregistered) return;

	// When a territory registers, bind its control-changed delegate
	Territory->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);

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
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);

	// Recalculate income for the owning faction (territory removed from their count)
	FGameplayTag Owner = Territory->GetOwningFaction();
	if (Owner.IsValid())
	{
		RecalculateIncome(Owner);
	}
}
