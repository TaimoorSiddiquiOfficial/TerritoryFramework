#include "Cinematics/TerritoryDialogueShotEditorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "DialogueBlueprint.h"
#include "Tales/DialogueBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneSpawnable.h"
#include "ObjectTools.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tales/Dialogue.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"

#define LOCTEXT_NAMESPACE "TerritoryDialogueShotEditorLibrary"

namespace TerritoryDialogueShotEditor
{
	constexpr int32 ShotDurationFrames = 720;
	const FName CinecamBindingTag(TEXT("Cinecam"));

	struct FStudioShotSpec
	{
		ETerritoryDialogueShotRole Role;
		const TCHAR* AssetName;
		const TCHAR* DisplayName;
		FVector CameraLocation;
		FRotator CameraRotation;
		float FocalLength;
		float Aperture;
		float ManualFocusDistance;
		const TCHAR* BestUse;
	};

	const TArray<FStudioShotSpec>& GetStudioShotSpecs()
	{
		static const TArray<FStudioShotSpec> Specs = {
			{ ETerritoryDialogueShotRole::Establishing,
				TEXT("LS_Territory_DLG_Establishing"), TEXT("Establishing"),
				FVector(-440.f, -125.f, 58.f), FRotator(-4.f, 0.f, 0.f), 35.f, 4.f,
				440.f, TEXT("Open the location, show geography, and establish both speakers before coverage.") },
			{ ETerritoryDialogueShotRole::TwoShot,
				TEXT("LS_Territory_DLG_TwoShot"), TEXT("Two Shot"),
				FVector(-315.f, -48.f, 28.f), FRotator(-2.f, 0.f, 0.f), 45.f, 3.2f,
				315.f, TEXT("Default dialogue master shot; keeps speaker geography readable and edits safe.") },
			{ ETerritoryDialogueShotRole::OverShoulder,
				TEXT("LS_Territory_DLG_OverShoulder"), TEXT("Over Shoulder"),
				FVector(-215.f, 52.f, 13.f), FRotator(-1.f, 0.f, 0.f), 55.f, 2.8f,
				215.f, TEXT("Dialogue coverage with foreground depth; alternate screen side without crossing the 180-degree line.") },
			{ ETerritoryDialogueShotRole::MediumCloseUp,
				TEXT("LS_Territory_DLG_MediumCloseUp"), TEXT("Medium Close-Up"),
				FVector(-155.f, 27.f, 5.f), FRotator(0.f, 0.f, 0.f), 65.f, 2.8f,
				155.f, TEXT("Primary speaking coverage; natural facial emphasis while preserving shoulders and gesture.") },
			{ ETerritoryDialogueShotRole::CloseUp,
				TEXT("LS_Territory_DLG_CloseUp"), TEXT("Close-Up"),
				FVector(-108.f, 13.f, 3.f), FRotator(0.f, 0.f, 0.f), 85.f, 2.4f,
				108.f, TEXT("Emotional story beat; reserve the shallow depth of field for important lines.") },
			{ ETerritoryDialogueShotRole::Reaction,
				TEXT("LS_Territory_DLG_Reaction"), TEXT("Reaction"),
				FVector(-132.f, -24.f, 4.f), FRotator(0.f, 0.f, 0.f), 75.f, 2.5f,
				132.f, TEXT("Silent listener response and player reply selection; creates editorial rhythm.") },
			{ ETerritoryDialogueShotRole::Insert,
				TEXT("LS_Territory_DLG_Insert"), TEXT("Insert"),
				FVector(-92.f, 0.f, -28.f), FRotator(8.f, 0.f, 0.f), 90.f, 3.2f,
				92.f, TEXT("Story prop, hand action, weapon, contract, or Territory handover detail.") }
		};
		return Specs;
	}

	void ConfigureCameraTemplate(ACineCameraActor* CameraTemplate,
		const FStudioShotSpec& Spec)
	{
		if (!CameraTemplate) return;
		CameraTemplate->SetActorTransform(FTransform(
			Spec.CameraRotation, Spec.CameraLocation));
		if (UCineCameraComponent* Camera =
			CameraTemplate->GetCineCameraComponent())
		{
			Camera->SetCurrentFocalLength(Spec.FocalLength);
			Camera->SetCurrentAperture(Spec.Aperture);
			Camera->SetConstraintAspectRatio(false);
			FCameraFocusSettings Focus = Camera->FocusSettings;
			Focus.FocusMethod = ECameraFocusMethod::Manual;
			Focus.ManualFocusDistance = Spec.ManualFocusDistance;
			Focus.bSmoothFocusChanges = true;
			Focus.FocusSmoothingInterpSpeed = 8.f;
			Camera->SetFocusSettings(Focus);
		}
	}

	bool EnsureCameraSpawnTrack(UMovieScene* MovieScene, const FGuid& CameraGuid,
		FText& OutError)
	{
		if (!MovieScene || !CameraGuid.IsValid()) return false;
		UMovieSceneSpawnTrack* SpawnTrack =
			MovieScene->FindTrack<UMovieSceneSpawnTrack>(CameraGuid);
		if (!SpawnTrack)
		{
			SpawnTrack = MovieScene->AddTrack<UMovieSceneSpawnTrack>(CameraGuid);
		}
		if (!SpawnTrack)
		{
			OutError = LOCTEXT("SpawnTrackCreateFailed",
				"Could not create the explicit CineCamera Spawn track.");
			return false;
		}
		SpawnTrack->Modify();
		SpawnTrack->SetObjectId(CameraGuid);
		SpawnTrack->RemoveAllAnimationData();
		UMovieSceneSpawnSection* SpawnSection =
			Cast<UMovieSceneSpawnSection>(SpawnTrack->CreateNewSection());
		if (!SpawnSection)
		{
			OutError = LOCTEXT("SpawnSectionCreateFailed",
				"Could not create the CineCamera Spawn section.");
			return false;
		}
		SpawnSection->SetRange(MovieScene->GetPlaybackRange());
		SpawnSection->GetChannel().SetDefault(true);
		SpawnTrack->AddSection(*SpawnSection);
		return true;
	}

	ULevelSequence* CreateStudioSequence(const FString& DestinationContentPath,
		const FStudioShotSpec& Spec, FText& OutError)
	{
		const FString PackageName = DestinationContentPath / Spec.AssetName;
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"),
			*PackageName, Spec.AssetName);
		if (UObject* ExistingAsset = StaticLoadObject(
			UObject::StaticClass(), nullptr, *ObjectPath))
		{
			ULevelSequence* ExistingSequence = Cast<ULevelSequence>(ExistingAsset);
			if (!ExistingSequence)
			{
				OutError = FText::Format(LOCTEXT("ShotAssetConflict",
					"'{0}' already exists but is not a Level Sequence."),
					FText::FromString(ObjectPath));
				return nullptr;
			}
			UMovieScene* ExistingMovieScene = ExistingSequence->GetMovieScene();
			if (!ExistingMovieScene)
			{
				OutError = FText::Format(LOCTEXT("ExistingMovieSceneMissing",
					"Existing Level Sequence '{0}' has no Movie Scene."),
					FText::FromString(ObjectPath));
				return nullptr;
			}
			for (int32 Index = 0;
				Index < ExistingMovieScene->GetSpawnableCount(); ++Index)
			{
				FMovieSceneSpawnable& Spawnable =
					ExistingMovieScene->GetSpawnable(Index);
				if (Spawnable.GetName() != TEXT("Cinecam")) continue;
				ACineCameraActor* CameraTemplate =
					Cast<ACineCameraActor>(Spawnable.GetObjectTemplate());
				if (!CameraTemplate)
				{
					ACineCameraActor* CameraSource = NewObject<ACineCameraActor>(
						GetTransientPackage(), ACineCameraActor::StaticClass(),
						NAME_None, RF_Transient);
					ConfigureCameraTemplate(CameraSource, Spec);
					Spawnable.CopyObjectTemplate(*CameraSource, *ExistingSequence);
					CameraTemplate = Cast<ACineCameraActor>(
						Spawnable.GetObjectTemplate());
					ExistingSequence->MarkPackageDirty();
				}
				ConfigureCameraTemplate(CameraTemplate, Spec);
				if (!EnsureCameraSpawnTrack(ExistingMovieScene,
					Spawnable.GetGuid(), OutError))
				{
					return nullptr;
				}
				ExistingSequence->MarkPackageDirty();
				return ExistingSequence;
			}
			OutError = FText::Format(LOCTEXT("ExistingCinecamMissing",
				"Existing Level Sequence '{0}' has no Cinecam spawnable."),
				FText::FromString(ObjectPath));
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);
		ULevelSequence* Sequence = NewObject<ULevelSequence>(Package,
			Spec.AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (!Sequence)
		{
			OutError = FText::Format(LOCTEXT("SequenceCreateFailed",
				"Could not create Level Sequence '{0}'."),
				FText::FromString(PackageName));
			return nullptr;
		}

		Sequence->Initialize();
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		if (!MovieScene)
		{
			OutError = FText::Format(LOCTEXT("MovieSceneMissing",
				"Level Sequence '{0}' did not initialize a Movie Scene."),
				FText::FromString(PackageName));
			return nullptr;
		}
		MovieScene->SetDisplayRate(FFrameRate(24, 1));
		MovieScene->SetTickResolutionDirectly(FFrameRate(24, 1));
		MovieScene->SetPlaybackRange(0, ShotDurationFrames);

		ACineCameraActor* CameraSource = NewObject<ACineCameraActor>(
			GetTransientPackage(), ACineCameraActor::StaticClass(),
			NAME_None, RF_Transient);
		const FGuid CameraGuid = CameraSource
			? MovieScene->AddSpawnable(TEXT("Cinecam"), *CameraSource)
			: FGuid();
		FMovieSceneSpawnable* CameraSpawnable =
			CameraGuid.IsValid() ? MovieScene->FindSpawnable(CameraGuid) : nullptr;
		if (!CameraSpawnable)
		{
			OutError = FText::Format(LOCTEXT("CameraBindingCreateFailed",
				"Could not create the CineCamera binding in '{0}'."),
				FText::FromString(PackageName));
			return nullptr;
		}
		CameraSpawnable->CopyObjectTemplate(*CameraSource, *Sequence);

		MovieScene->AddNewBindingTag(CinecamBindingTag);
		MovieScene->TagBinding(CinecamBindingTag,
			UE::MovieScene::FFixedObjectBindingID(
				CameraGuid, MovieSceneSequenceID::Root));

		ConfigureCameraTemplate(Cast<ACineCameraActor>(
			CameraSpawnable->GetObjectTemplate()), Spec);
		if (!EnsureCameraSpawnTrack(MovieScene, CameraGuid, OutError))
		{
			return nullptr;
		}

		UMovieScene3DTransformTrack* TransformTrack =
			MovieScene->AddTrack<UMovieScene3DTransformTrack>(CameraGuid);
		UMovieScene3DTransformSection* TransformSection = TransformTrack
			? Cast<UMovieScene3DTransformSection>(
				TransformTrack->CreateNewSection()) : nullptr;
		if (!TransformTrack || !TransformSection)
		{
			OutError = FText::Format(LOCTEXT("TransformTrackCreateFailed",
				"Could not create the static camera transform in '{0}'."),
				FText::FromString(PackageName));
			return nullptr;
		}
		TransformSection->SetRange(MovieScene->GetPlaybackRange());
		TransformTrack->AddSection(*TransformSection);
		TArrayView<FMovieSceneDoubleChannel*> Channels =
			TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		if (Channels.Num() < 9)
		{
			OutError = FText::Format(LOCTEXT("TransformChannelsMissing",
				"Camera transform in '{0}' exposes only {1} channels."),
				FText::FromString(PackageName),
				FText::AsNumber(Channels.Num()));
			return nullptr;
		}
		const double TransformDefaults[9] = {
			Spec.CameraLocation.X, Spec.CameraLocation.Y, Spec.CameraLocation.Z,
			Spec.CameraRotation.Roll, Spec.CameraRotation.Pitch, Spec.CameraRotation.Yaw,
			1.0, 1.0, 1.0
		};
		for (int32 Index = 0; Index < 9; ++Index)
		{
			Channels[Index]->SetDefault(TransformDefaults[Index]);
		}

		UMovieSceneCameraCutTrack* CameraCutTrack =
			Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(
				UMovieSceneCameraCutTrack::StaticClass()));
		UMovieSceneCameraCutSection* CameraCutSection = CameraCutTrack
			? Cast<UMovieSceneCameraCutSection>(
				CameraCutTrack->CreateNewSection()) : nullptr;
		if (!CameraCutTrack || !CameraCutSection)
		{
			OutError = FText::Format(LOCTEXT("CameraCutCreateFailed",
				"Could not create the camera cut in '{0}'."),
				FText::FromString(PackageName));
			return nullptr;
		}
		CameraCutSection->SetRange(MovieScene->GetPlaybackRange());
		CameraCutSection->SetCameraGuid(CameraGuid);
		CameraCutTrack->AddSection(*CameraCutSection);

		Sequence->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Sequence);
		return Sequence;
	}

	ULevelSequence* FindSequenceForRole(
		const FTerritoryDialogueShotPackBuildReport& ShotPack,
		const ETerritoryDialogueShotRole Role)
	{
		for (int32 Index = 0;
			Index < ShotPack.ShotRoles.Num()
				&& Index < ShotPack.SequenceAssets.Num(); ++Index)
		{
			if (ShotPack.ShotRoles[Index] == Role)
			{
				return ShotPack.SequenceAssets[Index];
			}
		}
		return nullptr;
	}

	UTerritoryDialogueShot* CreateInlineShot(UDialogue* Dialogue,
		UTerritoryDialogueShot* ExistingShot, const TCHAR* ObjectStem,
		ULevelSequence* Sequence,
		const FText& DisplayName, const ETerritoryDialogueShotRole Role,
		const EAnchorOriginRule AnchorRule,
		const EShotTrackingRule TrackingRule,
		const float FocalLength, const float Aperture,
		const bool bUse180DegreeRule = true)
	{
		if (!Dialogue || !Sequence) return nullptr;
		UTerritoryDialogueShot* Shot = ExistingShot;
		if (!Shot || Shot->GetOuter() != Dialogue)
		{
			const FName UniqueName = MakeUniqueObjectName(Dialogue,
				UTerritoryDialogueShot::StaticClass(), FName(ObjectStem));
			Shot = NewObject<UTerritoryDialogueShot>(
				Dialogue, UniqueName, RF_Transactional);
		}
		Shot->ConfigureEditorShot(Sequence, DisplayName, Role, AnchorRule,
			TrackingRule, FocalLength, Aperture, bUse180DegreeRule);
		return Shot;
	}
}

FTerritoryDialogueShotPackBuildReport
UTerritoryDialogueShotEditorLibrary::CreateStudioDialogueShotPack(
	const FString& DestinationContentPath)
{
	using namespace TerritoryDialogueShotEditor;
	FTerritoryDialogueShotPackBuildReport Report;
	Report.DestinationContentPath = DestinationContentPath;
	if (!FPackageName::IsValidLongPackageName(DestinationContentPath))
	{
		Report.Errors.Add(FText::Format(LOCTEXT("InvalidDestination",
			"'{0}' is not a valid Unreal content folder. Use a path such as /Game/Cinematics/Dialogue/Shots."),
			FText::FromString(DestinationContentPath)));
		return Report;
	}

	for (const FStudioShotSpec& Spec : GetStudioShotSpecs())
	{
		FText Error;
		if (ULevelSequence* Sequence = CreateStudioSequence(
			DestinationContentPath, Spec, Error))
		{
			Report.SequenceAssets.Add(Sequence);
			Report.ShotRoles.Add(Spec.Role);
			Report.ShotGuidance.Add(FText::Format(
				LOCTEXT("ShotGuidanceFormat", "{0}: {1}"),
				FText::FromString(Spec.DisplayName),
				FText::FromString(Spec.BestUse)));
		}
		else
		{
			Report.Errors.Add(Error);
			break;
		}
	}
	Report.bSucceeded = Report.Errors.IsEmpty()
		&& Report.SequenceAssets.Num() == GetStudioShotSpecs().Num();
	return Report;
}

FTerritoryDialogueShotPackBuildReport
UTerritoryDialogueShotEditorLibrary::ApplyStudioDialogueShotPack(
	UDialogueBlueprint* DialogueBlueprint,
	const FTerritoryDialogueShotPackBuildReport& ShotPack)
{
	using namespace TerritoryDialogueShotEditor;
	FTerritoryDialogueShotPackBuildReport Report = ShotPack;
	Report.bDialogueConfigured = false;
	if (!DialogueBlueprint || !DialogueBlueprint->DialogueTemplate)
	{
		Report.Errors.Add(LOCTEXT("DialogueRequired",
			"Select a compiled Narrative Dialogue Blueprint with a valid Dialogue Template."));
		Report.bSucceeded = false;
		return Report;
	}
	if (!ShotPack.bSucceeded)
	{
		Report.Errors.Add(LOCTEXT("SuccessfulPackRequired",
			"Create a complete Territory studio shot pack before applying it to a dialogue."));
		Report.bSucceeded = false;
		return Report;
	}

	ULevelSequence* TwoShot = FindSequenceForRole(ShotPack,
		ETerritoryDialogueShotRole::TwoShot);
	ULevelSequence* MediumCloseUp = FindSequenceForRole(ShotPack,
		ETerritoryDialogueShotRole::MediumCloseUp);
	ULevelSequence* Reaction = FindSequenceForRole(ShotPack,
		ETerritoryDialogueShotRole::Reaction);
	if (!TwoShot || !MediumCloseUp || !Reaction)
	{
		Report.Errors.Add(LOCTEXT("RequiredShotsMissing",
			"The shot pack must contain Two Shot, Medium Close-Up, and Reaction sequences."));
		Report.bSucceeded = false;
		return Report;
	}

	UDialogue* Dialogue = DialogueBlueprint->DialogueTemplate;
	DialogueBlueprint->Modify();
	Dialogue->Modify();

	Dialogue->DefaultDialogueShot = CreateInlineShot(Dialogue,
		Cast<UTerritoryDialogueShot>(Dialogue->DefaultDialogueShot),
		TEXT("Territory_Default_TwoShot"), TwoShot,
		LOCTEXT("DefaultTwoShot", "Territory Default Two Shot"),
		ETerritoryDialogueShotRole::TwoShot,
		EAnchorOriginRule::AOR_ConversationCenter,
		EShotTrackingRule::STR_Speaker, 45.f, 3.2f, false);

	for (int32 SpeakerIndex = 0; SpeakerIndex < Dialogue->Speakers.Num();
		++SpeakerIndex)
	{
		FSpeakerInfo& Speaker = Dialogue->Speakers[SpeakerIndex];
		Speaker.DefaultSpeakerShot = CreateInlineShot(Dialogue,
			Cast<UTerritoryDialogueShot>(Speaker.DefaultSpeakerShot),
			*FString::Printf(TEXT("Territory_NPC_%d_MCU"), SpeakerIndex),
			MediumCloseUp,
			LOCTEXT("NPCMediumCloseUp", "Territory NPC Medium Close-Up"),
			ETerritoryDialogueShotRole::MediumCloseUp,
			EAnchorOriginRule::AOR_Speaker,
			EShotTrackingRule::STR_Speaker, 65.f, 2.8f);
	}

	Dialogue->PlayerSpeakerInfo.DefaultSpeakerShot = CreateInlineShot(Dialogue,
		Cast<UTerritoryDialogueShot>(
			Dialogue->PlayerSpeakerInfo.DefaultSpeakerShot),
		TEXT("Territory_Player_MCU"), MediumCloseUp,
		LOCTEXT("PlayerMediumCloseUp", "Territory Player Medium Close-Up"),
		ETerritoryDialogueShotRole::MediumCloseUp,
		EAnchorOriginRule::AOR_Speaker,
		EShotTrackingRule::STR_Speaker, 65.f, 2.8f);
	Dialogue->PlayerSpeakerInfo.SelectingReplyShot = CreateInlineShot(Dialogue,
		Cast<UTerritoryDialogueShot>(
			Dialogue->PlayerSpeakerInfo.SelectingReplyShot),
		TEXT("Territory_Reply_Reaction"), Reaction,
		LOCTEXT("ReplyReaction", "Territory Reply-Selection Reaction"),
		ETerritoryDialogueShotRole::Reaction,
		EAnchorOriginRule::AOR_Listener,
		EShotTrackingRule::STR_Listener, 75.f, 2.5f);

	Dialogue->bShowCinematicBars = true;
	Dialogue->bAutoStopMovement = true;
	Dialogue->bAutoRotateSpeakers = true;
	Dialogue->bFreeMovement = false;
	Dialogue->DialogueBlendOutTime = 0.45f;

	FBlueprintEditorUtils::MarkBlueprintAsModified(DialogueBlueprint);
	FKismetEditorUtilities::CompileBlueprint(DialogueBlueprint);
	DialogueBlueprint->MarkPackageDirty();
	const UDialogueBlueprintGeneratedClass* GeneratedClass =
		Cast<UDialogueBlueprintGeneratedClass>(DialogueBlueprint->GeneratedClass);
	const UDialogue* RuntimeTemplate = GeneratedClass
		? GeneratedClass->GetDialogueTemplate() : nullptr;
	Report.bDialogueConfigured = DialogueBlueprint->Status != BS_Error
		&& RuntimeTemplate
		&& RuntimeTemplate->DefaultDialogueShot
		&& RuntimeTemplate->PlayerSpeakerInfo.DefaultSpeakerShot
		&& RuntimeTemplate->PlayerSpeakerInfo.SelectingReplyShot;
	if (Dialogue->Speakers.IsEmpty())
	{
		Report.Warnings.Add(LOCTEXT("NoExplicitNPCSpeakers",
			"This Dialogue has no explicit NPC speaker entries. NPC lines use the configured Territory default two-shot fallback; line-level shots still override it."));
	}
	Report.bSucceeded = Report.bDialogueConfigured && Report.Errors.IsEmpty();
	if (!Report.bDialogueConfigured)
	{
		Report.Errors.Add(LOCTEXT("DialogueCompileFailed",
			"The Narrative Dialogue did not compile after applying the shot pack."));
	}
	return Report;
}

FTerritoryDialogueShotPackBuildReport
UTerritoryDialogueShotEditorLibrary::CreateAndApplyStudioDialogueShotPack(
	UDialogueBlueprint* DialogueBlueprint,
	const FString& DestinationContentPath)
{
	return ApplyStudioDialogueShotPack(DialogueBlueprint,
		CreateStudioDialogueShotPack(DestinationContentPath));
}

#undef LOCTEXT_NAMESPACE
