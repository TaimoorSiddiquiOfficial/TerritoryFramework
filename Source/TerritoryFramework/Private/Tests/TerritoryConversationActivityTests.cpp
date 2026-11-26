#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/TerritoryConversationActivity.h"
#include "AI/TerritoryConversationGoal.h"
#include "AI/Activities/NPCActivity.h"
#include "BehaviorTree/BehaviorTree.h"
#include "UObject/UnrealType.h"

/**
 * The conversation activity drives movement through the controller instead of a Behaviour Tree,
 * and that contract is what makes the staging walk work. A tree still running from the preceding
 * activity keeps issuing its own move requests, which supersede the meeting MoveTo; measured in
 * PIE, the guard then stood still while the meeting timed out. Because the fix relies on the
 * conversation activity owning no tree, this test fails if anyone later gives it one.
 *
 * UPROPERTY members on UNPCActivity are protected, so every read goes through reflection. That is
 * the established pattern for contract assertions in this module.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryConversationActivityContractTest,
	"TerritoryFramework.AI.ConversationActivity.OwnsMovementWithoutBehaviorTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	bool ReadActivityTree(const UClass* Class, const UObject* Defaults, const UBehaviorTree*& OutTree)
	{
		const FObjectPropertyBase* Property =
			FindFProperty<FObjectPropertyBase>(Class, TEXT("BehaviourTree"));
		if (!Property || !Defaults) return false;
		OutTree = Cast<UBehaviorTree>(Property->GetObjectPropertyValue_InContainer(Defaults));
		return true;
	}
}

bool FTerritoryConversationActivityContractTest::RunTest(const FString& Parameters)
{
	const UClass* ConversationClass = UTerritoryConversationActivity::StaticClass();
	const UObject* ConversationDefaults = ConversationClass->GetDefaultObject();
	if (!TestNotNull(TEXT("Conversation activity has a native default object"), ConversationDefaults)) return false;

	const UBehaviorTree* ConversationTree = nullptr;
	if (TestTrue(TEXT("Conversation activity exposes its Behaviour Tree property"),
		ReadActivityTree(ConversationClass, ConversationDefaults, ConversationTree)))
	{
		TestNull(TEXT("Conversation activity runs no Behaviour Tree of its own"), ConversationTree);
	}

	const FClassProperty* GoalProperty =
		FindFProperty<FClassProperty>(ConversationClass, TEXT("SupportedGoalType"));
	if (TestNotNull(TEXT("Conversation activity declares its supported goal type"), GoalProperty))
	{
		TestTrue(TEXT("Conversation activity serves the conversation goal"),
			GoalProperty->GetPropertyValue_InContainer(ConversationDefaults) == UTerritoryConversationGoal::StaticClass());
	}

	const FBoolProperty* InterruptProperty =
		FindFProperty<FBoolProperty>(ConversationClass, TEXT("bIsInterruptable"));
	if (TestNotNull(TEXT("Conversation activity declares interruptability"), InterruptProperty))
	{
		TestTrue(TEXT("Conversation activity stays interruptable by combat and death"),
			InterruptProperty->GetPropertyValue_InContainer(ConversationDefaults));
	}

	// The patrol activity is the neighbour that strands the staging walk, so prove it does declare a
	// tree: without that difference the assertions above could hold while proving nothing about the
	// real conflict between a direct MoveTo and a surviving tree.
	const UClass* PatrolClass = LoadClass<UNPCActivity>(nullptr,
		TEXT("/Game/TerritoryFramework/AI/BPA_TerritoryPatrol.BPA_TerritoryPatrol_C"));
	if (TestNotNull(TEXT("Shipped patrol activity resolves"), PatrolClass))
	{
		const UObject* PatrolDefaults = PatrolClass->GetDefaultObject();
		const UBehaviorTree* PatrolTree = nullptr;
		if (TestTrue(TEXT("Patrol activity exposes its Behaviour Tree property"),
			ReadActivityTree(PatrolClass, PatrolDefaults, PatrolTree)))
		{
			TestNotNull(TEXT("Patrol activity owns the tree that overrides an unstopped meeting move"),
				PatrolTree);
		}
	}

	return true;
}

#endif
