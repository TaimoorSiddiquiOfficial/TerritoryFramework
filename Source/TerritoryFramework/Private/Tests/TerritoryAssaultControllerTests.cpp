#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/NarrativeNPCController.h"
#include "AI/TerritoryContextualAnimComponent.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Core/TerritoryGuardCharacter.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NarrativeSavableActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultMountedController,
	"TerritoryFramework.CounterAttack.Regression.NativeLoadPreservesMountedController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultMountedController::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World created"), World)) return false;
	World->CreateAISystem();
	auto* NPC = World->SpawnActor<ATerritoryAssaultCharacter>();
	auto* Mount = World->SpawnActor<APawn>();
	if (!NPC || !Mount)
	{
		AddError(TEXT("Could not create the possession fixture"));
		World->DestroyWorld(false);
		return false;
	}
	NPC->SpawnDefaultController();
	ANarrativeNPCController* Original = NPC->GetNPCController();
	if (!TestNotNull(TEXT("Native controller spawned"), Original))
	{
		World->DestroyWorld(false);
		return false;
	}
	auto CountControllers = [World]()
	{
		int32 Count = 0;
		for (TActorIterator<ANarrativeNPCController> It(World); It; ++It)
		{
			if (IsValid(*It)) ++Count;
		}
		return Count;
	};
	const int32 InitialCount = CountControllers();
	auto* Activity = NPC->GetActivityComponent();
	const FGuid ID = FGuid::NewGuid();
	NPC->SetActorGUID_Implementation(ID);
	Original->Possess(Mount);
	TestNull(TEXT("Mounted character is deliberately unpossessed"), NPC->GetController());
	TestEqual(TEXT("Native controller retains its owned NPC"), Original->GetOwnedNPC(),
		static_cast<ANarrativeNPCCharacter*>(NPC));
	for (int32 Repeat = 0; Repeat < 3; ++Repeat)
	{
		// Native Load calls SpawnDefaultController when the character is unpossessed;
		// late character-definition completion calls the same virtual unconditionally.
		INarrativeSavableActor::Execute_Load(NPC);
		NPC->SpawnDefaultController();
		TestTrue(TEXT("Readiness retains the existing Native activity"), NPC->EnsureNarrativeControllerReady());
		TestEqual(TEXT("Mounted load preserves controller identity"), NPC->GetNPCController(), Original);
		TestEqual(TEXT("The original controller still drives the mount"), Mount->GetController(),
			static_cast<AController*>(Original));
		TestNull(TEXT("No replacement possesses the seated character"), NPC->GetController());
		TestEqual(TEXT("Activity authority survives the load callback"), NPC->GetActivityComponent(), Activity);
		TestEqual(TEXT("Stable survivor identity is unchanged"), NPC->GetActorGUID_Implementation(), ID);
		TestEqual(TEXT("Repeated callbacks cannot spawn duplicate controllers"), CountControllers(), InitialCount);
	}
	Original->UnPossess();
	NPC->SpawnDefaultController();
	TestEqual(TEXT("Temporary unpossession retains the owning controller"), NPC->GetNPCController(), Original);
	Original->Possess(NPC);
	NPC->SpawnDefaultController();
	TestEqual(TEXT("Dismount resumes with the same controller"), NPC->GetController(),
		static_cast<AController*>(Original));

	Original->UnPossess();
	Original->Destroy();
	NPC->SetRole(ROLE_SimulatedProxy);
	NPC->SpawnDefaultController();
	TestNull(TEXT("A client cannot create a replacement controller"), NPC->GetController());
	NPC->SetRole(ROLE_Authority);
	NPC->SpawnDefaultController();
	TestTrue(TEXT("A destroyed controller can be replaced on the server"),
		IsValid(NPC->GetController()) && NPC->GetController() != Original);
	TestEqual(TEXT("Recovery updates the existing Native cache"),
		static_cast<AController*>(NPC->GetNPCController()), NPC->GetController());
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryExecutionBindings,
	"TerritoryFramework.Combat.Regression.NarrativeSwordExecutionBindsTerritoryNPCs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryExecutionBindings::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Execution world created"), World)) return false;
	auto* Instigator = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* Assault = World->SpawnActor<ATerritoryAssaultCharacter>();
	auto* MissingComponent = World->SpawnActor<APawn>();
	const auto* Scene = LoadObject<UContextualAnimSceneAsset>(nullptr,
		TEXT("/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/9CG/Executions/CAS_Execution.CAS_Execution"));
	if (!Instigator || !Guard || !Assault || !MissingComponent || !Scene)
	{
		AddError(TEXT("Actual Narrative execution fixture failed to load"));
		World->DestroyWorld(false);
		return false;
	}
	const FContextualAnimSceneBindingContext Primary(Instigator);
	FContextualAnimSceneBindings Bindings;
	TestFalse(TEXT("The original component omission rejects Narrative execution"),
		FContextualAnimSceneBindings::TryCreateBindings(*Scene, 0, 0, Primary,
			FContextualAnimSceneBindingContext(MissingComponent), Bindings));
	for (ANarrativeNPCCharacter* Target : {static_cast<ANarrativeNPCCharacter*>(Guard),
		static_cast<ANarrativeNPCCharacter*>(Assault)})
	{
		auto* Component = Target->FindComponentByClass<UTerritoryContextualAnimComponent>();
		TestNotNull(TEXT("Territory NPC supplies the existing execution component contract"), Component);
		if (Component) TestTrue(TEXT("Contextual animation retains engine replication"), Component->GetIsReplicated());
		TestTrue(TEXT("Actual Narrative execution accepts both Territory NPC roles"),
			FContextualAnimSceneBindings::TryCreateBindings(*Scene, 0, 0, Primary,
				FContextualAnimSceneBindingContext(Target), Bindings));
		TestNotNull(TEXT("Target is present in the verified scene bindings"), Bindings.FindBindingByActor(Target));
	}
	World->DestroyWorld(false);
	return true;
}

#endif
