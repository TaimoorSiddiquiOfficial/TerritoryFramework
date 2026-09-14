#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GroomComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Tales/Dialogue.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritorySharedDialogueLOD,
	"TerritoryFramework.Presentation.Cinematics.SharedSpeakerLODReleaseOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritorySharedDialogueLOD::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* First = NewObject<UTerritoryCinematicPresentationSubsystem>(NewObject<ULocalPlayer>(GEngine));
	auto* Second = NewObject<UTerritoryCinematicPresentationSubsystem>(NewObject<ULocalPlayer>(GEngine));
	auto* Subject = World->SpawnActor<AActor>();
	auto* Sync = NewObject<ULODSyncComponent>(Subject);
	auto* Mesh = NewObject<USkeletalMeshComponent>(Subject);
	auto* Groom = NewObject<UGroomComponent>(Subject);
	// SetForcedLOD clamps against render-data LOD count. An empty component
	// cannot exercise nonzero restoration; supply transient data without loading
	// a project character or registering an incomplete mesh with the renderer.
	auto* MeshAsset = NewObject<USkeletalMesh>();
	MeshAsset->AllocateResourceForRendering();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		MeshAsset->AddLODInfo();
		MeshAsset->GetResourceForRendering()->LODRenderData.Add(new FSkeletalMeshLODRenderData());
	}
	Mesh->SetSkeletalMeshAsset(MeshAsset);
	TestEqual(TEXT("Fixture exposes three usable LOD indices"), Mesh->GetNumLODs(), 3);
	Subject->AddInstanceComponent(Sync);
	Subject->AddInstanceComponent(Mesh);
	Subject->AddInstanceComponent(Groom);
	auto* FirstDialogue = NewObject<UDialogue>(First);
	auto* SecondDialogue = NewObject<UDialogue>(Second);
	for (bool bFirstEndsFirst : { true, false })
	{
		Sync->ForcedLOD = 3;
		Mesh->SetForcedLOD(2);
		Groom->SetForcedLOD(4);
		const int32 PreviousMesh = Mesh->GetForcedLOD();
		const int32 PreviousGroom = Groom->GetForcedLOD();
		First->HandleDialogueBegan(FirstDialogue);
		Second->HandleDialogueBegan(SecondDialogue);
		First->RegisterCinematicSubject(Subject);
		Second->RegisterCinematicSubject(Subject);
		Second->RegisterCinematicSubject(Subject);
		TestEqual(TEXT("Repeated registration keeps one entry per component"), Second->ComponentLODOverrides.Num(), 3);
		(bFirstEndsFirst ? First : Second)->ClearPresentation();
		TestEqual(TEXT("Shared speaker remains detailed while either dialogue owns it"), Sync->ForcedLOD, 0);
		TestEqual(TEXT("Shared mesh stays at render LOD zero"), Mesh->GetForcedLOD(), 1);
		TestEqual(TEXT("Shared groom stays at LOD zero"), Groom->GetForcedLOD(), 0);
		(bFirstEndsFirst ? Second : First)->ClearPresentation();
		TestEqual(TEXT("Last dialogue restores the original LODSync value"), Sync->ForcedLOD, 3);
		TestEqual(TEXT("Last dialogue restores the original mesh value"), Mesh->GetForcedLOD(), PreviousMesh);
		TestEqual(TEXT("Last dialogue restores the original groom value"), Groom->GetForcedLOD(), PreviousGroom);
	}
	First->HandleDialogueBegan(FirstDialogue);
	Second->HandleDialogueBegan(SecondDialogue);
	First->RegisterCinematicSubject(Subject);
	Second->RegisterCinematicSubject(Subject);
	First->Deinitialize();
	TestEqual(TEXT("Removing a local player preserves another player's detail request"), Sync->ForcedLOD, 0);
	Sync->ForcedLOD = 5;
	Mesh->SetForcedLOD(3);
	Groom->SetForcedLOD(2);
	Second->ClearPresentation();
	TestEqual(TEXT("A later external LODSync setting is preserved"), Sync->ForcedLOD, 5);
	TestEqual(TEXT("A later external mesh setting is preserved"), Mesh->GetForcedLOD(), 3);
	TestEqual(TEXT("A later external groom setting is preserved"), Groom->GetForcedLOD(), 2);
	Second->HandleDialogueBegan(SecondDialogue);
	Second->RegisterCinematicSubject(Subject);
	Subject->Destroy();
	Second->ClearPresentation();
	TestTrue(TEXT("Removed visuals release all temporary records"), Second->ComponentLODOverrides.IsEmpty());
	Second->Deinitialize();
	World->DestroyWorld(false);
	return true;
}

#endif
