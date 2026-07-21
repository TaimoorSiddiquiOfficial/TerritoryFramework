#include "DataValidation/TerritoryDataValidator.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"

void UTerritoryDataValidator::Register()
{
	// TODO: Register with EditorValidatorSubsystem when available
	// For now, validators are called manually or via editor utility
	UE_LOG(LogTemp, Log, TEXT("TerritoryDataValidator registered"));
}

void UTerritoryDataValidator::Unregister()
{
	UE_LOG(LogTemp, Log, TEXT("TerritoryDataValidator unregistered"));
}

bool UTerritoryDataValidator::ValidateLevel(ULevel* Level, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Level) return false;

	// Run all checks
	CheckDuplicateTags(Level, OutErrors);
	CheckDuplicateGUIDs(Level, OutErrors);
	CheckHierarchyIntegrity(Level, OutErrors, OutWarnings);

	// Validate individual territories
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
	bool bValid = true;

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

	// Check for invalid initial faction (warning only — some territories may start unclaimed)
	FGameplayTag InitialFaction = Territory->GetOwningFaction();
	if (InitialFaction.IsValid())
	{
		// Verify it starts with Narrative.Factions
		if (!InitialFaction.ToString().StartsWith(TEXT("Narrative.Factions")))
		{
			OutWarnings.Add(FString::Printf(TEXT("%s: InitialOwningFaction '%s' doesn't start with Narrative.Factions"),
				*Label, *InitialFaction.ToString()));
		}
	}

	// Check guard spawn config (warning only)
	if (Territory->GetDefenderCount() == 0 && Territory->GetOwningFaction().IsValid())
	{
		OutWarnings.Add(FString::Printf(TEXT("%s: Owned territory has no registered defenders"),
			*Territory->GetActorLabel()));
	}

	return bValid;
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
	// Collect all tags
	TSet<FGameplayTag> AllTags;
	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

		FGameplayTag Tag = Territory->GetTerritoryTag();
		if (Tag.IsValid())
		{
			AllTags.Add(Tag);
		}
	}

	// Check parent references
	for (TActorIterator<ATerritoryVolume> It(Level->GetWorld()); It; ++It)
	{
		ATerritoryVolume* Territory = *It;
		if (!Territory || Territory->GetLevel() != Level) continue;

		FGameplayTag ParentTag = Territory->GetParentTerritoryTag();
		if (ParentTag.IsValid() && !AllTags.Contains(ParentTag))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: ParentTerritoryTag '%s' references a non-existent territory"),
				*Territory->GetActorLabel(), *ParentTag.ToString()));
		}

		// Check self-reference
		FGameplayTag SelfTag = Territory->GetTerritoryTag();
		if (ParentTag.IsValid() && ParentTag == SelfTag)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: ParentTerritoryTag references itself (cycle)"),
				*Territory->GetActorLabel()));
		}
	}
}
