#include "Tales/TerritoryDiplomacyEvent.h"

#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

namespace
{
	FString GetDiplomacyDisplayName(EDiplomacyState State)
	{
		const UEnum* StateEnum = StaticEnum<EDiplomacyState>();
		return StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(State)).ToString()
			: TEXT("Unknown");
	}
}

UTerritorySetDiplomacyEvent::UTerritorySetDiplomacyEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritorySetDiplomacyEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	(void)Target;
	(void)Controller;
	(void)NarrativeComponent;

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	if (!FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesDiplomacyEvent] Choose two different valid Narrative faction tags"));
		return;
	}

	UTerritoryDiplomacySubsystem* Diplomacy =
		World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!Diplomacy) return;

	switch (NewState)
	{
	case EDiplomacyState::War:
		Diplomacy->DeclareWar(FactionA, FactionB);
		break;
	case EDiplomacyState::Ceasefire:
		Diplomacy->DeclarePeace(FactionA, FactionB);
		break;
	case EDiplomacyState::Alliance:
		Diplomacy->FormAlliance(FactionA, FactionB);
		break;
	case EDiplomacyState::NonAggression:
		Diplomacy->SignNonAggression(FactionA, FactionB);
		break;
	case EDiplomacyState::TradeAgreement:
		Diplomacy->SignTradeAgreement(FactionA, FactionB, TradeDurationGameTime);
		break;
	case EDiplomacyState::None:
	default:
		Diplomacy->SetDiplomacyState(FactionA, FactionB, NewState);
		break;
	}

	if (Diplomacy->GetDiplomacyState(FactionA, FactionB) != NewState)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[TalesDiplomacyEvent] Failed to commit %s between %s and %s"),
			*GetDiplomacyDisplayName(NewState), *FactionA.ToString(), *FactionB.ToString());
	}
}

UTerritoryModifyReputationEvent::UTerritoryModifyReputationEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

FString UTerritorySetDiplomacyEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Diplomacy: set %s and %s to %s"),
		*FactionA.ToString(), *FactionB.ToString(), *GetDiplomacyDisplayName(NewState));
}

void UTerritoryModifyReputationEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(this, Target, Controller, NarrativeComponent)) return;
	(void)Target;
	(void)Controller;
	(void)NarrativeComponent;

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Faction.IsValid()) return;
	UTerritoryDiplomacySubsystem* Diplomacy =
		World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!Diplomacy) return;

	const int64 DesiredValue = Operation == ETerritoryReputationOperation::Add
		? static_cast<int64>(Diplomacy->GetReputation(Faction)) + static_cast<int64>(Value)
		: static_cast<int64>(Value);
	const int32 SafeValue = static_cast<int32>(FMath::Clamp<int64>(
		DesiredValue, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
	Diplomacy->SetReputation(Faction, SafeValue);
	if (Diplomacy->GetReputation(Faction) != SafeValue)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[TalesReputationEvent] Failed to commit reputation %d for %s"),
			SafeValue, *Faction.ToString());
	}
}

FString UTerritoryModifyReputationEvent::GetGraphDisplayText_Implementation()
{
	return Operation == ETerritoryReputationOperation::Add
		? FString::Printf(TEXT("Reputation: add %d to %s"), Value, *Faction.ToString())
		: FString::Printf(TEXT("Reputation: set %s to %d"), *Faction.ToString(), Value);
}
