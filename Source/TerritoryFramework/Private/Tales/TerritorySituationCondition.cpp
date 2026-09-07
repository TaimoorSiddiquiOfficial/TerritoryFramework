#include "Tales/TerritorySituationCondition.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DataValidation.h"

bool UTerritorySituationProfile::ReadPlaceHoldings(
	TConstArrayView<FReplicatedCaptureSummary> Rows, FGameplayTag ScopeTag,
	FGameplayTag Faction, FTerritorySituationReport& Report,
	TArray<FReplicatedCaptureSummary>* OutPlaces)
{
	Report.bHoldingsKnown = false;
	Report.AvailablePlaces = Report.FactionPlaces = 0;
	Report.FactionSharePercent = 0.f;
	Report.DominantFaction = FGameplayTag();
	if (OutPlaces) OutPlaces->Reset();
	if (!ScopeTag.IsValid() || !Faction.IsValid()) return false;
	TMap<FGameplayTag, const FReplicatedCaptureSummary*> ByTag;
	TSet<FGuid> SeenIDs;
	for (const FReplicatedCaptureSummary& Row : Rows)
	{
		if (!Row.TerritoryTag.IsValid()) continue;
		// An ambiguous directory must not choose whichever duplicate arrived first.
		if (ByTag.Contains(Row.TerritoryTag)
			|| (Row.TerritoryGUID.IsValid() && SeenIDs.Contains(Row.TerritoryGUID))) return false;
		ByTag.Add(Row.TerritoryTag, &Row);
		if (Row.TerritoryGUID.IsValid()) SeenIDs.Add(Row.TerritoryGUID);
	}
	const auto* Found = ByTag.Find(ScopeTag);
	if (!Found || ((*Found)->HierarchyLevel != ETerritoryHierarchyLevel::District
		&& (*Found)->HierarchyLevel != ETerritoryHierarchyLevel::City)
		|| (*Found)->Availability != ETerritoryAvailability::Unlocked) return false;
	TArray<FReplicatedCaptureSummary> Places;
	TFunction<bool(const FReplicatedCaptureSummary&, int32)> Visit;
	Visit = [&](const FReplicatedCaptureSummary& Parent, int32 Depth)
	{
		if (Depth > 2 || Parent.TotalChildren < 0) return false;
		const ETerritoryHierarchyLevel Expected = Parent.HierarchyLevel == ETerritoryHierarchyLevel::City
			? ETerritoryHierarchyLevel::District : ETerritoryHierarchyLevel::Place;
		int32 Children = 0;
		for (const FReplicatedCaptureSummary& Row : Rows)
		{
			if (Row.ParentTerritoryTag != Parent.TerritoryTag) continue;
			++Children;
			if (Row.HierarchyLevel != Expected) return false;
			if (Row.Availability != ETerritoryAvailability::Unlocked) continue;
			if (Expected == ETerritoryHierarchyLevel::Place) Places.Add(Row);
			else if (!Visit(Row, Depth + 1)) return false;
		}
		return Children == Parent.TotalChildren;
	};
	if (!Visit(**Found, 0)) return false;
	Places.Sort([](const FReplicatedCaptureSummary& A, const FReplicatedCaptureSummary& B)
	{
		return A.TerritoryTag.ToString() < B.TerritoryTag.ToString();
	});
	TArray<FGameplayTag> SecureOwners;
	for (const FReplicatedCaptureSummary& Place : Places)
	{
		const FGameplayTag Owner = Place.State == ETerritoryState::Claimed
			? Place.CurrentOwner : FGameplayTag();
		SecureOwners.Add(Owner);
		if (Owner == Faction) ++Report.FactionPlaces;
	}
	Report.AvailablePlaces = Places.Num();
	Report.FactionSharePercent = Places.IsEmpty() ? 0.f
		: 100.f * static_cast<float>(Report.FactionPlaces) / Places.Num();
	Report.DominantFaction = TerritoryHierarchyPolicy::FindStrictMajorityOwner(SecureOwners);
	Report.ScopeTerritory = ScopeTag;
	Report.bHoldingsKnown = true;
	if (OutPlaces) *OutPlaces = MoveTemp(Places);
	return true;
}

FGameplayTag UTerritorySituationProfile::ResolveRequestingFaction(APawn* NarrativeTarget,
	APlayerController* Controller, UTalesComponent* Tales) const
{
	if (!IsValid(NarrativeTarget) && IsValid(Tales)) NarrativeTarget = Tales->GetOwningPawn();
	if (!IsValid(Controller) && IsValid(Tales)) Controller = Tales->GetOwningController();
	switch (FactionSource)
	{
	case ETerritoryCaptureFactionSource::ExplicitFaction: return ExplicitFaction;
	case ETerritoryCaptureFactionSource::NarrativeTargetFaction:
		return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, NarrativeTarget);
	case ETerritoryCaptureFactionSource::ControllerPawnFaction:
		return UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Controller ? Controller->GetPawn().Get() : nullptr);
	default: return FGameplayTag();
	}
}

FTerritorySituationReport UTerritorySituationProfile::InspectSituation(
	APawn* NarrativeTarget, APlayerController* Controller, UTalesComponent* Tales,
	ETerritorySituationScope Scope) const
{
	FTerritorySituationReport Report;
	UWorld* World = TerritoryTales::ResolveWorld(this, NarrativeTarget, Controller, Tales);
	if (!World || !Territory.IsValid()
		|| (Scope != ETerritorySituationScope::District && Scope != ETerritorySituationScope::City)
		|| (IsValid(Controller) && Controller->GetWorld() != World)
		|| (IsValid(Tales) && Tales->GetWorld() != World)) return Report;
	Report.RequestingFaction = ResolveRequestingFaction(NarrativeTarget, Controller, Tales);
	ATerritoryWorldState* State = ATerritoryWorldState::FindTerritoryWorldState(World);
	if (!State || !Report.RequestingFaction.IsValid()) return Report;
	const FReplicatedCaptureSummary Target = State->GetCaptureSummary(Territory);
	if (Target.TerritoryTag != Territory || Target.HierarchyLevel != ETerritoryHierarchyLevel::Place) return Report;
	Report.bContextKnown = true;
	Report.CurrentOwner = Target.CurrentOwner;
	Report.bPreviouslyOwned = Target.FormerOwningFactions.HasTagExact(Report.RequestingFaction);
	Report.bRetakeNeeded = Report.bPreviouslyOwned && Report.CurrentOwner != Report.RequestingFaction;
	const UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	const auto ReadRelationship = [&](FGameplayTag Other, EDiplomacyState& OutState)
	{
		if (!Diplomacy || !Other.IsValid()) return false;
		OutState = Other == Report.RequestingFaction ? EDiplomacyState::Alliance
			: Diplomacy->GetDiplomacyState(Report.RequestingFaction, Other);
		return true;
	};
	Report.bOwnerRelationshipKnown = ReadRelationship(Report.CurrentOwner, Report.OwnerRelationship);
	const FReplicatedCaptureSummary District = State->GetCaptureSummary(Target.ParentTerritoryTag);
	if (District.TerritoryTag != Target.ParentTerritoryTag
		|| District.HierarchyLevel != ETerritoryHierarchyLevel::District)
	{
		Report.UnavailableReason = NSLOCTEXT("TerritorySituation", "MissingDistrict", "The Place's District is not available in the campaign directory.");
		return Report;
	}
	const FReplicatedCaptureSummary City = State->GetCaptureSummary(District.ParentTerritoryTag);
	Report.bTargetAvailable = Target.Availability == ETerritoryAvailability::Unlocked
		&& District.Availability == ETerritoryAvailability::Unlocked
		&& City.TerritoryTag == District.ParentTerritoryTag
		&& City.HierarchyLevel == ETerritoryHierarchyLevel::City
		&& City.Availability == ETerritoryAvailability::Unlocked;
	const FGameplayTag ScopeTag = Scope == ETerritorySituationScope::District
		? District.TerritoryTag : District.ParentTerritoryTag;
	const TArray<FReplicatedCaptureSummary> Rows = State->GetAllCaptureSummaries();
	ReadPlaceHoldings(Rows, ScopeTag, Report.RequestingFaction, Report);
	Report.bDominantRelationshipKnown = Report.bHoldingsKnown
		&& ReadRelationship(Report.DominantFaction, Report.DominantRelationship);

	// A streamed-out garrison is unknown, not zero defence. Holding counts remain
	// usable through the saved/replicated directory without loading any city cells.
	FTerritorySituationReport DistrictReport;
	TArray<FReplicatedCaptureSummary> DistrictPlaces;
	const UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	const UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	ATerritoryVolume* LoadedTarget = Registry ? Registry->GetTerritoryByTag(Territory) : nullptr;
	bool bCompleteFront = Report.bTargetAvailable && LoadedTarget && LoadedTarget->HasAuthority() && Counter
		&& LoadedTarget->GetOwningFaction() == Target.CurrentOwner
		&& ReadPlaceHoldings(Rows, District.TerritoryTag, Report.RequestingFaction, DistrictReport, &DistrictPlaces);
	if (bCompleteFront)
	{
		TSet<ATerritoryVolume*> ActualFront;
		for (ATerritoryVolume* Defence : TerritoryAssaultTargetPolicy::BuildDefenceFront(LoadedTarget))
		{
			if (Defence && Defence->GetOwningFaction() == Target.CurrentOwner) ActualFront.Add(Defence);
		}
		for (const FReplicatedCaptureSummary& Place : DistrictPlaces)
		{
			if (Place.CurrentOwner != Target.CurrentOwner) continue;
			ATerritoryVolume* Loaded = Registry->GetTerritoryByTag(Place.TerritoryTag);
			if (!Loaded || Loaded->GetOwningFaction() != Place.CurrentOwner
				|| (Place.TerritoryGUID.IsValid() && Loaded->GetTerritoryGUID() != Place.TerritoryGUID)
				|| ActualFront.Remove(Loaded) != 1)
			{
				bCompleteFront = false;
				break;
			}
		}
		bCompleteFront &= ActualFront.IsEmpty();
	}
	Report.bDefencePowerKnown = bCompleteFront
		&& Counter->TryGetDefenceFrontPower(LoadedTarget, Report.DistrictDefencePower);
	if (!Report.bHoldingsKnown)
		Report.UnavailableReason = NSLOCTEXT("TerritorySituation", "IncompleteHoldings", "The area's Place directory is incomplete or ambiguous.");
	else if (!Report.bDefencePowerKnown)
		Report.UnavailableReason = NSLOCTEXT("TerritorySituation", "UnknownDefence", "Current District defence requires its relevant Place actors on the server; unloaded defence is not treated as zero.");
	return Report;
}

#if WITH_EDITOR
EDataValidationResult UTerritorySituationProfile::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	if (!Territory.IsValid()) Context.AddError(NSLOCTEXT("TerritorySituation", "InvalidPlace", "Choose the Place's Territory tag."));
	if (FactionSource == ETerritoryCaptureFactionSource::ExplicitFaction && !ExplicitFaction.IsValid())
		Context.AddError(NSLOCTEXT("TerritorySituation", "InvalidFaction", "Choose an explicit Narrative faction or use the Narrative participant's faction."));
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

UTerritorySituationCondition::UTerritorySituationCondition()
{
	ConditionFilter = EConditionFilter::CF_DontTarget;
}

bool UTerritorySituationCondition::MatchesReport(const FTerritorySituationReport& Report) const
{
	if (!Report.bContextKnown) return false;
	const auto Compare = [&](float Actual)
	{
		return FMath::IsFinite(Actual) && FMath::IsFinite(Value) && Value >= 0.f
			&& UTerritoryControlProgressCondition::CompareValues(Actual, Comparison, Value, 0.001f);
	};
	switch (Query)
	{
	case ETerritorySituationQuery::ContextKnown: return true;
	case ETerritorySituationQuery::TargetAvailable: return Report.bTargetAvailable;
	case ETerritorySituationQuery::PreviouslyOwned: return Report.bPreviouslyOwned;
	case ETerritorySituationQuery::RetakeNeeded: return Report.bRetakeNeeded;
	case ETerritorySituationQuery::AlreadyOwned: return Report.CurrentOwner == Report.RequestingFaction;
	case ETerritorySituationQuery::HoldingsKnown: return Report.bHoldingsKnown;
	case ETerritorySituationQuery::FactionPlaceCount: return Report.bHoldingsKnown && Compare(Report.FactionPlaces);
	case ETerritorySituationQuery::FactionPlaceShare: return Report.bHoldingsKnown && Compare(Report.FactionSharePercent);
	case ETerritorySituationQuery::FactionDominant: return Report.bHoldingsKnown && Report.DominantFaction == Report.RequestingFaction;
	case ETerritorySituationQuery::NoDominantFaction: return Report.bHoldingsKnown && !Report.DominantFaction.IsValid();
	case ETerritorySituationQuery::RelationshipWithOwner: return Report.bOwnerRelationshipKnown && Report.OwnerRelationship == Relationship;
	case ETerritorySituationQuery::RelationshipWithDominant: return Report.bDominantRelationshipKnown && Report.DominantRelationship == Relationship;
	case ETerritorySituationQuery::DefencePowerKnown: return Report.bDefencePowerKnown;
	case ETerritorySituationQuery::DistrictDefencePower: return Report.bDefencePowerKnown && Compare(Report.DistrictDefencePower);
	default: return false;
	}
}

bool UTerritorySituationCondition::CheckCondition_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	return Profile && MatchesReport(Profile->InspectSituation(Target, Controller, NarrativeComponent, Scope));
}

FString UTerritorySituationCondition::GetGraphDisplayText_Implementation()
{
	const FString QueryName = StaticEnum<ETerritorySituationQuery>()->GetDisplayNameTextByValue(static_cast<int64>(Query)).ToString();
	FString Result = FString::Printf(TEXT("%s: %s (%s)"), Profile ? *Profile->Territory.ToString() : TEXT("Missing situation profile"),
		*QueryName, *StaticEnum<ETerritorySituationScope>()->GetDisplayNameTextByValue(static_cast<int64>(Scope)).ToString());
	if (Query == ETerritorySituationQuery::FactionPlaceCount || Query == ETerritorySituationQuery::FactionPlaceShare
		|| Query == ETerritorySituationQuery::DistrictDefencePower)
		Result += FString::Printf(TEXT(" %s %.3g"), *StaticEnum<ETerritoryFloatComparison>()->GetDisplayNameTextByValue(static_cast<int64>(Comparison)).ToString(), Value);
	if (Query == ETerritorySituationQuery::RelationshipWithOwner || Query == ETerritorySituationQuery::RelationshipWithDominant)
		Result += TEXT(" = ") + StaticEnum<EDiplomacyState>()->GetDisplayNameTextByValue(static_cast<int64>(Relationship)).ToString();
	return Result;
}
