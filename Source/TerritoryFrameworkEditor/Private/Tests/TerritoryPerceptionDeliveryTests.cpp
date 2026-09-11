#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TerritoryNPCSaveProbe.h"
#include "AI/TerritoryNPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPerceptionDelivery,
	"TerritoryFramework.AI.Regression.StoredPerceptionDelivery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPerceptionDelivery::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Perception world exists"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	World->CreateAISystem();
	UClass* ControllerClass = LoadClass<ANarrativeNPCController>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryNPCController.BP_TerritoryNPCController_C"));
	if (!TestNotNull(TEXT("Real inherited Native dispatcher class loads"), ControllerClass)) return false;
	auto* Controller = World->SpawnActor<ANarrativeNPCController>(ControllerClass);
	auto* NPC = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* Target = World->SpawnActor<ATerritoryGuardCharacter>();
	Controller->SetPawn(NPC);
	auto* Visual = World->SpawnActor<ANarrativeCharacterVisual>();
	Visual->bBaseAppearanceLoaded = false;
	FindFProperty<FObjectPropertyBase>(NPC->GetClass(), TEXT("CharVisual"))
		->SetObjectPropertyValue_InContainer(NPC, Visual);
	auto* Activity = CastChecked<UTerritoryNPCActivityComponent>(Controller->GetActivityComponent());
	FindFProperty<FObjectPropertyBase>(UNPCActivityComponent::StaticClass(), TEXT("OwnerController"))
		->SetObjectPropertyValue_InContainer(Activity, Controller);
	Activity->Activate();
	auto* Generator = CastChecked<UTerritoryNPCSaveProbe>(Activity->AddGoalGenerator(UTerritoryNPCSaveProbe::StaticClass(), true));
	auto* Removed = NewObject<UTerritoryNPCSaveReplacementProbe>(Activity);
	auto* Event = FindFProperty<FMulticastDelegateProperty>(ControllerClass, TEXT("OnPerceptionUpdated"));
	if (!TestNotNull(TEXT("Native Blueprint dispatcher exists"), Event)) return false;
	AddInfo(FString::Printf(TEXT("Native dispatcher parameters: %d"), Event->SignatureFunction->NumParms));
	for (TFieldIterator<FProperty> It(Event->SignatureFunction); It; ++It)
		AddInfo(FString::Printf(TEXT("%s: %s"), *It->GetName(), *It->GetCPPType()));
	for (auto* Listener : {Generator, static_cast<UTerritoryNPCSaveProbe*>(Removed)})
	{
		FScriptDelegate Binding;
		Binding.BindUFunction(Listener, TEXT("PerceptionReceived"));
		Event->AddDelegate(Binding, Controller);
	}
	auto* Perception = Controller->GetAIPerceptionComponent();
	if (!TestNotNull(TEXT("Native controller owns perception"), Perception)) return false;
	// This fixture isolates adapter deliveries. Keep the real engine sense store,
	// but disconnect this test controller's original visibility-edge forwarding.
	Perception->OnTargetPerceptionUpdated.RemoveAll(Controller);
	Perception->ConfigureSense(*NewObject<UAISenseConfig_Sight>(Perception));
	auto Observe = [&](float Strength)
	{
		Perception->RegisterStimulus(Target, FAIStimulus(*GetDefault<UAISense_Sight>(), Strength,
			Target->GetActorLocation(), NPC->GetActorLocation()));
		Perception->ProcessStimuli();
	};
	Activity->RefreshStoredPerception();
	TestTrue(TEXT("No stored perception means no manufactured sight"), Generator->ReceivedStrengths.IsEmpty());
	Observe(0.f);
	Observe(0.4f);
	// The original Native visibility edge is unrelated to this adapter. Isolate
	// the refresh deliveries while retaining the real engine perception store.
	Generator->ReceivedStrengths.Reset();
	Removed->ReceivedStrengths.Reset();
	Activity->RefreshStoredPerception();
	TestTrue(TEXT("Pending base appearance cannot drive AI callbacks"), Generator->ReceivedStrengths.IsEmpty());
	Visual->bBaseAppearanceLoaded = true;
	TestTrue(TEXT("Fixture NPC is alive"), NPC->IsAlive());
	TestTrue(TEXT("Fixture activity is active"), Activity->IsActive());
	TestTrue(TEXT("Fixture visual finished Native loading"), FTerritoryNarrativeProAdapter::IsCharacterReady(NPC));
	FActorPerceptionBlueprintInfo Stored;
	TestTrue(TEXT("Fixture has stored perception"), Perception->GetActorsPerception(Target, Stored));
	for (const FAIStimulus& Stimulus : Stored.LastSensedStimuli)
		AddInfo(FString::Printf(TEXT("Stored sense valid=%d seen=%d expired=%d strength=%.3f"),
			Stimulus.Type.IsValid(), Stimulus.WasSuccessfullySensed(), Stimulus.IsExpired(), Stimulus.Strength));
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Ready generator receives the stored positive observation"), Generator->ReceivedStrengths.Num(), 1);
	if (!Generator->ReceivedStrengths.IsEmpty())
		TestEqual(TEXT("Real strength is retained rather than promoted to full visibility"), Generator->ReceivedStrengths.Last(), 0.4f);
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Unchanged perception is not repeatedly broadcast"), Generator->ReceivedStrengths.Num(), 1);
	Observe(0.7f);
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Stored strength change wakes the current generator"), Generator->ReceivedStrengths.Num(), 2);
	TestTrue(TEXT("A stale removed generator is never notified"), Removed->ReceivedStrengths.IsEmpty());
	Controller->SetRole(ROLE_SimulatedProxy);
	Observe(0.8f);
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Client cannot deliver authoritative AI perception"), Generator->ReceivedStrengths.Num(), 2);
	Controller->SetRole(ROLE_Authority);
	Activity->PrepareForSave_Implementation();
	Activity->Load_Implementation();
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Save/load refreshes observations for restored generators once"), Generator->ReceivedStrengths.Num(), 3);
	Controller->SetPawn(nullptr);
	Activity->RefreshStoredPerception();
	TestTrue(TEXT("Dismount/unpossession clears the old pawn delivery cache"), Activity->DeliveredPerception.IsEmpty());
	Controller->SetPawn(NPC);
	Perception->ForgetActor(Target);
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Forgotten actor is not resurrected after repossession"), Generator->ReceivedStrengths.Num(), 3);
	Observe(0.6f);
	Generator->PerceptionCallback = [&]() { Activity->RemoveGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()); };
	Activity->RefreshStoredPerception();
	TestNull(TEXT("Reentrant generator removal remains removed"), Activity->GetGoalGenerator(UTerritoryNPCSaveProbe::StaticClass()));
	Activity->RefreshStoredPerception();
	TestEqual(TEXT("Removed generator receives no later refresh"), Generator->ReceivedStrengths.Num(), 4);
	return true;
}

#endif
