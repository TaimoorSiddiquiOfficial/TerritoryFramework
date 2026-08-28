#include "Tales/TerritoryStoryConditions.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"

namespace
{
	ATerritoryVolume* ResolveLoadedTerritory(const UObject* Context, const FGameplayTag& Tag)
	{
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		UTerritoryRegistrySubsystem* Registry = World
			? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
		return Registry && Tag.IsValid() ? Registry->GetTerritoryByTag(Tag) : nullptr;
	}

	FString EnumDisplayName(const UEnum* Enum, int64 Value, const TCHAR* Fallback)
	{
		return Enum ? Enum->GetDisplayNameTextByValue(Value).ToString() : FString(Fallback);
	}
}

UTerritoryQuestStateCondition::UTerritoryQuestStateCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryQuestStateCondition::CheckCondition_Implementation(
	APawn*, APlayerController*, UTalesComponent* NarrativeComponent)
{
	return UTerritoryQuestRulesLibrary::DoesQuestStateMatch(
		NarrativeComponent, QuestClass, RequiredState);
}

FString UTerritoryQuestStateCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Narrative quest %s is %s"), *GetNameSafe(QuestClass.Get()),
		*EnumDisplayName(StaticEnum<ETerritoryQuestStateRequirement>(),
			static_cast<int64>(RequiredState), TEXT("Unknown")));
}

UTerritoryEventContextCondition::UTerritoryEventContextCondition()
{
	ConditionFilter = EConditionFilter::CF_AnyCharacter;
}

bool UTerritoryEventContextCondition::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	const bool bHasValidTarget = IsValid(Target) && !Target->IsActorBeingDestroyed();
	if (bRequireTargetPawn && !bHasValidTarget)
	{
		return false;
	}
	if (bRequirePlayerControlledTarget
		&& (!bHasValidTarget || !Target->IsPlayerControlled()))
	{
		return false;
	}
	if (bRequireAbilitySystemComponent
		&& (!bHasValidTarget
			|| !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target)))
	{
		return false;
	}
	if (bRequirePlayerController
		&& (!IsValid(Controller) || Controller->IsActorBeingDestroyed()))
	{
		return false;
	}
	if (bRequireTalesComponent && !IsValid(NarrativeComponent))
	{
		return false;
	}
	return true;
}

FString UTerritoryEventContextCondition::GetGraphDisplayText_Implementation()
{
	TArray<FString> Requirements;
	if (bRequireTargetPawn) Requirements.Add(TEXT("valid target pawn"));
	if (bRequirePlayerControlledTarget) Requirements.Add(TEXT("player-controlled target"));
	if (bRequireAbilitySystemComponent) Requirements.Add(TEXT("target Ability System"));
	if (bRequirePlayerController) Requirements.Add(TEXT("player controller"));
	if (bRequireTalesComponent) Requirements.Add(TEXT("Tales component"));
	return Requirements.IsEmpty()
		? TEXT("Event context: no requirements")
		: FString::Printf(TEXT("Event context requires %s"),
			*FString::Join(Requirements, TEXT(", ")));
}

UTerritoryStateCondition::UTerritoryStateCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryStateCondition::CheckCondition_Implementation(APawn*, APlayerController*, UTalesComponent*)
{
	const ATerritoryVolume* Territory = ResolveLoadedTerritory(this, TerritoryToCheck);
	return Territory && Territory->GetTerritoryState() == RequiredState;
}

FString UTerritoryStateCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Territory: %s state is %s"), *TerritoryToCheck.ToString(),
		*EnumDisplayName(StaticEnum<ETerritoryState>(), static_cast<int64>(RequiredState), TEXT("Unknown")));
}

UTerritoryControlProgressCondition::UTerritoryControlProgressCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryControlProgressCondition::CompareValues(float ActualValue,
	ETerritoryFloatComparison Operation, float RequiredValue, float Tolerance)
{
	switch (Operation)
	{
	case ETerritoryFloatComparison::NearlyEqual:
		return FMath::IsNearlyEqual(ActualValue, RequiredValue, FMath::Max(0.f, Tolerance));
	case ETerritoryFloatComparison::AtLeast: return ActualValue >= RequiredValue;
	case ETerritoryFloatComparison::AtMost: return ActualValue <= RequiredValue;
	case ETerritoryFloatComparison::GreaterThan: return ActualValue > RequiredValue;
	case ETerritoryFloatComparison::LessThan: return ActualValue < RequiredValue;
	default: return false;
	}
}

bool UTerritoryControlProgressCondition::CheckCondition_Implementation(APawn*, APlayerController*, UTalesComponent*)
{
	const ATerritoryVolume* Territory = ResolveLoadedTerritory(this, TerritoryToCheck);
	return Territory && CompareValues(Territory->GetControlProgress() * 100.f, Comparison,
		FMath::Clamp(ProgressPercent, 0.f, 100.f), FMath::Max(0.f, EqualityTolerancePercent));
}

FString UTerritoryControlProgressCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Territory: %s control %s %.1f%%"), *TerritoryToCheck.ToString(),
		*EnumDisplayName(StaticEnum<ETerritoryFloatComparison>(), static_cast<int64>(Comparison), TEXT("Compare")),
		FMath::Clamp(ProgressPercent, 0.f, 100.f));
}

UTerritoryReputationCondition::UTerritoryReputationCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryReputationCondition::CheckCondition_Implementation(APawn*, APlayerController*, UTalesComponent*)
{
	UWorld* World = GetWorld();
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	return Diplomacy && Faction.IsValid()
		&& UTerritoryGarrisonCondition::CompareValues(Diplomacy->GetReputation(Faction), Comparison, Value);
}

FString UTerritoryReputationCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Reputation: %s %s %d"), *Faction.ToString(),
		*EnumDisplayName(StaticEnum<ETerritoryIntegerComparison>(), static_cast<int64>(Comparison), TEXT("Compare")), Value);
}

UTerritoryFactionDistrictHoldingCondition::UTerritoryFactionDistrictHoldingCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryFactionDistrictHoldingCondition::CheckCondition_Implementation(
	APawn*, APlayerController*, UTalesComponent*)
{
	UWorld* World = GetWorld();
	const UTerritoryCounterAttackSubsystem* Counter = World
		? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
	return Counter && Faction.IsValid()
		&& UTerritoryGarrisonCondition::CompareValues(
			Counter->GetSecureDistrictCountForFaction(Faction), Comparison,
			FMath::Max(0, DistrictCount));
}

FString UTerritoryFactionDistrictHoldingCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Secure Districts: %s %s %d"), *Faction.ToString(),
		*EnumDisplayName(StaticEnum<ETerritoryIntegerComparison>(),
			static_cast<int64>(Comparison), TEXT("Compare")), FMath::Max(0, DistrictCount));
}

UTerritoryAssaultCondition::UTerritoryAssaultCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

const FTerritoryAssaultRecord* UTerritoryAssaultCondition::SelectLatestRecord(
	const TArray<FTerritoryAssaultRecord>& Records)
{
	const FTerritoryAssaultRecord* Best = nullptr;
	for (const FTerritoryAssaultRecord& Candidate : Records)
	{
		if (!Best)
		{
			Best = &Candidate;
			continue;
		}
		const bool bCandidateLive = !Candidate.IsTerminal();
		const bool bBestLive = !Best->IsTerminal();
		if (bCandidateLive != bBestLive)
		{
			if (bCandidateLive) Best = &Candidate;
			continue;
		}
		if (Candidate.EvaluationCycle > Best->EvaluationCycle
			|| (Candidate.EvaluationCycle == Best->EvaluationCycle
				&& Candidate.ScheduledGameTime > Best->ScheduledGameTime)
			|| (Candidate.EvaluationCycle == Best->EvaluationCycle
				&& Candidate.ScheduledGameTime == Best->ScheduledGameTime
				&& Candidate.AssaultID.ToString() > Best->AssaultID.ToString()))
		{
			Best = &Candidate;
		}
	}
	return Best;
}

bool UTerritoryAssaultCondition::CheckCondition_Implementation(APawn*, APlayerController*, UTalesComponent*)
{
	UWorld* World = GetWorld();
	const UTerritoryCounterAttackSubsystem* Counter = World
		? World->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr;
	if (!Counter || !TerritoryToCheck.IsValid()) return false;

	const ATerritoryVolume* LoadedTerritory = ResolveLoadedTerritory(this, TerritoryToCheck);
	const TArray<FTerritoryAssaultRecord> Records = LoadedTerritory
		? Counter->GetAssaultsForTerritoryActor(LoadedTerritory)
		: Counter->GetAssaultsForTerritory(TerritoryToCheck);
	if (Query == ETerritoryAssaultConditionQuery::AnyPendingOrActive)
	{
		return Records.ContainsByPredicate([](const FTerritoryAssaultRecord& Record)
		{
			return !Record.IsTerminal();
		});
	}
	if (Query == ETerritoryAssaultConditionQuery::PhysicalAssaultActive)
	{
		return Records.ContainsByPredicate([](const FTerritoryAssaultRecord& Record)
		{
			return Record.State == ETerritoryAssaultState::Active;
		});
	}

	const FTerritoryAssaultRecord* Record = SelectLatestRecord(Records);
	if (!Record) return false;
	if (Query == ETerritoryAssaultConditionQuery::LatestState) return Record->State == RequiredState;
	if (Query == ETerritoryAssaultConditionQuery::LatestResolution) return Record->Resolution == RequiredResolution;

	int32 ActualValue = 0;
	switch (Query)
	{
	case ETerritoryAssaultConditionQuery::PlannedAttackers: ActualValue = Record->PlannedForce; break;
	case ETerritoryAssaultConditionQuery::LivingAttackers: ActualValue = Record->AliveForce; break;
	case ETerritoryAssaultConditionQuery::PendingReserveAttackers: ActualValue = Record->PendingReserveForce; break;
	case ETerritoryAssaultConditionQuery::KilledAttackers: ActualValue = Record->KilledForce; break;
	case ETerritoryAssaultConditionQuery::WithdrawnAttackers: ActualValue = Record->WithdrawnForce; break;
	case ETerritoryAssaultConditionQuery::RemainingAttackers:
		ActualValue = FMath::Max(0, Record->AliveForce) + FMath::Max(0, Record->PendingReserveForce);
		break;
	default: return false;
	}
	return UTerritoryGarrisonCondition::CompareValues(ActualValue, Comparison, FMath::Max(0, Value));
}

FString UTerritoryAssaultCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Enemy wave: %s %s"), *TerritoryToCheck.ToString(),
		*EnumDisplayName(StaticEnum<ETerritoryAssaultConditionQuery>(), static_cast<int64>(Query), TEXT("Assault query")));
}

UTerritoryPresenceCondition::UTerritoryPresenceCondition()
{
	ConditionFilter = EConditionFilter::CF_AnyCharacter;
}

bool UTerritoryPresenceCondition::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent*)
{
	APawn* ContextPawn = Target ? Target : (Controller ? Controller->GetPawn().Get() : nullptr);
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!ContextPawn || !Registry || !TerritoryToCheck.IsValid()) return false;

	for (ATerritoryVolume* Territory : Registry->GetTerritoriesAtLocation(ContextPawn->GetActorLocation()))
	{
		for (int32 Depth = 0; Territory && Depth < 32; ++Depth)
		{
			if (Territory->GetTerritoryTag() == TerritoryToCheck) return true;
			if (!bIncludeChildTerritories || !Territory->GetParentTerritoryTag().IsValid()) break;
			Territory = Registry->GetTerritoryByTag(Territory->GetParentTerritoryTag());
		}
	}
	return false;
}

FString UTerritoryPresenceCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Narrative target is inside %s%s"), *TerritoryToCheck.ToString(),
		bIncludeChildTerritories ? TEXT(" or a child Place") : TEXT(""));
}

UTerritoryProductionStatusCondition::UTerritoryProductionStatusCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryProductionStatusCondition::CheckCondition_Implementation(APawn*, APlayerController*, UTalesComponent*)
{
	UWorld* World = GetWorld();
	const UTerritoryEconomySubsystem* Economy = World
		? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	if (!Economy || !TerritoryToCheck.IsValid()) return false;
	const FTerritoryProductionSiteRecord Site = Economy->GetProductionSite(TerritoryToCheck);
	if (Site.TerritoryTag != TerritoryToCheck) return false;
	if (!RuleTag.IsValid()) return Site.LastStatus == RequiredStatus;
	const FTerritoryProductionRuleState* Rule = Site.RuleStates.FindByPredicate(
		[this](const FTerritoryProductionRuleState& Candidate)
		{
			return Candidate.RuleTag == RuleTag;
		});
	return Rule && Rule->Status == RequiredStatus;
}

FString UTerritoryProductionStatusCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Production: %s%s status is %s"), *TerritoryToCheck.ToString(),
		RuleTag.IsValid() ? *FString::Printf(TEXT(" / %s"), *RuleTag.ToString()) : TEXT(""),
		*EnumDisplayName(StaticEnum<ETerritoryProductionStatus>(), static_cast<int64>(RequiredStatus), TEXT("Unknown")));
}

UTerritoryResourceCondition::UTerritoryResourceCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritoryResourceCondition::CheckCondition_Implementation(APawn*, APlayerController*, UTalesComponent*)
{
	UWorld* World = GetWorld();
	const UTerritoryEconomySubsystem* Economy = World
		? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	if (!Economy || !Faction.IsValid() || !ResourceItem) return false;
	const FTerritoryFactionResourceSnapshot Snapshot = Economy->GetFactionResourceSnapshot(Faction);
	if (!Snapshot.bStorageAvailable) return false;
	int32 ActualQuantity = 0;
	for (const FTerritoryResourceAmount& Resource : Snapshot.Resources)
	{
		if (Resource.ItemClass == ResourceItem)
		{
			ActualQuantity = FMath::Max(0, Resource.Quantity);
			break;
		}
	}
	return UTerritoryGarrisonCondition::CompareValues(ActualQuantity, Comparison, FMath::Max(0, Quantity));
}

FString UTerritoryResourceCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Resources: %s has %s %s %d"), *Faction.ToString(),
		*GetNameSafe(ResourceItem.Get()),
		*EnumDisplayName(StaticEnum<ETerritoryIntegerComparison>(), static_cast<int64>(Comparison), TEXT("Compare")),
		FMath::Max(0, Quantity));
}
