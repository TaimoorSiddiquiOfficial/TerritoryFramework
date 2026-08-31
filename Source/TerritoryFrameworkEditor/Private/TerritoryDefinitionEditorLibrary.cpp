#include "TerritoryDefinitionEditorLibrary.h"

#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "GameplayEffect.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "ScopedTransaction.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphDelegates.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"

namespace
{
	template<typename TActor>
	TActor* SpawnTemplateActor(UWorld* World, TSoftClassPtr<TActor> SoftClass,
		const FTransform& Transform, const FString& Label,
		FTerritoryDefinitionSyncReport& Report)
	{
		UClass* ActorClass = SoftClass.LoadSynchronous();
		if (!ActorClass || !ActorClass->IsChildOf(TActor::StaticClass()))
		{
			Report.Errors.Add(FString::Printf(TEXT("%s has no compatible Blueprint class."),
				*Label));
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transactional;
		TActor* Actor = World->SpawnActor<TActor>(ActorClass, Transform, Params);
		if (!Actor)
		{
			Report.Errors.Add(FString::Printf(TEXT("Could not create %s."), *Label));
			return nullptr;
		}
		Actor->SetActorLabel(Label, true);
		++Report.CreatedActors;
		return Actor;
	}

	ATerritoryVolume* SyncTerritory(UWorld* World, UTerritoryDefinition* Definition,
		const FTransform& DesiredTransform, bool bCreate, bool bMoveExisting,
		FTerritoryDefinitionSyncReport& Report)
	{
		ATerritoryVolume* Actor = nullptr;
		bool bCreated = false;
		for (TActorIterator<ATerritoryVolume> It(World); It; ++It)
		{
			if (It->GetTerritoryDefinition() == Definition
				|| It->GetTerritoryTag() == Definition->TerritoryTag)
			{
				Actor = *It;
				break;
			}
		}
		if (!Actor && bCreate)
		{
			Actor = SpawnTemplateActor<ATerritoryVolume>(World,
				Definition->TerritoryActorClass, DesiredTransform,
				Definition->GetName(), Report);
			bCreated = Actor != nullptr;
		}
		if (!Actor)
		{
			Report.Errors.Add(FString::Printf(TEXT("No level actor for %s."),
				*Definition->GetPathName()));
			return nullptr;
		}
		Actor->Modify();
		if (bMoveExisting || bCreated)
		{
			Actor->SetActorTransform(DesiredTransform);
		}
		if (!Definition->ApplyToTerritory(Actor))
		{
			Report.Errors.Add(FString::Printf(TEXT("Definition %s rejected actor %s."),
				*Definition->GetPathName(), *Actor->GetPathName()));
		}
		else
		{
			++Report.UpdatedActors;
		}
		return Actor;
	}

	template<typename TActor, typename TDefinition>
	TActor* FindAuxiliary(UWorld* World, const TDefinition* Definition)
	{
		for (TActorIterator<TActor> It(World); It; ++It)
		{
			if (It->GetPlaceDefinition() == Definition
				|| It->GetTargetTerritoryTag() == Definition->TerritoryTag)
			{
				return *It;
			}
		}
		return nullptr;
	}

	template<>
	ATerritoryStoryOwnerSpawner* FindAuxiliary<ATerritoryStoryOwnerSpawner,
		UTerritoryPlaceDefinition>(UWorld* World,
		const UTerritoryPlaceDefinition* Definition)
	{
		for (TActorIterator<ATerritoryStoryOwnerSpawner> It(World); It; ++It)
		{
			if (It->GetPlaceDefinition() == Definition
				|| It->GetTerritoryTag() == Definition->TerritoryTag)
			{
				return *It;
			}
		}
		return nullptr;
	}

	void SyncPlaceHelpers(UWorld* World, UTerritoryPlaceDefinition* Definition,
		ATerritoryVolume* TerritoryActor, bool bCreate, bool bMoveExisting,
		FTerritoryDefinitionSyncReport& Report)
	{
		if (!TerritoryActor) return;
		const FTransform Parent = TerritoryActor->GetActorTransform();
		if (Definition->CapturePoint.bEnabled)
		{
			ATerritoryCapturePoint* Actor = FindAuxiliary<ATerritoryCapturePoint>(
				World, Definition);
			if (!Actor && bCreate)
			{
				Actor = SpawnTemplateActor<ATerritoryCapturePoint>(World,
					Definition->CapturePoint.ActorClass,
					Definition->CapturePoint.RelativeTransform * Parent,
					Definition->GetName() + TEXT("_CapturePoint"), Report);
			}
			if (Actor)
			{
				Actor->Modify();
				Actor->SetPlaceDefinition(Definition);
				Actor->ApplyPlaceDefinition();
				if (bMoveExisting) Actor->SetActorTransform(
					Definition->CapturePoint.RelativeTransform * Parent);
				++Report.UpdatedActors;
			}
		}

		if (Definition->StoryOwner.bEnabled)
		{
			ATerritoryStoryOwnerSpawner* Actor =
				FindAuxiliary<ATerritoryStoryOwnerSpawner>(World, Definition);
			if (!Actor && bCreate)
			{
				Actor = SpawnTemplateActor<ATerritoryStoryOwnerSpawner>(World,
					Definition->StoryOwner.ActorClass,
					Definition->StoryOwner.RelativeTransform * Parent,
					Definition->GetName() + TEXT("_StoryOwner"), Report);
			}
			if (Actor)
			{
				Actor->Modify();
				Actor->SetPlaceDefinition(Definition);
				Actor->ApplyPlaceDefinition();
				if (bMoveExisting) Actor->SetActorTransform(
					Definition->StoryOwner.RelativeTransform * Parent);
				++Report.UpdatedActors;
			}
		}

		// Guard posts are physical Place infrastructure. City/District definitions
		// are aggregate authority records and must never create them.
		for (const FTerritoryGuardPostTemplate& Post : Definition->GuardPosts)
		{
			ATerritoryGuardSpawnPoint* GuardPost = nullptr;
			for (TActorIterator<ATerritoryGuardSpawnPoint> It(World); It; ++It)
			{
				if ((It->GetTerritoryDefinition() == Definition
					|| It->OwnerTerritoryTag == Definition->TerritoryTag)
					&& (It->GetGuardPostID() == Post.GuardPostID
						|| (It->GetGuardPostID().IsNone()
							&& It->GetName() == Post.GuardPostID.ToString())))
				{
					GuardPost = *It;
					break;
				}
			}
			if (!GuardPost && bCreate)
			{
				GuardPost = SpawnTemplateActor<ATerritoryGuardSpawnPoint>(World,
					Post.ActorClass, Post.RelativeTransform * Parent,
					Definition->GetName() + TEXT("_") + Post.GuardPostID.ToString(), Report);
			}
			if (GuardPost)
			{
				GuardPost->Modify();
				GuardPost->SetDefinitionBinding(Definition, Post.GuardPostID);
				GuardPost->ApplyTerritoryDefinition();
				if (bMoveExisting) GuardPost->SetActorTransform(
					Post.RelativeTransform * Parent);
				++Report.UpdatedActors;
			}
		}
	}

	void SyncCommonHelpers(UWorld* World, UTerritoryDefinition* Definition,
		ATerritoryVolume* TerritoryActor, bool bCreate, bool bMoveExisting,
		FTerritoryDefinitionSyncReport& Report)
	{
		if (!TerritoryActor) return;
		const FTransform Parent = TerritoryActor->GetActorTransform();
		if (Definition->ManagementPoint.bEnabled)
		{
			ATerritoryDistrictManagementPoint* Management = nullptr;
			for (TActorIterator<ATerritoryDistrictManagementPoint> It(World); It; ++It)
			{
				if (It->GetTerritoryDefinition() == Definition
					|| (Definition->ManagementPoint.ManagedDistrictOverride.IsValid()
						&& It->GetManagedDistrictTag() ==
						Definition->ManagementPoint.ManagedDistrictOverride))
				{
					Management = *It;
					break;
				}
			}
			if (!Management && bCreate)
			{
				Management = SpawnTemplateActor<ATerritoryDistrictManagementPoint>(
					World, Definition->ManagementPoint.ActorClass,
					Definition->ManagementPoint.RelativeTransform * Parent,
					Definition->GetName() + TEXT("_Management"), Report);
			}
			if (Management)
			{
				Management->Modify();
				Management->SetTerritoryDefinition(Definition);
				Management->ApplyTerritoryDefinition();
				if (bMoveExisting) Management->SetActorTransform(
					Definition->ManagementPoint.RelativeTransform * Parent);
				++Report.UpdatedActors;
			}
		}
	}
}

bool UTerritoryDefinitionEditorLibrary::AlignAttackDamageEffectWithNarrativePro(
	TSubclassOf<UGameplayEffect> GameplayEffectClass, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	UGameplayEffect* Effect = GameplayEffectClass
		? GameplayEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!Effect)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "MissingAdaptiveDamageEffect",
			"Select a Gameplay Effect class.");
		return false;
	}

	FGameplayModifierInfo* AttackDamageModifier = Effect->Modifiers.FindByPredicate(
		[](const FGameplayModifierInfo& Modifier)
		{
			return Modifier.Attribute == UNarrativeAttributeSetBase::GetAttackDamageAttribute()
				&& Modifier.ModifierMagnitude.GetMagnitudeCalculationType()
					== EGameplayEffectMagnitudeCalculation::SetByCaller;
		});
	if (!AttackDamageModifier)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "MissingAdaptiveDamageModifier",
			"The Gameplay Effect needs a Set By Caller modifier for Narrative AttackDamage.");
		return false;
	}

	Effect->Modify();
	FSetByCallerFloat NarrativeAttackDamage;
	NarrativeAttackDamage.DataTag =
		FNarrativeGameplayTags::Get().SetByCaller_AttackDamage;
	AttackDamageModifier->ModifierMagnitude =
		FGameplayEffectModifierMagnitude(NarrativeAttackDamage);
	Effect->MarkPackageDirty();
	return true;
}

bool UTerritoryDefinitionEditorLibrary::EnsureStraightVehicleApproachRoad(
	ATerritoryVolume* Territory, FName ApproachID, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	UWorld* World = Territory ? Territory->GetWorld() : nullptr;
	if (!Territory || !World || World->IsGameWorld())
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadNeedsEditorTerritory",
			"Select a Territory actor in a non-PIE editor world.");
		return false;
	}

	const FTerritoryAssaultApproach* Approach =
		Territory->GetCounterAttackApproaches().FindByPredicate(
			[ApproachID](const FTerritoryAssaultApproach& Candidate)
			{
				return Candidate.ApproachID == ApproachID;
			});
	if (!Approach || !Approach->bEnabled
		|| Approach->EntryType != ETerritoryAssaultEntryType::NarrativeVehicle)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadNeedsApproach",
			"The enabled approach must exist and use Narrative Vehicle entry.");
		return false;
	}

	const UZoneGraphSettings* Settings = GetDefault<UZoneGraphSettings>();
	const FZoneLaneProfile* RoadProfile = Settings
		? Settings->GetLaneProfiles().FindByPredicate(
			[](const FZoneLaneProfile& Profile)
			{
				return Profile.Name == TEXT("Road");
			}) : nullptr;
	if (!RoadProfile)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadNeedsProfile",
			"ZoneGraph Settings needs a lane profile named Road.");
		return false;
	}

	const FString StableTagString = FString::Printf(TEXT("TerritoryVehicleRoad_%s_%s"),
		*Territory->GetTerritoryGUID().ToString(EGuidFormats::Digits),
		*ApproachID.ToString());
	const FName StableTag(*StableTagString);
	AZoneShape* RoadShape = nullptr;
	ATerritoryRoadGuide* RoadGuide = nullptr;
	for (TActorIterator<AZoneShape> It(World); It; ++It)
	{
		if (It->Tags.Contains(StableTag))
		{
			RoadShape = *It;
			break;
		}
	}
	for (TActorIterator<ATerritoryRoadGuide> It(World); It; ++It)
	{
		if (It->Tags.Contains(StableTag))
		{
			RoadGuide = *It;
			break;
		}
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("TerritoryEditor", "EnsureVehicleRoadTransaction",
			"Ensure Territory Vehicle Approach Road"));
	Territory->Modify();
	if (!RoadShape)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.OverrideLevel = Territory->GetLevel();
		SpawnParams.ObjectFlags = RF_Transactional;
		RoadShape = World->SpawnActor<AZoneShape>(AZoneShape::StaticClass(),
			FTransform::Identity, SpawnParams);
		if (!RoadShape)
		{
			OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadSpawnFailed",
				"Could not create the ZoneShape road actor.");
			return false;
		}
		RoadShape->Tags.Add(StableTag);
		RoadShape->SetActorLabel(FString::Printf(TEXT("Vehicle Road - %s - %s"),
			*Territory->GetTerritoryDisplayName().ToString(), *ApproachID.ToString()));
	}
	if (!RoadGuide)
	{
		UClass* RoadGuideClass = nullptr;
		if (const UTerritoryDeveloperSettings* TerritorySettings =
			GetDefault<UTerritoryDeveloperSettings>())
		{
			RoadGuideClass = TerritorySettings->RoadGuideBlueprintClass.LoadSynchronous();
		}
		if (!RoadGuideClass
			|| !RoadGuideClass->IsChildOf(ATerritoryRoadGuide::StaticClass()))
		{
			OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleGuideBlueprintMissing",
				"Set Territory Framework > Road Guide Blueprint Class to a Blueprint child of TerritoryRoadGuide before creating production level roads.");
			return false;
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.OverrideLevel = Territory->GetLevel();
		SpawnParams.ObjectFlags = RF_Transactional;
		RoadGuide = World->SpawnActor<ATerritoryRoadGuide>(
			RoadGuideClass, FTransform::Identity, SpawnParams);
		if (!RoadGuide)
		{
			OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleGuideSpawnFailed",
				"Could not create the Territory spline Road Guide actor.");
			return false;
		}
		RoadGuide->Tags.Add(StableTag);
		RoadGuide->SetActorLabel(FString::Printf(TEXT("Road Guide - %s - %s"),
			*Territory->GetTerritoryDisplayName().ToString(), *ApproachID.ToString()));
	}

	const FTransform TerritoryTransform = Territory->GetActorTransform();
	const FVector WorldSpawn =
		(Approach->RelativeSpawnTransform * TerritoryTransform).GetLocation();
	const FVector WorldDropOff =
		(Approach->RelativeVehicleDropOffTransform * TerritoryTransform).GetLocation();
	if (FVector::DistSquared(WorldSpawn, WorldDropOff) < FMath::Square(100.f))
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadTooShort",
			"Vehicle spawn and drop-off must be at least 100 cm apart.");
		return false;
	}

	RoadShape->Modify();
	RoadShape->SetActorTransform(FTransform(FRotator::ZeroRotator, WorldSpawn));
	UZoneShapeComponent* Shape = const_cast<UZoneShapeComponent*>(RoadShape->GetShape());
	if (!Shape)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadShapeMissing",
			"The ZoneShape actor has no ZoneShape component.");
		return false;
	}

	Shape->Modify();
	Shape->SetShapeType(FZoneShapeType::Spline);
	Shape->SetCommonLaneProfile(FZoneLaneProfileRef(*RoadProfile));
	TArray<FZoneShapePoint>& Points = Shape->GetMutablePoints();
	Points.Reset(2);
	Points.Emplace(FVector::ZeroVector);
	Points.Emplace(RoadShape->GetActorTransform().InverseTransformPosition(WorldDropOff));
	Shape->UpdateShape();
	Shape->MarkPackageDirty();

	RoadGuide->Modify();
	RoadGuide->RoadGuideID = Approach->RoadGuideID.IsNone()
		? Approach->ApproachID : Approach->RoadGuideID;
	RoadGuide->SetStraightRoute(WorldSpawn, WorldDropOff);
	RoadGuide->MarkPackageDirty();

	if (World->GetSubsystem<UZoneGraphSubsystem>())
	{
		UE::ZoneGraphDelegates::OnZoneGraphRequestRebuild.Broadcast();
	}
	else
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadSubsystemMissing",
			"ZoneGraph subsystem is unavailable in this editor world.");
		return false;
	}
	World->MarkPackageDirty();
	return true;
}

bool UTerritoryDefinitionEditorLibrary::EnsureStraightVehicleApproachRoadFromDefinition(
	UWorld* World, UTerritoryDefinition* TerritoryDefinition, FName ApproachID,
	FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	if (!World || !TerritoryDefinition || !World->PersistentLevel)
	{
		OutFailureReason = NSLOCTEXT("TerritoryEditor", "VehicleRoadNeedsDefinitionWorld",
			"Load a map and select a Territory Definition.");
		return false;
	}

	ATerritoryVolume* Territory = nullptr;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		ATerritoryVolume* Candidate = Cast<ATerritoryVolume>(Actor);
		if (Candidate
			&& (Candidate->GetTerritoryDefinition() == TerritoryDefinition
				|| Candidate->GetTerritoryTag() == TerritoryDefinition->TerritoryTag))
		{
			Territory = Candidate;
			break;
		}
	}
	if (!Territory)
	{
		OutFailureReason = FText::Format(
			NSLOCTEXT("TerritoryEditor", "VehicleRoadDefinitionActorMissing",
				"No actor in this map uses {0}."),
			FText::FromString(TerritoryDefinition->GetPathName()));
		return false;
	}

	return EnsureStraightVehicleApproachRoad(
		Territory, ApproachID, OutFailureReason);
}

FTerritoryDefinitionSyncReport
UTerritoryDefinitionEditorLibrary::SynchronizeCityInCurrentLevel(
	UTerritoryCityDefinition* CityDefinition, FTransform NewCityAnchor,
	bool bCreateMissingActors, bool bMoveExistingActorsToDefinitionTransforms)
{
	FTerritoryDefinitionSyncReport Report;
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || World->IsGameWorld() || !IsValid(CityDefinition))
	{
		Report.Errors.Add(TEXT("Open an editor level and provide a valid City definition."));
		return Report;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("TerritoryFramework",
		"SyncTerritoryDefinition", "Synchronize Territory Definitions"));
	CityDefinition->RefreshHierarchyLinks();
	ATerritoryVolume* CityActor = SyncTerritory(World, CityDefinition,
		NewCityAnchor, bCreateMissingActors,
		bMoveExistingActorsToDefinitionTransforms, Report);
	if (!CityActor) return Report;
	SyncCommonHelpers(World, CityDefinition, CityActor, bCreateMissingActors,
		bMoveExistingActorsToDefinitionTransforms, Report);

	for (UTerritoryDistrictDefinition* District : CityDefinition->Districts)
	{
		if (!District) continue;
		const FTransform DistrictTransform =
			District->RelativeTransform * CityActor->GetActorTransform();
		ATerritoryVolume* DistrictActor = SyncTerritory(World, District,
			DistrictTransform, bCreateMissingActors,
			bMoveExistingActorsToDefinitionTransforms, Report);
		SyncCommonHelpers(World, District, DistrictActor, bCreateMissingActors,
			bMoveExistingActorsToDefinitionTransforms, Report);
		for (UTerritoryPlaceDefinition* Place : District->Places)
		{
			if (!Place || !DistrictActor) continue;
			const FTransform PlaceTransform =
				Place->RelativeTransform * DistrictActor->GetActorTransform();
			ATerritoryVolume* PlaceActor = SyncTerritory(World, Place,
				PlaceTransform, bCreateMissingActors,
				bMoveExistingActorsToDefinitionTransforms, Report);
			SyncCommonHelpers(World, Place, PlaceActor, bCreateMissingActors,
				bMoveExistingActorsToDefinitionTransforms, Report);
			SyncPlaceHelpers(World, Place, PlaceActor, bCreateMissingActors,
				bMoveExistingActorsToDefinitionTransforms, Report);
		}
	}

	World->MarkPackageDirty();
	Report.bSucceeded = Report.Errors.IsEmpty();
	return Report;
}
