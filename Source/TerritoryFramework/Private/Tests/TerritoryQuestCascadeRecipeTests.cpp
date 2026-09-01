#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "Tales/TerritoryStateTask.h"
#include "Tales/TerritoryStoryConditions.h"

namespace TerritoryQuestCascadeRecipeTests
{
	UTerritoryQuestCascadeRecipe* MakeValidRecipe()
	{
		UTerritoryQuestCascadeRecipe* Recipe =
			NewObject<UTerritoryQuestCascadeRecipe>();
		Recipe->QuestName = FText::FromString(TEXT("Secure the Blacksmith"));
		Recipe->QuestDescription = FText::FromString(
			TEXT("Clear the defenders and claim the Place."));
		Recipe->StartStateID = TEXT("Assault");

		FTerritoryQuestCascadeState& Assault = Recipe->States.AddDefaulted_GetRef();
		Assault.StateID = TEXT("Assault");
		Assault.Description = FText::FromString(TEXT("Clear the defenders."));
		Assault.Conditions.Add(
			NewObject<UTerritoryEventContextCondition>(Recipe));
		FTerritoryQuestCascadeBranch& Route =
			Assault.Branches.AddDefaulted_GetRef();
		Route.BranchID = TEXT("DefendersCleared");
		Route.DestinationStateID = TEXT("Secured");
		Route.Tasks.Add(NewObject<UTerritoryStateTask>(Recipe));

		FTerritoryQuestCascadeState& Success = Recipe->States.AddDefaulted_GetRef();
		Success.StateID = TEXT("Secured");
		Success.Type = ETerritoryQuestCascadeStateType::Success;
		Success.Description = FText::FromString(TEXT("The Place is secure."));
		return Recipe;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryQuestCascadeRecipeContract,
	"TerritoryFramework.Tales.QuestCascade.ValidRecipeAndPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryQuestCascadeRecipeContract::RunTest(const FString& Parameters)
{
	UTerritoryQuestCascadeRecipe* Recipe =
		TerritoryQuestCascadeRecipeTests::MakeValidRecipe();
	const FTerritoryQuestCascadeValidation Validation = Recipe->ValidateRecipe();
	TestTrue(TEXT("A connected objective-to-success recipe is valid"),
		Validation.bValid);
	TestTrue(TEXT("Valid recipe has no errors"), Validation.Errors.IsEmpty());
	const FString Preview = Recipe->BuildPlainTextPreview();
	TestTrue(TEXT("Preview explains same-branch AND task logic"),
		Preview.Contains(TEXT("ALL non-optional tasks required")));
	TestTrue(TEXT("Preview names the reusable branch"),
		Preview.Contains(TEXT("DefendersCleared")));
	TestTrue(TEXT("Preview explains shared state requirements"),
		Preview.Contains(TEXT("REQUIRE EVERY ROUTE")));
	const FTerritoryQuestCascadeLogicSummary Summary =
		Recipe->BuildMissionLogicSummary();
	TestEqual(TEXT("Summary counts the route condition"), Summary.Conditions, 1);
	TestEqual(TEXT("Summary counts the authored player task"),
		Summary.PlayerTasks, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryQuestCascadeRecipeValidation,
	"TerritoryFramework.Tales.QuestCascade.RejectsUnsafeGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryQuestCascadeRecipeValidation::RunTest(const FString& Parameters)
{
	UTerritoryQuestCascadeRecipe* Recipe =
		TerritoryQuestCascadeRecipeTests::MakeValidRecipe();
	Recipe->States[0].Branches[0].BranchID = Recipe->States[1].StateID;
	Recipe->States[0].Branches[0].DestinationStateID = TEXT("MissingEnding");
	Recipe->States[0].Branches[0].Tasks.Reset();
	Recipe->States[0].Conditions.Reset();

	const FTerritoryQuestCascadeValidation Validation = Recipe->ValidateRecipe();
	TestFalse(TEXT("Duplicate IDs, missing destination, and empty tasks are invalid"),
		Validation.bValid);
	TestTrue(TEXT("Unsafe recipe reports several actionable errors"),
		Validation.Errors.Num() >= 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
