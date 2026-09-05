#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryNarrativeConditionTask.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Tales/NarrativeNodeBase.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativePartyComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Party policy world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// Exercise Narrative's multiplayer policy branch without opening a network socket.
	World->NextURL = TEXT("/Game/PartyPolicyTest?listen");
	TestEqual(TEXT("Fixture exercises multiplayer party evaluation"), World->GetNetMode(), NM_ListenServer);
	AActor* PartyOwner = NewObject<AActor>(World->PersistentLevel);
	PartyOwner->SetRole(ROLE_Authority);
	UNarrativePartyComponent* Party = NewObject<UNarrativePartyComponent>(PartyOwner);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		APlayerController* MemberOwner = NewObject<APlayerController>(World->PersistentLevel);
		MemberOwner->SetRole(ROLE_Authority);
		TestTrue(TEXT("Narrative admits a real controller-owned party member"),
			Party->AddPartyMember(NewObject<UTalesComponent>(MemberOwner)));
	}
	First->PartyConditionPolicy = EPartyConditionPolicy::AnyPlayerPasses;
	First->Callback = []() { return false; };
	Event->Conditions = {First};
	TestFalse(TEXT("Any Party Member Passes rejects when every member fails"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	Task->OwningComp = Party;
	Task->ConditionProbe = NewObject<UNarrativeNodeBase>(Task);
	Task->Conditions = {First};
	TestFalse(TEXT("Quest condition gates also reject an entirely failing party"), Task->AreGateConditionsMet());
	int32 PartyChecks = 0;
	First->Callback = [&PartyChecks]() { return ++PartyChecks == 2; };
	TestTrue(TEXT("A later passing member satisfies Any Party Member Passes"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	TestEqual(TEXT("Each member is evaluated once until one passes"), PartyChecks, 2);
	First->Callback = []() { return false; };
	First->bNot = true;
	TestTrue(TEXT("Party member evaluation honors Narrative Not"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party->GetPartyMembers()[0]));
	First->bNot = false;
	First->Callback = []() { return true; };
	Event->Conditions = {First, Second};
	TestFalse(TEXT("A passing party policy cannot skip the later AND condition"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	Event->Conditions = {First};
	First->PartyConditionPolicy = EPartyConditionPolicy::PartyLeaderPasses;
	TestTrue(TEXT("The existing party leader can satisfy its policy"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	First->PartyConditionPolicy = EPartyConditionPolicy::AllPlayersPass;
	PartyChecks = 0;
	First->Callback = [&PartyChecks]() { return ++PartyChecks == 1; };
	TestFalse(TEXT("Native All Players policy still rejects a later failing member"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	for (UTalesComponent* Member : Party->GetPartyMembers()) Party->RemovePartyMember(Member);
	First->PartyConditionPolicy = EPartyConditionPolicy::AnyPlayerPasses;
	First->Callback = []() { return true; };
	TestFalse(TEXT("An empty party has no passing member"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	First->PartyConditionPolicy = EPartyConditionPolicy::PartyLeaderPasses;
	TestFalse(TEXT("A party without a leader fails safely"),
		TerritoryTales::DoEventConditionsPass(Event, nullptr, nullptr, Party));
	TestFalse(TEXT("The missing leader also fails the quest gate safely"), Task->AreGateConditionsMet());
	World->NextURL.Reset();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
