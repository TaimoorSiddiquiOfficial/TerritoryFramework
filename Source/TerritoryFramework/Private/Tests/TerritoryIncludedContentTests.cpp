#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/**
 * Collects every tag declared in the plugin's own tag config directory, the same directory
 * FTerritoryFrameworkModule::StartupModule hands to
 * UGameplayTagsManager::AddTagIniSearchPath. The tests read that directory rather than repeating
 * a hand-written list, because a list is what drifted: see RunTest.
 */
static void CollectDeclaredExampleTags(TSet<FString>& OutTags, int32& OutFileCount)
{
	OutTags.Reset();
	OutFileCount = 0;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TerritoryFramework"));
	if (!Plugin.IsValid())
	{
		return;
	}

	const FString TagDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/Tags"));
	TArray<FString> TagFiles;
	IFileManager::Get().FindFiles(TagFiles, *(TagDirectory / TEXT("*.ini")), true, false);
	OutFileCount = TagFiles.Num();

	for (const FString& TagFile : TagFiles)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *(TagDirectory / TagFile)))
		{
			continue;
		}
		for (const FString& Line : Lines)
		{
			// Skip comments, as the ini parser and FileFilter.ReadRulesFromFile do, so a
			// commented-out tag reads as absent instead of as still declared.
			const FString TrimmedLine = Line.TrimStartAndEnd();
			if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT(";")))
			{
				continue;
			}
			// GameplayTagList=(Tag="A.B",DevComment="...")
			int32 TagStart = TrimmedLine.Find(TEXT("Tag=\""), ESearchCase::CaseSensitive);
			if (TagStart == INDEX_NONE)
			{
				continue;
			}
			TagStart += 5;
			const int32 TagEnd = TrimmedLine.Find(TEXT("\""), ESearchCase::CaseSensitive,
				ESearchDir::FromStart, TagStart);
			if (TagEnd != INDEX_NONE && TagEnd > TagStart)
			{
				OutTags.Add(TrimmedLine.Mid(TagStart, TagEnd - TagStart));
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryIncludedExampleTags,
	"TerritoryFramework.Content.ExampleTagsRegisteredAtStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryIncludedExampleTags::RunTest(const FString& Parameters)
{
	TSet<FString> DeclaredTags;
	int32 TagFileCount = 0;
	CollectDeclaredExampleTags(DeclaredTags, TagFileCount);

	// Guard against a parse that quietly found nothing, which would make every assertion below
	// vacuous and leave the real content unasserted.
	TestTrue(TEXT("The plugin tag config directory is readable"), TagFileCount > 0);
	TestTrue(TEXT("The plugin tag config declares example identities"), DeclaredTags.Num() > 0);

	// Assert what the shipped config actually declares, so an identity added to the config is
	// covered whether or not anyone remembered to update this test.
	// Territory.HavenReach.MarketSquare.Warehouse is the identity that exposed that gap: it was
	// added to the config, this test kept a hand-written list of five, and the sixth tag was the
	// one nothing asserted.
	TArray<FString> SortedDeclaredTags = DeclaredTags.Array();
	SortedDeclaredTags.Sort();
	for (const FString& Name : SortedDeclaredTags)
	{
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Name), false);
		TestTrue(*FString::Printf(TEXT("Included identity %s is registered without project tag config"), *Name),
			Tag.IsValid());
	}

	// Reading the directory cannot prove the directory is still complete, so also pin the sample
	// identities the plugin documents as part of its included content. This is the direction the
	// directory sweep cannot cover: a tag removed from the config would otherwise simply stop
	// being asserted instead of failing.
	const TCHAR* DocumentedIdentities[] = {
		TEXT("Territory.HavenReach"),
		TEXT("Territory.HavenReach.MarketSquare"),
		TEXT("Territory.HavenReach.CastleHill"),
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"),
		TEXT("Territory.HavenReach.MarketSquare.Warehouse"),
		TEXT("Territory.HavenReach.CastleHill.Farm")
	};
	for (const TCHAR* Identity : DocumentedIdentities)
	{
		const FString IdentityName(Identity);
		TestTrue(*FString::Printf(TEXT("Sample identity %s is still declared by the plugin tag config"), Identity),
			DeclaredTags.Contains(IdentityName));
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(Identity), false);
		TestTrue(*FString::Printf(TEXT("Sample identity %s is registered"), Identity), Tag.IsValid());
	}
	return true;
}

#endif
