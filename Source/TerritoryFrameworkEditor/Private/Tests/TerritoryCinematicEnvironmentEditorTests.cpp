#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cinematics/TerritoryDialogueShotEditorLibrary.h"
#include "Environment/TerritoryHDRSceneEditorLibrary.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
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
	const FString Folder = TEXT("/Game/TerritoryFramework/Cinematics/Dialogue/Shots");
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

#endif
