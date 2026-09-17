#include "TerritoryLightElementProbe.h"
#include "TerritoryLightRigProbe.h"
#include "Camera/CameraActor.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/PointLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UTerritoryLightElementAdapterProbe::UTerritoryLightElementAdapterProbe()
{
	RequiredRigInterface = UTerritoryLightElementProbeInterface::StaticClass();
	bUpdateEveryFrame = true;
}
bool UTerritoryLightElementAdapterProbe::PrepareRig_Implementation()
{
	auto* Element = Cast<ATerritoryLightElementProbe>(Rig);
	bPreparedBeforeConstruction = Element && !Element->bConstructed;
	return IsValid(Element) && IsValid(Visual) && IsValid(Camera);
}
bool UTerritoryLightElementAdapterProbe::ActivateRig_Implementation()
{
	auto* Element = Cast<ATerritoryLightElementProbe>(Rig);
	bActive = Element && Element->bConstructed;
	return bActive && (SceneTag.IsNone() || RefreshTaggedSceneLights(SceneTag, 0.f, true));
}
bool UTerritoryLightElementAdapterProbe::IsRigReady_Implementation() const { return bActive && IsValid(Rig); }
bool UTerritoryLightElementAdapterProbe::UpdateCamera_Implementation() { ++CameraChanges; return IsValid(Camera); }
bool UTerritoryLightElementAdapterProbe::UpdateRig_Implementation(float DeltaSeconds)
{
	++Updates;
	if (!SceneTag.IsNone() && !RefreshTaggedSceneLights(SceneTag, DeltaSeconds, false)) return false;
	for (ALight* Light : GetCapturedSceneLights()) Light->GetLightComponent()->SetIntensity(987.f);
	return IsValid(Rig);
}
void UTerritoryLightElementAdapterProbe::ReleaseRig_Implementation() { ++Releases; bActive = false; /* Deliberately omit Super: shutdown must still restore. */ }

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryLightElementAdapterTest,
	"TerritoryFramework.Presentation.Cinematics.DirectLightElementAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryLightElementAdapterTest::RunTest(const FString&)
{
	TGuardValue<bool> ScriptExecution(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Instance = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(Instance);
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	World->InitializeActorsForPlay(FURL());
	auto* Viewer = World->SpawnActor<APlayerController>();
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	Instance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));
	Viewer->SetPlayer(LocalPlayer);
	auto* Subject = World->SpawnActor<AActor>();
	auto* Mesh = NewObject<USkeletalMeshComponent>(Subject, TEXT("Body"));
	Subject->AddInstanceComponent(Mesh);
	Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube")));
	Mesh->RegisterComponent();
	auto* Profile = NewObject<UTerritoryCinematicLightRigProfile>();
	Profile->RigClass = ATerritoryLightElementProbe::StaticClass();
	Profile->MeshRequirements.Add({TEXT("Body"), {}});
	FString Reason;
	TestFalse(TEXT("Raw element cannot pretend to implement the whole-rig interface"), Profile->HasValidConfiguration(Reason));
	auto* Template = NewObject<UTerritoryLightElementAdapterProbe>(Profile);
	Template->SceneTag = TEXT("TerritoryLightAudit");
	Profile->ElementAdapter = Template;
	TestTrue(TEXT("A matching adapter accepts the raw element class"), Profile->HasValidConfiguration(Reason));
	Template->bRequiresCharacterMeshes = false;
	Profile->MeshRequirements.Reset();
	TestTrue(TEXT("A background adapter can use an actor anchor without mesh requirements"), Profile->HasValidConfiguration(Reason) && Profile->IsVisualReady(Subject));
	Template->bRequiresCharacterMeshes = true;
	Profile->MeshRequirements.Add({TEXT("Body"), {}});
	Profile->RigClass = AActor::StaticClass();
	TestFalse(TEXT("An unrelated actor is rejected"), Profile->HasValidConfiguration(Reason));
	Profile->RigClass = ATerritoryLightElementProbe::StaticClass();
	TestNull(TEXT("An authoring template has no gameplay world"), Template->GetWorld());
	auto* Copy = DuplicateObject<UTerritoryCinematicLightRigProfile>(Profile, GetTransientPackage());
	TestTrue(TEXT("Duplicated profile owns a distinct adapter configuration"), Copy->ElementAdapter && Copy->ElementAdapter != Template && Copy->ElementAdapter->GetOuter() == Copy);
	auto* Light = World->SpawnActor<APointLight>();
	Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Light->Tags.Add(Template->SceneTag);
	Light->GetLightComponent()->SetIntensity(123.f);
	Light->GetLightComponent()->SetLightFColor(FColor(41, 127, 211));
	Light->GetLightComponent()->SetUseTemperature(true);
	Light->GetLightComponent()->SetTemperature(4200.f);
	Light->GetLightComponent()->SetCastRaytracedShadows(ECastRayTracedShadow::UseProjectSetting);
	Light->GetLightComponent()->SetSamplesPerPixel(3);
	auto* Owner = World->SpawnActor<AActor>();
	auto* Player = NewObject<UTerritoryLightRigPlayerProbe>(Owner);
	auto* FirstCamera = World->SpawnActor<ACameraActor>();
	auto* SecondCamera = World->SpawnActor<ACameraActor>();
	Player->CutTo(FirstCamera->GetCameraComponent());
	Viewer->SetViewTarget(FirstCamera);
	auto* Session = NewObject<UTerritoryCinematicLightRigComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(Session);
	Session->RegisterComponent();
	Session->LocalViewer = Viewer;
	Session->SequencePlayer = Player;
	Session->SubjectActor = Subject;
	Session->ActiveProfile = Profile;
	Session->RefreshRig(0.f);
	auto* Element = Cast<ATerritoryLightElementProbe>(Session->GetSpawnedLightRig());
	auto* Adapter = Cast<UTerritoryLightElementAdapterProbe>(Session->RuntimeAdapter);
	if (TestNotNull(TEXT("The selected raw class is the actual spawned actor"), Element)
		&& TestNotNull(TEXT("Each session has a runtime adapter"), Adapter))
	{
		TestTrue(TEXT("Prepare precedes construction and activation follows it"), Adapter->bPreparedBeforeConstruction && Adapter->bActive);
		TestTrue(TEXT("Adapter is private, transient and owned by this session"), Adapter != Template && Adapter->GetOuter() == Session && Adapter->HasAnyFlags(RF_Transient));
		TestNull(TEXT("Template is never given a live actor"), Template->Rig.Get());
		TestEqual(TEXT("Frame-driven elements use a zero tick interval"), Session->GetComponentTickInterval(), 0.f);
		Session->RefreshRig(0.016f);
		TestEqual(TEXT("The adapter really updates the captured light"), Light->GetLightComponent()->Intensity, 987.f);
		Player->CutTo(SecondCamera->GetCameraComponent());
		Viewer->SetViewTarget(SecondCamera);
		Session->RefreshRig(0.016f);
		TestEqual(TEXT("Camera cut keeps the raw actor"), Session->GetSpawnedLightRig(), static_cast<AActor*>(Element));
		TestEqual(TEXT("Camera cut updates the adapter once"), Adapter->CameraChanges, 1);
		TestEqual(TEXT("Template counters stay untouched"), Template->Updates, 0);

		TArray<uint8> Bytes;
		FMemoryWriter SaveBuffer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Save(SaveBuffer, false);
		Adapter->Serialize(Save);
		auto* Loaded = NewObject<UTerritoryLightElementAdapterProbe>();
		FMemoryReader LoadBuffer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Load(LoadBuffer, true);
		Loaded->Serialize(Load);
		TestEqual(TEXT("Serialized configuration retains the authored actor tag"), Loaded->SceneTag, Template->SceneTag);
		TestNull(TEXT("Serialization does not restore a runtime actor pointer"), Loaded->Rig.Get());
		TestEqual(TEXT("Serialization does not restore live scene leases"), Loaded->GetCapturedSceneLights().Num(), 0);

		auto* Competing = NewObject<UTerritoryLightElementAdapterProbe>(Owner);
		auto* OtherElement = World->SpawnActor<ATerritoryLightElementProbe>();
		TestTrue(TEXT("Competing raw actor can prepare"), Competing->Initialize(OtherElement, Subject, FirstCamera));
		TestFalse(TEXT("Overlapping scene targets are refused atomically"), Competing->RefreshTaggedSceneLights(Template->SceneTag, 0.f, true));
		TestEqual(TEXT("Refused capture leaves the owner's light untouched"), Light->GetLightComponent()->Intensity, 987.f);
		Competing->Shutdown();
		OtherElement->Destroy();

		Light->GetLightComponent()->SetLightFColor(FColor::Red);
		Light->GetLightComponent()->SetUseTemperature(false);
		Light->GetLightComponent()->SetTemperature(1700.f);
		Light->GetLightComponent()->SetCastRaytracedShadows(ECastRayTracedShadow::Enabled);
		Light->GetLightComponent()->SetSamplesPerPixel(7);
		Light->Tags.Reset();
		TestTrue(TEXT("Tag removal reconciles the target set"), Adapter->RefreshTaggedSceneLights(Template->SceneTag, 0.5f, false));
		TestEqual(TEXT("Removed target restores original intensity"), Light->GetLightComponent()->Intensity, 123.f);
		TestEqual(TEXT("Removed target restores exact stored color"), Light->GetLightComponent()->LightColor, FColor(41, 127, 211));
		TestTrue(TEXT("Removed target restores temperature switch"), bool(Light->GetLightComponent()->bUseTemperature));
		TestEqual(TEXT("Removed target restores temperature"), Light->GetLightComponent()->Temperature, 4200.f);
		TestEqual(TEXT("Removed target restores samples"), Light->GetLightComponent()->SamplesPerPixel, 3);
		TestEqual(TEXT("Removed target restores project shadow mode"), Light->GetLightComponent()->CastRaytracedShadow.GetValue(), ECastRayTracedShadow::UseProjectSetting);
		auto* LateLight = World->SpawnActor<APointLight>();
		LateLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		LateLight->Tags.Add(Template->SceneTag);
		LateLight->GetLightComponent()->SetIntensity(456.f);
		Session->RefreshRig(0.5f);
		TestEqual(TEXT("A later-loaded light is found and updated"), LateLight->GetLightComponent()->Intensity, 987.f);
		LateLight->Destroy();
		TestEqual(TEXT("Unloaded/destroyed lights are excluded immediately"), Adapter->GetCapturedSceneLights().Num(), 0);
		Light->Tags.Add(Template->SceneTag);
		Session->RefreshRig(0.5f);
		TestEqual(TEXT("A restored target can be captured again"), Light->GetLightComponent()->Intensity, 987.f);
		Session->StopLightRig();
		TestEqual(TEXT("Shutdown restores even when Blueprint omits its parent cleanup"), Light->GetLightComponent()->Intensity, 123.f);
		TestEqual(TEXT("Release is called exactly once"), Adapter->Releases, 1);
		Adapter->Shutdown();
		TestEqual(TEXT("Repeated cleanup is idempotent"), Adapter->Releases, 1);
		TestTrue(TEXT("The raw element is destroyed"), !IsValid(Element) || Element->IsActorBeingDestroyed());
	}
	Instance->RemoveLocalPlayer(LocalPlayer);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
