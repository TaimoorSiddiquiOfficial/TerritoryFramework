#include "Core/TerritoryDeveloperSettings.h"

UTerritoryDeveloperSettings::UTerritoryDeveloperSettings()
{
	DefaultPlayerFaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
}
