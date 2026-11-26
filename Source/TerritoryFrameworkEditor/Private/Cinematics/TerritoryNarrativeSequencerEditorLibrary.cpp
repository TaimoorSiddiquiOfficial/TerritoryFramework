#include "Cinematics/TerritoryNarrativeSequencerEditorLibrary.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "TerritoryNarrativeSequencerEditorLibrary"

namespace TerritoryNarrativeSequencerEditor
{
	const TCHAR* QTETrackClassPath =
		TEXT("/NarrativePro/Pro/Core/BP/Sequencer/Tracks/QuicktimeEvent/BP_QTETrack.BP_QTETrack_C");
	const TCHAR* QTESectionClassFragment = TEXT("/QuicktimeEvent/BP_QTESection.BP_QTESection_C");
	const TCHAR* QTETrackInstanceClassFragment =
		TEXT("/QuicktimeEvent/BP_QTETrackInst.BP_QTETrackInst_C");

	FString ExportPropertyValue(UObject* Object, const FName PropertyName)
	{
		if (!Object)
		{
			return FString();
		}

		const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), PropertyName);
		if (!Property)
		{
			return FString();
		}

		FString Value;
		Property->ExportText_InContainer(0, Value, Object, Object, Object,
			PPF_None, nullptr);
		return Value;
	}
}

FTerritoryNarrativeQTETrackRepairReport
UTerritoryNarrativeSequencerEditorLibrary::NormalizeNarrativeQTETrackConfiguration(
	ULevelSequence* Sequence)
{
	using namespace TerritoryNarrativeSequencerEditor;

	FTerritoryNarrativeQTETrackRepairReport Report;
	if (!IsValid(Sequence) || !IsValid(Sequence->GetMovieScene()))
	{
		Report.Errors.Add(LOCTEXT("MissingSequence", "A saved Level Sequence is required."));
		return Report;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	const UMovieScene* ConstMovieScene = MovieScene;
	for (const FMovieSceneBinding& Binding : ConstMovieScene->GetBindings())
	{
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (!IsValid(Track) || Track->GetClass()->GetPathName() != QTETrackClassPath)
			{
				continue;
			}

			++Report.QTETracksFound;
			const FString DefaultSectionType =
				ExportPropertyValue(Track, TEXT("DefaultSectionType"));
			const FString TrackInstanceType =
				ExportPropertyValue(Track, TEXT("TrackInstanceType"));
			if (!DefaultSectionType.Contains(QTESectionClassFragment)
				|| !TrackInstanceType.Contains(QTETrackInstanceClassFragment))
			{
				Report.Errors.Add(FText::Format(
					LOCTEXT("UnexpectedQTEClassContract",
						"QTE track {0} does not use Narrative Pro's QTE section and track-instance classes."),
					FText::FromString(Track->GetPathName())));
				continue;
			}

			FArrayProperty* SupportedSections = FindFProperty<FArrayProperty>(
				Track->GetClass(), TEXT("SupportedSections"));
			if (!SupportedSections)
			{
				Report.Errors.Add(FText::Format(
					LOCTEXT("MissingSupportedSections",
						"QTE track {0} has no SupportedSections property."),
					FText::FromString(Track->GetPathName())));
				continue;
			}

			FScriptArrayHelper SupportedSectionValues(SupportedSections,
				SupportedSections->ContainerPtrToValuePtr<void>(Track));
			if (SupportedSectionValues.Num() > 0)
			{
				Track->Modify();
				SupportedSectionValues.EmptyValues();
				Track->PostEditChange();
				++Report.InvalidSupportedSectionOverridesRemoved;
			}
		}
	}

	if (Report.QTETracksFound == 0)
	{
		Report.Errors.Add(LOCTEXT("NoQTETrack",
			"The sequence has no Narrative Pro BP_QTETrack."));
	}

	Report.bSucceeded = Report.QTETracksFound > 0 && Report.Errors.IsEmpty();
	if (Report.bSucceeded && Report.InvalidSupportedSectionOverridesRemoved > 0)
	{
		Sequence->Modify();
		MovieScene->Modify();
		Sequence->MarkPackageDirty();
		Sequence->PostEditChange();
	}

	return Report;
}

#undef LOCTEXT_NAMESPACE
