#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryTypes.h"
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
	// The EconomySubsystem holds the authoritative treasury/transaction state;
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
			int32 TxLimit = Economy ? Economy->MaxTransactionHistory : 500;
			for (const FGameplayTag& Faction : Factions)
			{
				TArray<FTerritoryTransaction> SubsystemTx = Economy->GetTransactionHistory(Faction, TxLimit);
				for (const FTerritoryTransaction& Tx : SubsystemTx)
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
		}
	}

	SavedTreasuries = ReplicatedTreasuries;
	SavedTransactions = ReplicatedTransactions;
	SavedTreaties = ReplicatedTreaties;
	SavedReputation = ReplicatedReputation;
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
		for (const FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
		{
			FTerritoryTreasury Treasury;
			Treasury.IncomePerTick = Entry.IncomePerTick;
			Treasury.CostsPerTick = Entry.CostsPerTick;
			Treasury.TerritoryCount = Entry.TerritoryCount;
			Economy->SetFactionTreasury(Entry.Faction, Treasury);
		}
	}

	// Sync diplomacy subsystem
	if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		for (const FReplicatedTreaty& Treaty : ReplicatedTreaties)
		{
			Diplomacy->SetDiplomacyState(Treaty.FactionA, Treaty.FactionB, Treaty.State);
		}
		for (const FReplicatedFactionReputation& Entry : ReplicatedReputation)
		{
			Diplomacy->SetReputation(Entry.Faction, Entry.Reputation);
		}
		Diplomacy->SyncToGameState();
	}
}
