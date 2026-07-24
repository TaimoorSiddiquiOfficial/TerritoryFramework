#include "DataValidation/TerritoryDataValidator.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritorySavableData.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Components/ShapeComponent.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"

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
		for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
		{
			return true;
		}
	}

	// Also validate individual territory-related assets
	if (InAsset->IsA(ATerritoryVolume::StaticClass()) ||
		InAsset->IsA(ATerritoryWorldState::StaticClass()) ||
		InAsset->IsA(ATerritorySavableData::StaticClass()))
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

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (Territory && Territory->GetLevel() == Level)
		{
			ValidateTerritory(Territory, OutErrors, OutWarnings);
		}
	}

	return OutErrors.Num() == 0;
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

	// Check guard config
	CheckGuardConfig(Territory, OutWarnings);

	return OutErrors.Num() == 0;
}

void UTerritoryDataValidator::CheckDuplicateTags(ULevel* Level, TArray<FString>& OutErrors)
{
	TMap<FGameplayTag, FString> TagOwners;

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

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

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

		FGuid GUID = Territory->GetActorGUID_Implementation();
		if (!GUID.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: TerritoryGUID is invalid (will cause save/load failures)"),
				*Territory->GetActorLabel()));
			continue;
		}

		if (GUIDOwners.Contains(GUID))
		{
			OutErrors.Add(FString::Printf(TEXT("DUPLICATE GUID '%s': both '%s' and '%s' use it"),
				*GUID.ToString(), *GUIDOwners[GUID], *Territory->GetActorLabel()));
		}
		else
		{
			GUIDOwners.Add(GUID, Territory->GetActorLabel());
		}
	}
}

void UTerritoryDataValidator::CheckHierarchyIntegrity(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	TSet<FGameplayTag> AllTags;
	TMap<FGameplayTag, UClass*> TagToClass;

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

		FGameplayTag Tag = Territory->GetTerritoryTag();
		if (Tag.IsValid())
		{
			AllTags.Add(Tag);
			TagToClass.Add(Tag, Territory->GetClass());
		}
	}

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

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
				bool bTypeOK = true;

				if (Territory->IsA(ATerritoryDistrict::StaticClass()) && !ParentClass->IsChildOf(ATerritoryCity::StaticClass()))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: District's parent '%s' is not a TerritoryCity"),
						*Label, *ParentTag.ToString()));
					bTypeOK = false;
				}
				if (Territory->IsA(ATerritoryProperty::StaticClass()) && !ParentClass->IsChildOf(ATerritoryDistrict::StaticClass()))
				{
					OutErrors.Add(FString::Printf(TEXT("%s: Property's parent '%s' is not a TerritoryDistrict"),
						*Label, *ParentTag.ToString()));
					bTypeOK = false;
				}
			}
		}
	}
}

void UTerritoryDataValidator::CheckSingletonActors(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	int32 WorldStateCount = 0;
	int32 SavableDataCount = 0;

	for (TActorIterator<AActor> It(Level->GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->GetLevel() != Level) continue;

		if (Actor->IsA(ATerritoryWorldState::StaticClass()))
		{
			WorldStateCount++;
		}
		if (Actor->IsA(ATerritorySavableData::StaticClass()))
		{
			SavableDataCount++;
		}
	}

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

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

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

void UTerritoryDataValidator::CheckGuardConfig(ATerritoryVolume* Territory, TArray<FString>& OutWarnings)
{
	if (!Territory) return;

	FString Label = Territory->GetActorLabel();

	bool bHasNPCDef = Territory->GuardNPCDefinition != nullptr;
	bool bHasSpawnCount = Territory->GuardSpawnCount > 0;
	bool bHasSpawnPoints = Territory->GuardSpawnPoints.Num() > 0;

	// Spawn count > 0 but no NPC definition
	if (bHasSpawnCount && !bHasNPCDef)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: GuardSpawnCount=%d but no GuardNPCDefinition — SpawnGuards will no-op"), *Label, Territory->GuardSpawnCount));
	}

	// Spawn points reference non-spawn-point actors
	for (int32 i = 0; i < Territory->GuardSpawnPoints.Num(); ++i)
	{
		if (Territory->GuardSpawnPoints[i] && !Territory->GuardSpawnPoints[i]->IsA(ATerritoryGuardSpawnPoint::StaticClass()))
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: GuardSpawnPoints[%d] '%s' is not an ATerritoryGuardSpawnPoint"),
				*Label, i, *Territory->GuardSpawnPoints[i]->GetName()));
		}
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

	// Collect all territory tags present in the level
	TSet<FGameplayTag> AllTerritoryTags;
	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (Territory && Territory->GetLevel() == Level)
		{
			FGameplayTag Tag = Territory->GetTerritoryTag();
			if (Tag.IsValid()) AllTerritoryTags.Add(Tag);
		}
	}

	// Check each spawn point — does any territory reference it?
	TSet<AActor*> ReferencedSpawnPoints;
	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;
		for (AActor* SP : Territory->GuardSpawnPoints)
		{
			if (SP) ReferencedSpawnPoints.Add(SP);
		}
	}

	// Find orphaned spawn points
	for (TActorIterator<ATerritoryGuardSpawnPoint> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryGuardSpawnPoint* SP = *It;
		if (!SP || SP->GetLevel() != Level) continue;

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

	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

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
