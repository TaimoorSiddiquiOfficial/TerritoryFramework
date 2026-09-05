#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryNarrativeConditionTask.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/TalesComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTalesConditionCallbacks,
	"TerritoryFramework.Tales.Regression.ConditionMutationAndTaskEndCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTalesConditionCallbacks::RunTest(const FString& Parameters)
{
	UTerritoryCaptureEvent* Event = NewObject<UTerritoryCaptureEvent>();
	UTerritoryAuditCondition* First = NewObject<UTerritoryAuditCondition>(Event);
	UTerritoryAuditCondition* Second = NewObject<UTerritoryAuditCondition>(Event);
	First->ConditionFilter = EConditionFilter::CF_DontTarget;
	Second->ConditionFilter = EConditionFilter::CF_DontTarget;
	int32 RemainingChecks = 0;
	First->Callback = [Event]() { Event->Conditions.Reset(); return true; };
	Second->Callback = [&RemainingChecks]() { ++RemainingChecks; return false; };
	Event->Conditions = {First, Second};
	TestFalse(TEXT("A condition cannot erase the remaining requirements from its current evaluation"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, nullptr));
	TestEqual(TEXT("The original second condition is evaluated exactly once"), RemainingChecks, 1);

	UTalesComponent* Tales = NewObject<UTalesComponent>();
	Event->Conditions = {First, Second};
	TestFalse(TEXT("Narrative's node evaluator retains the original AND requirements too"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Tales));
	TestEqual(TEXT("Both supported evaluation paths reach the second requirement"), RemainingChecks, 2);

	UTerritoryNarrativeConditionTask* Task = NewObject<UTerritoryNarrativeConditionTask>();
	Task->OwningComp = Tales;
	Task->ConditionProbe = NewObject<UNarrativeNodeBase>(Task);
	Task->Conditions = {First, Second};
	First->Callback = [Task]() { Task->EndTask(); return true; };
	TestFalse(TEXT("Ending a quest task during its condition aborts evaluation safely"), Task->AreGateConditionsMet());
	TestEqual(TEXT("An ended task never evaluates a later condition"), RemainingChecks, 2);
	TestFalse(TEXT("Ended task remains unable to satisfy its gate"), Task->AreGateConditionsMet());

	Task->ConditionProbe = NewObject<UNarrativeNodeBase>(Task);
	Task->Conditions = {First};
	bool NestedResult = true;
	First->Callback = [Task, &NestedResult]() { NestedResult = Task->AreGateConditionsMet(); return true; };
	TestTrue(TEXT("The outer condition evaluation can finish normally"), Task->AreGateConditionsMet());
	TestFalse(TEXT("A condition cannot recursively reenter the same gate"), NestedResult);
	return true;
}

#endif
