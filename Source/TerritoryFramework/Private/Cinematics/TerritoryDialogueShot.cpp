#include "Cinematics/TerritoryDialogueShot.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Core/TerritoryTypes.h"
#include "GameFramework/PlayerController.h"
#include "Tales/Dialogue.h"

namespace TerritoryCinematicStudio
{
	FTerritoryCinematicStudioSettings MakeWarmDustyDay()
	{
		FTerritoryCinematicStudioSettings Settings;
		Settings.bOverrideWhiteBalance = true;
		Settings.WhiteBalanceTemperature = 6000.f;
		Settings.ExposureCompensation = -0.3f;
		Settings.Saturation = 0.97f;
		Settings.Contrast = 1.03f;
		Settings.LocalExposureHighlightContrast = 0.75f;
		Settings.LocalExposureShadowContrast = 0.8f;
		Settings.BloomIntensity = 0.15f;
		Settings.VignetteIntensity = 0.1f;
		Settings.FilmGrainIntensity = 0.015f;
		return Settings;
	}

	FTerritoryCinematicStudioSettings MakeMoonlitBlueNight()
	{
		FTerritoryCinematicStudioSettings Settings;
		Settings.bOverrideWhiteBalance = true;
		Settings.WhiteBalanceTemperature = 4300.f;
		Settings.ExposureCompensation = 0.15f;
		Settings.Saturation = 0.93f;
		Settings.Contrast = 1.04f;
		Settings.LocalExposureHighlightContrast = 0.8f;
		Settings.LocalExposureShadowContrast = 0.9f;
		Settings.BloomIntensity = 0.22f;
		Settings.VignetteIntensity = 0.18f;
		Settings.FilmGrainIntensity = 0.025f;
		return Settings;
	}
}

UTerritoryDialogueShot::UTerritoryDialogueShot()
{
	FriendlyShotName = NSLOCTEXT("TerritoryDialogueShot",
		"DefaultFriendlyName", "Territory Cinematic Shot");
	CropSettings.AspectRatio = 2.39f;
	PlaybackSettings.bHideHud = true;
	bShouldRestart = false;
	AnchorOriginRule = EAnchorOriginRule::AOR_Speaker;
	AnchorRotationRule = EAnchorRotationRule::ARR_Conversation;
	bUse180DegreeRule = true;
	UnitsY180DegreeRule = 12.f;
	DegreesYaw180DegreeRule = 12.f;

	LookAtTrackingSettings.AvatarToTrack = EShotTrackingRule::STR_Speaker;
	LookAtTrackingSettings.bUpdateTrackingEveryFrame = true;
	LookAtTrackingSettings.UpdateTrackingInterpSpeed = 8.f;
	FocusTrackingSettings.AvatarToTrack = EShotTrackingRule::STR_Speaker;
	FocusTrackingSettings.bUpdateTrackingEveryFrame = true;
	FocusTrackingSettings.UpdateTrackingInterpSpeed = 8.f;
}

FTerritoryCinematicStudioSettings
UTerritoryDialogueShot::GetResolvedStudioSettings() const
{
	switch (StudioLook)
	{
	case ETerritoryCinematicStudioLook::WarmDustyDay:
		return TerritoryCinematicStudio::MakeWarmDustyDay();
	case ETerritoryCinematicStudioLook::MoonlitBlueNight:
		return TerritoryCinematicStudio::MakeMoonlitBlueNight();
	case ETerritoryCinematicStudioLook::Custom:
		return CustomStudioSettings;
	case ETerritoryCinematicStudioLook::Neutral:
	default:
		return FTerritoryCinematicStudioSettings();
	}
}

void UTerritoryDialogueShot::ApplyStudioLookToCamera(
	UCineCameraComponent* CameraComponent) const
{
	if (!CameraComponent || !bApplyCinematicStudioLook) return;

	const FTerritoryCinematicStudioSettings Settings =
		GetResolvedStudioSettings();
	FPostProcessSettings& PP = CameraComponent->PostProcessSettings;
	CameraComponent->SetPostProcessBlendWeight(
		FMath::Clamp(StudioLookBlendWeight, 0.f, 1.f));

	PP.bOverride_TemperatureType = Settings.bOverrideWhiteBalance;
	PP.TemperatureType = TEMP_WhiteBalance;
	PP.bOverride_WhiteTemp = Settings.bOverrideWhiteBalance;
	PP.WhiteTemp = FMath::Clamp(
		Settings.WhiteBalanceTemperature, 1500.f, 15000.f);
	PP.bOverride_WhiteTint = Settings.bOverrideWhiteBalance;
	PP.WhiteTint = 0.f;

	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = FMath::Clamp(
		Settings.ExposureCompensation, -2.f, 2.f);
	const float Saturation = FMath::Clamp(Settings.Saturation, 0.75f, 1.25f);
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(
		Saturation, Saturation, Saturation, Saturation);
	const float Contrast = FMath::Clamp(Settings.Contrast, 0.75f, 1.25f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(Contrast, Contrast, Contrast, Contrast);

	PP.bOverride_LocalExposureMethod = true;
	PP.LocalExposureMethod = ELocalExposureMethod::Bilateral;
	PP.bOverride_LocalExposureHighlightContrastScale = true;
	PP.LocalExposureHighlightContrastScale = FMath::Clamp(
		Settings.LocalExposureHighlightContrast, 0.6f, 1.f);
	PP.bOverride_LocalExposureShadowContrastScale = true;
	PP.LocalExposureShadowContrastScale = FMath::Clamp(
		Settings.LocalExposureShadowContrast, 0.6f, 1.f);
	PP.bOverride_LocalExposureDetailStrength = true;
	PP.LocalExposureDetailStrength = FMath::Clamp(
		Settings.LocalExposureDetailStrength, 0.f, 4.f);
	PP.bOverride_LocalExposureBlurredLuminanceBlend = true;
	PP.LocalExposureBlurredLuminanceBlend = FMath::Clamp(
		Settings.LocalExposureBlurredLuminanceBlend, 0.f, 1.f);

	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = FMath::Clamp(Settings.BloomIntensity, 0.f, 1.f);
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = FMath::Clamp(Settings.VignetteIntensity, 0.f, 1.f);
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = FMath::Clamp(
		Settings.FilmGrainIntensity, 0.f, 0.1f);
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = FMath::Clamp(Settings.MotionBlurAmount, 0.f, 1.f);
}

#if WITH_EDITOR
void UTerritoryDialogueShot::ConfigureEditorShot(
	ULevelSequence* Sequence, const FText& DisplayName,
	const ETerritoryDialogueShotRole InRole,
	const EAnchorOriginRule InAnchorOrigin,
	const EShotTrackingRule InTrackingRule,
	const float InFocalLength, const float InAperture,
	const bool bInUse180DegreeRule)
{
	Modify();
	SequenceAssets.Reset();
	if (Sequence)
	{
		SequenceAssets.Add(Sequence);
	}
	FriendlyShotName = DisplayName;
	ShotRole = InRole;
	AnchorOriginRule = InAnchorOrigin;
	AnchorRotationRule = EAnchorRotationRule::ARR_Conversation;
	bUse180DegreeRule = bInUse180DegreeRule;
	LookAtTrackingSettings.AvatarToTrack = InTrackingRule;
	FocusTrackingSettings.AvatarToTrack = InTrackingRule;
	FocalLength = InFocalLength;
	Aperture = InAperture;
	PlaybackSettings.bPauseAtEnd = true;
}
#endif

void UTerritoryDialogueShot::BeginPlaySequence(
	ALevelSequenceActor* InSequenceActor, UDialogue* InDialogue,
	AActor* InSpeaker, AActor* InListener)
{
	if (SequenceAssets.IsEmpty())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Territory dialogue shot '%s' has no Level Sequence; Narrative cannot create a camera cut."),
			*GetNameSafe(this));
	}

	Super::BeginPlaySequence(InSequenceActor, InDialogue, InSpeaker, InListener);

	if (InDialogue && InDialogue->OwningController)
	{
		if (UTerritoryCinematicPresentationSubsystem* Presentation =
			UTerritoryCinematicPresentationSubsystem::GetForPlayerController(
				InDialogue->OwningController))
		{
			Presentation->RegisterCinematicSubject(InSpeaker);
			Presentation->RegisterCinematicSubject(InListener);
		}
	}

	UCineCameraComponent* CameraComponent = Cinecam.IsValid()
		? Cinecam->GetCineCameraComponent() : nullptr;
	if (!CameraComponent) return;

	if (bApplyLensOverride)
	{
		CameraComponent->SetCurrentFocalLength(FocalLength);
		CameraComponent->SetCurrentAperture(Aperture);
	}
	ApplyStudioLookToCamera(CameraComponent);
	FCameraFocusSettings FocusSettings = CameraComponent->FocusSettings;
	FocusSettings.bSmoothFocusChanges = bSmoothTrackingFocus;
	FocusSettings.FocusSmoothingInterpSpeed = FocusSmoothingSpeed;
	CameraComponent->SetFocusSettings(FocusSettings);
}
