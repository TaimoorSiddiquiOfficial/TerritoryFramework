#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeDialogueSequence.h"
#include "TerritoryDialogueShot.generated.h"

/** Editorial intent used to organize a studio dialogue-shot library. */
UENUM(BlueprintType)
enum class ETerritoryDialogueShotRole : uint8
{
	Establishing,
	TwoShot,
	OverShoulder,
	MediumCloseUp,
	CloseUp,
	Reaction,
	Insert
};

/** Camera-local look used only while a Territory Narrative shot is active. */
UENUM(BlueprintType)
enum class ETerritoryCinematicStudioLook : uint8
{
	Neutral UMETA(DisplayName="Neutral Cinematic",
		ToolTip="Neutral camera grade for general dialogue. Narrative UDS keeps lighting authority."),
	WarmDustyDay UMETA(DisplayName="Warm Dusty Day",
		ToolTip="Restrained warm daylight treatment for exterior Territory gameplay-to-dialogue cuts."),
	MoonlitBlueNight UMETA(DisplayName="Moonlit Blue Night",
		ToolTip="Cool, low-key night treatment with protected highlights and readable silhouettes."),
	Custom UMETA(DisplayName="Custom Studio Settings",
		ToolTip="Use the editable Custom Studio Settings values on this shot.")
};

/**
 * Scene-referred camera grade for a cinematic shot. Values are intentionally
 * conservative and uniform across RGBA channels so a shot cannot introduce the
 * red/green level tint caused by a malformed global color vector.
 */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryCinematicStudioSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|White Balance",
		meta=(ToolTip="Override white balance on this Cine Camera only. Leave disabled for a neutral shot controlled entirely by Narrative Ultra Dynamic Sky."))
	bool bOverrideWhiteBalance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|White Balance",
		meta=(EditCondition="bOverrideWhiteBalance", ClampMin="1500.0", ClampMax="15000.0", Units="K",
			ToolTip="Camera white balance in Kelvin. Warm Dusty Day uses 6000 K; Moonlit Blue Night uses 4300 K."))
	float WhiteBalanceTemperature = 6500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Exposure",
		meta=(ClampMin="-2.0", ClampMax="2.0", ToolTip="Shot-local exposure compensation in stops. This layers over the gameplay exposure authority only while the Cine Camera is active."))
	float ExposureCompensation = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Color",
		meta=(ClampMin="0.75", ClampMax="1.25", ToolTip="Uniform RGBA saturation multiplier. Keep close to 1.0; never enter different per-channel values for this studio control."))
	float Saturation = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Color",
		meta=(ClampMin="0.75", ClampMax="1.25", ToolTip="Uniform RGBA contrast multiplier. Keep close to 1.0 and preserve MetaHuman skin and Groom detail."))
	float Contrast = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Local Exposure",
		meta=(ClampMin="0.6", ClampMax="1.0", ToolTip="Local-exposure highlight contrast. Epic's recommended working range is 0.6 to 1.0."))
	float LocalExposureHighlightContrast = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Local Exposure",
		meta=(ClampMin="0.6", ClampMax="1.0", ToolTip="Local-exposure shadow contrast. Higher values preserve deeper night silhouettes."))
	float LocalExposureShadowContrast = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Local Exposure",
		meta=(ClampMin="0.0", ClampMax="4.0", ToolTip="Local-exposure detail strength. 1.0 is the neutral recommended value."))
	float LocalExposureDetailStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Local Exposure",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Blend between bilateral and blurred luminance. 0.5 is a stable midpoint for Lumen scenes."))
	float LocalExposureBlurredLuminanceBlend = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Lens",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Camera-local bloom. Keep below 0.25 for the supplied looks."))
	float BloomIntensity = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Lens",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Camera-local vignette. Use restrained values so gameplay-to-dialogue cuts do not look filtered."))
	float VignetteIntensity = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Lens",
		meta=(ClampMin="0.0", ClampMax="0.1", ToolTip="Subtle camera-local film grain. Set to zero for clean Groom and facial-quality review."))
	float FilmGrainIntensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Studio|Lens",
		meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Camera-local motion blur. 0.25 is a restrained cinematic preview; Movie Render Queue temporal sampling owns final offline blur quality."))
	float MotionBlurAmount = 0.25f;
};

/**
 * Narrative Pro dialogue shot with stable cinematic defaults and a lens/focus pass.
 * The authored Level Sequence still owns composition and camera movement; use a
 * binding tagged "Cinecam" so Narrative and this class can configure its camera.
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories=("Territory Cinematic Look"))
class TERRITORYFRAMEWORK_API UTerritoryDialogueShot
	: public UNarrativeDialogueSequence
{
	GENERATED_BODY()

public:
	UTerritoryDialogueShot();

	virtual void BeginPlaySequence(ALevelSequenceActor* InSequenceActor,
		UDialogue* InDialogue, AActor* InSpeaker, AActor* InListener) override;

	UFUNCTION(BlueprintPure, Category="Territory|Cinematics")
	ETerritoryDialogueShotRole GetShotRole() const { return ShotRole; }

	UFUNCTION(BlueprintPure, Category="Territory|Cinematics")
	float GetCinematicAspectRatio() const { return CropSettings.AspectRatio; }

	/** Resolve the selected built-in look or the editable custom values. */
	UFUNCTION(BlueprintPure, Category="Territory|Cinematics|Studio")
	FTerritoryCinematicStudioSettings GetResolvedStudioSettings() const;

	/** Apply the camera-local grade without changing the gameplay Post Process Volume. */
	UFUNCTION(BlueprintCallable, Category="Territory|Cinematics|Studio")
	void ApplyStudioLookToCamera(class UCineCameraComponent* CameraComponent) const;

#if WITH_EDITOR
	/** Configure an inline Narrative shot produced by the Territory editor shot-pack builder. */
	void ConfigureEditorShot(class ULevelSequence* Sequence, const FText& DisplayName,
		ETerritoryDialogueShotRole InRole, EAnchorOriginRule InAnchorOrigin,
		EShotTrackingRule InTrackingRule, float InFocalLength, float InAperture,
		bool bInUse180DegreeRule = true);
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look")
	ETerritoryDialogueShotRole ShotRole = ETerritoryDialogueShotRole::MediumCloseUp;

	/** Override the focal length authored on the Cinecam binding at shot start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look")
	bool bApplyLensOverride = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look",
		meta=(EditCondition="bApplyLensOverride", ClampMin="12.0", ClampMax="250.0", Units="mm"))
	float FocalLength = 65.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look",
		meta=(EditCondition="bApplyLensOverride", ClampMin="1.0", ClampMax="22.0"))
	float Aperture = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look|Focus")
	bool bSmoothTrackingFocus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look|Focus",
		meta=(EditCondition="bSmoothTrackingFocus", ClampMin="0.1", ClampMax="30.0"))
	float FocusSmoothingSpeed = 8.f;

	/** Apply a camera-local studio profile while this Narrative shot is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look|Studio",
		meta=(ToolTip="Keeps gameplay on the neutral global HDR baseline and applies this look only to the spawned Narrative Cine Camera."))
	bool bApplyCinematicStudioLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look|Studio",
		meta=(EditCondition="bApplyCinematicStudioLook", ToolTip="Neutral, warm daylight, moonlit night, or custom camera-local look. It never edits the level-wide gameplay grade."))
	ETerritoryCinematicStudioLook StudioLook =
		ETerritoryCinematicStudioLook::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look|Studio",
		meta=(EditCondition="bApplyCinematicStudioLook && StudioLook == ETerritoryCinematicStudioLook::Custom", ShowOnlyInnerProperties,
			ToolTip="Editable camera grade used when Studio Look is Custom."))
	FTerritoryCinematicStudioSettings CustomStudioSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory Cinematic Look|Studio",
		meta=(EditCondition="bApplyCinematicStudioLook", ClampMin="0.0", ClampMax="1.0",
			ToolTip="Blend weight for the Cine Camera post process. 1.0 applies the selected profile fully during the shot."))
	float StudioLookBlendWeight = 1.f;
};
