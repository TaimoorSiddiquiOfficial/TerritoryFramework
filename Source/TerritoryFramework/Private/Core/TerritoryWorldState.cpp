#include "Core/TerritoryWorldState.h"
#include "Core/TerritorySaveSerialization.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "SaveSystemStatics.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/Level.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

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
		Result.bReputationDerived = Treaty.bReputationDerived;
		Result.bNarrativeObserved = Treaty.bNarrativeObserved;
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
		Summary.Availability = TerritoryResolveInitialAvailability(
			Definition->InitialState, Definition->InitialAvailability);
		// One shared rule decides ownership, and both the owner tag and the State are derived from
		// that single answer. This used to be a second, hand-written copy of the rule, which is how
		// a summary could in principle report a State that disagreed with its own CurrentOwner.
		const bool bStartsClaimed =
			TerritoryResolveInitialPoliticalState(
				Definition->InitialState,
				Definition->InitialOwningFaction.IsValid()) == ETerritoryState::Claimed;
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
		// Some project GameModes reach BeginPlay without a Native save object.
		// Let Native interpret its own save URL/new-game policy before loading us.
		if (UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			if (!Save->GetSaveObject()) Save->InitializeSaveSystem(*GetWorld());
		}
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

		// The delegates above only fire when a relationship changes. A session that opens
		// with an authored war (or any restored treaty) would otherwise replicate none of it:
		// the subsystem loads from the GameState in OnWorldBeginPlay, which runs before this
		// actor BeginPlay, and LoadFromGameState broadcasts nothing.
		PublishDiplomacyReadModel();

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
	DOREPLIFETIME(ATerritoryWorldState, ReplicatedReputationSubjectFaction);
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

void ATerritoryWorldState::Serialize(FArchive& Ar)
{
	FTerritorySaveSerializationScope SaveScope(*this, Ar);
	Super::Serialize(Ar);
}

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
	if (IsDirectoryIdentityRetired(Summary.TerritoryGUID)) return;

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
			ReconcileUnloadedAncestors(Summary.TerritoryTag);
			ForceNetUpdate();
			return;
		}
	}

	ReplicatedCaptureSummaries.Add(Summary);
	ReconcileUnloadedAncestors(Summary.TerritoryTag);
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
	Summary.FormerOwningFactions = Territory->GetOwnershipData().FormerOwningFactions;
	Summary.CapturedBy = Territory->GetOwnershipData().CapturedBy;
	Summary.CapturedFor = Territory->GetOwnershipData().CapturedFor;
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
	TSet<FGameplayTag> ChangedParents;
	TFunction<void(const UTerritoryDefinition*)> RegisterRecursive;
	RegisterRecursive = [this, &Visited, &ChangedParents, &RegisterRecursive](
		const UTerritoryDefinition* Current)
	{
		if (!Current || Visited.Contains(Current) || IsDirectoryIdentityRetired(Current->StableTerritoryGUID)) return;
		Visited.Add(Current);
		if (Current->TerritoryTag.IsValid())
		{
			RegisteredHierarchyDefinitions.Add(Current->TerritoryTag,
				const_cast<UTerritoryDefinition*>(Current));
		}

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
		uint32 TopologyRevision = HashCombineFast(GetTypeHash(Current->StableTerritoryGUID),
			GetTypeHash(GetDefinitionChildCount(Current)));
		for (const UTerritoryDefinition* Child : Children)
		{
			TopologyRevision = HashCombineFast(TopologyRevision, GetTypeHash(Child->TerritoryTag));
			TopologyRevision = HashCombineFast(TopologyRevision, GetTypeHash(Child->StableTerritoryGUID));
			TopologyRevision = HashCombineFast(TopologyRevision, GetTypeHash(Child->DerivedParentTerritoryTag));
		}
		const uint32* PriorRevision = RegisteredHierarchyRevisions.Find(Current->TerritoryTag);
		if (!PriorRevision || *PriorRevision != TopologyRevision)
			ChangedParents.Add(Current->TerritoryTag);
		RegisteredHierarchyRevisions.Add(Current->TerritoryTag, TopologyRevision);

		FReplicatedCaptureSummary Seed;
		Seed.TerritoryTag = Current->TerritoryTag;
		Seed.TerritoryGUID = Current->StableTerritoryGUID;
		Seed.ParentTerritoryTag = Current->DerivedParentTerritoryTag;
		Seed.DisplayName = Current->DisplayName;
		Seed.HierarchyLevel = GetDefinitionHierarchyLevel(Current);
		Seed.TotalChildren = GetDefinitionChildCount(Current);
		Seed.bDefinitionBacked = true;
		InitializeDefinitionPoliticalState(Current, Seed);

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
	ReconcileUnloadedHierarchy(&ChangedParents);
	for (const FGameplayTag& Changed : ChangedParents) ReconcileUnloadedAncestors(Changed);
	ForceNetUpdate();
}

void ATerritoryWorldState::ReconcileUnloadedAncestors(const FGameplayTag& ChangedChild)
{
	if (!HasAuthority()) return;
	const auto* Registry = GetWorld() ? GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	ATerritoryVolume* Source = Registry ? Registry->GetTerritoryByTag(ChangedChild) : nullptr;
	TSet<FGameplayTag> Parents;
	TArray<FGameplayTag> OrderedParents;
	FGameplayTag Tag = ChangedChild;
	while (const auto* Definition = RegisteredHierarchyDefinitions.Find(Tag))
	{
		const FGameplayTag Parent = *Definition ? (*Definition)->DerivedParentTerritoryTag : FGameplayTag();
		if (!Parent.IsValid() || Parent == ChangedChild || Parents.Contains(Parent)) break;
		Parents.Add(Parent);
		OrderedParents.Add(Parent);
		Tag = Parent;
	}
	// Complete each level before its parent reads it. An absent District can feed
	// a loaded City: that City must still commit through its own reducer/Volume.
	// ImportPersistentState intentionally calls only ReconcileUnloadedHierarchy;
	// Native restores loaded actors independently of the saved directory.
	for (const FGameplayTag& Parent : OrderedParents)
	{
		const TSet<FGameplayTag> ThisLevel{Parent};
		ReconcileUnloadedHierarchy(&ThisLevel);
		ATerritoryVolume* Loaded = Registry ? Registry->GetTerritoryByTag(Parent) : nullptr;
		const auto* Definition = RegisteredHierarchyDefinitions.Find(Parent);
		if (!IsValid(Loaded) || !Definition || !*Definition
			|| Loaded->GetTerritoryGUID() != (*Definition)->StableTerritoryGUID) continue;

		if (TransitionFrameDepth > 0)
		{
			// A source transition is still unwinding, so committing this ancestor now would fire
			// its events against a child that has not finished changing - guards not yet
			// refreshed, availability not yet reconciled, the child's own state events not yet
			// fired. Queue the pair and let the outermost frame exit drain it, when the child
			// commit is complete and its transition context is still installed.
			//
			// The directory pass above deliberately stays here rather than moving into the queue:
			// it is a read model the in-flight transition may already depend on, and deferring it
			// would hide the change from anything that reads the row during the transition.
			DeferredLoadedAncestorReconciles.Emplace(Parent, ChangedChild);
			continue;
		}

		ReconcileLoadedAncestor(Loaded, Source);
	}
}

void ATerritoryWorldState::ReconcileLoadedAncestor(ATerritoryVolume* Loaded,
	ATerritoryVolume* Source)
{
	if (!IsValid(Loaded)) return;
	if (auto* City = Cast<ATerritoryCity>(Loaded)) City->ReconcileDerivedControl(Source);
	else if (auto* District = Cast<ATerritoryDistrict>(Loaded)) District->ReconcileDerivedControl(Source);
	// A Native actor may already match the derived result while its directory
	// still contains the older saved value. A no-op Volume commit emits nothing.
	PublishTerritorySummary(Loaded);
}

void ATerritoryWorldState::EnterTransitionFrame()
{
	++TransitionFrameDepth;
}

void ATerritoryWorldState::ExitTransitionFrame()
{
	if (TransitionFrameDepth > 0) --TransitionFrameDepth;
	if (TransitionFrameDepth > 0) return;
	DrainDeferredAncestorReconciles();
}

void ATerritoryWorldState::DrainDeferredAncestorReconciles()
{
	// A commit performed by the drain opens and closes its own frame, so its exit re-enters here.
	// Returning immediately is correct: the loop below keeps popping, and the entries that nested
	// exit queued are already in the array it is walking.
	if (bDrainingDeferredReconciles) return;
	if (DeferredLoadedAncestorReconciles.IsEmpty()) return;
	// Authority-gated like every sibling reconciliation entry point. A client never queues - the
	// producer returns before queueing without authority - so this is a contract guard, not the
	// thing that keeps the queue empty.
	if (!HasAuthority())
	{
		DeferredLoadedAncestorReconciles.Empty();
		return;
	}
	TGuardValue<bool> DrainGuard(bDrainingDeferredReconciles, true);

	// Bounded rather than trusting the topology: a malformed or self-referential hierarchy must
	// not hang the transition. Dropping the remainder with a warning is recoverable - the next
	// transition rebuilds the queue - whereas spinning is not.
	constexpr int32 MaxAncestorReconcilesPerFrame = 64;
	int32 Drained = 0;
	int32 Index = 0;
	while (Index < DeferredLoadedAncestorReconciles.Num())
	{
		if (Index >= MaxAncestorReconcilesPerFrame)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[TransitionFrame] Deferred ancestor reconcile cap (%d) reached; dropping %d queued entries rather than hanging the transition."),
				MaxAncestorReconcilesPerFrame, DeferredLoadedAncestorReconciles.Num() - Index);
			break;
		}
		const TPair<FGameplayTag, FGameplayTag> Entry = DeferredLoadedAncestorReconciles[Index];
		++Index;
		++Drained;

		// Resolved now rather than captured at queue time: the pair holds tags precisely so a
		// queued entry cannot dangle, and the child is re-resolved to the actor that exists at
		// drain time rather than the one that existed when the transition began.
		const auto* Registry = GetWorld()
			? GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
		if (!Registry) continue;
		ReconcileLoadedAncestor(Registry->GetTerritoryByTag(Entry.Key),
			Registry->GetTerritoryByTag(Entry.Value));
	}
	if (Index > 0) DeferredLoadedAncestorReconciles.RemoveAt(0, Index, EAllowShrinking::No);
}

ATerritoryWorldState::FTransitionFrameScope::FTransitionFrameScope(
	ATerritoryWorldState* InWorldState)
	: WorldState(InWorldState)
{
	if (ATerritoryWorldState* Pinned = WorldState.Get()) Pinned->EnterTransitionFrame();
}

ATerritoryWorldState::FTransitionFrameScope::~FTransitionFrameScope()
{
	if (ATerritoryWorldState* Pinned = WorldState.Get()) Pinned->ExitTransitionFrame();
}

void ATerritoryWorldState::ReconcileUnloadedHierarchy(const TSet<FGameplayTag>* ParentsToRebuild)
{
	if (!HasAuthority()) return;
	const auto* Registry = GetWorld() ? GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	TSet<const UTerritoryDefinition*> Visited;
	TFunction<void(const UTerritoryDefinition*)> Reduce;
	Reduce = [&](const UTerritoryDefinition* Parent)
	{
		if (!Parent || Visited.Contains(Parent)) return;
		Visited.Add(Parent);
		TArray<const UTerritoryDefinition*> Children;
		if (const auto* City = Cast<UTerritoryCityDefinition>(Parent))
			for (const UTerritoryDistrictDefinition* District : City->Districts) Children.Add(District);
		else if (const auto* District = Cast<UTerritoryDistrictDefinition>(Parent))
			for (const UTerritoryPlaceDefinition* Place : District->Places) Children.Add(Place);
		else return;
		for (const auto* Child : Children) Reduce(Child);
		if (ParentsToRebuild && !ParentsToRebuild->Contains(Parent->TerritoryTag)) return;
		// Loaded parents still commit through their own hierarchy lifecycle.
		if (Registry && Registry->GetTerritoryByTag(Parent->TerritoryTag)) return;
		FReplicatedCaptureSummary* Summary = ReplicatedCaptureSummaries.FindByPredicate(
			[Parent](const FReplicatedCaptureSummary& Row)
			{ return Row.TerritoryTag == Parent->TerritoryTag && Row.TerritoryGUID == Parent->StableTerritoryGUID; });
		if (!Summary) return;
		TArray<TerritoryHierarchyPolicy::FChildControlView> Views;
		TSet<FGameplayTag> Seen;
		int32 InconsistentChildren = 0;
		int32 UnresolvedChildren = 0;
		for (const UTerritoryDefinition* Child : Children)
		{
			auto& View = Views.AddDefaulted_GetRef();
			if (!Child || !Child->TerritoryTag.IsValid() || Seen.Contains(Child->TerritoryTag)
				|| Child->DerivedParentTerritoryTag != Parent->TerritoryTag)
			{
				// An empty or duplicate authored tag is a topology defect rather than a
				// hierarchy still arriving. It reduces to a default view either way, and
				// committing that would clear a restored owner and record a tenure that never
				// ended. Refusing it here is only safe because the defect is now reported by
				// UTerritoryDefinition::IsDataValid, so the authoring fix is a failed
				// validation gate rather than a reconciliation deferred forever.
				++InconsistentChildren;
				continue;
			}
			Seen.Add(Child->TerritoryTag);
			const FReplicatedCaptureSummary Row = GetCaptureSummary(Child->TerritoryTag);
			if (Row.TerritoryTag != Child->TerritoryTag
				|| Row.TerritoryGUID != Child->StableTerritoryGUID
				|| Row.ParentTerritoryTag != Parent->TerritoryTag
				|| Row.HierarchyLevel != GetDefinitionHierarchyLevel(Child))
			{
				++UnresolvedChildren;
				continue;
			}
			// The child has an exact row, but its own reduction may still be incomplete: a
			// resolved District whose Places are unresolved holds a last-known owner. Reading
			// that as verified control is exactly the upward propagation this contract forbids,
			// and without this check the parent would commit from a value the child refused to
			// stand behind.
			if (!IsHierarchyReductionComplete(Child->TerritoryTag))
			{
				++UnresolvedChildren;
				continue;
			}
			View.Owner = Row.CurrentOwner;
			View.State = Row.State;
			View.Availability = Row.Availability;
		}

		if (UnresolvedChildren + InconsistentChildren > 0)
		{
			// Incomplete. Owner, state, progress and history all keep their last verified
			// values and nothing is announced: this reduction is a default view standing in for
			// an unknown, not a result. The history append below is the one durable side effect
			// here, which is why it sits inside the complete branch rather than above this gate
			// - recording a tenure from an unknown is what made the mistake permanent.
			return;
		}

		const auto Derived = TerritoryHierarchyPolicy::ReduceControl(Views);
		if (Summary->CurrentOwner.IsValid() && Summary->CurrentOwner != Derived.SecuredOwner)
			Summary->FormerOwningFactions.AddTag(Summary->CurrentOwner);
		Summary->CurrentOwner = Derived.SecuredOwner;
		Summary->State = Derived.State;
		Summary->ContestingFaction = FGameplayTag();
		Summary->ControlProgress = Derived.State == ETerritoryState::Claimed ? 1.f : 0.f;
	};
	for (const auto& Pair : RegisteredHierarchyDefinitions) Reduce(Pair.Value);
}

bool ATerritoryWorldState::IsDirectoryIdentityRetired(const FGuid& Identity) const
{
	return Identity.IsValid() && RetiredDirectoryGUIDs.Contains(Identity);
}

void ATerritoryWorldState::ApplyDirectoryRetirements(bool bReconcileAssaults)
{
	if (!HasAuthority() || RetiredDirectoryGUIDs.IsEmpty()) return;
	if (bReconcileAssaults && GetWorld())
	{
		if (auto* Counter = GetWorld()->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			Counter->ReconcileRetiredTargets();
			ReplicatedAssaults = Counter->GetPersistentState();
			ForceNetUpdate();
		}
	}
	const int32 Removed = ReplicatedCaptureSummaries.RemoveAll(
		[this](const FReplicatedCaptureSummary& Row) { return IsDirectoryIdentityRetired(Row.TerritoryGUID); });
	for (auto It = RegisteredHierarchyDefinitions.CreateIterator(); It; ++It)
	{
		if (It.Value() && IsDirectoryIdentityRetired(It.Value()->StableTerritoryGUID))
		{
			RegisteredHierarchyRevisions.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	if (Removed > 0)
	{
		ReconcileUnloadedHierarchy();
		ForceNetUpdate();
	}
}

#if WITH_EDITOR
EDataValidationResult ATerritoryWorldState::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bConflict = RetiredDirectoryGUIDs.Contains(FGuid());
	if (bConflict) Context.AddError(NSLOCTEXT("TerritoryDirectory", "EmptyRetirementGuid",
		"Retired Directory GUIDs contains an empty identity."));
	TSet<const UTerritoryDefinition*> Visited;
	TFunction<void(const UTerritoryDefinition*)> Check;
	Check = [&](const UTerritoryDefinition* Definition)
	{
		if (!Definition || Visited.Contains(Definition)) return;
		Visited.Add(Definition);
		if (IsDirectoryIdentityRetired(Definition->StableTerritoryGUID))
		{
			bConflict = true;
			Context.AddError(FText::Format(NSLOCTEXT("TerritoryDirectory", "RetiredAuthoredDefinition",
				"Campaign Definition {0} still references a retired directory GUID."),
				FText::FromString(Definition->GetPathName())));
		}
		if (const auto* City = Cast<UTerritoryCityDefinition>(Definition))
			for (const UTerritoryDistrictDefinition* District : City->Districts) Check(District);
		else if (const auto* District = Cast<UTerritoryDistrictDefinition>(Definition))
			for (const UTerritoryPlaceDefinition* Place : District->Places) Check(Place);
	};
	for (const UTerritoryCityDefinition* City : CampaignCities) Check(City);
	if (GetWorld())
	{
		for (TActorIterator<ATerritoryVolume> It(GetWorld()); It; ++It)
		{
			if (!IsDirectoryIdentityRetired(It->GetTerritoryGUID())) continue;
			bConflict = true;
			Context.AddError(FText::Format(NSLOCTEXT("TerritoryDirectory", "RetiredPlacedActor",
				"Placed Territory {0} still uses a retired directory GUID."),
				FText::FromString(It->GetPathName())));
		}
	}
	return bConflict ? EDataValidationResult::Invalid
		: SuperResult == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : SuperResult;
}
#endif

void ATerritoryWorldState::RefreshStrategicDirectory()
{
	if (!HasAuthority()) return;
	ApplyDirectoryRetirements();
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
	// CountClaimedDistrictsForFaction is the row-shape rule; this is the gameplay gate above it.
	// A row that is Claimed with an owner is not yet verified control: an incomplete reduction
	// keeps the last verified owner and state rather than committing a default view, so a
	// District whose own Places could not be resolved would otherwise grant staging eligibility
	// to a faction that may no longer hold it.
	TArray<FReplicatedCaptureSummary> Verified;
	Verified.Reserve(ReplicatedCaptureSummaries.Num());
	for (const FReplicatedCaptureSummary& Summary : ReplicatedCaptureSummaries)
	{
		if (IsHierarchyReductionComplete(Summary.TerritoryTag)) Verified.Add(Summary);
	}
	return CountClaimedDistrictsForFaction(Verified, Faction);
}

bool ATerritoryWorldState::IsHierarchyReductionComplete(const FGameplayTag& TerritoryTag) const
{
	TSet<FGameplayTag> Visited;
	return IsHierarchyReductionComplete(TerritoryTag, Visited);
}

bool ATerritoryWorldState::IsHierarchyReductionComplete(const FGameplayTag& TerritoryTag,
	TSet<FGameplayTag>& Visited) const
{
	if (!TerritoryTag.IsValid()) return true;

	// A row set that reaches the same tag twice is malformed. Failing closed here keeps a bad
	// save from recursing until the stack runs out.
	if (Visited.Contains(TerritoryTag)) return false;
	Visited.Add(TerritoryTag);

	const FReplicatedCaptureSummary* Row = ReplicatedCaptureSummaries.FindByPredicate(
		[&TerritoryTag](const FReplicatedCaptureSummary& Entry)
		{
			return Entry.TerritoryTag == TerritoryTag;
		});

	// No durable row means nothing is being retained under this tag: a caller that needed one
	// has already failed to resolve it, so there is no unknown value here to report.
	if (!Row) return true;

	// A Place is a leaf, so there is no authored child that could be missing.
	if (Row->HierarchyLevel == ETerritoryHierarchyLevel::Place) return true;

	const ETerritoryHierarchyLevel ChildLevel =
		Row->HierarchyLevel == ETerritoryHierarchyLevel::City
			? ETerritoryHierarchyLevel::District : ETerritoryHierarchyLevel::Place;

	int32 ResolvedChildren = 0;
	for (const FReplicatedCaptureSummary& Child : ReplicatedCaptureSummaries)
	{
		if (Child.ParentTerritoryTag != TerritoryTag || Child.HierarchyLevel != ChildLevel) continue;
		++ResolvedChildren;
		if (!IsHierarchyReductionComplete(Child.TerritoryTag, Visited)) return false;
	}

	// TotalChildren is the authored direct-child count, so fewer rows than that means at least
	// one authored child has no row at all. This is a lower bound rather than an equality: an
	// extra row is never treated as missing data. The reduction's own identity requirement -
	// exact tag, GUID, parent and level - is enforced per child by the two reducers, so a row
	// that satisfies this count but not that test is refused there rather than here.
	return ResolvedChildren >= Row->TotalChildren;
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
	ApplyDirectoryRetirements();

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

		PublishDiplomacyReadModel();

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
	SavedReputationSubjectFaction = ReplicatedReputationSubjectFaction;
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
	ReplicatedReputationSubjectFaction = SavedReputationSubjectFaction;
	ReplicatedDiplomacyHistory = SavedDiplomacyHistory;
	ReplicatedAssaults = SavedAssaults;
	ReplicatedCaptureSummaries = SavedStrategicDirectory;
	// Scheduler restoration migrates the incoming records, never the old campaign.
	ApplyDirectoryRetirements(false);
	ReconcileUnloadedHierarchy();
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

void ATerritoryWorldState::PublishDiplomacyReadModel()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (!World) return;
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!Diplomacy) return;

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
	ReplicatedReputationSubjectFaction = Diplomacy->GetReputationSubjectFaction();

	// The live handlers push each change as it happens; this seeds the standing state a
	// session opens with, which no live delegate ever fires for.
	ForceNetUpdate();
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
			Record.bReputationDerived = Treaty.bReputationDerived;
			Record.bNarrativeObserved = Treaty.bNarrativeObserved;
			Treaties.Add(Record);
		}

		TMap<FGameplayTag, int32> Reputation;
		for (const FReplicatedFactionReputation& Entry : ReplicatedReputation)
		{
			Reputation.Add(Entry.Faction, Entry.Reputation);
		}
		Diplomacy->RestorePersistentState(Treaties, Reputation, ReplicatedDiplomacyHistory,
			ReplicatedReputationSubjectFaction);
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
	FWorldDelegates::PreLevelRemovedFromWorld.RemoveAll(this);
	FWorldDelegates::PreLevelRemovedFromWorld.AddUObject(this, &ATerritoryWorldState::SaveStreamingLevel);

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
		Diplomacy->OnReputationSubjectChanged.AddDynamic(this, &ATerritoryWorldState::OnReputationSubjectChangedLive);
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
	FWorldDelegates::PreLevelRemovedFromWorld.RemoveAll(this);
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
		Diplomacy->OnReputationSubjectChanged.RemoveDynamic(this, &ATerritoryWorldState::OnReputationSubjectChangedLive);
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

void ATerritoryWorldState::SaveStreamingLevel(ULevel* Level, UWorld* World)
{
	if (!HasAuthority() || !World || World != GetWorld() || !Level
		|| Level->GetWorld() != World || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return;
	}
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!Save || Save->IsLoading()) return;

	TSet<AActor*> ActorsToSave;
	for (AActor* Actor : Level->Actors)
	{
		if (!IsValid(Actor) || !Actor->HasAuthority() || !Actor->HasActorBegunPlay()) continue;
		if (ATerritoryVolume* Territory = Cast<ATerritoryVolume>(Actor))
		{
			ActorsToSave.Add(Territory);
			// A Place can unload before its posts. Its EndPlay retires guards,
			// so record those posts while their active slots still exist.
			for (ATerritoryGuardSpawnPoint* Post : Territory->GetGuardSpawnPoints())
			{
				if (IsValid(Post) && Post->HasAuthority()) ActorsToSave.Add(Post);
			}
		}
		else if (Actor->IsA<ATerritoryGuardSpawnPoint>()) ActorsToSave.Add(Actor);
	}
	if (ActorsToSave.IsEmpty()) return;
	if (!Save->GetSaveObject())
	{
		UE_LOG(LogTerritory, Error, TEXT("Cannot preserve streaming Territory actors: Narrative save system is not initialized."));
		return;
	}
	for (AActor* Actor : ActorsToSave)
	{
		const FGuid ID = INarrativeStableActor::Execute_GetActorGUID(Actor);
		if (!ID.IsValid() || !Save->SaveSingleActor(Actor) || !Save->DoesRecordExist(ID))
		{
			UE_LOG(LogTerritory, Error, TEXT("Failed to preserve streaming Territory actor %s in Narrative save records."), *Actor->GetPathName());
		}
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
			const bool bTrimA = A.IsTerminal() && !A.IsRetainedStoryOutcome();
			const bool bTrimB = B.IsTerminal() && !B.IsRetainedStoryOutcome();
			if (bTrimA != bTrimB) return bTrimA;
			return A.CapturedGameTime < B.CapturedGameTime;
		});
		while (ReplicatedAssaults.Num() > MaximumRecords
			&& ReplicatedAssaults[0].IsTerminal() && !ReplicatedAssaults[0].IsRetainedStoryOutcome())
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
	const UTerritoryEconomySubsystem* Economy = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	// An earlier listener may load a snapshot that removed or already included
	// this transaction. Project only the current authority, once per durable ID.
	if (!Economy || !Economy->GetAllTransactionHistory().ContainsByPredicate(
		[&](const FTerritoryTransaction& Tx) { return Tx.TransactionID == Transaction.TransactionID; })
		|| ReplicatedTransactions.ContainsByPredicate(
		[&](const FReplicatedTransaction& Tx) { return Tx.TransactionID == Transaction.TransactionID; })) return;

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

void ATerritoryWorldState::OnReputationSubjectChangedLive(FGameplayTag SubjectFaction)
{
	if (!HasAuthority()) return;
	// Read the current authority in case an earlier listener changed the subject again.
	const UTerritoryDiplomacySubsystem* Diplomacy = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	if (!Diplomacy) return;
	ReplicatedReputationSubjectFaction = Diplomacy->GetReputationSubjectFaction();
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
