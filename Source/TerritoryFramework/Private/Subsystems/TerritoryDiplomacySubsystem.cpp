#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

void UTerritoryDiplomacySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (ANarrativeGameState* GS = GetNarrativeGameState())
	{
		GS->OnFactionAttitudeChanged.AddDynamic(this, &UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged);
		LoadFromGameState();
	}

	// Treaty expiration timer — server only
	UWorld* World = GetWorld();
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

// ═══════════════════════════════════════════════════════════════════════════════
// Diplomacy Actions — each sets treaty metadata AND syncs Narrative attitude
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryDiplomacySubsystem::DeclareWar(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!GetWorld()->GetAuthGameMode()) return;
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::War);
	RecordEvent(EDiplomacyEventType::DeclaredWar, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::DeclarePeace(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!GetWorld()->GetAuthGameMode()) return;
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::Ceasefire);
	RecordEvent(EDiplomacyEventType::DeclaredPeace, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::BreakCeasefire(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!GetWorld()->GetAuthGameMode()) return;
	if (GetDiplomacyState(FactionA, FactionB) != EDiplomacyState::Ceasefire) return;

	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::None);
	RecordEvent(EDiplomacyEventType::BrokeCeasefire, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::FormAlliance(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!GetWorld()->GetAuthGameMode()) return;
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::Alliance);
	RecordEvent(EDiplomacyEventType::FormedAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SignNonAggression(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!GetWorld()->GetAuthGameMode()) return;
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::NonAggression);
	RecordEvent(EDiplomacyEventType::SignedNonAggression, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::BreakAlliance(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (!GetWorld()->GetAuthGameMode()) return;
	if (GetDiplomacyState(FactionA, FactionB) != EDiplomacyState::Alliance) return;

	// Remove treaty metadata, then reset Narrative attitude to Neutral
	RemoveTreaty(FactionA, FactionB);
	SetNarrativeAttitude(FactionA, FactionB, ETeamAttitude::Neutral);
	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, EDiplomacyState::None);
	RecordEvent(EDiplomacyEventType::BrokeAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SignTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB, float DurationGameTime)
{
	if (!GetWorld()->GetAuthGameMode()) return;
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
	SetNarrativeAttitude(FactionA, FactionB, DiplomacyStateToAttitude(EDiplomacyState::TradeAgreement));
	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, EDiplomacyState::TradeAgreement);
	RecordEvent(EDiplomacyEventType::SignedTradeAgreement, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SetDiplomacyState(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode()) return;
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugDiplomacy();

	FTreatyRecord* Existing = FindTreaty(FactionA, FactionB);
	EDiplomacyState OldState = Existing ? Existing->State : EDiplomacyState::None;

	if (OldState == NewState) return;

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
		SetNarrativeAttitude(FactionA, FactionB, ETeamAttitude::Neutral);
	}
	else if (Existing)
	{
		Existing->State = NewState;
		// Sync Narrative attitude based on new treaty state
		SetNarrativeAttitude(FactionA, FactionB, DiplomacyStateToAttitude(NewState));
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
		SetNarrativeAttitude(FactionA, FactionB, DiplomacyStateToAttitude(NewState));
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
	if (!GetWorld()->GetAuthGameMode()) return;
	if (!Faction.IsValid()) return;
	int32& Rep = FactionReputation.FindOrAdd(Faction);
	Rep += Amount;
	OnReputationChanged.Broadcast(Faction, Rep);
}

void UTerritoryDiplomacySubsystem::SetReputation(FGameplayTag Faction, int32 Value)
{
	if (!GetWorld()->GetAuthGameMode()) return;
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
	// Push all treaty-derived attitudes to Narrative GameState
	for (const FTreatyRecord& Treaty : ActiveTreaties)
	{
		SetNarrativeAttitude(Treaty.FactionA, Treaty.FactionB, DiplomacyStateToAttitude(Treaty.State));
	}
}

void UTerritoryDiplomacySubsystem::LoadFromGameState()
{
	// Rebuild treaty metadata from Narrative GameState attitudes.
	// Preserve metadata (timing, permanence, expiry) for treaties that already exist
	// and whose attitude hasn't changed. Only create new treaties for attitudes that
	// have no corresponding treaty record.
	ANarrativeGameState* GS = GetNarrativeGameState();
	if (!GS) return;

	// Track which faction pairs we've seen from GameState attitudes
	TSet<FGuid> SeenPairs;

	for (const auto& Pair : GS->FactionAllianceMap)
	{
		const FGameplayTag& FactionA = Pair.Key;
		for (const auto& AttitudePair : Pair.Value.AttitudeMap)
		{
			const FGameplayTag& FactionB = AttitudePair.Key;
			ETeamAttitude::Type Attitude = AttitudePair.Value;

			// Avoid duplicates (A→B and B→A)
			if (GetTypeHash(FactionA) > GetTypeHash(FactionB)) continue;

			const uint32 HashA = GetTypeHash(FactionA);
			const uint32 HashB = GetTypeHash(FactionB);
			FGuid CanonicalKey = FGuid(FMath::Min(HashA, HashB), FMath::Max(HashA, HashB), 0, 0);
			SeenPairs.Add(CanonicalKey);

			EDiplomacyState State = AttitudeToDiplomacyState(Attitude);
			if (State == EDiplomacyState::None)
			{
				// Neutral attitude — remove any existing treaty for this pair
				FTreatyRecord* Existing = FindTreaty(FactionA, FactionB);
				if (Existing)
				{
					RemoveTreaty(FactionA, FactionB);
				}
				continue;
			}

			// Check if treaty already exists with same state — preserve metadata
			FTreatyRecord* Existing = FindTreaty(FactionA, FactionB);
			if (Existing && Existing->State == State)
			{
				// Keep existing treaty with its metadata (timing, permanence, expiry)
				continue;
			}
			else if (Existing)
			{
				// State changed — update state but preserve metadata
				Existing->State = State;
			}
			else
			{
				// New treaty from attitude
				FTreatyRecord Treaty;
				Treaty.FactionA = FactionA;
				Treaty.FactionB = FactionB;
				Treaty.State = State;
				Treaty.bPermanent = true;
				ActiveTreaties.Add(Treaty);
			}
		}
	}

	// Remove treaties for faction pairs that no longer have any attitude entry
	for (int32 i = ActiveTreaties.Num() - 1; i >= 0; --i)
	{
		FTreatyRecord& Treaty = ActiveTreaties[i];
		FGuid Key = FGuid(
			FMath::Min(GetTypeHash(Treaty.FactionA), GetTypeHash(Treaty.FactionB)),
			FMath::Max(GetTypeHash(Treaty.FactionA), GetTypeHash(Treaty.FactionB)),
			0, 0);
		if (!SeenPairs.Contains(Key))
		{
			ActiveTreaties.RemoveAt(i);
		}
	}
}

void UTerritoryDiplomacySubsystem::SetNarrativeAttitude(FGameplayTag FactionA, FGameplayTag FactionB, ETeamAttitude::Type Attitude)
{
	ANarrativeGameState* GS = GetNarrativeGameState();
	if (!GS) return;
	GS->SetFactionAttitude(FactionA, FactionB, Attitude);
}

void UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged(FGameplayTag Faction, FGameplayTag OtherFaction, ETeamAttitude::Type NewAttitude)
{
	// Reentrancy guard — prevent recursive mutation from delegate listeners
	if (bSuppressSync) return;
	bSuppressSync = true;
	struct FSyncGuard { bool& Flag; ~FSyncGuard() { Flag = false; } } Guard{bSuppressSync};

	// External attitude change from Narrative GameState.
	// Reconcile treaty metadata WITHOUT collapsing rich treaty states.
	//
	// Key invariant: TerritoryFramework is authoritative for rich treaty metadata
	// (TradeAgreement, NonAggression, Ceasefire). Narrative is authoritative for
	// combat attitude only. When an external attitude change arrives:
	// - If a rich treaty exists and the new attitude is compatible, KEEP the treaty.
	// - Only create/overwrite a treaty when NONE exists, or when Hostile (war is
	//   always intentional and overrides peaceful treaties).

	FTreatyRecord* Existing = FindTreaty(Faction, OtherFaction);

	if (NewAttitude == ETeamAttitude::Neutral)
	{
		// Narrative set to Neutral — remove treaty record if present
		if (Existing)
		{
			RemoveTreaty(Faction, OtherFaction);
			OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, EDiplomacyState::None);
		}
	}
	else if (NewAttitude == ETeamAttitude::Hostile)
	{
		// Hostile is always authoritative — overrides any peaceful treaty
		if (Existing && Existing->State != EDiplomacyState::War)
		{
			Existing->State = EDiplomacyState::War;
			OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, EDiplomacyState::War);
		}
		else if (!Existing)
		{
			FTreatyRecord Treaty;
			Treaty.FactionA = Faction;
			Treaty.FactionB = OtherFaction;
			Treaty.State = EDiplomacyState::War;
			Treaty.bPermanent = true;
			ActiveTreaties.Add(Treaty);
			OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, EDiplomacyState::War);
		}
	}
	else if (NewAttitude == ETeamAttitude::Friendly)
	{
		// Friendly attitude is compatible with Alliance, TradeAgreement, NonAggression.
		// If a rich treaty already exists, DO NOT overwrite it to Alliance.
		// Only create a new Alliance treaty when none exists.
		if (!Existing)
		{
			FTreatyRecord Treaty;
			Treaty.FactionA = Faction;
			Treaty.FactionB = OtherFaction;
			Treaty.State = EDiplomacyState::Alliance;
			Treaty.bPermanent = true;
			ActiveTreaties.Add(Treaty);
			OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, EDiplomacyState::Alliance);
		}
		// else: existing treaty (Alliance/TradeAgreement/NonAggression) is preserved
	}
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

	// Cap history to prevent unbounded growth in long sessions
	constexpr int32 MaxDiplomacyHistory = 500;
	while (DiplomacyHistory.Num() > MaxDiplomacyHistory)
	{
		DiplomacyHistory.RemoveAt(0);
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
	TArray<int32> ExpiredIndices;

	for (int32 i = 0; i < ActiveTreaties.Num(); ++i)
	{
		if (ActiveTreaties[i].IsExpired(CurrentTime))
		{
			ExpiredIndices.Add(i);
		}
	}

	for (int32 i = ExpiredIndices.Num() - 1; i >= 0; --i)
	{
		FTreatyRecord& Treaty = ActiveTreaties[ExpiredIndices[i]];
		RecordEvent(EDiplomacyEventType::ExpiredTreaty, Treaty.FactionA, Treaty.FactionB);
		SetDiplomacyState(Treaty.FactionA, Treaty.FactionB, EDiplomacyState::None);
	}
}

void UTerritoryDiplomacySubsystem::OnTreatyExpirationTick()
{
	CheckTreatyExpirations();
}
