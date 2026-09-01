#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "Tales/TerritoryStateTask.h"

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
	TestTrue(TEXT("Preview explains that same-branch tasks are all required"),
		Preview.Contains(TEXT("ALL required")));
	TestTrue(TEXT("Preview names the reusable branch"),
		Preview.Contains(TEXT("DefendersCleared")));
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

	const FTerritoryQuestCascadeValidation Validation = Recipe->ValidateRecipe();
	TestFalse(TEXT("Duplicate IDs, missing destination, and empty tasks are invalid"),
		Validation.bValid);
	TestTrue(TEXT("Unsafe recipe reports several actionable errors"),
		Validation.Errors.Num() >= 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

