#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryDialogueLifecycleProbe.h"
#include "Tales/TerritoryDialogueLifecycleComponent.h"
#include "Tales/Dialogue.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryRemoteDialogueLifecycle,
	"TerritoryFramework.Presentation.Cinematics.RejectedReplacementSendsNativeExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryRemoteDialogueLifecycle::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> FrameCounter(GFrameCounter, GFrameCounter);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	auto* Tales = NewObject<UTerritoryDialogueLifecycleProbe>(Controller);
	auto* TalesProperty = FindFProperty<FObjectPropertyBase>(Controller->GetClass(), TEXT("TalesComponent"));
	auto* CurrentProperty = FindFProperty<FObjectPropertyBase>(Tales->GetClass(), TEXT("CurrentDialogue"));
	if (!TalesProperty || !CurrentProperty) { World->DestroyWorld(false); return false; }
	TalesProperty->SetObjectPropertyValue_InContainer(Controller, Tales);
	Controller->AddInstanceComponent(Tales);
	Tales->RegisterComponent();
	auto* Adapter = UTerritoryDialogueLifecycleComponent::FindOrCreate(Controller);
	TestNotNull(TEXT("Authoritative controller installs lifecycle observer"), Adapter);
	TestTrue(TEXT("Repeated installation returns the same observer"), Adapter == UTerritoryDialogueLifecycleComponent::FindOrCreate(Controller));
	TestFalse(TEXT("Observer adds no replicated component"), Adapter->GetIsReplicated());
	const auto Begin = [&]()
	{
		auto* Dialogue = NewObject<UDialogue>(Tales);
		Dialogue->OwningComp = Tales;
		Dialogue->OwningController = Controller;
		CurrentProperty->SetObjectPropertyValue_InContainer(Tales, Dialogue);
		Tales->OnDialogueBegan.Broadcast(Dialogue);
		return Dialogue;
	};
	const auto Advance = [&]() { ++GFrameCounter; World->GetTimerManager().Tick(0.01f); };
	Begin();
	AddExpectedError(TEXT("MakeDialogue was passed UDialogue"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Native replacement fails after clearing the server session"), Tales->BeginDialogue(UDialogue::StaticClass(), FDialoguePlayParams()));
	TestEqual(TEXT("Exit waits for the replacement attempt to finish"), Tales->ExitDispatches, 0);
	Advance();
	TestEqual(TEXT("Failed replacement sends Native's client exit exactly once"), Tales->ExitDispatches, 1);
	Advance();
	TestEqual(TEXT("No recurring exit requests"), Tales->ExitDispatches, 1);

	auto* Previous = Begin();
	Tales->OnDialogueFinished.Broadcast(Previous, true, EExitDialogueReason::EDR_NewDialogueStarted);
	auto* Next = Begin();
	Advance();
	TestEqual(TEXT("Successful replacement cancels the deferred exit"), Tales->ExitDispatches, 1);
	TestTrue(TEXT("New Native dialogue remains current"), Tales->GetCurrentDialogue() == Next);
	Tales->OnDialogueFinished.Broadcast(Previous, true, EExitDialogueReason::EDR_NewDialogueStarted);
	Advance();
	TestEqual(TEXT("Stale finish cannot end the new session"), Tales->ExitDispatches, 1);

	Tales->OnDialogueFinished.Broadcast(Next, true, EExitDialogueReason::EDR_NewDialogueStarted);
	CurrentProperty->SetObjectPropertyValue_InContainer(Tales, nullptr);
	Controller->SetRole(ROLE_SimulatedProxy);
	Advance();
	TestEqual(TEXT("Lost server authority cannot send an exit"), Tales->ExitDispatches, 1);
	TestNull(TEXT("Client cannot install authoritative repair"), UTerritoryDialogueLifecycleComponent::FindOrCreate(Controller));
	Controller->SetRole(ROLE_Authority);
	Previous = Begin();
	Tales->OnDialogueFinished.Broadcast(Previous, true, EExitDialogueReason::EDR_NewDialogueStarted);
	CurrentProperty->SetObjectPropertyValue_InContainer(Tales, nullptr);
	Adapter->UnregisterComponent();
	Advance();
	TestEqual(TEXT("Unregister cancels delayed network work"), Tales->ExitDispatches, 1);
	TestFalse(TEXT("Old Tales delegates released"), Tales->OnDialogueFinished.IsAlreadyBound(Adapter, &UTerritoryDialogueLifecycleComponent::HandleDialogueFinished));
	Adapter->RegisterComponent();
	TestTrue(TEXT("Re-register binds the live Native authority"), Tales->OnDialogueFinished.IsAlreadyBound(Adapter, &UTerritoryDialogueLifecycleComponent::HandleDialogueFinished));
	Adapter->DestroyComponent();
	World->DestroyWorld(false);
	return true;
}
#endif
