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
	FactionGold.Empty();
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

	TArray<FGameplayTag> Factions;
	FactionTreasuries.GetKeys(Factions);
	for (const FGameplayTag& Faction : Factions)
	{
		const FTerritoryTreasury* Treasury = FactionTreasuries.Find(Faction);
		if (!Treasury) continue;
		const FTerritoryTreasury TickTreasury = *Treasury;

		if (TickTreasury.IncomePerTick > 0)
		{
			AddToTreasury(Faction, TickTreasury.IncomePerTick,
				TEXT("Periodic income"), ETerritoryTransactionType::Income);
		}

		const int32 AvailableForUpkeep = GetTreasury(Faction);
		const int32 ActualUpkeep = FMath::Min(TickTreasury.CostsPerTick, AvailableForUpkeep);
		if (ActualUpkeep > 0)
		{
			const FString Reason = ActualUpkeep == TickTreasury.CostsPerTick
				? TEXT("Guard upkeep")
				: FString::Printf(TEXT("Guard upkeep (partial: %d/%d)"), ActualUpkeep, TickTreasury.CostsPerTick);
			TryDebitTreasury(Faction, ActualUpkeep, Reason, ETerritoryTransactionType::GuardUpkeep);
		}

		const int32 Aggregate = GetFactionAggregateCurrency(Faction);
		const int32 NetIncome = TickTreasury.IncomePerTick - TickTreasury.CostsPerTick;
		const int32 MemberCount = GetFactionMembers(Faction).Num();

		if (bDebugTicks)
		{
			UE_LOG(LogTerritory, Log, TEXT("[EconomyTick] %s: aggregate=%d, income=%d, costs=%d, net=%d, members=%d, territories=%d"),
				*Faction.ToString(), Aggregate, TickTreasury.IncomePerTick,
				TickTreasury.CostsPerTick, NetIncome, MemberCount, TickTreasury.TerritoryCount);

			if (Settings->IsDebugEnabled())
			{
				const FString Msg = FString::Printf(TEXT("[Economy] %s: $%d (+%d/-%d) [%d members]"),
					*Faction.ToString(), Aggregate, TickTreasury.IncomePerTick, TickTreasury.CostsPerTick, MemberCount);
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Msg);
			}
		}

		FTerritoryEconomySnapshot Snapshot;
		Snapshot.Treasury = Aggregate;
		Snapshot.TotalIncome = TickTreasury.IncomePerTick;
		Snapshot.TotalCosts = TickTreasury.CostsPerTick;
		Snapshot.TerritoryCount = TickTreasury.TerritoryCount;

		OnEconomyTickFired.Broadcast(Faction, Snapshot);
	}

	// Trim ledger once after all factions processed (not per-faction)
	const int32 Excess = TransactionLedger.Num() - MaxTransactionHistory;
	if (Excess > 0)
	{
		TransactionLedger.RemoveAt(0, Excess);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Query API (Read-Only)
// ═══════════════════════════════════════════════════════════════════════════════

int32 UTerritoryEconomySubsystem::GetTreasury(const FGameplayTag& Faction) const
{
	// Faction wealth = dedicated faction gold + aggregate of all online members' Currency
	return FactionGold.FindRef(Faction) + GetFactionAggregateCurrency(Faction);
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
	if (!GetWorld() || !GetWorld()->GetAuthGameMode()) return;
	if (!Faction.IsValid() || PositiveAmount <= 0) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	// Add to dedicated faction gold balance (primary treasury)
	int32& Gold = FactionGold.FindOrAdd(Faction);
	Gold += PositiveAmount;

	// Also distribute to online members' UInventoryComponent::Currency so they see
	// the income in their personal inventory. This is a secondary effect — the
	// canonical treasury is FactionGold.
	TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);
	if (!Members.IsEmpty())
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
	Tx.BalanceAfter = GetTreasury(Faction);
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
		UE_LOG(LogTerritory, Log, TEXT("[Transaction] CREDIT %s: +%d (%s) treasury=%d, gold=%d, members=%d"),
			*Faction.ToString(), PositiveAmount, *Reason, Tx.BalanceAfter, Gold, Members.Num());
	}
}

bool UTerritoryEconomySubsystem::TryDebitTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason, ETerritoryTransactionType Type)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode()) return false;
	if (!Faction.IsValid() || PositiveAmount <= 0) return false;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugTx = Settings && Settings->ShouldDebugTransactions();

	// Check total treasury (FactionGold + member aggregate) before proceeding
	if (GetTreasury(Faction) < PositiveAmount) return false;

	int32& Gold = FactionGold.FindOrAdd(Faction);
	int32 Debited = 0;

	// ─── Phase 1: Collect — determine what to take from each source,
	// without modifying any balance yet ───

	// FactionGold contribution
	int32 FromGold = FMath::Min(Gold, PositiveAmount);
	int32 Remaining = PositiveAmount - FromGold;

	// Member contributions
	TArray<ANarrativeCharacter*> Members;
	TArray<int32> MemberDebits; // parallel array, 0 = no debit

	if (Remaining > 0)
	{
		Members = GetFactionMembers(Faction);
		MemberDebits.Init(0, Members.Num());

		int32 DebitPerMember = Remaining / FMath::Max(1, Members.Num());
		int32 Remainder = Remaining - (DebitPerMember * Members.Num());
		int32 MemberCollected = 0;

		// First pass: proportional debits
		for (int32 i = 0; i < Members.Num(); ++i)
		{
			ANarrativeCharacter* Member = Members[i];
			UNarrativeInventoryComponent* Inv = Member ? Member->GetInventoryComponent() : nullptr;
			if (!Inv) continue;

			int32 AttemptDebit = DebitPerMember + (i == 0 ? Remainder : 0);
			int32 CurrentCurrency = Inv->GetCurrency();
			int32 ActualDebit = FMath::Min(AttemptDebit, CurrentCurrency);
			if (ActualDebit > 0)
			{
				MemberDebits[i] = ActualDebit;
				MemberCollected += ActualDebit;
			}
		}

		// Second pass: redistribute shortfall
		int32 MemberRemaining = Remaining - MemberCollected;
		for (int32 i = 0; i < Members.Num() && MemberRemaining > 0; ++i)
		{
			if (UNarrativeInventoryComponent* Inv = Members[i]->GetInventoryComponent())
			{
				const int32 SpareCurrency = Inv->GetCurrency() - MemberDebits[i];
				const int32 AdditionalDebit = FMath::Min(SpareCurrency, MemberRemaining);
				MemberDebits[i] += AdditionalDebit;
				MemberCollected += AdditionalDebit;
				MemberRemaining -= AdditionalDebit;
			}
		}

		// If still short after members, abort without mutating anything
		if (MemberRemaining > 0)
		{
			return false;
		}

		Remaining = MemberRemaining; // 0
	}

	// ─── Phase 2: Apply — atomically apply all collected debits ───
	Gold -= FromGold;
	Debited += FromGold;

	for (int32 i = 0; i < Members.Num(); ++i)
	{
		if (MemberDebits[i] > 0)
		{
			if (UNarrativeInventoryComponent* Inv = Members[i]->GetInventoryComponent())
			{
				Inv->AddCurrency(-MemberDebits[i]);
				Debited += MemberDebits[i];
			}
		}
	}

	// Record transaction
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = -Debited;
	Tx.BalanceAfter = GetTreasury(Faction);
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
		UE_LOG(LogTerritory, Log, TEXT("[Transaction] DEBIT %s: -%d (%s) treasury=%d, gold=%d"),
			*Faction.ToString(), Debited, *Reason, Tx.BalanceAfter, Gold);
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

void UTerritoryEconomySubsystem::RestoreTransactionHistory(const TArray<FTerritoryTransaction>& Transactions)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;

	TransactionLedger = Transactions;
	if (TransactionLedger.Num() > MaxTransactionHistory)
	{
		TransactionLedger.RemoveAt(0, TransactionLedger.Num() - MaxTransactionHistory);
	}
}

void UTerritoryEconomySubsystem::RestoreTreasuryState(const TMap<FGameplayTag, FTerritoryTreasury>& Treasuries)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;

	FactionTreasuries = Treasuries;
	DirtyFactions.Empty();
}

void UTerritoryEconomySubsystem::RestoreFactionGold(const TMap<FGameplayTag, int32>& Gold)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;

	FactionGold = Gold;
}

void UTerritoryEconomySubsystem::SetFactionTreasury(const FGameplayTag& Faction, const FTerritoryTreasury& Treasury)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	if (!Faction.IsValid()) return;
	FactionTreasuries.Add(Faction, Treasury);
}

void UTerritoryEconomySubsystem::RecalculateIncome(const FGameplayTag& Faction)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	if (!Faction.IsValid()) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
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
