#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataTable.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "UObject/UnrealType.h"

namespace TerritoryNarrativeQTEAssetTests
{
	const FProperty* FindAuthoredProperty(const UStruct* Struct, const FString& AuthoredName)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetAuthoredName() == AuthoredName)
			{
				return *It;
			}
		}
		return nullptr;
	}

	FFrameNumber DisplayFrameToTick(const UMovieScene* MovieScene, const int32 DisplayFrame)
	{
		return FFrameRate::TransformTime(FFrameTime(DisplayFrame),
			MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryHopTrendyFightQTEAssetContract,
	"TerritoryFramework.Editor.Cinematics.HopTrendyFightQTEAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryHopTrendyFightQTEAssetContract::RunTest(const FString& Parameters)
{
	using namespace TerritoryNarrativeQTEAssetTests;

	ULevelSequence* Main = LoadObject<ULevelSequence>(nullptr,
		TEXT("/Game/HOPTRENDY/Cinematics/LS_FightCutscene.LS_FightCutscene"));
	ULevelSequence* Dodge = LoadObject<ULevelSequence>(nullptr,
		TEXT("/Game/HOPTRENDY/Cinematics/LS_FightDoge.LS_FightDoge"));
	if (!Main || !Dodge)
	{
		AddWarning(TEXT("HOPTRENDY fight-QTE project fixtures are not installed; skipping project contract."));
		return true;
	}

	UMovieScene* MovieScene = Main->GetMovieScene();
	TestNotNull(TEXT("Main fight sequence has a Movie Scene"), MovieScene);
	if (!MovieScene)
	{
		return false;
	}

	const TArray<FMovieSceneObjectBindingID> ControllerReferences =
		Main->FindBindingsByTag(TEXT("PlayerController"));
	TestEqual(TEXT("Main has one explicit Narrative PlayerController binding"),
		ControllerReferences.Num(), 1);
	if (ControllerReferences.Num() != 1)
	{
		return false;
	}

	const FMovieSceneBinding* ControllerBinding =
		MovieScene->FindBinding(ControllerReferences[0].GetGuid());
	TestNotNull(TEXT("Tagged PlayerController binding resolves"), ControllerBinding);
	if (!ControllerBinding)
	{
		return false;
	}

	UMovieSceneTrack* QTETrack = nullptr;
	for (UMovieSceneTrack* Track : ControllerBinding->GetTracks())
	{
		if (IsValid(Track) && Track->GetClass()->GetName() == TEXT("BP_QTETrack_C"))
		{
			QTETrack = Track;
			break;
		}
	}
	TestNotNull(TEXT("PlayerController binding uses Narrative Pro's QTE track"), QTETrack);
	if (!QTETrack)
	{
		return false;
	}

	const FArrayProperty* SupportedSections = FindFProperty<FArrayProperty>(
		QTETrack->GetClass(), TEXT("SupportedSections"));
	TestNotNull(TEXT("Narrative QTE track exposes SupportedSections"), SupportedSections);
	if (SupportedSections)
	{
		FScriptArrayHelper Values(SupportedSections,
			SupportedSections->ContainerPtrToValuePtr<void>(QTETrack));
		TestEqual(TEXT("QTE track has no stale Input/UI SupportedSections override"),
			Values.Num(), 0);
	}

	const TArray<UMovieSceneSection*>& Sections = QTETrack->GetAllSections();
	TestEqual(TEXT("QTE track has exactly one authored prompt window"), Sections.Num(), 1);
	if (Sections.Num() != 1)
	{
		return false;
	}

	UMovieSceneSection* Section = Sections[0];
	TestEqual(TEXT("QTE starts at display frame 71"),
		Section->GetInclusiveStartFrame(), DisplayFrameToTick(MovieScene, 71));
	TestEqual(TEXT("QTE ends at display frame 111"),
		Section->GetExclusiveEndFrame(), DisplayFrameToTick(MovieScene, 111));

	const FArrayProperty* Options = FindFProperty<FArrayProperty>(
		Section->GetClass(), TEXT("QuicktimeOptions"));
	TestNotNull(TEXT("QTE section exposes QuicktimeOptions"), Options);
	if (!Options)
	{
		return false;
	}

	FScriptArrayHelper OptionValues(Options, Options->ContainerPtrToValuePtr<void>(Section));
	TestEqual(TEXT("QTE has exactly one Dodge option"), OptionValues.Num(), 1);
	const FStructProperty* OptionStruct = CastField<FStructProperty>(Options->Inner);
	if (!TestNotNull(TEXT("QTE option uses Narrative's authored struct"), OptionStruct)
		|| OptionValues.Num() != 1)
	{
		return false;
	}

	void* OptionData = OptionValues.GetRawPtr(0);
	const FStructProperty* TriggeringInput = CastField<FStructProperty>(
		FindAuthoredProperty(OptionStruct->Struct, TEXT("TriggeringInput")));
	TestNotNull(TEXT("QTE option exposes TriggeringInput"), TriggeringInput);
	if (TriggeringInput && TriggeringInput->Struct == TBaseStructure<FDataTableRowHandle>::Get())
	{
		const FDataTableRowHandle* Handle = TriggeringInput->ContainerPtrToValuePtr<FDataTableRowHandle>(
			OptionData);
		TestNotNull(TEXT("QTE Interact input table is assigned"), Handle->DataTable.Get());
		TestEqual(TEXT("QTE listens for Narrative CommonUI Interact"),
			Handle->RowName, FName(TEXT("Interact")));
	}

	const FObjectPropertyBase* DestinationProperty = CastField<FObjectPropertyBase>(
		FindAuthoredProperty(OptionStruct->Struct, TEXT("OptionDestination")));
	TestNotNull(TEXT("QTE option exposes OptionDestination"), DestinationProperty);
	if (DestinationProperty)
	{
		const UObject* Destination = DestinationProperty->GetObjectPropertyValue_InContainer(OptionData);
		TestTrue(TEXT("Successful QTE branches to the local Dodge sequence"),
			Destination == Dodge);
	}

	return !HasAnyErrors();
}

#endif
