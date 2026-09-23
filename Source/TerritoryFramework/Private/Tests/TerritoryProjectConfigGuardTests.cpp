#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CString.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"

/**
 * Guards project config that NarrativePro's setup merge can damage in the host project.
 *
 * Two distinct failures, both the same shape: the vendor setup templates under
 * Plugins/NarrativePro/Resources/IniSetups/Add/ are what an "Update Project Settings" click folds
 * into Config/*.ini, and neither failure leaves any other trace.
 *
 *  1. StartupMapsSurviveVendorSetupMerge -- UpdateIniValuesFromSource *replaces* a single-valued
 *     non-`+` key that differs, so the vendor's DefaultEngine.ini rewrites EditorStartupMap and
 *     GameEntryMap to its own demo map and the editor stops opening the project's district. The
 *     entire protection is one flag (bCheckProjectSettingsOnStartup=False); nothing else notices,
 *     because the ignore mechanism (Resources/IniSetups/Ignore) does not exist on disk.
 *
 *  2. SingleAssetManagerRulePerPrimaryAssetType -- `+` keys are appended, not replaced, so the
 *     vendor's DefaultGame.ini appends a second NPCDefinition and PlayerDefinition entry to the
 *     project's own. The engine's scan loop consumes every entry with no dedupe, so scan paths
 *     accumulate and the last entry's Rules win: the effective cook rule is decided by array order.
 *
 * Both assert the host project's own config, which is why they skip on any host other than TDA --
 * the same way TerritoryNarrativeProMigrationTests.cpp states its /Game/HopDistrictTest dependency
 * rather than failing on a host that legitimately lacks it.
 */
namespace TerritoryProjectConfigGuardTests
{
	/** The map TDA deliberately opens in the editor and enters the game through. */
	const TCHAR* HostProjectName = TEXT("TDA");
	const TCHAR* ProjectMap = TEXT("/Game/HopDistrictTest.HopDistrictTest");

	const TCHAR* GameMapsSettingsSection = TEXT("/Script/EngineSettings.GameMapsSettings");
	const TCHAR* ArsenalSettingsSection = TEXT("/Script/NarrativeArsenal.ArsenalSettings");

	/** The startup-map key and the NarrativePro game-entry key that an apply can rewrite. */
	const TCHAR* EditorStartupMapKey = TEXT("EditorStartupMap");
	const TCHAR* GameEntryMapKey = TEXT("GameEntryMap");

	/** The settings key that decides whether the vendor template is part of an apply at all. */
	const TCHAR* GateKey = TEXT("bCheckProjectSettingsOnStartup");

	FString VendorTemplatePath()
	{
		return FPaths::ProjectPluginsDir() / TEXT("NarrativePro/Resources/IniSetups/Add/DefaultEngine.ini");
	}

	/** Reads a key from the live config the engine actually uses, through GConfig rather than by
	 *  parsing text, so a key removed from the file reports the engine default instead of nothing. */
	bool ReadLiveEngineValue(const TCHAR* Section, const TCHAR* Key, FString& OutValue)
	{
		return GConfig != nullptr && GConfig->GetString(Section, Key, OutValue, GEngineIni);
	}

	/** Parses one key out of an ini file's text. Needed for the vendor template, because GConfig holds
	 *  the merged engine config and cannot report what a file that was never merged actually says. */
	bool ReadIniKeyFromText(const FString& IniText, const FString& Section, const FString& Key, FString& OutValue)
	{
		const FString SectionHeader = FString::Printf(TEXT("[%s]"), *Section);
		bool bInSection = false;

		TArray<FString> Lines;
		IniText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);
		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			// Skip comments, so a commented-out key reads as absent rather than as still declared.
			if (Line.IsEmpty() || Line.StartsWith(TEXT(";")))
			{
				continue;
			}
			if (Line.StartsWith(TEXT("[")))
			{
				bInSection = Line.Equals(SectionHeader, ESearchCase::CaseSensitive);
				continue;
			}
			if (bInSection && Line.StartsWith(Key + TEXT("="), ESearchCase::CaseSensitive))
			{
				OutValue = Line.Mid(Key.Len() + 1);
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryHostProjectStartupMapsSurviveVendorSetupMerge,
	"TerritoryFramework.ProjectConfig.StartupMapsSurviveVendorSetupMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryHostProjectStartupMapsSurviveVendorSetupMerge::RunTest(const FString& Parameters)
{
	using namespace TerritoryProjectConfigGuardTests;

	// State the host dependency instead of failing on a host that is not TDA. Deliberately keyed on the
	// project name and not on the guarded map: keying it on the map would mean deleting the map turned
	// this guard into a silent skip, which is the one thing it must never do.
	if (FCString::Stricmp(FApp::GetProjectName(), HostProjectName) != 0)
	{
		AddInfo(FString::Printf(
			TEXT("Skipped: this guard asserts the %s host project's Config/DefaultEngine.ini; the host project is %s."),
			HostProjectName, FApp::GetProjectName()));
		return true;
	}

	// The host project's own values. These are the deliberate deviation from the vendor template, and
	// the whole reason this test exists.
	FString LiveStartupMap;
	FString LiveEntryMap;
	const bool bHasStartupMap = ReadLiveEngineValue(GameMapsSettingsSection, EditorStartupMapKey, LiveStartupMap);
	const bool bHasEntryMap = ReadLiveEngineValue(ArsenalSettingsSection, GameEntryMapKey, LiveEntryMap);

	TestTrue(TEXT("The project config declares EditorStartupMap"), bHasStartupMap);
	TestTrue(TEXT("The project config declares GameEntryMap"), bHasEntryMap);
	TestEqual(TEXT("The editor opens this project's district"), LiveStartupMap, FString(ProjectMap));
	TestEqual(TEXT("The game is entered through this project's district"), LiveEntryMap, FString(ProjectMap));

	// The flag is the entire protection, and it is deliberately False. With it False, CombineIniUpdates
	// never folds the vendor template into an apply, so neither key above has a source that could
	// overwrite it. Turning it on does not lose the maps by itself -- it makes the next click on
	// "Update Project Settings" (Project Settings > Narrative Arsenal) able to, and that click then
	// writes the template's own bCheckProjectSettingsOnStartup=False back, so the flag re-disables
	// itself as a side effect of the loss it enabled. That is why it must not be "tidied" on.
	bool bCheckProjectSettingsOnStartup = true;
	const bool bHasGate = GConfig != nullptr && GConfig->GetBool(
		ArsenalSettingsSection, GateKey, bCheckProjectSettingsOnStartup, GEngineIni);
	TestTrue(TEXT("The project config declares bCheckProjectSettingsOnStartup"), bHasGate);
	TestFalse(
		TEXT("bCheckProjectSettingsOnStartup is False: it is what keeps the vendor template, which "
			 "declares both maps as its own demo map, out of an apply. Turning it on lets one click on "
			 "Update Project Settings overwrite the maps, and that click turns it back off."),
		bCheckProjectSettingsOnStartup);

	// The add-on gate is already True, so an NP_ add-on template is folded into an apply on that path.
	// None declares the map keys today, which is what makes the gate above sufficient; assert that
	// rather than assume it, because a vendor update could add one.
	int32 AddOnTemplateFiles = 0;
	TArray<FString> AddOnTemplatesDeclaringMaps;
	{
		TArray<FString> AddOnDirectories;
		IFileManager::Get().FindFiles(AddOnDirectories, *(FPaths::ProjectPluginsDir() / TEXT("NP_*")),
			/*Files=*/false, /*Directories=*/true);

		for (const FString& AddOnDirectory : AddOnDirectories)
		{
			const FString AddPath = FPaths::ProjectPluginsDir() / AddOnDirectory / TEXT("Resources/IniSetups/Add");
			TArray<FString> TemplateFiles;
			IFileManager::Get().FindFiles(TemplateFiles, *(AddPath / TEXT("*.ini")), true, false);

			for (const FString& TemplateFile : TemplateFiles)
			{
				FString TemplateText;
				if (!FFileHelper::LoadFileToString(TemplateText, *(AddPath / TemplateFile)))
				{
					continue;
				}
				++AddOnTemplateFiles;
				if (TemplateText.Contains(EditorStartupMapKey, ESearchCase::CaseSensitive)
					|| TemplateText.Contains(GameEntryMapKey, ESearchCase::CaseSensitive))
				{
					AddOnTemplatesDeclaringMaps.AddUnique(AddOnDirectory / TemplateFile);
				}
			}
		}
	}
	// Guard against a scan that quietly found nothing, which would make the assertion below vacuous.
	TestTrue(TEXT("Add-on setup templates were found to scan"), AddOnTemplateFiles > 0);
	TestTrue(*FString::Printf(
		TEXT("No NP_ add-on setup template declares the guarded map keys (found in: %s)"),
		AddOnTemplatesDeclaringMaps.Num() > 0 ? *FString::Join(AddOnTemplatesDeclaringMaps, TEXT(", ")) : TEXT("none")),
		AddOnTemplatesDeclaringMaps.Num() == 0);

	// Assert the threat is still real, not merely that the maps are correct. If the vendor template
	// stops shipping these keys, this test's premise is gone and the guards above protect nothing --
	// worth saying out loud rather than passing silently. This is also what catches the template being
	// moved, which would otherwise make every read above quietly find nothing.
	const FString TemplatePath = VendorTemplatePath();
	FString TemplateText;
	if (!TestTrue(*FString::Printf(TEXT("The vendor setup template is readable at %s"), *TemplatePath),
		FFileHelper::LoadFileToString(TemplateText, *TemplatePath)))
	{
		return true;
	}

	FString TemplateStartupMap;
	FString TemplateEntryMap;
	TestTrue(TEXT("The vendor template still declares EditorStartupMap"),
		ReadIniKeyFromText(TemplateText, GameMapsSettingsSection, EditorStartupMapKey, TemplateStartupMap));
	TestTrue(TEXT("The vendor template still declares GameEntryMap"),
		ReadIniKeyFromText(TemplateText, ArsenalSettingsSection, GameEntryMapKey, TemplateEntryMap));
	TestNotEqual(TEXT("The vendor template's EditorStartupMap differs from this project's, so an apply "
					  "that included it would change the editor's startup map"),
		TemplateStartupMap, LiveStartupMap);
	TestNotEqual(TEXT("The vendor template's GameEntryMap differs from this project's, so an apply that "
					  "included it would change the game entry map"),
		TemplateEntryMap, LiveEntryMap);

	return true;
}

// ---------------------------------------------------------------------------------------------
// Guard 2: one AssetManager scan rule per PrimaryAssetType.
// ---------------------------------------------------------------------------------------------

namespace TerritoryProjectConfigGuardTests
{
	/** The two NarrativePro primary asset types the vendor setup template appends entries for. */
	const TCHAR* NpcDefinitionType = TEXT("NPCDefinition");
	const TCHAR* PlayerDefinitionType = TEXT("PlayerDefinition");

	/** The directories the vendor's appended entry scans, and the rule it applies to them. */
	const TCHAR* VendorScannedPaths[] = { TEXT("/Game"), TEXT("/NarrativePro") };
	const TCHAR* VendorCookRule = TEXT("AlwaysCook");

	FString VendorGameTemplatePath()
	{
		return FPaths::ProjectPluginsDir() / TEXT("NarrativePro/Resources/IniSetups/Add/DefaultGame.ini");
	}

	/** One `+PrimaryAssetTypesToScan=` declaration, parsed from ini text. */
	struct FScanDeclaration
	{
		FString Type;
		TArray<FString> Directories;
		FString CookRule;
	};

	/** Reads Key="value" out of a single ini line. */
	bool ExtractQuotedValue(const FString& Line, const FString& Key, FString& OutValue)
	{
		const FString Marker = Key + TEXT("=\"");
		const int32 MarkerStart = Line.Find(Marker, ESearchCase::CaseSensitive);
		if (MarkerStart == INDEX_NONE)
		{
			return false;
		}
		const int32 ValueStart = MarkerStart + Marker.Len();
		const int32 ValueEnd = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		if (ValueEnd == INDEX_NONE)
		{
			return false;
		}
		OutValue = Line.Mid(ValueStart, ValueEnd - ValueStart);
		return true;
	}

	/** Collects every (Path="...") inside a line's Directories field. */
	void ExtractDirectoryPaths(const FString& Line, TArray<FString>& OutPaths)
	{
		const int32 DirectoriesStart = Line.Find(TEXT("Directories="), ESearchCase::CaseSensitive);
		const int32 DirectoriesEnd = Line.Find(TEXT(",SpecificAssets="), ESearchCase::CaseSensitive);
		if (DirectoriesStart == INDEX_NONE || DirectoriesEnd <= DirectoriesStart)
		{
			return;
		}
		const FString Block = Line.Mid(DirectoriesStart, DirectoriesEnd - DirectoriesStart);

		int32 SearchFrom = 0;
		while (true)
		{
			const int32 PathStart = Block.Find(TEXT("Path=\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (PathStart == INDEX_NONE)
			{
				break;
			}
			const int32 ValueStart = PathStart + 6;
			const int32 ValueEnd = Block.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				break;
			}
			OutPaths.Add(Block.Mid(ValueStart, ValueEnd - ValueStart));
			SearchFrom = ValueEnd + 1;
		}
	}

	/** Reads the CookRule out of a line's trailing Rules=(...) block. */
	bool ExtractCookRule(const FString& Line, FString& OutCookRule)
	{
		const int32 RuleStart = Line.Find(TEXT("CookRule="), ESearchCase::CaseSensitive);
		if (RuleStart == INDEX_NONE)
		{
			return false;
		}
		const int32 ValueStart = RuleStart + 9;
		const int32 ValueEnd = Line.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		OutCookRule = (ValueEnd == INDEX_NONE) ? Line.Mid(ValueStart) : Line.Mid(ValueStart, ValueEnd - ValueStart);
		OutCookRule = OutCookRule.TrimStartAndEnd();
		return true;
	}

	/** Parses every `+PrimaryAssetTypesToScan=` line out of ini file text, for template files that
	 *  were never merged into GConfig and so cannot be read through it. */
	void ParseScanDeclarations(const FString& IniText, TArray<FScanDeclaration>& OutDeclarations)
	{
		TArray<FString> Lines;
		IniText.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (!Line.StartsWith(TEXT("+PrimaryAssetTypesToScan="), ESearchCase::CaseSensitive))
			{
				continue;
			}

			FScanDeclaration Declaration;
			if (!ExtractQuotedValue(Line, TEXT("PrimaryAssetType"), Declaration.Type))
			{
				continue;
			}
			ExtractDirectoryPaths(Line, Declaration.Directories);
			ExtractCookRule(Line, Declaration.CookRule);
			Declaration.Directories.Sort();
			OutDeclarations.Add(MoveTemp(Declaration));
		}
	}

	TArray<FString> SortedVendorPaths()
	{
		TArray<FString> Paths;
		for (const TCHAR* Path : VendorScannedPaths)
		{
			Paths.Add(Path);
		}
		Paths.Sort();
		return Paths;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySingleAssetManagerRulePerPrimaryAssetType,
	"TerritoryFramework.ProjectConfig.SingleAssetManagerRulePerPrimaryAssetType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySingleAssetManagerRulePerPrimaryAssetType::RunTest(const FString& Parameters)
{
	using namespace TerritoryProjectConfigGuardTests;

	// State the host dependency instead of failing on a host that is not TDA.
	if (FCString::Stricmp(FApp::GetProjectName(), HostProjectName) != 0)
	{
		AddInfo(FString::Printf(
			TEXT("Skipped: this guard asserts the %s host project's Config/DefaultGame.ini; the host project is %s."),
			HostProjectName, FApp::GetProjectName()));
		return true;
	}

	const UAssetManagerSettings* Settings = GetDefault<UAssetManagerSettings>();
	if (!TestNotNull(TEXT("Asset Manager settings are available"), Settings))
	{
		return true;
	}

	// This is the array UAssetManager::ScanPrimaryAssetTypesFromConfig iterates, so counting it is
	// counting what the engine will actually consume -- not what some other file happens to say.
	const TArray<FPrimaryAssetTypeInfo>& ScanList = Settings->PrimaryAssetTypesToScan;
	TestTrue(TEXT("The Asset Manager scan list declares at least one type"), ScanList.Num() > 0);

	// What the Asset Manager actually resolved. The config parser keeps one entry per type even when
	// the file declares a type twice -- measured 2026-09-23: the four +declarations in this project's
	// config for two types resolve to two entries, the LAST declaration of each type winning, with no
	// union of the losing entry's scan paths. So this count is normally 1 and the losing declarations
	// in the file are dead text. Asserted anyway: if an engine change stops collapsing them, the
	// repeated declarations become live and this is what says so.
	TMap<FName, int32> DeclarationsPerType;
	for (const FPrimaryAssetTypeInfo& TypeInfo : ScanList)
	{
		DeclarationsPerType.FindOrAdd(TypeInfo.PrimaryAssetType) += 1;
	}

	TArray<FString> DuplicatedTypes;
	for (const TPair<FName, int32>& Pair : DeclarationsPerType)
	{
		if (Pair.Value > 1)
		{
			DuplicatedTypes.Add(FString::Printf(TEXT("%s x%d"), *Pair.Key.ToString(), Pair.Value));
		}
	}
	DuplicatedTypes.Sort();

	// Positive controls first: a count of nothing must not be able to pass the assertions below.
	TestTrue(TEXT("The Asset Manager resolves NPCDefinition"),
		DeclarationsPerType.Contains(FName(NpcDefinitionType)));
	TestTrue(TEXT("The Asset Manager resolves PlayerDefinition"),
		DeclarationsPerType.Contains(FName(PlayerDefinitionType)));

	TestTrue(*FString::Printf(
		TEXT("The Asset Manager resolves each PrimaryAssetType exactly once. The scan loop consumes "
			 "every entry it is handed with no dedupe (AssetManager.cpp, ScanPrimaryAssetTypesFromConfig "
			 "calls ScanPathsForPrimaryAssets and SetPrimaryAssetTypeRules once per entry), so if the "
			 "config parser ever stops collapsing repeated declarations, a second entry would make scan "
			 "paths accumulate and the last entry's Rules win. Offenders: %s"),
		DuplicatedTypes.Num() > 0 ? *FString::Join(DuplicatedTypes, TEXT(", ")) : TEXT("none")),
		DuplicatedTypes.Num() == 0);

	// The defect this guard exists for: the file a human edits declares a type more than once. The
	// engine tolerates that silently -- it keeps the last declaration and discards the rest -- so the
	// losing declaration is dead text that reads exactly as live as the winning one, which is how a
	// careful edit to the wrong line comes to change nothing. Worse, the resolution is by position:
	// reordering the declarations flips the effective cook rule with no diagnostic at all.
	const FString LiveGameConfigPath = FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini");
	FString LiveGameConfigText;
	if (TestTrue(*FString::Printf(TEXT("The project's Config/DefaultGame.ini is readable at %s"), *LiveGameConfigPath),
		FFileHelper::LoadFileToString(LiveGameConfigText, *LiveGameConfigPath)))
	{
		TArray<FScanDeclaration> LiveDeclarations;
		ParseScanDeclarations(LiveGameConfigText, LiveDeclarations);

		TestTrue(*FString::Printf(
			TEXT("Config/DefaultGame.ini declares +PrimaryAssetTypesToScan entries at all (parsed %d)"),
			LiveDeclarations.Num()),
			LiveDeclarations.Num() > 0);

		TMap<FString, int32> DeclarationsInFile;
		for (const FScanDeclaration& Declaration : LiveDeclarations)
		{
			DeclarationsInFile.FindOrAdd(Declaration.Type) += 1;
		}

		TArray<FString> RepeatedTypes;
		for (const TPair<FString, int32>& Pair : DeclarationsInFile)
		{
			if (Pair.Value > 1)
			{
				RepeatedTypes.Add(FString::Printf(TEXT("%s x%d"), *Pair.Key, Pair.Value));
			}
		}
		RepeatedTypes.Sort();

		TestTrue(*FString::Printf(
			TEXT("Every PrimaryAssetType is declared exactly once in Config/DefaultGame.ini. A repeated "
				 "declaration is never reported: the engine keeps the last one and silently discards the "
				 "earlier ones, so they are dead text that reads as live. Offenders: %s"),
			RepeatedTypes.Num() > 0 ? *FString::Join(RepeatedTypes, TEXT(", ")) : TEXT("none")),
			RepeatedTypes.Num() == 0);
	}

	// Which declaration must be the survivor, pinned by value. The engine resolves a repeated
	// declaration by keeping the last one, so the vendor's entry is the effective one today; resolving
	// the duplication the other way -- deleting the vendor's pair -- would silently drop the cook rule
	// from AlwaysCook to Unknown, and this is the assertion that would notice.
	struct FExpectedType
	{
		const TCHAR* Type;
		const TCHAR* BaseClass;
	};
	const FExpectedType ExpectedTypes[] =
	{
		{ NpcDefinitionType,    TEXT("/Script/NarrativeArsenal.NPCDefinition") },
		{ PlayerDefinitionType, TEXT("/Script/NarrativeArsenal.PlayerDefinition") },
	};

	for (const FExpectedType& Expected : ExpectedTypes)
	{
		const FPrimaryAssetTypeInfo* Effective = nullptr;
		for (const FPrimaryAssetTypeInfo& TypeInfo : ScanList)
		{
			if (TypeInfo.PrimaryAssetType == FName(Expected.Type))
			{
				Effective = &TypeInfo;
			}
		}

		if (!TestNotNull(*FString::Printf(TEXT("The scan list declares %s"), Expected.Type), Effective))
		{
			continue;
		}

		TArray<FString> Directories;
		for (const FDirectoryPath& Directory : Effective->GetDirectories())
		{
			Directories.Add(Directory.Path);
		}
		Directories.Sort();

		TestEqual(*FString::Printf(TEXT("%s scans the vendor entry's directories"), Expected.Type),
			FString::Join(Directories, TEXT(", ")), FString::Join(SortedVendorPaths(), TEXT(", ")));
		TestTrue(*FString::Printf(
			TEXT("%s keeps CookRule=AlwaysCook, the rule in force today (the vendor's entry is last, so it wins)"),
			Expected.Type),
			Effective->Rules.CookRule == EPrimaryAssetCookRule::AlwaysCook);
		TestEqual(*FString::Printf(TEXT("%s keeps its NarrativeArsenal base class"), Expected.Type),
			Effective->GetAssetBaseClass().ToString(), FString(Expected.BaseClass));
		TestFalse(*FString::Printf(TEXT("%s is not editor-only"), Expected.Type), Effective->bIsEditorOnly);
	}

	// Premise liveness: the project is converged onto the vendor template's own text, which is what
	// makes a future "Update Project Settings" click a no-op for these types. If the template stops
	// shipping them, that reason is gone and this says so instead of passing quietly.
	FString TemplateText;
	if (TestTrue(*FString::Printf(TEXT("The vendor setup template is readable at %s"), *VendorGameTemplatePath()),
		FFileHelper::LoadFileToString(TemplateText, *VendorGameTemplatePath())))
	{
		TArray<FScanDeclaration> TemplateDeclarations;
		ParseScanDeclarations(TemplateText, TemplateDeclarations);

		TestTrue(*FString::Printf(
			TEXT("The vendor template declares +PrimaryAssetTypesToScan entries at all (parsed %d)"),
			TemplateDeclarations.Num()),
			TemplateDeclarations.Num() > 0);

		for (const FExpectedType& Expected : ExpectedTypes)
		{
			const FScanDeclaration* Found = nullptr;
			int32 MatchCount = 0;
			for (const FScanDeclaration& Declaration : TemplateDeclarations)
			{
				if (Declaration.Type == Expected.Type)
				{
					++MatchCount;
					Found = &Declaration;
				}
			}

			TestEqual(*FString::Printf(TEXT("The vendor template declares %s exactly once"), Expected.Type),
				MatchCount, 1);

			if (Found != nullptr)
			{
				TestEqual(*FString::Printf(TEXT("The vendor template's %s scans %s"), Expected.Type,
					*FString::Join(SortedVendorPaths(), TEXT(", "))),
					FString::Join(Found->Directories, TEXT(", ")), FString::Join(SortedVendorPaths(), TEXT(", ")));
				TestEqual(*FString::Printf(TEXT("The vendor template's %s sets CookRule=%s"), Expected.Type, VendorCookRule),
					Found->CookRule, FString(VendorCookRule));
			}
		}
	}

	// Observe the resolved state when the Asset Manager has scanned in this session: the engine's own
	// answer to which entry won, rather than a restatement of the assertions above. Reported as
	// skipped, never as passed, when the scan has not populated the type map.
	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		bool bObservedResolvedRule = false;
		for (const FExpectedType& Expected : ExpectedTypes)
		{
			FPrimaryAssetTypeInfo ResolvedInfo;
			if (AssetManager->GetPrimaryAssetTypeInfo(FPrimaryAssetType(Expected.Type), ResolvedInfo))
			{
				bObservedResolvedRule = true;
				TestTrue(*FString::Printf(TEXT("The Asset Manager resolved %s with CookRule=AlwaysCook"), Expected.Type),
					ResolvedInfo.Rules.CookRule == EPrimaryAssetCookRule::AlwaysCook);
			}
		}
		if (!bObservedResolvedRule)
		{
			AddInfo(TEXT("The Asset Manager has not scanned the primary asset types in this session, so the "
						 "resolved cook rule could not be observed; the config-level assertions above still apply."));
		}
	}
	else
	{
		AddInfo(TEXT("The Asset Manager is not initialized in this session; skipped observing the resolved cook rule."));
	}

	return true;
}

#endif
