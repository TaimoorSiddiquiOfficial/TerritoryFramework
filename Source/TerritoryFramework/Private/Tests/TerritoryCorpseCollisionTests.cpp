#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AI/TerritoryDeathCollisionState.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeArsenal.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCorpseCollision,
	"TerritoryFramework.AI.Regression.CorpseCollisionCameraPlayerAndRevival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCorpseCollision::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
		.RequiresHitProxies(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	if (!TestNotNull(TEXT("Collision world"), World)) return false;
	const FBoolProperty* Dead = FindFProperty<FBoolProperty>(
		UNarrativeAbilitySystemComponent::StaticClass(), TEXT("bIsDead"));
	if (!TestNotNull(TEXT("Native death flag"), Dead)) { World->DestroyWorld(false); return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	for (UClass* Class : {ATerritoryGuardCharacter::StaticClass(), ATerritoryAssaultCharacter::StaticClass()})
	{
		ANarrativeNPCCharacter* NPC = World->SpawnActor<ANarrativeNPCCharacter>(Class,
			FVector(0, 0, 100), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("Territory NPC"), NPC)) continue;
		auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
		auto* Capsule = NPC->GetCapsuleComponent();
		auto* Mesh = NPC->GetMesh();
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Overlap);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Block);
		Mesh->CanCharacterStepUpOn = ECB_Yes;
		const auto MeshCollision = Mesh->GetCollisionEnabled();
		const auto MeshObjectType = Mesh->GetCollisionObjectType();
		FTerritoryDeathCollisionState State;
		State.Refresh(*NPC);
		TestEqual(TEXT("Alive collision is not changed"), Mesh->GetCollisionResponseToChannel(ECC_Camera), ECR_Overlap);
		auto CameraHitsNPC = [&]()
		{
			FHitResult Hit;
			World->LineTraceSingleByChannel(Hit, FVector(-200, 0, 100), FVector(200, 0, 100), ECC_Camera);
			return Hit.GetActor() == NPC;
		};
		TestTrue(TEXT("Before death the camera sweep hits the real capsule"), CameraHitsNPC());
		Dead->SetPropertyValue_InContainer(ASC, true);
		State.Refresh(*NPC);
		State.Refresh(*NPC);
		TestFalse(TEXT("Dead capsule no longer obstructs the camera"), CameraHitsNPC());
		for (UPrimitiveComponent* Component : {static_cast<UPrimitiveComponent*>(Capsule), static_cast<UPrimitiveComponent*>(Mesh)})
		{
			TestEqual(TEXT("Corpse ignores player collision"), Component->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
			TestEqual(TEXT("Corpse ignores camera collision"), Component->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
			TestEqual(TEXT("Corpse cannot become a walking base"), Component->CanCharacterStepUpOn.GetValue(), ECB_No);
			TestEqual(TEXT("Ground collision remains"), Component->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
		}
		TestEqual(TEXT("Loot remains traceable"), Mesh->GetCollisionResponseToChannel(TraceChannel_NarrativeInteraction), ECR_Block);
		TestEqual(TEXT("Helper does not take over physics mode"), Mesh->GetCollisionEnabled(), MeshCollision);
		TestEqual(TEXT("Helper does not take over object type"), Mesh->GetCollisionObjectType(), MeshObjectType);
		TestFalse(TEXT("Helper does not author Native ragdoll"), NPC->IsRagdoll(false));
		Dead->SetPropertyValue_InContainer(ASC, false);
		State.Refresh(*NPC);
		TestTrue(TEXT("Revival restores capsule camera collision"), CameraHitsNPC());
		TestEqual(TEXT("Revival restores runtime mesh response, not CDO default"), Mesh->GetCollisionResponseToChannel(ECC_Camera), ECR_Overlap);
		TestEqual(TEXT("Revival restores runtime pawn response"), Mesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
		TestEqual(TEXT("Revival restores original step policy"), Mesh->CanCharacterStepUpOn.GetValue(), ECB_Yes);

		// The real class hook consumes Native state and must apply the same policy.
		Dead->SetPropertyValue_InContainer(ASC, true);
		if (auto* Guard = Cast<ATerritoryGuardCharacter>(NPC)) Guard->ReconcileNarrativeDeathState(ASC, false);
		if (auto* Assault = Cast<ATerritoryAssaultCharacter>(NPC)) Assault->ReconcileNarrativeDeathState(ASC, false);
		TestTrue(TEXT("Native still starts ragdoll"), NPC->IsRagdoll(false));
		TestFalse(TEXT("Class death hook clears camera obstruction"), CameraHitsNPC());
		TestTrue(TEXT("Death remains authored by Native ASC"), ASC->IsDead());
		FByteProperty* Role = FindFProperty<FByteProperty>(AActor::StaticClass(), TEXT("Role"));
		if (TestNotNull(TEXT("Simulated proxy role"), Role))
		{
			Role->SetPropertyValue_InContainer(NPC, ROLE_SimulatedProxy);
			Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Ignore);
			if (auto* Guard = Cast<ATerritoryGuardCharacter>(NPC)) Guard->ReconcileNarrativeDeathState(ASC, false);
			if (auto* Assault = Cast<ATerritoryAssaultCharacter>(NPC)) Assault->ReconcileNarrativeDeathState(ASC, false);
			TestEqual(TEXT("Late proxy initialization restores Native loot collision"),
				Mesh->GetCollisionResponseToChannel(TraceChannel_NarrativeInteraction), ECR_Block);
			TestTrue(TEXT("Late presentation keeps replicated ragdoll"), NPC->IsRagdoll(false));
			TestFalse(TEXT("Late presentation still ignores camera"), CameraHitsNPC());
			Role->SetPropertyValue_InContainer(NPC, ROLE_Authority);
		}
		TestTrue(TEXT("Corpse can be removed"), NPC->Destroy());
		State.Refresh(*NPC); // Removed actors must not resurrect collision or retain components.
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
