#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NPCDefinition.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "NarrativeGameplayTags.h"
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
	// Native damage bursts may arrive on a late-joining client before CharVisual.
	// Exercise the same AActor interface dispatch used by GameplayCueManager.
	auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	if (!TestNotNull(TEXT("Guard cue target exists"), Guard)) return false;
	const FGameplayTag Damage = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.TakeDamage"), false);
	const FGameplayTag ChildDamage = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.TakeDamage.Stumble"), false);
	const FGameplayTag OtherCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue"), false);
	if (!TestTrue(TEXT("Native cue tags exist"), Damage.IsValid() && ChildDamage.IsValid() && OtherCue.IsValid())) return false;
	const FGameplayCueParameters CueParameters;
	auto* Mesh = NewObject<USkeletalMeshComponent>(Visual);
	Visual->AddInstanceComponent(Mesh);
	Mesh->RegisterComponent();
	auto* MeshProperty = FindFProperty<FMapProperty>(Visual->GetClass(), TEXT("MeshComponents"));
	if (!TestNotNull(TEXT("Native visual mesh map is available for fixture"), MeshProperty)) return false;
	FScriptMapHelper Meshes(MeshProperty, MeshProperty->ContainerPtrToValuePtr<void>(Visual));
	const FGameplayTag MeshSlot = FNarrativeGameplayTags::Get().Equipment_Slot_Character_Mesh;
	USkeletalMeshComponent* MeshValue = Mesh;
	Meshes.AddPair(&MeshSlot, &MeshValue);
	TestEqual(TEXT("Fixture exercises Native main mesh lookup"), Visual->GetMainMesh(), Mesh);
	USkeletalMesh* TestMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
	if (!TestNotNull(TEXT("Engine skeletal fixture exists"), TestMesh)) return false;

	for (ANarrativeCharacter* Character : {static_cast<ANarrativeCharacter*>(NPC), static_cast<ANarrativeCharacter*>(Guard)})
	{
		IGameplayCueInterface* CueTarget = Cast<IGameplayCueInterface>(Character);
		if (!TestNotNull(TEXT("Territory NPC participates in public GAS cue acceptance"), CueTarget)) return false;
		auto* VisualProperty = FindFProperty<FObjectPropertyBase>(Character->GetClass(), TEXT("CharVisual"));
		VisualProperty->SetObjectPropertyValue_InContainer(Character, nullptr);
		const auto Accepts = [&](FGameplayTag Tag, EGameplayCueEvent::Type Event)
		{
			return CueTarget->ShouldAcceptGameplayCue(static_cast<AActor*>(Character), Tag, Event, CueParameters);
		};
		TestFalse(TEXT("No visual rejects only an unrenderable damage burst"), Accepts(Damage, EGameplayCueEvent::Executed));
		TestTrue(TEXT("Unrelated burst remains accepted"), Accepts(OtherCue, EGameplayCueEvent::Executed));
		for (EGameplayCueEvent::Type Event : {EGameplayCueEvent::OnActive, EGameplayCueEvent::WhileActive, EGameplayCueEvent::Removed})
		{
			TestTrue(TEXT("Persistent cue lifecycle including removal remains accepted"), Accepts(Damage, Event));
		}
		VisualProperty->SetObjectPropertyValue_InContainer(Character, Visual);
		Visual->bBaseAppearanceLoaded = false;
		TestFalse(TEXT("Partial visual rejects a child damage tag"), Accepts(ChildDamage, EGameplayCueEvent::Executed));
		Visual->bBaseAppearanceLoaded = true;
		Mesh->SetSkeletalMeshAsset(nullptr);
		TestFalse(TEXT("Base-loaded flag without usable mesh cannot render damage"), Accepts(Damage, EGameplayCueEvent::Executed));
		Mesh->SetSkeletalMeshAsset(TestMesh);
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		if (!TestNotNull(TEXT("Ready fixture has a real animation instance"), Mesh->GetAnimInstance())) return false;
		TestTrue(TEXT("Ready authority retains normal damage cue"), Accepts(Damage, EGameplayCueEvent::Executed));
		Character->SetRole(ROLE_SimulatedProxy);
		TestTrue(TEXT("Ready client retains normal damage cue without AI controller readiness"), Accepts(ChildDamage, EGameplayCueEvent::Executed));
		VisualProperty->SetObjectPropertyValue_InContainer(Character, nullptr);
		TestFalse(TEXT("Late client without visual safely discards the burst"), Accepts(ChildDamage, EGameplayCueEvent::Executed));
		TestNull(TEXT("Query cannot fabricate client visual"), Character->GetCharacterVisual());
		Character->SetRole(ROLE_Authority);
		VisualProperty->SetObjectPropertyValue_InContainer(Character, Visual);
		TestTrue(TEXT("Subsequent ready burst recovers without timer or replay"), Accepts(Damage, EGameplayCueEvent::Executed));
	}

	return true;
}

#endif
