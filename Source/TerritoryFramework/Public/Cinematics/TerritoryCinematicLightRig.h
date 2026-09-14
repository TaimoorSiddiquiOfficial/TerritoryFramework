#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "UObject/Interface.h"
#include "TerritoryCinematicLightRig.generated.h"

class ACameraActor;
class ALevelSequenceActor;
class ANarrativeLevelSequenceActor;
class APlayerController;
class UCameraComponent;
class UDialogue;
class ULevelSequencePlayer;

/** Implement this on a project-owned child of an optional runtime light rig. */
UINTERFACE(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryCinematicLightRig : public UInterface
{
	GENERATED_BODY()
};

class TERRITORYFRAMEWORK_API ITerritoryCinematicLightRig
{
	GENERATED_BODY()
public:
	/** Set the ready visual and camera before construction/BeginPlay. Do not spawn editor tools or global post process. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Territory|Cinematics|Optional Lights")
	bool PrepareLightRig(AActor* Visual, ACameraActor* Camera);
	/** Return true only after BeginPlay created and configured the runtime lights. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Territory|Cinematics|Optional Lights")
	bool IsLightRigReady() const;
	/** Update the existing lights when the camera changes. Do not rebuild the preset or create more lights. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Territory|Cinematics|Optional Lights")
	bool SetLightRigCamera(ACameraActor* Camera);
};

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryLightRigMeshRequirement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Light Rig",
		meta=(ToolTip="Exact skeletal mesh component name on the character visual. For Narrative MetaHumans use Body or FaceMesh."))
	FName ComponentName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Light Rig",
		meta=(ToolTip="Bones or sockets this rig needs. Lights wait until every listed socket exists on this component."))
	TArray<FName> RequiredSockets;
};

/** Optional authored look. The host project owns third-party pack references. Leaving a shot's profile empty disables extra lights. */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryCinematicLightRigProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="01 Light Rig",
		meta=(MustImplement="/Script/TerritoryFramework.TerritoryCinematicLightRig",
		ToolTip="Project runtime rig Blueprint implementing the Territory Cinematic Light Rig interface. Its defaults hold the look made in the optional control panel."))
	TSubclassOf<AActor> RigClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Character",
		meta=(ToolTip="Required component names and sockets. Use the same names as the rig's skeleton configuration. At least one mesh is required."))
	TArray<FTerritoryLightRigMeshRequirement> MeshRequirements;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="02 Character", meta=(ClampMin="0.1", ClampMax="60.0",
		ToolTip="How long to wait for a streamed character visual and its bones. A failed rig is skipped for this subject, with one warning."))
	float ReadyTimeout = 10.f;
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category="03 Optional Editor Tool", meta=(AllowedClasses="/Script/Blutility.EditorUtilityWidgetBlueprint",
		ToolTip="Optional editor control panel for this rig pack. This reference is removed when cooking the game."))
	TSoftObjectPtr<UObject> AuthoringPanel;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="03 Optional Editor Tool",
		meta=(ToolTip="Name of the panel variable holding its preview rig. Used by Use Panel Look in Runtime Rig. Leave empty if the panel does not support copying a preview."))
	FName PreviewRigProperty;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="03 Optional Editor Tool",
		meta=(ToolTip="Only these editable preset properties are copied from the preview to the runtime rig defaults. Do not list character or camera references."))
	TArray<FName> PreviewPropertiesToCopy;
#endif

	bool HasValidConfiguration(FString& Reason) const;
	bool IsVisualReady(AActor* Visual) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/** Local cosmetic lifetime attached to the existing sequence actor. Narrative remains the playback authority. */
UCLASS(BlueprintType, NotBlueprintable, Transient)
class TERRITORYFRAMEWORK_API UTerritoryCinematicLightRigComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTerritoryCinematicLightRigComponent();

	/** Call locally for each intended viewer after creating the Narrative sequence actor. This never starts playback. */
	UFUNCTION(BlueprintCallable, Category="Territory|Cinematics|Optional Lights",
		meta=(ToolTip="Add optional character lights to this Narrative cutscene for an explicit local player and subject. Camera cuts reuse the lights. Held final frames stay lit until playback stops or the camera is released. Dedicated servers and split screen skip lights."))
	static UTerritoryCinematicLightRigComponent* FollowNarrativeSequence(
		ANarrativeLevelSequenceActor* SequenceActor, APlayerController* Viewer,
		AActor* Subject, UTerritoryCinematicLightRigProfile* Profile);

	/** Internal adapter called by the existing Territory Narrative shot. */
	static void FollowDialogueSequence(ALevelSequenceActor* SequenceActor, UDialogue* Dialogue);

	UFUNCTION(BlueprintCallable, Category="Territory|Cinematics|Optional Lights",
		meta=(ToolTip="Remove this session's temporary lights and listeners. Does not stop Narrative playback or change the character."))
	void StopLightRig();
	UFUNCTION(BlueprintPure, Category="Territory|Cinematics|Optional Lights")
	AActor* GetSpawnedLightRig() const { return SpawnedRig; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	friend class FTFTerritoryLightRigLifecycle;
	static bool CanUseLights(const APlayerController* Viewer, const UWorld* World);
	static UTerritoryCinematicLightRigComponent* FindOrCreate(ALevelSequenceActor* Actor, APlayerController* Viewer);
	void RefreshRig(float DeltaTime);
	void ClearRig();
	void Fail(const FString& Reason);
	UFUNCTION() void CameraCut(UCameraComponent* Camera);
	UFUNCTION() void PlaybackEnded();
	UPROPERTY(Transient) TObjectPtr<UTerritoryCinematicLightRigProfile> ActiveProfile;
	UPROPERTY(Transient) TObjectPtr<AActor> SpawnedRig;
	TWeakObjectPtr<APlayerController> LocalViewer;
	TWeakObjectPtr<UDialogue> SourceDialogue;
	TWeakObjectPtr<ULevelSequencePlayer> SequencePlayer;
	TWeakObjectPtr<AActor> SubjectActor;
	TWeakObjectPtr<AActor> BoundVisual;
	TWeakObjectPtr<ACameraActor> BoundCamera;
	bool bFollowingDialogue = false;
	bool bFailed = false;
	float WaitingTime = 0.f;
};
