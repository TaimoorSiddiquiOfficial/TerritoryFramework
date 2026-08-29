#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

namespace
{
	bool CanMutateDiplomacy(const UWorld* World)
	{
		// A World Subsystem is authoritative in every non-client world. Requiring an
		// AuthGameMode made Definition state events silently fail during valid server
		// startup, streaming, and automation contexts before a GameMode was available.
		return World && World->GetNetMode() != NM_Client;
	}
}

void UTerritoryDiplomacySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		if (ANarrativeGameState* GS = GetNarrativeGameState())
		{
			GS->OnFactionAttitudeChanged.AddDynamic(this, &UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged);
		}
		if (UNarrativeSaveSubsystem* SaveSubsystem = World->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			SaveSubsystem->OnFinishedLoad.AddUniqueDynamic(this, &UTerritoryDiplomacySubsystem::OnNarrativeLoadFinished);
		}
	}

	// Treaty expiration timer — server only
	if (World && World->GetNetMode() != NM_Client)
	{
		const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
		const float TreatyInterval = Settings ? Settings->TreatyExpirationCheckInterval : 10.f;

		World->GetTimerManager().SetTimer(
			TreatyExpirationTimerHandle,
			this,
			&UTerritoryDiplomacySubsystem::OnTreatyExpirationTick,
			TreatyInterval,
			true);
	}

	UE_LOG(LogTerritory, Log, TEXT("TerritoryDiplomacySubsystem initialized"));
}

void UTerritoryDiplomacySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TreatyExpirationTimerHandle);
		if (UNarrativeSaveSubsystem* SaveSubsystem = World->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			SaveSubsystem->OnFinishedLoad.RemoveDynamic(this, &UTerritoryDiplomacySubsystem::OnNarrativeLoadFinished);
		}
	}

	if (ANarrativeGameState* GS = GetNarrativeGameState())
	{
		GS->OnFactionAttitudeChanged.RemoveDynamic(this, &UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged);
	}

	ActiveTreaties.Empty();
	FactionReputation.Empty();
	DiplomacyHistory.Empty();
	Super::Deinitialize();
}

void UTerritoryDiplomacySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() != NM_Client)
	{
		if (ANarrativeGameState* GS = GetNarrativeGameState())
		{
			GS->OnFactionAttitudeChanged.AddUniqueDynamic(this, &UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged);
			LoadFromGameState();
		}
		if (UNarrativeSaveSubsystem* SaveSubsystem = InWorld.GetSubsystem<UNarrativeSaveSubsystem>())
		{
			SaveSubsystem->OnFinishedLoad.AddUniqueDynamic(this, &UTerritoryDiplomacySubsystem::OnNarrativeLoadFinished);
		}
		InWorld.GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UTerritoryDiplomacySubsystem::FinalizeGameStateSync));
	}
}

void UTerritoryDiplomacySubsystem::FinalizeGameStateSync()
{
	// Actor-level save loads run during BeginPlay. Re-apply the rich treaty state
	// one tick later so Narrative GameState load order cannot overwrite it.
	SyncToGameState();
}

void UTerritoryDiplomacySubsystem::OnNarrativeLoadFinished()
{
	FinalizeGameStateSync();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Diplomacy Actions — each sets treaty metadata AND syncs Narrative attitude
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryDiplomacySubsystem::DeclareWar(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	const EDiplomacyState OldState = GetDiplomacyState(FactionA, FactionB);
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::War);
	if (OldState != EDiplomacyState::War) RecordEvent(EDiplomacyEventType::DeclaredWar, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::DeclarePeace(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	const EDiplomacyState OldState = GetDiplomacyState(FactionA, FactionB);
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::Ceasefire);
	if (OldState != EDiplomacyState::Ceasefire) RecordEvent(EDiplomacyEventType::DeclaredPeace, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::BreakCeasefire(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	if (GetDiplomacyState(FactionA, FactionB) != EDiplomacyState::Ceasefire) return;

	// Breaking a ceasefire is a hostile act — escalate to War so the action has
	// gameplay consequences (capture remains valid, faction attitudes reflect hostility).
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::War);
	RecordEvent(EDiplomacyEventType::BrokeCeasefire, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::FormAlliance(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	const EDiplomacyState OldState = GetDiplomacyState(FactionA, FactionB);
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::Alliance);
	if (OldState != EDiplomacyState::Alliance) RecordEvent(EDiplomacyEventType::FormedAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SignNonAggression(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	const EDiplomacyState OldState = GetDiplomacyState(FactionA, FactionB);
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::NonAggression);
	if (OldState != EDiplomacyState::NonAggression) RecordEvent(EDiplomacyEventType::SignedNonAggression, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::BreakAlliance(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	if (GetDiplomacyState(FactionA, FactionB) != EDiplomacyState::Alliance) return;

	// Remove treaty metadata, then reset Narrative attitude to Neutral
	RemoveTreaty(FactionA, FactionB);
	SyncNarrativeAttitudeForTreaty(FactionA, FactionB);
	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, EDiplomacyState::None);
	RecordEvent(EDiplomacyEventType::BrokeAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SignTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB, float DurationGameTime)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;
	// FIX: Don't call SetDiplomacyState after adding the treaty — it would
	// see the treaty already exists and early-return without syncing Narrative.
	RemoveTreaty(FactionA, FactionB);

	FTreatyRecord Treaty;
	Treaty.FactionA = FactionA;
	Treaty.FactionB = FactionB;
	Treaty.State = EDiplomacyState::TradeAgreement;
	Treaty.bPermanent = (DurationGameTime <= 0.f);

	if (ANarrativeGameState* GS = GetNarrativeGameState())
	{
		Treaty.SignedGameTime = GS->GetAccumulatedTime();
		if (!Treaty.bPermanent)
		{
			Treaty.ExpiryGameTime = Treaty.SignedGameTime + DurationGameTime;
		}
	}

	ActiveTreaties.Add(Treaty);

	// Explicitly sync Narrative attitude for this treaty
	SyncNarrativeAttitudeForTreaty(FactionA, FactionB);
	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, EDiplomacyState::TradeAgreement);
	RecordEvent(EDiplomacyEventType::SignedTradeAgreement, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SetDiplomacyState(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugDiplomacy();

	FTreatyRecord* Existing = FindTreaty(FactionA, FactionB);
	EDiplomacyState OldState = Existing ? Existing->State : EDiplomacyState::None;

	if (OldState == NewState)
	{
		// A matching rich Territory state does not prove Narrative's directional
		// attitude map is still correct. A level, save migration, or external
		// Narrative listener may have left it stale. Reconcile even the apparent
		// no-op -- especially None, which intentionally has no treaty row.
		SyncNarrativeAttitudeForTreaty(FactionA, FactionB);
		return;
	}

	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Diplomacy] %s ↔ %s: %d → %d"),
			*FactionA.ToString(), *FactionB.ToString(),
			static_cast<int32>(OldState), static_cast<int32>(NewState));
	}

	if (NewState == EDiplomacyState::None)
	{
		// FIX: Remove treaty metadata, then reset Narrative attitude to Neutral
		RemoveTreaty(FactionA, FactionB);
		SyncNarrativeAttitudeForTreaty(FactionA, FactionB);
	}
	else if (Existing)
	{
		Existing->State = NewState;
		// P1-N09: Preserve existing timing data. Only reset to permanent when the
		// treaty transitions from None (newly created) or when the new state is
		// more restrictive (War/None), which should clear any pending expiry.
		const EDiplomacyState PrevState = OldState;
		if (PrevState == EDiplomacyState::None || NewState == EDiplomacyState::War)
		{
			Existing->bPermanent = true;
			Existing->ExpiryGameTime = -1.f;
		}
		SyncNarrativeAttitudeForTreaty(FactionA, FactionB);
	}
	else
	{
		FTreatyRecord NewTreaty;
		NewTreaty.FactionA = FactionA;
		NewTreaty.FactionB = FactionB;
		NewTreaty.State = NewState;
		NewTreaty.bPermanent = true;

		if (ANarrativeGameState* GS = GetNarrativeGameState())
		{
			NewTreaty.SignedGameTime = GS->GetAccumulatedTime();
		}

		ActiveTreaties.Add(NewTreaty);
		// Sync Narrative attitude based on new treaty state
		SyncNarrativeAttitudeForTreaty(FactionA, FactionB);
	}

	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, NewState);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Query API — reads from treaty metadata, NOT Narrative GameState
// ═══════════════════════════════════════════════════════════════════════════════

EDiplomacyState UTerritoryDiplomacySubsystem::GetDiplomacyState(FGameplayTag FactionA, FGameplayTag FactionB) const
{
	const FTreatyRecord* Treaty = FindTreaty(FactionA, FactionB);
	return Treaty ? Treaty->State : EDiplomacyState::None;
}

bool UTerritoryDiplomacySubsystem::AreAnyFactionsAtWar(
	const FGameplayTagContainer& FactionsA,
	const FGameplayTagContainer& FactionsB) const
{
	for (const FGameplayTag& FactionA : FactionsA)
	{
		for (const FGameplayTag& FactionB : FactionsB)
		{
			if (FactionA.IsValid() && FactionB.IsValid() && FactionA != FactionB
				&& IsAtWar(FactionA, FactionB))
			{
				return true;
			}
		}
	}
	return false;
}

bool UTerritoryDiplomacySubsystem::IsAtWar(FGameplayTag FactionA, FGameplayTag FactionB) const
{
	return GetDiplomacyState(FactionA, FactionB) == EDiplomacyState::War;
}

bool UTerritoryDiplomacySubsystem::IsAllied(FGameplayTag FactionA, FGameplayTag FactionB) const
{
	return GetDiplomacyState(FactionA, FactionB) == EDiplomacyState::Alliance;
}

bool UTerritoryDiplomacySubsystem::HasTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB) const
{
	return GetDiplomacyState(FactionA, FactionB) == EDiplomacyState::TradeAgreement;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Reputation API
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryDiplomacySubsystem::AddReputation(FGameplayTag Faction, int32 Amount)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	if (!Faction.IsValid()) return;
	int32& Rep = FactionReputation.FindOrAdd(Faction);
	Rep += Amount;
	OnReputationChanged.Broadcast(Faction, Rep);
}

void UTerritoryDiplomacySubsystem::SetReputation(FGameplayTag Faction, int32 Value)
{
	if (!CanMutateDiplomacy(GetWorld())) return;
	if (!Faction.IsValid()) return;
	FactionReputation.FindOrAdd(Faction) = Value;
	OnReputationChanged.Broadcast(Faction, Value);
}

int32 UTerritoryDiplomacySubsystem::GetReputation(FGameplayTag Faction) const
{
	const int32* Rep = FactionReputation.Find(Faction);
	return Rep ? *Rep : 0;
}

TMap<FGameplayTag, int32> UTerritoryDiplomacySubsystem::GetAllReputation() const
{
	return FactionReputation;
}

TArray<FTreatyRecord> UTerritoryDiplomacySubsystem::GetAllTreaties() const
{
	return ActiveTreaties;
}

TArray<FTreatyRecord> UTerritoryDiplomacySubsystem::GetTreatiesForFaction(FGameplayTag Faction) const
{
	TArray<FTreatyRecord> Result;
	for (const FTreatyRecord& Treaty : ActiveTreaties)
	{
		if (Treaty.FactionA == Faction || Treaty.FactionB == Faction)
		{
			Result.Add(Treaty);
		}
	}
	return Result;
}

TArray<FDiplomacyEvent> UTerritoryDiplomacySubsystem::GetDiplomacyHistory() const
{
	return DiplomacyHistory;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Narrative GameState Bridge — Narrative is sole authority for AI attitudes
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryDiplomacySubsystem::SyncToGameState()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;

	// Delegate listeners may mutate diplomacy, so iterate a stable snapshot.
	const TArray<FTreatyRecord> Treaties = ActiveTreaties;
	for (const FTreatyRecord& Treaty : Treaties)
	{
		SyncNarrativeAttitudeForTreaty(Treaty.FactionA, Treaty.FactionB);
	}
}

void UTerritoryDiplomacySubsystem::RestorePersistentState(
	const TArray<FTreatyRecord>& Treaties,
	const TMap<FGameplayTag, int32>& Reputation,
	const TArray<FDiplomacyEvent>& History)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const TArray<FTreatyRecord> PreviousTreaties = ActiveTreaties;
	ActiveTreaties.Reset();
	for (const FTreatyRecord& Treaty : Treaties)
	{
		if (Treaty.IsValid() && Treaty.FactionA != Treaty.FactionB)
		{
			ActiveTreaties.Add(Treaty);
		}
	}

	// Relations removed by the save must not remain stale in Narrative's map.
	for (const FTreatyRecord& PreviousTreaty : PreviousTreaties)
	{
		if (!FindTreaty(PreviousTreaty.FactionA, PreviousTreaty.FactionB))
		{
			SyncNarrativeAttitudeForTreaty(PreviousTreaty.FactionA, PreviousTreaty.FactionB);
		}
	}
	FactionReputation = Reputation;
	DiplomacyHistory = History;
	if (DiplomacyHistory.Num() > 500)
	{
		DiplomacyHistory.RemoveAt(0, DiplomacyHistory.Num() - 500);
	}

	SyncToGameState();
}

void UTerritoryDiplomacySubsystem::LoadFromGameState()
{
	// Rebuild treaty metadata from Narrative GameState attitudes.
	// Preserve metadata (timing, permanence, expiry) for treaties that already exist
	// and whose attitude hasn't changed. Only create new treaties for attitudes that
	// have no corresponding treaty record.
	ANarrativeGameState* GS = GetNarrativeGameState();
	UWorld* World = GetWorld();
	if (!GS || !World || World->GetNetMode() == NM_Client) return;

	// Track which faction pairs we've seen from GameState attitudes
	TSet<FString> SeenPairs;

	for (const FTerritoryNarrativeAttitudeSnapshot& Snapshot
		: FTerritoryNarrativeProAdapter::ReadFactionAttitudes(GS))
	{
		const FGameplayTag& FactionA = Snapshot.FactionA;
		const FGameplayTag& FactionB = Snapshot.FactionB;
		const FString CanonicalKey = FTerritoryNarrativeProAdapter::MakeCanonicalFactionPairKey(FactionA, FactionB);
		SeenPairs.Add(CanonicalKey);
		const ETeamAttitude::Type Attitude = Snapshot.Attitude;

		const EDiplomacyState State = AttitudeToDiplomacyState(Attitude);
		FTreatyRecord* Existing = FindTreaty(FactionA, FactionB);
		if (Existing && DiplomacyStateToAttitude(Existing->State) == Attitude)
		{
			// Preserve compatible rich metadata (trade, non-aggression, ceasefire).
			continue;
		}

		if (State == EDiplomacyState::None)
		{
			RemoveTreaty(FactionA, FactionB);
			continue;
		}

		if (Existing)
		{
			Existing->State = State;
			Existing->bPermanent = true;
			Existing->ExpiryGameTime = -1.f;
		}
		else
		{
			// New treaty from attitude
			FTreatyRecord Treaty;
			Treaty.FactionA = FactionA;
			Treaty.FactionB = FactionB;
			Treaty.State = State;
			Treaty.bPermanent = true;
			Treaty.SignedGameTime = GS->GetAccumulatedTime();
			ActiveTreaties.Add(Treaty);
		}
	}

	// Remove treaties for faction pairs that no longer have any attitude entry
	for (int32 i = ActiveTreaties.Num() - 1; i >= 0; --i)
	{
		FTreatyRecord& Treaty = ActiveTreaties[i];
		const FString Key = FTerritoryNarrativeProAdapter::MakeCanonicalFactionPairKey(
			Treaty.FactionA, Treaty.FactionB);
		if (!SeenPairs.Contains(Key))
		{
			ActiveTreaties.RemoveAt(i);
		}
	}
}

void UTerritoryDiplomacySubsystem::SetNarrativeAttitude(FGameplayTag FactionA, FGameplayTag FactionB, ETeamAttitude::Type Attitude)
{
	ANarrativeGameState* GS = GetNarrativeGameState();
	UWorld* World = GetWorld();
	if (!GS || !World || World->GetNetMode() == NM_Client) return;

	// Ignore the delegate echo from our own bridge write. Without this guard,
	// setting a Ceasefire to Neutral immediately removes the treaty metadata.
	const bool bWasSuppressingSync = bSuppressSync;
	bSuppressSync = true;
	FTerritoryNarrativeProAdapter::SetSymmetricFactionAttitude(GS, FactionA, FactionB, Attitude);
	bSuppressSync = bWasSuppressingSync;
}

void UTerritoryDiplomacySubsystem::SyncNarrativeAttitudeForTreaty(FGameplayTag FactionA, FGameplayTag FactionB)
{
	// Re-read treaty metadata after each write in case external Narrative listeners
	// made a reentrant diplomacy change. Idempotent listeners converge immediately.
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		const FTreatyRecord* Treaty = FindTreaty(FactionA, FactionB);
		const ETeamAttitude::Type Attitude = Treaty
			? DiplomacyStateToAttitude(Treaty->State)
			: ETeamAttitude::Neutral;
		SetNarrativeAttitude(FactionA, FactionB, Attitude);

		const FTreatyRecord* CurrentTreaty = FindTreaty(FactionA, FactionB);
		const ETeamAttitude::Type CurrentAttitude = CurrentTreaty
			? DiplomacyStateToAttitude(CurrentTreaty->State)
			: ETeamAttitude::Neutral;
		if (CurrentAttitude == Attitude) return;
	}
}

void UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged(FGameplayTag Faction, FGameplayTag OtherFaction, ETeamAttitude::Type NewAttitude)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	// Reentrancy guard — prevent recursive mutation from delegate listeners
	if (bSuppressSync) return;
	bSuppressSync = true;
	struct FSyncGuard { bool& Flag; ~FSyncGuard() { Flag = false; } } Guard{bSuppressSync};

	FTreatyRecord* Existing = FindTreaty(Faction, OtherFaction);
	const EDiplomacyState OldState = Existing ? Existing->State : EDiplomacyState::None;
	if (Existing && NewAttitude != ETeamAttitude::Neutral
		&& DiplomacyStateToAttitude(Existing->State) == NewAttitude)
	{
		// Preserve compatible rich metadata and repair a missing reverse entry.
		SyncNarrativeAttitudeForTreaty(Faction, OtherFaction);
		return;
	}

	const EDiplomacyState NewState = AttitudeToDiplomacyState(NewAttitude);
	if (NewState == EDiplomacyState::None)
	{
		RemoveTreaty(Faction, OtherFaction);
	}
	else if (Existing)
	{
		Existing->State = NewState;
		Existing->bPermanent = true;
		Existing->ExpiryGameTime = -1.f;
	}
	else
	{
		FTreatyRecord Treaty;
		Treaty.FactionA = Faction;
		Treaty.FactionB = OtherFaction;
		Treaty.State = NewState;
		// Narrative attitudes do not carry an expiry, so the only lossless import is
		// a permanent Territory treaty until another explicit change arrives.
		Treaty.bPermanent = true;
		Treaty.ExpiryGameTime = -1.f;
		if (ANarrativeGameState* GS = GetNarrativeGameState())
		{
			Treaty.SignedGameTime = GS->GetAccumulatedTime();
		}
		ActiveTreaties.Add(Treaty);
	}

	if (OldState != NewState)
	{
		OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, NewState);
	}
	SyncNarrativeAttitudeForTreaty(Faction, OtherFaction);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════════

FTreatyRecord* UTerritoryDiplomacySubsystem::FindTreaty(FGameplayTag FactionA, FGameplayTag FactionB)
{
	for (FTreatyRecord& Treaty : ActiveTreaties)
	{
		if ((Treaty.FactionA == FactionA && Treaty.FactionB == FactionB) ||
			(Treaty.FactionA == FactionB && Treaty.FactionB == FactionA))
		{
			return &Treaty;
		}
	}
	return nullptr;
}

const FTreatyRecord* UTerritoryDiplomacySubsystem::FindTreaty(FGameplayTag FactionA, FGameplayTag FactionB) const
{
	for (const FTreatyRecord& Treaty : ActiveTreaties)
	{
		if ((Treaty.FactionA == FactionA && Treaty.FactionB == FactionB) ||
			(Treaty.FactionA == FactionB && Treaty.FactionB == FactionA))
		{
			return &Treaty;
		}
	}
	return nullptr;
}

void UTerritoryDiplomacySubsystem::RemoveTreaty(FGameplayTag FactionA, FGameplayTag FactionB)
{
	ActiveTreaties.RemoveAll([&](const FTreatyRecord& Treaty)
	{
		return (Treaty.FactionA == FactionA && Treaty.FactionB == FactionB) ||
			   (Treaty.FactionA == FactionB && Treaty.FactionB == FactionA);
	});
}

void UTerritoryDiplomacySubsystem::RecordEvent(EDiplomacyEventType EventType, FGameplayTag FactionA, FGameplayTag FactionB)
{
	FDiplomacyEvent Event;
	Event.EventType = EventType;
	Event.FactionA = FactionA;
	Event.FactionB = FactionB;

	if (ANarrativeGameState* GS = GetNarrativeGameState())
	{
		Event.GameTime = GS->GetAccumulatedTime();
	}

	DiplomacyHistory.Add(Event);

	// P2-N13: Batch removal instead of O(N²) loop
	constexpr int32 MaxDiplomacyHistory = 500;
	const int32 Excess = DiplomacyHistory.Num() - MaxDiplomacyHistory;
	if (Excess > 0)
	{
		DiplomacyHistory.RemoveAt(0, Excess);
	}

	OnDiplomacyEvent.Broadcast(Event);
}

ANarrativeGameState* UTerritoryDiplomacySubsystem::GetNarrativeGameState() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	return Cast<ANarrativeGameState>(World->GetGameState());
}

ETeamAttitude::Type UTerritoryDiplomacySubsystem::DiplomacyStateToAttitude(EDiplomacyState State) const
{
	switch (State)
	{
	case EDiplomacyState::Alliance:
	case EDiplomacyState::TradeAgreement:
	case EDiplomacyState::NonAggression:
		return ETeamAttitude::Friendly;
	case EDiplomacyState::War:
		return ETeamAttitude::Hostile;
	case EDiplomacyState::Ceasefire:
	case EDiplomacyState::None:
	default:
		return ETeamAttitude::Neutral;
	}
}

EDiplomacyState UTerritoryDiplomacySubsystem::AttitudeToDiplomacyState(ETeamAttitude::Type Attitude) const
{
	switch (Attitude)
	{
	case ETeamAttitude::Friendly: return EDiplomacyState::Alliance;
	case ETeamAttitude::Hostile: return EDiplomacyState::War;
	default: return EDiplomacyState::None;
	}
}

void UTerritoryDiplomacySubsystem::CheckTreatyExpirations()
{
	ANarrativeGameState* GS = GetNarrativeGameState();
	if (!GS) return;

	float CurrentTime = GS->GetAccumulatedTime();
	TArray<FTreatyRecord> ExpiredTreaties;

	for (int32 i = 0; i < ActiveTreaties.Num(); ++i)
	{
		if (ActiveTreaties[i].IsExpired(CurrentTime))
		{
			ExpiredTreaties.Add(ActiveTreaties[i]);
		}
	}

	for (const FTreatyRecord& ExpiredTreaty : ExpiredTreaties)
	{
		const FTreatyRecord* CurrentTreaty = FindTreaty(ExpiredTreaty.FactionA, ExpiredTreaty.FactionB);
		if (!CurrentTreaty || !CurrentTreaty->IsExpired(CurrentTime)) continue;

		SetDiplomacyState(ExpiredTreaty.FactionA, ExpiredTreaty.FactionB, EDiplomacyState::None);
		RecordEvent(EDiplomacyEventType::ExpiredTreaty, ExpiredTreaty.FactionA, ExpiredTreaty.FactionB);

		// Broadcast dedicated delegate so Blueprint can react without
		// filtering the generic OnDiplomacyEvent by event type.
		OnTreatyExpired.Broadcast(ExpiredTreaty.FactionA, ExpiredTreaty.FactionB, EDiplomacyState::None);
	}
}

void UTerritoryDiplomacySubsystem::OnTreatyExpirationTick()
{
	CheckTreatyExpirations();
}
