#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/PackageName.h"
#include "Editor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Environment/TerritoryHDRSceneEditorLibrary.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryHDRLightingPresets,
	"TerritoryFramework.Editor.Environment.UDSLightingAndExposureOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryHDRLightingPresets::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->PlayWorld) return false;
	UWorld* PreviousWorld = GEditor->GetEditorWorldContext().World();
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!World) return false;
	GEditor->GetEditorWorldContext().SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEditor->GetEditorWorldContext().SetCurrentWorld(PreviousWorld);
		World->DestroyWorld(false);
	};
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	FTerritoryHDRSceneOptions Options;
	// The included utility may be saved while its optional plugin is absent.
	Options.NarrativeUltraDynamicSkyClass.Reset();
	Options.bAnalyzeLoadedSceneMemory = false;
	Options.bRunSceneReadinessAudit = false;
	Options.ExposureCompensation = -0.4f;
	Options.MinimumEV100 = -3.f;
	Options.MaximumEV100 = 15.f;
	// HopDistrictTest has an older unbound volume whose serialized spatial flag
	// is still true. A fresh volume alone does not exercise the engine assertion.
	auto* ExistingVolume = World->SpawnActor<APostProcessVolume>();
	ExistingVolume->Tags.Add(TEXT("Territory.AAA.PostProcess"));
	ExistingVolume->SetIsSpatiallyLoaded(true);
	ExistingVolume->bUnbound = true;
	const auto First = UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(Options);
	if (!FPackageName::DoesPackageExist(TEXT("/NP_UltraDynamicSky/Narrative_UDS_Sky")))
	{
		TestFalse(TEXT("Missing optional UDS integration reports failure"), First.bSucceeded);
		TestNull(TEXT("Missing UDS integration does not create a fake sky"), First.NarrativeUltraDynamicSkyActor);
		TestTrue(TEXT("The existing post-process volume survives the failed setup"), IsValid(ExistingVolume));
		AddWarning(TEXT("UDS is not installed: its missing-dependency failure path was tested; live lighting checks were not run."));
		return true;
	}
	if (!TestTrue(TEXT("Actual Narrative UDS setup succeeds"), First.bSucceeded)
		|| !First.NarrativeUltraDynamicSkyActor || !First.PostProcessVolume) return false;
	AActor* Sky = First.NarrativeUltraDynamicSkyActor;
	APostProcessVolume* Volume = First.PostProcessVolume;
	FTerritoryHDRSceneOptions ExplicitInvalid = Options;
	ExplicitInvalid.NarrativeUltraDynamicSkyClass = APostProcessVolume::StaticClass();
	TestFalse(TEXT("An explicit incompatible class is not replaced by the default sky"),
		UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(ExplicitInvalid).bSucceeded);
	TestEqual(TEXT("Existing unbound volume is migrated in place"), Volume, ExistingVolume);
	const auto ReadNumber = [Sky](FName Name)
	{
		const FDoubleProperty* Property = FindFProperty<FDoubleProperty>(Sky->GetClass(), Name);
		return Property ? Property->GetPropertyValue_InContainer(Sky) : -999.;
	};
	TestTrue(TEXT("UDS receives authored daylight intensity"), FMath::IsNearlyEqual(ReadNumber(TEXT("Sun Light Intensity")), 10.));
	TestTrue(TEXT("UDS receives exposure compensation"), FMath::IsNearlyEqual(ReadNumber(TEXT("Exposure Bias Day")), -0.4, 0.001));
	TestTrue(TEXT("Indoor fog is reduced by the existing UDS occlusion system"), FMath::IsNearlyEqual(ReadNumber(TEXT("Fog Density Multiplier in Interior")), 0.25));
	TestFalse(TEXT("Global sky cannot stream out spatially"), Sky->GetIsSpatiallyLoaded());
	TestFalse(TEXT("Global post process cannot stream out spatially"), Volume->GetIsSpatiallyLoaded());
	TestFalse(TEXT("UDS owns exposure bias"), Volume->Settings.bOverride_AutoExposureBias);
	TestFalse(TEXT("UDS owns the adaptation curve"), Volume->Settings.bOverride_AutoExposureBiasCurve);
	TestFalse(TEXT("UDS owns the EV100 range"), Volume->Settings.bOverride_AutoExposureMinBrightness);
	const double GameplayCloudSamples = ReadNumber(TEXT("View Sample Scale (Day)"));
	Options.Quality = ETerritoryHDRSceneQuality::AAACinematic;
	const auto Cinematic = UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(Options);
	TestTrue(TEXT("Cinematic update succeeds"), Cinematic.bSucceeded);
	TestEqual(TEXT("Changing quality reuses the same sky authority"), Cinematic.NarrativeUltraDynamicSkyActor.Get(), Sky);
	TestEqual(TEXT("Changing quality reuses the same post process"), Cinematic.PostProcessVolume.Get(), Volume);
	TestTrue(TEXT("Cinematic clouds increase sampling"), ReadNumber(TEXT("View Sample Scale (Day)")) > GameplayCloudSamples);
	TestEqual(TEXT("Cinematic day/night GI propagates faster"), Volume->Settings.LumenSceneLightingUpdateSpeed, 2.f);
	Options.Quality = ETerritoryHDRSceneQuality::Balanced;
	TestTrue(TEXT("Gameplay preset can be restored"), UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(Options).bSucceeded);
	TestEqual(TEXT("Gameplay restores its GI update budget"), Volume->Settings.LumenSceneLightingUpdateSpeed, 1.f);

	// Changing only post-processing must still respect an existing UDS exposure owner.
	Options.bEnsureNarrativeUltraDynamicSky = false;
	Volume->Settings.bOverride_AutoExposureBias = true;
	TestTrue(TEXT("Post-process-only updates remain supported"), UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(Options).bSucceeded);
	TestFalse(TEXT("Post-process-only update repairs a stale competing override"), Volume->Settings.bOverride_AutoExposureBias);
	Options.SunLightIntensity = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite Blueprint inputs fail before scene mutation"),
		UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(Options).bSucceeded);
	TestTrue(TEXT("Invalid input preserves the existing sky"), FMath::IsNearlyEqual(ReadNumber(TEXT("Sun Light Intensity")), 10.));
	Options.SunLightIntensity = 10.f;

	auto* Duplicate = World->SpawnActor<APostProcessVolume>();
	Duplicate->Tags.Add(TEXT("Territory.AAA.PostProcess"));
	Options.BloomIntensity = 0.9f;
	const float PreviousBloom = Volume->Settings.BloomIntensity;
	const auto Rejected = UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(Options);
	TestFalse(TEXT("Duplicate authority fails before editing the scene"), Rejected.bSucceeded);
	TestEqual(TEXT("Duplicate rejection provides a structured error"), Rejected.Errors.Num(), 1);
	TestEqual(TEXT("Rejected setup leaves the existing look unchanged"), Volume->Settings.BloomIntensity, PreviousBloom);
	return true;
}

#endif
