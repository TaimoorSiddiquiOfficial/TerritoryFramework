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
			// P2-09: Mark factions dirty instead of recalculating per-territory — batched on first economy tick.
			for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (Territory)
				{
					Territory->OnTerritoryOwnershipChanged.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);
					FGameplayTag Owner = Territory->GetOwningFaction();
					if (Owner.IsValid())
					{
						MarkFactionDirty(Owner);
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

UNarrativeInventoryComponent* UTerritoryEconomySubsystem::ResolveCurrencyAccount(const AActor* RequestingActor) const
{
	if (!RequestingActor) return nullptr;

	const AActor* AccountActor = RequestingActor;
	if (const APlayerController* Controller = Cast<APlayerController>(RequestingActor))
	{
		AccountActor = Controller->GetPawn();
	}

	if (!AccountActor) return nullptr;
	if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(AccountActor))
	{
		return Character->GetInventoryComponent();
	}

	return const_cast<AActor*>(AccountActor)->FindComponentByClass<UNarrativeInventoryComponent>();
}

int32 UTerritoryEconomySubsystem::GetActorCurrency(const AActor* RequestingActor) const
{
	if (const UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(RequestingActor))
	{
		return Inventory->GetCurrency();
	}
	return 0;
}

bool UTerritoryEconomySubsystem::CanActorAfford(const AActor* RequestingActor, int32 Cost) const
{
	return Cost >= 0 && GetActorCurrency(RequestingActor) >= Cost;
}

void UTerritoryEconomySubsystem::RecordCurrencyTransaction(
	const FGameplayTag& Faction, int32 Amount, int32 BalanceAfter,
	const FString& Reason, ETerritoryTransactionType Type, const AActor* AccountActor)
{
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = Amount;
	Tx.BalanceAfter = BalanceAfter;
	Tx.Reason = AccountActor
		? FString::Printf(TEXT("%s [Account=%s]"), *Reason, *AccountActor->GetName())
		: Reason;

	// P2-N12: Null guard on GetWorld to prevent crash during shutdown
	if (UWorld* W = GetWorld())
	{
		if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(W->GetGameState()))
		{
			Tx.GameTime = GS->GetAccumulatedTime();
		}
	}

	TransactionLedger.Add(Tx);
	// Trimming deferred to OnEconomyTick batch trim — avoids per-insert O(N) shift
	OnTransactionRecorded.Broadcast(Tx);
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

		// P2-N10: Cache faction members once per tick instead of 3 separate GetFactionMembers calls
		const TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);

		if (TickTreasury.IncomePerTick > 0)
		{
			CreditCurrencyToFaction(Faction, TickTreasury.IncomePerTick, IncomePayoutPolicy,
				TEXT("Periodic income"), ETerritoryTransactionType::Income);
		}

		int32 AvailableForUpkeep = 0;
		for (const ANarrativeCharacter* Member : Members)
		{
			if (const UNarrativeInventoryComponent* Inventory = Member->GetInventoryComponent())
			{
				AvailableForUpkeep += Inventory->GetCurrency();
			}
		}
		const int32 ActualUpkeep = FMath::Min(TickTreasury.CostsPerTick, AvailableForUpkeep);
		const bool bUpkeepFullyPaid = (ActualUpkeep >= TickTreasury.CostsPerTick);
		bool bDeficitAlreadyBroadcast = false;
		if (ActualUpkeep > 0)
		{
			const FString Reason = bUpkeepFullyPaid
				? TEXT("Guard upkeep")
				: FString::Printf(TEXT("Guard upkeep (partial: %d/%d)"), ActualUpkeep, TickTreasury.CostsPerTick);
			// P1-N15: TOCTOU fix — the AvailableForUpkeep scan above is a snapshot. Between
			// that scan and the actual debit, another system may have drained the same funds.
			// If TryDebitFactionMembers returns false despite ActualUpkeep > 0, treat as full
			// deficit and broadcast accordingly.
			// P2-N10: TryDebitFactionMembers calls GetFactionMembers internally (separate function scope)
			const bool bDebitSucceeded = TryDebitFactionMembers(Faction, ActualUpkeep, Reason, ETerritoryTransactionType::GuardUpkeep);
			if (!bDebitSucceeded && TickTreasury.CostsPerTick > 0)
			{
				UE_LOG(LogTerritory, Warning, TEXT("[EconomyTick] %s TOCTOU: debit failed despite %d available — treating as full deficit"),
					*Faction.ToString(), TickTreasury.CostsPerTick);
				OnFactionUpkeepDeficit.Broadcast(Faction, TickTreasury.CostsPerTick);
				bDeficitAlreadyBroadcast = true;
			}
		}

		// Upkeep consequence: when a faction can't pay full upkeep, broadcast deficit
		// so territories can suspend reserve respawns or reduce desired guard count.
		// P2-06/P1-09: Only broadcast if we didn't already broadcast from TOCTOU path above.
		if (!bUpkeepFullyPaid && TickTreasury.CostsPerTick > 0 && !bDeficitAlreadyBroadcast)
		{
			const int32 Deficit = TickTreasury.CostsPerTick - ActualUpkeep;
			UE_LOG(LogTerritory, Warning, TEXT("[EconomyTick] %s has upkeep deficit: paid %d/%d (short %d) — reserves may be suspended"),
				*Faction.ToString(), ActualUpkeep, TickTreasury.CostsPerTick, Deficit);
			OnFactionUpkeepDeficit.Broadcast(Faction, Deficit);
		}

		const int32 NetIncome = TickTreasury.IncomePerTick - TickTreasury.CostsPerTick;
		// P2-N10: Use cached Members instead of calling GetFactionMembers again
		const int32 MemberCount = Members.Num();

		if (bDebugTicks)
		{
			UE_LOG(LogTerritory, Log, TEXT("[EconomyTick] %s: income=%d, costs=%d, net=%d, members=%d, territories=%d"),
				*Faction.ToString(), TickTreasury.IncomePerTick,
				TickTreasury.CostsPerTick, NetIncome, MemberCount, TickTreasury.TerritoryCount);

			if (Settings->IsDebugEnabled())
			{
				const FString Msg = FString::Printf(TEXT("[Economy] %s: +%d/-%d [%d members]"),
					*Faction.ToString(), TickTreasury.IncomePerTick, TickTreasury.CostsPerTick, MemberCount);
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Msg);
			}
		}

		FTerritoryEconomySnapshot Snapshot;
		// TerritoryFramework owns rates; Narrative inventory accounts own currency.
		Snapshot.Treasury = 0;
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
	// Deprecated: TerritoryFramework no longer exposes a faction wallet.
	return 0;
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
	// Deprecated: callers must provide an exact Narrative inventory account.
	return Cost == 0;
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
	UE_LOG(LogTerritory, Warning, TEXT("AddToTreasury is deprecated and ignored for faction %s: %s"),
		*Faction.ToString(), *Reason);
}

bool UTerritoryEconomySubsystem::TryDebitTreasury(const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason, ETerritoryTransactionType Type)
{
	UE_LOG(LogTerritory, Warning, TEXT("TryDebitTreasury is deprecated and rejected for faction %s: %s"),
		*Faction.ToString(), *Reason);
	return false;
}

bool UTerritoryEconomySubsystem::TryDebitCurrency(
	AActor* RequestingActor, int32 PositiveAmount, const FGameplayTag& Faction,
	const FString& Reason, ETerritoryTransactionType Type)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || PositiveAmount <= 0) return false;

	UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(RequestingActor);
	if (!Inventory || Inventory->GetCurrency() < PositiveAmount) return false;

	Inventory->AddCurrency(-PositiveAmount);
	RecordCurrencyTransaction(Faction, -PositiveAmount, Inventory->GetCurrency(), Reason, Type, RequestingActor);
	return true;
}

bool UTerritoryEconomySubsystem::CreditCurrency(
	AActor* Beneficiary, int32 PositiveAmount, const FGameplayTag& Faction,
	const FString& Reason, ETerritoryTransactionType Type)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || PositiveAmount <= 0) return false;

	UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Beneficiary);
	if (!Inventory) return false;

	Inventory->AddCurrency(PositiveAmount);
	RecordCurrencyTransaction(Faction, PositiveAmount, Inventory->GetCurrency(), Reason, Type, Beneficiary);
	return true;
}

int32 UTerritoryEconomySubsystem::CreditCurrencyToFaction(
	const FGameplayTag& Faction, int32 PositiveAmount, ETerritoryIncomePayoutPolicy Policy,
	const FString& Reason, ETerritoryTransactionType Type, AActor* PreferredBeneficiary)
{
	if (!Faction.IsValid() || PositiveAmount <= 0 || Policy == ETerritoryIncomePayoutPolicy::NoCurrencyPayout)
	{
		return 0;
	}

	if (Policy == ETerritoryIncomePayoutPolicy::CapturingPlayer
		|| Policy == ETerritoryIncomePayoutPolicy::SharedNarrativeAccount)
	{
		return PreferredBeneficiary
			&& CreditCurrency(PreferredBeneficiary, PositiveAmount, Faction, Reason, Type)
			? PositiveAmount
			: 0;
	}

	TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);
	if (Members.IsEmpty()) return 0;

	if (Policy == ETerritoryIncomePayoutPolicy::FactionLeader)
	{
		// P2-N11: FactionLeader is a placeholder — no actual leader resolution exists.
		// Fall through to equal split to avoid dropping income.
		UE_LOG(LogTerritory, Warning, TEXT("CreditCurrencyToFaction: FactionLeader fallback — distributing equally among %d members"),
			Members.Num());
	}

	const int32 BaseShare = PositiveAmount / Members.Num();
	const int32 Remainder = PositiveAmount - (BaseShare * Members.Num());
	int32 Paid = 0;
	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		const int32 Share = BaseShare + (Index == 0 ? Remainder : 0);
		if (Share > 0 && CreditCurrency(Members[Index], Share, Faction, Reason, Type))
		{
			Paid += Share;
		}
	}
	return Paid;
}

bool UTerritoryEconomySubsystem::TryDebitFactionMembers(
	const FGameplayTag& Faction, int32 PositiveAmount, const FString& Reason,
	ETerritoryTransactionType Type)
{
	if (!Faction.IsValid() || PositiveAmount <= 0) return false;

	TArray<ANarrativeCharacter*> Members = GetFactionMembers(Faction);
	TArray<int32> Debits;
	Debits.Init(0, Members.Num());

	int32 TotalCurrency = 0;
	for (ANarrativeCharacter* Member : Members)
	{
		if (const UNarrativeInventoryComponent* Inventory = Member->GetInventoryComponent())
		{
			TotalCurrency += Inventory->GetCurrency();
		}
	}
	if (TotalCurrency < PositiveAmount) return false;

	int32 Remaining = PositiveAmount;
	for (int32 Index = 0; Index < Members.Num() && Remaining > 0; ++Index)
	{
		if (const UNarrativeInventoryComponent* Inventory = Members[Index]->GetInventoryComponent())
		{
			Debits[Index] = FMath::Min(Inventory->GetCurrency(), Remaining);
			Remaining -= Debits[Index];
		}
	}
	if (Remaining > 0) return false;

	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		if (Debits[Index] <= 0) continue;
		if (UNarrativeInventoryComponent* Inventory = Members[Index]->GetInventoryComponent())
		{
			Inventory->AddCurrency(-Debits[Index]);
			RecordCurrencyTransaction(Faction, -Debits[Index], Inventory->GetCurrency(), Reason, Type, Members[Index]);
		}
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
	// Trim on restore to cap loaded data
	const int32 RestoreExcess = TransactionLedger.Num() - MaxTransactionHistory;
	if (RestoreExcess > 0)
	{
		TransactionLedger.RemoveAt(0, RestoreExcess);
	}
}

void UTerritoryEconomySubsystem::RestoreTreasuryState(const TMap<FGameplayTag, FTerritoryTreasury>& Treasuries)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;

	FactionTreasuries = Treasuries;
	DirtyFactions.Empty();
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

		// GuardCost is the per-guard upkeep. Use the persistent desired garrison
		// rather than the configured maximum so add/remove commands affect finances.
		const int64 GuardUpkeep = static_cast<int64>(FMath::Max(0, Territory->GetGuardCost()))
			* static_cast<int64>(Territory->GetDesiredGuardCount());
		Treasury.CostsPerTick = FMath::Clamp<int64>(
			static_cast<int64>(Treasury.CostsPerTick) + GuardUpkeep,
			0,
			MAX_int32);
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

	// P0-01: Economy is server-authoritative. On clients, registration populates the
	// registry for lookups only — no income mutation or delegate binding.
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Client) return;

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
