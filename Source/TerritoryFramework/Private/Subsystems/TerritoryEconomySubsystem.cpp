#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryWorldState.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Items/InventoryComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
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
	SharedFactionAccounts.Empty();
	FactionLeaderAccounts.Empty();
	Super::Deinitialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Faction Member Bridge — reads/writes NarrativePro UInventoryComponent::Currency
// ═══════════════════════════════════════════════════════════════════════════════

bool UTerritoryEconomySubsystem::IsAutomaticPlayerCurrencyAccount(
	const AActor* AccountActor)
{
	const ANarrativePlayerCharacter* PlayerCharacter =
		Cast<ANarrativePlayerCharacter>(AccountActor);
	return PlayerCharacter && PlayerCharacter->GetInventoryComponent();
}

bool UTerritoryEconomySubsystem::IsExplicitAccountSelectionValid(
	const AActor* RegisteredAccount, const AActor* PreferredBeneficiary)
{
	return RegisteredAccount
		&& (!PreferredBeneficiary || PreferredBeneficiary == RegisteredAccount);
}

TArray<ANarrativePlayerCharacter*> UTerritoryEconomySubsystem::GetOnlineFactionPlayers(
	const FGameplayTag& Faction) const
{
	TArray<ANarrativePlayerCharacter*> Players;
	if (!Faction.IsValid()) return Players;

	UWorld* World = GetWorld();
	if (!World) return Players;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		ANarrativePlayerState* PS = PC->GetPlayerState<ANarrativePlayerState>();
		if (!PS || !PS->GetFactions().HasTagExact(Faction)) continue;

		ANarrativePlayerCharacter* PlayerCharacter = nullptr;
		if (const ANarrativePlayerController* NarrativePC =
			Cast<ANarrativePlayerController>(PC))
		{
			PlayerCharacter = NarrativePC->GetOwnedCharacter();
		}
		if (!PlayerCharacter)
		{
			PlayerCharacter = Cast<ANarrativePlayerCharacter>(PC->GetPawn());
		}
		if (IsAutomaticPlayerCurrencyAccount(PlayerCharacter))
		{
			Players.AddUnique(PlayerCharacter);
		}
	}

	// Make remainder distribution and multi-player debits deterministic.
	Players.Sort([](const ANarrativePlayerCharacter& A,
		const ANarrativePlayerCharacter& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	return Players;
}

TArray<AActor*> UTerritoryEconomySubsystem::ResolvePeriodicSettlementAccounts(
	const FGameplayTag& Faction, ETerritoryIncomePayoutPolicy Policy) const
{
	TArray<AActor*> Accounts;
	if (Policy == ETerritoryIncomePayoutPolicy::SharedNarrativeAccount
		|| Policy == ETerritoryIncomePayoutPolicy::FactionLeader)
	{
		if (AActor* Registered = ResolveRegisteredCurrencyAccount(Faction, Policy))
		{
			Accounts.Add(Registered);
		}
		return Accounts;
	}

	for (ANarrativePlayerCharacter* Player : GetOnlineFactionPlayers(Faction))
	{
		Accounts.Add(Player);
	}
	return Accounts;
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

bool UTerritoryEconomySubsystem::DoesAccountBelongToFaction(
	const AActor* AccountActor, const FGameplayTag& Faction) const
{
	if (!AccountActor || !Faction.IsValid()) return false;
	if (const APlayerController* Controller = Cast<APlayerController>(AccountActor))
	{
		AccountActor = Controller->GetPawn();
	}
	const INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(AccountActor);
	return TeamAgent && TeamAgent->GetFactions().HasTagExact(Faction);
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

		// Income and upkeep share one explicit settlement cohort. The default cohort
		// is online players only; guards can never become an accidental wallet.
		const TArray<AActor*> SettlementAccounts =
			ResolvePeriodicSettlementAccounts(Faction, IncomePayoutPolicy);

		if (TickTreasury.IncomePerTick > 0)
		{
			CreditCurrencyToFaction(Faction, TickTreasury.IncomePerTick, IncomePayoutPolicy,
				TEXT("Periodic income"), ETerritoryTransactionType::Income);
		}

		int64 AvailableForUpkeep = 0;
		for (const AActor* Account : SettlementAccounts)
		{
			if (const UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Account))
			{
				AvailableForUpkeep += FMath::Max(0, Inventory->GetCurrency());
			}
		}
		const int32 ActualUpkeep = static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(TickTreasury.CostsPerTick), AvailableForUpkeep));
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
			const bool bDebitSucceeded = TryDebitSettlementAccounts(
				Faction, ActualUpkeep, IncomePayoutPolicy, Reason,
				ETerritoryTransactionType::GuardUpkeep);
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
		const int32 MemberCount = SettlementAccounts.Num();

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
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || PositiveAmount <= 0
		|| !DoesAccountBelongToFaction(RequestingActor, Faction)) return false;

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
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || PositiveAmount <= 0
		|| !DoesAccountBelongToFaction(Beneficiary, Faction)) return false;

	UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Beneficiary);
	if (!Inventory) return false;
	const int64 FinalBalance = static_cast<int64>(Inventory->GetCurrency()) + PositiveAmount;
	if (FinalBalance > MAX_int32) return false;

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

	if (Policy == ETerritoryIncomePayoutPolicy::CapturingPlayer)
	{
		if (PreferredBeneficiary
			&& CreditCurrency(PreferredBeneficiary, PositiveAmount, Faction, Reason, Type))
		{
			return PositiveAmount;
		}
		UE_LOG(LogTerritory, Warning,
			TEXT("CreditCurrencyToFaction: CapturingPlayer has no valid beneficiary; falling back to deterministic member split for %s"),
			*Faction.ToString());
	}
	else if (Policy == ETerritoryIncomePayoutPolicy::SharedNarrativeAccount
		|| Policy == ETerritoryIncomePayoutPolicy::FactionLeader)
	{
		AActor* ExplicitAccount = ResolveRegisteredCurrencyAccount(Faction, Policy);
		if (IsExplicitAccountSelectionValid(ExplicitAccount, PreferredBeneficiary)
			&& CreditCurrency(ExplicitAccount, PositiveAmount, Faction, Reason, Type))
		{
			return PositiveAmount;
		}
		UE_LOG(LogTerritory, Warning,
			TEXT("CreditCurrencyToFaction: policy %d has no valid matching registered Narrative account for %s; payout rejected"),
			static_cast<int32>(Policy), *Faction.ToString());
		return 0;
	}

	TArray<ANarrativePlayerCharacter*> Players = GetOnlineFactionPlayers(Faction);
	if (Players.IsEmpty()) return 0;

	int32 Remaining = PositiveAmount;
	int32 Paid = 0;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		UNarrativeInventoryComponent* Inventory = Players[Index]->GetInventoryComponent();
		const int64 Capacity = Inventory
			? static_cast<int64>(MAX_int32) - FMath::Max(0, Inventory->GetCurrency())
			: 0;
		const int32 AccountsRemaining = Players.Num() - Index;
		const int32 FairShare = AccountsRemaining > 0
			? FMath::DivideAndRoundUp(Remaining, AccountsRemaining) : 0;
		const int32 Share = static_cast<int32>(FMath::Min<int64>(FairShare, Capacity));
		if (Share > 0 && CreditCurrency(Players[Index], Share, Faction, Reason, Type))
		{
			Paid += Share;
			Remaining -= Share;
		}
	}
	if (Remaining > 0)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("CreditCurrencyToFaction: %d currency could not be stored because every online Narrative player account is at capacity for %s"),
			Remaining, *Faction.ToString());
	}
	return Paid;
}

bool UTerritoryEconomySubsystem::RegisterFactionCurrencyAccount(
	const FGameplayTag& Faction, ETerritoryIncomePayoutPolicy AccountRole, AActor* AccountActor)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Faction.IsValid()
		|| !AccountActor || AccountActor->GetWorld() != World
		|| !ResolveCurrencyAccount(AccountActor)
		|| !DoesAccountBelongToFaction(AccountActor, Faction))
	{
		return false;
	}

	if (AccountRole == ETerritoryIncomePayoutPolicy::SharedNarrativeAccount)
	{
		SharedFactionAccounts.Add(Faction, AccountActor);
		return true;
	}
	if (AccountRole == ETerritoryIncomePayoutPolicy::FactionLeader)
	{
		FactionLeaderAccounts.Add(Faction, AccountActor);
		return true;
	}
	return false;
}

void UTerritoryEconomySubsystem::UnregisterFactionCurrencyAccount(
	const FGameplayTag& Faction, AActor* AccountActor)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Faction.IsValid()) return;

	auto RemoveMatchingAccount = [&Faction, AccountActor](TMap<FGameplayTag, TWeakObjectPtr<AActor>>& Accounts)
	{
		if (const TWeakObjectPtr<AActor>* Existing = Accounts.Find(Faction))
		{
			if (!AccountActor || Existing->Get() == AccountActor)
			{
				Accounts.Remove(Faction);
			}
		}
	};
	RemoveMatchingAccount(SharedFactionAccounts);
	RemoveMatchingAccount(FactionLeaderAccounts);
}

AActor* UTerritoryEconomySubsystem::ResolveRegisteredCurrencyAccount(
	const FGameplayTag& Faction, ETerritoryIncomePayoutPolicy AccountRole) const
{
	const TMap<FGameplayTag, TWeakObjectPtr<AActor>>* Accounts = nullptr;
	if (AccountRole == ETerritoryIncomePayoutPolicy::SharedNarrativeAccount)
	{
		Accounts = &SharedFactionAccounts;
	}
	else if (AccountRole == ETerritoryIncomePayoutPolicy::FactionLeader)
	{
		Accounts = &FactionLeaderAccounts;
	}
	if (!Accounts) return nullptr;

	const TWeakObjectPtr<AActor>* Account = Accounts->Find(Faction);
	AActor* Actor = Account ? Account->Get() : nullptr;
	UWorld* World = GetWorld();
	return Actor && World && Actor->GetWorld() == World
		&& ResolveCurrencyAccount(Actor)
		&& DoesAccountBelongToFaction(Actor, Faction)
		? Actor : nullptr;
}

bool UTerritoryEconomySubsystem::TryDebitSettlementAccounts(
	const FGameplayTag& Faction, int32 PositiveAmount,
	ETerritoryIncomePayoutPolicy Policy, const FString& Reason,
	ETerritoryTransactionType Type)
{
	if (!Faction.IsValid() || PositiveAmount <= 0) return false;

	TArray<AActor*> Accounts = ResolvePeriodicSettlementAccounts(Faction, Policy);
	TArray<int32> Debits;
	Debits.Init(0, Accounts.Num());

	int64 TotalCurrency = 0;
	for (AActor* Account : Accounts)
	{
		if (const UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Account))
		{
			TotalCurrency += FMath::Max(0, Inventory->GetCurrency());
		}
	}
	if (TotalCurrency < static_cast<int64>(PositiveAmount)) return false;

	int32 Remaining = PositiveAmount;
	for (int32 Index = 0; Index < Accounts.Num() && Remaining > 0; ++Index)
	{
		if (const UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Accounts[Index]))
		{
			Debits[Index] = FMath::Min(Inventory->GetCurrency(), Remaining);
			Remaining -= Debits[Index];
		}
	}
	if (Remaining > 0) return false;

	for (int32 Index = 0; Index < Accounts.Num(); ++Index)
	{
		if (Debits[Index] <= 0) continue;
		if (UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Accounts[Index]))
		{
			Inventory->AddCurrency(-Debits[Index]);
			RecordCurrencyTransaction(Faction, -Debits[Index], Inventory->GetCurrency(), Reason, Type, Accounts[Index]);
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
	if (!World) return;

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
	if (!World) return;

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

	int64 TotalIncome = 0;
	int64 TotalCosts = 0;
	Treasury.TerritoryCount = Territories.Num();

	for (const ATerritoryVolume* Territory : Territories)
	{
		// Only count leaf-level (Property) income to avoid hierarchy double-counting.
		// Cities and Districts are containers — their PeriodicIncome is metadata
		// for UI display, not a separate income source.
		if (Territory->IsA<ATerritoryProperty>())
		{
			const ATerritoryProperty* Property = Cast<const ATerritoryProperty>(Territory);
			TotalIncome += static_cast<int64>(Property->GetEffectiveIncome());
		}

		// GuardCost is the per-guard upkeep. Use the persistent desired garrison
		// rather than the configured maximum so add/remove commands affect finances.
		const int64 GuardUpkeep = static_cast<int64>(FMath::Max(0, Territory->GetGuardCost()))
			* static_cast<int64>(Territory->GetDesiredGuardCount());
		TotalCosts += GuardUpkeep;
	}
	Treasury.IncomePerTick = static_cast<int32>(FMath::Clamp<int64>(TotalIncome, 0, MAX_int32));
	Treasury.CostsPerTick = static_cast<int32>(FMath::Clamp<int64>(TotalCosts, 0, MAX_int32));

	// WorldState owns the replicated/late-join economy read model. Publish every
	// recalculation so staffing changes do not wait for the next payout interval.
	for (TActorIterator<ATerritoryWorldState> It(World); It; ++It)
	{
		It->SetFactionTreasury(Faction, Treasury);
		break;
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
