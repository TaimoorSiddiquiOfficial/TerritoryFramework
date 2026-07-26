#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "SaveSystemStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

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
			WorldStateGUID = FGuid::NewGuid();
		}

		USaveSystemStatics::LoadSingleActor(this);
	}
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
				Summary.CurrentOwner = Territory->GetOwningFaction();
				Summary.ContestingFaction = Territory->GetContestingFaction();
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

	// Sync capture state — restore in-progress contests from saved summaries.
	// Territories are guaranteed to be registered at this point (BeginPlay order).
	if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
		{
			for (const FReplicatedCaptureSummary& Summary : ReplicatedCaptureSummaries)
			{
				ATerritoryVolume* Territory = Registry->GetTerritoryByTag(Summary.TerritoryTag);
				if (!Territory) continue;

				// Only set ownership if it actually changed — avoids unnecessary
				// guard despawn/re-spawn and duplicate ownership-change events.
				if (Summary.CurrentOwner != Territory->GetOwningFaction())
				{
					Territory->SetOwningFaction(Summary.CurrentOwner);
				}
				if (Summary.State == ETerritoryState::Contested)
				{
					Territory->SetTerritoryState(ETerritoryState::Contested);
					Control->RestoreCaptureState(Territory, Summary.ContestingFaction, Summary.ControlProgress);
				}
				else if (Summary.CurrentOwner.IsValid())
				{
					Territory->SetTerritoryState(ETerritoryState::Claimed);
				}
			}
		}
	}
}
