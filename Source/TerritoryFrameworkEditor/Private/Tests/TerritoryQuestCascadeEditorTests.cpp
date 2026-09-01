#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Factories/Factory.h"
#include "Modules/ModuleManager.h"
#include "QuestBlueprint.h"
#include "Tales/Quest.h"
#include "Tales/QuestBlueprintGeneratedClass.h"
#include "Tales/QuestSM.h"
#include "Tales/TerritoryQuestCascadeEditorLibrary.h"
#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "Tales/STerritoryQuestMissionSummaryPanel.h"
#include "Tales/TerritoryCaptureTask.h"
#include "Tales/TerritoryNarrativeConditionTask.h"
#include "Tales/TerritoryStateTask.h"
#include "Tales/TerritoryStoryConditions.h"
#include "UObject/UnrealType.h"

namespace TerritoryQuestCascadeEditorTests
{
	UTerritoryQuestCascadeRecipe* MakeRecipe()
	{
		UTerritoryQuestCascadeRecipe* Recipe =
			NewObject<UTerritoryQuestCascadeRecipe>();
		Recipe->QuestName = FText::FromString(TEXT("Liberate the Blacksmith"));
		Recipe->QuestDescription = FText::FromString(
			TEXT("Defeat the defenders and claim the Place."));
		Recipe->bTracked = false;
		Recipe->QuestDialoguePlayParams.StartFromID = TEXT("MissionBriefing");
		Recipe->QuestDialoguePlayParams.Priority = 7;
		Recipe->StartStateID = TEXT("Assault");

		FTerritoryQuestCascadeState& Assault = Recipe->States.AddDefaulted_GetRef();
		Assault.StateID = TEXT("Assault");
		Assault.Description = FText::FromString(TEXT("Clear and capture."));
		Assault.Conditions.Add(
			NewObject<UTerritoryEventContextCondition>(Recipe));
		FTerritoryQuestCascadeBranch& Tasks =
			Assault.Branches.AddDefaulted_GetRef();
		Tasks.BranchID = TEXT("ClearAndCapture");
		Tasks.Description = FText::FromString(
			TEXT("Every task on this branch is required."));
		Tasks.DestinationStateID = TEXT("Secured");
		Tasks.Tasks.Add(NewObject<UTerritoryStateTask>(Recipe));
		Tasks.Tasks.Add(NewObject<UTerritoryCaptureTask>(Recipe));

		FTerritoryQuestCascadeState& Success = Recipe->States.AddDefaulted_GetRef();
		Success.StateID = TEXT("Secured");
		Success.Type = ETerritoryQuestCascadeStateType::Success;
		Success.Description = FText::FromString(TEXT("The Blacksmith is secure."));
		return Recipe;
	}

	UQuestBlueprint* MakeQuestBlueprint()
	{
		FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("NarrativeQuestEditor"));
		UClass* FactoryClass = FindObject<UClass>(nullptr,
			TEXT("/Script/NarrativeQuestEditor.QuestAssetFactory"));
		if (!FactoryClass || !FactoryClass->IsChildOf(UFactory::StaticClass()))
		{
			return nullptr;
		}
		UPackage* Package = CreatePackage(TEXT("/Temp/TF_CascadeQuest"));
		UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		return Cast<UQuestBlueprint>(Factory->FactoryCreateNew(
			UQuestBlueprint::StaticClass(), Package, TEXT("NQ_CascadeTest"),
			RF_Public | RF_Standalone | RF_Transient, nullptr, GWarn));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryQuestCascadeMaterializesNarrative,
	"TerritoryFramework.Editor.Tales.QuestCascade.MaterializesNarrativeGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryQuestCascadeMaterializesNarrative::RunTest(
	const FString& Parameters)
{
	UQuestBlueprint* Quest = TerritoryQuestCascadeEditorTests::MakeQuestBlueprint();
	if (!TestNotNull(TEXT("Narrative Quest factory creates the test asset"), Quest))
	{
		return false;
	}
	UTerritoryQuestCascadeRecipe* Recipe =
		TerritoryQuestCascadeEditorTests::MakeRecipe();
	// Blueprint compilation may collect transient objects. A real recipe is a
	// loaded asset, so root this transient fixture for the same lifetime.
	Recipe->AddToRoot();
	const FTerritoryQuestCascadeBuildReport Report =
		UTerritoryQuestCascadeEditorLibrary::BuildEmptyQuestFromRecipe(
			Quest, Recipe);
	for (const FText& Error : Report.Errors)
	{
		AddError(Error.ToString());
	}
	TestTrue(TEXT("Recipe materialization succeeds"), Report.bSucceeded);
	TestEqual(TEXT("Generator creates both Narrative states"),
		Report.CreatedStates, 2);
	TestEqual(TEXT("Generator creates one Narrative branch"),
		Report.CreatedBranches, 1);
	TestEqual(TEXT("Generator duplicates both tasks and adds one condition gate"),
		Report.CreatedTasks, 3);
	TestEqual(TEXT("Generator creates one functional condition gate"),
		Report.CreatedConditionGates, 1);
	TestEqual(TEXT("Generator mirrors one condition onto its Narrative state"),
		Report.CopiedConditions, 1);

	const UQuest* SourceTemplate = Quest->QuestTemplate;
	TestNotNull(TEXT("Editable Narrative Quest retains its source template"),
		SourceTemplate);
	if (SourceTemplate)
	{
		const FBoolProperty* TrackedProperty = FindFProperty<FBoolProperty>(
			SourceTemplate->GetClass(), TEXT("bTracked"));
		TestNotNull(TEXT("Installed Narrative Quest exposes tracked setting"),
			TrackedProperty);
		if (TrackedProperty)
		{
			TestFalse(TEXT("Generator copies the recipe tracked setting"),
				TrackedProperty->GetPropertyValue_InContainer(SourceTemplate));
		}
		const FStructProperty* PlayParamsProperty =
			FindFProperty<FStructProperty>(SourceTemplate->GetClass(),
				TEXT("QuestDialoguePlayParams"));
		TestNotNull(TEXT("Installed Narrative Quest exposes dialogue play params"),
			PlayParamsProperty);
		if (PlayParamsProperty)
		{
			const FDialoguePlayParams* Params =
				PlayParamsProperty->ContainerPtrToValuePtr<FDialoguePlayParams>(
					SourceTemplate);
			TestEqual(TEXT("Generator copies dialogue start node"),
				Params->StartFromID, FName(TEXT("MissionBriefing")));
			TestEqual(TEXT("Generator copies dialogue priority"),
				Params->Priority, 7);
		}
		TestEqual(TEXT("Source template has two states"),
			SourceTemplate->GetStates().Num(), 2);
		if (SourceTemplate->GetStates().Num() > 0)
		{
			TestEqual(TEXT("Generator mirrors the state condition for graph visibility"),
				SourceTemplate->GetStates()[0]->Conditions.Num(), 1);
		}
		TestEqual(TEXT("Source template has one branch"),
			SourceTemplate->GetBranches().Num(), 1);
		if (SourceTemplate->GetBranches().Num() == 1)
		{
			const UQuestBranch* Branch = SourceTemplate->GetBranches()[0];
			TestEqual(TEXT("Condition gate and both task templates live on one Narrative branch"),
				Branch->QuestTasks.Num(), 3);
			if (!Branch->QuestTasks.IsEmpty())
			{
				const UTerritoryNarrativeConditionTask* Gate =
					Cast<UTerritoryNarrativeConditionTask>(Branch->QuestTasks[0]);
				TestNotNull(TEXT("First generated task is the functional condition gate"),
					Gate);
				if (Gate)
				{
					TestEqual(TEXT("Gate owns the shared state condition"),
						Gate->Conditions.Num(), 1);
				}
				TestTrue(TEXT("Authored tasks are duplicated rather than referencing the recipe"),
					Branch->QuestTasks[1] != Recipe->States[0].Branches[0].Tasks[0]);
			}
			TestNotNull(TEXT("Branch has a generated destination state"),
				Branch->DestinationState);
			if (Branch->DestinationState)
			{
				TestEqual(TEXT("Branch destination is the generated Success state"),
					Branch->DestinationState->GetID(), FName(TEXT("Secured")));
			}
		}
	}

	const UQuestBlueprintGeneratedClass* GeneratedClass =
		Cast<UQuestBlueprintGeneratedClass>(Quest->GeneratedClass.Get());
	TestNotNull(TEXT("Generated asset compiles as a Narrative Quest class"),
		GeneratedClass);
	if (GeneratedClass)
	{
		const UQuest* CompiledTemplate = GeneratedClass->GetQuestTemplate();
		TestNotNull(TEXT("Compiled Narrative class owns its Quest template"),
			CompiledTemplate);
		if (CompiledTemplate)
		{
			TestEqual(TEXT("Compiled Narrative template has one branch"),
				CompiledTemplate->GetBranches().Num(), 1);
			if (CompiledTemplate->GetBranches().Num() == 1)
			{
				TestEqual(TEXT("Compiled Narrative branch retains gate and both tasks"),
					CompiledTemplate->GetBranches()[0]->QuestTasks.Num(), 3);
			}
		}
	}

	const int32 StatesBefore = SourceTemplate
		? SourceTemplate->GetStates().Num() : 0;
	const FTerritoryQuestCascadeBuildReport SecondBuild =
		UTerritoryQuestCascadeEditorLibrary::BuildEmptyQuestFromRecipe(
			Quest, Recipe);
	TestFalse(TEXT("Generator refuses to overwrite an authored quest"),
		SecondBuild.bSucceeded);
	TestEqual(TEXT("Rejected overwrite leaves existing states unchanged"),
		Quest->QuestTemplate->GetStates().Num(), StatesBefore);
	Recipe->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryQuestMissionSummaryPanelSmoke,
	"TerritoryFramework.Editor.Tales.QuestCascade.MissionLogicPanelIsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryQuestMissionSummaryPanelSmoke::RunTest(
	const FString& Parameters)
{
	UPackage* Package = CreatePackage(TEXT("/Temp/TF_MissionLogicPanel"));
	UTerritoryQuestCascadeRecipe* Recipe =
		NewObject<UTerritoryQuestCascadeRecipe>(Package,
			TEXT("DA_TestMissionRecipe"), RF_Public | RF_Standalone);
	Recipe->QuestName = FText::FromString(TEXT("Test Mission"));
	Package->SetDirtyFlag(false);
	TSharedRef<STerritoryQuestMissionSummaryPanel> Panel =
		SNew(STerritoryQuestMissionSummaryPanel).Recipe(Recipe);
	TestTrue(TEXT("Mission Logic panel constructs a valid Slate widget"),
		Panel->GetChildren() != nullptr);
	TestFalse(TEXT("Constructing the Mission Logic panel never dirties the recipe"),
		Package->IsDirty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
