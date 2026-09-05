#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Story/TerritoryStoryOutcomeAnalyzer.h"
#include "Story/STerritoryStoryOutcomePanel.h"
#include "Tales/TerritoryOwnershipCondition.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/UnrealType.h"

namespace TerritoryStoryOutcomeTests
{
	const FTerritoryStoryOutcomeScenario* FindScenario(
		const FTerritoryStoryOutcomeReport& Report, const FString& Title)
	{
		return Report.Scenarios.FindByPredicate(
			[&Title](const FTerritoryStoryOutcomeScenario& Scenario)
			{
				return Scenario.Title == Title;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCaptureAuthoringModeNormalization,
	"TerritoryFramework.Editor.Definitions.CaptureModesAreMutuallyExclusive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCaptureAuthoringModeNormalization::RunTest(
	const FString& Parameters)
{
	UTerritoryPlaceDefinition* Place =
		NewObject<UTerritoryPlaceDefinition>();
	Place->CapturePoint.bEnabled = true;
	Place->CapturePoint.bAutomaticCapture = true;
	Place->bStoryCaptureFromBounds = true;
	FProperty* StoryProperty = FindFProperty<FProperty>(
		UTerritoryDefinition::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UTerritoryDefinition,
			bStoryCaptureFromBounds));
	FPropertyChangedEvent ChangeEvent(StoryProperty,
		EPropertyChangeType::ValueSet);
	Place->PostEditChangeProperty(ChangeEvent);
	TestTrue(TEXT("Story bounds remains the selected capture authority"),
		Place->bStoryCaptureFromBounds);
	TestFalse(TEXT("Story bounds turns off contradictory physical auto capture"),
		Place->CapturePoint.bAutomaticCapture);
	TestTrue(TEXT("The optional physical point may remain for presentation"),
		Place->CapturePoint.bEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryOutcomePlaceReport,
	"TerritoryFramework.Editor.StoryOutcome.PlaceReportIsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryOutcomePlaceReport::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Temp/TF_StoryOutcomePlace"));
	UTerritoryPlaceDefinition* Place = NewObject<UTerritoryPlaceDefinition>(
		Package, TEXT("DA_TestStoryPlace"), RF_Public | RF_Standalone);
	Place->DisplayName = FText::FromString(TEXT("Test Blacksmith"));
	Place->InitialAvailability = ETerritoryAvailability::Locked;
	Place->InitialState = ETerritoryInitialState::Claimed;
	Place->InitialOwningFaction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Place->bStoryCaptureFromBounds = true;
	Place->InitialGuardCount = 4;
	Package->SetDirtyFlag(false);

	const FTerritoryStoryOutcomeReport Report =
		FTerritoryStoryOutcomeAnalyzer::Analyze(Place, false);
	TestEqual(TEXT("Place report identifies its type"), Report.DefinitionType,
		FString(TEXT("Place")));
	TestTrue(TEXT("Place report contains several story branches"),
		Report.Scenarios.Num() >= 6);
	const FTerritoryStoryOutcomeScenario* Campaign =
		TerritoryStoryOutcomeTests::FindScenario(Report, TEXT("New campaign seed"));
	TestNotNull(TEXT("New-campaign branch exists"), Campaign);
	if (Campaign)
	{
		TestTrue(TEXT("New-campaign branch explains locked availability"),
			Campaign->Then.Contains(TEXT("Locked")));
		TestTrue(TEXT("New-campaign branch explains initial guards"),
			Campaign->Then.Contains(TEXT("4 initial guards")));
	}
	const FTerritoryStoryOutcomeScenario* Capture =
		TerritoryStoryOutcomeTests::FindScenario(
			Report, TEXT("Capture admission and progress"));
	TestNotNull(TEXT("Capture branch exists"), Capture);
	if (Capture)
	{
		TestTrue(TEXT("Story bounds explain automatic capture suppression"),
			Capture->Then.Contains(TEXT("Automatic Capture Point progress is disabled")));
	}
	int32 LockedAvailabilityRows = 0;
	for (const FTerritoryStoryOutcomeScenario& Scenario : Report.Scenarios)
	{
		LockedAvailabilityRows +=
			Scenario.Title == TEXT("Locked availability lifecycle") ? 1 : 0;
	}
	TestEqual(TEXT("Locked settings produce one consolidated availability row"),
		LockedAvailabilityRows, 1);
	TestNull(TEXT("Locked settings are not repeated as a political state row"),
		TerritoryStoryOutcomeTests::FindScenario(Report, TEXT("Locked lifecycle")));

	TSet<FString> UniqueOutcomes;
	for (const FTerritoryStoryOutcomeScenario& Scenario : Report.Scenarios)
	{
		UniqueOutcomes.Add(Scenario.Category + TEXT("\x1f")
			+ Scenario.Title + TEXT("\x1f") + Scenario.When + TEXT("\x1f")
			+ Scenario.OnlyIf + TEXT("\x1f") + Scenario.Then + TEXT("\x1f")
			+ Scenario.IfNot + TEXT("\x1f") + Scenario.AlsoAffects);
	}
	TestEqual(TEXT("The report contains no duplicate rendered outcomes"),
		UniqueOutcomes.Num(), Report.Scenarios.Num());
	TestFalse(TEXT("Generating a report never dirties its Definition package"),
		Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryOutcomeParentHierarchy,
	"TerritoryFramework.Editor.StoryOutcome.ParentHierarchyAndConditionWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryOutcomeParentHierarchy::RunTest(const FString& Parameters)
{
	UTerritoryDistrictDefinition* District =
		NewObject<UTerritoryDistrictDefinition>();
	District->DisplayName = FText::FromString(TEXT("Market Square"));
	UTerritoryPlaceDefinition* PlaceA = NewObject<UTerritoryPlaceDefinition>(District);
	PlaceA->DisplayName = FText::FromString(TEXT("Blacksmith"));
	UTerritoryPlaceDefinition* PlaceB = NewObject<UTerritoryPlaceDefinition>(District);
	PlaceB->DisplayName = FText::FromString(TEXT("Bakery"));
	District->Places = { PlaceA, PlaceB };

	FTerritoryStateConfig& Claimed =
		District->StateConfigs.FindOrAdd(ETerritoryState::Claimed);
	Claimed.EntryConditions.Add(NewObject<UTerritoryOwnershipCondition>(District));

	const FTerritoryStoryOutcomeReport Report =
		FTerritoryStoryOutcomeAnalyzer::Analyze(District, false);
	const FTerritoryStoryOutcomeScenario* Hierarchy =
		TerritoryStoryOutcomeTests::FindScenario(
			Report, TEXT("District control is reduced from Places"));
	TestNotNull(TEXT("District hierarchy branch exists"), Hierarchy);
	if (Hierarchy)
	{
		TestTrue(TEXT("District report counts its complete child set"),
			Hierarchy->Then.Contains(TEXT("All 2 Places")));
		TestTrue(TEXT("District report names its authored children"),
			Hierarchy->Then.Contains(TEXT("Blacksmith"))
			&& Hierarchy->Then.Contains(TEXT("Bakery")));
	}
	const FTerritoryStoryOutcomeScenario* ClaimedLifecycle =
		TerritoryStoryOutcomeTests::FindScenario(
			Report, TEXT("Claimed lifecycle"));
	TestNotNull(TEXT("Claimed parent lifecycle branch exists"), ClaimedLifecycle);
	if (ClaimedLifecycle)
	{
		TestEqual(TEXT("Parent political conditions are reported as an authoring warning"),
			ClaimedLifecycle->Certainty,
			ETerritoryStoryOutcomeCertainty::Warning);
		TestTrue(TEXT("Warning explains that hierarchy control cannot be blocked"),
			ClaimedLifecycle->IfNot.Contains(TEXT("cannot block the hierarchy reducer")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryOutcomeProductionAndCounter,
	"TerritoryFramework.Editor.StoryOutcome.ProductionAndCounterBranches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryOutcomeProductionAndCounter::RunTest(
	const FString& Parameters)
{
	UTerritoryPlaceDefinition* Place = NewObject<UTerritoryPlaceDefinition>();
	Place->DisplayName = FText::FromString(TEXT("Farm"));
	Place->ProductionProfile = NewObject<UTerritoryProductionProfile>(Place);
	FTerritoryProductionRule& Rule =
		Place->ProductionProfile->Rules.AddDefaulted_GetRef();
	Rule.DisplayName = FText::FromString(TEXT("Daily Grain"));
	Rule.MinimumUpgradeLevel = 2;

	Place->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>(Place);
	FTerritoryFactionAssaultConfig& Force =
		Place->CounterAttackProfile->FactionForces.AddDefaulted_GetRef();
	Force.Faction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Force.ScheduleMode = ETerritoryCounterScheduleMode::FiniteSeries;
	Force.MaximumScheduledAssaults = 3;
	Force.PlannedForce = 6;
	Force.WaveSize = 2;

	const FTerritoryStoryOutcomeReport Report =
		FTerritoryStoryOutcomeAnalyzer::Analyze(Place, false);
	const FTerritoryStoryOutcomeScenario* Production =
		TerritoryStoryOutcomeTests::FindScenario(Report, TEXT("Daily Grain"));
	TestNotNull(TEXT("Production rule becomes a story outcome branch"), Production);
	if (Production)
	{
		TestTrue(TEXT("Production branch explains its upgrade gate"),
			Production->OnlyIf.Contains(TEXT("at least 2")));
		TestTrue(TEXT("Production branch explains loss stops future production"),
			Production->AlsoAffects.Contains(TEXT("stops future production")));
	}

	const FTerritoryStoryOutcomeScenario* Counter =
		Report.Scenarios.FindByPredicate(
			[](const FTerritoryStoryOutcomeScenario& Scenario)
			{
				return Scenario.Category == TEXT("Counterattack")
					&& Scenario.Title.Contains(TEXT("strategic response"));
			});
	TestNotNull(TEXT("Faction counterattack becomes a chance-based branch"), Counter);
	if (Counter)
	{
		TestEqual(TEXT("Counterattack report never claims a guaranteed launch"),
			Counter->Certainty,
			ETerritoryStoryOutcomeCertainty::ChanceBased);
		TestTrue(TEXT("Counterattack report explains the finite schedule"),
			Counter->Then.Contains(TEXT("at most 3 battles")));
		TestTrue(TEXT("Counterattack report keeps force finite"),
			Counter->Then.Contains(TEXT("6 total attackers")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryOutcomeValidationIsReadOnly,
	"TerritoryFramework.Editor.StoryOutcome.FullValidationIsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryOutcomeValidationIsReadOnly::RunTest(
	const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Temp/TF_StoryOutcomeValidation"));
	UTerritoryCityDefinition* City = NewObject<UTerritoryCityDefinition>(
		Package, TEXT("DA_TestStoryCity"), RF_Public | RF_Standalone);
	City->DisplayName = FText::FromString(TEXT("Test City"));
	Package->SetDirtyFlag(false);

	const FTerritoryStoryOutcomeReport Report =
		FTerritoryStoryOutcomeAnalyzer::Analyze(City, true);
	const FTerritoryStoryOutcomeScenario* HUD =
		TerritoryStoryOutcomeTests::FindScenario(
			Report, TEXT("Passive gameplay HUD card"));
	TestNotNull(TEXT("Every Definition report explains passive HUD presentation"), HUD);
	if (HUD)
	{
		TestTrue(TEXT("A City report explains its quiet default"),
			HUD->Then.Contains(TEXT("stays hidden")));
		TestTrue(TEXT("Disabling the card preserves strategic UI"),
			HUD->AlsoAffects.Contains(TEXT("Command Center")));
	}
	TestTrue(TEXT("Existing Data Validation findings appear in setup health"),
		!Report.ValidationErrors.IsEmpty() || !Report.ValidationWarnings.IsEmpty());
	TestFalse(TEXT("Full validation plus report generation does not dirty the package"),
		Package->IsDirty());
	TestTrue(TEXT("Plain-text export identifies itself as read only"),
		Report.BuildPlainText().Contains(TEXT("STORY OUTCOME (READ ONLY)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryOutcomePanelSmoke,
	"TerritoryFramework.Editor.StoryOutcome.DetailsPanelSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryOutcomePanelSmoke::RunTest(const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Temp/TF_StoryOutcomePanel"));
	UTerritoryPlaceDefinition* Place = NewObject<UTerritoryPlaceDefinition>(
		Package, TEXT("DA_TestStoryPanel"), RF_Public | RF_Standalone);
	Place->DisplayName = FText::FromString(TEXT("Panel Place"));
	Package->SetDirtyFlag(false);

	TSharedRef<STerritoryStoryOutcomePanel> Panel =
		SNew(STerritoryStoryOutcomePanel)
		.Definition(Place);
	TestTrue(TEXT("Details panel constructs a valid Slate widget"),
		Panel->GetChildren() != nullptr);
	TestFalse(TEXT("Constructing the Details panel does not dirty the Definition"),
		Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStoryOutcomeSingleRegistration,
	"TerritoryFramework.Editor.StoryOutcome.SingleInheritedDetailsRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStoryOutcomeSingleRegistration::RunTest(
	const FString& Parameters)
{
	FPropertyEditorModule& PropertyEditor =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
			TEXT("PropertyEditor"));
	const FCustomDetailLayoutNameMap& Layouts =
		PropertyEditor.GetClassNameToDetailLayoutNameMap();
	TestTrue(TEXT("The base Territory Definition owns the inherited customization"),
		Layouts.Contains(UTerritoryDefinition::StaticClass()->GetFName()));
	TestFalse(TEXT("Place does not register the same customization a second time"),
		Layouts.Contains(UTerritoryPlaceDefinition::StaticClass()->GetFName()));
	TestFalse(TEXT("District does not register the same customization a second time"),
		Layouts.Contains(UTerritoryDistrictDefinition::StaticClass()->GetFName()));
	TestFalse(TEXT("City does not register the same customization a second time"),
		Layouts.Contains(UTerritoryCityDefinition::StaticClass()->GetFName()));
	return true;
}

#endif
