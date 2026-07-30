#include "DataValidation/TerritoryDataValidator.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritorySavableData.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "AI/NPCDefinition.h"
#include "Components/ShapeComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

namespace
{
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

bool UTerritoryDataValidator::CanValidateAsset_Implementation(UObject* InAsset) const
{
	if (!InAsset) return false;

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
		InAsset->IsA(UTerritoryCounterAttackProfile::StaticClass()))
	{
		return true;
	}

	return false;
}

EDataValidationResult UTerritoryDataValidator::ValidateLoadedAsset_Implementation(
	UObject* InAsset, TArray<FText>& ValidationErrors)
{
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
				UClass* NPCClass = Force.AttackerDefinition->NPCClassPath.LoadSynchronous();
				if (!NPCClass || !NPCClass->IsChildOf(ATerritoryAssaultCharacter::StaticClass()))
				{
					Errors.Add(FString::Printf(TEXT("Counterattack force %s NPC class must derive from ATerritoryAssaultCharacter"),
						*Force.Faction.ToString()));
				}
			}
			if (Force.PlannedForce <= 0 || Force.WaveSize <= 0 || Force.WaveSize > Force.PlannedForce)
			{
				Errors.Add(FString::Printf(TEXT("Counterattack force %s has invalid planned force/wave size"),
					*Force.Faction.ToString()));
			}
		}
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
		ValidationErrors.Add(FText::FromString(Error));
	}

	// Emit warnings — UE 5.7 ValidateLoadedAsset only has ValidationErrors,
	// so we append warnings as non-blocking entries with a [WARNING] prefix.
	// Data Validation UI shows all ValidationErrors but only treats the
	// returned result as pass/fail.
	for (const FString& Warning : Warnings)
	{
		ValidationErrors.Add(FText::FromString(TEXT("[WARNING] ") + Warning));
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
	const bool bHasSpawnCount = Territory->GuardSpawnCount > 0;

	// Spawn count > 0 but no NPC definition
	if (bHasSpawnCount && !bHasNPCDef)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: GuardSpawnCount=%d but no GuardNPCDefinition — SpawnGuards will no-op"), *Label, Territory->GuardSpawnCount));
	}

	auto ValidateDefinition = [&OutErrors, &Label](UNPCDefinition* Definition, const FString& Context)
	{
		if (!Definition) return;
		UClass* NPCClass = Definition->NPCClassPath.LoadSynchronous();
		if (!NPCClass || !NPCClass->IsChildOf(ATerritoryGuardCharacter::StaticClass()))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s NPC class must derive from ATerritoryGuardCharacter"),
				*Label, *Context));
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
		if (!Territory->GuardSpawnPoints[i])
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: GuardSpawnPoints[%d] is null"), *Label, i));
		}
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
	if (Profile->MinimumLaunchProbability > Profile->MaximumLaunchProbability)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: counterattack minimum launch probability exceeds maximum"), *Label));
	}

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
			UClass* NPCClass = Force.AttackerDefinition->NPCClassPath.LoadSynchronous();
			if (!NPCClass || !NPCClass->IsChildOf(ATerritoryAssaultCharacter::StaticClass()))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: counterattack force %s NPC class must derive from ATerritoryAssaultCharacter"),
					*Label, *Force.Faction.ToString()));
			}
		}
		if (Force.PlannedForce <= 0 || Force.WaveSize <= 0 || Force.WaveSize > Force.PlannedForce)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: counterattack force %s has invalid planned force/wave size"),
				*Label, *Force.Faction.ToString()));
		}
	}

	TSet<FName> SeenApproachIDs;
	int32 EnabledApproaches = 0;
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

	// Check each spawn point — does any territory reference it?
	TSet<AActor*> ReferencedSpawnPoints;
	for (ATerritoryVolume* Territory : GetActorsForValidation<ATerritoryVolume>(Level))
	{
		for (AActor* SP : Territory->GuardSpawnPoints)
		{
			if (SP) ReferencedSpawnPoints.Add(SP);
		}
	}

	// Find orphaned spawn points
	for (ATerritoryGuardSpawnPoint* SP : GetActorsForValidation<ATerritoryGuardSpawnPoint>(Level))
	{
		if (!ReferencedSpawnPoints.Contains(SP))
		{
			OutWarnings.Add(FString::Printf(TEXT("Orphaned GuardSpawnPoint '%s' — not referenced by any territory"),
				*SP->GetActorLabel()));
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
