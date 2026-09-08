#include "Environment/TerritoryHDRSceneEditorLibrary.h"

#include "Editor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/ActorComponent.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "TerritoryHDRSceneEditorLibrary"

namespace TerritoryHDRSceneEditor
{
	const FName PostProcessTag(TEXT("Territory.AAA.PostProcess"));
	const FName NarrativeUDSTag(TEXT("Territory.AAA.NarrativeUDS"));

	UClass* LoadNarrativeSkyClass(const FTerritoryHDRSceneOptions& Options)
	{
		// A Blueprint saved without the optional integration can have an empty
		// soft-class pin. Keep the same default as a newly created options struct.
		// An explicit class, including one that cannot load, remains authoritative.
		return (Options.NarrativeUltraDynamicSkyClass.IsNull()
			? FTerritoryHDRSceneOptions().NarrativeUltraDynamicSkyClass
			: Options.NarrativeUltraDynamicSkyClass).LoadSynchronous();
	}

	UWorld* GetEditorWorld(FText& OutError)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World || World->IsGameWorld() || GEditor->PlayWorld)
		{
			OutError = LOCTEXT("EditorWorldRequired",
				"Open a level in the editor and stop PIE before running the AAA HDR Scene Maker.");
			return nullptr;
		}
		return World;
	}

	bool SkyOwnsExposure(const AActor* Sky)
	{
		const FBoolProperty* Property = Sky ? FindFProperty<FBoolProperty>(
			Sky->GetClass(), TEXT("Apply Exposure Settings")) : nullptr;
		return Property && Property->GetPropertyValue_InContainer(Sky);
	}

	bool ConfigureSky(AActor* Sky, const FTerritoryHDRSceneOptions& Options,
		FTerritoryHDRSceneBuildReport& Report, bool bApply = true)
	{
		if (!Sky || !Options.bConfigureSkyLighting) return true;
		const bool bCinematic = Options.Quality == ETerritoryHDRSceneQuality::AAACinematic;
		const bool bPerformance = Options.Quality == ETerritoryHDRSceneQuality::Performance;
		const TArray<TPair<FName, double>> Numbers = {
			{TEXT("Sun Light Intensity"), FMath::Clamp(Options.SunLightIntensity, 0.f, 200000.f)},
			{TEXT("Moon Light Intensity"), FMath::Clamp(Options.MoonLightIntensity, 0.f, 10.f)},
			{TEXT("Sky Light Intensity"), FMath::Clamp(Options.SkyLightIntensity, 0.f, 10.f)},
			{TEXT("Base Fog Density"), FMath::Clamp(Options.BaseFogDensity, 0.f, 0.1f)},
			{TEXT("Volumetric Fog Distance"), bCinematic ? 24000. : 16000.},
			{TEXT("Volumetric Fog Extinction"), 1.},
			{TEXT("Fog Density Multiplier in Interior"), FMath::Clamp(Options.InteriorFogMultiplier, 0.f, 1.f)},
			{TEXT("Exposure Bias in Interior"), FMath::Clamp(Options.InteriorExposureBias, -2.f, 2.f)},
			{TEXT("Exposure Bias Day"), FMath::Clamp(Options.ExposureCompensation, -5.f, 5.f)},
			{TEXT("Exposure Bias Dawn/Dusk"), FMath::Clamp(Options.ExposureCompensation, -5.f, 5.f)},
			{TEXT("Exposure Bias Night"), FMath::Clamp(Options.ExposureCompensation, -5.f, 5.f)},
			{TEXT("View Sample Scale (Day)"), bCinematic ? 3. : (bPerformance ? 1. : 2.2)},
			{TEXT("View Sample Scale (Night)"), bCinematic ? 2.2 : (bPerformance ? 0.8 : 1.7)},
			{TEXT("Shadow Sample Scale"), bCinematic ? 0.6 : 0.4}
		};
		const TArray<TPair<FName, bool>> Bools = {
			{TEXT("Apply Exposure Settings"), true},
			{TEXT("Apply Interior Adjustments"), Options.bApplyInteriorAdjustments},
			{TEXT("Use Volumetric Fog"), !bPerformance},
			{TEXT("Render Exponential Height Fog"), true},
			{TEXT("Render Sky Light"), true},
			{TEXT("Render Sun Directional Light"), true},
			{TEXT("Render Moon Directional Light"), true},
			{TEXT("Sun Casts Shadows"), true},
			{TEXT("Moon Casts Shadows"), true},
			{TEXT("Use Cloud Shadows"), true},
			{TEXT("Real Time Capture"), true},
			{TEXT("Real Time Capture Uses Time Slicing"), true}
		};
		const TArray<FName> Mobilities = {TEXT("Sun Mobility"), TEXT("Moon Mobility"), TEXT("Sky Light Mobility")};
		UClass* Class = Sky->GetClass();
		TArray<FString> Missing;
		for (const auto& Entry : Numbers)
		{
			if (!FindFProperty<FDoubleProperty>(Class, Entry.Key) || !FMath::IsFinite(Entry.Value)) Missing.Add(Entry.Key.ToString());
		}
		for (const auto& Entry : Bools)
		{
			if (!FindFProperty<FBoolProperty>(Class, Entry.Key)) Missing.Add(Entry.Key.ToString());
		}
		for (FName Name : Mobilities)
		{
			const FByteProperty* Property = FindFProperty<FByteProperty>(Class, Name);
			if (!Property || !Property->Enum || Property->Enum->GetFName() != TEXT("EComponentMobility")) Missing.Add(Name.ToString());
		}
		FByteProperty* Metering = FindFProperty<FByteProperty>(Class, TEXT("Exposure Metering Mode"));
		FStructProperty* Range = FindFProperty<FStructProperty>(Class, TEXT("Exposure Brightness Range"));
		if (!Metering || !Metering->Enum || Metering->Enum->GetFName() != TEXT("EAutoExposureMethod")) Missing.Add(TEXT("Exposure Metering Mode"));
		if (!Range || Range->Struct->GetFName() != TEXT("FloatRange")) Missing.Add(TEXT("Exposure Brightness Range"));
		if (!FMath::IsFinite(Options.MinimumEV100) || !FMath::IsFinite(Options.MaximumEV100)) Missing.Add(TEXT("Finite EV100 range"));
		if (!Missing.IsEmpty())
		{
			Report.Errors.Add(FText::FromString(FString::Printf(
				TEXT("UDS lighting was not changed: incompatible properties or values: %s. Review the installed UDS version."), *FString::Join(Missing, TEXT(", ")))));
			return false;
		}
		if (!bApply) return true;
		// Validate the entire optional Blueprint contract before making any writes.
		// UDS remains the owner of components, time-dependent colors and exposure.
		Sky->Modify();
		for (const auto& Entry : Numbers) FindFProperty<FDoubleProperty>(Class, Entry.Key)->SetPropertyValue_InContainer(Sky, Entry.Value);
		for (const auto& Entry : Bools) FindFProperty<FBoolProperty>(Class, Entry.Key)->SetPropertyValue_InContainer(Sky, Entry.Value);
		for (FName Name : Mobilities) FindFProperty<FByteProperty>(Class, Name)->SetPropertyValue_InContainer(Sky, EComponentMobility::Movable);
		Metering->SetPropertyValue_InContainer(Sky, AEM_Histogram);
		*Range->ContainerPtrToValuePtr<FFloatRange>(Sky) = FFloatRange(
			FMath::Clamp(FMath::Min(Options.MinimumEV100, Options.MaximumEV100), -10.f, 20.f),
			FMath::Clamp(FMath::Max(Options.MinimumEV100, Options.MaximumEV100), -10.f, 20.f));
		// Construction applies UDS's static controls and resets its derived caches.
		Sky->RerunConstructionScripts();
		if (Sky->CanChangeIsSpatiallyLoadedFlag()) Sky->SetIsSpatiallyLoaded(false);
		Sky->MarkPackageDirty();
		return true;
	}

	void ConfigurePostProcess(APostProcessVolume* Volume,
		const FTerritoryHDRSceneOptions& Options, bool bSkyExposure)
	{
		if (!Volume) return;
		Volume->Modify();
		Volume->bEnabled = true;
		// Existing unbound volumes lock this flag even when legacy serialized data
		// still has it set. Clear it while bound, then apply the final global mode.
		Volume->bUnbound = false;
		if (Volume->CanChangeIsSpatiallyLoadedFlag()) Volume->SetIsSpatiallyLoaded(false);
		Volume->bUnbound = true;
		Volume->BlendWeight = 1.f;
		Volume->Priority = 90.f;

		FPostProcessSettings& PP = Volume->Settings;
		PP.bOverride_DynamicGlobalIlluminationMethod = true;
		PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		PP.bOverride_ReflectionMethod = true;
		PP.ReflectionMethod = EReflectionMethod::Lumen;
		PP.bOverride_AutoExposureMethod = !bSkyExposure;
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		PP.bOverride_AutoExposureMinBrightness = !bSkyExposure;
		PP.AutoExposureMinBrightness = FMath::Min(Options.MinimumEV100, Options.MaximumEV100);
		PP.bOverride_AutoExposureMaxBrightness = !bSkyExposure;
		PP.AutoExposureMaxBrightness = FMath::Max(Options.MinimumEV100, Options.MaximumEV100);
		PP.bOverride_AutoExposureBias = !bSkyExposure;
		PP.AutoExposureBias = Options.ExposureCompensation;
		PP.bOverride_AutoExposureBiasCurve = !bSkyExposure;
		PP.AutoExposureBiasCurve = nullptr;
		PP.bOverride_AutoExposureLowPercent = true;
		PP.AutoExposureLowPercent = 70.f;
		PP.bOverride_AutoExposureHighPercent = true;
		PP.AutoExposureHighPercent = 90.f;
		PP.bOverride_AutoExposureSpeedUp = true;
		PP.AutoExposureSpeedUp = 3.f;
		PP.bOverride_AutoExposureSpeedDown = true;
		PP.AutoExposureSpeedDown = 1.f;
		PP.bOverride_LocalExposureHighlightContrastScale = true;
		PP.LocalExposureHighlightContrastScale = 0.8f;
		PP.bOverride_LocalExposureShadowContrastScale = true;
		PP.LocalExposureShadowContrastScale = 0.8f;
		PP.bOverride_LocalExposureDetailStrength = true;
		PP.LocalExposureDetailStrength = 1.f;
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = FMath::Max(0.f, Options.BloomIntensity);
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = FMath::Clamp(Options.VignetteIntensity, 0.f, 1.f);
		PP.bOverride_MotionBlurAmount = true;
		PP.MotionBlurAmount = FMath::Clamp(Options.MotionBlurAmount, 0.f, 1.f);
		PP.bOverride_MotionBlurMax = true;
		PP.MotionBlurMax = 20.f;
		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = FMath::Clamp(Options.WhiteBalanceTemperature, 1500.f, 15000.f);
		// A level-wide saturation/contrast override can fight UDS time-of-day color
		// and shot-specific grading. Keep the tool-owned volume neutral unless an
		// artist explicitly opts into a reviewed global grade. Assigning neutral
		// values while clearing the overrides also repairs stale settings from an
		// earlier run of the scene maker.
		PP.bOverride_ColorSaturation = Options.bApplyGlobalColorGrading;
		const float Saturation = FMath::Clamp(
			Options.GlobalSaturation, 0.75f, 1.25f);
		PP.ColorSaturation = FVector4(
			Saturation, Saturation, Saturation, Saturation);
		PP.bOverride_ColorContrast = Options.bApplyGlobalColorGrading;
		const float Contrast = FMath::Clamp(
			Options.GlobalContrast, 0.75f, 1.25f);
		PP.ColorContrast = FVector4(Contrast, Contrast, Contrast, Contrast);
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = FMath::Clamp(Options.FilmGrainIntensity, 0.f, 1.f);
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = FMath::Clamp(
			Options.ChromaticAberrationIntensity, 0.f, 5.f);

		const bool bAAA = Options.Quality == ETerritoryHDRSceneQuality::AAACinematic;
		const bool bPerformance = Options.Quality == ETerritoryHDRSceneQuality::Performance;
		PP.bOverride_LumenSceneLightingQuality = true;
		PP.LumenSceneLightingQuality = bAAA ? 2.f : (bPerformance ? 0.75f : 1.25f);
		PP.bOverride_LumenSceneDetail = true;
		PP.LumenSceneDetail = bAAA ? 2.f : (bPerformance ? 0.75f : 1.25f);
		PP.bOverride_LumenSceneViewDistance = true;
		PP.LumenSceneViewDistance = bAAA ? 80000.f : (bPerformance ? 25000.f : 50000.f);
		PP.bOverride_LumenFinalGatherQuality = true;
		PP.LumenFinalGatherQuality = bAAA ? 2.f : (bPerformance ? 0.75f : 1.25f);
		PP.bOverride_LumenReflectionQuality = true;
		PP.LumenReflectionQuality = bAAA ? 2.f : (bPerformance ? 0.75f : 1.25f);
		PP.bOverride_LumenMaxTraceDistance = true;
		PP.LumenMaxTraceDistance = bAAA ? 50000.f : (bPerformance ? 10000.f : 30000.f);
		PP.bOverride_LumenReflectionsScreenTraces = true;
		PP.LumenReflectionsScreenTraces = true;
		PP.bOverride_LumenFinalGatherScreenTraces = true;
		PP.LumenFinalGatherScreenTraces = true;
		PP.bOverride_LumenSceneLightingUpdateSpeed = true;
		PP.LumenSceneLightingUpdateSpeed = bAAA ? 2.f : 1.f;
		PP.bOverride_LumenFinalGatherLightingUpdateSpeed = true;
		PP.LumenFinalGatherLightingUpdateSpeed = bAAA ? 2.f : 1.f;
		Volume->MarkPackageDirty();
	}

	void AddReferencedAssets(UObject* Source, TSet<UObject*>& Assets,
		TArray<UObject*>& FollowUp)
	{
		if (!Source) return;
		TArray<UObject*> References;
		FReferenceFinder Finder(References, nullptr, false, true, false, true);
		Finder.FindReferences(Source);
		for (UObject* Reference : References)
		{
			if (!IsValid(Reference) || !Reference->IsAsset()
				|| Reference->HasAnyFlags(RF_Transient))
			{
				continue;
			}
			if (!Assets.Contains(Reference))
			{
				Assets.Add(Reference);
				FollowUp.Add(Reference);
			}
		}
	}

	void FillMemoryReport(UWorld* World, float BudgetMB, int32 TopItemCount,
		FTerritoryHDRSceneBuildReport& Report)
	{
		TSet<UObject*> Assets;
		TArray<UObject*> FirstLevelAssets;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			AddReferencedAssets(Actor, Assets, FirstLevelAssets);
			TInlineComponentArray<UActorComponent*> Components;
			Actor->GetComponents(Components);
			for (UActorComponent* Component : Components)
			{
				AddReferencedAssets(Component, Assets, FirstLevelAssets);
			}
		}

		// One asset-dependency hop catches the expensive textures, meshes, groom
		// bindings and material resources directly used by loaded actor assets while
		// avoiding an unbounded walk through all Engine defaults.
		const TArray<UObject*> DirectAssets = FirstLevelAssets;
		for (UObject* Asset : DirectAssets)
		{
			TArray<UObject*> IgnoredFollowUp;
			AddReferencedAssets(Asset, Assets, IgnoredFollowUp);
		}

		TArray<FTerritorySceneMemoryItem> Items;
		Items.Reserve(Assets.Num());
		double TotalBytes = 0.0;
		for (UObject* Asset : Assets)
		{
			const int64 Bytes = FMath::Max<int64>(0,
				Asset->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
			TotalBytes += static_cast<double>(Bytes);
			FTerritorySceneMemoryItem& Item = Items.AddDefaulted_GetRef();
			Item.AssetPath = Asset->GetPathName();
			Item.AssetClass = Asset->GetClass()->GetName();
			Item.EstimatedMB = static_cast<float>(Bytes / (1024.0 * 1024.0));
		}
		Items.Sort([](const FTerritorySceneMemoryItem& A,
			const FTerritorySceneMemoryItem& B)
		{
			if (!FMath::IsNearlyEqual(A.EstimatedMB, B.EstimatedMB))
			{
				return A.EstimatedMB > B.EstimatedMB;
			}
			return A.AssetPath < B.AssetPath;
		});
		Report.AnalyzedAssetCount = Items.Num();
		Report.EstimatedLoadedSceneMB = static_cast<float>(
			TotalBytes / (1024.0 * 1024.0));
		Report.TopMemoryAssets = Items;
		Report.TopMemoryAssets.SetNum(FMath::Min(
			FMath::Clamp(TopItemCount, 1, 100), Items.Num()));
		if (Report.EstimatedLoadedSceneMB > FMath::Max(1.f, BudgetMB))
		{
			Report.Warnings.Add(FText::Format(LOCTEXT("MemoryBudgetExceeded",
				"Loaded scene estimate is {0} MiB, above the {1} MiB budget. Review the returned largest assets, then confirm with a cooked platform Unreal Insights memory trace."),
				FText::AsNumber(Report.EstimatedLoadedSceneMB),
				FText::AsNumber(BudgetMB)));
		}
	}

	void AddAuditItem(FTerritoryHDRSceneBuildReport& Report, const FName CheckID,
		const ETerritoryHDRSceneAuditSeverity Severity, const FText& Finding,
		const FText& Recommendation = FText::GetEmpty())
	{
		FTerritoryHDRSceneAuditItem& Item = Report.AuditItems.AddDefaulted_GetRef();
		Item.CheckID = CheckID;
		Item.Severity = Severity;
		Item.Finding = Finding;
		Item.Recommendation = Recommendation;
		switch (Severity)
		{
		case ETerritoryHDRSceneAuditSeverity::Pass:
			++Report.PassedChecks;
			break;
		case ETerritoryHDRSceneAuditSeverity::Advisory:
			++Report.AdvisoryChecks;
			break;
		case ETerritoryHDRSceneAuditSeverity::Warning:
			++Report.WarningChecks;
			Report.Warnings.Add(FText::Format(LOCTEXT("AuditWarningFormat",
				"[{0}] {1} {2}"), FText::FromName(CheckID), Finding,
				Recommendation));
			break;
		case ETerritoryHDRSceneAuditSeverity::Error:
			++Report.ErrorChecks;
			Report.Errors.Add(FText::Format(LOCTEXT("AuditErrorFormat",
				"[{0}] {1} {2}"), FText::FromName(CheckID), Finding,
				Recommendation));
			break;
		default:
			break;
		}
	}

	int32 ReadConsoleInt(const TCHAR* Name, const int32 MissingValue = INDEX_NONE)
	{
		const IConsoleVariable* Variable =
			IConsoleManager::Get().FindConsoleVariable(Name);
		return Variable ? Variable->GetInt() : MissingValue;
	}

	float ReadConsoleFloat(const TCHAR* Name, const float MissingValue = -1.f)
	{
		const IConsoleVariable* Variable =
			IConsoleManager::Get().FindConsoleVariable(Name);
		return Variable ? Variable->GetFloat() : MissingValue;
	}

	void AddRequiredConsoleCheck(FTerritoryHDRSceneBuildReport& Report,
		const FName CheckID, const TCHAR* ConsoleVariable, const int32 ExpectedValue,
		const ETerritoryHDRSceneAuditSeverity FailureSeverity,
		const FText& PassText, const FText& FailureRecommendation)
	{
		const int32 Actual = ReadConsoleInt(ConsoleVariable);
		if (Actual == ExpectedValue)
		{
			AddAuditItem(Report, CheckID, ETerritoryHDRSceneAuditSeverity::Pass,
				PassText);
			return;
		}
		const FText Finding = Actual == INDEX_NONE
			? FText::Format(LOCTEXT("CVarUnavailable",
				"Renderer setting {0} is unavailable in this editor process."),
				FText::FromString(ConsoleVariable))
			: FText::Format(LOCTEXT("CVarUnexpected",
				"Renderer setting {0} is {1}; expected {2}."),
				FText::FromString(ConsoleVariable), FText::AsNumber(Actual),
				FText::AsNumber(ExpectedValue));
		AddAuditItem(Report, CheckID, FailureSeverity, Finding,
			FailureRecommendation);
	}

	void RunSceneReadinessAudit(UWorld* World,
		const FTerritoryHDRSceneOptions& Options,
		FTerritoryHDRSceneBuildReport& Report)
	{
		if (!World) return;

		TArray<APostProcessVolume*> TaggedVolumes;
		TArray<FString> ConflictingVolumeNames;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			APostProcessVolume* Volume = *It;
			if (!IsValid(Volume)) continue;
			if (Volume->Tags.Contains(PostProcessTag))
			{
				TaggedVolumes.Add(Volume);
				if (!Report.PostProcessVolume) Report.PostProcessVolume = Volume;
			}
			else if (Volume->bEnabled && Volume->bUnbound
				&& Volume->BlendWeight > KINDA_SMALL_NUMBER)
			{
				ConflictingVolumeNames.Add(FString::Printf(TEXT("%s (priority %.1f)"),
					*Volume->GetActorLabel(), Volume->Priority));
			}
		}
		Report.ConflictingUnboundPostProcessVolumes = ConflictingVolumeNames.Num();
		if (TaggedVolumes.Num() == 1)
		{
			const APostProcessVolume* Volume = TaggedVolumes[0];
			const FPostProcessSettings& PP = Volume->Settings;
			AddAuditItem(Report, TEXT("TerritoryPostProcess"),
				Volume->bEnabled && Volume->bUnbound && Volume->BlendWeight > 0.99f
					? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Error,
				FText::Format(LOCTEXT("TerritoryPPFinding",
					"Exactly one tagged Territory post process exists: {0}."),
					FText::FromString(Volume->GetActorLabel())),
				LOCTEXT("TerritoryPPRecommendation",
					"Enable the tagged volume, make it unbound, and keep blend weight at 1.0."));
			const bool bVolumeUsesLumen =
				PP.bOverride_DynamicGlobalIlluminationMethod
				&& PP.DynamicGlobalIlluminationMethod ==
					EDynamicGlobalIlluminationMethod::Lumen
				&& PP.bOverride_ReflectionMethod
				&& PP.ReflectionMethod == EReflectionMethod::Lumen;
			AddAuditItem(Report, TEXT("PostProcessLumen"),
				bVolumeUsesLumen ? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Error,
				bVolumeUsesLumen
					? LOCTEXT("PostProcessLumenPass",
						"The Territory post process explicitly selects Lumen GI and reflections.")
					: LOCTEXT("PostProcessLumenFail",
						"The Territory post process does not explicitly select both Lumen GI and Lumen reflections."),
				LOCTEXT("PostProcessLumenFix",
					"Run Create Or Update Territory AAA HDR Scene again with post-process setup enabled."));
			const bool bLookWithinSafeRange = PP.BloomIntensity <= 0.35f
				&& PP.VignetteIntensity <= 0.3f && PP.MotionBlurAmount <= 0.5f
				&& PP.FilmGrainIntensity <= 0.1f && PP.SceneFringeIntensity <= 1.f;
			AddAuditItem(Report, TEXT("RestrainedLensLook"),
				bLookWithinSafeRange ? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Warning,
				bLookWithinSafeRange
					? LOCTEXT("RestrainedLensPass",
						"Bloom, vignette, motion blur, grain, and chromatic aberration remain inside the recommended cinematic range.")
					: LOCTEXT("RestrainedLensFail",
						"One or more global lens effects exceed the recommended cinematic range."),
				LOCTEXT("RestrainedLensFix",
					"Reduce the global effect and use shot-specific camera or post-process tracks for deliberate stylization."));
			const auto IsUniformGradeVector = [](const FVector4& Value)
			{
				return FMath::IsNearlyEqual(Value.X, Value.Y)
					&& FMath::IsNearlyEqual(Value.X, Value.Z)
					&& FMath::IsNearlyEqual(Value.X, Value.W);
			};
			const bool bGlobalGradeDisabled = !PP.bOverride_ColorSaturation
				&& !PP.bOverride_ColorContrast;
			const bool bGlobalGradeUniform =
				PP.bOverride_ColorSaturation == PP.bOverride_ColorContrast
				&& IsUniformGradeVector(PP.ColorSaturation)
				&& IsUniformGradeVector(PP.ColorContrast);
			AddAuditItem(Report, TEXT("GlobalColorGrade"),
				bGlobalGradeDisabled || bGlobalGradeUniform
					? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Warning,
				bGlobalGradeDisabled
					? LOCTEXT("GlobalGradeDisabled",
						"Level-wide saturation and contrast overrides are disabled; Narrative UDS and shot grades retain color authority.")
					: (bGlobalGradeUniform
						? LOCTEXT("GlobalGradeUniform",
							"The explicitly enabled global saturation and contrast grade uses equal RGBA channels.")
						: LOCTEXT("GlobalGradeSkewed",
							"The global saturation or contrast grade has unequal color channels or only one override enabled, which can tint the level green/red.")),
				LOCTEXT("GlobalGradeFix",
					"Run the scene maker with Apply Global Color Grading disabled, or set every RGBA saturation and contrast channel to the same reviewed value."));
		}
		else
		{
			AddAuditItem(Report, TEXT("TerritoryPostProcess"),
				ETerritoryHDRSceneAuditSeverity::Error,
				FText::Format(LOCTEXT("TaggedPPCount",
					"Found {0} Post Process Volumes tagged Territory.AAA.PostProcess; exactly one is required."),
					FText::AsNumber(TaggedVolumes.Num())),
				LOCTEXT("TaggedPPCountFix",
					"Keep one reviewed Territory volume. Remove the tag from intentional alternatives instead of allowing ambiguous ownership."));
		}

		if (Options.bWarnAboutConflictingUnboundPostProcessVolumes
			&& !ConflictingVolumeNames.IsEmpty())
		{
			AddAuditItem(Report, TEXT("UnboundPostProcessConflicts"),
				ETerritoryHDRSceneAuditSeverity::Warning,
				FText::Format(LOCTEXT("UnboundPPConflictFinding",
					"Found {0} other enabled unbound Post Process Volume(s): {1}."),
					FText::AsNumber(ConflictingVolumeNames.Num()),
					FText::FromString(FString::Join(ConflictingVolumeNames, TEXT(", ")))),
				LOCTEXT("UnboundPPConflictFix",
					"Review their priority and blend settings. The audit never disables or edits artist-owned volumes."));
		}
		else
		{
			AddAuditItem(Report, TEXT("UnboundPostProcessConflicts"),
				ETerritoryHDRSceneAuditSeverity::Pass,
				LOCTEXT("NoUnboundPPConflicts",
					"No additional enabled unbound Post Process Volume is competing with the Territory look."));
		}

		if (Options.bEnsureNarrativeUltraDynamicSky)
		{
			UClass* SkyClass = LoadNarrativeSkyClass(Options);
			int32 SkyCount = 0;
			if (SkyClass && SkyClass->IsChildOf(AActor::StaticClass()))
			{
				for (TActorIterator<AActor> It(World, SkyClass); It; ++It)
				{
					if (!Report.NarrativeUltraDynamicSkyActor)
					{
						Report.NarrativeUltraDynamicSkyActor = *It;
					}
					++SkyCount;
				}
			}
			const bool bExactlyOneSky = SkyCount == 1;
			AddAuditItem(Report, TEXT("NarrativeUltraDynamicSky"),
				bExactlyOneSky ? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Error,
				FText::Format(LOCTEXT("NarrativeUDSCount",
					"Resolved {0} actor(s) of the configured Narrative Ultra Dynamic Sky class."),
					FText::AsNumber(SkyCount)),
				LOCTEXT("NarrativeUDSCountFix",
					"Enable the Narrative UDS integration and keep exactly one Narrative_UDS_Sky authority in the persistent level."));
			if (bExactlyOneSky)
			{
				const AActor* Sky = Report.NarrativeUltraDynamicSkyActor;
				const bool bSkyExposure = SkyOwnsExposure(Sky);
				const FPostProcessSettings* PP = Report.PostProcessVolume ? &Report.PostProcessVolume->Settings : nullptr;
				const bool bExposureConflict = bSkyExposure && PP &&
					(PP->bOverride_AutoExposureMethod || PP->bOverride_AutoExposureMinBrightness
					|| PP->bOverride_AutoExposureMaxBrightness || PP->bOverride_AutoExposureBias
					|| PP->bOverride_AutoExposureBiasCurve);
				AddAuditItem(Report, TEXT("UDSExposureOwnership"),
					bExposureConflict ? ETerritoryHDRSceneAuditSeverity::Error : ETerritoryHDRSceneAuditSeverity::Pass,
					FText::FromString(bExposureConflict ? TEXT("Territory post process overrides exposure fields owned by UDS.")
						: (bSkyExposure ? TEXT("UDS owns metering, EV100 range, exposure bias and its day/night curve.")
							: TEXT("UDS exposure is disabled; the scene post process owns exposure."))),
					LOCTEXT("ExposureOwnershipFix", "Run scene setup to clear competing Territory exposure overrides."));
				bool bMovableLights = true;
				for (FName Name : {FName(TEXT("Sun Mobility")), FName(TEXT("Moon Mobility")), FName(TEXT("Sky Light Mobility"))})
				{
					const FByteProperty* Property = FindFProperty<FByteProperty>(Sky->GetClass(), Name);
					bMovableLights &= Property && Property->GetPropertyValue_InContainer(Sky) == EComponentMobility::Movable;
				}
				AddAuditItem(Report, TEXT("UDSDynamicLights"),
					bMovableLights ? ETerritoryHDRSceneAuditSeverity::Pass : ETerritoryHDRSceneAuditSeverity::Error,
					FText::FromString(bMovableLights ? TEXT("UDS sun, moon and skylight support dynamic day/night lighting.")
						: TEXT("A UDS light is not movable or its mobility contract is missing.")),
					LOCTEXT("UDSMobilityFix", "Configure mobility through the UDS actor controls."));
				AddAuditItem(Report, TEXT("InteriorVisualReview"), ETerritoryHDRSceneAuditSeverity::Advisory,
					LOCTEXT("InteriorVisualReviewFinding", "Interior adaptation uses UDS occlusion and Lumen sky shadowing. Settings alone cannot verify room lighting."),
					LOCTEXT("InteriorVisualReviewFix", "Review doors, windows and enclosed rooms at noon, dusk and night. Use UDS Occlusion Volumes for collision gaps and authored local lights where rooms need illumination."));
			}
		}
		else
		{
			AddAuditItem(Report, TEXT("NarrativeUltraDynamicSky"),
				ETerritoryHDRSceneAuditSeverity::Advisory,
				LOCTEXT("NarrativeUDSSkipped",
					"Narrative Ultra Dynamic Sky verification was disabled by the audit options."),
				LOCTEXT("NarrativeUDSSkippedAdvice",
					"Enable it when Narrative Game State should remain the authoritative time-of-day source."));
		}

		AddRequiredConsoleCheck(Report, TEXT("ProjectLumenGI"),
			TEXT("r.DynamicGlobalIlluminationMethod"), 1,
			ETerritoryHDRSceneAuditSeverity::Error,
			LOCTEXT("ProjectLumenGIPass", "Project default global illumination is Lumen."),
			LOCTEXT("ProjectLumenGIFix", "Enable Lumen Global Illumination in Project Settings > Rendering and restart the editor."));
		AddRequiredConsoleCheck(Report, TEXT("ProjectLumenReflections"),
			TEXT("r.ReflectionMethod"), 1,
			ETerritoryHDRSceneAuditSeverity::Error,
			LOCTEXT("ProjectLumenReflectionsPass", "Project default reflections use Lumen."),
			LOCTEXT("ProjectLumenReflectionsFix", "Enable Lumen Reflections in Project Settings > Rendering and restart the editor."));
		AddRequiredConsoleCheck(Report, TEXT("MeshDistanceFields"),
			TEXT("r.GenerateMeshDistanceFields"), 1,
			ETerritoryHDRSceneAuditSeverity::Warning,
			LOCTEXT("MeshDistanceFieldsPass", "Mesh Distance Fields are enabled for software Lumen coverage."),
			LOCTEXT("MeshDistanceFieldsFix", "Enable Generate Mesh Distance Fields and restart before validating software Lumen."));
		AddRequiredConsoleCheck(Report, TEXT("VirtualShadowMaps"),
			TEXT("r.Shadow.Virtual.Enable"), 1,
			ETerritoryHDRSceneAuditSeverity::Warning,
			LOCTEXT("VirtualShadowMapsPass", "Virtual Shadow Maps are enabled."),
			LOCTEXT("VirtualShadowMapsFix", "Enable Virtual Shadow Maps or document the platform-specific shadow alternative."));
		AddRequiredConsoleCheck(Report, TEXT("ExtendedEV100"),
			TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"), 1,
			ETerritoryHDRSceneAuditSeverity::Error,
			LOCTEXT("ExtendedEV100Pass", "Extended EV100 luminance range is enabled for physical lighting."),
			LOCTEXT("ExtendedEV100Fix", "Enable Extend default luminance range in Auto Exposure settings; otherwise the authored EV100 range is not meaningful."));
		AddRequiredConsoleCheck(Report, TEXT("TemporalSuperResolution"),
			TEXT("r.AntiAliasingMethod"), 4,
			ETerritoryHDRSceneAuditSeverity::Warning,
			LOCTEXT("TSRPass", "Temporal Super Resolution is the project anti-aliasing method."),
			LOCTEXT("TSRFix", "Use TSR for the target profile or validate Groom, motion, and Lumen stability with the selected alternative."));
		AddRequiredConsoleCheck(Report, TEXT("NaniteProjectSupport"),
			TEXT("r.Nanite.ProjectEnabled"), 1,
			ETerritoryHDRSceneAuditSeverity::Warning,
			LOCTEXT("NanitePass", "Nanite project support is enabled."),
			LOCTEXT("NaniteFix", "Enable Nanite support or provide platform LOD/HLOD coverage for the environment."));

		const int32 TexturePoolMB = ReadConsoleInt(TEXT("r.Streaming.PoolSize"));
		AddAuditItem(Report, TEXT("TextureStreamingPool"),
			TexturePoolMB >= 1024 ? ETerritoryHDRSceneAuditSeverity::Pass
				: ETerritoryHDRSceneAuditSeverity::Warning,
			FText::Format(LOCTEXT("TexturePoolFinding",
				"Texture streaming pool is {0} MiB."), FText::AsNumber(TexturePoolMB)),
			LOCTEXT("TexturePoolAdvice",
				"Set this from measured target-platform VRAM; do not treat the editor value as a shipping budget."));

		const bool bHardwareRayTracing = ReadConsoleInt(TEXT("r.RayTracing"), 0) > 0;
		if (bHardwareRayTracing)
		{
			Report.RayTracingGeometryPoolMB = ReadConsoleFloat(
				TEXT("r.RayTracing.ResidentGeometryMemoryPoolSizeInMB"), 0.f);
			const bool bPoolMeetsThreshold = Report.RayTracingGeometryPoolMB +
				KINDA_SMALL_NUMBER >= Options.MinimumRayTracingGeometryPoolMB;
			AddAuditItem(Report, TEXT("RayTracingGeometryPool"),
				bPoolMeetsThreshold ? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Warning,
				FText::Format(LOCTEXT("RayTracingPoolFinding",
					"Hardware ray tracing is enabled with a {0} MiB resident geometry pool; the audit threshold is {1} MiB."),
					FText::AsNumber(Report.RayTracingGeometryPoolMB),
					FText::AsNumber(Options.MinimumRayTracingGeometryPoolMB)),
				LOCTEXT("RayTracingPoolAdvice",
					"The yellow REQUESTED MEMORY OVER BUDGET message means live geometry exceeded this pool. Profile Nanite fallback meshes, skeletal meshes, Groom, LODs, and target VRAM before raising r.RayTracing.ResidentGeometryMemoryPoolSizeInMB."));
		}
		else
		{
			AddAuditItem(Report, TEXT("RayTracingGeometryPool"),
				ETerritoryHDRSceneAuditSeverity::Advisory,
				LOCTEXT("SoftwareLumenRayTracing",
					"Hardware ray tracing is disabled; the scene will use the configured software Lumen path."),
				LOCTEXT("SoftwareLumenRayTracingAdvice",
					"This is valid for scalable gameplay. Validate the intended platform path rather than enabling hardware ray tracing only for the editor."));
		}

		if (Options.bAnalyzeLoadedSceneMemory && Report.AnalyzedAssetCount > 0)
		{
			const bool bWithinBudget = Report.EstimatedLoadedSceneMB <=
				FMath::Max(1.f, Options.LoadedSceneMemoryBudgetMB);
			AddAuditItem(Report, TEXT("LoadedSceneResourceEstimate"),
				bWithinBudget ? ETerritoryHDRSceneAuditSeverity::Pass
					: ETerritoryHDRSceneAuditSeverity::Warning,
				FText::Format(LOCTEXT("LoadedSceneAuditFinding",
					"Loaded-scene resource estimate is {0} MiB against a {1} MiB editor threshold."),
					FText::AsNumber(Report.EstimatedLoadedSceneMB),
					FText::AsNumber(Options.LoadedSceneMemoryBudgetMB)),
				LOCTEXT("LoadedSceneAuditAdvice",
					"Review Top Memory Assets, then confirm with a cooked target-platform Insights memory trace."));
		}

		Report.bReadyForAAACinematic = Report.ErrorChecks == 0
			&& Report.WarningChecks == 0;
	}

	void PublishReport(const TCHAR* Operation,
		const FTerritoryHDRSceneBuildReport& Report)
	{
		for (const FTerritoryHDRSceneAuditItem& Item : Report.AuditItems)
		{
			const TCHAR* Severity = TEXT("PASS");
			ELogVerbosity::Type Verbosity = ELogVerbosity::Display;
			switch (Item.Severity)
			{
			case ETerritoryHDRSceneAuditSeverity::Advisory:
				Severity = TEXT("ADVISORY");
				break;
			case ETerritoryHDRSceneAuditSeverity::Warning:
				Severity = TEXT("WARNING");
				Verbosity = ELogVerbosity::Warning;
				break;
			case ETerritoryHDRSceneAuditSeverity::Error:
				Severity = TEXT("ERROR");
				Verbosity = ELogVerbosity::Error;
				break;
			default:
				break;
			}
			FMsg::Logf(__FILE__, __LINE__, LogTemp.GetCategoryName(), Verbosity,
				TEXT("[TerritoryHDRScene][%s][%s][%s] %s%s%s"),
				Operation, *Item.CheckID.ToString(), Severity,
				*Item.Finding.ToString(),
				Item.Recommendation.IsEmpty() ? TEXT("") : TEXT(" Recommendation: "),
				*Item.Recommendation.ToString());
		}
		if (IsRunningCommandlet()) return;

		const FText Summary = FText::Format(LOCTEXT("HDRSceneNotification",
			"Territory HDR {0}: {1} pass, {2} advisory, {3} warning, {4} error. AAA ready: {5}"),
			FText::FromString(Operation), FText::AsNumber(Report.PassedChecks),
			FText::AsNumber(Report.AdvisoryChecks),
			FText::AsNumber(Report.WarningChecks), FText::AsNumber(Report.ErrorChecks),
			Report.bReadyForAAACinematic ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No"));
		FNotificationInfo Info(Summary);
		Info.bFireAndForget = true;
		Info.FadeOutDuration = 0.5f;
		Info.ExpireDuration = Report.bReadyForAAACinematic ? 6.f : 10.f;
		if (TSharedPtr<SNotificationItem> Notification =
			FSlateNotificationManager::Get().AddNotification(Info))
		{
			Notification->SetCompletionState(Report.ErrorChecks > 0
				? SNotificationItem::CS_Fail
				: (Report.WarningChecks > 0 ? SNotificationItem::CS_Pending
					: SNotificationItem::CS_Success));
		}
	}
}

FTerritoryHDRSceneOptions::FTerritoryHDRSceneOptions()
	: NarrativeUltraDynamicSkyClass(FSoftObjectPath(
		TEXT("/NP_UltraDynamicSky/Narrative_UDS_Sky.Narrative_UDS_Sky_C")))
{
}

FTerritoryHDRSceneBuildReport
UTerritoryHDRSceneEditorLibrary::CreateOrUpdateAAAHDRScene(
	const FTerritoryHDRSceneOptions& Options)
{
	using namespace TerritoryHDRSceneEditor;
	FTerritoryHDRSceneBuildReport Report;
	FText WorldError;
	UWorld* World = GetEditorWorld(WorldError);
	if (!World)
	{
		Report.Errors.Add(WorldError);
		return Report;
	}

	const float Values[] = {Options.SunLightIntensity, Options.MoonLightIntensity,
		Options.SkyLightIntensity, Options.BaseFogDensity, Options.InteriorFogMultiplier,
		Options.InteriorExposureBias, Options.ExposureCompensation, Options.MinimumEV100,
		Options.MaximumEV100, Options.BloomIntensity, Options.VignetteIntensity,
		Options.MotionBlurAmount, Options.WhiteBalanceTemperature, Options.GlobalSaturation,
		Options.GlobalContrast, Options.FilmGrainIntensity, Options.ChromaticAberrationIntensity};
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			Report.Errors.Add(LOCTEXT("FiniteLightingOptionsRequired", "Lighting options must contain finite numbers. No scene actors were changed."));
			return Report;
		}
	}
	// Refuse ambiguous existing authorities before changing either actor.
	UClass* ExistingSkyClass = LoadNarrativeSkyClass(Options);
	if (Options.bEnsureNarrativeUltraDynamicSky && ExistingSkyClass
		&& ExistingSkyClass->IsChildOf(AActor::StaticClass())
		&& !ConfigureSky(Cast<AActor>(ExistingSkyClass->GetDefaultObject()), Options, Report, false))
	{
		return Report;
	}
	int32 ExistingSkyCount = 0;
	int32 ExistingPostProcessCount = 0;
	if (ExistingSkyClass && ExistingSkyClass->IsChildOf(AActor::StaticClass()))
	{
		for (TActorIterator<AActor> It(World, ExistingSkyClass); It; ++It)
		{
			Report.NarrativeUltraDynamicSkyActor = *It;
			++ExistingSkyCount;
		}
	}
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->Tags.Contains(PostProcessTag)) ++ExistingPostProcessCount;
	}
	if (ExistingSkyCount > 1 || ExistingPostProcessCount > 1)
	{
		Report.Errors.Add(LOCTEXT("AmbiguousEnvironmentAuthorities", "Multiple configured skies or tagged Territory post processes exist. Resolve the duplicate authorities before applying a preset."));
		PublishReport(TEXT("Build"), Report);
		return Report;
	}
	const FScopedTransaction Transaction(LOCTEXT("CreateAAAHDRScene",
		"Create Or Update Territory AAA HDR Scene"));
	if (Options.bEnsureNarrativeUltraDynamicSky)
	{
		UClass* SkyClass = LoadNarrativeSkyClass(Options);
		if (!SkyClass || !SkyClass->IsChildOf(AActor::StaticClass()))
		{
			Report.Errors.Add(LOCTEXT("NarrativeUDSUnavailable",
				"Narrative UDS class could not load. Enable NP_UltraDynamicSky and its Ultra Dynamic Sky dependency, or choose the Narrative_UDS_Sky class."));
			PublishReport(TEXT("Build"), Report);
			return Report;
		}
		else
		{
			for (TActorIterator<AActor> It(World, SkyClass); It; ++It)
			{
				Report.NarrativeUltraDynamicSkyActor = *It;
				break;
			}
			if (!Report.NarrativeUltraDynamicSkyActor)
			{
				FActorSpawnParameters Params;
				Params.OverrideLevel = World->PersistentLevel;
				Params.ObjectFlags = RF_Transactional;
				Report.NarrativeUltraDynamicSkyActor = World->SpawnActor<AActor>(
					SkyClass, FTransform::Identity, Params);
			}
			if (Report.NarrativeUltraDynamicSkyActor)
			{
				if (!ConfigureSky(Report.NarrativeUltraDynamicSkyActor, Options, Report))
				{
					PublishReport(TEXT("Build"), Report);
					return Report;
				}
				Report.NarrativeUltraDynamicSkyActor->Modify();
				Report.NarrativeUltraDynamicSkyActor->Tags.AddUnique(NarrativeUDSTag);
				Report.NarrativeUltraDynamicSkyActor->SetActorLabel(
					TEXT("Narrative Ultra Dynamic Sky - Territory AAA"), true);
				Report.NarrativeUltraDynamicSkyActor->MarkPackageDirty();
			}
			else
			{
				Report.Errors.Add(LOCTEXT("NarrativeUDSSpawnFailed",
					"The selected Narrative UDS class loaded but could not be spawned in the persistent level."));
			}
		}
	}

	if (Options.bCreateOrUpdatePostProcessVolume)
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->Tags.Contains(PostProcessTag))
			{
				Report.PostProcessVolume = *It;
				break;
			}
		}
		if (!Report.PostProcessVolume)
		{
			FActorSpawnParameters Params;
			Params.OverrideLevel = World->PersistentLevel;
			Params.ObjectFlags = RF_Transactional;
			Report.PostProcessVolume = World->SpawnActor<APostProcessVolume>(
				APostProcessVolume::StaticClass(), FTransform::Identity, Params);
		}
		if (Report.PostProcessVolume)
		{
			Report.PostProcessVolume->Tags.AddUnique(PostProcessTag);
			Report.PostProcessVolume->SetActorLabel(
				TEXT("Territory AAA HDR - Lumen Post Process"), true);
			ConfigurePostProcess(Report.PostProcessVolume, Options,
				SkyOwnsExposure(Report.NarrativeUltraDynamicSkyActor));
		}
		else
		{
			Report.Errors.Add(LOCTEXT("PostProcessSpawnFailed",
				"Could not create the Territory AAA HDR Post Process Volume."));
		}
	}

	if (Options.bAnalyzeLoadedSceneMemory)
	{
		FillMemoryReport(World, Options.LoadedSceneMemoryBudgetMB,
			Options.TopMemoryItems, Report);
	}
	if (Options.bRunSceneReadinessAudit)
	{
		RunSceneReadinessAudit(World, Options, Report);
	}
	else
	{
		Report.bReadyForAAACinematic = false;
	}
	Report.bSucceeded = Report.Errors.IsEmpty();
	PublishReport(TEXT("Build"), Report);
	return Report;
}

FTerritoryHDRSceneBuildReport
UTerritoryHDRSceneEditorLibrary::AnalyzeLoadedSceneMemory(float MemoryBudgetMB,
	int32 TopItemCount)
{
	using namespace TerritoryHDRSceneEditor;
	FTerritoryHDRSceneBuildReport Report;
	FText WorldError;
	if (UWorld* World = GetEditorWorld(WorldError))
	{
		FillMemoryReport(World, MemoryBudgetMB, TopItemCount, Report);
		Report.bSucceeded = true;
	}
	else
	{
		Report.Errors.Add(WorldError);
	}
	return Report;
}

FTerritoryHDRSceneBuildReport
UTerritoryHDRSceneEditorLibrary::AuditAAAHDRScene(
	const FTerritoryHDRSceneOptions& Options)
{
	using namespace TerritoryHDRSceneEditor;
	FTerritoryHDRSceneBuildReport Report;
	FText WorldError;
	UWorld* World = GetEditorWorld(WorldError);
	if (!World)
	{
		Report.Errors.Add(WorldError);
		return Report;
	}
	if (Options.bAnalyzeLoadedSceneMemory)
	{
		FillMemoryReport(World, Options.LoadedSceneMemoryBudgetMB,
			Options.TopMemoryItems, Report);
	}
	RunSceneReadinessAudit(World, Options, Report);
	Report.bSucceeded = Report.Errors.IsEmpty();
	PublishReport(TEXT("Audit"), Report);
	return Report;
}

#undef LOCTEXT_NAMESPACE
