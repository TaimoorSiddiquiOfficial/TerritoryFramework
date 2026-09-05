#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "SaveSystemStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	FReplicatedTreaty MakeReplicatedTreaty(const FTreatyRecord& Treaty)
	{
		FReplicatedTreaty Result;
		Result.TreatyID = Treaty.GetCanonicalKey();
		Result.FactionA = Treaty.FactionA;
		Result.FactionB = Treaty.FactionB;
		Result.State = Treaty.State;
		Result.SignedGameTime = Treaty.SignedGameTime;
		Result.ExpiryGameTime = Treaty.ExpiryGameTime;
		Result.bPermanent = Treaty.bPermanent;
		return Result;
	}

	ETerritoryHierarchyLevel GetDefinitionHierarchyLevel(
		const UTerritoryDefinition* Definition)
	{
		if (Definition && Definition->IsA<UTerritoryCityDefinition>())
		{
			return ETerritoryHierarchyLevel::City;
		}
		if (Definition && Definition->IsA<UTerritoryDistrictDefinition>())
		{
			return ETerritoryHierarchyLevel::District;
		}
		return ETerritoryHierarchyLevel::Place;
	}

	int32 GetDefinitionChildCount(const UTerritoryDefinition* Definition)
	{
		if (const UTerritoryCityDefinition* City =
			Cast<UTerritoryCityDefinition>(Definition))
		{
			return City->Districts.Num();
		}
		if (const UTerritoryDistrictDefinition* District =
			Cast<UTerritoryDistrictDefinition>(Definition))
		{
			return District->Places.Num();
		}
		return 0;
	}

	bool MatchesCaptureIdentity(const FReplicatedCaptureSummary& Existing,
		const FGameplayTag& TerritoryTag, const FGuid& TerritoryGUID)
	{
		// Invalid GameplayTags compare equal. They must never collapse two rows that
		// are intentionally identified by different stable GUIDs.
		return (TerritoryTag.IsValid() && Existing.TerritoryTag == TerritoryTag)
			|| (TerritoryGUID.IsValid()
				&& Existing.TerritoryGUID == TerritoryGUID);
	}

	void InitializeDefinitionPoliticalState(const UTerritoryDefinition* Definition,
		FReplicatedCaptureSummary& Summary)
	{
		if (!Definition) return;
		Summary.Availability = Definition->InitialState == ETerritoryInitialState::Locked
			? ETerritoryAvailability::Locked : Definition->InitialAvailability;
		const bool bStartsClaimed = Definition->InitialOwningFaction.IsValid()
			&& Definition->InitialState != ETerritoryInitialState::Unclaimed;
		Summary.CurrentOwner = bStartsClaimed
			? Definition->InitialOwningFaction : FGameplayTag();
		Summary.State = bStartsClaimed
			? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Summary.ControlProgress = bStartsClaimed ? 1.f : 0.f;
	}
}

ATerritoryWorldState::ATerritoryWorldState()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

ATerritoryWorldState* ATerritoryWorldState::FindTerritoryWorldState(
	const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ATerritoryWorldState> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATerritoryWorldState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!WorldStateGUID.IsValid())
		{
			// P1-10: Missing GUID disables save/load only — live replication still subscribes
			UE_LOG(LogTerritory, Error, TEXT("TerritoryWorldState %s has no authored WorldStateGUID; save/load is disabled."),
				*GetPathName());
		}
		else
		{
			USaveSystemStatics::LoadSingleActor(this);
		}

		// P0-02: Subscribe to subsystem delegates for live replication
		// P1-10: Always subscribe regardless of GUID — live replication works without save
		SubscribeToLiveUpdates();
		RefreshStrategicDirectory();

		// Actor BeginPlay order is not guaranteed. Seed summaries for Territory actors
		// that registered before this WorldState; territories that start later publish
		// their own summary from ATerritoryVolume::BeginPlay.
		if (UTerritoryRegistrySubsystem* Registry =
			GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (!Territory) continue;
				PublishTerritorySummary(Territory);
			}
		}
	}
	else
	{
		// Initial replication normally invokes the OnRep handlers. This also hydrates
		// clients that join after an empty/default snapshot was established.
		SyncSubsystemsFromReplicatedState();
	}
}

void ATerritoryWorldState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
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
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedProductionSites);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedResourceSnapshots);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedTreaties);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedReputation);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedDiplomacyHistory);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedCaptureSummaries);
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedAssaults);
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
			ForceNetUpdate();
			return;
		}
	}

	FReplicatedFactionEconomy NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.IncomePerTick = Treasury.IncomePerTick;
	NewEntry.CostsPerTick = Treasury.CostsPerTick;
	NewEntry.TerritoryCount = Treasury.TerritoryCount;
	ReplicatedTreasuries.Add(NewEntry);
	ForceNetUpdate();
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

void ATerritoryWorldState::SetProductionState(
	const TArray<FTerritoryProductionCheckpoint>& Checkpoints,
	const TArray<FTerritoryProductionSiteRecord>& Sites,
	const TArray<FTerritoryFactionResourceSnapshot>& ResourceSnapshots)
{
	if (!HasAuthority()) return;
	SavedProductionCheckpoints = Checkpoints;
	ReplicatedProductionSites = Sites;
	ReplicatedResourceSnapshots = ResourceSnapshots;
	OnProductionStateChanged.Broadcast();
	ForceNetUpdate();
}

TArray<FTerritoryProductionSiteRecord> ATerritoryWorldState::GetProductionSitesForFaction(
	const FGameplayTag& Faction) const
{
	TArray<FTerritoryProductionSiteRecord> Result;
	for (const FTerritoryProductionSiteRecord& Site : ReplicatedProductionSites)
	{
		if (Site.OwnerFaction == Faction) Result.Add(Site);
	}
	return Result;
}

FTerritoryFactionResourceSnapshot ATerritoryWorldState::GetFactionResourceSnapshot(
	const FGameplayTag& Faction) const
{
	for (const FTerritoryFactionResourceSnapshot& Snapshot : ReplicatedResourceSnapshots)
	{
		if (Snapshot.Faction == Faction) return Snapshot;
	}
	return FTerritoryFactionResourceSnapshot();
}

// ─── Transaction API ───

void ATerritoryWorldState::RecordTransaction(const FReplicatedTransaction& Transaction)
{
	if (!HasAuthority()) return;
	ReplicatedTransactions.Add(Transaction);

	// Use MaxTransactionHistory from EconomySubsystem (default 500)
	const UTerritoryEconomySubsystem* Economy = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	const int32 MaxHistory = FMath::Max(0, Economy ? Economy->MaxTransactionHistory : 500);
	const int32 Excess = ReplicatedTransactions.Num() - MaxHistory;
	if (Excess > 0)
	{
		ReplicatedTransactions.RemoveAt(0, Excess);
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
	ForceNetUpdate();
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
			ForceNetUpdate();
			return;
		}
	}

	ReplicatedTreaties.Add(Treaty);
	ForceNetUpdate();
}

void ATerritoryWorldState::RemoveTreaty(const FGuid& TreatyID)
{
	if (!HasAuthority()) return;
	if (ReplicatedTreaties.RemoveAll(
		[&TreatyID](const FReplicatedTreaty& T) { return T.TreatyID == TreatyID; }) > 0)
	{
		ForceNetUpdate();
	}
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
			ForceNetUpdate();
			return;
		}
	}

	FReplicatedFactionReputation NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.Reputation = Value;
	ReplicatedReputation.Add(NewEntry);
	ForceNetUpdate();
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
	if (!HasAuthority() || (!Summary.TerritoryTag.IsValid()
		&& !Summary.TerritoryGUID.IsValid())) return;

	for (FReplicatedCaptureSummary& Entry : ReplicatedCaptureSummaries)
	{
		if (MatchesCaptureIdentity(Entry, Summary.TerritoryTag,
			Summary.TerritoryGUID))
		{
			const bool bPoliticalChange = Entry.Availability != Summary.Availability
				|| Entry.State != Summary.State
				|| Entry.CurrentOwner != Summary.CurrentOwner
				|| Entry.ContestingFaction != Summary.ContestingFaction;
			FReplicatedCaptureSummary Merged = Summary;
			// Runtime-only publishers from older integrations may omit directory
			// metadata. Never erase Definition identity that is already replicated.
			if (!Merged.bDefinitionBacked && Entry.bDefinitionBacked)
			{
				Merged.DisplayName = Entry.DisplayName;
				Merged.HierarchyLevel = Entry.HierarchyLevel;
				Merged.TotalChildren = Entry.TotalChildren;
				Merged.bDefinitionBacked = true;
			}
			Entry = MoveTemp(Merged);
			if (bPoliticalChange)
			{
				if (const UTerritoryDeveloperSettings* Settings =
					GetDefault<UTerritoryDeveloperSettings>();
					Settings && Settings->ShouldDebugWorldState())
				{
					UE_LOG(LogTerritory, Log,
						TEXT("[WorldState] updated %s availability=%d state=%d owner=%s contesting=%s"),
						*Entry.TerritoryTag.ToString(),
						static_cast<int32>(Entry.Availability),
						static_cast<int32>(Entry.State),
						*Entry.CurrentOwner.ToString(),
						*Entry.ContestingFaction.ToString());
				}
			}
			ForceNetUpdate();
			return;
		}
	}

	ReplicatedCaptureSummaries.Add(Summary);
	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugWorldState())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[WorldState] registered %s availability=%d state=%d owner=%s definition=%d"),
			*Summary.TerritoryTag.ToString(), static_cast<int32>(Summary.Availability),
			static_cast<int32>(Summary.State), *Summary.CurrentOwner.ToString(),
			Summary.bDefinitionBacked ? 1 : 0);
	}
	ForceNetUpdate();
}

void ATerritoryWorldState::PublishTerritorySummary(
	const ATerritoryVolume* Territory)
{
	if (!HasAuthority() || !IsValid(Territory)) return;

	const UTerritoryDefinition* Definition = Territory->GetTerritoryDefinition();
	if (Definition)
	{
		RegisterDefinitionHierarchy(Definition);
	}

	FReplicatedCaptureSummary Summary;
	Summary.TerritoryTag = Territory->GetTerritoryTag();
	Summary.TerritoryGUID = Territory->GetTerritoryGUID();
	Summary.ParentTerritoryTag = Territory->GetParentTerritoryTag();
	Summary.DisplayName = Territory->GetTerritoryDisplayName();
	Summary.CurrentOwner = Territory->GetOwningFaction();
	Summary.ContestingFaction = Territory->GetContestingFaction_Implementation();
	Summary.ControlProgress = Territory->GetControlProgress();
	Summary.State = Territory->GetTerritoryState();
	Summary.Availability = Territory->GetTerritoryAvailability();
	Summary.bDefinitionBacked = Definition != nullptr;
	if (Definition)
	{
		Summary.HierarchyLevel = GetDefinitionHierarchyLevel(Definition);
		Summary.TotalChildren = GetDefinitionChildCount(Definition);
	}
	else if (Territory->IsA<ATerritoryCity>())
	{
		Summary.HierarchyLevel = ETerritoryHierarchyLevel::City;
		Summary.TotalChildren = CastChecked<ATerritoryCity>(Territory)->GetDistricts().Num();
	}
	else if (Territory->IsA<ATerritoryDistrict>())
	{
		Summary.HierarchyLevel = ETerritoryHierarchyLevel::District;
		Summary.TotalChildren = CastChecked<ATerritoryDistrict>(Territory)->GetProperties().Num();
	}
	SetCaptureSummary(Summary);
}

void ATerritoryWorldState::RegisterDefinitionHierarchy(
	const UTerritoryDefinition* Definition)
{
	if (!HasAuthority() || !Definition) return;

	TSet<const UTerritoryDefinition*> Visited;
	TFunction<void(const UTerritoryDefinition*)> RegisterRecursive;
	RegisterRecursive = [this, &Visited, &RegisterRecursive](
		const UTerritoryDefinition* Current)
	{
		if (!Current || Visited.Contains(Current)) return;
		Visited.Add(Current);

		TArray<const UTerritoryDefinition*> Children;
		if (const UTerritoryCityDefinition* City =
			Cast<UTerritoryCityDefinition>(Current))
		{
			for (const UTerritoryDistrictDefinition* District : City->Districts)
			{
				if (District) Children.Add(District);
			}
		}
		else if (const UTerritoryDistrictDefinition* District =
			Cast<UTerritoryDistrictDefinition>(Current))
		{
			for (const UTerritoryPlaceDefinition* Place : District->Places)
			{
				if (Place) Children.Add(Place);
			}
		}
		for (const UTerritoryDefinition* Child : Children)
		{
			RegisterRecursive(Child);
		}

		FReplicatedCaptureSummary Seed;
		Seed.TerritoryTag = Current->TerritoryTag;
		Seed.TerritoryGUID = Current->StableTerritoryGUID;
		Seed.ParentTerritoryTag = Current->DerivedParentTerritoryTag;
		Seed.DisplayName = Current->DisplayName;
		Seed.HierarchyLevel = GetDefinitionHierarchyLevel(Current);
		Seed.TotalChildren = Children.Num();
		Seed.bDefinitionBacked = true;
		InitializeDefinitionPoliticalState(Current, Seed);

		if (!Children.IsEmpty())
		{
			FGameplayTag CommonOwner;
			bool bAllSecure = true;
			bool bAnyPoliticalControl = false;
			for (const UTerritoryDefinition* Child : Children)
			{
				const FReplicatedCaptureSummary ChildSummary =
					GetCaptureSummary(Child->TerritoryTag);
				if (ChildSummary.TerritoryTag != Child->TerritoryTag)
				{
					bAllSecure = false;
					continue;
				}
				bAnyPoliticalControl |= ChildSummary.CurrentOwner.IsValid()
					|| ChildSummary.State == ETerritoryState::Contested;
				if (ChildSummary.Availability != ETerritoryAvailability::Unlocked
					|| ChildSummary.State != ETerritoryState::Claimed
					|| !ChildSummary.CurrentOwner.IsValid())
				{
					bAllSecure = false;
					continue;
				}
				if (!CommonOwner.IsValid()) CommonOwner = ChildSummary.CurrentOwner;
				else if (CommonOwner != ChildSummary.CurrentOwner) bAllSecure = false;
			}
			Seed.CurrentOwner = bAllSecure ? CommonOwner : FGameplayTag();
			Seed.State = bAllSecure && CommonOwner.IsValid()
				? ETerritoryState::Claimed
				: bAnyPoliticalControl ? ETerritoryState::Contested
					: ETerritoryState::Unclaimed;
			Seed.ControlProgress = Seed.State == ETerritoryState::Claimed ? 1.f : 0.f;
		}

		FReplicatedCaptureSummary* Existing = ReplicatedCaptureSummaries.FindByPredicate(
			[&Seed](const FReplicatedCaptureSummary& Entry)
			{
				return MatchesCaptureIdentity(Entry, Seed.TerritoryTag,
					Seed.TerritoryGUID);
			});
		if (Existing)
		{
			// Definition registration is presentation reconciliation, never a second
			// ownership authority. Preserve the live/saved political snapshot.
			Existing->TerritoryTag = Seed.TerritoryTag;
			Existing->TerritoryGUID = Seed.TerritoryGUID;
			Existing->ParentTerritoryTag = Seed.ParentTerritoryTag;
			Existing->DisplayName = Seed.DisplayName;
			Existing->HierarchyLevel = Seed.HierarchyLevel;
			Existing->TotalChildren = Seed.TotalChildren;
			Existing->bDefinitionBacked = true;
		}
		else if (Seed.TerritoryTag.IsValid() || Seed.TerritoryGUID.IsValid())
		{
			ReplicatedCaptureSummaries.Add(MoveTemp(Seed));
		}
	};

	RegisterRecursive(Definition);
	ForceNetUpdate();
}

void ATerritoryWorldState::RefreshStrategicDirectory()
{
	if (!HasAuthority()) return;
	for (const UTerritoryCityDefinition* City : CampaignCities)
	{
		RegisterDefinitionHierarchy(City);
	}
	if (UWorld* World = GetWorld())
	{
		if (const UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			for (const ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (Territory) RegisterDefinitionHierarchy(
					Territory->GetTerritoryDefinition());
			}
		}
	}
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

int32 ATerritoryWorldState::GetClaimedDistrictCountForFaction(
	const FGameplayTag& Faction) const
{
	return CountClaimedDistrictsForFaction(ReplicatedCaptureSummaries, Faction);
}

int32 ATerritoryWorldState::CountClaimedDistrictsForFaction(
	TConstArrayView<FReplicatedCaptureSummary> Summaries,
	const FGameplayTag& Faction)
{
	if (!Faction.IsValid()) return 0;

	int32 Count = 0;
	TSet<FGameplayTag> CountedTags;
	TSet<FGuid> CountedGuids;
	for (const FReplicatedCaptureSummary& Summary : Summaries)
	{
		if (Summary.HierarchyLevel != ETerritoryHierarchyLevel::District
			|| Summary.Availability != ETerritoryAvailability::Unlocked
			|| Summary.State != ETerritoryState::Claimed
			|| Summary.CurrentOwner != Faction)
		{
			continue;
		}

		// The live directory is already unique, but defensive identity filtering
		// prevents a malformed migration cache from inflating a story condition.
		if ((Summary.TerritoryTag.IsValid() && CountedTags.Contains(Summary.TerritoryTag))
			|| (Summary.TerritoryGUID.IsValid() && CountedGuids.Contains(Summary.TerritoryGUID))) continue;
		if (!Summary.TerritoryTag.IsValid() && !Summary.TerritoryGUID.IsValid()) continue;
		if (Summary.TerritoryTag.IsValid())
		{
			CountedTags.Add(Summary.TerritoryTag);
		}
		if (Summary.TerritoryGUID.IsValid())
		{
			CountedGuids.Add(Summary.TerritoryGUID);
		}
		++Count;
	}
	return Count;
}

bool ATerritoryWorldState::HasContestedTerritoryBetweenFactions(
	const FGameplayTag& FactionA, const FGameplayTag& FactionB,
	const FGameplayTag& ExcludedTerritoryTag) const
{
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB)
	{
		return false;
	}
	for (const FReplicatedCaptureSummary& Summary : ReplicatedCaptureSummaries)
	{
		if (Summary.State != ETerritoryState::Contested
			|| (ExcludedTerritoryTag.IsValid()
				&& Summary.TerritoryTag == ExcludedTerritoryTag))
		{
			continue;
		}
		if ((Summary.CurrentOwner == FactionA && Summary.ContestingFaction == FactionB)
			|| (Summary.CurrentOwner == FactionB && Summary.ContestingFaction == FactionA))
		{
			return true;
		}
	}
	return false;
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

			SavedProductionCheckpoints = Economy->GetProductionCheckpoints();
			ReplicatedProductionSites = Economy->GetAllProductionSites();
			ReplicatedResourceSnapshots = Economy->GetAllResourceSnapshots();
		}

		if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
		{
			ReplicatedTreaties.Empty();
			for (const FTreatyRecord& Treaty : Diplomacy->GetAllTreaties())
			{
				ReplicatedTreaties.Add(MakeReplicatedTreaty(Treaty));
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
			// Merge loaded actors into the runtime read model. Do not clear summaries
			// for World Partition actors that are currently unloaded; hierarchy and
			// effective-availability queries still need their last authoritative row.
			for (const ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (!Territory) continue;
				PublishTerritorySummary(Territory);
			}
		}

		if (UTerritoryCounterAttackSubsystem* Counterattacks =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			ReplicatedAssaults = Counterattacks->GetPersistentState();
			SavedAssaultCycles = Counterattacks->GetPersistentCycleState();
		}
	}

	SavedTreasuries = ReplicatedTreasuries;
	SavedTransactions = ReplicatedTransactions;
	SavedProductionSites = ReplicatedProductionSites;
	SavedResourceSnapshots = ReplicatedResourceSnapshots;
	SavedTreaties = ReplicatedTreaties;
	SavedReputation = ReplicatedReputation;
	SavedDiplomacyHistory = ReplicatedDiplomacyHistory;
	SavedAssaults = ReplicatedAssaults;
	// This is a presentation/query cache only. It prevents unloaded World Partition
	// rows from reverting to Definition defaults after a restart, but is never
	// imported into ATerritoryVolume ownership.
	SavedStrategicDirectory = ReplicatedCaptureSummaries;
}

void ATerritoryWorldState::ImportPersistentState()
{
	if (!HasAuthority()) return;

	// Direct assignment — no artificial transactions
	ReplicatedTreasuries = SavedTreasuries;
	ReplicatedTransactions = SavedTransactions;
	ReplicatedProductionSites = SavedProductionSites;
	ReplicatedResourceSnapshots = SavedResourceSnapshots;
	ReplicatedTreaties = SavedTreaties;
	ReplicatedReputation = SavedReputation;
	ReplicatedDiplomacyHistory = SavedDiplomacyHistory;
	ReplicatedAssaults = SavedAssaults;
	ReplicatedCaptureSummaries = SavedStrategicDirectory;
	// Capture rows are deliberately not pushed into Territory actors. Loaded Volumes
	// restore their own OwnershipData and then publish over this cached read model.

	SyncSubsystemsFromReplicatedState();
}

void ATerritoryWorldState::SyncSubsystemsFromReplicatedState()
{
	SyncEconomySubsystemFromReplicatedState();
	SyncDiplomacySubsystemFromReplicatedState();
	SyncCounterAttackSubsystemFromReplicatedState();
	// P0-03: Capture summary sync removed — TerritoryVolume is sole ownership authority.
	// ApplyPendingCaptureSummaries and OnTerritoryRegistered removed entirely.
}

void ATerritoryWorldState::OnRep_EconomyState()
{
	SyncEconomySubsystemFromReplicatedState();
}

void ATerritoryWorldState::OnRep_ProductionState()
{
	SyncEconomySubsystemFromReplicatedState();
	OnProductionStateChanged.Broadcast();
}

void ATerritoryWorldState::OnRep_DiplomacyState()
{
	SyncDiplomacySubsystemFromReplicatedState();
}

void ATerritoryWorldState::OnRep_AssaultState()
{
	SyncCounterAttackSubsystemFromReplicatedState();
}

void ATerritoryWorldState::SyncEconomySubsystemFromReplicatedState()
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
		Economy->RestoreProductionState(
			HasAuthority() ? SavedProductionCheckpoints
				: TArray<FTerritoryProductionCheckpoint>(),
			ReplicatedProductionSites, ReplicatedResourceSnapshots);
	}
}

void ATerritoryWorldState::SyncDiplomacySubsystemFromReplicatedState()
{
	UWorld* World = GetWorld();
	if (!World) return;

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
		if (HasAuthority())
		{
			// Publish the normalized authoritative rows after migration, including
			// stable IDs, so late joiners cannot observe the original malformed cache.
			ReplicatedTreaties.Reset();
			for (const FTreatyRecord& Treaty : Diplomacy->GetAllTreaties())
			{
				ReplicatedTreaties.Add(MakeReplicatedTreaty(Treaty));
			}
			ForceNetUpdate();
		}
	}
}

void ATerritoryWorldState::SyncCounterAttackSubsystemFromReplicatedState()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryCounterAttackSubsystem* Counterattacks =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			if (HasAuthority())
			{
				Counterattacks->RestorePersistentState(ReplicatedAssaults, SavedAssaultCycles);
				// Active NPC pointers are never campaign state. RestorePersistentState
				// normalizes saved live survivors into finite pending reserve; publish that
				// authoritative read model immediately so a late join never sees phantom
				// AliveForce counts from the serialized snapshot.
				ReplicatedAssaults = Counterattacks->GetPersistentState();
				ForceNetUpdate();
			}
			else
			{
				Counterattacks->RestorePersistentState(ReplicatedAssaults);
			}
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
		Diplomacy->OnDiplomacyEvent.AddDynamic(this, &ATerritoryWorldState::OnDiplomacyEventLive);
		Diplomacy->OnReputationChanged.AddDynamic(this, &ATerritoryWorldState::OnReputationChangedLive);
	}

	if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		Control->OnTerritoryControlChanged.AddDynamic(this, &ATerritoryWorldState::OnTerritoryControlChangedLive);
	}
	if (UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
	{
		Counterattacks->OnAssaultChanged.AddDynamic(this, &ATerritoryWorldState::OnAssaultChangedLive);
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
		Diplomacy->OnDiplomacyEvent.RemoveDynamic(this, &ATerritoryWorldState::OnDiplomacyEventLive);
		Diplomacy->OnReputationChanged.RemoveDynamic(this, &ATerritoryWorldState::OnReputationChangedLive);
	}

	if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		Control->OnTerritoryControlChanged.RemoveDynamic(this, &ATerritoryWorldState::OnTerritoryControlChangedLive);
	}
	if (UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
	{
		Counterattacks->OnAssaultChanged.RemoveDynamic(this, &ATerritoryWorldState::OnAssaultChangedLive);
	}
}

void ATerritoryWorldState::OnAssaultChangedLive(const FTerritoryAssaultRecord& Assault)
{
	if (!HasAuthority() || !Assault.AssaultID.IsValid()) return;
	if (FTerritoryAssaultRecord* Existing = ReplicatedAssaults.FindByPredicate(
		[&Assault](const FTerritoryAssaultRecord& Record)
		{
			return Record.AssaultID == Assault.AssaultID;
		}))
	{
		*Existing = Assault;
	}
	else
	{
		ReplicatedAssaults.Add(Assault);
	}
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const int32 MaximumRecords = FMath::Max(0, Settings ? Settings->MaxRetainedAssaultRecords : 100);
	if (ReplicatedAssaults.Num() > MaximumRecords)
	{
		ReplicatedAssaults.Sort([](const FTerritoryAssaultRecord& A, const FTerritoryAssaultRecord& B)
		{
			if (A.IsTerminal() != B.IsTerminal()) return A.IsTerminal();
			return A.CapturedGameTime < B.CapturedGameTime;
		});
		while (ReplicatedAssaults.Num() > MaximumRecords
			&& ReplicatedAssaults[0].IsTerminal())
		{
			ReplicatedAssaults.RemoveAt(0);
		}
	}
	ForceNetUpdate();
}

void ATerritoryWorldState::OnEconomyTickLive(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot)
{
	if (!HasAuthority() || !Faction.IsValid()) return;

	// Update or add the faction's treasury entry
	for (FReplicatedFactionEconomy& Entry : ReplicatedTreasuries)
	{
		if (Entry.Faction == Faction)
		{
			// P0-N2: Treasury field intentionally excluded — matches ExportPersistentState
			// which documents "Treasury no longer used". Faction wealth lives in member inventories.
			Entry.IncomePerTick = Snapshot.TotalIncome;
			Entry.CostsPerTick = Snapshot.TotalCosts;
			Entry.TerritoryCount = Snapshot.TerritoryCount;
			ForceNetUpdate();
			return;
		}
	}

	// Faction not yet in replicated array — add it
	FReplicatedFactionEconomy NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.IncomePerTick = Snapshot.TotalIncome;
	NewEntry.CostsPerTick = Snapshot.TotalCosts;
	NewEntry.TerritoryCount = Snapshot.TerritoryCount;
	ReplicatedTreasuries.Add(NewEntry);
	ForceNetUpdate();
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

	// P0-N1: Cap replicated transactions to prevent unbounded array growth.
	// Mirrors EconomySubsystem's MaxTransactionHistory cap.
	const UTerritoryEconomySubsystem* Economy = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	const int32 MaxReplicatedTransactions = FMath::Max(0, Economy ? Economy->MaxTransactionHistory : 500);
	const int32 Excess = ReplicatedTransactions.Num() - MaxReplicatedTransactions;
	if (Excess > 0)
	{
		ReplicatedTransactions.RemoveAt(0, Excess);
	}
	ForceNetUpdate();
}

void ATerritoryWorldState::OnDiplomacyChangedLive(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	if (!HasAuthority() || !FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;

	auto MatchesPair = [&FactionA, &FactionB](const FReplicatedTreaty& Treaty)
	{
		return (Treaty.FactionA == FactionA && Treaty.FactionB == FactionB)
			|| (Treaty.FactionA == FactionB && Treaty.FactionB == FactionA);
	};

	// None is represented by the absence of a Territory treaty. Keeping a row with
	// State=None makes the client read model disagree with the authoritative subsystem.
	if (NewState == EDiplomacyState::None)
	{
		const int32 Removed = ReplicatedTreaties.RemoveAll(MatchesPair);
		if (Removed > 0) ForceNetUpdate();
		return;
	}

	FReplicatedTreaty Snapshot;
	bool bHasAuthoritativeSnapshot = false;
	if (UWorld* World = GetWorld())
	{
		if (const UTerritoryDiplomacySubsystem* Diplomacy =
			World->GetSubsystem<UTerritoryDiplomacySubsystem>())
		{
			for (const FTreatyRecord& Record : Diplomacy->GetTreatiesForFaction(FactionA))
			{
				if ((Record.FactionA == FactionA && Record.FactionB == FactionB)
					|| (Record.FactionA == FactionB && Record.FactionB == FactionA))
				{
					Snapshot = MakeReplicatedTreaty(Record);
					bHasAuthoritativeSnapshot = true;
					break;
				}
			}
		}
	}

	for (FReplicatedTreaty& Existing : ReplicatedTreaties)
	{
		if (!MatchesPair(Existing)) continue;
		if (bHasAuthoritativeSnapshot)
		{
			Existing = Snapshot;
		}
		else
		{
			// Delegate ordering normally makes the rich subsystem record available.
			// Preserve the last replicated timing metadata if a custom broadcaster fires
			// before that record is visible instead of silently resetting the treaty.
			Existing.State = NewState;
		}
		ForceNetUpdate();
		return;
	}

	if (!bHasAuthoritativeSnapshot)
	{
		FTreatyRecord Fallback;
		Fallback.FactionA = FactionA;
		Fallback.FactionB = FactionB;
		Fallback.State = NewState;
		Snapshot = MakeReplicatedTreaty(Fallback);
	}
	ReplicatedTreaties.Add(Snapshot);
	ForceNetUpdate();
}

void ATerritoryWorldState::OnDiplomacyEventLive(const FDiplomacyEvent& Event)
{
	if (!HasAuthority()) return;
	ReplicatedDiplomacyHistory.Add(Event);
	constexpr int32 MaxDiplomacyHistory = 500;
	const int32 Excess = ReplicatedDiplomacyHistory.Num() - MaxDiplomacyHistory;
	if (Excess > 0)
	{
		ReplicatedDiplomacyHistory.RemoveAt(0, Excess);
	}
	ForceNetUpdate();
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
			ForceNetUpdate();
			return;
		}
	}

	// New faction
	FReplicatedFactionReputation NewEntry;
	NewEntry.Faction = Faction;
	NewEntry.Reputation = NewReputation;
	ReplicatedReputation.Add(NewEntry);
	ForceNetUpdate();
}

void ATerritoryWorldState::OnTerritoryControlChangedLive(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (!HasAuthority() || !Territory) return;
	(void)OldOwner;
	(void)NewOwner;
	PublishTerritorySummary(Territory);
}
