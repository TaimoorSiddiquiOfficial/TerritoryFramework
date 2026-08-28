#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Economy/TerritoryProductionProfile.h"
#include "AIController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryAIGoalSafetyContractTest,
	"TerritoryFramework.AI.GoalGenerator.PerceptionRefreshLifecycleSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryAIGoalSafetyContractTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("A missing goal generator is rejected"),
		UTerritoryBlueprintLibrary::CanSafelyRefreshPerceivedActors(nullptr, nullptr));

	UObject* DetachedGenerator = NewObject<UTerritoryProductionProfile>();
	AAIController* DetachedController =
		AAIController::StaticClass()->GetDefaultObject<AAIController>();
	TestFalse(TEXT("A detached controller without pawn, activity, or perception is rejected"),
		UTerritoryBlueprintLibrary::CanSafelyRefreshPerceivedActors(
			DetachedGenerator, DetachedController));
	TestFalse(TEXT("Rejected lifecycle context never invokes inherited Blueprint logic"),
		UTerritoryBlueprintLibrary::RefreshParentPerceivedActorsSafely(
			DetachedGenerator, DetachedController));

	const UFunction* GuardFunction = UTerritoryBlueprintLibrary::StaticClass()
		->FindFunctionByName(TEXT("RefreshParentPerceivedActorsSafely"));
	TestNotNull(TEXT("The project-owned Narrative adapter is Blueprint callable"), GuardFunction);
	if (GuardFunction)
	{
		TestTrue(TEXT("The adapter has an execution pin"),
			!GuardFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));
	}
	return true;
}

#endif
