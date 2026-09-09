#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryGuardCharacter.h"
#include "AI/NPCInteractable.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "NarrativeArsenal.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNPCClientDeathPresentation,
	"TerritoryFramework.AI.Regression.NPCClientDeathPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNPCClientDeathPresentation::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Missing NPC is rejected"),
		UTerritoryBlueprintLibrary::UpdateNarrativeNPCClientDeathPresentation(nullptr));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Client presentation test world"), World)) return false;
	// Territory's C++ child keeps Native's ASC/interactable and valid collision profiles.
	ANarrativeNPCCharacter* NPC = World->SpawnActor<ATerritoryGuardCharacter>();
	UNarrativeAbilitySystemComponent* ASC = NPC->GetNarrativeAbilitySystemComponent();
	UNPCInteractable* Interactable = NPC->FindComponentByClass<UNPCInteractable>();
	FBoolProperty* Dead = FindFProperty<FBoolProperty>(UNarrativeAbilitySystemComponent::StaticClass(), TEXT("bIsDead"));
	if (!TestNotNull(TEXT("Native ASC owns the replicated death flag"), Dead)
		|| !TestNotNull(TEXT("Native NPC interactable exists"), Interactable))
	{
		World->DestroyWorld(false);
		return false;
	}
	const float HealthBefore = NPC->GetHealth();
	Dead->SetPropertyValue_InContainer(ASC, true);
	TestFalse(TEXT("Client helper cannot replace server death handling"),
		UTerritoryBlueprintLibrary::UpdateNarrativeNPCClientDeathPresentation(NPC));
	TestEqual(TEXT("Rejected server call leaves collision unchanged"),
		NPC->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
	NPC->SetRole(ROLE_SimulatedProxy);
	TestNull(TEXT("Regression fixture has no client-side AI controller"), NPC->GetNPCController());
	NPC->GetCharacterMovement()->Velocity = FVector(300.f, 0.f, 0.f);
	for (int32 Repeat = 0; Repeat < 3; ++Repeat)
	{
		TestTrue(TEXT("Replicated dead state safely updates a controllerless client"),
			UTerritoryBlueprintLibrary::UpdateNarrativeNPCClientDeathPresentation(NPC));
	}
	TestTrue(TEXT("Dead proxy stops its old locomotion sample"), NPC->GetVelocity().IsNearlyZero());
	TestEqual(TEXT("Dead proxy no longer blocks pawns"),
		NPC->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestEqual(TEXT("Dead mesh remains lootable"),
		NPC->GetMesh()->GetCollisionResponseToChannel(TraceChannel_NarrativeInteraction), ECR_Block);
	TestEqual(TEXT("Native interaction shows Loot"),
		Interactable->GetInteractableActionText(nullptr, nullptr).ToString(), FString(TEXT("Loot")));
	TestFalse(TEXT("Presentation waits for separate Native ragdoll replication"), NPC->IsRagdoll(false));
	TestTrue(TEXT("Presentation does not change Native death state"), ASC->IsDead());
	TestEqual(TEXT("Presentation never writes health"), NPC->GetHealth(), HealthBefore);
	Dead->SetPropertyValue_InContainer(ASC, false);
	TestTrue(TEXT("Restored live state updates presentation without resetting attributes"),
		UTerritoryBlueprintLibrary::UpdateNarrativeNPCClientDeathPresentation(NPC));
	TestEqual(TEXT("Live proxy blocks pawns again"),
		NPC->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
	TestEqual(TEXT("Live mesh restores its interaction response"),
		NPC->GetMesh()->GetCollisionResponseToChannel(TraceChannel_NarrativeInteraction), ECR_Ignore);
	TestEqual(TEXT("Live prompt returns to its authored archetype"),
		Interactable->GetInteractableActionText(nullptr, nullptr).ToString(), FString(TEXT("Talk")));
	TestEqual(TEXT("Live reconciliation still does not write attributes"), NPC->GetHealth(), HealthBefore);
	World->bIsTearingDown = true;
	TestFalse(TEXT("World teardown does not run presentation"),
		UTerritoryBlueprintLibrary::UpdateNarrativeNPCClientDeathPresentation(NPC));
	NPC->SetRole(ROLE_Authority);
	World->DestroyWorld(false);
	const UFunction* Function = UTerritoryBlueprintLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UTerritoryBlueprintLibrary, UpdateNarrativeNPCClientDeathPresentation));
	TestTrue(TEXT("Presentation has an execution pin and is not a network mutation"),
		Function && Function->HasAllFunctionFlags(FUNC_BlueprintCallable)
		&& !Function->HasAnyFunctionFlags(FUNC_BlueprintPure | FUNC_Net | FUNC_BlueprintAuthorityOnly));
	return true;
}

#endif
