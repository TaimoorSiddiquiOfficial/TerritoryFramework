#include "DataValidation/TerritoryDataValidator.h"
#include "AI/TerritoryPatrolGoal.h"
#include "AI/TerritoryInvestigationActivity.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritorySavableData.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Economy/TerritoryProductionProfile.h"
#include "UI/TerritoryDistrictManagementWidget.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Music/TaggedMusicSet.h"
#include "AI/NPCDefinition.h"
#include "DialogueBlueprint.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Components/ShapeComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Items/WeaponItem.h"
#include "Misc/DataValidation.h"
#include "NarrativeGameplayTags.h"
#include "NavigationSystem.h"
#include "QuestBlueprint.h"
#include "Tales/Quest.h"
#include "Tales/QuestSM.h"
#include "Tales/QuestTask.h"
#include "Tales/Dialogue.h"
#include "Tales/NarrativeDialogueSequence.h"
#include "Tales/TerritoryDiplomacyCondition.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "Tales/TerritoryStoryEvents.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "Tracks/MovieSceneSpawnTrack.h"

namespace
{
	bool IsTerritoryStoryCaptureAsset(const FAssetData& AssetData)
	{
		return AssetData.PackageName.ToString().StartsWith(
			TEXT("/Game/TerritoryFramework/StoryCapture/"));
	}

	void ValidateDialogueShot(const UNarrativeDialogueSequence* Shot,
		const FString& Context, TSet<const UNarrativeDialogueSequence*>& Visited,
		TArray<FString>& OutErrors)
	{
		if (!Shot || Visited.Contains(Shot)) return;
		Visited.Add(Shot);

		const TArray<ULevelSequence*> Sequences = Shot->GetSequenceAssets();
		if (Sequences.IsEmpty())
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s has no Level Sequence; Narrative cannot create a camera cut"),
				*Context));
			return;
		}
		for (int32 Index = 0; Index < Sequences.Num(); ++Index)
		{
			const ULevelSequence* Sequence = Sequences[Index];
			if (!Sequence)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s contains an empty Level Sequence entry at index %d"),
					*Context, Index));
				continue;
			}
			if (Sequence->FindBindingsByTag(TEXT("Cinecam")).IsEmpty())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s sequence %s has no binding tagged Cinecam; Narrative focus, crop, and Territory lens settings cannot be applied"),
					*Context, *Sequence->GetName()));
			}
			else if (UMovieScene* MovieScene = const_cast<UMovieScene*>(
				Sequence->GetMovieScene()))
			{
				for (const FMovieSceneObjectBindingID& Binding :
					Sequence->FindBindingsByTag(TEXT("Cinecam")))
				{
					const FGuid Guid = Binding.GetGuid();
					const UMovieSceneSpawnTrack* SpawnTrack =
						MovieScene->FindTrack<UMovieSceneSpawnTrack>(Guid);
					if (!MovieScene->FindSpawnable(Guid) || !SpawnTrack
						|| SpawnTrack->GetAllSections().IsEmpty())
					{
						OutErrors.Add(FString::Printf(
							TEXT("%s sequence %s Cinecam must be a spawnable with an explicit non-empty Spawn track; otherwise Narrative can bind a camera cut without creating the camera"),
							*Context, *Sequence->GetName()));
					}
				}
			}
		}
	}

	bool IsTerritoryNarrativeTask(const UNarrativeTask* Task)
	{
		for (const UClass* Class = Task ? Task->GetClass() : nullptr;
			Class; Class = Class->GetSuperClass())
		{
			if (Class->GetOutermost()->GetName() == TEXT("/Script/TerritoryFramework"))
			{
				return true;
			}
		}
		return false;
	}

	bool UsesTerritoryNarrativeTask(const UQuestBlueprint* QuestBlueprint)
	{
		const UQuest* Quest = QuestBlueprint ? QuestBlueprint->QuestTemplate : nullptr;
		if (!Quest) return false;
		for (const UQuestBranch* Branch : Quest->GetBranches())
		{
			if (!Branch) continue;
			for (const UNarrativeTask* Task : Branch->QuestTasks)
			{
				if (IsTerritoryNarrativeTask(Task)) return true;
			}
		}
		return false;
	}

	bool HasNarrativeAttackDamageSetByCaller(
		TSubclassOf<UGameplayEffect> EffectClass)
	{
		const UGameplayEffect* Effect = EffectClass
			? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!Effect) return false;

		return Effect->Modifiers.ContainsByPredicate(
			[](const FGameplayModifierInfo& Modifier)
			{
				return Modifier.Attribute
						== UNarrativeAttributeSetBase::GetAttackDamageAttribute()
					&& Modifier.ModifierMagnitude.GetMagnitudeCalculationType()
						== EGameplayEffectMagnitudeCalculation::SetByCaller
					&& Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag
						== FNarrativeGameplayTags::Get().SetByCaller_AttackDamage;
			});
	}

	void ValidateCounterAttackPowerScaling(
		const FTerritoryFactionAssaultConfig& Force,
		const FString& Context,
		TArray<FString>& OutErrors,
		TArray<FString>& OutWarnings)
	{
		if (!Force.bScaleLevelToRelevantPlayerPower) return;
		const FString FactionName = Force.Faction.IsValid()
			? Force.Faction.ToString() : TEXT("<missing faction>");
		auto Error = [&Context, &OutErrors](const FString& Message)
		{
			OutErrors.Add(Context.IsEmpty() ? Message
				: FString::Printf(TEXT("%s: %s"), *Context, *Message));
		};
		auto Warning = [&Context, &OutWarnings](const FString& Message)
		{
			OutWarnings.Add(Context.IsEmpty() ? Message
				: FString::Printf(TEXT("%s: %s"), *Context, *Message));
		};

		if (Force.MinimumScaledEnemyLevel < 1
			|| Force.MaximumScaledEnemyLevel < Force.MinimumScaledEnemyLevel)
		{
			Error(FString::Printf(TEXT("counterattack force %s adaptive enemy level range is invalid"),
				*FactionName));
		}

		TSet<FGameplayTag> SeenPowerTags;
		for (const FTerritoryPlayerPowerTier& Tier : Force.PlayerPowerTiers)
		{
			if (!Tier.PlayerPowerTag.IsValid())
			{
				Error(FString::Printf(TEXT("counterattack force %s has a power tier without a Gameplay Tag"),
					*FactionName));
				continue;
			}
			if (SeenPowerTags.Contains(Tier.PlayerPowerTag))
			{
				Error(FString::Printf(TEXT("counterattack force %s repeats player power tag %s"),
					*FactionName, *Tier.PlayerPowerTag.ToString()));
			}
			SeenPowerTags.Add(Tier.PlayerPowerTag);
			if (Tier.PlayerPowerLevel < 1)
			{
				Error(FString::Printf(TEXT("counterattack force %s power tag %s must map to level one or higher"),
					*FactionName, *Tier.PlayerPowerTag.ToString()));
			}
		}

		if (!FMath::IsFinite(Force.PowerScalingMagnitudePerEnemyLevel)
			|| Force.PowerScalingMagnitudePerEnemyLevel < 0.f)
		{
			Error(FString::Printf(TEXT("counterattack force %s Attack Damage per level must be finite and non-negative"),
				*FactionName));
		}
		else if (Force.PowerScalingMagnitudePerEnemyLevel > 0.f)
		{
			if (!Force.PowerScalingEffect)
			{
				Error(FString::Printf(TEXT("counterattack force %s has Attack Damage per level but no Power Scaling Effect"),
					*FactionName));
			}
			else if (!HasNarrativeAttackDamageSetByCaller(Force.PowerScalingEffect))
			{
				Error(FString::Printf(TEXT("counterattack force %s Power Scaling Effect must modify Narrative AttackDamage from SetByCaller.AttackDamage"),
					*FactionName));
			}
		}

		if (Force.PowerScalingEffect
			&& FMath::IsNearlyZero(Force.PowerScalingMagnitudePerEnemyLevel))
		{
			Warning(FString::Printf(TEXT("counterattack force %s uses only the Gameplay Effect level curve; verify the effect does not require SetByCaller.AttackDamage"),
				*FactionName));
		}
	}

	void ValidateCounterAttackForceSchedule(
		const FTerritoryFactionAssaultConfig& Force,
		const FString& Context,
		TArray<FString>& OutErrors)
	{
		auto Error = [&Context, &OutErrors](const FString& Message)
		{
			OutErrors.Add(Context.IsEmpty() ? Message
				: FString::Printf(TEXT("%s: %s"), *Context, *Message));
		};
		const FString FactionName = Force.Faction.IsValid()
			? Force.Faction.ToString() : TEXT("<missing faction>");
		if (Force.ScheduleMode != ETerritoryCounterScheduleMode::SingleAssault
			&& (!FMath::IsFinite(Force.RecurringCounterCooldownGameTime)
				|| Force.RecurringCounterCooldownGameTime < 1.f))
		{
			Error(FString::Printf(TEXT("counterattack force %s recurring cooldown must be finite and at least one for a repeating schedule"),
				*FactionName));
		}
		if (Force.ScheduleMode == ETerritoryCounterScheduleMode::FiniteSeries
			&& Force.MaximumScheduledAssaults < 1)
		{
			Error(FString::Printf(TEXT("counterattack force %s finite schedule must allow at least one assault"),
				*FactionName));
		}
		if (Force.TimePolicy == ETerritoryCounterTimePolicy::NarrativeTimeWindow
			&& (!FMath::IsFinite(Force.TimeWindowStart)
				|| !FMath::IsFinite(Force.TimeWindowEnd)
				|| !FMath::IsWithinInclusive(Force.TimeWindowStart, 0.f, 2400.f)
				|| !FMath::IsWithinInclusive(Force.TimeWindowEnd, 0.f, 2400.f)))
		{
			Error(FString::Printf(TEXT("counterattack force %s Narrative time window must use finite values from 0000 through 2400"),
				*FactionName));
		}
	}

	void ValidateCounterAttackFactionVehicles(
		const FTerritoryFactionAssaultConfig& Force,
		const FString& Context, TArray<FString>& OutErrors)
	{
		auto Error = [&Context, &OutErrors](const FString& Message)
		{
			OutErrors.Add(Context.IsEmpty() ? Message
				: FString::Printf(TEXT("%s: %s"), *Context, *Message));
		};
		const FString FactionName = Force.Faction.IsValid()
			? Force.Faction.ToString() : TEXT("<missing faction>");
		if (!Force.SignatureVehicleClass.IsNull())
		{
			UClass* SignatureClass = Force.SignatureVehicleClass.LoadSynchronous();
			if (!SignatureClass
				|| !SignatureClass->IsChildOf(ANarrativeVehicleBase::StaticClass()))
			{
				Error(FString::Printf(TEXT("counterattack force %s signature vehicle must derive from ANarrativeVehicleBase"),
					*FactionName));
			}
		}
		TSet<ENarrativeGameplayDifficulty> SeenDifficulties;
		for (const FTerritoryDifficultyVehicleCount& Entry :
			Force.VehicleCountsByDifficulty)
		{
			if (SeenDifficulties.Contains(Entry.Difficulty))
			{
				Error(FString::Printf(TEXT("counterattack force %s has duplicate car-count rows for one Narrative difficulty"),
					*FactionName));
			}
			SeenDifficulties.Add(Entry.Difficulty);
			if (!FMath::IsWithinInclusive(Entry.MaximumCars, 0, 8))
			{
				Error(FString::Printf(TEXT("counterattack force %s difficulty car count must be between zero and eight"),
					*FactionName));
			}
		}
	}

	void ValidateStealthProfile(const UTerritoryStealthProfile* Profile,
		const FString& Context, TArray<FString>& OutErrors)
	{
		if (!Profile) return;
		auto Error = [&Context, &OutErrors](const TCHAR* Message)
		{
			OutErrors.Add(Context.IsEmpty() ? FString(Message)
				: FString::Printf(TEXT("%s: %s"), *Context, Message));
		};
		if (!FMath::IsWithinInclusive(Profile->MinimumSightEvidence, 0.f, 1.f)
			|| !FMath::IsWithinInclusive(Profile->ImmediateSightExposureThreshold, 0.f, 1.f)
			|| Profile->MinimumSightEvidence > Profile->ImmediateSightExposureThreshold)
		{
			Error(TEXT("stealth sight thresholds must be ordered between zero and one"));
		}
		if (!FMath::IsFinite(Profile->GuardDetectionMultiplier)
			|| Profile->GuardDetectionMultiplier < 0.f
			|| !FMath::IsFinite(Profile->MaximumStealthRating)
			|| Profile->MaximumStealthRating <= 0.f)
		{
			Error(TEXT("stealth detection multiplier and rating scale must be finite and positive"));
		}
		if (Profile->MaximumInvestigators < 0 || Profile->InvestigationRadius < 100.f
			|| Profile->InvestigationDuration < 0.5f
			|| Profile->InvestigationAcceptanceRadius < 10.f)
		{
			Error(TEXT("stealth investigation count, radius, duration, or acceptance radius is invalid"));
		}
		if (!Profile->InvestigationActivityClass
			|| !Profile->InvestigationActivityClass->IsChildOf(
				UTerritoryInvestigationActivity::StaticClass()))
		{
			Error(TEXT("stealth Investigation Activity Class must inherit Territory Investigation Activity"));
		}
		if (Profile->bSendBreakStealthGameplayEvent
			&& !Profile->BreakStealthGameplayEventTag.IsValid())
		{
			Error(TEXT("stealth exposure event is enabled but its Gameplay Tag is empty"));
		}
		TSet<TSubclassOf<UGameplayEffect>> SeenStealthEffects;
		for (const TSubclassOf<UGameplayEffect> EffectClass :
			Profile->StealthGameplayEffectsToRemove)
		{
			if (!EffectClass)
			{
				Error(TEXT("stealth exposure contains an empty Gameplay Effect removal entry"));
				continue;
			}
			if (SeenStealthEffects.Contains(EffectClass))
			{
				Error(TEXT("stealth exposure repeats a Gameplay Effect removal entry"));
			}
			SeenStealthEffects.Add(EffectClass);
			const UGameplayEffect* Effect = EffectClass->GetDefaultObject<UGameplayEffect>();
			if (Effect && Effect->DurationPolicy == EGameplayEffectDurationType::Instant)
			{
				Error(TEXT("stealth exposure cannot remove an Instant Gameplay Effect because it has no active handle"));
			}
		}
		if (!FMath::IsWithinInclusive(Profile->MinimumDisguiseQuality, 0.f, 1.f))
		{
			Error(TEXT("minimum disguise quality must be between zero and one"));
		}
	}

	void ValidateDisguiseProfile(const UTerritoryDisguiseProfile* Profile,
		const FString& Context, TArray<FString>& OutErrors)
	{
		if (!Profile) return;
		auto Error = [&Context, &OutErrors](const TCHAR* Message)
		{
			OutErrors.Add(Context.IsEmpty() ? FString(Message)
				: FString::Printf(TEXT("%s: %s"), *Context, Message));
		};
		if (!Profile->PerceivedFaction.IsValid())
		{
			Error(TEXT("disguise must define a perceived Narrative faction"));
		}
		if (!FMath::IsWithinInclusive(Profile->Quality, 0.f, 1.f))
		{
			Error(TEXT("disguise quality must be between zero and one"));
		}
		if (!Profile->ActivatedEventTag.IsValid()
			|| !Profile->RemovedEventTag.IsValid()
			|| !Profile->CompromisedEventTag.IsValid()
			|| !Profile->RestoredEventTag.IsValid()
			|| !Profile->IdentityCheckPassedEventTag.IsValid()
			|| !Profile->IdentityCheckFailedEventTag.IsValid())
		{
			Error(TEXT("disguise Gameplay Event tags must all be configured"));
		}
	}

	void ValidateCounterAttackProfileBounds(
		const UTerritoryCounterAttackProfile* Profile,
		const FString& Context,
		TArray<FString>& OutErrors)
	{
		if (!Profile) return;

		auto AddError = [&Context, &OutErrors](const TCHAR* Message)
		{
			OutErrors.Add(Context.IsEmpty()
				? FString(Message)
				: FString::Printf(TEXT("%s: %s"), *Context, Message));
		};

		if (Profile->MinimumLaunchProbability > Profile->MaximumLaunchProbability)
		{
			AddError(TEXT("counterattack minimum launch probability exceeds maximum"));
		}
		if (!FMath::IsWithinInclusive(Profile->UnguardedLaunchProbability, 0.f, 1.f))
		{
			AddError(TEXT("UnguardedLaunchProbability must be between zero and one"));
		}
		if (Profile->UnguardedLaunchProbability < Profile->MinimumLaunchProbability)
		{
			AddError(TEXT("UnguardedLaunchProbability must be greater than or equal to MinimumLaunchProbability so adding the first guard cannot increase launch probability"));
		}
		if (Profile->MaximumApproaches <= 0)
		{
			AddError(TEXT("counterattack MaximumApproaches must be at least one"));
		}
		if (!FMath::IsFinite(Profile->MinimumInfluenceTimingScale)
			|| !FMath::IsWithinInclusive(Profile->MinimumInfluenceTimingScale, 0.05f, 1.f))
		{
			AddError(TEXT("counterattack MinimumInfluenceTimingScale must be between 0.05 and 1"));
		}
		if (!FMath::IsFinite(Profile->InfluenceWeight) || Profile->InfluenceWeight < 0.f)
		{
			AddError(TEXT("counterattack InfluenceWeight must be finite and non-negative"));
		}
		if (!FMath::IsFinite(Profile->ParticipantSpacing) || Profile->ParticipantSpacing < 100.f)
		{
			AddError(TEXT("counterattack ParticipantSpacing must be finite and at least 100 units"));
		}
		if (!FMath::IsWithinInclusive(Profile->SpawnPlacementAttemptsPerParticipant, 1, 16))
		{
			AddError(TEXT("counterattack SpawnPlacementAttemptsPerParticipant must be between 1 and 16"));
		}
		if (!FMath::IsFinite(Profile->StalledMovementRetryInterval) ||
			!FMath::IsWithinInclusive(Profile->StalledMovementRetryInterval, 0.25f, 10.f))
		{
			AddError(TEXT("counterattack StalledMovementRetryInterval must be between 0.25 and 10 seconds"));
		}
		if (!FMath::IsWithinInclusive(Profile->MaxStalledMovementRetries, 1, 100))
		{
			AddError(TEXT("counterattack MaxStalledMovementRetries must be between 1 and 100"));
		}
		if (!FMath::IsFinite(Profile->DefendingPlayerEngagementPadding)
			|| !FMath::IsWithinInclusive(
				Profile->DefendingPlayerEngagementPadding, 0.f, 5000.f))
		{
			AddError(TEXT("counterattack DefendingPlayerEngagementPadding must be between 0 and 5000 cm"));
		}
		if (!FMath::IsWithinInclusive(Profile->MaxConsecutiveSpawnFailures, 1, 100))
		{
			AddError(TEXT("counterattack MaxConsecutiveSpawnFailures must be between 1 and 100"));
		}
	}

	template<typename TActor>
	TArray<TActor*> GetActorsForValidation(ULevel* Level)
	{
		TArray<TActor*> Result;
		if (!Level) return Result;
		if (UWorld* World = Level->GetWorld())
		{
			for (ULevel* WorldLevel : World->GetLevels())
			{
				if (!WorldLevel) continue;
				for (AActor* Actor : WorldLevel->Actors)
				{
					if (TActor* Typed = Cast<TActor>(Actor)) Result.AddUnique(Typed);
				}
			}
		}
		else
		{
			for (AActor* Actor : Level->Actors)
			{
				if (TActor* Typed = Cast<TActor>(Actor)) Result.AddUnique(Typed);
			}
		}
		return Result;
	}
}

UTerritoryDataValidator::UTerritoryDataValidator()
{
	// Auto-register with the validation system
	bIsEnabled = true;
}

bool UTerritoryDataValidator::CanValidateAsset_Implementation(
	const FAssetData& InAssetData,
	UObject* InAsset,
	FDataValidationContext& InContext) const
{
	(void)InAssetData;
	(void)InContext;
	if (!InAsset) return false;
	if (IsTerritoryStoryCaptureAsset(InAssetData)
		&& (InAsset->IsA(UNPCDefinition::StaticClass())
			|| InAsset->IsA(UDialogueBlueprint::StaticClass())))
	{
		return true;
	}

	// Validate any level/world that contains territory actors
	if (ULevel* Level = Cast<ULevel>(InAsset))
	{
		for (AActor* Actor : Level->Actors)
		{
			if (IsValid(Actor) && Actor->IsA<ATerritoryVolume>()) return true;
		}
	}
	if (UWorld* World = Cast<UWorld>(InAsset))
	{
		for (ULevel* Level : World->GetLevels())
		{
			if (Level && !GetActorsForValidation<ATerritoryVolume>(Level).IsEmpty()) return true;
		}
		if (UWorldPartition* WorldPartition = World->GetWorldPartition())
		{
			bool bFoundTerritory = false;
			FWorldPartitionHelpers::ForEachActorDescInstance<ATerritoryVolume>(WorldPartition,
				[&bFoundTerritory](const FWorldPartitionActorDescInstance*)
				{
					bFoundTerritory = true;
					return false;
				});
			if (bFoundTerritory) return true;
		}
	}

	// Also validate individual territory-related assets
	if (InAsset->IsA(ATerritoryVolume::StaticClass()) ||
		InAsset->IsA(ATerritoryWorldState::StaticClass()) ||
		InAsset->IsA(ATerritorySavableData::StaticClass()) ||
		InAsset->IsA(UTerritoryGuardPostDefinition::StaticClass()) ||
		InAsset->IsA(UTerritoryCounterAttackProfile::StaticClass()) ||
		InAsset->IsA(UTerritoryProductionProfile::StaticClass()) ||
		InAsset->IsA(UTerritoryStealthProfile::StaticClass()) ||
		InAsset->IsA(UTerritoryDisguiseProfile::StaticClass()) ||
		InAsset->IsA(UTerritoryDefinition::StaticClass()))
	{
		return true;
	}
	if (const UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(InAsset))
	{
		return UsesTerritoryNarrativeTask(QuestBlueprint);
	}

	return false;
}

EDataValidationResult UTerritoryDataValidator::ValidateLoadedAsset_Implementation(
	const FAssetData& InAssetData,
	UObject* InAsset,
	FDataValidationContext& InContext)
{
	(void)InAssetData;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	if (ATerritoryVolume* Territory = Cast<ATerritoryVolume>(InAsset))
	{
		ValidateTerritory(Territory, Errors, Warnings);
	}
	else if (ULevel* Level = Cast<ULevel>(InAsset))
	{
		ValidateLevel(Level, Errors, Warnings);
	}
	else if (UWorld* World = Cast<UWorld>(InAsset))
	{
		ValidateWorld(World, Errors, Warnings);
	}
	else if (UTerritoryCounterAttackProfile* Profile = Cast<UTerritoryCounterAttackProfile>(InAsset))
	{
		ValidateCounterAttackProfileBounds(Profile, TEXT("Counterattack profile"), Errors);
		if (Profile->FactionForces.IsEmpty())
		{
			Warnings.Add(TEXT("Counterattack profile has no faction force definitions"));
		}
		TSet<FGameplayTag> SeenFactions;
		for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
		{
			if (!Force.Faction.IsValid()) Errors.Add(TEXT("Counterattack faction force has no faction tag"));
			else if (SeenFactions.Contains(Force.Faction)) Errors.Add(FString::Printf(
				TEXT("Counterattack profile has duplicate force config for %s"), *Force.Faction.ToString()));
			SeenFactions.Add(Force.Faction);
			if (!Force.AttackerDefinition) Errors.Add(FString::Printf(
				TEXT("Counterattack force %s has no Narrative NPC definition"), *Force.Faction.ToString()));
			else
			{
				FText SpawnClassFailure;
				if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
					Force.AttackerDefinition, Force.PlannedForce, SpawnClassFailure))
				{
					Errors.Add(FString::Printf(
						TEXT("Counterattack force %s NPC class is not physically spawn-ready: %s"),
						*Force.Faction.ToString(), *SpawnClassFailure.ToString()));
				}
			}
			if (Force.PlannedForce <= 0 || Force.WaveSize <= 0 || Force.WaveSize > Force.PlannedForce)
			{
				Errors.Add(FString::Printf(TEXT("Counterattack force %s has invalid planned force/wave size"),
					*Force.Faction.ToString()));
			}
			if (Force.MilitaryPower <= 0.f) Errors.Add(FString::Printf(
				TEXT("Counterattack force %s must have positive military power"), *Force.Faction.ToString()));
			if (!FMath::IsWithinInclusive(Force.TerritorialInfluence, 0.f, 1.f)) Errors.Add(FString::Printf(
				TEXT("Counterattack force %s TerritorialInfluence must be between zero and one"), *Force.Faction.ToString()));
			ValidateCounterAttackForceSchedule(Force,
				TEXT("Counterattack profile"), Errors);
			ValidateCounterAttackFactionVehicles(Force,
				TEXT("Counterattack profile"), Errors);
			ValidateCounterAttackPowerScaling(Force,
				TEXT("Counterattack profile"), Errors, Warnings);
		}
	}
	else if (UTerritoryGuardPostDefinition* GuardPost =
		Cast<UTerritoryGuardPostDefinition>(InAsset))
	{
		if (GuardPost->DisplayName.IsEmpty())
		{
			Warnings.Add(TEXT("Guard post definition has no display name"));
		}
		if (GuardPost->PatrolRoute.Num() == 1)
		{
			Errors.Add(TEXT("Guard post patrol route has one node; author at least two nodes or clear the route for an intentional static post"));
		}
		for (int32 NodeIndex = 0; NodeIndex < GuardPost->PatrolRoute.Num(); ++NodeIndex)
		{
			const FTerritoryPatrolNode& Node = GuardPost->PatrolRoute[NodeIndex];
			if (Node.Location.ContainsNaN() || Node.Rotation.ContainsNaN()
				|| !FMath::IsFinite(Node.WaitTime) || Node.WaitTime < 0.f)
			{
				Errors.Add(FString::Printf(
					TEXT("Guard post patrol node %d has an invalid transform or wait time"), NodeIndex));
			}
		}
		if (GuardPost->ReserveSlots < 0 || GuardPost->ReserveSpawnDelay < 0.1f
			|| GuardPost->ReserveSpawnRetryInterval < 0.1f
			|| GuardPost->ReserveSpawnRadius < 100.f
			|| GuardPost->ReserveMinimumPlayerDistance < 0.f
			|| !FMath::IsWithinInclusive(GuardPost->ReserveSpawnCandidateCount, 1, 64))
		{
			Errors.Add(TEXT("Guard post reserve deployment settings are outside their supported bounds"));
		}
		if (GuardPost->NPCDefinition)
		{
			FText FailureReason;
			if (!ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
				GuardPost->NPCDefinition, 1, FailureReason))
			{
				Errors.Add(FString::Printf(TEXT("Guard post Narrative NPC definition is not spawn-ready: %s"),
					*FailureReason.ToString()));
			}
		}
	}
	else if (UTerritoryProductionProfile* ProductionProfile = Cast<UTerritoryProductionProfile>(InAsset))
	{
		FText FailureReason;
		if (!ProductionProfile->ValidateProfile(FailureReason))
		{
			Errors.Add(FString::Printf(TEXT("Invalid production profile: %s"),
				*FailureReason.ToString()));
		}
	}
	else if (UTerritoryStealthProfile* StealthProfile = Cast<UTerritoryStealthProfile>(InAsset))
	{
		ValidateStealthProfile(StealthProfile, TEXT("Stealth profile"), Errors);
	}
	else if (UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(InAsset))
	{
		ValidateDialogue(DialogueBlueprint->DialogueTemplate, Errors, Warnings);
	}
	else if (UNPCDefinition* NPCDefinition = Cast<UNPCDefinition>(InAsset))
	{
		const FSoftObjectPath AppearancePath =
			NPCDefinition->DefaultAppearance.ToSoftObjectPath();
		if (AppearancePath.IsNull())
		{
			Errors.Add(TEXT("Story NPC has no Narrative Character Appearance"));
		}
		else if (AppearancePath.GetLongPackageName().Contains(TEXT("/Mannequin/")))
		{
			Warnings.Add(FString::Printf(
				TEXT("Story NPC uses prototype Narrative mannequin appearance %s; mesh/card hair cannot receive MetaHuman Groom quality—replace it with an authored cinematic appearance or MetaHuman-compatible NPC visual"),
				*AppearancePath.ToString()));
		}
		if (NPCDefinition->Dialogue.IsNull())
		{
			Warnings.Add(TEXT("Story NPC has no default Narrative dialogue"));
		}
	}
	else if (UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(InAsset))
	{
		ValidateQuest(QuestBlueprint, Errors, Warnings);
	}
	else if (UTerritoryDisguiseProfile* DisguiseProfile = Cast<UTerritoryDisguiseProfile>(InAsset))
	{
		ValidateDisguiseProfile(DisguiseProfile, TEXT("Disguise profile"), Errors);
	}
	else if (UTerritoryDefinition* Definition = Cast<UTerritoryDefinition>(InAsset))
	{
		ValidateDefinition(Definition, Errors, Warnings);
	}
	else if (ATerritoryWorldState* WS = Cast<ATerritoryWorldState>(InAsset))
	{
		// Validate WorldState has a valid GUID
		if (!WS->GetActorGUID_Implementation().IsValid())
		{
			Warnings.Add(TEXT("ATerritoryWorldState has no GUID — save/load will not match this actor"));
		}
	}
	else if (ATerritorySavableData* SD = Cast<ATerritorySavableData>(InAsset))
	{
		// Validate SavableData has a valid GUID
		if (!SD->GetActorGUID_Implementation().IsValid())
		{
			Warnings.Add(TEXT("ATerritorySavableData has no GUID — save/load will not match this actor"));
		}
	}

	// Emit errors
	for (const FString& Error : Errors)
	{
		InContext.AddError(FText::FromString(Error));
	}

	// Warnings remain non-blocking and are reported with their correct severity.
	for (const FString& Warning : Warnings)
	{
		InContext.AddWarning(FText::FromString(Warning));
	}

	// Invalid if any errors; warnings alone don't fail validation
	return Errors.Num() == 0 ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Manual Validation API
// ═══════════════════════════════════════════════════════════════════════════════

bool UTerritoryDataValidator::ValidateLevel(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Level) return false;

	CheckDuplicateTags(Level, OutErrors);
	CheckDuplicateGUIDs(Level, OutErrors);
	CheckHierarchyIntegrity(Level, OutErrors, OutWarnings);
	CheckSingletonActors(Level, OutErrors, OutWarnings);
	CheckDuplicateDisplayNames(Level, OutWarnings);
	CheckOrphanedSpawnPoints(Level, OutWarnings);
	CheckMissingParentTags(Level, OutWarnings);

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		ValidateTerritory(Territory, OutErrors, OutWarnings);
	}

	return OutErrors.Num() == 0;
}

bool UTerritoryDataValidator::ValidateWorld(UWorld* World, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!World || !World->PersistentLevel) return false;

	// Pin all relevant external actors for the duration of validation. This makes
	// duplicate IDs, hierarchy links and singleton checks deterministic even when
	// World Partition has not streamed those actors into the editor viewport.
	TArray<FWorldPartitionReference> LoadedReferences;
	if (UWorldPartition* WorldPartition = World->GetWorldPartition())
	{
		auto PinActorClass = [WorldPartition, &LoadedReferences](UClass* ActorClass)
		{
			FWorldPartitionHelpers::ForEachActorDescInstance(WorldPartition, ActorClass,
				[WorldPartition, &LoadedReferences](const FWorldPartitionActorDescInstance* ActorDesc)
				{
					if (ActorDesc)
					{
						LoadedReferences.Emplace(WorldPartition, ActorDesc->GetGuid());
					}
					return true;
				});
		};

		PinActorClass(ATerritoryVolume::StaticClass());
		PinActorClass(ATerritoryGuardSpawnPoint::StaticClass());
		PinActorClass(ATerritoryWorldState::StaticClass());
		PinActorClass(ATerritorySavableData::StaticClass());
	}

	return ValidateLevel(World->PersistentLevel, OutErrors, OutWarnings);
}

bool UTerritoryDataValidator::ValidateTerritory(ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Territory) return false;

	FString Label = Territory->GetActorLabel();
	UTerritoryDefinition* Definition = Territory->GetTerritoryDefinition();
	if (!Definition)
	{
		OutErrors.Add(FString::Printf(
			TEXT("%s: Territory Definition is required; Blueprint/actor-side authoring is no longer supported"),
			*Label));
	}
	else
	{
		if (!Definition->IsDefinitionCompatible(Territory))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: Definition %s is not compatible with actor class %s"),
				*Label, *Definition->GetName(), *Territory->GetClass()->GetName()));
		}
		if (Definition->TerritoryTag != Territory->GetTerritoryTag())
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: actor tag differs from its Definition; reapply/synchronize the Definition"),
				*Label));
		}
		if (Definition->StableTerritoryGUID.IsValid()
			&& Definition->StableTerritoryGUID != Territory->GetTerritoryGUID())
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: actor save GUID differs from its Definition"), *Label));
		}
	}

	// Check for empty tag
	FGameplayTag Tag = Territory->GetTerritoryTag();
	if (!Tag.IsValid())
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: TerritoryTag is not set"), *Label));
	}

	// Check for missing display name
	FText DisplayName = Territory->GetTerritoryDisplayName();
	if (DisplayName.IsEmpty())
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: TerritoryDisplayName is empty"), *Label));
	}

	// FIX: Use InitialOwningFaction (editor property) not GetOwningFaction (runtime state)
	FGameplayTag FactionTag;
	if (ATerritoryVolume* Vol = Territory)
	{
		// Read the InitialOwningFaction property — the editor-authored value
		FactionTag = Vol->GetInitialOwningFaction();
	}

	if (FactionTag.IsValid())
	{
		if (!FactionTag.ToString().StartsWith(TEXT("Narrative.Factions.")))
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: InitialOwningFaction '%s' doesn't start with Narrative.Factions"),
				*Label, *FactionTag.ToString()));
		}
	}

	// Check economy configuration
	CheckEconomyConfig(Territory, OutWarnings);

	// Check bounds shape
	CheckBoundsShape(Territory, OutWarnings);

	// Check Narrative guard and physical counterattack configuration.
	CheckGuardConfig(Territory, OutErrors, OutWarnings);
	CheckCounterAttackConfig(Territory, OutErrors, OutWarnings);

	return OutErrors.Num() == 0;
}

void UTerritoryDataValidator::CheckDuplicateTags(ULevel* Level, TArray<FString>& OutErrors)
{
	TMap<FGameplayTag, FString> TagOwners;

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		FGameplayTag Tag = Territory->GetTerritoryTag();
		if (!Tag.IsValid()) continue;

		if (TagOwners.Contains(Tag))
		{
			OutErrors.Add(FString::Printf(TEXT("DUPLICATE TAG '%s': both '%s' and '%s' use it"),
				*Tag.ToString(), *TagOwners[Tag], *Territory->GetActorLabel()));
		}
		else
		{
			TagOwners.Add(Tag, Territory->GetActorLabel());
		}
	}
}

void UTerritoryDataValidator::CheckDuplicateGUIDs(ULevel* Level, TArray<FString>& OutErrors)
{
	TMap<FGuid, FString> GUIDOwners;
	auto AddGUID = [&GUIDOwners, &OutErrors](const FGuid& GUID, const FString& Label, const TCHAR* Kind)
	{
		if (!GUID.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s '%s' has an invalid persistent GUID"), Kind, *Label));
			return;
		}
		if (const FString* Existing = GUIDOwners.Find(GUID))
		{
			OutErrors.Add(FString::Printf(TEXT("DUPLICATE GUID '%s': both '%s' and '%s' use it"),
				*GUID.ToString(), **Existing, *Label));
		}
		else
		{
			GUIDOwners.Add(GUID, Label);
		}
	};

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		AddGUID(Territory->GetActorGUID_Implementation(), Territory->GetActorLabel(), TEXT("Territory"));
	}
	for (ATerritoryGuardSpawnPoint* SpawnPoint : GetActorsForValidation<ATerritoryGuardSpawnPoint>(Level))
	{
		AddGUID(SpawnPoint->GetActorGUID_Implementation(), SpawnPoint->GetActorLabel(), TEXT("Guard spawn point"));
	}
	for (ATerritoryWorldState* WorldState : GetActorsForValidation<ATerritoryWorldState>(Level))
	{
		AddGUID(WorldState->GetActorGUID_Implementation(), WorldState->GetActorLabel(), TEXT("World state"));
	}
	for (ATerritorySavableData* SavableData : GetActorsForValidation<ATerritorySavableData>(Level))
	{
		AddGUID(SavableData->GetActorGUID_Implementation(), SavableData->GetActorLabel(), TEXT("Deprecated savable data"));
	}
}

void UTerritoryDataValidator::CheckHierarchyIntegrity(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	TSet<FGameplayTag> AllTags;
	TMap<FGameplayTag, UClass*> TagToClass;
	TMap<FGameplayTag, FGameplayTag> ParentByTag;

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		FGameplayTag Tag = Territory->GetTerritoryTag();
		if (Tag.IsValid())
		{
			AllTags.Add(Tag);
			TagToClass.Add(Tag, Territory->GetClass());
			ParentByTag.Add(Tag, Territory->GetParentTerritoryTag());
		}
	}

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		FGameplayTag ParentTag = Territory->GetParentTerritoryTag();
		FGameplayTag SelfTag = Territory->GetTerritoryTag();
		FString Label = Territory->GetActorLabel();

		// Missing parent reference
		if (ParentTag.IsValid() && !AllTags.Contains(ParentTag))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: ParentTerritoryTag '%s' references a non-existent territory"),
				*Label, *ParentTag.ToString()));
		}

		// Self-reference (cycle)
		if (ParentTag.IsValid() && ParentTag == SelfTag)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: ParentTerritoryTag references itself (cycle)"),
				*Label));
		}

		// Parent class type mismatch
		if (ParentTag.IsValid())
		{
			UClass** ParentClassPtr = TagToClass.Find(ParentTag);
			if (ParentClassPtr && *ParentClassPtr)
			{
				UClass* ParentClass = *ParentClassPtr;
				if (Territory->IsA(ATerritoryDistrict::StaticClass()) && !ParentClass->IsChildOf(ATerritoryCity::StaticClass()))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: District's parent '%s' is not a TerritoryCity"),
						*Label, *ParentTag.ToString()));
				}
				if (Territory->IsA(ATerritoryProperty::StaticClass()) && !ParentClass->IsChildOf(ATerritoryDistrict::StaticClass()))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: Property's parent '%s' is not a TerritoryDistrict"),
						*Label, *ParentTag.ToString()));
				}
			}
		}
	}

	// Detect cycles of any length, not only direct self-parenting.
	TSet<FGameplayTag> ReportedCycleTags;
	for (const TPair<FGameplayTag, FGameplayTag>& StartPair : ParentByTag)
	{
		TArray<FGameplayTag> Path;
		FGameplayTag Cursor = StartPair.Key;
		while (Cursor.IsValid() && ParentByTag.Contains(Cursor))
		{
			const int32 ExistingIndex = Path.IndexOfByKey(Cursor);
			if (ExistingIndex != INDEX_NONE)
			{
				if (!ReportedCycleTags.Contains(Cursor))
				{
					TArray<FString> CycleNames;
					for (int32 Index = ExistingIndex; Index < Path.Num(); ++Index)
					{
						CycleNames.Add(Path[Index].ToString());
						ReportedCycleTags.Add(Path[Index]);
					}
					CycleNames.Add(Cursor.ToString());
					OutErrors.Add(FString::Printf(TEXT("Territory hierarchy cycle: %s"),
						*FString::Join(CycleNames, TEXT(" -> "))));
				}
				break;
			}
			Path.Add(Cursor);
			Cursor = ParentByTag.FindRef(Cursor);
		}
	}
}

void UTerritoryDataValidator::CheckSingletonActors(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	int32 WorldStateCount = 0;
	int32 SavableDataCount = 0;

	WorldStateCount = GetActorsForValidation<ATerritoryWorldState>(Level).Num();
	SavableDataCount = GetActorsForValidation<ATerritorySavableData>(Level).Num();

	if (WorldStateCount > 1)
	{
		OutErrors.Add(FString::Printf(TEXT("Multiple ATerritoryWorldState actors (%d found) — only one is allowed"),
			WorldStateCount));
	}
	if (SavableDataCount > 1)
	{
		OutErrors.Add(FString::Printf(TEXT("Multiple ATerritorySavableData actors (%d found) — only one is allowed"),
			SavableDataCount));
	}

	if (WorldStateCount == 0 && SavableDataCount == 0)
	{
		OutWarnings.Add(TEXT("No TerritoryWorldState or TerritorySavableData actor found — economy/diplomacy state will not persist"));
	}

	if (SavableDataCount > 0)
	{
		if (WorldStateCount > 0)
		{
			OutErrors.Add(TEXT("Both ATerritoryWorldState and ATerritorySavableData actors found — ATerritorySavableData is deprecated and will cause save/load conflicts. Remove ATerritorySavableData and use only ATerritoryWorldState."));
		}
		else
		{
			OutWarnings.Add(TEXT("ATerritorySavableData is deprecated. Replace with ATerritoryWorldState for multiplayer compatibility and richer save/load support."));
		}
	}
}

void UTerritoryDataValidator::CheckEconomyConfig(ATerritoryVolume* Territory, TArray<FString>& OutWarnings)
{
	if (!Territory) return;

	FString Label = Territory->GetActorLabel();

	int32 Income = Territory->GetPeriodicIncome();
	int32 GuardCost = Territory->GetGuardCost();

	if (Income < 0)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: PeriodicIncome is negative (%d)"), *Label, Income));
	}
	if (GuardCost < 0)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: GuardCost is negative (%d)"), *Label, GuardCost));
	}
	if (Territory->GetMaxConcurrentAttackers() < 1)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: MaxConcurrentAttackers < 1 — no NPCs can attack"), *Label));
	}

	if (const ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		if (const UTerritoryProductionProfile* Profile = Property->GetProductionProfile())
		{
			FText FailureReason;
			if (!Profile->ValidateProfile(FailureReason))
			{
				OutWarnings.Add(FString::Printf(TEXT("%s: ProductionProfile is invalid: %s"),
					*Label, *FailureReason.ToString()));
			}
		}
	}
}

bool UTerritoryDataValidator::ValidateQuest(UQuestBlueprint* QuestBlueprint,
	TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!QuestBlueprint || !QuestBlueprint->QuestTemplate)
	{
		OutErrors.Add(TEXT("Narrative Quest has no Quest Template"));
		return false;
	}

	const FString Context = QuestBlueprint->GetPathName();
	const UQuest* Quest = QuestBlueprint->QuestTemplate;
	auto Error = [&OutErrors, &Context](const FString& Message)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: %s"), *Context, *Message));
	};
	auto Warning = [&OutWarnings, &Context](const FString& Message)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: %s"), *Context, *Message));
	};

	if (!Quest->GetQuestStartState())
	{
		Error(TEXT("Quest Start State is required"));
	}

	for (const UQuestState* State : Quest->GetStates())
	{
		if (!State)
		{
			Error(TEXT("Quest States contains an empty row"));
			continue;
		}
		if (State->StateNodeType != EStateNodeType::Regular
			&& !State->Branches.IsEmpty())
		{
			const FString StateType = StaticEnum<EStateNodeType>()->GetNameStringByValue(
				static_cast<int64>(State->StateNodeType));
			Error(FString::Printf(
				TEXT("Quest state '%s' is marked %s but still has an outgoing objective. Narrative completes the quest as soon as this state is entered, so the later Territory task can complete the same quest again. Change this intermediate state to Regular and keep Success or Failure only on the final state."),
				*GetNameSafe(State), *StateType));
		}
	}

	for (const UQuestBranch* Branch : Quest->GetBranches())
	{
		if (!Branch)
		{
			Error(TEXT("Quest Branches contains an empty row"));
			continue;
		}
		if (!Branch->DestinationState)
		{
			Error(FString::Printf(TEXT("Quest branch '%s' has no Destination State"),
				*GetNameSafe(Branch)));
		}
		if (Branch->QuestTasks.IsEmpty())
		{
			Warning(FString::Printf(
				TEXT("Quest branch '%s' has no tasks and may advance immediately"),
				*GetNameSafe(Branch)));
		}
		for (const UNarrativeTask* Task : Branch->QuestTasks)
		{
			if (!Task)
			{
				Error(FString::Printf(TEXT("Quest branch '%s' contains an empty task row"),
					*GetNameSafe(Branch)));
			}
		}
	}

	return OutErrors.IsEmpty();
}

bool UTerritoryDataValidator::ValidateDialogue(UDialogue* Dialogue,
	TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Dialogue)
	{
		OutErrors.Add(TEXT("Narrative dialogue has no template object"));
		return false;
	}

	TSet<const UNarrativeDialogueSequence*> VisitedShots;
	bool bHasAnyShot = Dialogue->DefaultDialogueShot != nullptr;
	ValidateDialogueShot(Dialogue->DefaultDialogueShot,
		TEXT("Default dialogue shot"), VisitedShots, OutErrors);

	for (const FSpeakerInfo& Speaker : Dialogue->Speakers)
	{
		bHasAnyShot |= Speaker.DefaultSpeakerShot != nullptr;
		ValidateDialogueShot(Speaker.DefaultSpeakerShot,
			FString::Printf(TEXT("Speaker %s default shot"),
				*Speaker.GetSpeakerID().ToString()), VisitedShots, OutErrors);
	}
	bHasAnyShot |= Dialogue->PlayerSpeakerInfo.DefaultSpeakerShot != nullptr;
	bHasAnyShot |= Dialogue->PlayerSpeakerInfo.SelectingReplyShot != nullptr;
	ValidateDialogueShot(Dialogue->PlayerSpeakerInfo.DefaultSpeakerShot,
		TEXT("Player default shot"), VisitedShots, OutErrors);
	ValidateDialogueShot(Dialogue->PlayerSpeakerInfo.SelectingReplyShot,
		TEXT("Player reply-selection shot"), VisitedShots, OutErrors);

	for (const UDialogueNode* Node : Dialogue->GetNodes())
	{
		if (!Node) continue;
		const FString NodeContext = FString::Printf(TEXT("Dialogue node %s"),
			*Node->GetName());
		bHasAnyShot |= Node->Line.Shot != nullptr;
		ValidateDialogueShot(Node->Line.Shot, NodeContext,
			VisitedShots, OutErrors);
		for (int32 LineIndex = 0; LineIndex < Node->AlternativeLines.Num(); ++LineIndex)
		{
			const FDialogueLine& Line = Node->AlternativeLines[LineIndex];
			bHasAnyShot |= Line.Shot != nullptr;
			ValidateDialogueShot(Line.Shot,
				FString::Printf(TEXT("%s alternative line %d"),
					*NodeContext, LineIndex), VisitedShots, OutErrors);
		}
	}

	if (!bHasAnyShot)
	{
		OutWarnings.Add(TEXT("Dialogue has no default, speaker, reply-selection, or line shot; it will retain gameplay framing and HUD instead of a controlled cinematic camera"));
	}
	if (!Dialogue->bFreeMovement && !Dialogue->bAutoStopMovement)
	{
		OutWarnings.Add(TEXT("Cinematic dialogue does not auto-stop participant movement; anchored shots can drift out of composition"));
	}
	if (Dialogue->DialogueBlendOutTime <= 0.f)
	{
		OutWarnings.Add(TEXT("Dialogue blend-out time is zero; the final shot will hard-cut to gameplay camera"));
	}
	return OutErrors.IsEmpty();
}

bool UTerritoryDataValidator::ValidateDefinition(UTerritoryDefinition* Definition,
	TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Definition)
	{
		OutErrors.Add(TEXT("Territory Definition is null"));
		return false;
	}

	const FString Context = Definition->GetPathName();
	auto Error = [&OutErrors, &Context](const FString& Message)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: %s"), *Context, *Message));
	};
	auto Warning = [&OutWarnings, &Context](const FString& Message)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: %s"), *Context, *Message));
	};

	if (!Definition->TerritoryTag.IsValid()) Error(TEXT("Territory Tag is required"));
	if (Definition->DisplayName.IsEmpty()) Warning(TEXT("Display Name is empty"));
	if (!Definition->StableTerritoryGUID.IsValid())
		Error(TEXT("Stable Territory GUID is missing; migration/save identity is unsafe"));
	if (Definition->RelativeTransform.ContainsNaN()) Error(TEXT("Relative Transform contains invalid values"));

	UClass* ActorClass = Definition->TerritoryActorClass.LoadSynchronous();
	if (!ActorClass)
	{
		Error(TEXT("Territory Actor Class is required"));
	}
	else if (Definition->IsA<UTerritoryPlaceDefinition>()
		&& !ActorClass->IsChildOf(ATerritoryProperty::StaticClass()))
	{
		Error(TEXT("Place Definition actor class must inherit TerritoryProperty"));
	}
	else if (Definition->IsA<UTerritoryDistrictDefinition>()
		&& (!ActorClass->IsChildOf(ATerritoryDistrict::StaticClass())
			|| ActorClass->IsChildOf(ATerritoryCity::StaticClass())))
	{
		Error(TEXT("District Definition actor class must inherit TerritoryDistrict"));
	}
	else if (Definition->IsA<UTerritoryCityDefinition>()
		&& !ActorClass->IsChildOf(ATerritoryCity::StaticClass()))
	{
		Error(TEXT("City Definition actor class must inherit TerritoryCity"));
	}

	if (Definition->IsA<UTerritoryCityDefinition>())
	{
		if (Definition->DerivedParentTerritoryTag.IsValid())
			Error(TEXT("City must not have a derived parent tag"));
	}
	else if (!Definition->DerivedParentTerritoryTag.IsValid())
	{
		Warning(TEXT("Definition is not yet connected to its parent hierarchy asset"));
	}

	const bool bPlaceDefinition = Definition->IsA<UTerritoryPlaceDefinition>();
	if (bPlaceDefinition && Definition->ControlMode != ETerritoryControlMode::Independent)
	{
		Error(TEXT("Place Definition control mode must be Independent"));
	}
	if (!bPlaceDefinition)
	{
		if (Definition->ControlMode != ETerritoryControlMode::AggregateOnly)
			Error(TEXT("City and District control mode must be Aggregate Only"));
		if (Definition->InitialOwningFaction.IsValid()
			|| Definition->InitialState != ETerritoryInitialState::Automatic)
			Error(TEXT("City and District initial owner/state are derived from children; author only Initial Availability and State Rules on the parent"));
		if (Definition->MaxConcurrentAttackers != 1 || Definition->PeriodicIncome != 0
			|| Definition->GuardUpkeepPerCycle != 0 || Definition->GuardRecruitmentCost != 0)
			Error(TEXT("City and District cannot author physical attacker, income, or guard-cost values; put them on child Places"));
		if (Definition->bStoryCaptureFromBounds || Definition->CapturePoint.bEnabled)
			Error(TEXT("City and District cannot author physical capture; put it on a child Place"));
		if (Definition->DefaultGuardDefinition || !Definition->FactionGuardDefinitions.IsEmpty()
			|| Definition->InitialGuardCount != 0 || !Definition->GuardPosts.IsEmpty())
			Error(TEXT("City and District cannot author guards or spawn posts; put them on child Places"));
		if (!Definition->DefenderDiedEvents.IsEmpty()
			|| !Definition->AllDefendersDefeatedEvents.IsEmpty())
			Error(TEXT("Defender events are Place-only because aggregate parents have no physical defenders"));
		if (Definition->CounterAttackProfile || !Definition->CounterAttackApproaches.IsEmpty())
			Error(TEXT("Physical counterattack profiles and approaches belong to a target Place"));
		if (!FMath::IsNearlyZero(Definition->GuardQuality)
			|| !FMath::IsNearlyZero(Definition->FortificationStrength)
			|| !FMath::IsNearlyZero(Definition->NearbyAlliedSupport))
			Error(TEXT("City and District cannot author physical guard quality, fortification, or local support"));
		if (Definition->DefaultStealthProfile)
			Error(TEXT("Stealth infiltration belongs to a physical Place, not an aggregate City or District"));
	}

	for (const TPair<ETerritoryState, FTerritoryStateConfig>& Pair : Definition->StateConfigs)
	{
		auto CheckObjects = [&Error, &Pair](const auto& Objects, const TCHAR* Label)
		{
			for (int32 Index = 0; Index < Objects.Num(); ++Index)
			{
				if (!Objects[Index]) Error(FString::Printf(
					TEXT("State %d %s contains a null row at index %d"),
					static_cast<int32>(Pair.Key), Label, Index));
			}
		};
		CheckObjects(Pair.Value.EntryConditions, TEXT("Entry Conditions"));
		CheckObjects(Pair.Value.ExitConditions, TEXT("Exit Conditions"));
		CheckObjects(Pair.Value.EntryEvents, TEXT("Entry Events"));
		CheckObjects(Pair.Value.ExitEvents, TEXT("Exit Events"));
		bool bSchedulesEnemyWave = false;
		bool bEndsWar = false;
		bool bWarEstablishedBeforeWave = false;
		for (const UNarrativeEvent* Event : Pair.Value.EntryEvents)
		{
			if (const UTerritorySetDiplomacyEvent* DiplomacyEvent =
				Cast<UTerritorySetDiplomacyEvent>(Event))
			{
				bEndsWar |= DiplomacyEvent->NewState != EDiplomacyState::War;
				bWarEstablishedBeforeWave |=
					DiplomacyEvent->NewState == EDiplomacyState::War;
			}

			const UTerritoryScheduleEnemyWaveEvent* WaveEvent =
				Cast<UTerritoryScheduleEnemyWaveEvent>(Event);
			if (!WaveEvent) continue;
			bSchedulesEnemyWave = true;
			if (!WaveEvent->TargetTerritory.IsValid())
			{
				Error(FString::Printf(
					TEXT("State %d contains a Wave of Enemies event without a target Territory"),
					static_cast<int32>(Pair.Key)));
			}
			if (!WaveEvent->bChooseBestEligibleAttacker
				&& !WaveEvent->AttackingFaction.IsValid())
			{
				Error(FString::Printf(
					TEXT("State %d contains a Wave of Enemies event without an attacking faction; choose an attacker or enable Best Eligible Attacker"),
					static_cast<int32>(Pair.Key)));
			}

			const bool bHasWarCondition = WaveEvent->Conditions.ContainsByPredicate(
				[](const UNarrativeCondition* Condition)
				{
					const UTerritoryDiplomacyCondition* DiplomacyCondition =
						Cast<UTerritoryDiplomacyCondition>(Condition);
					return DiplomacyCondition
						&& DiplomacyCondition->RequiredState == EDiplomacyState::War;
				});
			if (!bWarEstablishedBeforeWave && !bHasWarCondition)
			{
				Warning(FString::Printf(
					TEXT("State %d schedules a Wave of Enemies without an earlier Set Territory Diplomacy = War event or an inherited Territory Diplomacy Condition requiring War. The runtime will reject the Wave while the attacker and defender are Neutral, allied, under ceasefire, trading, or non-aggressive."),
					static_cast<int32>(Pair.Key)));
			}
		}
		if (bSchedulesEnemyWave && bEndsWar)
		{
			Warning(FString::Printf(
				TEXT("State %d schedules an enemy Wave and also ends War in the same Entry Events row. Territory assaults require War during admission and deployment, so the peace-like event will cancel the Wave. Move peace to the Quest/assault-resolution path."),
				static_cast<int32>(Pair.Key)));
		}
		ValidateStealthProfile(Pair.Value.StealthProfileOverride,
			FString::Printf(TEXT("%s state %d stealth override"), *Context,
				static_cast<int32>(Pair.Key)), OutErrors);

		const FTerritoryStateAudioConfig& Audio = Pair.Value.Audio;
		if (Audio.bOverrideNarrativeMusic)
		{
			if (!Audio.MusicTheme.IsValid())
			{
				Error(FString::Printf(
					TEXT("State %d enables Narrative Music but has no Music Theme"),
					static_cast<int32>(Pair.Key)));
			}
			else
			{
				const FGameplayTag MusicRoot = FGameplayTag::RequestGameplayTag(
					TEXT("Music"), false);
				if (MusicRoot.IsValid() && !Audio.MusicTheme.MatchesTag(MusicRoot))
				{
					Error(FString::Printf(
						TEXT("State %d Music Theme must be under the Music tag root"),
						static_cast<int32>(Pair.Key)));
				}

				if (!Audio.MusicSetOverride.IsNull())
				{
					UTaggedMusicSet* MusicSet =
						Audio.MusicSetOverride.LoadSynchronous();
					if (!MusicSet)
					{
						Error(FString::Printf(
							TEXT("State %d Narrative Music Set could not be loaded"),
							static_cast<int32>(Pair.Key)));
					}
					else if (!MusicSet->Has(Audio.MusicTheme))
					{
						Error(FString::Printf(
							TEXT("State %d Narrative Music Set has no track row for %s"),
							static_cast<int32>(Pair.Key),
							*Audio.MusicTheme.ToString()));
					}
				}
			}
		}
		if (!FMath::IsWithinInclusive(Audio.StateEffectVolume, 0.f, 4.f)
			|| !FMath::IsWithinInclusive(Audio.StateEffectPitch, 0.25f, 4.f))
		{
			Error(FString::Printf(
				TEXT("State %d audio effect volume or pitch is outside supported bounds"),
				static_cast<int32>(Pair.Key)));
		}
		if (Audio.bPlayEnteredSoundOnPlayerArrival
			&& Audio.StateEnteredSound.IsNull())
		{
			Warning(FString::Printf(
				TEXT("State %d enables the arrival cue but has no State Entered Sound"),
				static_cast<int32>(Pair.Key)));
		}
		if (Audio.bPlayExitedSoundOnPlayerDeparture
			&& Audio.StateExitedSound.IsNull())
		{
			Warning(FString::Printf(
				TEXT("State %d enables the departure cue but has no State Exited Sound"),
				static_cast<int32>(Pair.Key)));
		}
	}
	if (UTerritoryPlaceDefinition* Place = Cast<UTerritoryPlaceDefinition>(Definition))
	{
		const FGameplayTag RoleRoot = FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Property.Role"), false);
		const FGameplayTag BenefitRoot = FGameplayTag::RequestGameplayTag(
			TEXT("Territory.Property.Benefit"), false);
		if (Place->PropertyRoleTag.IsValid() && RoleRoot.IsValid()
			&& !Place->PropertyRoleTag.MatchesTag(RoleRoot))
		{
			Error(TEXT("Property Role Tag must be under Territory.Property.Role"));
		}
		TSet<FGameplayTag> SeenBenefits;
		for (int32 Index = 0; Index < Place->GameplayBenefits.Num(); ++Index)
		{
			const FTerritoryPropertyGameplayBenefit& Benefit =
				Place->GameplayBenefits[Index];
			const FString Prefix = FString::Printf(TEXT("Gameplay Benefit %d"), Index);
			if (Benefit.RequiredUpgradeLevel < 0
				|| Benefit.RequiredUpgradeLevel > Place->MaxUpgradeLevel)
			{
				Error(Prefix + TEXT(" requires an upgrade level outside this Place's supported range"));
			}
			if (!Benefit.BenefitTag.IsValid())
			{
				Error(Prefix + TEXT(" requires a stable Benefit Tag"));
			}
			else
			{
				if (BenefitRoot.IsValid() && !Benefit.BenefitTag.MatchesTag(BenefitRoot))
				{
					Error(Prefix + TEXT(" Benefit Tag must be under Territory.Property.Benefit"));
				}
				if (SeenBenefits.Contains(Benefit.BenefitTag))
				{
					Error(Prefix + TEXT(" repeats a Benefit Tag; combine its payload into one tier"));
				}
				SeenBenefits.Add(Benefit.BenefitTag);
			}
			if (Benefit.DisplayName.IsEmpty())
			{
				Warning(Prefix + TEXT(" has no player-facing Display Name"));
			}
			TSet<UClass*> SeenPayloadClasses;
			for (const TSubclassOf<UNarrativeGameplayAbility> Ability : Benefit.GrantedAbilities)
			{
				if (!Ability) Error(Prefix + TEXT(" contains an empty Narrative ability"));
				else if (SeenPayloadClasses.Contains(Ability))
					Error(Prefix + TEXT(" repeats a Narrative ability"));
				else SeenPayloadClasses.Add(Ability);
			}
			for (const TSubclassOf<UGameplayEffect> EffectClass : Benefit.GrantedGameplayEffects)
			{
				const UGameplayEffect* Effect = EffectClass
					? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
				if (!Effect) Error(Prefix + TEXT(" contains an empty Gameplay Effect"));
				else if (Effect->DurationPolicy == EGameplayEffectDurationType::Instant)
					Error(Prefix + TEXT(" uses an Instant Gameplay Effect; use Infinite so ownership loss can revoke it"));
				else if (SeenPayloadClasses.Contains(EffectClass))
					Error(Prefix + TEXT(" repeats a Gameplay Effect"));
				else SeenPayloadClasses.Add(EffectClass);
			}
			for (const TSubclassOf<UWeaponItem> Weapon : Benefit.UnlockedWeaponItems)
			{
				if (!Weapon) Error(Prefix + TEXT(" contains an empty weapon unlock"));
				else if (SeenPayloadClasses.Contains(Weapon))
					Error(Prefix + TEXT(" repeats a weapon unlock"));
				else SeenPayloadClasses.Add(Weapon);
			}
		}
	}
	ValidateStealthProfile(Definition->DefaultStealthProfile,
		Context + TEXT(" default stealth profile"), OutErrors);
	if (Definition->DefaultStealthProfile && !Definition->bStoryCaptureFromBounds)
	{
		Warning(TEXT("A stealth profile is assigned but Story Capture From Bounds is disabled; the profile can still drive investigation, but it will not defer a capture-point contest."));
	}

	if (!Definition->GuardBehavior.PatrolGoalClass)
		Error(TEXT("Guard Behavior requires a Patrol Goal Class"));
	if (!FMath::IsFinite(Definition->GuardBehavior.PatrolAvoidanceConsiderationRadius)
		|| Definition->GuardBehavior.PatrolAvoidanceConsiderationRadius < 100.f
		|| !FMath::IsWithinInclusive(Definition->GuardBehavior.PatrolAvoidanceWeight, 0.f, 1.f)
		|| !FMath::IsWithinInclusive(
			Definition->GuardBehavior.ClosestHostilePlayerGoalScoreBonus, 0.f, 10.f))
	{
		Error(TEXT("Guard Behavior avoidance or target-priority values are outside supported bounds"));
	}
	TSet<FGameplayTag> GuardDialogueFactions;
	for (const FTerritoryFactionDialogueProfile& Mapping :
		Definition->GuardBehavior.FactionDialogueProfiles)
	{
		if (!Mapping.Faction.IsValid())
			Error(TEXT("Guard Behavior dialogue mapping requires an exact Narrative faction"));
		else if (GuardDialogueFactions.Contains(Mapping.Faction))
			Error(FString::Printf(TEXT("Guard Behavior has duplicate dialogue mapping for %s"),
				*Mapping.Faction.ToString()));
		GuardDialogueFactions.Add(Mapping.Faction);
		if (!Mapping.DialogueProfile)
			Error(FString::Printf(TEXT("Guard Behavior dialogue mapping for %s has no profile"),
				*Mapping.Faction.ToString()));
	}

	TSet<FName> GuardPostIDs;
	TSet<FGuid> GuardPostGUIDs;
	for (const FTerritoryGuardPostTemplate& Post : Definition->GuardPosts)
	{
		if (Post.GuardPostID.IsNone()) Error(TEXT("Guard Post ID is required"));
		else if (GuardPostIDs.Contains(Post.GuardPostID))
			Error(FString::Printf(TEXT("Duplicate Guard Post ID: %s"), *Post.GuardPostID.ToString()));
		GuardPostIDs.Add(Post.GuardPostID);
		if (!Post.StableGuardPostGUID.IsValid()) Error(FString::Printf(
			TEXT("Guard Post %s has no stable GUID"), *Post.GuardPostID.ToString()));
		else if (GuardPostGUIDs.Contains(Post.StableGuardPostGUID)) Error(FString::Printf(
			TEXT("Guard Post %s duplicates another post GUID"), *Post.GuardPostID.ToString()));
		GuardPostGUIDs.Add(Post.StableGuardPostGUID);
		if (Post.RelativeTransform.ContainsNaN()) Error(FString::Printf(
			TEXT("Guard Post %s has an invalid relative transform"), *Post.GuardPostID.ToString()));
		UClass* GuardPostClass = Post.ActorClass.LoadSynchronous();
		if (!GuardPostClass) Error(FString::Printf(
			TEXT("Guard Post %s has no Blueprint actor class"), *Post.GuardPostID.ToString()));
		else if (!GuardPostClass->IsChildOf(ATerritoryGuardSpawnPoint::StaticClass()))
			Error(FString::Printf(TEXT("Guard Post %s actor class is not a Territory Guard Spawn Point"),
				*Post.GuardPostID.ToString()));
		if (Post.ReserveSlots < 0 || Post.ReserveSpawnDelay < 0.1f
			|| Post.ReserveSpawnRetryInterval < 0.1f
			|| Post.ReserveSpawnRadius < 100.f
			|| Post.ReserveMinimumPlayerDistance < 0.f
			|| !FMath::IsWithinInclusive(Post.ReserveSpawnCandidateCount, 1, 64)
			|| !FMath::IsWithinInclusive(Post.ReserveCameraAvoidanceRetryLimit, 0, 20)
			|| !FMath::IsWithinInclusive(Post.ReserveTotalRetryLimit, 1, 100))
		{
			Error(FString::Printf(TEXT("Guard Post %s reserve settings are outside supported bounds"),
				*Post.GuardPostID.ToString()));
		}
		if (Post.NPCDefinitionOverride)
		{
			FText FailureReason;
			if (!ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
				Post.NPCDefinitionOverride, 1, FailureReason))
			{
				Error(FString::Printf(TEXT("Guard Post %s Narrative NPC is not spawn-ready: %s"),
					*Post.GuardPostID.ToString(), *FailureReason.ToString()));
			}
		}
		for (const FTerritoryGuardPatrolTemplateNode& Node : Post.PatrolRoute)
		{
			if (Node.RelativeTransform.ContainsNaN() || !FMath::IsFinite(Node.WaitTime)
				|| Node.WaitTime < 0.f)
			{
				Error(FString::Printf(TEXT("Guard Post %s has an invalid patrol node"),
					*Post.GuardPostID.ToString()));
				break;
			}
		}
	}

	if (Definition->CapturePoint.bEnabled)
	{
		UClass* CaptureClass = Definition->CapturePoint.ActorClass.LoadSynchronous();
		if (!Definition->IsA<UTerritoryPlaceDefinition>())
			Error(TEXT("Only a Place Definition can enable a Capture Point"));
		if (!CaptureClass) Error(TEXT("Enabled Capture Point has no Blueprint actor class"));
		else if (!CaptureClass->IsChildOf(ATerritoryCapturePoint::StaticClass()))
			Error(TEXT("Capture Point class must inherit Territory Capture Point"));
		if (Definition->CapturePoint.RelativeTransform.ContainsNaN()
			|| Definition->CapturePoint.CaptureRadius < 100.f)
			Error(TEXT("Capture Point transform or radius is invalid"));
	}
	if (Definition->bStoryCaptureFromBounds && Definition->CapturePoint.bEnabled
		&& Definition->CapturePoint.bAutomaticCapture)
		Warning(TEXT("Story Capture From Bounds disables the configured automatic Capture Point"));
	if (Definition->ManagementPoint.bEnabled)
	{
		UClass* ManagementClass = Definition->ManagementPoint.ActorClass.LoadSynchronous();
		if (!ManagementClass)
			Error(TEXT("Enabled Management Point has no Blueprint actor class"));
		else if (!ManagementClass->IsChildOf(
			ATerritoryDistrictManagementPoint::StaticClass()))
			Error(TEXT("Management Point class must inherit Territory District Management Point"));
		if (!Definition->ManagementPoint.WidgetClass.LoadSynchronous())
			Error(TEXT("Enabled Management Point has no management widget class"));
		if (Definition->ManagementPoint.RelativeTransform.ContainsNaN()
			|| Definition->ManagementPoint.InteractionDistance < 100.f)
			Error(TEXT("Management Point transform or interaction distance is invalid"));
	}

	if (const UTerritoryPlaceDefinition* Place = Cast<UTerritoryPlaceDefinition>(Definition))
	{
		if (Place->StoryOwner.bEnabled)
		{
			UClass* StoryOwnerClass = Place->StoryOwner.ActorClass.LoadSynchronous();
			if (!StoryOwnerClass)
				Error(TEXT("Enabled Story Owner has no spawner Blueprint class"));
			else if (!StoryOwnerClass->IsChildOf(
				ATerritoryStoryOwnerSpawner::StaticClass()))
				Error(TEXT("Story Owner class must inherit Territory Story Owner Spawner"));
			if (!Place->StoryOwner.NPCDefinition)
				Error(TEXT("Enabled Story Owner has no Narrative NPC Definition"));
			if (Place->StoryOwner.RelativeTransform.ContainsNaN()
				|| !FMath::IsWithinInclusive(
					Place->StoryOwner.InteractionDistance, 100.f, 1000.f))
				Error(TEXT("Story Owner transform or interaction distance is invalid"));
		}
	}
	else if (UTerritoryDistrictDefinition* District =
		Cast<UTerritoryDistrictDefinition>(Definition))
	{
		TSet<FGameplayTag> ChildTags;
		for (UTerritoryPlaceDefinition* ChildPlace : District->Places)
		{
			if (!ChildPlace) { Error(TEXT("Places contains a null asset")); continue; }
			if (ChildTags.Contains(ChildPlace->TerritoryTag)) Error(FString::Printf(
				TEXT("Duplicate Place tag in District: %s"), *ChildPlace->TerritoryTag.ToString()));
			ChildTags.Add(ChildPlace->TerritoryTag);
			if (ChildPlace->DerivedParentTerritoryTag != District->TerritoryTag)
				Error(FString::Printf(TEXT("Place %s parent link is stale; use Refresh Hierarchy Links"),
					*ChildPlace->GetName()));
		}
	}
	else if (UTerritoryCityDefinition* City = Cast<UTerritoryCityDefinition>(Definition))
	{
		TSet<FGameplayTag> DistrictTags;
		TSet<FGameplayTag> PlaceTags;
		TSet<FGuid> HierarchyGUIDs;
		HierarchyGUIDs.Add(City->StableTerritoryGUID);
		for (UTerritoryDistrictDefinition* ChildDistrict : City->Districts)
		{
			if (!ChildDistrict) { Error(TEXT("Districts contains a null asset")); continue; }
			if (DistrictTags.Contains(ChildDistrict->TerritoryTag)) Error(FString::Printf(
				TEXT("Duplicate District tag in City: %s"), *ChildDistrict->TerritoryTag.ToString()));
			DistrictTags.Add(ChildDistrict->TerritoryTag);
			if (HierarchyGUIDs.Contains(ChildDistrict->StableTerritoryGUID)) Error(FString::Printf(
				TEXT("District %s duplicates a Territory save GUID in this City"),
				*ChildDistrict->GetName()));
			HierarchyGUIDs.Add(ChildDistrict->StableTerritoryGUID);
			if (ChildDistrict->DerivedParentTerritoryTag != City->TerritoryTag)
				Error(FString::Printf(TEXT("District %s parent link is stale; use Refresh Hierarchy Links"),
					*ChildDistrict->GetName()));
			for (UTerritoryPlaceDefinition* ChildPlace : ChildDistrict->Places)
			{
				if (!ChildPlace) continue;
				if (PlaceTags.Contains(ChildPlace->TerritoryTag)) Error(FString::Printf(
					TEXT("Place tag appears in more than one District: %s"),
					*ChildPlace->TerritoryTag.ToString()));
				PlaceTags.Add(ChildPlace->TerritoryTag);
				if (HierarchyGUIDs.Contains(ChildPlace->StableTerritoryGUID)) Error(FString::Printf(
					TEXT("Place %s duplicates a Territory save GUID in this City"),
					*ChildPlace->GetName()));
				HierarchyGUIDs.Add(ChildPlace->StableTerritoryGUID);
			}
		}
	}

	return OutErrors.IsEmpty();
}

void UTerritoryDataValidator::CheckDuplicateDisplayNames(ULevel* Level, TArray<FString>& OutWarnings)
{
	TMap<FString, FString> NameOwners;

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		FText DisplayName = Territory->GetTerritoryDisplayName();
		if (DisplayName.IsEmpty()) continue;

		FString NameStr = DisplayName.ToString();
		if (NameOwners.Contains(NameStr))
		{
			OutWarnings.Add(FString::Printf(TEXT("Duplicate TerritoryDisplayName '%s': used by '%s' and '%s'"),
				*NameStr, *NameOwners[NameStr], *Territory->GetActorLabel()));
		}
		else
		{
			NameOwners.Add(NameStr, Territory->GetActorLabel());
		}
	}
}

void UTerritoryDataValidator::CheckGuardConfig(
	ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Territory) return;

	FString Label = Territory->GetActorLabel();
	const bool bHasNPCDef = Territory->GuardNPCDefinition != nullptr;
	bool bHasFactionNPCDef = false;
	for (const FTerritoryFactionGuardDefinition& Definition : Territory->FactionGuardDefinitions)
	{
		bHasFactionNPCDef |= Definition.NPCDefinition != nullptr;
	}
	const bool bHasSpawnCount = Territory->GuardSpawnCount > 0;

	// Spawn count > 0 but no NPC definition
	if (bHasSpawnCount && !bHasNPCDef && !bHasFactionNPCDef)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: GuardSpawnCount=%d but no default or per-faction NPC definition — SpawnGuards will no-op"), *Label, Territory->GuardSpawnCount));
	}

	const int32 RequiredInstances = FMath::Max(
		1, FMath::Max(Territory->GuardSpawnCount, Territory->GuardSpawnPoints.Num()));
	auto ValidateDefinition = [&OutErrors, &Label, RequiredInstances](
		UNPCDefinition* Definition, const FString& Context)
	{
		if (!Definition) return;
		FText FailureReason;
		if (!ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
			Definition, RequiredInstances, FailureReason))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s is not physically spawn-ready: %s"),
				*Label, *Context, *FailureReason.ToString()));
		}
	};
	ValidateDefinition(Territory->GuardNPCDefinition, TEXT("default guard definition"));

	TSet<FGameplayTag> SeenFactions;
	for (const FTerritoryFactionGuardDefinition& Definition : Territory->FactionGuardDefinitions)
	{
		if (!Definition.Faction.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: per-faction guard definition has no faction tag"), *Label));
		}
		else if (SeenFactions.Contains(Definition.Faction))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: duplicate guard definition for faction %s"),
				*Label, *Definition.Faction.ToString()));
		}
		SeenFactions.Add(Definition.Faction);
		if (!Definition.NPCDefinition)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: faction %s has no guard NPC definition"),
				*Label, *Definition.Faction.ToString()));
		}
		ValidateDefinition(Definition.NPCDefinition, FString::Printf(TEXT("guard definition for %s"),
			*Definition.Faction.ToString()));
	}

	// Typed references can still contain deleted/null actors.
	for (int32 i = 0; i < Territory->GuardSpawnPoints.Num(); ++i)
	{
		ATerritoryGuardSpawnPoint* SpawnPoint = Territory->GuardSpawnPoints[i];
		if (!SpawnPoint)
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: GuardSpawnPoints[%d] is null"), *Label, i));
			continue;
		}
		UNPCDefinition* OverrideDefinition = SpawnPoint->NPCDefinitionOverride;
		if (!OverrideDefinition && SpawnPoint->GuardPostDefinition)
		{
			OverrideDefinition = SpawnPoint->GuardPostDefinition->NPCDefinition;
		}
		ValidateDefinition(OverrideDefinition, FString::Printf(
			TEXT("guard spawn point %s definition"), *SpawnPoint->GetActorLabel()));
	}
}

void UTerritoryDataValidator::CheckCounterAttackConfig(
	ATerritoryVolume* Territory, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Territory) return;
	const FString Label = Territory->GetActorLabel();
	UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const TArray<FTerritoryAssaultApproach>& Approaches = Territory->GetCounterAttackApproaches();

	if (!Profile)
	{
		if (!Approaches.IsEmpty())
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: assault approaches are configured but CounterAttackProfile is null"), *Label));
		}
		return;
	}

	if (!Territory->GetTerritoryGUID().IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: counterattacks require a stable TerritoryGUID"), *Label));
	}
	if (Profile->FactionForces.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: counterattack profile has no faction force definitions"), *Label));
	}
	ValidateCounterAttackProfileBounds(Profile, Label, OutErrors);

	TSet<FGameplayTag> SeenFactions;
	for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
	{
		if (!Force.Faction.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: counterattack force has no faction tag"), *Label));
		}
		else if (SeenFactions.Contains(Force.Faction))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: duplicate counterattack force for %s"),
				*Label, *Force.Faction.ToString()));
		}
		SeenFactions.Add(Force.Faction);
		if (!Force.AttackerDefinition)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: counterattack force %s has no Narrative NPC definition"),
				*Label, *Force.Faction.ToString()));
		}
		else
		{
			FText SpawnClassFailure;
			if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
				Force.AttackerDefinition, Force.PlannedForce, SpawnClassFailure))
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: counterattack force %s NPC class is not physically spawn-ready: %s"),
					*Label, *Force.Faction.ToString(), *SpawnClassFailure.ToString()));
			}
		}
		if (Force.PlannedForce <= 0 || Force.WaveSize <= 0 || Force.WaveSize > Force.PlannedForce)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: counterattack force %s has invalid planned force/wave size"),
				*Label, *Force.Faction.ToString()));
		}
		if (Force.MilitaryPower <= 0.f)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: counterattack force %s must have positive military power"),
				*Label, *Force.Faction.ToString()));
		}
		if (!FMath::IsWithinInclusive(Force.TerritorialInfluence, 0.f, 1.f))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: counterattack force %s TerritorialInfluence must be between zero and one"),
				*Label, *Force.Faction.ToString()));
		}
		ValidateCounterAttackForceSchedule(Force, Label, OutErrors);
		ValidateCounterAttackFactionVehicles(Force, Label, OutErrors);
		ValidateCounterAttackPowerScaling(Force, Label, OutErrors, OutWarnings);
	}

	TSet<FName> SeenApproachIDs;
	int32 EnabledApproaches = 0;
	UWorld* ValidationWorld = Territory->GetWorld();
	UNavigationSystemV1* Navigation = ValidationWorld
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(ValidationWorld) : nullptr;
	for (const FTerritoryAssaultApproach& Approach : Approaches)
	{
		if (!Approach.bEnabled) continue;
		++EnabledApproaches;
		if (Approach.ApproachID.IsNone())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: enabled counterattack approach has no ApproachID"), *Label));
		}
		else if (SeenApproachIDs.Contains(Approach.ApproachID))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: duplicate counterattack ApproachID '%s'"),
				*Label, *Approach.ApproachID.ToString()));
		}
		SeenApproachIDs.Add(Approach.ApproachID);
		if (Approach.MaxWaveSize <= 0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: approach '%s' has invalid MaxWaveSize"),
				*Label, *Approach.ApproachID.ToString()));
		}
		if (Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle)
		{
			for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
			{
				UClass* VehicleClass =
					UTerritoryCounterAttackProfile::ResolveVehicleClass(
						Force, Approach).LoadSynchronous();
				if (!VehicleClass
					|| !VehicleClass->IsChildOf(ANarrativeVehicleBase::StaticClass()))
				{
					OutErrors.Add(FString::Printf(
						TEXT("%s: vehicle approach '%s' needs either a fallback car or a valid signature car for faction %s"),
						*Label, *Approach.ApproachID.ToString(),
						*Force.Faction.ToString()));
				}
			}
			if (Approach.MaximumVehicleDeployments <= 0
				|| !FMath::IsFinite(Approach.VehicleMaximumDriveSpeed)
				|| Approach.VehicleMaximumDriveSpeed < 0.f
				|| Approach.VehicleAwareness.BrakingDistance
					<= Approach.VehicleAwareness.EmergencyStopDistance
				|| Approach.VehicleRetirement.HardRetirementTimeout
					<= Approach.VehicleRetirement.EarliestRetirementDelay)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: vehicle approach '%s' has invalid deployment, drive, awareness, or retirement settings"),
					*Label, *Approach.ApproachID.ToString()));
			}
		}
		if (!Navigation)
		{
			OutWarnings.AddUnique(FString::Printf(
				TEXT("%s: no built navmesh is available, so counterattack routes could not be verified"),
				*Label));
			continue;
		}

		FTransform WorldTransform =
			Approach.RelativeSpawnTransform * Territory->GetActorTransform();
		FTransform FootDeploymentTransform = WorldTransform;
		if (Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle)
		{
			const FName RequestedGuideID = Approach.RoadGuideID.IsNone()
				? Approach.ApproachID : Approach.RoadGuideID;
			ATerritoryRoadGuide* RoadGuide = nullptr;
			int32 MatchingGuideCount = 0;
			for (TActorIterator<ATerritoryRoadGuide> It(ValidationWorld); It; ++It)
			{
				if (It->GetRoadGuideID() == RequestedGuideID)
				{
					++MatchingGuideCount;
					if (!RoadGuide || It->GetPathName() < RoadGuide->GetPathName())
					{
						RoadGuide = *It;
					}
				}
			}
			if (MatchingGuideCount > 1)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: vehicle approach '%s' resolves to %d Road Guides named '%s'; Road Guide IDs must be level-wide unique"),
					*Label, *Approach.ApproachID.ToString(), MatchingGuideCount,
					*RequestedGuideID.ToString()));
			}
			if (Approach.RoadGuideID.IsNone() == false && !RoadGuide)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: vehicle approach '%s' explicitly references missing Road Guide '%s'"),
					*Label, *Approach.ApproachID.ToString(),
					*Approach.RoadGuideID.ToString()));
			}
			if (RoadGuide)
			{
				FText GuideFailure;
				if (!RoadGuide->ValidateRoadGuide(GuideFailure))
				{
					OutErrors.Add(FString::Printf(
						TEXT("%s: Road Guide '%s' for approach '%s' is invalid: %s"),
						*Label, *RequestedGuideID.ToString(),
						*Approach.ApproachID.ToString(),
						*GuideFailure.ToString()));
				}
				WorldTransform = RoadGuide->GetRouteStartTransform(
					false, Approach.RoadLaneSide);
				FootDeploymentTransform = RoadGuide->GetRouteEndTransform(
					false, Approach.RoadLaneSide);
			}
			else
			{
				FootDeploymentTransform = Approach.RelativeVehicleDropOffTransform
					* Territory->GetActorTransform();
			}
			if (!RoadGuide && !WorldTransform.GetLocation().Equals(
				FootDeploymentTransform.GetLocation(), 100.f))
			{
				if (!UTerritoryCounterAttackSubsystem::ValidateNarrativeVehicleRoute(
					ValidationWorld,
					WorldTransform.GetLocation(), FootDeploymentTransform.GetLocation(),
					nullptr))
				{
					OutErrors.Add(FString::Printf(
						TEXT("%s: vehicle approach '%s' has no complete Narrative ZoneGraph route from spawn to drop-off"),
						*Label, *Approach.ApproachID.ToString()));
				}
			}
		}
		FString RouteFailure = TEXT("No shared Territory defence objective is reachable");
		bool bHasRoute = false;
		for (const FVector& Objective :
			TerritoryAssaultTargetPolicy::BuildObjectiveLocations(Territory, true))
		{
			if (UTerritoryCounterAttackSubsystem::ValidateNavigationRoute(
				ValidationWorld, FootDeploymentTransform.GetLocation(), Objective,
				&RouteFailure))
			{
				bHasRoute = true;
				break;
			}
		}
		if (!bHasRoute)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: counterattack approach '%s' is not a usable physical route at %s: %s"),
				*Label, *Approach.ApproachID.ToString(),
				*WorldTransform.GetLocation().ToCompactString(), *RouteFailure));
		}
	}
	if (EnabledApproaches == 0)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: counterattacks require at least one enabled approach"), *Label));
	}
}

void UTerritoryDataValidator::CheckBoundsShape(ATerritoryVolume* Territory, TArray<FString>& OutWarnings)
{
	if (!Territory) return;

	if (!Territory->BoundsShape)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: BoundsShape is null — ContainsPoint and spatial index will not work"),
			*Territory->GetActorLabel()));
	}
}

void UTerritoryDataValidator::CheckOrphanedSpawnPoints(ULevel* Level, TArray<FString>& OutWarnings)
{
	if (!Level) return;

	// A post may be connected by the typed Territory array or by its stable owner tag.
	const TArray<ATerritoryVolume*> Territories = GetActorsForValidation<ATerritoryVolume>(Level);
	const TArray<ATerritoryGuardSpawnPoint*> SpawnPoints =
		GetActorsForValidation<ATerritoryGuardSpawnPoint>(Level);
	TSet<AActor*> ReferencedSpawnPoints;
	TMap<FGameplayTag, ATerritoryVolume*> TerritoriesByTag;
	for (ATerritoryVolume* Territory : Territories)
	{
		if (Territory->GetTerritoryTag().IsValid())
		{
			TerritoriesByTag.Add(Territory->GetTerritoryTag(), Territory);
		}
		for (AActor* SP : Territory->GuardSpawnPoints)
		{
			if (SP) ReferencedSpawnPoints.Add(SP);
		}
	}

	// Find orphaned spawn points
	for (ATerritoryGuardSpawnPoint* SP : SpawnPoints)
	{
		if (ReferencedSpawnPoints.Contains(SP)) continue;
		if (SP->OwnerTerritoryTag.IsValid())
		{
			if (ATerritoryVolume* const* Resolved =
				TerritoriesByTag.Find(SP->OwnerTerritoryTag))
			{
				if ((*Resolved)->IsA<ATerritoryProperty>()) continue;
				OutWarnings.Add(FString::Printf(
					TEXT("GuardSpawnPoint '%s' targets aggregate Territory '%s'; guard posts belong to Place Definitions only"),
					*SP->GetActorLabel(), *SP->OwnerTerritoryTag.ToString()));
				continue;
			}
			OutWarnings.Add(FString::Printf(
				TEXT("GuardSpawnPoint '%s' OwnerTerritoryTag '%s' does not resolve to a loaded territory"),
				*SP->GetActorLabel(), *SP->OwnerTerritoryTag.ToString()));
		}
		else
		{
			TArray<ATerritoryVolume*> PlacementHits;
			for (ATerritoryVolume* Territory : Territories)
			{
				if (!Territory) continue;
				if (Territory->ContainsPoint(SP->GetActorLocation())) PlacementHits.Add(Territory);
				for (const FTerritoryPatrolNode& Node : SP->GetEffectivePatrolRoute())
				{
					if (Territory->ContainsPoint(Node.Location)) PlacementHits.Add(Territory);
				}
			}
			if (!ATerritoryGuardSpawnPoint::ChooseMostSpecificTerritory(PlacementHits))
			{
				OutWarnings.Add(FString::Printf(TEXT("Orphaned GuardSpawnPoint '%s' — bind it through a Place Definition or overlap its placement/patrol route with a Place"),
					*SP->GetActorLabel()));
			}
		}
	}

	// Capacity is physical. Warn when authored initial staffing cannot ever deploy
	// because the Territory has no explicit, tag-bound, or contained point.
	for (ATerritoryVolume* Territory : Territories)
	{
		if (!Territory || !Territory->IsA<ATerritoryProperty>()
			|| Territory->GuardSpawnCount <= 0) continue;
		bool bHasPhysicalSlot = false;
		for (AActor* ReferencedPoint : Territory->GuardSpawnPoints)
		{
			bHasPhysicalSlot |= IsValid(ReferencedPoint);
		}
		for (ATerritoryGuardSpawnPoint* SpawnPoint : SpawnPoints)
		{
			if (!SpawnPoint || bHasPhysicalSlot) continue;
			if (SpawnPoint->OwnerTerritoryTag.IsValid())
			{
				bHasPhysicalSlot = SpawnPoint->OwnerTerritoryTag == Territory->GetTerritoryTag();
			}
			else
			{
				bHasPhysicalSlot = Territory->ContainsPoint(SpawnPoint->GetActorLocation());
				for (const FTerritoryPatrolNode& Node : SpawnPoint->GetEffectivePatrolRoute())
				{
					bHasPhysicalSlot |= Territory->ContainsPoint(Node.Location);
				}
			}
		}
		if (!bHasPhysicalSlot)
		{
			OutWarnings.Add(FString::Printf(
				TEXT("%s: GuardSpawnCount=%d but no guard spawn-point actor resolves to this Territory; active capacity is zero"),
				*Territory->GetActorLabel(), Territory->GuardSpawnCount));
		}
	}
}

void UTerritoryDataValidator::CheckMissingParentTags(ULevel* Level, TArray<FString>& OutWarnings)
{
	if (!Level) return;

	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		FString Label = Territory->GetActorLabel();

		// Districts should have a parent city tag
		if (Territory->IsA(ATerritoryDistrict::StaticClass()))
		{
			FGameplayTag ParentTag = Territory->GetParentTerritoryTag();
			if (!ParentTag.IsValid())
			{
				OutWarnings.Add(FString::Printf(TEXT("%s: District has no ParentTerritoryTag set"), *Label));
			}
		}

		// Properties should have a parent district tag
		if (Territory->IsA(ATerritoryProperty::StaticClass()))
		{
			FGameplayTag ParentTag = Territory->GetParentTerritoryTag();
			if (!ParentTag.IsValid())
			{
				OutWarnings.Add(FString::Printf(TEXT("%s: Property has no ParentTerritoryTag set"), *Label));
			}
		}

		// Cities should NOT have a parent tag (they are top-level)
		if (Territory->IsA(ATerritoryCity::StaticClass()))
		{
			FGameplayTag ParentTag = Territory->GetParentTerritoryTag();
			if (ParentTag.IsValid())
			{
				OutWarnings.Add(FString::Printf(TEXT("%s: City has ParentTerritoryTag set — cities should be top-level"), *Label));
			}
		}
	}
}
