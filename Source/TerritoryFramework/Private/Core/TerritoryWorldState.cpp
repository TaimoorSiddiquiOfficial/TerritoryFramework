#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "SaveSystemStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

ATerritoryWorldState::ATerritoryWorldState()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ATerritoryWorldState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!WorldStateGUID.IsValid())
		{
			UE_LOG(LogTerritory, Error, TEXT("TerritoryWorldState %s has no authored WorldStateGUID; save/load is disabled."),
				*GetPathName());
			return;
		}

		if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.AddUniqueDynamic(this, &ATerritoryWorldState::OnTerritoryRegistered);
		}
		USaveSystemStatics::LoadSingleActor(this);
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ATerritoryWorldState::ApplyPendingCaptureSummaries));

		// P0-02: Subscribe to subsystem delegates for live replication
		SubscribeToLiveUpdates();
	}
}

void ATerritoryWorldState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingCaptureRetryTimerHandle);
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryWorldState::OnTerritoryRegistered);
		}
		// P0-02: Unsubscribe from live replication delegates
		UnsubscribeFromLiveUpdates();
	}
	Super::EndPlay(EndPlayReason);
}

void ATerritoryWorldState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATerritoryWorldState, ReplicatedTreasuries);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedTransactions);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedTreaties);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedReputation);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedDiplomacyHistory);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedCaptureSummaries);
}

#if WITH_EDITOR
void ATerritoryWorldState::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!WorldStateGUID.IsValid())
	{
		WorldStateGUID = FGuid::NewGuid();
	}
}

void ATerritoryWorldState::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// PIE world creation uses StaticDuplicateObject — must NOT regenerate GUID.
	// Only regenerate for actual editor duplication (user Ctrl+D).
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		WorldStateGUID = FGuid::NewGuid();
	}
}
#endif

// ─── INarrativeSavableActor ───

FGuid ATerritoryWorldState::GetActorGUID_Implementation() const { return WorldStateGUID; }
void ATerritoryWorldState::SetActorGUID_Implementation(const FGuid& NewGUID) { WorldStateGUID = NewGUID; }
bool ATerritoryWorldState::ShouldRespawn_Implementation() const { return false; }

void ATerritoryWorldState::PrepareForSave_Implementation()
{
	ExportPersistentState();
}

void ATerritoryWorldState::Load_Implementation()
{
	ImportPersistentState();
}

// ─── Economy API (TArray-based lookups) ───

void ATerritoryWorldState::SetFactionTreasury(const FGameplayTag& Faction, const FTerritoryTreasury& Treasury)
{
	if (!HasAuthority() || !Faction.IsValid()) return;

	// Find existing entry or add new
	for (FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
	{
		if (Entry.Faction == Faction)
		{
			Entry.IncomePerTick = Treasury.IncomePerTick;
			Entry.CostsPerTick = Treasury.CostsPerTick;
			Entry.TerritoryCount = Treasury.TerritoryCount;
			return;
		}
	}

	FReplicatedFactionEconomy NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.IncomePerTick = Treasury.IncomePerTick;
	NewEntry.CostsPerTick = Treasury.CostsPerTick;
	NewEntry.TerritoryCount = Treasury.TerritoryCount;
	ReplicatedTreasuries.Add(NewEntry);
}

FTerritoryTreasury ATerritoryWorldState::GetFactionTreasury(const FGameplayTag& Faction) const
{
	for (const FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
	{
		if (Entry.Faction == Faction)
		{
			FTerritoryTreasury Result;
			Result.IncomePerTick = Entry.IncomePerTick;
			Result.CostsPerTick = Entry.CostsPerTick;
			Result.TerritoryCount = Entry.TerritoryCount;
			return Result;
		}
	}
	return FTerritoryTreasury();
}

TArray<FGameplayTag> ATerritoryWorldState::GetAllFactionsWithEconomy() const
{
	TArray<FGameplayTag> Result;
	for (const FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
	{
		Result.Add(Entry.Faction);
	}
	return Result;
}

// ─── Transaction API ───

void ATerritoryWorldState::RecordTransaction(const FReplicatedTransaction& Transaction)
{
	if (!HasAuthority()) return;
	ReplicatedTransactions.Add(Transaction);

	// Use MaxTransactionHistory from EconomySubsystem (default 500)
	const UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
	int32 MaxHistory = Economy ? Economy->MaxTransactionHistory : 500;

	while (ReplicatedTransactions.Num() > MaxHistory)
	{
		ReplicatedTransactions.RemoveAt(0);
	}

	// Convert to FTerritoryTransaction for the delegate
	FTerritoryTransaction Tx;
	Tx.TransactionID = Transaction.TransactionID;
	Tx.Faction = Transaction.Faction;
	Tx.Type = Transaction.Type;
	Tx.Amount = Transaction.Amount;
	Tx.BalanceAfter = Transaction.BalanceAfter;
	Tx.GameTime = Transaction.GameTime;
	Tx.Reason = Transaction.Reason;
	Tx.SourceTerritory = Transaction.SourceTerritory;
	OnTransactionRecorded.Broadcast(Tx);
}

TArray<FReplicatedTransaction> ATerritoryWorldState::GetTransactionHistory(const FGameplayTag& Faction, int32 MaxEntries) const
{
	TArray<FReplicatedTransaction> Result;
	for (int32 i = ReplicatedTransactions.Num() - 1; i >= 0 && Result.Num() < MaxEntries; --i)
	{
		if (ReplicatedTransactions[i].Faction == Faction)
		{
			Result.Add(ReplicatedTransactions[i]);
		}
	}
	return Result;
}

// ─── Treaty API ───

void ATerritoryWorldState::SetTreaty(const FReplicatedTreaty& Treaty)
{
	if (!HasAuthority()) return;

	for (FReplicatedTreaty& Existing : ReplicatedTreaties)
	{
		if ((Existing.FactionA == Treaty.FactionA && Existing.FactionB == Treaty.FactionB) ||
			(Existing.FactionA == Treaty.FactionB && Existing.FactionB == Treaty.FactionA))
		{
			Existing = Treaty;
			return;
		}
	}

	ReplicatedTreaties.Add(Treaty);
}

void ATerritoryWorldState::RemoveTreaty(const FGuid& TreatyID)
{
	if (!HasAuthority()) return;
	ReplicatedTreaties.RemoveAll([&TreatyID](const FReplicatedTreaty& T) { return T.TreatyID == TreatyID; });
}

TArray<FReplicatedTreaty> ATerritoryWorldState::GetAllTreaties() const
{
	return ReplicatedTreaties;
}

FReplicatedTreaty ATerritoryWorldState::GetTreatyBetween(const FGameplayTag& FactionA, const FGameplayTag& FactionB) const
{
	for (const FReplicatedTreaty& Treaty : ReplicatedTreaties)
	{
		if ((Treaty.FactionA == FactionA && Treaty.FactionB == FactionB) ||
			(Treaty.FactionA == FactionB && Treaty.FactionB == FactionA))
		{
			return Treaty;
		}
	}
	return FReplicatedTreaty();
}

// ─── Reputation API (TArray-based lookups) ───

void ATerritoryWorldState::SetReputation(const FGameplayTag& Faction, int32 Value)
{
	if (!HasAuthority() || !Faction.IsValid()) return;

	for (FReplicatedFactionReputation& Entry : ReplicatedReputation)
	{
		if (Entry.Faction == Faction)
		{
			Entry.Reputation = Value;
			return;
		}
	}

	FReplicatedFactionReputation NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.Reputation = Value;
	ReplicatedReputation.Add(NewEntry);
}

int32 ATerritoryWorldState::GetReputation(const FGameplayTag& Faction) const
{
	for (const FReplicatedFactionReputation& Entry : ReplicatedReputation)
	{
		if (Entry.Faction == Faction)
		{
			return Entry.Reputation;
		}
	}
	return 0;
}

// ─── Capture Summary API (TArray-based lookups) ───

void ATerritoryWorldState::SetCaptureSummary(const FReplicatedCaptureSummary& Summary)
{
	if (!HasAuthority()) return;

	for (FReplicatedCaptureSummary& Entry : ReplicatedCaptureSummaries)
	{
		if (Entry.TerritoryTag == Summary.TerritoryTag)
		{
			Entry = Summary;
			return;
		}
	}

	ReplicatedCaptureSummaries.Add(Summary);
}

FReplicatedCaptureSummary ATerritoryWorldState::GetCaptureSummary(const FGameplayTag& TerritoryTag) const
{
	for (const FReplicatedCaptureSummary& Entry : ReplicatedCaptureSummaries)
	{
		if (Entry.TerritoryTag == TerritoryTag)
		{
			return Entry;
		}
	}
	return FReplicatedCaptureSummary();
}

// ─── State Export/Import ───

void ATerritoryWorldState::ExportPersistentState()
{
	if (!HasAuthority()) return;

	// Pull live state from subsystems into replicated arrays BEFORE copying to saved arrays.
	// The EconomySubsystem holds the authoritative economy parameters and ledger;
	// without this sync, ReplicatedTreasuries stays empty and nothing persists.
	UWorld* World = GetWorld();
	if (World)
	{
		if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
		{
			ReplicatedTreasuries.Empty();
			TArray<FGameplayTag> Factions = Economy->GetAllFactionsWithTreasury();
			for (const FGameplayTag& Faction : Factions)
			{
				FTerritoryTreasury Treasury = Economy->GetFactionEconomy(Faction);
				FReplicatedFactionEconomy Entry;
				Entry.Faction = Faction;
				// FReplicatedFactionEconomy::Treasury no longer used — faction wealth lives in
				// NarrativePro UInventoryComponent::Currency on each player's character (saved by NarrativePro).
				Entry.IncomePerTick = Treasury.IncomePerTick;
				Entry.CostsPerTick = Treasury.CostsPerTick;
				Entry.TerritoryCount = Treasury.TerritoryCount;
				ReplicatedTreasuries.Add(Entry);
			}

			ReplicatedTransactions.Empty();
			for (const FTerritoryTransaction& Tx : Economy->GetAllTransactionHistory())
			{
				FReplicatedTransaction RepTx;
				RepTx.TransactionID = Tx.TransactionID;
				RepTx.Faction = Tx.Faction;
				RepTx.Type = Tx.Type;
				RepTx.Amount = Tx.Amount;
				RepTx.BalanceAfter = Tx.BalanceAfter;
				RepTx.GameTime = Tx.GameTime;
				RepTx.Reason = Tx.Reason;
				RepTx.SourceTerritory = Tx.SourceTerritory;
				ReplicatedTransactions.Add(RepTx);
			}
		}

		if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
		{
			ReplicatedTreaties.Empty();
			for (const FTreatyRecord& Treaty : Diplomacy->GetAllTreaties())
			{
				FReplicatedTreaty RepTreaty;
				RepTreaty.TreatyID = Treaty.GetCanonicalKey();
				RepTreaty.FactionA = Treaty.FactionA;
				RepTreaty.FactionB = Treaty.FactionB;
				RepTreaty.State = Treaty.State;
				RepTreaty.SignedGameTime = Treaty.SignedGameTime;
				RepTreaty.ExpiryGameTime = Treaty.ExpiryGameTime;
				RepTreaty.bPermanent = Treaty.bPermanent;
				ReplicatedTreaties.Add(RepTreaty);
			}

			ReplicatedReputation.Empty();
			TMap<FGameplayTag, int32> AllRep = Diplomacy->GetAllReputation();
			for (const auto& Pair : AllRep)
			{
				FReplicatedFactionReputation RepRep;
				RepRep.Faction = Pair.Key;
				RepRep.Reputation = Pair.Value;
				ReplicatedReputation.Add(RepRep);
			}
			ReplicatedDiplomacyHistory = Diplomacy->GetDiplomacyHistory();
		}

		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			ReplicatedCaptureSummaries.Empty();
			for (const ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (!Territory) continue;
				FReplicatedCaptureSummary Summary;
				Summary.TerritoryTag = Territory->GetTerritoryTag();
				Summary.TerritoryGUID = Territory->GetActorGUID_Implementation();
				Summary.CurrentOwner = Territory->GetOwningFaction();
				Summary.ContestingFaction =
					ITerritoryOwnershipInterface::Execute_GetContestingFaction(Territory);
				Summary.ControlProgress = Territory->GetControlProgress();
				Summary.State = Territory->GetTerritoryState();
				ReplicatedCaptureSummaries.Add(Summary);
			}
		}
	}

	SavedTreasuries = ReplicatedTreasuries;
	SavedTransactions = ReplicatedTransactions;
	SavedTreaties = ReplicatedTreaties;
	SavedReputation = ReplicatedReputation;
	SavedDiplomacyHistory = ReplicatedDiplomacyHistory;
	SavedCaptureSummaries = ReplicatedCaptureSummaries;
}

void ATerritoryWorldState::ImportPersistentState()
{
	if (!HasAuthority()) return;

	// Direct assignment — no artificial transactions
	ReplicatedTreasuries = SavedTreasuries;
	ReplicatedTransactions = SavedTransactions;
	ReplicatedTreaties = SavedTreaties;
	ReplicatedReputation = SavedReputation;
	ReplicatedDiplomacyHistory = SavedDiplomacyHistory;
	ReplicatedCaptureSummaries = SavedCaptureSummaries;

	SyncSubsystemsFromReplicatedState();
}

void ATerritoryWorldState::SyncSubsystemsFromReplicatedState()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Sync economy subsystem — only income/cost/territory params.
	// Faction gold lives in NarrativePro player inventories (UInventoryComponent::Currency),
	// not in TerritoryFramework state.
	if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		TMap<FGameplayTag, FTerritoryTreasury> Treasuries;
		for (const FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
		{
			FTerritoryTreasury Treasury;
			Treasury.IncomePerTick = Entry.IncomePerTick;
			Treasury.CostsPerTick = Entry.CostsPerTick;
			Treasury.TerritoryCount = Entry.TerritoryCount;
			Treasuries.Add(Entry.Faction, Treasury);
		}
		Economy->RestoreTreasuryState(Treasuries);

		TArray<FTerritoryTransaction> Transactions;
		Transactions.Reserve(ReplicatedTransactions.Num());
		for (const FReplicatedTransaction& Entry : ReplicatedTransactions)
		{
			FTerritoryTransaction Transaction;
			Transaction.TransactionID = Entry.TransactionID;
			Transaction.Faction = Entry.Faction;
			Transaction.Type = Entry.Type;
			Transaction.Amount = Entry.Amount;
			Transaction.BalanceAfter = Entry.BalanceAfter;
			Transaction.GameTime = Entry.GameTime;
			Transaction.Reason = Entry.Reason;
			Transaction.SourceTerritory = Entry.SourceTerritory;
			Transactions.Add(Transaction);
		}
		Economy->RestoreTransactionHistory(Transactions);
	}

	// Sync diplomacy subsystem
	if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		TArray<FTreatyRecord> Treaties;
		Treaties.Reserve(ReplicatedTreaties.Num());
		for (const FReplicatedTreaty& Treaty : ReplicatedTreaties)
		{
			FTreatyRecord Record;
			Record.FactionA = Treaty.FactionA;
			Record.FactionB = Treaty.FactionB;
			Record.State = Treaty.State;
			Record.SignedGameTime = Treaty.SignedGameTime;
			Record.ExpiryGameTime = Treaty.ExpiryGameTime;
			Record.bPermanent = Treaty.bPermanent;
			Treaties.Add(Record);
		}

		TMap<FGameplayTag, int32> Reputation;
		for (const FReplicatedFactionReputation& Entry : ReplicatedReputation)
		{
			Reputation.Add(Entry.Faction, Entry.Reputation);
		}
		Diplomacy->RestorePersistentState(Treaties, Reputation, ReplicatedDiplomacyHistory);
	}

	// Sync capture state. Registry registration can happen after WorldState load,
	// especially with streaming, so retain unresolved summaries and apply them from
	// OnTerritoryRegistered instead of dropping them.
	PendingCaptureSummaries = ReplicatedCaptureSummaries;
	ApplyPendingCaptureSummaries();
	if (PendingCaptureSummaries.Num() > 0)
	{
		World->GetTimerManager().SetTimer(PendingCaptureRetryTimerHandle, this,
			&ATerritoryWorldState::ApplyPendingCaptureSummaries, 1.f, true);
	}
}

void ATerritoryWorldState::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (bWasUnregistered || !Territory || PendingCaptureSummaries.Num() == 0) return;
	ApplyPendingCaptureSummaries();
}

void ATerritoryWorldState::ApplyPendingCaptureSummaries()
{
	UWorld* World = GetWorld();
	// P0-01: Capture summary application is server-authoritative — requires ForceSetOwningFaction
	if (!World || World->GetNetMode() == NM_Client) return;
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	if (!Registry || !Control) return;

	for (int32 Index = PendingCaptureSummaries.Num() - 1; Index >= 0; --Index)
	{
		const FReplicatedCaptureSummary& Summary = PendingCaptureSummaries[Index];
		ATerritoryVolume* Territory = Summary.TerritoryGUID.IsValid()
			? Registry->GetTerritoryByGUID(Summary.TerritoryGUID)
			: nullptr;
		if (!Territory && Summary.TerritoryTag.IsValid())
		{
			Territory = Registry->GetTerritoryByTag(Summary.TerritoryTag);
		}
		if (!Territory) continue;

		if (Summary.CurrentOwner != Territory->GetOwningFaction())
		{
			Territory->ForceSetOwningFaction(Summary.CurrentOwner);
		}
		if (Summary.State == ETerritoryState::Contested)
		{
			Territory->ForceSetTerritoryState(ETerritoryState::Contested);
			Control->RestoreCaptureState(Territory, Summary.ContestingFaction, Summary.ControlProgress);
		}
		else if (Territory->GetTerritoryState() != Summary.State)
		{
			Territory->ForceSetTerritoryState(Summary.State);
		}

		PendingCaptureSummaries.RemoveAtSwap(Index);
	}

	if (PendingCaptureSummaries.Num() == 0 && World)
	{
		World->GetTimerManager().ClearTimer(PendingCaptureRetryTimerHandle);
		PendingCaptureRetryCount = 0;
	}
	else if (World)
	{
		// P2-14: Bounded retries — stop after MaxPendingCaptureRetries and log unresolved
		++PendingCaptureRetryCount;
		if (PendingCaptureRetryCount >= MaxPendingCaptureRetries)
		{
			World->GetTimerManager().ClearTimer(PendingCaptureRetryTimerHandle);
			for (const FReplicatedCaptureSummary& Orphan : PendingCaptureSummaries)
			{
				UE_LOG(LogTerritory, Warning, TEXT("[WorldState] Unresolved capture summary after %d retries — tag=%s guid=%s (territory may have been removed/renamed)"),
					MaxPendingCaptureRetries,
					*Orphan.TerritoryTag.ToString(),
					*Orphan.TerritoryGUID.ToString());
			}
			PendingCaptureSummaries.Empty();
			PendingCaptureRetryCount = 0;
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// P0-02: Live Replication Handlers
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritoryWorldState::SubscribeToLiveUpdates()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		Economy->OnEconomyTickFired.AddDynamic(this, &ATerritoryWorldState::OnEconomyTickLive);
		Economy->OnTransactionRecorded.AddDynamic(this, &ATerritoryWorldState::OnTransactionRecordedLive);
	}

	if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		Diplomacy->OnDiplomacyStateChanged.AddDynamic(this, &ATerritoryWorldState::OnDiplomacyChangedLive);
		Diplomacy->OnReputationChanged.AddDynamic(this, &ATerritoryWorldState::OnReputationChangedLive);
	}

	if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		Control->OnTerritoryControlChanged.AddDynamic(this, &ATerritoryWorldState::OnTerritoryControlChangedLive);
	}
}

void ATerritoryWorldState::UnsubscribeFromLiveUpdates()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		Economy->OnEconomyTickFired.RemoveDynamic(this, &ATerritoryWorldState::OnEconomyTickLive);
		Economy->OnTransactionRecorded.RemoveDynamic(this, &ATerritoryWorldState::OnTransactionRecordedLive);
	}

	if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		Diplomacy->OnDiplomacyStateChanged.RemoveDynamic(this, &ATerritoryWorldState::OnDiplomacyChangedLive);
		Diplomacy->OnReputationChanged.RemoveDynamic(this, &ATerritoryWorldState::OnReputationChangedLive);
	}

	if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		Control->OnTerritoryControlChanged.RemoveDynamic(this, &ATerritoryWorldState::OnTerritoryControlChangedLive);
	}
}

void ATerritoryWorldState::OnEconomyTickLive(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot)
{
	if (!HasAuthority() || !Faction.IsValid()) return;

	// Update or add the faction's treasury entry
	for (FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
	{
		if (Entry.Faction == Faction)
		{
			Entry.IncomePerTick = Snapshot.TotalIncome;
			Entry.CostsPerTick = Snapshot.TotalCosts;
			Entry.Treasury = Snapshot.Treasury;
			Entry.TerritoryCount = Snapshot.TerritoryCount;
			return;
		}
	}

	// Faction not yet in replicated array — add it
	FReplicatedFactionEconomy NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.IncomePerTick = Snapshot.TotalIncome;
	NewEntry.CostsPerTick = Snapshot.TotalCosts;
	NewEntry.Treasury = Snapshot.Treasury;
	NewEntry.TerritoryCount = Snapshot.TerritoryCount;
	ReplicatedTreasuries.Add(NewEntry);
}

void ATerritoryWorldState::OnTransactionRecordedLive(const FTerritoryTransaction& Transaction)
{
	if (!HasAuthority()) return;

	FReplicatedTransaction RepTx;
	RepTx.TransactionID = Transaction.TransactionID;
	RepTx.Faction = Transaction.Faction;
	RepTx.Type = Transaction.Type;
	RepTx.Amount = Transaction.Amount;
	RepTx.BalanceAfter = Transaction.BalanceAfter;
	RepTx.GameTime = Transaction.GameTime;
	RepTx.Reason = Transaction.Reason;
	RepTx.SourceTerritory = Transaction.SourceTerritory;
	ReplicatedTransactions.Add(RepTx);
}

void ATerritoryWorldState::OnDiplomacyChangedLive(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	if (!HasAuthority()) return;

	// Update or add treaty entry
	for (FReplicatedTreaty& Treaty : ReplicatedTreaties)
	{
		if ((Treaty.FactionA == FactionA && Treaty.FactionB == FactionB)
			|| (Treaty.FactionA == FactionB && Treaty.FactionB == FactionA))
		{
			Treaty.State = NewState;
			return;
		}
	}

	// New treaty
	FReplicatedTreaty NewTreaty;
	NewTreaty.TreatyID = FGuid::NewGuid();
	NewTreaty.FactionA = FactionA;
	NewTreaty.FactionB = FactionB;
	NewTreaty.State = NewState;
	ReplicatedTreaties.Add(NewTreaty);
}

void ATerritoryWorldState::OnReputationChangedLive(FGameplayTag Faction, int32 NewReputation)
{
	if (!HasAuthority() || !Faction.IsValid()) return;

	// Update or add reputation entry
	for (FReplicatedFactionReputation& Entry : ReplicatedReputation)
	{
		if (Entry.Faction == Faction)
		{
			Entry.Reputation = NewReputation;
			return;
		}
	}

	// New faction
	FReplicatedFactionReputation NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.Reputation = NewReputation;
	ReplicatedReputation.Add(NewEntry);
}

void ATerritoryWorldState::OnTerritoryControlChangedLive(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (!HasAuthority() || !Territory) return;

	const FGameplayTag TerrTag = Territory->GetTerritoryTag();

	// Update or add capture summary
	for (FReplicatedCaptureSummary& Summary : ReplicatedCaptureSummaries)
	{
		if (Summary.TerritoryTag == TerrTag)
		{
			Summary.CurrentOwner = NewOwner;
			Summary.ContestingFaction = Territory->IsContested() ? OldOwner : FGameplayTag();
			Summary.State = Territory->GetTerritoryState();
			Summary.ControlProgress = Territory->GetControlProgress();
			return;
		}
	}

	// New territory
	FReplicatedCaptureSummary NewSummary;
	NewSummary.TerritoryTag = TerrTag;
	NewSummary.TerritoryGUID = Territory->GetActorGUID_Implementation();
	NewSummary.CurrentOwner = NewOwner;
	NewSummary.ContestingFaction = Territory->IsContested() ? OldOwner : FGameplayTag();
	NewSummary.State = Territory->GetTerritoryState();
	NewSummary.ControlProgress = Territory->GetControlProgress();
	ReplicatedCaptureSummaries.Add(NewSummary);
}
