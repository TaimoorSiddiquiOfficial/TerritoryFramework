#include "TerritoryDefinitionEditorLibrary.h"

#include "AI/TerritoryPatrolGoal.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "AI/NPCDefinition.h"
#include "ScopedTransaction.h"

namespace
{
	bool IsDefaultGuardBehavior(const FTerritoryGuardBehaviorTemplate& Behavior)
	{
		const FTerritoryGuardBehaviorTemplate Defaults;
		return Behavior.PatrolGoalClass == Defaults.PatrolGoalClass
			&& Behavior.bEnablePatrolCrowdAvoidance ==
				Defaults.bEnablePatrolCrowdAvoidance
			&& FMath::IsNearlyEqual(Behavior.PatrolAvoidanceConsiderationRadius,
				Defaults.PatrolAvoidanceConsiderationRadius)
			&& FMath::IsNearlyEqual(Behavior.PatrolAvoidanceWeight,
				Defaults.PatrolAvoidanceWeight)
			&& Behavior.bPrioritizeClosestHostilePlayer ==
				Defaults.bPrioritizeClosestHostilePlayer
			&& FMath::IsNearlyEqual(Behavior.ClosestHostilePlayerGoalScoreBonus,
				Defaults.ClosestHostilePlayerGoalScoreBonus)
			&& !Behavior.DialogueProfile
			&& Behavior.FactionDialogueProfiles.IsEmpty();
	}

	const ATerritoryGuardCharacter* GetLegacyGuardCDO(const UNPCDefinition* NPCDefinition)
	{
		UClass* GuardClass = NPCDefinition
			? NPCDefinition->NPCClassPath.LoadSynchronous() : nullptr;
		return GuardClass && GuardClass->IsChildOf(ATerritoryGuardCharacter::StaticClass())
			? GuardClass->GetDefaultObject<ATerritoryGuardCharacter>() : nullptr;
	}

	void AddDialogueMapping(FTerritoryGuardBehaviorTemplate& Behavior,
		const FGameplayTag& Faction,
		UTerritoryDiplomacyDialogueProfile* Profile)
	{
		if (!Faction.IsValid() || !Profile) return;
		if (FTerritoryFactionDialogueProfile* Existing =
			Behavior.FactionDialogueProfiles.FindByPredicate(
				[&Faction](const FTerritoryFactionDialogueProfile& Row)
				{
					return Row.Faction == Faction;
				}))
		{
			Existing->DialogueProfile = Profile;
			return;
		}
		FTerritoryFactionDialogueProfile& Added =
			Behavior.FactionDialogueProfiles.AddDefaulted_GetRef();
		Added.Faction = Faction;
		Added.DialogueProfile = Profile;
	}

	void MigrateLegacyGuardBehavior(UTerritoryDefinition* Definition,
		FTerritoryDefinitionSyncReport& Report)
	{
		if (!Definition || !IsDefaultGuardBehavior(Definition->GuardBehavior)) return;

		FTerritoryGuardBehaviorTemplate Migrated;
		bool bFoundLegacyGuard = false;
		auto MergeDefinition = [&Migrated, &bFoundLegacyGuard](
			const UNPCDefinition* NPCDefinition, const FGameplayTag& ExpectedFaction)
		{
			const ATerritoryGuardCharacter* GuardCDO = GetLegacyGuardCDO(NPCDefinition);
			if (!GuardCDO) return;

			FTerritoryGuardBehaviorTemplate Candidate;
			GuardCDO->CopyLegacyGuardBehavior(Candidate);
			if (!bFoundLegacyGuard)
			{
				Migrated = Candidate;
				bFoundLegacyGuard = true;
				if (ExpectedFaction.IsValid())
				{
					Migrated.DialogueProfile = nullptr;
				}
			}

			for (const FTerritoryFactionDialogueProfile& Mapping :
				Candidate.FactionDialogueProfiles)
			{
				AddDialogueMapping(Migrated, Mapping.Faction, Mapping.DialogueProfile);
			}
			if (ExpectedFaction.IsValid())
			{
				UTerritoryDiplomacyDialogueProfile* ExpectedProfile =
					Candidate.DialogueProfile;
				if (const FTerritoryFactionDialogueProfile* Exact =
					Candidate.FactionDialogueProfiles.FindByPredicate(
						[&ExpectedFaction](const FTerritoryFactionDialogueProfile& Row)
						{
							return Row.Faction == ExpectedFaction;
						}))
				{
					ExpectedProfile = Exact->DialogueProfile;
				}
				AddDialogueMapping(Migrated, ExpectedFaction, ExpectedProfile);
			}
		};

		MergeDefinition(Definition->DefaultGuardDefinition, FGameplayTag());
		for (const FTerritoryFactionGuardDefinition& GuardDefinition :
			Definition->FactionGuardDefinitions)
		{
			MergeDefinition(GuardDefinition.NPCDefinition, GuardDefinition.Faction);
		}
		if (!bFoundLegacyGuard || IsDefaultGuardBehavior(Migrated)) return;

		Definition->Modify();
		Definition->GuardBehavior = Migrated;
		Definition->MarkPackageDirty();
		Report.Warnings.Add(FString::Printf(
			TEXT("%s copied hidden guard Blueprint defaults into Guard Behavior; save this Definition before removing the migration bridge."),
			*Definition->GetPathName()));
	}

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
		MigrateLegacyGuardBehavior(Definition, Report);
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
