#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameplayTagsManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryIncludedExampleTags,
	"TerritoryFramework.Content.ExampleTagsRegisteredAtStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryIncludedExampleTags::RunTest(const FString& Parameters)
{
	const TCHAR* Names[] = {
		TEXT("Territory.HavenReach"),
		TEXT("Territory.HavenReach.MarketSquare"),
		TEXT("Territory.HavenReach.CastleHill"),
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"),
		TEXT("Territory.HavenReach.CastleHill.Farm")
	};
	for (const TCHAR* Name : Names)
	{
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(Name), false);
		TestTrue(*FString::Printf(TEXT("Included identity %s is registered without project tag config"), Name), Tag.IsValid());
	}
	return true;
}

#endif
