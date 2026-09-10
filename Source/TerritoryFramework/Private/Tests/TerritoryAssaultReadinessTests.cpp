#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NPCDefinition.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultNativeAppearanceReadiness,
	"TerritoryFramework.CounterAttack.Regression.WaitForNativeBaseAppearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultNativeAppearanceReadiness::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Readiness world exists"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	World->CreateAISystem();
	auto* NPC = World->SpawnActor<ATerritoryAssaultCharacter>();
	if (!TestNotNull(TEXT("Assault NPC exists"), NPC)) return false;
	TestTrue(TEXT("Native controller and activity initialize"), NPC->EnsureNarrativeControllerReady());
	TestFalse(TEXT("Missing definition cannot be ready"), NPC->IsNarrativeSpawnReady());
	// Supply the same definition/visual boundary used by Native's asynchronous
	// callback. A spawned visual can exist before its base meshes finish loading.
	FindFProperty<FObjectPropertyBase>(NPC->GetClass(), TEXT("NPCDefinition"))
		->SetObjectPropertyValue_InContainer(NPC, NewObject<UNPCDefinition>());
	TestFalse(TEXT("Missing visual cannot be ready"), NPC->IsNarrativeSpawnReady());
	auto* Visual = World->SpawnActor<ANarrativeCharacterVisual>();
	if (!TestNotNull(TEXT("Native visual exists"), Visual)) return false;
	FindFProperty<FObjectPropertyBase>(NPC->GetClass(), TEXT("CharVisual"))
		->SetObjectPropertyValue_InContainer(NPC, Visual);
	const auto NativePendingLoad = [NPC]()
	{
		// Native exposes this protected C++ query to Blueprint/Python callers.
		bool bPending = false;
		NPC->ProcessEvent(NPC->FindFunctionChecked(TEXT("IsCharacterPendingLoad")), &bPending);
		return bPending;
	};
	Visual->bBaseAppearanceLoaded = false;
	TestFalse(TEXT("No individual mesh handles does not mean the base appearance finished"), Visual->HasLoadHandles());
	TestTrue(TEXT("Native still reports pending base initialization"), NativePendingLoad());
	TestFalse(TEXT("Territory must wait for Native base initialization"), NPC->IsNarrativeSpawnReady());
	Visual->bBaseAppearanceLoaded = true;
	TestFalse(TEXT("Native reports a completed definition and appearance"), NativePendingLoad());
	TestTrue(TEXT("An initialized unarmed NPC is ready"), NPC->IsNarrativeSpawnReady());
	// Readiness is a query. A client has no authority to initialize a controller,
	// grant abilities or mutate the finite assault simply by reading this value.
	NPC->SetRole(ROLE_SimulatedProxy);
	Visual->bBaseAppearanceLoaded = false;
	TestFalse(TEXT("Client queries also respect pending Native appearance"), NPC->IsNarrativeSpawnReady());
	TestFalse(TEXT("Readiness query does not complete Native appearance"), Visual->bBaseAppearanceLoaded);
	NPC->SetRole(ROLE_Authority);
	return true;
}

#endif
