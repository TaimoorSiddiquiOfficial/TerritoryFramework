#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cinematics/TerritoryDialogueShotEditorLibrary.h"
#include "Cinematics/TerritoryDialogueShotPackSpec.h"
#include "Cinematics/TerritoryShotRoleTags.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Engine/DataAsset.h"
#include "Environment/TerritoryHDRSceneEditorLibrary.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryAAADialogueShotAssetContract,
	"TerritoryFramework.Editor.Cinematics.AAADialogueShotAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryAAADialogueShotAssetContract::RunTest(const FString& Parameters)
{
	static const TCHAR* ShotNames[] = {
		TEXT("LS_Territory_DLG_Establishing"),
		TEXT("LS_Territory_DLG_TwoShot"),
		TEXT("LS_Territory_DLG_OverShoulder"),
		TEXT("LS_Territory_DLG_MediumCloseUp"),
		TEXT("LS_Territory_DLG_CloseUp"),
		TEXT("LS_Territory_DLG_Reaction"),
		TEXT("LS_Territory_DLG_Insert")
	};
	const FString Folder = TEXT("/TerritoryFramework/Cinematics/Dialogue/Shots");
	if (!FPackageName::DoesPackageExist(Folder / ShotNames[0]))
	{
		AddInfo(TEXT("Skipped optional project shot-pack fixture; install or generate the Territory AAA dialogue shot assets first."));
		return true;
	}

	for (const TCHAR* ShotName : ShotNames)
	{
		const FString PackageName = Folder / ShotName;
		ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr,
			*FString::Printf(TEXT("%s.%s"), *PackageName, ShotName));
		TestNotNull(*FString::Printf(TEXT("%s loads"), ShotName), Sequence);
		if (!Sequence) continue;

		UMovieScene* MovieScene = Sequence->GetMovieScene();
		TestNotNull(*FString::Printf(TEXT("%s has a Movie Scene"), ShotName),
			MovieScene);
		if (!MovieScene) continue;

		const TArray<FMovieSceneObjectBindingID> Cinecams =
			Sequence->FindBindingsByTag(TEXT("Cinecam"));
		TestEqual(*FString::Printf(TEXT("%s has one Cinecam binding"), ShotName),
			Cinecams.Num(), 1);
		if (Cinecams.Num() != 1) continue;

		const FGuid CameraGuid = Cinecams[0].GetGuid();
		TestNotNull(*FString::Printf(TEXT("%s Cinecam is a spawnable"), ShotName),
			MovieScene->FindSpawnable(CameraGuid));
		const UMovieSceneSpawnTrack* SpawnTrack =
			MovieScene->FindTrack<UMovieSceneSpawnTrack>(CameraGuid);
		TestNotNull(*FString::Printf(TEXT("%s has an explicit Spawn track"), ShotName),
			SpawnTrack);
		if (SpawnTrack)
		{
			TestEqual(*FString::Printf(TEXT("%s Spawn track has one section"), ShotName),
				SpawnTrack->GetAllSections().Num(), 1);
		}
		const UMovieSceneCameraCutTrack* CameraCutTrack =
			Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
		TestNotNull(*FString::Printf(TEXT("%s has a Camera Cut track"), ShotName),
			CameraCutTrack);
		if (CameraCutTrack)
		{
			TestEqual(*FString::Printf(TEXT("%s Camera Cut has one section"), ShotName),
				CameraCutTrack->GetAllSections().Num(), 1);
		}
	}
	return true;
}

namespace TerritoryFightQTEAssetContract
{
	static const FMovieSceneBinding* FindBindingByName(
		const UMovieScene* MovieScene, const FString& Name)
	{
		if (!MovieScene)
		{
			return nullptr;
		}

		return MovieScene->GetBindings().FindByPredicate(
			[MovieScene, &Name](const FMovieSceneBinding& Binding)
			{
				return MovieScene->GetObjectDisplayName(Binding.GetObjectGuid()).ToString()
					== Name;
			});
	}

	static UMovieSceneTrack* FindTrackByClassName(
		const FMovieSceneBinding* Binding, const FName ClassName)
	{
		if (!Binding)
		{
			return nullptr;
		}

		for (UMovieSceneTrack* Track : Binding->GetTracks())
		{
			if (Track && Track->GetClass()->GetFName() == ClassName)
			{
				return Track;
			}
		}
		return nullptr;
	}

	static bool HasTrackClass(const UMovieScene* MovieScene, const FName ClassName)
	{
		if (!MovieScene)
		{
			return false;
		}

		for (UMovieSceneTrack* Track : MovieScene->GetTracks())
		{
			if (Track && Track->GetClass()->GetFName() == ClassName)
			{
				return true;
			}
		}
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			if (FindTrackByClassName(&Binding, ClassName))
			{
				return true;
			}
		}
		return false;
	}

	static const FProperty* FindPropertyByAuthoredName(
		const UStruct* Owner, const FName AuthoredName)
	{
		if (!Owner)
		{
			return nullptr;
		}

		const FString Prefix = AuthoredName.ToString() + TEXT("_");
		for (TFieldIterator<FProperty> It(Owner); It; ++It)
		{
			if (It->GetFName() == AuthoredName || It->GetName().StartsWith(Prefix))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static FString ExportArrayStructPropertyValue(
		UObject* Object, const FName ArrayName, const FName MemberName)
	{
		if (!Object)
		{
			return FString();
		}

		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(
			FindPropertyByAuthoredName(Object->GetClass(), ArrayName));
		const FStructProperty* StructProperty = ArrayProperty
			? CastField<FStructProperty>(ArrayProperty->Inner)
			: nullptr;
		if (!ArrayProperty || !StructProperty)
		{
			return FString();
		}

		FScriptArrayHelper ArrayHelper(ArrayProperty,
			ArrayProperty->ContainerPtrToValuePtr<void>(Object));
		if (ArrayHelper.Num() != 1)
		{
			return FString();
		}

		const FProperty* MemberProperty =
			FindPropertyByAuthoredName(StructProperty->Struct, MemberName);
		if (!MemberProperty)
		{
			return FString();
		}

		const void* Entry = ArrayHelper.GetRawPtr(0);
		FString Result;
		MemberProperty->ExportTextItem_Direct(Result,
			MemberProperty->ContainerPtrToValuePtr<void>(Entry), nullptr, Object, PPF_None);
		return Result;
	}

	static int32 GetArrayNum(const UObject* Object, const FName ArrayName)
	{
		const FArrayProperty* ArrayProperty = Object
			? CastField<FArrayProperty>(FindPropertyByAuthoredName(
				Object->GetClass(), ArrayName))
			: nullptr;
		if (!ArrayProperty)
		{
			return INDEX_NONE;
		}

		const FScriptArrayHelper ArrayHelper(ArrayProperty,
			ArrayProperty->ContainerPtrToValuePtr<void>(Object));
		return ArrayHelper.Num();
	}

	static FFrameNumber DisplayFrameToTick(
		const UMovieScene* MovieScene, const int32 DisplayFrame)
	{
		return MovieScene
			? FFrameRate::TransformTime(FFrameTime(DisplayFrame),
				MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).FloorToFrame()
			: FFrameNumber();
	}

	static bool SectionHasRange(const UMovieScene* MovieScene,
		const UMovieSceneSection* Section, const int32 StartFrame, const int32 EndFrame)
	{
		if (!MovieScene || !Section || !Section->HasStartFrame() || !Section->HasEndFrame())
		{
			return false;
		}

		return Section->GetInclusiveStartFrame() == DisplayFrameToTick(MovieScene, StartFrame)
			&& Section->GetExclusiveEndFrame() == DisplayFrameToTick(MovieScene, EndFrame);
	}

	static bool TransformMatchesAtDisplayFrame(const UMovieScene* MovieScene,
		const FMovieSceneBinding* Binding, const int32 DisplayFrame,
		const TArray<double>& Expected)
	{
		UMovieSceneTrack* Track = FindTrackByClassName(
			Binding, TEXT("MovieScene3DTransformTrack"));
		if (!MovieScene || !Track || Track->GetAllSections().Num() != 1 || Expected.Num() != 9)
		{
			return false;
		}

		UMovieScene3DTransformSection* Section =
			Cast<UMovieScene3DTransformSection>(Track->GetAllSections()[0]);
		if (!Section)
		{
			return false;
		}

		const TArrayView<FMovieSceneDoubleChannel*> Channels =
			Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		if (Channels.Num() < Expected.Num())
		{
			return false;
		}

		const FFrameTime Time(DisplayFrameToTick(MovieScene, DisplayFrame));
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			double Value = 0.0;
			if (!Channels[Index] || !Channels[Index]->Evaluate(Time, Value)
				|| !FMath::IsNearlyEqual(Value, Expected[Index], 0.05))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryFightQTEAssetContract,
	"TerritoryFramework.Editor.Cinematics.FightQTEAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryFightQTEAssetContract::RunTest(const FString& Parameters)
{
	using namespace TerritoryFightQTEAssetContract;

	static const TCHAR* MainPath =
		TEXT("/Game/TerritoryFramework/Cinematics/QTE/LS_Territory_Fight_QTE.LS_Territory_Fight_QTE");
	static const TCHAR* DodgePath =
		TEXT("/Game/TerritoryFramework/Cinematics/QTE/LS_Territory_Fight_QTE_Dodge.LS_Territory_Fight_QTE_Dodge");
	ULevelSequence* Main = LoadObject<ULevelSequence>(nullptr, MainPath);
	ULevelSequence* Dodge = LoadObject<ULevelSequence>(nullptr, DodgePath);
	TestNotNull(TEXT("Fight QTE main sequence loads"), Main);
	TestNotNull(TEXT("Fight QTE local Dodge branch loads"), Dodge);
	if (!Main || !Dodge)
	{
		return false;
	}

	for (const TPair<const TCHAR*, ULevelSequence*> Fixture : {
		TPair<const TCHAR*, ULevelSequence*>(TEXT("Main"), Main),
		TPair<const TCHAR*, ULevelSequence*>(TEXT("Dodge"), Dodge) })
	{
		ULevelSequence* Sequence = Fixture.Value;
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		TestNotNull(*FString::Printf(TEXT("%s has a Movie Scene"), Fixture.Key), MovieScene);
		if (!MovieScene)
		{
			continue;
		}

		TestEqual(*FString::Printf(TEXT("%s has one Narrative Player tag"), Fixture.Key),
			Sequence->FindBindingsByTag(TEXT("Player")).Num(), 1);
		TestEqual(*FString::Printf(TEXT("%s has one Narrative PlayerController tag"), Fixture.Key),
			Sequence->FindBindingsByTag(TEXT("PlayerController")).Num(), 1);
		TestEqual(*FString::Printf(TEXT("%s has no stale Craty tag"), Fixture.Key),
			Sequence->FindBindingsByTag(TEXT("Craty")).Num(), 0);
		TestNotNull(*FString::Printf(TEXT("%s has a localized ActionCamera"), Fixture.Key),
			FindBindingByName(MovieScene, TEXT("ActionCamera")));
		TestNotNull(*FString::Printf(TEXT("%s has a localized WideCamera"), Fixture.Key),
			FindBindingByName(MovieScene, TEXT("WideCamera")));
		TestNull(*FString::Printf(TEXT("%s removed the vendor crate binding"), Fixture.Key),
			FindBindingByName(MovieScene, TEXT("SM_Craty_Crate")));
		TestNull(*FString::Printf(TEXT("%s removed the stale sequence character binding"), Fixture.Key),
			FindBindingByName(MovieScene, TEXT("BP Sequence Char C")));
		TestFalse(*FString::Printf(TEXT("%s removed vendor event tracks"), Fixture.Key),
			HasTrackClass(MovieScene, TEXT("MovieSceneEventTrack")));
	}

	UMovieScene* MainMovieScene = Main->GetMovieScene();
	UMovieScene* DodgeMovieScene = Dodge->GetMovieScene();
	TestEqual(TEXT("Main playback begins at frame zero"),
		MainMovieScene->GetPlaybackRange().GetLowerBoundValue(),
		DisplayFrameToTick(MainMovieScene, 0));
	TestEqual(TEXT("Main playback ends at frame 223"),
		MainMovieScene->GetPlaybackRange().GetUpperBoundValue(),
		DisplayFrameToTick(MainMovieScene, 223));
	TestEqual(TEXT("Dodge playback begins at frame zero"),
		DodgeMovieScene->GetPlaybackRange().GetLowerBoundValue(),
		DisplayFrameToTick(DodgeMovieScene, 0));
	TestEqual(TEXT("Dodge playback ends at frame 105"),
		DodgeMovieScene->GetPlaybackRange().GetUpperBoundValue(),
		DisplayFrameToTick(DodgeMovieScene, 105));

	const TArray<double> MainActionTransform = {
		-80.0, -300.0, 150.0, 0.0, -7.2, 57.7, 1.0, 1.0, 1.0 };
	const TArray<double> MainWideTransform = {
		-450.0, -650.0, 260.0, 0.0, -11.1, 50.3, 1.0, 1.0, 1.0 };
	const FMovieSceneBinding* MainActionCamera =
		FindBindingByName(MainMovieScene, TEXT("ActionCamera"));
	const FMovieSceneBinding* MainWideCamera =
		FindBindingByName(MainMovieScene, TEXT("WideCamera"));
	TestTrue(TEXT("Main action camera starts on the authored local fight composition"),
		TransformMatchesAtDisplayFrame(MainMovieScene,
			MainActionCamera, 0, MainActionTransform));
	TestTrue(TEXT("Main action camera cannot drift back to the vendor set"),
		TransformMatchesAtDisplayFrame(MainMovieScene,
			MainActionCamera, 222, MainActionTransform));
	TestTrue(TEXT("Main wide camera starts on the authored local fight composition"),
		TransformMatchesAtDisplayFrame(MainMovieScene,
			MainWideCamera, 0, MainWideTransform));
	TestTrue(TEXT("Main wide camera cannot drift back to the vendor set"),
		TransformMatchesAtDisplayFrame(MainMovieScene,
			MainWideCamera, 222, MainWideTransform));

	const FMovieSceneBinding* Controller =
		FindBindingByName(MainMovieScene, TEXT("BP_NarrativePlayerController"));
	TestNotNull(TEXT("Main binds the Narrative player controller"), Controller);
	UMovieSceneTrack* QTETrack = FindTrackByClassName(Controller, TEXT("BP_QTETrack_C"));
	TestNotNull(TEXT("Main uses Narrative Pro's QTE track"), QTETrack);
	if (QTETrack)
	{
		const TArray<UMovieSceneSection*>& Sections = QTETrack->GetAllSections();
		TestEqual(TEXT("Main has one reachable QTE section"), Sections.Num(), 1);
		if (Sections.Num() == 1)
		{
			UMovieSceneSection* QTESection = Sections[0];
			TestTrue(TEXT("QTE prompt spans frames 64 through 124"),
				SectionHasRange(MainMovieScene, QTESection, 64, 124));
			TestEqual(TEXT("QTE section authors exactly one option"),
				GetArrayNum(QTESection, TEXT("QuicktimeOptions")), 1);
			const FString Destination = ExportArrayStructPropertyValue(
				QTESection, TEXT("QuicktimeOptions"), TEXT("OptionDestination"));
			TestTrue(TEXT("Dodge option branches to the local Territory sequence"),
				Destination.Contains(TEXT("LS_Territory_Fight_QTE_Dodge")));
			TestFalse(TEXT("Dodge option no longer branches into Narrative's demo content"),
				Destination.Contains(TEXT("/NarrativePro/Pro/Demo/Cinematics/Craty")));
		}
	}

	const FMovieSceneBinding* MainController =
		FindBindingByName(MainMovieScene, TEXT("BP_NarrativePlayerController"));
	const FMovieSceneBinding* DodgeController =
		FindBindingByName(DodgeMovieScene, TEXT("BP_NarrativePlayerController"));
	UMovieSceneTrack* MainInput = FindTrackByClassName(MainController, TEXT("BP_InputTrack_C"));
	UMovieSceneTrack* DodgeInput = FindTrackByClassName(DodgeController, TEXT("BP_InputTrack_C"));
	TestNotNull(TEXT("Main uses Narrative Pro's Input track"), MainInput);
	TestNotNull(TEXT("Dodge branch uses Narrative Pro's Input track"), DodgeInput);
	if (MainInput && MainInput->GetAllSections().Num() == 1)
	{
		TestTrue(TEXT("Main input policy covers the complete shot"),
			SectionHasRange(MainMovieScene, MainInput->GetAllSections()[0], 0, 223));
	}
	if (DodgeInput && DodgeInput->GetAllSections().Num() == 1)
	{
		TestTrue(TEXT("Dodge input policy covers the complete branch"),
			SectionHasRange(DodgeMovieScene, DodgeInput->GetAllSections()[0], 0, 105));
	}
	TestNull(TEXT("Dodge branch has no recursive QTE track"),
		FindTrackByClassName(DodgeController, TEXT("BP_QTETrack_C")));

	const UMovieSceneCameraCutTrack* MainCuts =
		Cast<UMovieSceneCameraCutTrack>(MainMovieScene->GetCameraCutTrack());
	const UMovieSceneCameraCutTrack* DodgeCuts =
		Cast<UMovieSceneCameraCutTrack>(DodgeMovieScene->GetCameraCutTrack());
	TestNotNull(TEXT("Main has a Camera Cut track"), MainCuts);
	TestNotNull(TEXT("Dodge branch has a Camera Cut track"), DodgeCuts);
	if (MainCuts)
	{
		const TArray<UMovieSceneSection*>& Sections = MainCuts->GetAllSections();
		TestEqual(TEXT("Main keeps two authored camera beats"), Sections.Num(), 2);
		if (Sections.Num() == 2)
		{
			TestTrue(TEXT("First main camera beat covers frames 0 through 77"),
				SectionHasRange(MainMovieScene, Sections[0], 0, 77));
			TestTrue(TEXT("Second main camera beat covers frames 77 through 223"),
				SectionHasRange(MainMovieScene, Sections[1], 77, 223));
		}
	}
	if (DodgeCuts)
	{
		const TArray<UMovieSceneSection*>& Sections = DodgeCuts->GetAllSections();
		TestEqual(TEXT("Dodge branch has one continuous camera beat"), Sections.Num(), 1);
		if (Sections.Num() == 1)
		{
			TestTrue(TEXT("Dodge camera covers its complete playback range"),
				SectionHasRange(DodgeMovieScene, Sections[0], 0, 105));
		}
	}

	const FMovieSceneBinding* Opponent =
		FindBindingByName(MainMovieScene, TEXT("OpponentCine"));
	TestNotNull(TEXT("Main keeps one authored cinematic opponent"), Opponent);
	TestEqual(TEXT("Main resolves one live Narrative opponent through the Opponent tag"),
		Main->FindBindingsByTag(TEXT("Opponent")).Num(), 1);
	TestEqual(TEXT("Dodge branch does not own or duplicate the live opponent"),
		Dodge->FindBindingsByTag(TEXT("Opponent")).Num(), 0);
	if (Opponent)
	{
		const FGuid OpponentGuid = Opponent->GetObjectGuid();
		TestNotNull(TEXT("Opponent is a possessable owned by live gameplay"),
			MainMovieScene->FindPossessable(OpponentGuid));
		TestNull(TEXT("Opponent is not a Sequencer-spawned duplicate combat actor"),
			MainMovieScene->FindSpawnable(OpponentGuid));
		TestNull(TEXT("Tagged opponent has no spawn track"),
			FindTrackByClassName(Opponent, TEXT("MovieSceneSpawnTrack")));
		UMovieSceneTrack* OpponentTransform =
			FindTrackByClassName(Opponent, TEXT("MovieScene3DTransformTrack"));
		TestNotNull(TEXT("Opponent keeps its authored transform"), OpponentTransform);
		if (OpponentTransform && OpponentTransform->GetAllSections().Num() == 1)
		{
			TestTrue(TEXT("Opponent transform remains evaluated for the complete shot"),
				SectionHasRange(MainMovieScene,
					OpponentTransform->GetAllSections()[0], 0, 223));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryHDRSceneMakerMetadataContract,
	"TerritoryFramework.Editor.Environment.HDRSceneMakerMetadataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryHDRSceneMakerMetadataContract::RunTest(const FString& Parameters)
{
	const FTerritoryHDRSceneOptions Options;
	TestTrue(TEXT("Balanced Gameplay is the safe default"),
		Options.Quality == ETerritoryHDRSceneQuality::Balanced);
	TestTrue(TEXT("Narrative UDS integration is enabled by default"),
		Options.bEnsureNarrativeUltraDynamicSky);
	TestEqual(TEXT("Narrative UDS bridge uses the expected project plugin path"),
		Options.NarrativeUltraDynamicSkyClass.ToSoftObjectPath().ToString(),
		FString(TEXT("/NP_UltraDynamicSky/Narrative_UDS_Sky.Narrative_UDS_Sky_C")));
	TestTrue(TEXT("Loaded-scene memory analysis is enabled by default"),
		Options.bAnalyzeLoadedSceneMemory);
	TestTrue(TEXT("Default memory budget is positive"),
		Options.LoadedSceneMemoryBudgetMB > 0.f);
	TestTrue(TEXT("Final readiness audit is enabled by default"),
		Options.bRunSceneReadinessAudit);
	TestTrue(TEXT("Conflicting unbound post-process detection is enabled by default"),
		Options.bWarnAboutConflictingUnboundPostProcessVolumes);
	TestTrue(TEXT("Ray-tracing audit threshold is positive"),
		Options.MinimumRayTracingGeometryPoolMB > 0.f);
	TestTrue(TEXT("Default cinematic look uses restrained film grain"),
		Options.FilmGrainIntensity >= 0.f && Options.FilmGrainIntensity <= 0.1f);
	TestFalse(TEXT("Level-wide saturation and contrast grading is opt-in"),
		Options.bApplyGlobalColorGrading);
	TestTrue(TEXT("Default saturation is neutral"),
		FMath::IsNearlyEqual(Options.GlobalSaturation, 1.f));
	TestTrue(TEXT("Default contrast is neutral"),
		FMath::IsNearlyEqual(Options.GlobalContrast, 1.f));
	TestTrue(TEXT("Default global chromatic aberration remains disabled"),
		FMath::IsNearlyZero(Options.ChromaticAberrationIntensity));

	const UClass* ToolClass = UTerritoryHDRSceneEditorLibrary::StaticClass();
	TestNotNull(TEXT("AAA HDR Scene Maker Blueprint node exists"),
		ToolClass->FindFunctionByName(TEXT("CreateOrUpdateAAAHDRScene")));
	TestNotNull(TEXT("Loaded Scene Memory Blueprint node exists"),
		ToolClass->FindFunctionByName(TEXT("AnalyzeLoadedSceneMemory")));
	TestNotNull(TEXT("Final AAA HDR Scene Audit Blueprint node exists"),
		ToolClass->FindFunctionByName(TEXT("AuditAAAHDRScene")));

	for (TFieldIterator<FProperty> It(FTerritoryHDRSceneOptions::StaticStruct()); It; ++It)
	{
		TestFalse(*FString::Printf(TEXT("%s exposes an artist tooltip"),
			*It->GetName()), It->GetToolTipText().IsEmpty());
	}
	for (TFieldIterator<FProperty> It(FTerritoryHDRSceneAuditItem::StaticStruct()); It; ++It)
	{
		TestFalse(*FString::Printf(TEXT("Audit item %s exposes an artist tooltip"),
			*It->GetName()), It->GetToolTipText().IsEmpty());
	}
	return true;
}

/**
 * Contract for the designer-authored dialogue shot pack. Read-only on purpose: it never builds or
 * applies a pack, so running the suite cannot mark project content dirty. The spec-driven build and
 * apply are exercised by the integration run that authors DA_TerritoryStudioShotPack.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryDialogueShotPackSpecContract,
	"TerritoryFramework.Editor.Cinematics.DialogueShotPackSpecContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryDialogueShotPackSpecContract::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Shot pack spec is a primary data asset so it can be saved under /Game"),
		UTerritoryDialogueShotPackSpec::StaticClass()->IsChildOf(
			UPrimaryDataAsset::StaticClass()));
	TestTrue(TEXT("Shot pack spec rows are exposed to designers"),
		FTerritoryStudioShotSpec::StaticStruct() != nullptr);

	const UClass* ToolClass = UTerritoryDialogueShotEditorLibrary::StaticClass();
	const UFunction* CreateFunction =
		ToolClass->FindFunctionByName(TEXT("CreateStudioDialogueShotPack"));
	TestNotNull(TEXT("Create Studio Dialogue Shot Pack is still Blueprint callable"),
		CreateFunction);
	if (CreateFunction)
	{
		// NumParms counts the return value as well: destination + spec + return.
		TestEqual(TEXT("Create takes the destination folder plus the optional shot spec"),
			CreateFunction->NumParms, 3);
	}
	const UFunction* ApplyFunction =
		ToolClass->FindFunctionByName(TEXT("ApplyStudioDialogueShotPack"));
	TestNotNull(TEXT("Apply Studio Dialogue Shot Pack is still Blueprint callable"),
		ApplyFunction);
	if (ApplyFunction)
	{
		// NumParms counts the return value as well: dialogue + report + spec + return.
		TestEqual(TEXT("Apply takes the dialogue, the build report and the optional shot spec"),
			ApplyFunction->NumParms, 4);
	}

	// Failure paths must always answer with a reason and never crash.
	const FTerritoryDialogueShotPackBuildReport NullDialogue =
		UTerritoryDialogueShotEditorLibrary::ApplyStudioDialogueShotPack(nullptr,
			FTerritoryDialogueShotPackBuildReport());
	TestFalse(TEXT("Applying a pack to a null dialogue fails"), NullDialogue.bSucceeded);
	TestTrue(TEXT("A null dialogue reports why it failed"), NullDialogue.Errors.Num() > 0);
	TestFalse(TEXT("A null dialogue is never reported as configured"),
		NullDialogue.bDialogueConfigured);

	FTerritoryDialogueShotPackBuildReport RejectedPack;
	RejectedPack.bSucceeded = false;
	TestFalse(TEXT("An unsuccessful pack is rejected before a dialogue is touched"),
		UTerritoryDialogueShotEditorLibrary::ApplyStudioDialogueShotPack(nullptr,
			RejectedPack).bSucceeded);

	// Every exposed property must carry an artist tooltip, matching the repository convention.
	for (TFieldIterator<FProperty> It(FTerritoryStudioShotSpec::StaticStruct()); It; ++It)
	{
		TestFalse(*FString::Printf(TEXT("Shot spec field %s exposes an artist tooltip"),
			*It->GetName()), It->GetToolTipText().IsEmpty());
	}

	// Role identity: every built-in role maps to a valid, distinct native tag.
	const FGameplayTag EstablishingTag =
		TerritoryShotRoleTags::FromLegacyRole(ETerritoryDialogueShotRole::Establishing);
	const FGameplayTag InsertTag =
		TerritoryShotRoleTags::FromLegacyRole(ETerritoryDialogueShotRole::Insert);
	TestTrue(TEXT("Built-in roles map to valid native tags"),
		EstablishingTag.IsValid() && InsertTag.IsValid());
	TestTrue(TEXT("Distinct roles map to distinct tags"), EstablishingTag != InsertTag);
	TestTrue(TEXT("Two Shot maps to its documented tag"),
		TerritoryShotRoleTags::FromLegacyRole(ETerritoryDialogueShotRole::TwoShot)
			== TerritoryShotRoleTags::TwoShot);

	UTerritoryDialogueShot* ProbeShot = NewObject<UTerritoryDialogueShot>();
	ProbeShot->ShotRoleTag = TerritoryShotRoleTags::CloseUp;
	TestTrue(TEXT("A shot reports the role tag it carries"),
		ProbeShot->GetShotRoleTag() == TerritoryShotRoleTags::CloseUp);

	TestFalse(TEXT("Ensuring a speaker on a null dialogue fails"),
		UTerritoryDialogueShotEditorLibrary::EnsureDialogueSpeaker(nullptr,
			TEXT("Nobody"), nullptr));
	TestFalse(TEXT("Ensuring a speaker without an NPC definition fails"),
		UTerritoryDialogueShotEditorLibrary::EnsureDialogueSpeaker(nullptr,
			NAME_None, nullptr));

	// Optional project fixture: the authored spec must resolve all three dialogue slots, either
	// through a slot row or through the built-in Two Shot / Medium Close-Up / Reaction roles.
	static const TCHAR* SpecName = TEXT("DA_TerritoryStudioShotPack");
	const FString SpecFolder =
		TEXT("/Game/TerritoryFramework/Cinematics/Dialogue");
	const UTerritoryDialogueShotPackSpec* Spec = LoadObject<UTerritoryDialogueShotPackSpec>(
		nullptr, *FString::Printf(TEXT("%s/%s.%s"), *SpecFolder, SpecName, SpecName));
	if (!Spec)
	{
		AddInfo(TEXT("Skipped optional project shot pack spec fixture; author DA_TerritoryStudioShotPack to cover it."));
		return true;
	}

	TestTrue(TEXT("Project shot pack spec authors at least one row"),
		Spec->StudioShots.Num() > 0);
	bool bMaster = false;
	bool bSpeakerCoverage = false;
	bool bReplySelection = false;
	for (const FTerritoryStudioShotSpec& Row : Spec->StudioShots)
	{
		TestFalse(*FString::Printf(TEXT("Spec row '%s' names a shot asset"),
			*Row.AssetName), Row.AssetName.IsEmpty());
		TestTrue(*FString::Printf(TEXT("Spec row '%s' frames a usable focal length"),
			*Row.AssetName), Row.FocalLength > 0.f);
		TestTrue(*FString::Printf(TEXT("Spec row '%s' documents its editorial use"),
			*Row.AssetName), !Row.BestUse.IsEmpty());
		bMaster |= Row.Slot == ETerritoryDialogueShotSlot::Master
			|| Row.Role == ETerritoryDialogueShotRole::TwoShot;
		bSpeakerCoverage |= Row.Slot == ETerritoryDialogueShotSlot::SpeakerCoverage
			|| Row.Role == ETerritoryDialogueShotRole::MediumCloseUp;
		bReplySelection |= Row.Slot == ETerritoryDialogueShotSlot::ReplySelection
			|| Row.Role == ETerritoryDialogueShotRole::Reaction;
	}
	TestTrue(TEXT("Spec resolves a master shot"), bMaster);
	TestTrue(TEXT("Spec resolves speaker coverage"), bSpeakerCoverage);
	TestTrue(TEXT("Spec resolves reply selection"), bReplySelection);
	return true;
}

#endif
