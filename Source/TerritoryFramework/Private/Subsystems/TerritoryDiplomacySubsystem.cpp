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
		World->GetTimerManager().SetTimer(
			TreatyExpirationTimerHandle,
			this,
			&UTerritoryDiplomacySubsystem::OnTreatyExpirationTick,
			10.f,
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
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::War);
	RecordEvent(EDiplomacyEventType::DeclaredWar, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::DeclarePeace(FGameplayTag FactionA, FGameplayTag FactionB)
{
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::Ceasefire);
	RecordEvent(EDiplomacyEventType::DeclaredPeace, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::FormAlliance(FGameplayTag FactionA, FGameplayTag FactionB)
{
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::Alliance);
	RecordEvent(EDiplomacyEventType::FormedAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::BreakAlliance(FGameplayTag FactionA, FGameplayTag FactionB)
{
	if (GetDiplomacyState(FactionA, FactionB) != EDiplomacyState::Alliance) return;

	// Remove treaty metadata, then reset Narrative attitude to Neutral
	RemoveTreaty(FactionA, FactionB);
	SetNarrativeAttitude(FactionA, FactionB, ETeamAttitude::Neutral);
	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, EDiplomacyState::None);
	RecordEvent(EDiplomacyEventType::BrokeAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SignTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB, float DurationGameTime)
{
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
	if (!Faction.IsValid()) return;
	int32& Rep = FactionReputation.FindOrAdd(Faction);
	Rep += Amount;
	OnReputationChanged.Broadcast(Faction, Rep);
}

void UTerritoryDiplomacySubsystem::SetReputation(FGameplayTag Faction, int32 Value)
{
	if (!Faction.IsValid()) return;
	FactionReputation.FindOrAdd(Faction) = Value;
	OnReputationChanged.Broadcast(Faction, Value);
}

int32 UTerritoryDiplomacySubsystem::GetReputation(FGameplayTag Faction) const
{
	const int32* Rep = FactionReputation.Find(Faction);
	return Rep ? *Rep : 0;
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
	// Rebuild treaty metadata from Narrative GameState attitudes
	ANarrativeGameState* GS = GetNarrativeGameState();
	if (!GS) return;

	ActiveTreaties.Empty();

	for (const auto& Pair : GS->FactionAllianceMap)
	{
		const FGameplayTag& FactionA = Pair.Key;
		for (const auto& AttitudePair : Pair.Value.AttitudeMap)
		{
			const FGameplayTag& FactionB = AttitudePair.Key;
			ETeamAttitude::Type Attitude = AttitudePair.Value;

			// Avoid duplicates (A→B and B→A)
			if (GetTypeHash(FactionA) > GetTypeHash(FactionB)) continue;

			EDiplomacyState State = AttitudeToDiplomacyState(Attitude);
			if (State != EDiplomacyState::None)
			{
				FTreatyRecord Treaty;
				Treaty.FactionA = FactionA;
				Treaty.FactionB = FactionB;
				Treaty.State = State;
				Treaty.bPermanent = true;
				ActiveTreaties.Add(Treaty);
			}
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
	// External attitude change from Narrative GameState.
	// Reconcile treaty metadata with the new attitude.

	EDiplomacyState NewState = AttitudeToDiplomacyState(NewAttitude);
	const FTreatyRecord* Existing = FindTreaty(Faction, OtherFaction);

	if (NewState == EDiplomacyState::None)
	{
		// FIX: Narrative set to Neutral — remove dead treaty record
		if (Existing)
		{
			RemoveTreaty(Faction, OtherFaction);
			OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, EDiplomacyState::None);
		}
	}
	else if (Existing && Existing->State != NewState)
	{
		// FIX: Attitude changed — update treaty state, preserve metadata (timing, permanence)
		FTreatyRecord* MutableTreaty = const_cast<FTreatyRecord*>(Existing);
		MutableTreaty->State = NewState;
		OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, NewState);
	}
	else if (!Existing && NewState != EDiplomacyState::None)
	{
		FTreatyRecord Treaty;
		Treaty.FactionA = Faction;
		Treaty.FactionB = OtherFaction;
		Treaty.State = NewState;
		Treaty.bPermanent = true;
		ActiveTreaties.Add(Treaty);
		OnDiplomacyStateChanged.Broadcast(Faction, OtherFaction, NewState);
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
