#include "Cinematics/TerritoryDialogueShot.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Core/TerritoryTypes.h"
#include "GameFramework/PlayerController.h"
#include "Tales/Dialogue.h"

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
	FCameraFocusSettings FocusSettings = CameraComponent->FocusSettings;
	FocusSettings.bSmoothFocusChanges = bSmoothTrackingFocus;
	FocusSettings.FocusSmoothingInterpSpeed = FocusSmoothingSpeed;
	CameraComponent->SetFocusSettings(FocusSettings);
}
