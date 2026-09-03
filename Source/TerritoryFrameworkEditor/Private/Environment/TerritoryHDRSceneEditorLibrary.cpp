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
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "TerritoryHDRSceneEditorLibrary"

namespace TerritoryHDRSceneEditor
{
	const FName PostProcessTag(TEXT("Territory.AAA.PostProcess"));
	const FName NarrativeUDSTag(TEXT("Territory.AAA.NarrativeUDS"));

	UWorld* GetEditorWorld(FText& OutError)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World || World->IsGameWorld())
		{
			OutError = LOCTEXT("EditorWorldRequired",
				"Open a level in the editor and stop PIE before running the AAA HDR Scene Maker.");
			return nullptr;
		}
		return World;
	}

	void ConfigurePostProcess(APostProcessVolume* Volume,
		const FTerritoryHDRSceneOptions& Options)
	{
		if (!Volume) return;
		Volume->Modify();
		Volume->bEnabled = true;
		Volume->bUnbound = true;
		Volume->BlendWeight = 1.f;
		Volume->Priority = 90.f;

		FPostProcessSettings& PP = Volume->Settings;
		PP.bOverride_DynamicGlobalIlluminationMethod = true;
		PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		PP.bOverride_ReflectionMethod = true;
		PP.ReflectionMethod = EReflectionMethod::Lumen;
		PP.bOverride_AutoExposureMethod = true;
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = FMath::Min(Options.MinimumEV100, Options.MaximumEV100);
		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = FMath::Max(Options.MinimumEV100, Options.MaximumEV100);
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = Options.ExposureCompensation;
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
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(FMath::Clamp(Options.GlobalSaturation, 0.f, 2.f));
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(FMath::Clamp(Options.GlobalContrast, 0.5f, 2.f));
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
			UClass* SkyClass = Options.NarrativeUltraDynamicSkyClass.LoadSynchronous();
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

	const FScopedTransaction Transaction(LOCTEXT("CreateAAAHDRScene",
		"Create Or Update Territory AAA HDR Scene"));
	if (Options.bEnsureNarrativeUltraDynamicSky)
	{
		UClass* SkyClass = Options.NarrativeUltraDynamicSkyClass.LoadSynchronous();
		if (!SkyClass || !SkyClass->IsChildOf(AActor::StaticClass()))
		{
			Report.Warnings.Add(LOCTEXT("NarrativeUDSUnavailable",
				"Narrative UDS class could not load. Enable NP_UltraDynamicSky and its Ultra Dynamic Sky dependency, or choose the Narrative_UDS_Sky class. Lumen setup and memory analysis still ran."));
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
			ConfigurePostProcess(Report.PostProcessVolume, Options);
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
