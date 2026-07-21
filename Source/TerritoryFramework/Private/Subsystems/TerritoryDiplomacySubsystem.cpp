#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryTypes.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void UTerritoryDiplomacySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (ANarrativeGameState* GS = GetNarrativeGameState())
	{
		GS->OnFactionAttitudeChanged.AddDynamic(this, &UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged);
		LoadFromGameState();
	}

	UE_LOG(LogTerritory, Log, TEXT("TerritoryDiplomacySubsystem initialized"));
}

void UTerritoryDiplomacySubsystem::Deinitialize()
{
	if (ANarrativeGameState* GS = GetNarrativeGameState())
	{
		GS->OnFactionAttitudeChanged.RemoveDynamic(this, &UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged);
	}

	ActiveTreaties.Empty();
	FactionReputation.Empty();
	DiplomacyHistory.Empty();
	Super::Deinitialize();
}

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
	EDiplomacyState Current = GetDiplomacyState(FactionA, FactionB);
	if (Current != EDiplomacyState::Alliance) return;

	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::None);
	RecordEvent(EDiplomacyEventType::BrokeAlliance, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SignTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB, float DurationGameTime)
{
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
	SetDiplomacyState(FactionA, FactionB, EDiplomacyState::TradeAgreement);
	RecordEvent(EDiplomacyEventType::SignedTradeAgreement, FactionA, FactionB);
}

void UTerritoryDiplomacySubsystem::SetDiplomacyState(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;

	FTreatyRecord* Existing = FindTreaty(FactionA, FactionB);
	EDiplomacyState OldState = Existing ? Existing->State : EDiplomacyState::None;

	if (OldState == NewState) return;

	if (NewState == EDiplomacyState::None)
	{
		RemoveTreaty(FactionA, FactionB);
	}
	else if (Existing)
	{
		Existing->State = NewState;
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
	}

	SyncToGameState();
	OnDiplomacyStateChanged.Broadcast(FactionA, FactionB, NewState);
}

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

void UTerritoryDiplomacySubsystem::SyncToGameState()
{
	ANarrativeGameState* GS = GetNarrativeGameState();
	if (!GS) return;

	for (const FTreatyRecord& Treaty : ActiveTreaties)
	{
		ETeamAttitude::Type Attitude = DiplomacyStateToAttitude(Treaty.State);
		GS->SetFactionAttitude(Treaty.FactionA, Treaty.FactionB, Attitude);
	}

	// Reset factions with no active treaty to neutral
	// (only if they had a non-neutral attitude from a now-removed treaty)
}

void UTerritoryDiplomacySubsystem::LoadFromGameState()
{
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

			EDiplomacyState State = EDiplomacyState::None;
			switch (Attitude)
			{
			case ETeamAttitude::Friendly: State = EDiplomacyState::Alliance; break;
			case ETeamAttitude::Hostile: State = EDiplomacyState::War; break;
			default: State = EDiplomacyState::None; break;
			}

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
		return ETeamAttitude::Friendly;
	case EDiplomacyState::War:
		return ETeamAttitude::Hostile;
	default:
		return ETeamAttitude::Neutral;
	}
}

void UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged(FGameplayTag Faction, FGameplayTag OtherFaction, ETeamAttitude::Type NewAttitude)
{
	// External change via GameState — sync back to our treaty records
	EDiplomacyState NewState = EDiplomacyState::None;
	switch (NewAttitude)
	{
	case ETeamAttitude::Friendly: NewState = EDiplomacyState::Alliance; break;
	case ETeamAttitude::Hostile: NewState = EDiplomacyState::War; break;
	default: NewState = EDiplomacyState::None; break;
	}

	const FTreatyRecord* Existing = FindTreaty(Faction, OtherFaction);
	if (Existing && Existing->State != NewState)
	{
		// Find mutable reference
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
