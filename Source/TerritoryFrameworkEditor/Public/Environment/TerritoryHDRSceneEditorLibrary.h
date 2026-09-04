#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryHDRSceneEditorLibrary.generated.h"

class AActor;
class APostProcessVolume;

UENUM(BlueprintType)
enum class ETerritoryHDRSceneQuality : uint8
{
	Performance UMETA(DisplayName="Performance Preview",
		ToolTip="Lower-cost Lumen settings for editor iteration and mid-range targets."),
	Balanced UMETA(DisplayName="Balanced Gameplay",
		ToolTip="Recommended gameplay preset: stable auto exposure, moderate Lumen detail, and restrained lens effects."),
	AAACinematic UMETA(DisplayName="AAA Cinematic",
		ToolTip="Highest preset for close dialogue and hero shots. Validate GPU time and scene memory before shipping.")
};

UENUM(BlueprintType)
enum class ETerritoryHDRSceneAuditSeverity : uint8
{
	Pass UMETA(DisplayName="Pass", ToolTip="The scene satisfies this readiness check."),
	Advisory UMETA(DisplayName="Advisory", ToolTip="Informational production guidance; no correction is required."),
	Warning UMETA(DisplayName="Warning", ToolTip="The scene can run, but this issue should be reviewed before final quality approval."),
	Error UMETA(DisplayName="Error", ToolTip="A required AAA HDR scene contract is missing or contradictory.")
};

/** Artist-facing controls used by the Territory AAA HDR Scene Maker Editor Utility node. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryHDRSceneOptions
{
	GENERATED_BODY()

	FTerritoryHDRSceneOptions();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="01 Quality",
		meta=(ToolTip="Balanced Gameplay is the best default. Use AAA Cinematic for controlled Sequence shots, not as an unmeasured global scalability replacement."))
	ETerritoryHDRSceneQuality Quality = ETerritoryHDRSceneQuality::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="02 Narrative Ultra Dynamic Sky",
		meta=(ToolTip="When enabled, reuse or spawn Narrative Pro's Ultra Dynamic Sky bridge so Narrative Game State remains the time-of-day authority."))
	bool bEnsureNarrativeUltraDynamicSky = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="02 Narrative Ultra Dynamic Sky",
		meta=(AllowedClasses="/Script/Engine.Actor",
			ToolTip="Narrative-aligned UDS actor class. Recommended: /NP_UltraDynamicSky/Narrative_UDS_Sky. Enable the Narrative Pro - Ultra Dynamic Sky integration and its Ultra Dynamic Sky dependency before running."))
	TSoftClassPtr<AActor> NarrativeUltraDynamicSkyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="03 Post Process",
		meta=(ToolTip="Reuse the actor tagged Territory.AAA.PostProcess. If absent, the tool creates one unbound Post Process Volume in the persistent level."))
	bool bCreateOrUpdatePostProcessVolume = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="03 Post Process",
		meta=(ClampMin="-5.0", ClampMax="5.0", UIMin="-2.0", UIMax="2.0",
			ToolTip="Exposure compensation in stops. Recommended starting point is -0.25 to protect bright skies and skin highlights."))
	float ExposureCompensation = -0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="03 Post Process",
		meta=(ClampMin="-10.0", ClampMax="20.0",
			ToolTip="Dark end of histogram auto exposure in EV100. -4 keeps night readable without flattening it."))
	float MinimumEV100 = -4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="03 Post Process",
		meta=(ClampMin="-10.0", ClampMax="20.0",
			ToolTip="Bright end of histogram auto exposure in EV100. 16 supports daylight and emissive highlights."))
	float MaximumEV100 = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="03 Post Process",
		meta=(ClampMin="0.0", ClampMax="2.0", UIMin="0.0", UIMax="1.0",
			ToolTip="Restrained bloom preserves HDR highlight shape. Recommended 0.15 gameplay, up to 0.25 for cinematic night shots."))
	float BloomIntensity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="03 Post Process",
		meta=(ClampMin="0.0", ClampMax="1.0",
			ToolTip="Subtle edge falloff. Recommended 0.12; values above 0.3 usually look game-filtered rather than cinematic."))
	float VignetteIntensity = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(ClampMin="1500.0", ClampMax="15000.0", UIMin="3200.0", UIMax="9000.0", Units="K",
			ToolTip="Neutral white-balance temperature for the Territory post process. 6500 K is a safe daylight-neutral baseline; use Narrative UDS lighting, not extreme grading, to establish time of day."))
	float WhiteBalanceTemperature = 6500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(ToolTip="Disabled by default so the level-wide HDR setup preserves the authored sky and sunlight colors. Enable only for a deliberately reviewed global grade; prefer Cine Camera or Level Sequence post-process tracks for shot-specific color."))
	bool bApplyGlobalColorGrading = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(EditCondition="bApplyGlobalColorGrading", ClampMin="0.75", ClampMax="1.25", UIMin="0.85", UIMax="1.15",
			ToolTip="Global saturation multiplier used only when Apply Global Color Grading is enabled. Keep at 1.0 for neutral color; prefer shot-specific grading."))
	float GlobalSaturation = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(EditCondition="bApplyGlobalColorGrading", ClampMin="0.75", ClampMax="1.25", UIMin="0.85", UIMax="1.15",
			ToolTip="Global contrast multiplier used only when Apply Global Color Grading is enabled. Keep at 1.0 for neutral color; add contrast per Cine Camera or Level Sequence shot after skin-tone review."))
	float GlobalContrast = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="0.1",
			ToolTip="Subtle film grain. Recommended 0.02 for cinematic shots and 0.0 for clean gameplay or aggressive temporal upscaling."))
	float FilmGrainIntensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(ClampMin="0.0", ClampMax="5.0", UIMin="0.0", UIMax="1.0",
			ToolTip="Chromatic aberration/fringe intensity. Recommended 0.0; increase only for a deliberate damaged-lens or stylized shot."))
	float ChromaticAberrationIntensity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="04 Cinematic Look",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="0.5",
			ToolTip="Camera motion-blur amount. Recommended 0.15 for gameplay and up to 0.25 for controlled 24 fps cinematics. Character and Groom review should also be performed with blur disabled."))
	float MotionBlurAmount = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="05 Memory Analyzer",
		meta=(ToolTip="Run a unique loaded-scene resource estimate after setup. This is an editor estimate, not a replacement for Insights or a cooked platform memory trace."))
	bool bAnalyzeLoadedSceneMemory = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="05 Memory Analyzer",
		meta=(ClampMin="128.0", ClampMax="65536.0", Units="MB",
			ToolTip="Warning threshold for unique assets directly referenced by loaded actors and their immediate asset dependencies. Set this to the platform's measured scene budget."))
	float LoadedSceneMemoryBudgetMB = 4096.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="05 Memory Analyzer",
		meta=(ClampMin="128.0", ClampMax="8192.0", Units="MB",
			ToolTip="Read-only readiness threshold for r.RayTracing.ResidentGeometryMemoryPoolSizeInMB when hardware ray tracing is enabled. Recommended starting audit threshold is 512 MiB for this project. The tool never raises the pool automatically; profile geometry and target VRAM first."))
	float MinimumRayTracingGeometryPoolMB = 512.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="05 Memory Analyzer",
		meta=(ClampMin="1", ClampMax="100",
			ToolTip="Number of largest unique loaded resources returned for quick texture, mesh, groom, material, and cinematic triage."))
	int32 TopMemoryItems = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="06 Final Readiness Audit",
		meta=(ToolTip="Run the structured final scene audit after applying the preset. The audit checks Narrative UDS, tagged and conflicting post processes, Lumen, Virtual Shadow Maps, TSR, extended EV100, Nanite, texture streaming, and hardware ray-tracing budget guidance."))
	bool bRunSceneReadinessAudit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="06 Final Readiness Audit",
		meta=(ToolTip="Report other enabled unbound Post Process Volumes that can blend with or override the Territory look. The tool never deletes, disables, or edits those artist-owned volumes."))
	bool bWarnAboutConflictingUnboundPostProcessVolumes = true;
};

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryHDRSceneAuditItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene Audit",
		meta=(ToolTip="Stable machine-readable identifier for this readiness check."))
	FName CheckID;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene Audit",
		meta=(ToolTip="Pass, advisory, warning, or blocking error result."))
	ETerritoryHDRSceneAuditSeverity Severity = ETerritoryHDRSceneAuditSeverity::Pass;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene Audit",
		meta=(ToolTip="What the audit observed in the current editor world or renderer configuration."))
	FText Finding;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene Audit",
		meta=(ToolTip="Specific next action. Empty when the check passed and no additional guidance is needed."))
	FText Recommendation;
};

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritorySceneMemoryItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Full object path of the unique loaded asset."))
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Asset class, useful for identifying textures, skeletal meshes, groom bindings, materials, Niagara systems, and Level Sequences."))
	FString AssetClass;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Estimated resident resource size in MiB reported by Unreal for this loaded object."))
	float EstimatedMB = 0.f;
};

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryHDRSceneBuildReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="True when requested scene setup operations completed. Warnings may still require artist review."))
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Narrative-aligned Ultra Dynamic Sky actor reused or created by the tool."))
	TObjectPtr<AActor> NarrativeUltraDynamicSkyActor;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Tagged unbound Post Process Volume holding the selected Lumen and restrained lens preset."))
	TObjectPtr<APostProcessVolume> PostProcessVolume;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Unique loaded asset count included in the editor memory estimate."))
	int32 AnalyzedAssetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Sum of Unreal's estimated resident resource sizes for unique assets reached from loaded level actors."))
	float EstimatedLoadedSceneMB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Largest estimated loaded resources, sorted descending."))
	TArray<FTerritorySceneMemoryItem> TopMemoryAssets;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="True when the final structured audit contains no warnings or errors. Advisories do not block readiness."))
	bool bReadyForAAACinematic = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Ordered pass, advisory, warning, and error results from the final scene readiness audit."))
	TArray<FTerritoryHDRSceneAuditItem> AuditItems;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Number of readiness checks that passed."))
	int32 PassedChecks = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Number of informational production advisories."))
	int32 AdvisoryChecks = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Number of non-blocking checks needing review."))
	int32 WarningChecks = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Number of blocking scene-contract errors."))
	int32 ErrorChecks = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Enabled unbound Post Process Volumes other than the tagged Territory volume. These can change the final look through blending and priority."))
	int32 ConflictingUnboundPostProcessVolumes = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Current ray-tracing resident geometry pool in MiB. A viewport over-budget warning means requested geometry exceeded this pool; profile before raising it."))
	float RayTracingGeometryPoolMB = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Blocking setup failures."))
	TArray<FText> Errors;

	UPROPERTY(BlueprintReadOnly, Category="Territory HDR Scene",
		meta=(ToolTip="Actionable quality, plugin, or memory-budget recommendations."))
	TArray<FText> Warnings;
};

/** Editor Utility Blueprint API for Narrative-aligned sky, Lumen look setup, and scene-memory triage. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryHDRSceneEditorLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Territory|Environment|Editor",
		meta=(DisplayName="Create Or Update Territory AAA HDR Scene",
			ToolTip="Runs in the current non-PIE editor world. Reuses or creates Narrative UDS and a tagged unbound Lumen Post Process Volume, then optionally returns a loaded-scene memory estimate. The tool does not alter global project scalability, OS HDR output, or Narrative vendor assets."))
	static FTerritoryHDRSceneBuildReport CreateOrUpdateAAAHDRScene(
		UPARAM(meta=(ToolTip="Artist settings. Balanced Gameplay is the recommended starting preset."))
		const FTerritoryHDRSceneOptions& Options);

	UFUNCTION(BlueprintCallable, Category="Territory|Environment|Editor",
		meta=(DisplayName="Analyze Territory Loaded Scene Memory",
			ToolTip="Read-only estimate for unique resources reachable from actors in the current loaded editor world. Use Unreal Insights and cooked platform traces for final shipping budgets."))
	static FTerritoryHDRSceneBuildReport AnalyzeLoadedSceneMemory(
		UPARAM(meta=(ClampMin="128.0", Units="MB", ToolTip="Platform scene-memory warning threshold in MiB."))
		float MemoryBudgetMB = 4096.f,
		UPARAM(meta=(ClampMin="1", ClampMax="100", ToolTip="Number of largest resources to return."))
		int32 TopItemCount = 20);

	UFUNCTION(BlueprintCallable, Category="Territory|Environment|Editor",
		meta=(DisplayName="Audit Territory AAA HDR Scene",
			ToolTip="Read-only final confirmation for the current editor level. Checks Narrative UDS, post-process ownership/conflicts, Lumen and rendering prerequisites, ray-tracing pool guidance, and optional loaded-scene memory without changing actors, project settings, scalability, or vendor content."))
	static FTerritoryHDRSceneBuildReport AuditAAAHDRScene(
		UPARAM(meta=(ToolTip="Audit thresholds and the expected Narrative UDS class. No setup values are applied by this node."))
		const FTerritoryHDRSceneOptions& Options);
};
