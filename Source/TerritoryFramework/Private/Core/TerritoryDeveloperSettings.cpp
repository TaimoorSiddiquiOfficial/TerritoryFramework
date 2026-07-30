#include "Core/TerritoryDeveloperSettings.h"

UTerritoryDeveloperSettings::UTerritoryDeveloperSettings()
{
	DefaultPlayerFaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
	DefaultNarrativeButtonClass = TSoftClassPtr<UNarrativeCommonButtonBase>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/UI/Widgets/Base/WBP_NarrativeButton_Text.WBP_NarrativeButton_Text_C")));
}
