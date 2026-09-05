#include "Core/TerritoryDefinition.h"

#include "AI/TerritoryPatrolGoal.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"

namespace
{
	bool EnsureCompleteStateConfigs(UTerritoryDefinition* Definition)
	{
		if (!Definition) return false;
		bool bChanged = false;
		for (const ETerritoryState State : {
			ETerritoryState::Locked,
			ETerritoryState::Unclaimed,
			ETerritoryState::Contested,
			ETerritoryState::Claimed })
		{
			if (!Definition->StateConfigs.Contains(State))
			{
				Definition->StateConfigs.Add(State, FTerritoryStateConfig());
				bChanged = true;
			}
		}
		return bChanged;
	}

	bool NormalizeAggregateDefinition(UTerritoryDefinition* Definition)
	{
		if (!Definition || Definition->IsA<UTerritoryPlaceDefinition>()) return false;

		const FTerritoryCapturePointTemplate DefaultCapturePoint;
		const FTerritoryGuardBehaviorTemplate DefaultGuardBehavior;
		const bool bChanged = Definition->InitialOwningFaction.IsValid()
			|| Definition->InitialState != ETerritoryInitialState::Automatic
			|| Definition->ControlMode != ETerritoryControlMode::AggregateOnly
			|| Definition->MaxConcurrentAttackers != 1
			|| Definition->PeriodicIncome != 0
			|| Definition->GuardUpkeepPerCycle != 0
			|| Definition->GuardRecruitmentCost != 0
			|| Definition->DefaultStealthProfile != nullptr
			|| Definition->bStoryCaptureFromBounds
			|| Definition->CapturePoint.bEnabled
			|| !Definition->CapturePoint.ActorClass.IsNull()
			|| !Definition->CapturePoint.RelativeTransform.Equals(
				DefaultCapturePoint.RelativeTransform)
			|| !FMath::IsNearlyEqual(Definition->CapturePoint.CaptureRadius,
				DefaultCapturePoint.CaptureRadius)
			|| Definition->CapturePoint.bAutomaticCapture
				!= DefaultCapturePoint.bAutomaticCapture
			|| Definition->CapturePoint.bHideWhileUnavailable
				!= DefaultCapturePoint.bHideWhileUnavailable
			|| Definition->DefaultGuardDefinition != nullptr
			|| !Definition->FactionGuardDefinitions.IsEmpty()
			|| Definition->InitialGuardCount != 0
			|| Definition->PostCaptureGarrisonPolicy
				!= ETerritoryPostCaptureGarrisonPolicy::AlwaysUnstaffed
			|| Definition->GuardBehavior.PatrolGoalClass
				!= DefaultGuardBehavior.PatrolGoalClass
			|| Definition->GuardBehavior.bEnablePatrolCrowdAvoidance
				!= DefaultGuardBehavior.bEnablePatrolCrowdAvoidance
			|| !FMath::IsNearlyEqual(
				Definition->GuardBehavior.PatrolAvoidanceConsiderationRadius,
				DefaultGuardBehavior.PatrolAvoidanceConsiderationRadius)
			|| !FMath::IsNearlyEqual(Definition->GuardBehavior.PatrolAvoidanceWeight,
				DefaultGuardBehavior.PatrolAvoidanceWeight)
			|| Definition->GuardBehavior.bPrioritizeClosestHostilePlayer
				!= DefaultGuardBehavior.bPrioritizeClosestHostilePlayer
			|| !FMath::IsNearlyEqual(
				Definition->GuardBehavior.ClosestHostilePlayerGoalScoreBonus,
				DefaultGuardBehavior.ClosestHostilePlayerGoalScoreBonus)
			|| Definition->GuardBehavior.DialogueProfile != nullptr
			|| !Definition->GuardBehavior.FactionDialogueProfiles.IsEmpty()
			|| !Definition->GuardPosts.IsEmpty()
			|| !Definition->DefenderDiedEvents.IsEmpty()
			|| !Definition->AllDefendersDefeatedEvents.IsEmpty()
			|| Definition->CounterAttackProfile != nullptr
			|| !Definition->CounterAttackApproaches.IsEmpty()
			|| !FMath::IsNearlyZero(Definition->GuardQuality)
			|| !FMath::IsNearlyZero(Definition->FortificationStrength)
			|| !FMath::IsNearlyZero(Definition->NearbyAlliedSupport);

		Definition->InitialOwningFaction = FGameplayTag();
		Definition->InitialState = ETerritoryInitialState::Automatic;
		Definition->ControlMode = ETerritoryControlMode::AggregateOnly;
		Definition->MaxConcurrentAttackers = 1;
		Definition->PeriodicIncome = 0;
		Definition->GuardUpkeepPerCycle = 0;
		Definition->GuardRecruitmentCost = 0;
		Definition->DefaultStealthProfile = nullptr;
		Definition->bStoryCaptureFromBounds = false;
		Definition->CapturePoint = FTerritoryCapturePointTemplate();
		Definition->DefaultGuardDefinition = nullptr;
		Definition->FactionGuardDefinitions.Reset();
		Definition->InitialGuardCount = 0;
		Definition->PostCaptureGarrisonPolicy =
			ETerritoryPostCaptureGarrisonPolicy::AlwaysUnstaffed;
		Definition->GuardBehavior = FTerritoryGuardBehaviorTemplate();
		Definition->GuardPosts.Reset();
		Definition->DefenderDiedEvents.Reset();
		Definition->AllDefendersDefeatedEvents.Reset();
		Definition->CounterAttackProfile = nullptr;
		Definition->CounterAttackApproaches.Reset();
		Definition->GuardQuality = 0.f;
		Definition->FortificationStrength = 0.f;
		Definition->NearbyAlliedSupport = 0.f;
		return bChanged;
	}
}

UTerritoryDefinition::UTerritoryDefinition()
{
	EnsureCompleteStateConfigs(this);
}

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
	Territory->InitialAvailability = InitialState == ETerritoryInitialState::Locked
		? ETerritoryAvailability::Locked : InitialAvailability;
	Territory->InitialState = InitialState;
	Territory->ParentTerritoryTag = DerivedParentTerritoryTag;
	Territory->StrategicValue = FMath::Max(0.f, StrategicValue);

	const bool bPhysicalPlace = IsA<UTerritoryPlaceDefinition>();
	Territory->ControlMode = bPhysicalPlace
		? ETerritoryControlMode::Independent : ETerritoryControlMode::AggregateOnly;
	Territory->InitialMaxConcurrentAttackers = bPhysicalPlace
		? FMath::Max(1, MaxConcurrentAttackers) : 1;
	Territory->InitialPeriodicIncome = bPhysicalPlace ? FMath::Max(0, PeriodicIncome) : 0;
	Territory->InitialGuardCost = bPhysicalPlace ? FMath::Max(0, GuardUpkeepPerCycle) : 0;
	Territory->InitialGuardRecruitmentCost = bPhysicalPlace
		? FMath::Max(0, GuardRecruitmentCost) : 0;
	Territory->bStoryCaptureFromBounds = bPhysicalPlace && bStoryCaptureFromBounds;
	Territory->GuardNPCDefinition = bPhysicalPlace ? DefaultGuardDefinition : nullptr;
	Territory->FactionGuardDefinitions = bPhysicalPlace
		? FactionGuardDefinitions : TArray<FTerritoryFactionGuardDefinition>();
	Territory->GuardSpawnCount = bPhysicalPlace ? FMath::Max(0, InitialGuardCount) : 0;
	Territory->PostCaptureGarrisonPolicy = bPhysicalPlace
		? PostCaptureGarrisonPolicy : ETerritoryPostCaptureGarrisonPolicy::AlwaysUnstaffed;
	Territory->CounterAttackProfile = bPhysicalPlace ? CounterAttackProfile : nullptr;
	Territory->CounterAttackApproaches = bPhysicalPlace
		? CounterAttackApproaches : TArray<FTerritoryAssaultApproach>();
	Territory->GuardQuality = bPhysicalPlace ? FMath::Max(0.f, GuardQuality) : 0.f;
	Territory->FortificationStrength = bPhysicalPlace
		? FMath::Max(0.f, FortificationStrength) : 0.f;
	Territory->NearbyAlliedSupport = bPhysicalPlace
		? FMath::Max(0.f, NearbyAlliedSupport) : 0.f;

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

void UTerritoryDefinition::PostLoad()
{
	Super::PostLoad();
	bool bMigrated = EnsureCompleteStateConfigs(this);
	if (InitialState == ETerritoryInitialState::Locked)
	{
		// One-time in-memory migration for definitions saved before availability and
		// political control were separated. Keep the same new-campaign behavior.
		InitialAvailability = ETerritoryAvailability::Locked;
		InitialState = InitialOwningFaction.IsValid()
			? ETerritoryInitialState::Claimed : ETerritoryInitialState::Unclaimed;
		bMigrated = true;
	}

	if (IsA<UTerritoryPlaceDefinition>())
	{
		if (ControlMode != ETerritoryControlMode::Independent)
		{
			ControlMode = ETerritoryControlMode::Independent;
			bMigrated = true;
		}
	}
	else
	{
		bMigrated |= NormalizeAggregateDefinition(this);
	}
#if WITH_EDITOR
	if (bMigrated && !IsRunningCookCommandlet())
	{
		MarkPackageDirty();
	}
#endif
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
	EnsureCompleteStateConfigs(this);
	// A Place must have one capture authority. Story bounds cover multi-floor
	// missions; physical automatic progress covers domination/multiplayer. Keep
	// authored assets unambiguous instead of allowing a checked option that the
	// runtime then has to ignore.
	if (IsA<UTerritoryPlaceDefinition>() && bStoryCaptureFromBounds
		&& CapturePoint.bAutomaticCapture)
	{
		CapturePoint.bAutomaticCapture = false;
	}
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
	NormalizeAggregateDefinition(this);
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
	// A City is a broad ambient region. Keep the compact capture/location card for
	// its more specific Districts and Places unless a designer deliberately opts in.
	bShowGameplayHUD = false;
	NormalizeAggregateDefinition(this);
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
