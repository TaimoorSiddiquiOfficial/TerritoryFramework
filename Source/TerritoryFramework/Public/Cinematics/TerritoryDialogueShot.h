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
};
