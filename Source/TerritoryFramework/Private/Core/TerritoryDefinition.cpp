#include "Core/TerritoryDefinition.h"

#include "AI/TerritoryPatrolGoal.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"

FTerritoryGuardBehaviorTemplate::FTerritoryGuardBehaviorTemplate()
	: PatrolGoalClass(UTerritoryPatrolGoal::StaticClass())
{
}

bool UTerritoryDefinition::ApplyToTerritory(ATerritoryVolume* Territory) const
{
	if (!Territory || !IsDefinitionCompatible(Territory))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Territory Definition %s is not compatible with actor %s"),
			*GetPathName(), *GetNameSafe(Territory));
		return false;
	}

	if (!TerritoryTag.IsValid())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Territory Definition %s has no Territory Tag"), *GetPathName());
		return false;
	}
	Territory->TerritoryDefinition = const_cast<UTerritoryDefinition*>(this);

	Territory->TerritoryTag = TerritoryTag;
	Territory->TerritoryDisplayName = DisplayName;
	Territory->InitialOwningFaction = InitialOwningFaction;
	Territory->InitialState = InitialState;
	Territory->ControlMode = ControlMode;
	Territory->InitialMaxConcurrentAttackers = FMath::Max(1, MaxConcurrentAttackers);
	Territory->InitialPeriodicIncome = FMath::Max(0, PeriodicIncome);
	Territory->InitialGuardCost = FMath::Max(0, GuardUpkeepPerCycle);
	Territory->InitialGuardRecruitmentCost = FMath::Max(0, GuardRecruitmentCost);
	Territory->ParentTerritoryTag = DerivedParentTerritoryTag;
	Territory->bStoryCaptureFromBounds = bStoryCaptureFromBounds;
	Territory->GuardNPCDefinition = DefaultGuardDefinition;
	Territory->FactionGuardDefinitions = FactionGuardDefinitions;
	Territory->GuardSpawnCount = FMath::Max(0, InitialGuardCount);
	Territory->PostCaptureGarrisonPolicy = PostCaptureGarrisonPolicy;
	Territory->CounterAttackProfile = CounterAttackProfile;
	Territory->CounterAttackApproaches = CounterAttackApproaches;
	Territory->GuardQuality = FMath::Max(0.f, GuardQuality);
	Territory->FortificationStrength = FMath::Max(0.f, FortificationStrength);
	Territory->NearbyAlliedSupport = FMath::Max(0.f, NearbyAlliedSupport);
	Territory->StrategicValue = FMath::Max(0.f, StrategicValue);

	if (StableTerritoryGUID.IsValid())
	{
		Territory->TerritoryGUID = StableTerritoryGUID;
	}

	if (const UTerritoryPlaceDefinition* Place = Cast<UTerritoryPlaceDefinition>(this))
	{
		if (ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
		{
			Property->ProductionProfile = Place->ProductionProfile;
			Property->MaxUpgradeLevel = FMath::Max(0, Place->MaxUpgradeLevel);
			Property->UpgradeCostPerLevel = FMath::Max(0, Place->UpgradeCostPerLevel);
			Property->IncomeBonusPerLevel = FMath::Max(0, Place->IncomeBonusPerLevel);
		}
	}
	else if (const UTerritoryDistrictDefinition* District =
		Cast<UTerritoryDistrictDefinition>(this))
	{
		if (ATerritoryDistrict* DistrictActor = Cast<ATerritoryDistrict>(Territory))
		{
			DistrictActor->bIsCapital = District->bIsCapital;
			DistrictActor->CapitalIncomeMultiplier =
				FMath::Max(1.f, District->CapitalIncomeMultiplier);
		}
	}

#if WITH_EDITOR
	Territory->EnsureCounterAttackApproachIDs();
#endif
	Territory->RebuildRuntimeNarrativeConfiguration(*this);
	return true;
}

const FTerritoryGuardPostTemplate* UTerritoryDefinition::FindGuardPost(
	FName GuardPostID) const
{
	if (GuardPostID.IsNone()) return nullptr;
	return GuardPosts.FindByPredicate([GuardPostID](const FTerritoryGuardPostTemplate& Post)
	{
		return Post.GuardPostID == GuardPostID;
	});
}

bool UTerritoryDefinition::GetGuardPostTemplate(FName GuardPostID,
	FTerritoryGuardPostTemplate& OutGuardPost) const
{
	if (const FTerritoryGuardPostTemplate* Post = FindGuardPost(GuardPostID))
	{
		OutGuardPost = *Post;
		return true;
	}
	return false;
}

void UTerritoryDefinition::RefreshHierarchyLinks()
{
#if WITH_EDITOR
	for (int32 Index = 0; Index < GuardPosts.Num(); ++Index)
	{
		FTerritoryGuardPostTemplate& Post = GuardPosts[Index];
		if (Post.GuardPostID.IsNone())
		{
			Post.GuardPostID = FName(*FString::Printf(TEXT("GuardPost_%02d"), Index + 1));
		}
		if (!Post.StableGuardPostGUID.IsValid())
		{
			Post.StableGuardPostGUID = FGuid::NewGuid();
		}
	}
#endif
}

bool UTerritoryDefinition::IsDefinitionCompatible(
	const ATerritoryVolume* Territory) const
{
	return Territory != nullptr;
}

void UTerritoryDefinition::SetDerivedParentTag(const FGameplayTag& ParentTag)
{
	if (DerivedParentTerritoryTag == ParentTag) return;
#if WITH_EDITOR
	Modify();
#endif
	DerivedParentTerritoryTag = ParentTag;
#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

#if WITH_EDITOR
void UTerritoryDefinition::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
		&& GetOutermost() != GetTransientPackage()
		&& !StableTerritoryGUID.IsValid())
	{
		StableTerritoryGUID = FGuid::NewGuid();
	}
}

void UTerritoryDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		StableTerritoryGUID = FGuid::NewGuid();
		for (FTerritoryGuardPostTemplate& Post : GuardPosts)
		{
			Post.StableGuardPostGUID = FGuid::NewGuid();
		}
		DerivedParentTerritoryTag = FGameplayTag();
		MarkPackageDirty();
	}
}

void UTerritoryDefinition::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!StableTerritoryGUID.IsValid())
	{
		StableTerritoryGUID = FGuid::NewGuid();
	}
	RefreshHierarchyLinks();
}
#endif

UTerritoryPlaceDefinition::UTerritoryPlaceDefinition()
{
	ControlMode = ETerritoryControlMode::Independent;
}

FPrimaryAssetId UTerritoryPlaceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryPlace"), GetFName());
}

bool UTerritoryPlaceDefinition::IsDefinitionCompatible(
	const ATerritoryVolume* Territory) const
{
	return Territory && Territory->IsA<ATerritoryProperty>();
}

UTerritoryDistrictDefinition::UTerritoryDistrictDefinition()
{
	ControlMode = ETerritoryControlMode::AggregateOnly;
}

FPrimaryAssetId UTerritoryDistrictDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryDistrict"), GetFName());
}

void UTerritoryDistrictDefinition::RefreshHierarchyLinks()
{
	Super::RefreshHierarchyLinks();
	for (UTerritoryPlaceDefinition* Place : Places)
	{
		if (Place) Place->SetDerivedParentTag(TerritoryTag);
	}
}

bool UTerritoryDistrictDefinition::IsDefinitionCompatible(
	const ATerritoryVolume* Territory) const
{
	return Territory && Territory->IsA<ATerritoryDistrict>()
		&& !Territory->IsA<ATerritoryCity>();
}

UTerritoryCityDefinition::UTerritoryCityDefinition()
{
	ControlMode = ETerritoryControlMode::AggregateOnly;
}

FPrimaryAssetId UTerritoryCityDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryCity"), GetFName());
}

void UTerritoryCityDefinition::RefreshHierarchyLinks()
{
	Super::RefreshHierarchyLinks();
	for (UTerritoryDistrictDefinition* District : Districts)
	{
		if (!District) continue;
		District->SetDerivedParentTag(TerritoryTag);
		District->RefreshHierarchyLinks();
	}
}

bool UTerritoryCityDefinition::IsDefinitionCompatible(
	const ATerritoryVolume* Territory) const
{
	return Territory && Territory->IsA<ATerritoryCity>();
}
