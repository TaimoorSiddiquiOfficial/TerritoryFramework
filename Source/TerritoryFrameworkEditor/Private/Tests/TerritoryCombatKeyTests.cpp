#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/TerritoryCombatKeyProbe.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/NarrativeNPCController.h"
#include "Combat/TerritoryWeaponSelectionLibrary.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCombatKeySnapshot,
	"TerritoryFramework.AI.Combat.GoalKeySnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCombatKeySnapshot::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) return false;
	World->CreateAISystem();
	UClass* NPCClass = LoadClass<ANarrativeNPCCharacter>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryStoryNPC.BP_TerritoryStoryNPC_C"));
	if (!TestNotNull(TEXT("Concrete Narrative stable NPC"), NPCClass))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Controller = World->SpawnActor<ANarrativeNPCController>();
	auto* NPC = World->SpawnActor<ANarrativeNPCCharacter>(NPCClass);
	auto* Target = World->SpawnActor<ANarrativeNPCCharacter>(NPCClass);
	Controller->SetPawn(NPC);
	auto* Goal = NewObject<UTerritoryCombatKeyProbe>(Controller);
	Goal->OwnerController = Controller;
	Goal->FirstKey = Target;
	UClass* MeleeClass = LoadClass<UNPCActivity>(nullptr,
		TEXT("/TerritoryFramework/AI/Combat/BPA_TerritoryAttack_Melee.BPA_TerritoryAttack_Melee_C"));
	if (!TestNotNull(TEXT("Concrete melee activity"), MeleeClass))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Melee = NewObject<UNPCActivity>(Controller, MeleeClass);
	Melee->SetOwner(Controller, Controller->GetActivityComponent());
	TestTrue(TEXT("Valid single snapshot permits ordinary Narrative combat"),
		UTerritoryBlueprintLibrary::CanScoreTerritoryCombatGoal(Controller, Goal));
	TestEqual(TEXT("Admission reads the Blueprint-overridable key once"), Goal->KeyReads, 1);
	Goal->KeyReads = 0;
	Target->SetActorLocation(NPC->GetActorLocation() + FVector(200.f, 0.f, 0.f));
	TestTrue(TEXT("Distance uses the validated target even if the next key read would be null"),
		UTerritoryWeaponSelectionLibrary::PreferMeleeAtCloseRange(Controller, Goal, Melee, 3.9f) > 4.f);
	TestEqual(TEXT("Preference does not invoke the key callback twice"), Goal->KeyReads, 1);
	Goal->KeyReads = 0;
	Goal->FirstKey = NewObject<USceneComponent>(NPC);
	TestEqual(TEXT("Non-actor keys fail safely"),
		UTerritoryWeaponSelectionLibrary::PreferMeleeAtCloseRange(Controller, Goal, Melee, 3.9f), 0.f);
	World->DestroyWorld(false);
	return true;
}

#endif
