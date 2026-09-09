#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/TerritoryNPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCInteractable.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/LatentActionManager.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

// Advance real engine frames, so this exercises the Blueprint latent action rather
// than directly invoking the helper a second time or changing the frame counter.
class FTFStoryNPCVisualOrderCheck final : public IAutomationLatentCommand
{
public:
	FTFStoryNPCVisualOrderCheck(FAutomationTestBase* InTest, UWorld* InWorld, ANarrativeNPCCharacter* InNPC)
		: Test(InTest), World(InWorld), NPC(InNPC), ReadyAt(FPlatformTime::Seconds() + 0.1) {}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() < ReadyAt) return false;
		UNPCInteractable* Interaction = NPC->FindComponentByClass<UNPCInteractable>();
		{
			TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
			World->GetLatentActionManager().BeginFrame();
			World->GetLatentActionManager().ProcessLatentActions(NPC, 0.1f);
		}
		Test->TestEqual(bVisualBroadcast ? TEXT("Visual setup cannot leave a dead late-join proxy showing Talk")
			: TEXT("BeginPlay catches a death flag received before delegate binding"),
			Interaction->GetInteractableActionText(nullptr, nullptr).ToString(), FString(TEXT("Loot")));
		if (!bVisualBroadcast)
		{
			// Native broadcasts before its marker/equipment fixups. The adapter must
			// reconcile after the callback stack, including repeated visual rebuilds.
			{
				TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
				NPC->CharacterVisualInitialized.Broadcast(NPC);
				NPC->CharacterVisualInitialized.Broadcast(NPC);
			}
			Interaction->SetInteractableActionText(FText::FromString(TEXT("Talk")));
			Test->TestEqual(TEXT("Visual callback waits until Native finishes initialization"),
				World->GetLatentActionManager().GetNumActionsForObject(NPC), 1);
			bVisualBroadcast = true;
			ReadyAt = FPlatformTime::Seconds() + 0.1;
			return false;
		}
		Test->TestTrue(TEXT("Deferred reconciliation preserves Native death state"), NPC->GetNarrativeAbilitySystemComponent()->IsDead());
		Test->TestNull(TEXT("Deferred client reconciliation does not create an AI controller"), NPC->GetNPCController());
		NPC->SetRole(ROLE_Authority);
		World->DestroyWorld(false);
		return true;
	}
private:
	FAutomationTestBase* Test;
	UWorld* World;
	ANarrativeNPCCharacter* NPC;
	double ReadyAt;
	bool bVisualBroadcast = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStoryNPCBlueprintDeathRegression,
	"TerritoryFramework.Editor.Narrative.StoryNPCClientDeathAndInheritance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStoryNPCBlueprintDeathRegression::RunTest(const FString& Parameters)
{
	UClass* NativeNPC = LoadClass<ANarrativeNPCCharacter>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPC.BP_NarrativeNPC_C"));
	UClass* StoryNPC = LoadClass<ANarrativeNPCCharacter>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryStoryNPC.BP_TerritoryStoryNPC_C"));
	UClass* NativeController = LoadClass<ANarrativeNPCController>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPCController.BP_NarrativeNPCController_C"));
	UClass* StoryController = LoadClass<ANarrativeNPCController>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryStoryNPCController.BP_TerritoryStoryNPCController_C"));
	if (!TestTrue(TEXT("Story NPC preserves Native NPC Blueprint inheritance"),
		NativeNPC && StoryNPC && StoryNPC->IsChildOf(NativeNPC))
		|| !TestTrue(TEXT("Story controller preserves Native Blueprint inheritance"),
			NativeController && StoryController && StoryController->IsChildOf(NativeController))) return false;
	TestEqual(TEXT("Story NPC selects the compatible save controller"),
		StoryNPC->GetDefaultObject<ANarrativeNPCCharacter>()->AIControllerClass.Get(), StoryController);
	const UTerritoryNPCActivityComponent* Activity = Cast<UTerritoryNPCActivityComponent>(
		StoryController->GetDefaultObject<ANarrativeNPCController>()->GetActivityComponent());
	if (TestNotNull(TEXT("Story controller uses the existing Native save adapter"), Activity))
		TestTrue(TEXT("Generic story controller does not retire project-specific generator classes"),
			Activity->IgnoredSavedGoalGeneratorClasses.IsEmpty());

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Client Blueprint test world"), World)) return false;
	ANarrativeNPCCharacter* NPC = World->SpawnActor<ANarrativeNPCCharacter>(StoryNPC);
	UNarrativeAbilitySystemComponent* ASC = NPC->GetNarrativeAbilitySystemComponent();
	NPC->SetRole(ROLE_SimulatedProxy);
	TestNull(TEXT("A remote NPC has no authoritative AI controller"), NPC->GetNPCController());
	FBoolProperty* Dead = FindFProperty<FBoolProperty>(UNarrativeAbilitySystemComponent::StaticClass(), TEXT("bIsDead"));
	Dead->SetPropertyValue_InContainer(ASC, true);
	UFunction* Death = NPC->FindFunction(TEXT("HandleDeath"));
	FStructOnScope Arguments(Death);
	FindFProperty<FObjectPropertyBase>(Death, TEXT("KilledActor"))->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), NPC);
	FindFProperty<FObjectPropertyBase>(Death, TEXT("KilledActorASC"))->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), ASC);
	// An old event argument must not override the current replicated ASC state.
	FindFProperty<FBoolProperty>(Death, TEXT("bIsDead"))->SetPropertyValue_InContainer(Arguments.GetStructMemory(), false);
	{
		TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
		NPC->ProcessEvent(Death, Arguments.GetStructMemory());
	}
	TestEqual(TEXT("Actual child Blueprint routes remote death to safe presentation"),
		NPC->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestFalse(TEXT("Actual child Blueprint leaves ragdoll to Native replication"), NPC->IsRagdoll(false));
	TestTrue(TEXT("Native replicated death state is preserved"), ASC->IsDead());
	// A late join can miss the earlier ASC death notification. Run the actual
	// child Blueprint BeginPlay and then Native's public visual-ready delegate.
	NPC->FindComponentByClass<UNPCInteractable>()->SetInteractableActionText(FText::FromString(TEXT("Talk")));
	{
		TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
		NPC->ProcessEvent(NPC->FindFunction(TEXT("ReceiveBeginPlay")), nullptr);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FTFStoryNPCVisualOrderCheck(this, World, NPC));
	return true;
}

#endif
