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
#include "Items/NarrativeItem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
const AActor* ResolveNarrativeAccountActor(const AActor* Actor)
{
	if (const ANarrativePlayerController* Controller = Cast<ANarrativePlayerController>(Actor))
	{
		if (IsValid(Controller->GetOwnedCharacter())) return Controller->GetOwnedCharacter();
	}
	if (const APlayerController* Controller = Cast<APlayerController>(Actor)) return Controller->GetPawn();
	return Actor;
}

bool IsProductionSiteAvailable(UWorld* World,
	const FTerritoryProductionSiteRecord& Site)
{
	if (!World || Site.Availability == ETerritoryAvailability::Locked) return false;
	FGameplayTag ParentTag = Site.ParentTerritoryTag;
	if (!ParentTag.IsValid()) return true;

	const UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	const ATerritoryWorldState* WorldState =
		ATerritoryWorldState::FindTerritoryWorldState(World);

	TSet<FGameplayTag> Visited;
	if (Site.TerritoryTag.IsValid()) Visited.Add(Site.TerritoryTag);
	while (ParentTag.IsValid())
	{
		if (Visited.Contains(ParentTag)) return false;
		Visited.Add(ParentTag);
		if (const ATerritoryVolume* Parent = Registry
			? Registry->GetTerritoryByTag(ParentTag) : nullptr)
		{
			if (Parent->IsLocked()) return false;
			ParentTag = Parent->GetParentTerritoryTag();
			continue;
		}
		if (!WorldState) return false;
		const FReplicatedCaptureSummary Summary =
			WorldState->GetCaptureSummary(ParentTag);
		if (Summary.TerritoryTag != ParentTag
			|| Summary.Availability == ETerritoryAvailability::Locked)
		{
			return false;
		}
		ParentTag = Summary.ParentTerritoryTag;
	}
	return true;
}

bool BuildScaledAmounts(const TArray<FTerritoryResourceRate>& Rates,
	int32 UpgradeLevel, int32 BatchCount,
	TArray<FTerritoryResourceAmount>& OutAmounts, FText& OutFailureReason)
{
	TMap<UClass*, int64> Totals;
	for (const FTerritoryResourceRate& Rate : Rates)
	{
		int32 Quantity = 0;
		if (!UTerritoryProductionProfile::CalculateScaledQuantity(
			Rate, UpgradeLevel, BatchCount, Quantity))
		{
			OutFailureReason = NSLOCTEXT("TerritoryProduction", "InvalidScaledQuantity",
				"A production quantity is invalid or exceeds the supported integer range.");
			return false;
		}
		int64& Total = Totals.FindOrAdd(Rate.ItemClass.Get());
		Total += Quantity;
		if (Total > MAX_int32)
		{
			OutFailureReason = NSLOCTEXT("TerritoryProduction", "CombinedQuantityOverflow",
				"Combined production quantity exceeds the supported integer range.");
			return false;
		}
	}

	TArray<UClass*> Classes;
	Totals.GetKeys(Classes);
	Classes.Sort([](const UClass& A, const UClass& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	for (UClass* ItemClass : Classes)
	{
		FTerritoryResourceAmount Amount;
		Amount.ItemClass = ItemClass;
		Amount.Quantity = static_cast<int32>(Totals.FindRef(ItemClass));
		OutAmounts.Add(Amount);
	}
	return true;
}

int32 ConsumeExactItems(UNarrativeInventoryComponent* Inventory,
	TSubclassOf<UNarrativeItem> ItemClass, int32 Quantity)
{
	if (!Inventory || !ItemClass || Quantity <= 0) return 0;
	int32 Remaining = Quantity;
	for (UNarrativeItem* Item : Inventory->FindItemsByClass(
		TSoftClassPtr<UNarrativeItem>(ItemClass), false))
	{
		if (!Item || Remaining <= 0) continue;
		Remaining -= Inventory->ConsumeItem(Item, Remaining);
	}
	return Quantity - Remaining;
}

int32 AddExactItems(UNarrativeInventoryComponent* Inventory,
	TSubclassOf<UNarrativeItem> ItemClass, int32 Quantity)
{
	if (!Inventory || !ItemClass || Quantity <= 0) return 0;

	int32 Remaining = Quantity;
	// Narrative's public GetSpaceForItem rejects a full-slot inventory before
	// considering an existing partial stack. Fill exact stacks first so a valid
	// production result is not rejected by that ordering.
	for (UNarrativeItem* Item : Inventory->FindItemsByClass(
		TSoftClassPtr<UNarrativeItem>(ItemClass), false))
	{
		if (!Item || Remaining <= 0) continue;
		const int32 Added = FMath::Min(Remaining, Item->GetStackSpace());
		if (Added > 0)
		{
			Item->SetQuantity(Item->GetQuantity() + Added);
			Remaining -= Added;
		}
	}

	if (Remaining > 0)
	{
		const FItemAddResult AddResult = Inventory->TryAddItemFromClass(
			ItemClass, Remaining, false);
		Remaining -= AddResult.AmountGiven;
	}
	return Quantity - Remaining;
}

bool CanApplyResourceTransaction(UNarrativeInventoryComponent* Inventory,
	const TArray<FTerritoryResourceAmount>& Inputs,
	const TArray<FTerritoryResourceAmount>& Outputs,
	ETerritoryProductionStatus& OutStatus, FText& OutFailureReason)
{
	if (!Inventory)
	{
		OutStatus = ETerritoryProductionStatus::StorageUnavailable;
		OutFailureReason = NSLOCTEXT("TerritoryProduction", "StorageUnavailable",
			"No Narrative resource inventory is registered for this faction.");
		return false;
	}

	struct FSimulatedStack
	{
		UClass* ItemClass = nullptr;
		int32 Quantity = 0;
		int32 Maximum = 1;
		float UnitWeight = 0.f;
		bool bCanRemove = true;
	};

	TArray<FSimulatedStack> Stacks;
	for (UNarrativeItem* Item : Inventory->GetItems())
	{
		if (!Item) continue;
		FSimulatedStack& Stack = Stacks.AddDefaulted_GetRef();
		Stack.ItemClass = Item->GetClass();
		Stack.Quantity = Item->GetQuantity();
		Stack.Maximum = Item->GetMaxStackSize();
		Stack.UnitWeight = Item->Weight;
		Stack.bCanRemove = Item->CanBeRemoved();
	}

	int32 OccupiedSlots = Stacks.Num();
	double SimulatedWeight = Inventory->GetCurrentWeight();
	for (const FTerritoryResourceAmount& Input : Inputs)
	{
		int32 Remaining = Input.Quantity;
		for (FSimulatedStack& Stack : Stacks)
		{
			if (Stack.ItemClass != Input.ItemClass.Get() || Stack.Quantity <= 0) continue;
			if (!Stack.bCanRemove)
			{
				OutStatus = ETerritoryProductionStatus::MissingInput;
				OutFailureReason = NSLOCTEXT("TerritoryProduction", "InputNotRemovable",
					"A required Narrative item cannot be removed from storage.");
				return false;
			}
			const int32 Removed = FMath::Min(Remaining, Stack.Quantity);
			Stack.Quantity -= Removed;
			Remaining -= Removed;
			SimulatedWeight -= static_cast<double>(Removed) * Stack.UnitWeight;
			if (Stack.Quantity == 0) --OccupiedSlots;
			if (Remaining == 0) break;
		}
		if (Remaining > 0)
		{
			OutStatus = ETerritoryProductionStatus::MissingInput;
			OutFailureReason = NSLOCTEXT("TerritoryProduction", "MissingInput",
				"The faction resource inventory does not contain every required input.");
			return false;
		}
	}

	for (const FTerritoryResourceAmount& Output : Outputs)
	{
		const UNarrativeItem* CDO = Output.ItemClass
			? GetDefault<UNarrativeItem>(Output.ItemClass) : nullptr;
		if (!CDO)
		{
			OutStatus = ETerritoryProductionStatus::InvalidProfile;
			OutFailureReason = NSLOCTEXT("TerritoryProduction", "InvalidOutputClass",
				"A production output does not resolve to a Narrative item class.");
			return false;
		}

		SimulatedWeight += static_cast<double>(Output.Quantity) * CDO->Weight;
		if (SimulatedWeight > static_cast<double>(Inventory->GetWeightCapacity()) + KINDA_SMALL_NUMBER)
		{
			OutStatus = ETerritoryProductionStatus::StorageFull;
			OutFailureReason = NSLOCTEXT("TerritoryProduction", "StorageWeightFull",
				"The faction resource inventory has insufficient weight capacity.");
			return false;
		}

		int32 Remaining = Output.Quantity;
		for (FSimulatedStack& Stack : Stacks)
		{
			if (Stack.ItemClass != Output.ItemClass.Get() || Stack.Quantity <= 0) continue;
			const int32 Added = FMath::Min(Remaining, Stack.Maximum - Stack.Quantity);
			Stack.Quantity += Added;
			Remaining -= Added;
			if (Remaining == 0) break;
		}

		const int32 MaxStackSize = FMath::Max(1, CDO->GetMaxStackSize());
		while (Remaining > 0)
		{
			if (OccupiedSlots >= Inventory->GetCapacity())
			{
				OutStatus = ETerritoryProductionStatus::StorageFull;
				OutFailureReason = NSLOCTEXT("TerritoryProduction", "StorageSlotsFull",
					"The faction resource inventory has insufficient item slots.");
				return false;
			}
			FSimulatedStack& NewStack = Stacks.AddDefaulted_GetRef();
			NewStack.ItemClass = Output.ItemClass.Get();
			NewStack.Maximum = MaxStackSize;
			NewStack.Quantity = FMath::Min(Remaining, MaxStackSize);
			NewStack.UnitWeight = CDO->Weight;
			Remaining -= NewStack.Quantity;
			++OccupiedSlots;
		}
	}
	return true;
}
}

void UTerritoryEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Production binds registration and ownership delegates immediately below.
	Collection.InitializeDependency<UTerritoryRegistrySubsystem>();

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
					Territory->OnTerritoryStateChangedDelegate.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryStateChanged);
					Territory->OnTerritoryAvailabilityChanged.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryAvailabilityChanged);
					if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
					{
						RefreshProductionSite(Property);
					}
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

		const float ObservationInterval = Settings
			? FMath::Max(0.1f, Settings->ProductionCycleObservationIntervalSeconds)
			: 1.f;
		World->GetTimerManager().SetTimer(
			ProductionCycleObservationTimerHandle,
			this,
			&UTerritoryEconomySubsystem::ObserveNarrativeProductionCycle,
			ObservationInterval,
			true);
	}

	if (Settings && Settings->ShouldDebugEconomy())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Economy] subsystem initialized (tick: %.0fs, server-only: %s)"),
			TickIntervalSeconds, World && World->GetNetMode() != NM_Client ? TEXT("true") : TEXT("false"));
	}
}

void UTerritoryEconomySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EconomyTickTimerHandle);
		World->GetTimerManager().ClearTimer(ProductionCycleObservationTimerHandle);

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
					Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryStateChanged);
					Territory->OnTerritoryAvailabilityChanged.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryAvailabilityChanged);
				}
			}
		}
	}

	FactionTreasuries.Empty();
	DirtyFactions.Empty();
	SharedFactionAccounts.Empty();
	FactionLeaderAccounts.Empty();
	FactionResourceAccounts.Empty();
	ProductionCheckpoints.Empty();
	ProductionSites.Empty();
	ResourceSnapshots.Empty();
	LastObservedProductionCycle = INDEX_NONE;
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
	if (!IsCurrencySettlementEnabled(Policy))
	{
		return Accounts;
	}
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
	if (Accounts.IsEmpty())
	{
		if (AActor* Fallback = ResolveFallbackFactionAccount(Faction))
		{
			Accounts.Add(Fallback);
		}
	}
	return Accounts;
}

UNarrativeInventoryComponent* UTerritoryEconomySubsystem::ResolveCurrencyAccount(const AActor* RequestingActor) const
{
	if (!RequestingActor) return nullptr;

	const AActor* AccountActor = ResolveNarrativeAccountActor(RequestingActor);
	if (!IsValid(AccountActor)) return nullptr;
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
	AccountActor = ResolveNarrativeAccountActor(AccountActor);
	if (!IsValid(AccountActor)) return false;
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
	const FString& Reason, ETerritoryTransactionType Type, const AActor* AccountActor,
	const FGameplayTag& SourceTerritory)
{
	FTerritoryTransaction Tx;
	Tx.TransactionID = FGuid::NewGuid();
	Tx.Faction = Faction;
	Tx.Type = Type;
	Tx.Amount = Amount;
	Tx.BalanceAfter = BalanceAfter;
	Tx.SourceTerritory = SourceTerritory;
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
		if (!IsCurrencySettlementEnabled(IncomePayoutPolicy))
		{
			FTerritoryEconomySnapshot Snapshot;
			Snapshot.Treasury = 0;
			Snapshot.TotalIncome = TickTreasury.IncomePerTick;
			Snapshot.TotalCosts = TickTreasury.CostsPerTick;
			Snapshot.TerritoryCount = TickTreasury.TerritoryCount;
			OnEconomyTickFired.Broadcast(Faction, Snapshot);
			continue;
		}

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

	ProcessResourceProduction();
}

void UTerritoryEconomySubsystem::ObserveNarrativeProductionCycle()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;

	const int64 CurrentCycle = GetCurrentProductionCycle();
	if (!HasProductionCycleAdvanced(LastObservedProductionCycle, CurrentCycle)) return;

	// ProcessResourceProduction records each Territory/rule/cycle checkpoint before
	// this observer or the slower economy tick can award it again.
	ProcessResourceProduction();
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
		|| !IsValid(RequestingActor) || RequestingActor->GetWorld() != GetWorld()
		|| !DoesAccountBelongToFaction(RequestingActor, Faction)) return false;

	UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(RequestingActor);
	if (!Inventory || !IsValid(Inventory->GetOwner()) || !Inventory->GetOwner()->HasAuthority()
		|| Inventory->GetWorld() != GetWorld() || Inventory->GetCurrency() < PositiveAmount) return false;

	Inventory->AddCurrency(-PositiveAmount);
	RecordCurrencyTransaction(Faction, -PositiveAmount, Inventory->GetCurrency(), Reason, Type, RequestingActor);
	return true;
}

bool UTerritoryEconomySubsystem::CreditCurrency(
	AActor* Beneficiary, int32 PositiveAmount, const FGameplayTag& Faction,
	const FString& Reason, ETerritoryTransactionType Type)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || PositiveAmount <= 0
		|| !IsValid(Beneficiary) || Beneficiary->GetWorld() != GetWorld()
		|| !DoesAccountBelongToFaction(Beneficiary, Faction)) return false;

	UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(Beneficiary);
	if (!Inventory || !IsValid(Inventory->GetOwner()) || !Inventory->GetOwner()->HasAuthority()
		|| Inventory->GetWorld() != GetWorld()) return false;
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
	if (!Faction.IsValid() || PositiveAmount <= 0 || !IsCurrencySettlementEnabled(Policy))
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
	if (Players.IsEmpty())
	{
		if (AActor* Fallback = ResolveFallbackFactionAccount(Faction))
		{
			return CreditCurrency(Fallback, PositiveAmount, Faction, Reason, Type)
				? PositiveAmount : 0;
		}
		return 0;
	}

	int32 Remaining = PositiveAmount;
	int32 Paid = 0;
	TArray<ANarrativePlayerCharacter*> EligiblePlayers = Players;
	// Each ordinary redistribution fills at least one constrained account. Bound
	// retries by the original account count even when currency callbacks change it.
	for (int32 Pass = 0; Pass < Players.Num() && Remaining > 0; ++Pass)
	{
		EligiblePlayers.RemoveAll([](const ANarrativePlayerCharacter* Player)
		{
			return !IsValid(Player) || !Player->GetInventoryComponent()
				|| Player->GetInventoryComponent()->GetCurrency() >= MAX_int32;
		});
		int32 AccountsRemaining = EligiblePlayers.Num();
		if (AccountsRemaining == 0) break;
		const int32 PaidBeforePass = Paid;
		for (ANarrativePlayerCharacter* Player : EligiblePlayers)
		{
			const int64 FairShare = FMath::DivideAndRoundUp<int64>(Remaining, AccountsRemaining--);
			UNarrativeInventoryComponent* Inventory = IsValid(Player) ? Player->GetInventoryComponent() : nullptr;
			const int64 Capacity = Inventory
				? static_cast<int64>(MAX_int32) - FMath::Max(0, Inventory->GetCurrency()) : 0;
			const int32 Share = static_cast<int32>(FMath::Min(FairShare, Capacity));
			if (Share > 0 && CreditCurrency(Player, Share, Faction, Reason, Type))
			{
				Paid += Share;
				Remaining -= Share;
			}
		}
		if (Paid == PaidBeforePass) break;
	}
	if (Remaining > 0)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("CreditCurrencyToFaction: %d currency remains unpaid after bounded settlement; Narrative accounts are full, unavailable, or rejected the credit for %s"),
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

bool UTerritoryEconomySubsystem::RegisterFactionResourceAccount(
	const FGameplayTag& Faction, AActor* AccountActor)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Faction.IsValid()
		|| !AccountActor || AccountActor->GetWorld() != World
		|| !ResolveCurrencyAccount(AccountActor)
		|| !DoesAccountBelongToFaction(AccountActor, Faction))
	{
		return false;
	}

	FactionResourceAccounts.Add(Faction, AccountActor);
	UpdateResourceSnapshot(Faction, GetCurrentProductionCycle());
	PublishProductionState();
	return true;
}

void UTerritoryEconomySubsystem::UnregisterFactionResourceAccount(
	const FGameplayTag& Faction, AActor* AccountActor)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Faction.IsValid()) return;
	if (const TWeakObjectPtr<AActor>* Existing = FactionResourceAccounts.Find(Faction))
	{
		if (!AccountActor || Existing->Get() == AccountActor)
		{
			FactionResourceAccounts.Remove(Faction);
			FTerritoryFactionResourceSnapshot& Snapshot = ResourceSnapshots.FindOrAdd(Faction);
			Snapshot.Faction = Faction;
			Snapshot.bStorageAvailable = false;
			Snapshot.SnapshotCycle = GetCurrentProductionCycle();
			PublishProductionState();
		}
	}
}

AActor* UTerritoryEconomySubsystem::ResolveRegisteredResourceAccount(
	const FGameplayTag& Faction) const
{
	const TWeakObjectPtr<AActor>* Entry = FactionResourceAccounts.Find(Faction);
	AActor* Account = Entry ? Entry->Get() : nullptr;
	UWorld* World = GetWorld();
	return Account && World && Account->GetWorld() == World
		&& ResolveCurrencyAccount(Account)
		&& DoesAccountBelongToFaction(Account, Faction)
		? Account : nullptr;
}

ANarrativePlayerCharacter* UTerritoryEconomySubsystem::SelectSoleOnlineResourceAccount(
	const TArray<ANarrativePlayerCharacter*>& Players)
{
	return Players.Num() == 1 && IsAutomaticPlayerCurrencyAccount(Players[0])
		? Players[0] : nullptr;
}

AActor* UTerritoryEconomySubsystem::GetFactionResourceAccount(
	const FGameplayTag& Faction) const
{
	if (AActor* Registered = ResolveRegisteredResourceAccount(Faction))
	{
		return Registered;
	}
	if (!bUseSoleOnlineFactionPlayerInventory)
	{
		return nullptr;
	}
	return SelectSoleOnlineResourceAccount(GetOnlineFactionPlayers(Faction));
}

UNarrativeInventoryComponent* UTerritoryEconomySubsystem::ResolveResourceInventory(
	const FGameplayTag& Faction) const
{
	return ResolveCurrencyAccount(GetFactionResourceAccount(Faction));
}

int64 UTerritoryEconomySubsystem::GetCurrentProductionCycle() const
{
	if (ProductionCycleLength <= 0.f) return INDEX_NONE;
	const UWorld* World = GetWorld();
	const ANarrativeGameState* GameState = World
		? Cast<ANarrativeGameState>(World->GetGameState()) : nullptr;
	return GameState
		? FMath::FloorToInt64(GameState->GetAccumulatedTime() / ProductionCycleLength)
		: INDEX_NONE;
}

FString UTerritoryEconomySubsystem::MakeProductionCheckpointKey(
	const FGuid& TerritoryGUID, const FGameplayTag& RuleTag)
{
	return TerritoryGUID.ToString(EGuidFormats::DigitsWithHyphensLower)
		+ TEXT("|") + RuleTag.ToString();
}

bool UTerritoryEconomySubsystem::ExecuteResourceRecipe(
	AActor* RequestingActor, const FGameplayTag& Faction,
	const FTerritoryProductionRule& Recipe, int32 UpgradeLevel, int32 BatchCount,
	const FGameplayTag& SourceTerritory, FTerritoryProductionResult& OutResult)
{
	OutResult = FTerritoryProductionResult();
	OutResult.BatchID = FGuid::NewGuid();
	OutResult.Faction = Faction;
	OutResult.RuleTag = Recipe.RuleTag;
	OutResult.TerritoryTag = SourceTerritory;

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || BatchCount <= 0
		|| !DoesAccountBelongToFaction(RequestingActor, Faction))
	{
		OutResult.Status = ETerritoryProductionStatus::AuthorityRejected;
		OutResult.FailureReason = NSLOCTEXT("TerritoryProduction", "AuthorityRejected",
			"The resource request was not made by an authoritative faction account.");
		return false;
	}

	UNarrativeInventoryComponent* Inventory = ResolveCurrencyAccount(RequestingActor);
	const bool bSuccess = ExecuteResourceRecipeOnInventory(Inventory, Faction, Recipe,
		UpgradeLevel, BatchCount, OutResult);
	if (bSuccess)
	{
		UpdateResourceSnapshot(Faction, GetCurrentProductionCycle());
		PublishProductionState();
	}
	OnProductionSettled.Broadcast(OutResult);
	return bSuccess;
}

bool UTerritoryEconomySubsystem::ExecuteResourceRecipeOnInventory(
	UNarrativeInventoryComponent* Inventory, const FGameplayTag& Faction,
	const FTerritoryProductionRule& Recipe, int32 UpgradeLevel, int32 BatchCount,
	FTerritoryProductionResult& OutResult)
{
	if (!OutResult.BatchID.IsValid()) OutResult.BatchID = FGuid::NewGuid();
	OutResult.Faction = Faction;
	OutResult.RuleTag = Recipe.RuleTag;

	FText FailureReason;
	if (!UTerritoryProductionProfile::IsRuleConfigurationValid(Recipe, FailureReason)
		|| UpgradeLevel < 0 || BatchCount <= 0)
	{
		OutResult.Status = ETerritoryProductionStatus::InvalidProfile;
		OutResult.FailureReason = FailureReason.IsEmpty()
			? NSLOCTEXT("TerritoryProduction", "InvalidRecipe", "The production recipe is invalid.")
			: FailureReason;
		return false;
	}
	if (!Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority())
	{
		OutResult.Status = ETerritoryProductionStatus::StorageUnavailable;
		OutResult.FailureReason = NSLOCTEXT("TerritoryProduction", "NoAuthoritativeStorage",
			"No authoritative Narrative resource inventory is available.");
		return false;
	}

	if (!BuildScaledAmounts(Recipe.Inputs, UpgradeLevel, BatchCount,
		OutResult.InputsConsumed, OutResult.FailureReason)
		|| !BuildScaledAmounts(Recipe.Outputs, UpgradeLevel, BatchCount,
			OutResult.OutputsProduced, OutResult.FailureReason))
	{
		OutResult.Status = ETerritoryProductionStatus::InvalidProfile;
		OutResult.InputsConsumed.Empty();
		OutResult.OutputsProduced.Empty();
		return false;
	}

	if (!CanApplyResourceTransaction(Inventory, OutResult.InputsConsumed,
		OutResult.OutputsProduced, OutResult.Status, OutResult.FailureReason))
	{
		return false;
	}

	TArray<FTerritoryResourceAmount> AppliedInputs;
	for (const FTerritoryResourceAmount& Input : OutResult.InputsConsumed)
	{
		const int32 Consumed = ConsumeExactItems(Inventory, Input.ItemClass, Input.Quantity);
		FTerritoryResourceAmount& Applied = AppliedInputs.AddDefaulted_GetRef();
		Applied.ItemClass = Input.ItemClass;
		Applied.Quantity = Consumed;
		if (Consumed != Input.Quantity)
		{
			for (const FTerritoryResourceAmount& Rollback : AppliedInputs)
			{
				if (Rollback.Quantity > 0)
				{
					AddExactItems(Inventory, Rollback.ItemClass, Rollback.Quantity);
				}
			}
			OutResult.Status = ETerritoryProductionStatus::MissingInput;
			OutResult.FailureReason = NSLOCTEXT("TerritoryProduction", "InputChangedDuringCommit",
				"A required resource changed during settlement; the transaction was rolled back.");
			return false;
		}
	}

	TArray<FTerritoryResourceAmount> AppliedOutputs;
	for (const FTerritoryResourceAmount& Output : OutResult.OutputsProduced)
	{
		const int32 Added = AddExactItems(Inventory, Output.ItemClass, Output.Quantity);
		FTerritoryResourceAmount& Applied = AppliedOutputs.AddDefaulted_GetRef();
		Applied.ItemClass = Output.ItemClass;
		Applied.Quantity = Added;
		if (Added != Output.Quantity)
		{
			for (const FTerritoryResourceAmount& Rollback : AppliedOutputs)
			{
				if (Rollback.Quantity > 0)
				{
					ConsumeExactItems(Inventory, Rollback.ItemClass, Rollback.Quantity);
				}
			}
			bool bRollbackComplete = true;
			for (const FTerritoryResourceAmount& Rollback : AppliedInputs)
			{
				bRollbackComplete &= AddExactItems(Inventory, Rollback.ItemClass,
					Rollback.Quantity) == Rollback.Quantity;
			}
			OutResult.Status = ETerritoryProductionStatus::StorageFull;
			OutResult.FailureReason = NSLOCTEXT("TerritoryProduction", "OutputChangedDuringCommit",
				"Resource storage changed during settlement; the transaction was rolled back.");
			if (!bRollbackComplete)
			{
				UE_LOG(LogTerritory, Error,
					TEXT("Resource transaction %s could not fully restore inputs after an unexpected Narrative inventory mutation."),
					*OutResult.BatchID.ToString());
			}
			return false;
		}
	}

	OutResult.bSuccess = true;
	OutResult.Status = ETerritoryProductionStatus::Produced;
	OutResult.FailureReason = FText::GetEmpty();
	return true;
}

void UTerritoryEconomySubsystem::RefreshProductionSite(ATerritoryProperty* Property)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Property
		|| Property->GetWorld() != World) return;

	const FGuid TerritoryGUID = Property->GetActorGUID_Implementation();
	if (!TerritoryGUID.IsValid())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Production site %s has no stable GUID and will not be scheduled."),
			*Property->GetPathName());
		return;
	}

	FTerritoryProductionSiteRecord& Site = ProductionSites.FindOrAdd(TerritoryGUID);
	const FGameplayTag PreviousOwner = Site.OwnerFaction;
	const bool bProfileChanged = Site.ProductionProfile.Get() != Property->GetProductionProfile();
	Site.TerritoryGUID = TerritoryGUID;
	Site.TerritoryTag = Property->GetTerritoryTag();
	Site.ParentTerritoryTag = Property->GetParentTerritoryTag();
	Site.DisplayName = Property->GetTerritoryDisplayName();
	Site.OwnerFaction = Property->GetOwningFaction();
	Site.ProductionProfile = Property->GetProductionProfile();
	Site.UpgradeLevel = FMath::Max(0, Property->UpgradeLevel);
	Site.TerritoryState = Property->GetTerritoryState();
	Site.Availability = Property->GetTerritoryAvailability();

	const int64 CurrentCycle = GetCurrentProductionCycle();
	UTerritoryProductionProfile* Profile = Property->GetProductionProfile();
	if (CurrentCycle != INDEX_NONE && Profile)
	{
		for (const FTerritoryProductionRule& Rule : Profile->Rules)
		{
			const FString Key = MakeProductionCheckpointKey(TerritoryGUID, Rule.RuleTag);
			FTerritoryProductionCheckpoint& Checkpoint = ProductionCheckpoints.FindOrAdd(Key);
			if (Checkpoint.LastProcessedCycle == INDEX_NONE
				|| PreviousOwner != Site.OwnerFaction || bProfileChanged)
			{
				Checkpoint.TerritoryGUID = TerritoryGUID;
				Checkpoint.TerritoryTag = Site.TerritoryTag;
				Checkpoint.RuleTag = Rule.RuleTag;
				Checkpoint.OwnerFaction = Site.OwnerFaction;
				Checkpoint.LastProcessedCycle = CurrentCycle;
			}
		}
	}
	if (PreviousOwner != Site.OwnerFaction || bProfileChanged)
	{
		Site.RuleStates.Empty();
		Site.LastRuleTag = FGameplayTag();
		Site.LastEvaluatedCycle = CurrentCycle;
		Site.LastInputs.Empty();
		Site.LastOutputs.Empty();
		Site.LastStatus = Profile && Site.OwnerFaction.IsValid()
			? ETerritoryProductionStatus::AlreadyProcessed
			: ETerritoryProductionStatus::Inactive;
		Site.StatusReason = Profile && Site.OwnerFaction.IsValid()
			? NSLOCTEXT("TerritoryProduction", "StartsNextCycleAfterChange",
				"Production begins on the next campaign cycle.")
			: NSLOCTEXT("TerritoryProduction", "InactiveAfterChange",
				"Production requires a profile and an owning faction.");
	}
}

void UTerritoryEconomySubsystem::EvaluateProductionSite(
	FTerritoryProductionSiteRecord& Site, int64 CurrentCycle)
{
	UTerritoryProductionProfile* Profile = Site.ProductionProfile.LoadSynchronous();
	if (!Profile)
	{
		Site.LastStatus = ETerritoryProductionStatus::Inactive;
		Site.StatusReason = NSLOCTEXT("TerritoryProduction", "NoProfile",
			"This Property has no resource production profile.");
		Site.RuleStates.Empty();
		return;
	}

	FText ProfileFailure;
	if (!Profile->ValidateProfile(ProfileFailure))
	{
		Site.LastStatus = ETerritoryProductionStatus::InvalidProfile;
		Site.StatusReason = ProfileFailure;
		Site.RuleStates.Empty();
		return;
	}

	TArray<const FTerritoryProductionRule*> Rules;
	for (const FTerritoryProductionRule& Rule : Profile->Rules) Rules.Add(&Rule);
	Rules.Sort([](const FTerritoryProductionRule& A, const FTerritoryProductionRule& B)
	{
		return A.Priority == B.Priority
			? A.RuleTag.ToString() < B.RuleTag.ToString()
			: A.Priority < B.Priority;
	});

	TMap<FGameplayTag, FTerritoryProductionRuleState> PreviousStates;
	for (const FTerritoryProductionRuleState& State : Site.RuleStates)
	{
		PreviousStates.Add(State.RuleTag, State);
	}
	Site.RuleStates.Empty(Rules.Num());

	TMap<FGameplayTag, int64> FirstPendingCycles;
	int64 EarliestCycle = CurrentCycle + 1;
	for (const FTerritoryProductionRule* Rule : Rules)
	{
		if (!Rule) continue;
		FTerritoryProductionRuleState& RuleState = Site.RuleStates.AddDefaulted_GetRef();
		if (const FTerritoryProductionRuleState* Previous = PreviousStates.Find(Rule->RuleTag))
		{
			RuleState = *Previous;
		}
		RuleState.RuleTag = Rule->RuleTag;
		RuleState.DisplayName = Rule->DisplayName;

		const FString Key = MakeProductionCheckpointKey(Site.TerritoryGUID, Rule->RuleTag);
		FTerritoryProductionCheckpoint& Checkpoint = ProductionCheckpoints.FindOrAdd(Key);
		if (Checkpoint.LastProcessedCycle == INDEX_NONE
			|| Checkpoint.OwnerFaction != Site.OwnerFaction)
		{
			Checkpoint.TerritoryGUID = Site.TerritoryGUID;
			Checkpoint.TerritoryTag = Site.TerritoryTag;
			Checkpoint.RuleTag = Rule->RuleTag;
			Checkpoint.OwnerFaction = Site.OwnerFaction;
			Checkpoint.LastProcessedCycle = CurrentCycle;
			RuleState.Status = ETerritoryProductionStatus::AlreadyProcessed;
			RuleState.LastEvaluatedCycle = CurrentCycle;
			RuleState.StatusReason = NSLOCTEXT("TerritoryProduction", "StartsNextCycle",
				"Production begins on the next campaign cycle.");
			RuleState.LastInputs.Empty();
			RuleState.LastOutputs.Empty();
			continue;
		}

		const int32 PendingCount = UTerritoryProductionProfile::CalculatePendingCycleCount(
			Checkpoint.LastProcessedCycle, CurrentCycle, MaxProductionCatchupCycles);
		if (PendingCount <= 0)
		{
			if (RuleState.Status == ETerritoryProductionStatus::NeverEvaluated)
			{
				RuleState.Status = ETerritoryProductionStatus::AlreadyProcessed;
				RuleState.LastEvaluatedCycle = CurrentCycle;
				RuleState.StatusReason = NSLOCTEXT("TerritoryProduction", "StartsNextCycle",
					"Production begins on the next campaign cycle.");
			}
			continue;
		}
		const int64 FirstCycle = CurrentCycle - PendingCount + 1;
		// Cycles outside the configured catch-up window are deliberately expired.
		Checkpoint.LastProcessedCycle = FMath::Max(
			Checkpoint.LastProcessedCycle, FirstCycle - 1);
		FirstPendingCycles.Add(Rule->RuleTag, FirstCycle);
		EarliestCycle = FMath::Min(EarliestCycle, FirstCycle);
	}

	TSet<FGameplayTag> DeferredRules;
	for (int64 EvaluationCycle = EarliestCycle;
		EvaluationCycle <= CurrentCycle; ++EvaluationCycle)
	{
		for (int32 RuleIndex = 0; RuleIndex < Rules.Num(); ++RuleIndex)
		{
			const FTerritoryProductionRule* Rule = Rules[RuleIndex];
			if (!Rule || DeferredRules.Contains(Rule->RuleTag)) continue;
			const int64* FirstCycle = FirstPendingCycles.Find(Rule->RuleTag);
			if (!FirstCycle || EvaluationCycle < *FirstCycle) continue;

			const FString Key = MakeProductionCheckpointKey(Site.TerritoryGUID, Rule->RuleTag);
			FTerritoryProductionCheckpoint& Checkpoint = ProductionCheckpoints.FindChecked(Key);
			if (Checkpoint.LastProcessedCycle >= EvaluationCycle) continue;

			FTerritoryProductionResult Result;
			Result.BatchID = FGuid::NewGuid();
			Result.TerritoryGUID = Site.TerritoryGUID;
			Result.TerritoryTag = Site.TerritoryTag;
			Result.Faction = Site.OwnerFaction;
			Result.RuleTag = Rule->RuleTag;
			Result.CycleIndex = EvaluationCycle;

			FText StateFailure;
			if (!IsProductionSiteAvailable(GetWorld(), Site))
			{
				Result.Status = ETerritoryProductionStatus::Inactive;
				Result.FailureReason = NSLOCTEXT("TerritoryProduction", "TerritoryLocked",
					"Production is paused while this Place or one of its parent Territories is story-locked.");
				Checkpoint.LastProcessedCycle = EvaluationCycle;
			}
			else if (!Site.OwnerFaction.IsValid()
				|| !UTerritoryProductionProfile::CanRuleRunForState(
					*Rule, Site.TerritoryState, Site.UpgradeLevel, StateFailure))
			{
				Result.Status = ETerritoryProductionStatus::Inactive;
				Result.FailureReason = Site.OwnerFaction.IsValid() ? StateFailure
					: NSLOCTEXT("TerritoryProduction", "NoOwner",
						"Production requires an owning faction.");
				Checkpoint.LastProcessedCycle = EvaluationCycle;
			}
			else if (UNarrativeInventoryComponent* Inventory =
				ResolveResourceInventory(Site.OwnerFaction))
			{
				ExecuteResourceRecipeOnInventory(Inventory, Site.OwnerFaction, *Rule,
					Site.UpgradeLevel, 1, Result);
				if (Result.Status == ETerritoryProductionStatus::StorageUnavailable
					|| Result.Status == ETerritoryProductionStatus::StorageFull)
				{
					DeferredRules.Add(Rule->RuleTag);
				}
				else
				{
					Checkpoint.LastProcessedCycle = EvaluationCycle;
				}
			}
			else
			{
				Result.Status = ETerritoryProductionStatus::StorageUnavailable;
				Result.FailureReason = NSLOCTEXT("TerritoryProduction", "FactionStorageUnavailable",
					"No Narrative resource inventory is registered for this faction.");
				DeferredRules.Add(Rule->RuleTag);
			}

			FTerritoryProductionRuleState& RuleState = Site.RuleStates[RuleIndex];
			RuleState.Status = Result.Status;
			RuleState.LastEvaluatedCycle = EvaluationCycle;
			RuleState.StatusReason = Result.FailureReason;
			RuleState.LastInputs = Result.InputsConsumed;
			RuleState.LastOutputs = Result.OutputsProduced;
			if (const UTerritoryDeveloperSettings* Settings =
				GetDefault<UTerritoryDeveloperSettings>();
				Settings && Settings->ShouldDebugProduction())
			{
				UE_LOG(LogTerritory, Log,
					TEXT("[Production] cycle=%lld territory=%s faction=%s rule=%s status=%d inputs=%d outputs=%d reason=%s"),
					EvaluationCycle, *Site.TerritoryTag.ToString(),
					*Site.OwnerFaction.ToString(), *Rule->RuleTag.ToString(),
					static_cast<int32>(Result.Status), Result.InputsConsumed.Num(),
					Result.OutputsProduced.Num(), *Result.FailureReason.ToString());
			}
			OnProductionSettled.Broadcast(Result);
		}
	}

	auto StatusPriority = [](ETerritoryProductionStatus Status)
	{
		switch (Status)
		{
		case ETerritoryProductionStatus::InvalidProfile: return 100;
		case ETerritoryProductionStatus::StorageUnavailable: return 90;
		case ETerritoryProductionStatus::StorageFull: return 80;
		case ETerritoryProductionStatus::MissingInput: return 70;
		case ETerritoryProductionStatus::AuthorityRejected: return 60;
		case ETerritoryProductionStatus::Produced: return 40;
		case ETerritoryProductionStatus::Ready: return 30;
		case ETerritoryProductionStatus::AlreadyProcessed: return 20;
		case ETerritoryProductionStatus::Inactive: return 10;
		default: return 0;
		}
	};
	const FTerritoryProductionRuleState* SummaryState = nullptr;
	for (const FTerritoryProductionRuleState& RuleState : Site.RuleStates)
	{
		if (!SummaryState || StatusPriority(RuleState.Status) > StatusPriority(SummaryState->Status))
		{
			SummaryState = &RuleState;
		}
	}
	if (SummaryState)
	{
		Site.LastStatus = SummaryState->Status;
		Site.LastRuleTag = SummaryState->RuleTag;
		Site.LastEvaluatedCycle = SummaryState->LastEvaluatedCycle;
		Site.StatusReason = SummaryState->StatusReason;
		Site.LastInputs = SummaryState->LastInputs;
		Site.LastOutputs = SummaryState->LastOutputs;
	}
}

void UTerritoryEconomySubsystem::ProcessResourceProduction()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	const int64 CurrentCycle = GetCurrentProductionCycle();
	if (CurrentCycle == INDEX_NONE) return;
	LastObservedProductionCycle = CurrentCycle;

	if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
		{
			if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
			{
				RefreshProductionSite(Property);
			}
		}
	}

	TArray<FGuid> SiteIDs;
	ProductionSites.GetKeys(SiteIDs);
	SiteIDs.Sort([](const FGuid& A, const FGuid& B)
	{
		return A.ToString(EGuidFormats::Digits) < B.ToString(EGuidFormats::Digits);
	});
	TSet<FGameplayTag> AffectedFactions;
	for (const FGuid& SiteID : SiteIDs)
	{
		if (FTerritoryProductionSiteRecord* Site = ProductionSites.Find(SiteID))
		{
			EvaluateProductionSite(*Site, CurrentCycle);
			if (Site->OwnerFaction.IsValid()) AffectedFactions.Add(Site->OwnerFaction);
		}
	}
	for (const FGameplayTag& Faction : AffectedFactions)
	{
		UpdateResourceSnapshot(Faction, CurrentCycle);
	}
	PublishProductionState();
}

void UTerritoryEconomySubsystem::UpdateResourceSnapshot(
	const FGameplayTag& Faction, int64 CurrentCycle)
{
	if (!Faction.IsValid()) return;
	FTerritoryFactionResourceSnapshot& Snapshot = ResourceSnapshots.FindOrAdd(Faction);
	Snapshot.Faction = Faction;
	Snapshot.SnapshotCycle = CurrentCycle;
	Snapshot.Resources.Empty();

	UNarrativeInventoryComponent* Inventory = ResolveResourceInventory(Faction);
	Snapshot.bStorageAvailable = Inventory != nullptr;
	if (!Inventory) return;

	TSet<UClass*> ReferencedClasses;
	for (const TPair<FGuid, FTerritoryProductionSiteRecord>& Pair : ProductionSites)
	{
		const UTerritoryProductionProfile* Profile = Pair.Value.ProductionProfile.LoadSynchronous();
		if (!Profile) continue;
		for (const FTerritoryProductionRule& Rule : Profile->Rules)
		{
			for (const FTerritoryResourceRate& Rate : Rule.Inputs)
			{
				if (Rate.ItemClass) ReferencedClasses.Add(Rate.ItemClass.Get());
			}
			for (const FTerritoryResourceRate& Rate : Rule.Outputs)
			{
				if (Rate.ItemClass) ReferencedClasses.Add(Rate.ItemClass.Get());
			}
		}
	}

	TArray<UClass*> Classes = ReferencedClasses.Array();
	Classes.Sort([](const UClass& A, const UClass& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	for (UClass* ItemClass : Classes)
	{
		FTerritoryResourceAmount& Amount = Snapshot.Resources.AddDefaulted_GetRef();
		Amount.ItemClass = ItemClass;
		Amount.Quantity = Inventory->GetTotalQuantityOfItemExact(
			TSoftClassPtr<UNarrativeItem>(ItemClass), false);
	}
}

void UTerritoryEconomySubsystem::PublishProductionState() const
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	if (ATerritoryWorldState* WorldState =
		ATerritoryWorldState::FindTerritoryWorldState(World))
	{
		WorldState->SetProductionState(GetProductionCheckpoints(), GetAllProductionSites(),
			GetAllResourceSnapshots());
	}
}

FTerritoryFactionResourceSnapshot UTerritoryEconomySubsystem::GetFactionResourceSnapshot(
	const FGameplayTag& Faction) const
{
	const FTerritoryFactionResourceSnapshot* Snapshot = ResourceSnapshots.Find(Faction);
	return Snapshot ? *Snapshot : FTerritoryFactionResourceSnapshot();
}

TArray<FTerritoryProductionSiteRecord> UTerritoryEconomySubsystem::GetProductionSitesForFaction(
	const FGameplayTag& Faction) const
{
	TArray<FTerritoryProductionSiteRecord> Result;
	for (const TPair<FGuid, FTerritoryProductionSiteRecord>& Pair : ProductionSites)
	{
		if (Pair.Value.OwnerFaction == Faction) Result.Add(Pair.Value);
	}
	Result.Sort([](const FTerritoryProductionSiteRecord& A,
		const FTerritoryProductionSiteRecord& B)
	{
		return A.TerritoryTag.ToString() < B.TerritoryTag.ToString();
	});
	return Result;
}

FTerritoryProductionSiteRecord UTerritoryEconomySubsystem::GetProductionSite(
	const FGameplayTag& TerritoryTag) const
{
	for (const TPair<FGuid, FTerritoryProductionSiteRecord>& Pair : ProductionSites)
	{
		if (Pair.Value.TerritoryTag == TerritoryTag) return Pair.Value;
	}
	return FTerritoryProductionSiteRecord();
}

TArray<FTerritoryProductionCheckpoint> UTerritoryEconomySubsystem::GetProductionCheckpoints() const
{
	TArray<FTerritoryProductionCheckpoint> Result;
	ProductionCheckpoints.GenerateValueArray(Result);
	Result.Sort([](const FTerritoryProductionCheckpoint& A,
		const FTerritoryProductionCheckpoint& B)
	{
		const FString AKey = MakeProductionCheckpointKey(A.TerritoryGUID, A.RuleTag);
		const FString BKey = MakeProductionCheckpointKey(B.TerritoryGUID, B.RuleTag);
		return AKey < BKey;
	});
	return Result;
}

TArray<FTerritoryProductionSiteRecord> UTerritoryEconomySubsystem::GetAllProductionSites() const
{
	TArray<FTerritoryProductionSiteRecord> Result;
	ProductionSites.GenerateValueArray(Result);
	Result.Sort([](const FTerritoryProductionSiteRecord& A,
		const FTerritoryProductionSiteRecord& B)
	{
		return A.TerritoryGUID.ToString(EGuidFormats::Digits)
			< B.TerritoryGUID.ToString(EGuidFormats::Digits);
	});
	return Result;
}

TArray<FTerritoryFactionResourceSnapshot> UTerritoryEconomySubsystem::GetAllResourceSnapshots() const
{
	TArray<FTerritoryFactionResourceSnapshot> Result;
	ResourceSnapshots.GenerateValueArray(Result);
	Result.Sort([](const FTerritoryFactionResourceSnapshot& A,
		const FTerritoryFactionResourceSnapshot& B)
	{
		return A.Faction.ToString() < B.Faction.ToString();
	});
	return Result;
}

void UTerritoryEconomySubsystem::RestoreProductionState(
	const TArray<FTerritoryProductionCheckpoint>& Checkpoints,
	const TArray<FTerritoryProductionSiteRecord>& Sites,
	const TArray<FTerritoryFactionResourceSnapshot>& InResourceSnapshots)
{
	ProductionCheckpoints.Empty();
	for (const FTerritoryProductionCheckpoint& Checkpoint : Checkpoints)
	{
		if (Checkpoint.TerritoryGUID.IsValid() && Checkpoint.RuleTag.IsValid())
		{
			ProductionCheckpoints.Add(MakeProductionCheckpointKey(
				Checkpoint.TerritoryGUID, Checkpoint.RuleTag), Checkpoint);
		}
	}
	ProductionSites.Empty();
	for (const FTerritoryProductionSiteRecord& Site : Sites)
	{
		if (Site.TerritoryGUID.IsValid()) ProductionSites.Add(Site.TerritoryGUID, Site);
	}
	ResourceSnapshots.Empty();
	for (const FTerritoryFactionResourceSnapshot& Snapshot : InResourceSnapshots)
	{
		if (Snapshot.Faction.IsValid()) ResourceSnapshots.Add(Snapshot.Faction, Snapshot);
	}
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

AActor* UTerritoryEconomySubsystem::ResolveFallbackFactionAccount(
	const FGameplayTag& Faction) const
{
	if (AActor* Shared = ResolveRegisteredCurrencyAccount(
		Faction, ETerritoryIncomePayoutPolicy::SharedNarrativeAccount))
	{
		return Shared;
	}
	return ResolveRegisteredCurrencyAccount(
		Faction, ETerritoryIncomePayoutPolicy::FactionLeader);
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
	int32 ActivePlaceCount = 0;

	for (const ATerritoryVolume* Territory : Territories)
	{
		if (!Territory || !Territory->IsAvailableForGameplay()) continue;
		// Only count leaf-level (Property) income to avoid hierarchy double-counting.
		// Cities and Districts are containers — their PeriodicIncome is metadata
		// for UI display, not a separate income source.
		if (Territory->IsA<ATerritoryProperty>())
		{
			const ATerritoryProperty* Property = Cast<const ATerritoryProperty>(Territory);
			++ActivePlaceCount;
			TotalIncome += static_cast<int64>(Property->GetEffectiveIncome());
		}

		// GuardCost is the per-guard upkeep. Use the persistent desired garrison
		// rather than the configured maximum so add/remove commands affect finances.
		const int64 GuardUpkeep = static_cast<int64>(FMath::Max(0, Territory->GetGuardCost()))
			* static_cast<int64>(Territory->GetDesiredGuardCount());
		TotalCosts += GuardUpkeep;
	}
	Treasury.TerritoryCount = ActivePlaceCount;
	Treasury.IncomePerTick = static_cast<int32>(FMath::Clamp<int64>(TotalIncome, 0, MAX_int32));
	Treasury.CostsPerTick = static_cast<int32>(FMath::Clamp<int64>(TotalCosts, 0, MAX_int32));

	// WorldState owns the replicated/late-join economy read model. Publish every
	// recalculation so staffing changes do not wait for the next payout interval.
	if (ATerritoryWorldState* WorldState =
		ATerritoryWorldState::FindTerritoryWorldState(World))
	{
		WorldState->SetFactionTreasury(Faction, Treasury);
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
	if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		RefreshProductionSite(Property);
		// Ownership and checkpoint reset must reach the replicated WorldState in the
		// same atomic transition. Waiting for the next economy timer would let clients
		// and late joiners display the former faction as still earning this Property.
		PublishProductionState();
	}
}

void UTerritoryEconomySubsystem::OnTerritoryStateChanged(
	ATerritoryVolume* Territory, ETerritoryState NewState)
{
	if (!Territory) return;
	if (Territory->GetOwningFaction().IsValid()) MarkFactionDirty(Territory->GetOwningFaction());
	if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		RefreshProductionSite(Property);
		PublishProductionState();
	}
}

void UTerritoryEconomySubsystem::OnTerritoryAvailabilityChanged(
	ATerritoryVolume* Territory, ETerritoryAvailability NewAvailability)
{
	OnTerritoryStateChanged(Territory,
		Territory ? Territory->GetTerritoryState() : ETerritoryState::Unclaimed);
	if (!Territory || Territory->GetControlMode() != ETerritoryControlMode::AggregateOnly)
	{
		return;
	}

	// A parent availability change gates every descendant Place without mutating
	// child ownership. Dirty the affected faction ledgers so income/upkeep changes
	// on the next economy pass instead of waiting for a child transition.
	UTerritoryRegistrySubsystem* Registry = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return;
	const FGameplayTag AncestorTag = Territory->GetTerritoryTag();
	for (ATerritoryVolume* Candidate : Registry->GetAllTerritories())
	{
		ATerritoryProperty* Place = Cast<ATerritoryProperty>(Candidate);
		if (!Place) continue;
		TSet<FGameplayTag> Visited;
		FGameplayTag ParentTag = Place->GetParentTerritoryTag();
		while (ParentTag.IsValid() && !Visited.Contains(ParentTag))
		{
			if (ParentTag == AncestorTag)
			{
				if (Place->GetOwningFaction().IsValid())
				{
					MarkFactionDirty(Place->GetOwningFaction());
				}
				break;
			}
			Visited.Add(ParentTag);
			const ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
			if (!Parent) break;
			ParentTag = Parent->GetParentTerritoryTag();
		}
	}
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
	Territory->OnTerritoryStateChangedDelegate.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryStateChanged);
	Territory->OnTerritoryAvailabilityChanged.AddDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryAvailabilityChanged);
	if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		RefreshProductionSite(Property);
		PublishProductionState();
	}

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
	if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		RefreshProductionSite(Property);
		PublishProductionState();
	}

	// Unbind the control-changed delegate to prevent dangling references
	Territory->OnTerritoryOwnershipChanged.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryControlChanged);
	Territory->OnTerritoryStateChangedDelegate.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryStateChanged);
	Territory->OnTerritoryAvailabilityChanged.RemoveDynamic(this, &UTerritoryEconomySubsystem::OnTerritoryAvailabilityChanged);

	// Recalculate income for the owning faction (territory removed from their count)
	FGameplayTag Owner = Territory->GetOwningFaction();
	if (Owner.IsValid())
	{
		RecalculateIncome(Owner);
	}
}
