#include "Core/TerritoryDefinition.h"

#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryVolume.h"
#include "EngineUtils.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Spawners/NPCSpawnComponent.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/TerritoryOwnerHandoverEvent.h"

namespace
{
	template<typename TObjectType>
	TArray<TObjectPtr<TObjectType>> DuplicateInstancedArray(
		const TArray<TObjectPtr<TObjectType>>& Source, UObject* NewOuter)
	{
		TArray<TObjectPtr<TObjectType>> Result;
		Result.Reserve(Source.Num());
		for (TObjectType* Item : Source)
		{
			Result.Add(Item ? DuplicateObject<TObjectType>(Item, NewOuter) : nullptr);
		}
		return Result;
	}

	FTerritoryStateConfig DuplicateStateConfig(
		const FTerritoryStateConfig& Source, UObject* NewOuter)
	{
		FTerritoryStateConfig Result;
		Result.GrantedCommandCapabilities = Source.GrantedCommandCapabilities;
		Result.EntryConditions = DuplicateInstancedArray(Source.EntryConditions, NewOuter);
		Result.ExitConditions = DuplicateInstancedArray(Source.ExitConditions, NewOuter);
		Result.EntryEvents = DuplicateInstancedArray(Source.EntryEvents, NewOuter);
		Result.ExitEvents = DuplicateInstancedArray(Source.ExitEvents, NewOuter);
		return Result;
	}

	void RemoveLevelActorReferences(
		TArray<TObjectPtr<UNarrativeEvent>>& Events)
	{
		for (UNarrativeEvent* Event : Events)
		{
			if (UTerritoryOwnerHandoverEvent* Handover =
				Cast<UTerritoryOwnerHandoverEvent>(Event))
			{
				if (!Handover->OwnerTerritoryTag.IsValid()
					&& IsValid(Handover->OwnerSpawner))
				{
					Handover->OwnerTerritoryTag = Handover->OwnerSpawner->TerritoryTag;
				}
				Handover->OwnerSpawner = nullptr;
			}
		}
	}
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
	return true;
}

bool UTerritoryDefinition::CopyCapturePointFromActor(
	ATerritoryCapturePoint* CaptureActor,
	const ATerritoryVolume* RelativeToTerritory)
{
#if WITH_EDITOR
	if (!IsValid(CaptureActor) || !IsValid(RelativeToTerritory)
		|| RelativeToTerritory->GetTerritoryTag() != TerritoryTag
		|| !IsA<UTerritoryPlaceDefinition>())
	{
		return false;
	}
	Modify();
	CapturePoint.bEnabled = true;
	CapturePoint.ActorClass = CaptureActor->GetClass();
	CapturePoint.RelativeTransform = CaptureActor->GetActorTransform()
		.GetRelativeTransform(RelativeToTerritory->GetActorTransform());
	CapturePoint.CaptureRadius = CaptureActor->CaptureRadius;
	CapturePoint.bAutomaticCapture = CaptureActor->bCaptureEnabled;
	CapturePoint.bHideWhileUnavailable =
		CaptureActor->bHideMarkerWhileCaptureUnavailable;
	MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UTerritoryDefinition::CopyManagementPointFromActor(
	ATerritoryDistrictManagementPoint* ManagementActor,
	const ATerritoryVolume* RelativeToTerritory)
{
#if WITH_EDITOR
	if (!IsValid(ManagementActor) || !IsValid(RelativeToTerritory)
		|| RelativeToTerritory->GetTerritoryTag() != TerritoryTag)
	{
		return false;
	}
	Modify();
	ManagementPoint.bEnabled = true;
	ManagementPoint.ActorClass = ManagementActor->GetClass();
	ManagementPoint.RelativeTransform = ManagementActor->GetActorTransform()
		.GetRelativeTransform(RelativeToTerritory->GetActorTransform());
	ManagementPoint.ManagedDistrictOverride = ManagementActor->DistrictTag;
	ManagementPoint.WidgetClass = ManagementActor->ManagementWidgetClass;
	ManagementPoint.WidgetLayer = ManagementActor->ManagementLayerTag;
	ManagementPoint.InteractionDistance = ManagementActor->ManagementDistance;
	MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UTerritoryDefinition::CopyStoryOwnerFromActor(
	ATerritoryStoryOwnerSpawner* OwnerActor,
	const ATerritoryVolume* RelativeToTerritory)
{
#if WITH_EDITOR
	UTerritoryPlaceDefinition* Place = Cast<UTerritoryPlaceDefinition>(this);
	if (!Place || !IsValid(OwnerActor) || !IsValid(RelativeToTerritory)
		|| RelativeToTerritory->GetTerritoryTag() != TerritoryTag)
	{
		return false;
	}
	Modify();
	Place->StoryOwner.bEnabled = true;
	Place->StoryOwner.ActorClass = OwnerActor->GetClass();
	Place->StoryOwner.RelativeTransform = OwnerActor->GetActorTransform()
		.GetRelativeTransform(RelativeToTerritory->GetActorTransform());
	Place->StoryOwner.NPCDefinition = OwnerActor->OwnerSpawn
		? OwnerActor->OwnerSpawn->NPCToSpawn : nullptr;
	Place->StoryOwner.bBeginDialogueOnActivation =
		OwnerActor->bBeginDialogueOnActivation;
	Place->StoryOwner.DialogueOverride = OwnerActor->OverrideDialogue;
	Place->StoryOwner.DialogueStartFromID = OwnerActor->DialogueStartFromID;
	Place->StoryOwner.InteractionDistance = OwnerActor->OwnerInteractionDistance;
	MarkPackageDirty();
	return true;
#else
	return false;
#endif
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
bool UTerritoryDefinition::CopyFromTerritory(const ATerritoryVolume* Territory)
{
	if (!Territory || !IsDefinitionCompatible(Territory)) return false;

	Modify();
	TerritoryTag = Territory->TerritoryTag;
	DisplayName = Territory->TerritoryDisplayName;
	StableTerritoryGUID = Territory->TerritoryGUID;
	RelativeTransform = Territory->GetActorTransform();
	DerivedParentTerritoryTag = Territory->ParentTerritoryTag;
	if (UWorld* World = Territory->GetWorld();
		World && DerivedParentTerritoryTag.IsValid())
	{
		for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
		{
			if (*It != Territory
				&& It->GetTerritoryTag() == DerivedParentTerritoryTag)
			{
				RelativeTransform = Territory->GetActorTransform()
					.GetRelativeTransform(It->GetActorTransform());
				break;
			}
		}
	}
	TerritoryActorClass = Territory->GetClass();
	InitialOwningFaction = Territory->InitialOwningFaction;
	InitialState = Territory->InitialState;
	ControlMode = Territory->ControlMode;
	MaxConcurrentAttackers = Territory->InitialMaxConcurrentAttackers;
	PeriodicIncome = Territory->InitialPeriodicIncome;
	GuardUpkeepPerCycle = Territory->InitialGuardCost;
	GuardRecruitmentCost = Territory->InitialGuardRecruitmentCost;
	StateConfigs.Reset();
	for (const TPair<ETerritoryState, FTerritoryStateConfig>& Pair :
		Territory->StateConfigs)
	{
		FTerritoryStateConfig& Copy = StateConfigs.Add(Pair.Key,
			DuplicateStateConfig(Pair.Value, this));
		RemoveLevelActorReferences(Copy.EntryEvents);
		RemoveLevelActorReferences(Copy.ExitEvents);
	}
	bStoryCaptureFromBounds = Territory->bStoryCaptureFromBounds;
	DefaultGuardDefinition = Territory->GuardNPCDefinition;
	FactionGuardDefinitions = Territory->FactionGuardDefinitions;
	InitialGuardCount = Territory->GuardSpawnCount;
	PostCaptureGarrisonPolicy = Territory->PostCaptureGarrisonPolicy;
	DefenderDiedEvents = DuplicateInstancedArray(
		Territory->DefenderDiedEvents, this);
	AllDefendersDefeatedEvents = DuplicateInstancedArray(
		Territory->AllDefendersDefeatedEvents, this);
	RemoveLevelActorReferences(DefenderDiedEvents);
	RemoveLevelActorReferences(AllDefendersDefeatedEvents);
	CounterAttackProfile = Territory->CounterAttackProfile;
	CounterAttackApproaches = Territory->CounterAttackApproaches;
	GuardQuality = Territory->GuardQuality;
	FortificationStrength = Territory->FortificationStrength;
	NearbyAlliedSupport = Territory->NearbyAlliedSupport;
	StrategicValue = Territory->StrategicValue;
	GuardPosts.Reset();
	for (ATerritoryGuardSpawnPoint* SpawnPoint : Territory->GetGuardSpawnPoints())
	{
		if (!SpawnPoint) continue;
		FTerritoryGuardPostTemplate& Post = GuardPosts.AddDefaulted_GetRef();
		Post.GuardPostID = SpawnPoint->GuardPostID.IsNone()
			? FName(*SpawnPoint->GetName()) : SpawnPoint->GuardPostID;
		Post.StableGuardPostGUID = SpawnPoint->GetActorGUID_Implementation();
		Post.ActorClass = SpawnPoint->GetClass();
		Post.RelativeTransform = SpawnPoint->GetActorTransform().GetRelativeTransform(
			Territory->GetActorTransform());
		// Migration deliberately flattens the effective post settings into this row.
		// A designer may assign a shared GuardPostDefinition later, but the converted
		// Territory definition remains a complete, standalone authoring source.
		Post.GuardPostDefinition = nullptr;
		Post.NPCDefinitionOverride = SpawnPoint->NPCDefinitionOverride
			? SpawnPoint->NPCDefinitionOverride
			: (SpawnPoint->GuardPostDefinition
				? SpawnPoint->GuardPostDefinition->NPCDefinition : nullptr);
		Post.ActivityConfigurationOverride =
			SpawnPoint->ActivityConfigurationOverride
			? SpawnPoint->ActivityConfigurationOverride
			: (SpawnPoint->GuardPostDefinition
				? SpawnPoint->GuardPostDefinition->ActivityConfiguration : nullptr);
		Post.TriggerSetOverrides = !SpawnPoint->TriggerSetOverrides.IsEmpty()
			? SpawnPoint->TriggerSetOverrides
			: (SpawnPoint->GuardPostDefinition
				? SpawnPoint->GuardPostDefinition->TriggerSetOverrides
				: TArray<TSoftObjectPtr<UTriggerSet>>());
		Post.FactionOverride = SpawnPoint->GetEffectiveFactionOverride();
		Post.Priority = SpawnPoint->Priority;
		Post.ReserveSlots = SpawnPoint->GetEffectiveReserveSlots();
		Post.bAutoSpawnReserves = SpawnPoint->bAutoSpawnReserves;
		Post.ReserveSpawnDelay = SpawnPoint->GetEffectiveReserveSpawnDelay();
		Post.ReserveSpawnRetryInterval =
			SpawnPoint->GetEffectiveReserveRetryInterval();
		Post.ReserveSpawnRadius = SpawnPoint->GetEffectiveReserveRadius();
		Post.ReserveMinimumPlayerDistance =
			SpawnPoint->GetEffectiveMinimumPlayerDistance();
		Post.ReserveSpawnCandidateCount = SpawnPoint->GetEffectiveCandidateCount();
		Post.ReserveCameraAvoidanceRetryLimit =
			SpawnPoint->ReserveCameraAvoidanceRetryLimit;
		Post.ReserveTotalRetryLimit = SpawnPoint->ReserveTotalRetryLimit;
		Post.ReserveOwnershipPolicy = SpawnPoint->ReserveOwnershipPolicy;
		Post.bLoopPatrol = SpawnPoint->GetLoopPatrol();
		for (const FTerritoryPatrolNode& Source : SpawnPoint->GetPatrolRoute())
		{
			FTerritoryGuardPatrolTemplateNode& Node =
				Post.PatrolRoute.AddDefaulted_GetRef();
			const FTransform WorldTransform(Source.Rotation, Source.Location);
			Node.RelativeTransform = WorldTransform.GetRelativeTransform(
				SpawnPoint->GetActorTransform());
			Node.WaitTime = Source.WaitTime;
			Node.ActivityTag = Source.ActivityTag;
		}
	}

	// One migration click also gathers and links the supporting Blueprint actors.
	// Match stable tags/definition references first and use bounds only as the
	// fallback for old, untagged management points.
	if (UWorld* World = Territory->GetWorld())
	{
		if (UTerritoryPlaceDefinition* Place = Cast<UTerritoryPlaceDefinition>(this))
		{
			for (TActorIterator<ATerritoryCapturePoint> It(World); It; ++It)
			{
				if (It->PlaceDefinition == Place
					|| It->TargetTerritoryTag == TerritoryTag)
				{
					CopyCapturePointFromActor(*It, Territory);
					It->Modify();
					It->PlaceDefinition = Place;
					It->ApplyPlaceDefinition();
					break;
				}
			}
			for (TActorIterator<ATerritoryStoryOwnerSpawner> It(World); It; ++It)
			{
				if (It->PlaceDefinition == Place || It->TerritoryTag == TerritoryTag)
				{
					CopyStoryOwnerFromActor(*It, Territory);
					It->Modify();
					It->PlaceDefinition = Place;
					It->ApplyPlaceDefinition();
					break;
				}
			}
		}

		ATerritoryDistrictManagementPoint* BestManagement = nullptr;
		double BestDistanceSquared = TNumericLimits<double>::Max();
		const FGameplayTag ExpectedDistrict = IsA<UTerritoryDistrictDefinition>()
			? TerritoryTag : DerivedParentTerritoryTag;
		for (TActorIterator<ATerritoryDistrictManagementPoint> It(World); It; ++It)
		{
			const bool bExplicit = It->TerritoryDefinition == this;
			const bool bPlaceDefinition = IsA<UTerritoryPlaceDefinition>();
			const bool bTagMatch = bPlaceDefinition && ExpectedDistrict.IsValid()
				&& It->DistrictTag == ExpectedDistrict;
			const bool bContained = bPlaceDefinition
				&& Territory->ContainsPoint(It->GetActorLocation());
			if (!bExplicit && !bTagMatch && !bContained) continue;
			const double DistanceSquared = FVector::DistSquared(
				It->GetActorLocation(), Territory->GetActorLocation());
			if (bExplicit || DistanceSquared < BestDistanceSquared)
			{
				BestManagement = *It;
				BestDistanceSquared = bExplicit ? -1.0 : DistanceSquared;
			}
		}
		if (BestManagement)
		{
			CopyManagementPointFromActor(BestManagement, Territory);
			BestManagement->Modify();
			BestManagement->TerritoryDefinition = this;
			BestManagement->ApplyTerritoryDefinition();
		}

		for (FTerritoryGuardPostTemplate& Post : GuardPosts)
		{
			for (TActorIterator<ATerritoryGuardSpawnPoint> It(World); It; ++It)
			{
				if (It->GetActorGUID_Implementation() != Post.StableGuardPostGUID) continue;
				It->Modify();
				It->TerritoryDefinition = this;
				It->GuardPostID = Post.GuardPostID;
				It->ApplyTerritoryDefinition();
				break;
			}
		}
	}

	if (UTerritoryPlaceDefinition* Place = Cast<UTerritoryPlaceDefinition>(this))
	{
		if (const ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
		{
			Place->ProductionProfile = Property->ProductionProfile;
			Place->MaxUpgradeLevel = Property->MaxUpgradeLevel;
			Place->UpgradeCostPerLevel = Property->UpgradeCostPerLevel;
			Place->IncomeBonusPerLevel = Property->IncomeBonusPerLevel;
		}
	}
	else if (UTerritoryDistrictDefinition* District =
		Cast<UTerritoryDistrictDefinition>(this))
	{
		if (const ATerritoryDistrict* DistrictActor = Cast<ATerritoryDistrict>(Territory))
		{
			District->bIsCapital = DistrictActor->bIsCapital;
			District->CapitalIncomeMultiplier = DistrictActor->CapitalIncomeMultiplier;
		}
	}

	RefreshHierarchyLinks();
	MarkPackageDirty();
	return true;
}

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
