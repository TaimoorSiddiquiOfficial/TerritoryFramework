#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Cinematics/TerritoryDialogueShot.h"
#include "CineCameraComponent.h"
#include "Core/TerritoryDeveloperSettings.h"

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

#endif
