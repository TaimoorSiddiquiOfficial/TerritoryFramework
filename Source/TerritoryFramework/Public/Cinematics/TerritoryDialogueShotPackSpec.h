#pragma once

#include "Cinematics/TerritoryDialogueShot.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TerritoryDialogueShotPackSpec.generated.h"

class UTerritoryCinematicLightRigProfile;

/** Dialogue slot a studio shot row feeds. Rows left on None are authored per line or by hand. */
UENUM(BlueprintType)
enum class ETerritoryDialogueShotSlot : uint8
{
	None            UMETA(DisplayName="None (line or manual)"),
	Master          UMETA(DisplayName="Master (dialogue default)"),
	SpeakerCoverage UMETA(DisplayName="Speaker coverage"),
	ReplySelection  UMETA(DisplayName="Player reply selection")
};

/** One authored studio shot: what it is, how it is framed, and where the pack installs it. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryStudioShotSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot",
		meta=(ToolTip="Editorial role of this shot. Roles drive the shot guidance and the default dialogue wiring."))
	ETerritoryDialogueShotRole Role = ETerritoryDialogueShotRole::MediumCloseUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot",
		meta=(ToolTip="Optional stable role identity as a gameplay tag. Leave empty to derive it from the role above; set it to name a new editorial role without adding an enum value."))
	FGameplayTag ShotRoleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot",
		meta=(ToolTip="Dialogue slot this shot is installed into. None keeps the shot authored elsewhere, such as a single dialogue line."))
	ETerritoryDialogueShotSlot Slot = ETerritoryDialogueShotSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot",
		meta=(ToolTip="Level Sequence asset name to create or repair inside the pack destination folder. Must be unique within the folder."))
	FString AssetName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot",
		meta=(ToolTip="Designer-facing name shown in the shot pack guidance."))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing",
		meta=(ToolTip="Static CineCamera location on the conversation axis, in centimetres."))
	FVector CameraLocation = FVector(-155.f, 27.f, 5.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Framing",
		meta=(ToolTip="Static CineCamera rotation in degrees."))
	FRotator CameraRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera",
		meta=(ToolTip="CineCamera focal length in millimetres. 35 opens the geography, 85 compresses to a close-up.",
			ClampMin="12.0", ClampMax="300.0"))
	float FocalLength = 65.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera",
		meta=(ToolTip="CineCamera aperture. Smaller values give shallower depth of field for emotional beats.",
			ClampMin="1.0", ClampMax="22.0"))
	float Aperture = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera",
		meta=(ToolTip="Manual focus distance in centimetres from the camera to the subject.", ClampMin="1.0"))
	float ManualFocusDistance = 155.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Optional Lights",
		meta=(ToolTip="Optional extra character lights for this shot. Leave empty for normal scene lighting."))
	TObjectPtr<UTerritoryCinematicLightRigProfile> LightRigProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Optional Lights",
		meta=(EditCondition="LightRigProfile != nullptr", ToolTip="Light the listener instead of the speaker. Only one subject rig is created for this shot."))
	bool bLightRigUsesListener = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Editorial",
		meta=(ToolTip="Artist guidance returned in the shot pack report. Explain the strongest editorial use for this shot."))
	FText BestUse;
};

/**
 * Designer-authored studio dialogue shot pack. The editor shot pack tool reads this asset
 * instead of its built-in seven-shot table, so shots can be added, retuned or removed without code.
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryDialogueShotPackSpec : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot Pack",
		meta=(ToolTip="Ordered shot rows. Each row creates or repairs one 24 fps Narrative-ready Level Sequence with a Cinecam spawnable."))
	TArray<FTerritoryStudioShotSpec> StudioShots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Studio Shot Pack",
		meta=(ToolTip="Playback length in frames at 24 fps for every sequence this pack creates. Zero uses the built-in 720-frame default.",
			ClampMin="0"))
	int32 ShotDurationFrames = 0;
};
