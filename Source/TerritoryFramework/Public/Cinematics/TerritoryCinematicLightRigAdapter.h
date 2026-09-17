#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "Templates/SubclassOf.h"
#include "TerritoryCinematicLightRigAdapter.generated.h"

class AActor;
class ACameraActor;
class ALight;
class ULightComponent;
class USkeletalMeshComponent;

/** Instanced presentation setup, like a Narrative dialogue shot. A private transient copy runs per local session. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TERRITORYFRAMEWORK_API UTerritoryCinematicLightRigAdapter : public UObject
{
	GENERATED_BODY()
public:
	UTerritoryCinematicLightRigAdapter();
	virtual UWorld* GetWorld() const override;
	/** The default adapter uses the Territory whole-rig interface. Pack adapters select their own public interface. */
	UPROPERTY(EditDefaultsOnly, Category="Adapter Contract", meta=(ToolTip="Interface the selected Rig Class must implement. The pack element adapter uses BPI Light Rig Element."))
	TSubclassOf<UInterface> RequiredRigInterface;
	UPROPERTY(EditDefaultsOnly, Category="Adapter Contract", meta=(ToolTip="Base actor class allowed for this adapter. Child classes are accepted."))
	TSubclassOf<AActor> SupportedRigBaseClass;
	UPROPERTY(EditDefaultsOnly, Category="Adapter Contract", meta=(ToolTip="Enable for elements that need camera and character tracking each frame. Whole rigs already update their own lights."))
	bool bUpdateEveryFrame = false;
	UPROPERTY(EditDefaultsOnly, Category="Adapter Contract", meta=(ToolTip="Character elements wait for the configured meshes and bones. Background-only adapters can disable this to work with an actor anchor and no character mesh."))
	bool bRequiresCharacterMeshes = true;

	bool SupportsRigClass(UClass* Class) const;
	bool Initialize(AActor* InRig, AActor* InVisual, ACameraActor* InCamera);
	bool ChangeCamera(ACameraActor* InCamera);
	void Shutdown();

	UPROPERTY(Transient, BlueprintReadOnly, Category="Runtime") TObjectPtr<AActor> Rig;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Runtime") TObjectPtr<AActor> Visual;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Runtime") TObjectPtr<ACameraActor> Camera;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Runtime") FString FailureReason;

	/** Called before the selected actor's construction/BeginPlay. Supply its public setup inputs here. */
	UFUNCTION(BlueprintNativeEvent, Category="Light Rig Adapter") bool PrepareRig();
	/** Construction has created the selected actor's components. Call the element's Setup/UpdateGlobals here. */
	UFUNCTION(BlueprintNativeEvent, Category="Light Rig Adapter") bool ActivateRig();
	/** Called after BeginPlay. Check real configuration and component readiness. */
	UFUNCTION(BlueprintNativeEvent, Category="Light Rig Adapter") bool IsRigReady() const;
	/** Camera is already updated. Rebind the existing actor; do not spawn another one. */
	UFUNCTION(BlueprintNativeEvent, Category="Light Rig Adapter") bool UpdateCamera();
	/** Called only while Native owns the displayed camera. Return false to release this session on failure. */
	UFUNCTION(BlueprintNativeEvent, Category="Light Rig Adapter") bool UpdateRig(float DeltaSeconds);
	/** Optional pack cleanup. Native always restores captured scene lights after this hook, even without a parent call. */
	UFUNCTION(BlueprintNativeEvent, Category="Light Rig Adapter") void ReleaseRig();

	UFUNCTION(BlueprintPure, Category="Light Rig Adapter", meta=(ToolTip="Find one registered mesh with this exact component name on the bound Narrative visual. Duplicate or missing names return empty."))
	USkeletalMeshComponent* FindVisualMesh(FName ComponentName) const;
	UFUNCTION(BlueprintCallable, Category="Light Rig Adapter|Scene Lights", meta=(ToolTip="Reserve loaded ALight actors with this Actor Tag and remember their original settings. Refreshes at most twice per second unless forced. Restores removed targets and discovers streamed-in lights. Another active session owning a target causes failure before any target is changed."))
	bool RefreshTaggedSceneLights(FName ActorTag, float DeltaSeconds, bool bForce = false);
	UFUNCTION(BlueprintPure, Category="Light Rig Adapter|Scene Lights", meta=(ToolTip="Only the still-valid scene lights reserved by this session. Pass these to the pack background element before updating it."))
	TArray<ALight*> GetCapturedSceneLights() const;
	UFUNCTION(BlueprintCallable, Category="Light Rig Adapter|Scene Lights", meta=(ToolTip="Restore all scene-light settings captured by this session. Safe to call again. Shutdown also calls this automatically."))
	void RestoreSceneLights();

private:
	UFUNCTION() void RigDestroyed(AActor* DestroyedActor);
	struct FLightState
	{
		TWeakObjectPtr<ALight> Actor;
		TWeakObjectPtr<ULightComponent> Component;
		float Intensity = 0.f;
		FColor Color;
		float Temperature = 0.f;
		bool bUseTemperature = false;
		uint8 RayTracedShadows = 0;
		int32 Samples = 0;
	};
	void RestoreLight(const FLightState& State);
	TArray<FLightState> CapturedLights;
	FName CapturedTag;
	float RefreshRemaining = 0.f;
	bool bShuttingDown = false;
};
