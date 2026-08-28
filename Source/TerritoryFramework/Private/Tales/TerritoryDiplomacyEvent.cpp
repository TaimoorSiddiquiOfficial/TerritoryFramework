#include "Tales/TerritoryDiplomacyEvent.h"

#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
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

	FString GetFactionSourceDisplayName(ETerritoryDiplomacyFactionSource Source)
	{
		const UEnum* SourceEnum = StaticEnum<ETerritoryDiplomacyFactionSource>();
		return SourceEnum
			? SourceEnum->GetDisplayNameTextByValue(static_cast<int64>(Source)).ToString()
			: TEXT("Unknown Source");
	}

	FGameplayTag ResolveFactionSource(const ATerritoryVolume* Territory,
		ETerritoryDiplomacyFactionSource Source, const FGameplayTag& ExplicitFallback,
		bool bAllowFallback)
	{
		FGameplayTag Result;
		if (Source == ETerritoryDiplomacyFactionSource::ExplicitTag)
		{
			return ExplicitFallback;
		}

		if (Territory)
		{
			switch (Source)
			{
			case ETerritoryDiplomacyFactionSource::CurrentOwningFaction:
				Result = Territory->GetOwningFaction();
				break;
			case ETerritoryDiplomacyFactionSource::PreviousOwningFaction:
				Result = Territory->GetTransitionPreviousOwningFaction();
				break;
			case ETerritoryDiplomacyFactionSource::ContestingFaction:
				Result = Territory->GetContestingFaction_Implementation();
				break;
			case ETerritoryDiplomacyFactionSource::TransitionRequestingFaction:
				if (Territory->IsOwnershipTransitionActive())
				{
					Result = Territory->GetActiveTransitionContext().RequestingFaction;
				}
				break;
			case ETerritoryDiplomacyFactionSource::ExplicitTag:
			default:
				break;
			}
		}

		return Result.IsValid() || !bAllowFallback ? Result : ExplicitFallback;
	}

	bool IsSameUnorderedPair(const FGameplayTag& Owner, const FGameplayTag& Contesting,
		const FGameplayTag& FactionA, const FGameplayTag& FactionB)
	{
		return (Owner == FactionA && Contesting == FactionB)
			|| (Owner == FactionB && Contesting == FactionA);
	}

	bool HasOtherActiveTerritoryConflict(const UWorld* World,
		const ATerritoryVolume* ContainingTerritory,
		const FGameplayTag& FactionA, const FGameplayTag& FactionB)
	{
		if (!World || !FactionA.IsValid() || !FactionB.IsValid()) return false;
		const FGameplayTag ExcludedTag = ContainingTerritory
			? ContainingTerritory->GetTerritoryTag() : FGameplayTag();

		if (const UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			for (const ATerritoryVolume* Territory : Registry->GetAllTerritories())
			{
				if (!Territory || Territory == ContainingTerritory
					|| Territory->GetTerritoryState() != ETerritoryState::Contested)
				{
					continue;
				}
				if (IsSameUnorderedPair(Territory->GetOwningFaction(),
					Territory->GetContestingFaction_Implementation(), FactionA, FactionB))
				{
					return true;
				}
			}
		}

		for (TActorIterator<ATerritoryWorldState> It(World); It; ++It)
		{
			if (It->HasContestedTerritoryBetweenFactions(FactionA, FactionB, ExcludedTag))
			{
				return true;
			}
		}
		return false;
	}
}

UTerritorySetDiplomacyEvent::UTerritorySetDiplomacyEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

bool UTerritorySetDiplomacyEvent::ResolveFactionPair(
	FGameplayTag& OutFactionA, FGameplayTag& OutFactionB) const
{
	const ATerritoryVolume* Territory = GetTypedOuter<ATerritoryVolume>();
	OutFactionA = ResolveFactionSource(Territory, FactionASource, FactionA,
		bFallbackToExplicitFactionWhenContextMissing);
	OutFactionB = ResolveFactionSource(Territory, FactionBSource, FactionB,
		bFallbackToExplicitFactionWhenContextMissing);
	return OutFactionA.IsValid() && OutFactionB.IsValid() && OutFactionA != OutFactionB;
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
	FGameplayTag ResolvedFactionA;
	FGameplayTag ResolvedFactionB;
	if (!ResolveFactionPair(ResolvedFactionA, ResolvedFactionB))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[TalesDiplomacyEvent] Could not resolve two different factions from %s and %s"),
			*GetFactionSourceDisplayName(FactionASource),
			*GetFactionSourceDisplayName(FactionBSource));
		return;
	}

	const ATerritoryVolume* ContainingTerritory = GetTypedOuter<ATerritoryVolume>();
	if (bRequireContainingTerritoryOwner && ContainingTerritory)
	{
		const FGameplayTag CurrentOwner = ContainingTerritory->GetOwningFaction();
		if (CurrentOwner.IsValid()
			&& CurrentOwner != ResolvedFactionA && CurrentOwner != ResolvedFactionB)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[TalesDiplomacyEvent] %s skipped stale pair %s/%s because current owner is %s"),
				*ContainingTerritory->GetTerritoryTag().ToString(),
				*ResolvedFactionA.ToString(), *ResolvedFactionB.ToString(),
				*CurrentOwner.ToString());
			return;
		}
	}

	if (NewState != EDiplomacyState::War && bPreserveOtherActiveTerritoryWars
		&& HasOtherActiveTerritoryConflict(World, ContainingTerritory,
			ResolvedFactionA, ResolvedFactionB))
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[TalesDiplomacyEvent] Kept %s/%s at War because another Territory is still Contested"),
			*ResolvedFactionA.ToString(), *ResolvedFactionB.ToString());
		return;
	}

	UTerritoryDiplomacySubsystem* Diplomacy =
		World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!Diplomacy) return;

	switch (NewState)
	{
	case EDiplomacyState::War:
		Diplomacy->DeclareWar(ResolvedFactionA, ResolvedFactionB);
		break;
	case EDiplomacyState::Ceasefire:
		Diplomacy->DeclarePeace(ResolvedFactionA, ResolvedFactionB);
		break;
	case EDiplomacyState::Alliance:
		Diplomacy->FormAlliance(ResolvedFactionA, ResolvedFactionB);
		break;
	case EDiplomacyState::NonAggression:
		Diplomacy->SignNonAggression(ResolvedFactionA, ResolvedFactionB);
		break;
	case EDiplomacyState::TradeAgreement:
		Diplomacy->SignTradeAgreement(ResolvedFactionA, ResolvedFactionB, TradeDurationGameTime);
		break;
	case EDiplomacyState::None:
	default:
		Diplomacy->SetDiplomacyState(ResolvedFactionA, ResolvedFactionB, NewState);
		break;
	}

	if (Diplomacy->GetDiplomacyState(ResolvedFactionA, ResolvedFactionB) != NewState)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[TalesDiplomacyEvent] Failed to commit %s between %s and %s"),
			*GetDiplomacyDisplayName(NewState), *ResolvedFactionA.ToString(),
			*ResolvedFactionB.ToString());
	}
}

UTerritoryModifyReputationEvent::UTerritoryModifyReputationEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

FString UTerritorySetDiplomacyEvent::GetGraphDisplayText_Implementation()
{
	const FString SideA = FactionASource == ETerritoryDiplomacyFactionSource::ExplicitTag
		? FactionA.ToString() : GetFactionSourceDisplayName(FactionASource);
	const FString SideB = FactionBSource == ETerritoryDiplomacyFactionSource::ExplicitTag
		? FactionB.ToString() : GetFactionSourceDisplayName(FactionBSource);
	return FString::Printf(TEXT("Diplomacy: set %s and %s to %s"),
		*SideA, *SideB, *GetDiplomacyDisplayName(NewState));
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
