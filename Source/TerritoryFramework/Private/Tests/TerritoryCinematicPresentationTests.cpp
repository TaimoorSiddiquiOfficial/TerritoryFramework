#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Cinematics/TerritoryDialogueShot.h"
#include "CineCameraComponent.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Components/LODSyncComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Tales/Dialogue.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicDefaults,
	"TerritoryFramework.Presentation.Cinematics.SafeNarrativeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicDefaults::RunTest(const FString& Parameters)
{
	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	TestTrue(TEXT("Territory capture HUD is hidden during Narrative dialogue"),
		Settings->bHideTerritoryHUDDuringNarrativeDialogue);
	TestTrue(TEXT("Dialogue participant LOD quality is enabled"),
		Settings->bUseCinematicParticipantLODDuringDialogue);
	TestTrue(TEXT("Attached MetaHuman visual actors receive cinematic LOD"),
		Settings->bIncludeAttachedActorsInCinematicLOD);

	UTerritoryDialogueShot* Shot = NewObject<UTerritoryDialogueShot>();
	TestEqual(TEXT("Territory shots use a 2.39 presentation crop"),
		Shot->GetCinematicAspectRatio(), 2.39f);
	TestTrue(TEXT("Territory shots request Narrative HUD hiding"),
		Shot->GetPlaybackSettings().bHideHud);
	TestEqual(TEXT("Default shot intent is medium close-up"),
		Shot->GetShotRole(), ETerritoryDialogueShotRole::MediumCloseUp);
	TestTrue(TEXT("Cinematic lens override is enabled"),
		Shot->bApplyLensOverride);
	TestTrue(TEXT("Tracked focus smoothing is enabled"),
		Shot->bSmoothTrackingFocus);
	TestTrue(TEXT("Camera-local studio look is enabled by default"),
		Shot->bApplyCinematicStudioLook);
	TestEqual(TEXT("Default studio look is neutral"), Shot->StudioLook,
		ETerritoryCinematicStudioLook::Neutral);

	Shot->StudioLook = ETerritoryCinematicStudioLook::WarmDustyDay;
	const FTerritoryCinematicStudioSettings WarmSettings =
		Shot->GetResolvedStudioSettings();
	TestTrue(TEXT("Warm daylight protects highlights"),
		WarmSettings.ExposureCompensation < 0.f);
	TestTrue(TEXT("Warm daylight uses restrained saturation"),
		WarmSettings.Saturation > 0.9f && WarmSettings.Saturation <= 1.f);

	UCineCameraComponent* Camera = NewObject<UCineCameraComponent>();
	Shot->ApplyStudioLookToCamera(Camera);
	const FPostProcessSettings& CameraPP = Camera->PostProcessSettings;
	TestTrue(TEXT("Cinematic studio look uses camera post process"),
		Camera->PostProcessBlendWeight > 0.f);
	TestTrue(TEXT("Cinematic saturation override is active"),
		CameraPP.bOverride_ColorSaturation);
	TestTrue(TEXT("Cinematic saturation uses equal red and green channels"),
		FMath::IsNearlyEqual(CameraPP.ColorSaturation.X,
			CameraPP.ColorSaturation.Y));
	TestTrue(TEXT("Cinematic saturation uses equal red and blue channels"),
		FMath::IsNearlyEqual(CameraPP.ColorSaturation.X,
			CameraPP.ColorSaturation.Z));
	TestTrue(TEXT("Cinematic saturation uses equal alpha channel"),
		FMath::IsNearlyEqual(CameraPP.ColorSaturation.X,
			CameraPP.ColorSaturation.W));
	TestTrue(TEXT("Cinematic contrast uses equal RGB channels"),
		FMath::IsNearlyEqual(CameraPP.ColorContrast.X,
			CameraPP.ColorContrast.Y)
		&& FMath::IsNearlyEqual(CameraPP.ColorContrast.X,
			CameraPP.ColorContrast.Z));

	Shot->StudioLook = ETerritoryCinematicStudioLook::MoonlitBlueNight;
	const FTerritoryCinematicStudioSettings NightSettings =
		Shot->GetResolvedStudioSettings();
	TestTrue(TEXT("Moonlit night uses a cool camera white balance"),
		NightSettings.bOverrideWhiteBalance
		&& NightSettings.WhiteBalanceTemperature < 5000.f);
	TestTrue(TEXT("Moonlit night preserves deeper shadow contrast"),
		NightSettings.LocalExposureShadowContrast >
			WarmSettings.LocalExposureShadowContrast);

	TestTrue(TEXT("Presentation bridge is an automatic local-player subsystem"),
		UTerritoryCinematicPresentationSubsystem::StaticClass()->IsChildOf(
			ULocalPlayerSubsystem::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCinematicDialogueLifecycle,
	"TerritoryFramework.Presentation.Cinematics.NativeDialogueReplacementCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCinematicDialogueLifecycle::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> RestoreFrameCounter(GFrameCounter, GFrameCounter);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Dialogue lifecycle world"), World)) return false;
	// Native's base controller expects a project-authored stable GUID when
	// spawned. This transient fixture needs only its real Tales subobject.
	auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	auto* Tales = Controller ? Controller->GetTalesComponent() : nullptr;
	const auto* CurrentProperty = FindFProperty<FObjectProperty>(UTalesComponent::StaticClass(), TEXT("CurrentDialogue"));
	if (!Tales || !CurrentProperty) { World->DestroyWorld(false); return false; }
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	auto* Presentation = NewObject<UTerritoryCinematicPresentationSubsystem>(LocalPlayer);
	Presentation->BindToController(Controller);
	auto* Subject = World->SpawnActor<AActor>();
	auto* LOD = NewObject<ULODSyncComponent>(Subject);
	Subject->AddInstanceComponent(LOD);
	LOD->RegisterComponent();
	LOD->ForcedLOD = 3;
	auto* First = NewObject<UDialogue>(Controller);
	auto* Second = NewObject<UDialogue>(Controller);
	const auto Begin = [&](UDialogue* Dialogue)
	{
		// Seed an already-running dialogue without depending on project story assets.
		Dialogue->OwningComp = Tales;
		Dialogue->OwningController = Controller;
		CurrentProperty->SetObjectPropertyValue_InContainer(Tales, Dialogue);
		Tales->OnDialogueBegan.Broadcast(Dialogue);
	};
	const auto AdvanceTimers = [&]()
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(0.01f);
	};
	Begin(First);
	Presentation->RegisterCinematicSubject(Subject);
	TestTrue(TEXT("Native Began activates local presentation"), Presentation->IsNarrativeCinematicActive());
	TestEqual(TEXT("Speaker receives temporary cinematic detail"), LOD->ForcedLOD, 0);
	Tales->OnDialogueFinished.Broadcast(First, true, EExitDialogueReason::EDR_NewDialogueStarted);
	TestTrue(TEXT("A valid chain does not briefly release presentation"), Presentation->IsNarrativeCinematicActive());
	Begin(Second);
	TestEqual(TEXT("A replacement releases the old speaker's detail override"), LOD->ForcedLOD, 3);
	AdvanceTimers();
	TestTrue(TEXT("Successful Began cancels replacement reconciliation"), Presentation->IsNarrativeCinematicActive());
	Tales->OnDialogueFinished.Broadcast(First, false, EExitDialogueReason::EDR_NewDialogueStarted);
	TestTrue(TEXT("Late finish from an old dialogue cannot close the new one"), Presentation->IsNarrativeCinematicActive());
	Presentation->RegisterCinematicSubject(Subject);

	// Exercise Native's actual failure path: Finished(old, true), old teardown,
	// MakeDialogueInstance rejection, no Began event for the replacement.
	AddExpectedError(TEXT("MakeDialogue was passed UDialogue"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Native rejects a dialogue with no authored subclass"), Tales->BeginDialogue(UDialogue::StaticClass(), FDialoguePlayParams()));
	TestNull(TEXT("Native cleared the old dialogue after rejection"), Tales->GetCurrentDialogue());
	AdvanceTimers();
	TestFalse(TEXT("Failed replacement releases presentation on next tick"), Presentation->IsNarrativeCinematicActive());
	TestEqual(TEXT("Failed replacement restores original speaker detail"), LOD->ForcedLOD, 3);

	Begin(First);
	Presentation->RegisterCinematicSubject(Subject);
	Tales->OnDialogueFinished.Broadcast(First, true, EExitDialogueReason::EDR_NewDialogueStarted);
	Presentation->BindToController(nullptr);
	AdvanceTimers();
	TestFalse(TEXT("Controller removal cancels pending dialogue work"), Presentation->IsNarrativeCinematicActive());
	TestEqual(TEXT("Controller removal restores detail"), LOD->ForcedLOD, 3);
	TestFalse(TEXT("Controller removal unbinds the old Tales delegates"), Tales->OnDialogueBegan.IsAlreadyBound(Presentation, &UTerritoryCinematicPresentationSubsystem::HandleDialogueBegan));

	// Binding after a dialogue already began models local-controller recreation
	// and late client setup. Tales supplies the current dialogue, not saved UI state.
	Presentation->BindToController(Controller);
	TestTrue(TEXT("Late binding recovers Native current dialogue"), Presentation->IsNarrativeCinematicActive());
	Presentation->RegisterCinematicSubject(Subject);
	Subject->Destroy();
	CurrentProperty->SetObjectPropertyValue_InContainer(Tales, nullptr);
	Tales->OnDialogueFinished.Broadcast(First, false, EExitDialogueReason::EDR_NewDialogueStarted);
	TestFalse(TEXT("Ending a dialogue tolerates a removed speaker"), Presentation->IsNarrativeCinematicActive());
	Presentation->Deinitialize();
	World->DestroyWorld(false);
	return true;
}

#endif
