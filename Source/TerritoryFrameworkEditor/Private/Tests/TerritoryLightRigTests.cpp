#include "TerritoryLightRigProbe.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/NarrativeLevelSequencePlayer.h"
#include "Cinematics/TerritoryDialogueShot.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Tales/Dialogue.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "EngineUtils.h"
#include "CineCameraActor.h"
#include "LevelSequence.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"

ATerritoryLightRigProbe::ATerritoryLightRigProbe()
{
	LightChild = CreateDefaultSubobject<UChildActorComponent>(TEXT("OwnedLight"));
	RootComponent = LightChild;
	LightChild->SetChildActorClass(AActor::StaticClass());
}
bool ATerritoryLightRigProbe::PrepareLightRig_Implementation(AActor* InVisual, ACameraActor* InCamera)
{
	bPreparedBeforeBeginPlay = !HasActorBegunPlay(); Visual = InVisual; Camera = InCamera; return Visual && Camera;
}
bool ATerritoryLightRigProbe::IsLightRigReady_Implementation() const { return Visual && Camera && LightChild->GetChildActor(); }
bool ATerritoryLightRigProbe::SetLightRigCamera_Implementation(ACameraActor* InCamera) { Camera = InCamera; ++CameraUpdates; return IsValid(Camera); }

bool ATerritoryLightRigTestDriver::StartTest(APlayerController* Viewer, AActor* Subject,
	UTerritoryCinematicLightRigProfile* Profile, ULevelSequence* Sequence)
{
	StopTest();
	if (!IsValid(Viewer) || !IsValid(Subject) || !Sequence) return false;
	FNarrativeSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true;
	Settings.bAutoPlay = false;
	Settings.TagsToApplyWhilstBound.Reset();
	ANarrativeLevelSequenceActor* Actor = nullptr;
	if (!ANarrativeLevelSequenceActor::CreateNarrativeLevelSequencePlayer(Viewer, {Viewer},
		Subject->GetActorLocation(), 0.f, Sequence, Settings, Actor)) return false;
	TestSequence = Actor;
	Actor->bOverrideInstanceData = true;
	if (auto* Origin = Cast<UDefaultLevelSequenceInstanceData>(Actor->DefaultInstanceData))
	{
		auto* HeadResolver = NewObject<UDialogue>(this);
		Origin->TransformOrigin = FTransform(Subject->GetActorRotation(), HeadResolver->GetSpeakerHeadLocation(Subject));
	}
	Actor->GetSequencePlayer()->Play();
	TestSession = UTerritoryCinematicLightRigComponent::FollowNarrativeSequence(Actor, Viewer, Subject, Profile);
	return TestSession != nullptr;
}

bool ATerritoryLightRigTestDriver::StartInPIEWorld(int32 PIEInstance, FName SubjectName,
	UTerritoryCinematicLightRigProfile* Profile, ULevelSequence* Sequence)
{
	// Explicit editor fixture world selection; no production first-player fallback.
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType != EWorldType::PIE || Context.PIEInstance != PIEInstance || !Context.World()) continue;
		APlayerController* Viewer = nullptr;
		for (auto It = Context.World()->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->IsValid() && It->Get()->IsLocalPlayerController())
			{
				if (Viewer) return false;
				Viewer = It->Get();
			}
		}
		for (TActorIterator<AActor> It(Context.World()); It; ++It)
			if (It->GetFName() == SubjectName) return StartTest(Viewer, *It, Profile, Sequence);
	}
	return false;
}

bool UTerritoryLightRigTestDialogue::Initialize(UTalesComponent* Component, const FDialoguePlayParams Params)
{
	if (!Component || HasAnyFlags(RF_ClassDefaultObject)) return false;
	OwningComp = Component;
	OwningController = Component->GetOwningController();
	OwningPawn = Component->GetOwningPawn();
	PlayParams = Params;
	bFreeMovement = false;
	bAutoStopMovement = false;
	bAdjustPlayerTransform = false;
	DialogueBlendOutTime = 0.f;
	FSpeakerInfo Speaker;
	Speaker.SpeakerID = TEXT("LightRigSpeaker");
	Speakers.Add(Speaker);
	auto* Node = NewObject<UDialogueNode_NPC>(this);
	Node->SetID(TEXT("HeldLightRigLine"));
	Node->SetSpeakerID(Speaker.SpeakerID);
	Node->Line.Text = FText::FromString(TEXT("Optional character lights verification. Exit when finished."));
	Node->Line.Duration = ELineDuration::LD_Never;
	Node->OwningDialogue = this;
	Node->OwningComponent = Component;
	NPCReplies.Add(Node);
	RootDialogue = Node;
	return !Component->HasAuthority() || GenerateDialogueChunk(RootDialogue);
}

void UTerritoryLightRigTestDialogue::UseShot(UTerritoryDialogueShot* Shot)
{
	DefaultDialogueShot = Shot;
	PlayDialogueSequence(Shot, GetCurrentSpeakerAvatar(), GetCurrentListenerAvatar());
}

bool ATerritoryLightRigTestDriver::StartDialogueTest(UTalesComponent* Tales, ANarrativeNPCCharacter* Speaker,
	UTerritoryCinematicLightRigProfile* Profile, ULevelSequence* Sequence)
{
	if (!IsValid(Tales) || !Tales->HasAuthority() || !IsValid(Speaker) || !Sequence || !Profile
		|| Tales->GetWorld() != Speaker->GetWorld()) return false;
	FDialoguePlayParams Params;
	Params.Speakers.Add(Speaker);
	if (!Tales->BeginDialogue(UTerritoryLightRigTestDialogue::StaticClass(), Params)) return false;
	auto* Dialogue = Cast<UTerritoryLightRigTestDialogue>(Tales->GetCurrentDialogue());
	if (!Dialogue || !Dialogue->GetCurrentSpeakerAvatar()) return false;
	auto* Shot = NewObject<UTerritoryDialogueShot>(Dialogue);
	Shot->ConfigureEditorShot(Sequence, FText::FromString(TEXT("Optional lights verification")),
		ETerritoryDialogueShotRole::MediumCloseUp, EAnchorOriginRule::AOR_Speaker,
		EShotTrackingRule::STR_Speaker, 65.f, 2.8f, false);
	Shot->LightRigProfile = Profile;
	Dialogue->UseShot(Shot);
	return Dialogue->GetCurrentDialogueSequence() == Shot;
}

bool ATerritoryLightRigTestDriver::ReplayDialogueTest(UTalesComponent* Tales, bool UseListener)
{
	auto* Dialogue = Tales ? Cast<UTerritoryLightRigTestDialogue>(Tales->GetCurrentDialogue()) : nullptr;
	auto* Shot = Dialogue ? Cast<UTerritoryDialogueShot>(Dialogue->GetCurrentDialogueSequence()) : nullptr;
	if (!Shot) return false;
	Shot->bLightRigUsesListener = UseListener;
	Dialogue->UseShot(Shot);
	return Dialogue->GetCurrentDialogueSequence() == Shot;
}
bool ATerritoryLightRigTestDriver::ChangeCamera()
{
	if (!IsValid(TestSequence) || !TestSequence->GetSequencePlayer()) return false;
	UCameraComponent* Previous = TestSequence->GetSequencePlayer()->GetActiveCameraComponent();
	if (!Previous) return false;
	OverrideCamera = TestSequence->GetWorld()->SpawnActor<ACineCameraActor>(Previous->GetComponentLocation(), Previous->GetComponentRotation());
	TestSequence->SetBindingByTag(TEXT("Cinecam"), {OverrideCamera}, false);
	TestSequence->GetSequencePlayer()->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(0.2f, EUpdatePositionMethod::Play));
	return true;
}
void ATerritoryLightRigTestDriver::PauseTest() { if (IsValid(TestSequence)) TestSequence->GetSequencePlayer()->Pause(); }
void ATerritoryLightRigTestDriver::ResumeTest() { if (IsValid(TestSequence)) TestSequence->GetSequencePlayer()->Play(); }
void ATerritoryLightRigTestDriver::StopTest()
{
	if (IsValid(TestSequence)) { TestSequence->GetSequencePlayer()->Stop(); TestSequence->Destroy(); }
	if (IsValid(OverrideCamera)) OverrideCamera->Destroy();
	TestSequence = nullptr; TestSession = nullptr; OverrideCamera = nullptr;
}
void ATerritoryLightRigTestDriver::EndPlay(const EEndPlayReason::Type Reason) { StopTest(); Super::EndPlay(Reason); }

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryLightRigLifecycle,
	"TerritoryFramework.Presentation.Cinematics.OptionalLightRigLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryLightRigLifecycle::RunTest(const FString&)
{
	// This isolated world has no game loop. Allow the real interface dispatch
	// during the fixture, exactly as the editor does for CallInEditor actions.
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
	TestTrue(TEXT("One explicit local viewport can use cosmetic lights"), UTerritoryCinematicLightRigComponent::CanUseLights(Viewer, World));
	TestFalse(TEXT("A missing viewer cannot fall back to player zero"), UTerritoryCinematicLightRigComponent::CanUseLights(nullptr, World));
	auto* SecondPlayer = NewObject<ULocalPlayer>(GEngine);
	Instance->AddLocalPlayer(SecondPlayer, FPlatformUserId::CreateFromInternalId(1));
	TestFalse(TEXT("Split screen skips shared-world lights"), UTerritoryCinematicLightRigComponent::CanUseLights(Viewer, World));
	Instance->RemoveLocalPlayer(SecondPlayer);

	auto* Profile = NewObject<UTerritoryCinematicLightRigProfile>();
	FString Reason;
	TestFalse(TEXT("Missing pack configuration fails explicitly"), Profile->HasValidConfiguration(Reason));
	Profile->RigClass = ATerritoryLightRigProbe::StaticClass();
	Profile->MeshRequirements.Add({TEXT("Body"), {}});
	TestTrue(TEXT("A supported optional runtime rig is accepted"), Profile->HasValidConfiguration(Reason));
	Profile->MeshRequirements.Add({TEXT("Body"), {}});
	TestFalse(TEXT("Duplicate mesh requirements are rejected"), Profile->HasValidConfiguration(Reason));
	Profile->MeshRequirements.Pop();
	auto* Subject = World->SpawnActor<AActor>();
	auto* Mesh = NewObject<USkeletalMeshComponent>(Subject, TEXT("Body"));
	Subject->AddInstanceComponent(Mesh);
	TestFalse(TEXT("An actor without a ready visual cannot spawn a rig"), Profile->IsVisualReady(Subject));
	Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube")));
	Mesh->RegisterComponent();
	TestTrue(TEXT("The named registered mesh becomes ready"), Profile->IsVisualReady(Subject));
	Profile->MeshRequirements[0].RequiredSockets.Add(TEXT("missing_bone"));
	TestFalse(TEXT("Unready bones block initialization"), Profile->IsVisualReady(Subject));
	Profile->MeshRequirements[0].RequiredSockets.Reset();

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
	Session->RefreshRig(0.f);
	TestNull(TEXT("No profile means no extra lighting"), Session->GetSpawnedLightRig());
	Session->ActiveProfile = Profile;
	Session->RefreshRig(0.f);
	auto* Rig = Cast<ATerritoryLightRigProbe>(Session->GetSpawnedLightRig());
	if (TestNotNull(TEXT("A ready subject spawns one runtime rig"), Rig))
	{
		TestTrue(TEXT("Visual and camera are assigned before BeginPlay"), Rig->bPreparedBeforeBeginPlay);
		TestTrue(TEXT("Temporary lights are not saved or replicated"), Rig->HasAnyFlags(RF_Transient) && !Rig->GetIsReplicated());
		AActor* Child = Rig->LightChild->GetChildActor();
		Player->CutTo(SecondCamera->GetCameraComponent());
		Viewer->SetViewTarget(SecondCamera);
		Session->RefreshRig(0.f);
		TestEqual(TEXT("A camera cut reuses the same rig"), Session->GetSpawnedLightRig(), static_cast<AActor*>(Rig));
		TestEqual(TEXT("Only a changed camera is rebound"), Rig->CameraUpdates, 1);
		Session->RefreshRig(0.f);
		TestEqual(TEXT("Ordinary follow updates do not rebuild or rebind"), Rig->CameraUpdates, 1);
		TestEqual(TEXT("Existing child lights survive camera cuts"), Rig->LightChild->GetChildActor(), Child);
		Player->HoldFinalFrame();
		Session->PlaybackEnded();
		TestEqual(TEXT("OnFinished from pause-at-end keeps the displayed shot lit"), Session->GetSpawnedLightRig(), static_cast<AActor*>(Rig));
		Viewer->SetViewTarget(Owner);
		Session->RefreshRig(0.f);
		TestNull(TEXT("Returning to gameplay releases a paused sequence's lights"), Session->GetSpawnedLightRig());
		Session->StopLightRig();
		TestTrue(TEXT("Session removal destroys its rig"), !IsValid(Rig) || Rig->IsActorBeingDestroyed());
		TestTrue(TEXT("Owned light actors are also removed"), !IsValid(Child) || Child->IsActorBeingDestroyed());
	}
	Instance->RemoveLocalPlayer(LocalPlayer);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
