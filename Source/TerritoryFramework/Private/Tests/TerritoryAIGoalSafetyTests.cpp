#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Economy/TerritoryProductionProfile.h"
#include "AIController.h"
#include "Core/TerritoryGuardCharacter.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalGenerator.h"
#include "Perception/AIPerceptionComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "Engine/World.h"

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

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Native ownership fixture world"), World)) return false;
	ANarrativeNPCController* Controller = World->SpawnActor<ANarrativeNPCController>();
	ANarrativeNPCCharacter* Pawn = World->SpawnActor<ATerritoryGuardCharacter>();
	Controller->SetPawn(Pawn);
	UAIPerceptionComponent* Perception = NewObject<UAIPerceptionComponent>(Controller);
	Controller->SetPerceptionComponent(*Perception);
	UNPCActivityComponent* Activities = Controller->FindComponentByClass<UNPCActivityComponent>();
	Activities->Activate();
	UClass* GeneratorClass = LoadClass<UNPCGoalGenerator>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/GoalGenerator_Attack.GoalGenerator_Attack_C"));
	if (!TestNotNull(TEXT("Concrete Narrative attack generator"), GeneratorClass))
	{
		World->DestroyWorld(false);
		return false;
	}
	UNPCGoalGenerator* Generator = NewObject<UNPCGoalGenerator>(Activities, GeneratorClass);
	TestTrue(TEXT("A real Narrative controller-owned activity can refresh perception"),
		UTerritoryBlueprintLibrary::CanSafelyRefreshPerceivedActors(Generator, Controller));
	Activities->Deactivate();
	TestFalse(TEXT("Inactive Native activities remain protected"),
		UTerritoryBlueprintLibrary::CanSafelyRefreshPerceivedActors(Generator, Controller));
	Activities->Activate();
	Controller->SetPawn(nullptr);
	TestFalse(TEXT("Unpossessed controllers remain protected"),
		UTerritoryBlueprintLibrary::CanSafelyRefreshPerceivedActors(Generator, Controller));
	World->DestroyWorld(false);

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
