#pragma once
#include "Cinematics/TerritoryCinematicLightRig.h"
#include "LevelSequencePlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Tales/Dialogue.h"
#include "TerritoryLightRigProbe.generated.h"

UCLASS()
class UTerritoryLightRigPlayerProbe : public ULevelSequencePlayer
{
	GENERATED_BODY()
public:
	void CutTo(UCameraComponent* Camera) { Status = EMovieScenePlayerStatus::Playing; CachedCameraComponent = Camera; OnCameraCut.Broadcast(Camera); }
	void HoldFinalFrame() { Status = EMovieScenePlayerStatus::Paused; }
};

UCLASS()
class ATerritoryLightRigProbe : public AActor, public ITerritoryCinematicLightRig
{
	GENERATED_BODY()
public:
	ATerritoryLightRigProbe();
	UPROPERTY() TObjectPtr<class UChildActorComponent> LightChild;
	UPROPERTY() TObjectPtr<AActor> Visual;
	UPROPERTY() TObjectPtr<ACameraActor> Camera;
	int32 CameraUpdates = 0;
	bool bPreparedBeforeBeginPlay = false;
	virtual bool PrepareLightRig_Implementation(AActor* InVisual, ACameraActor* InCamera) override;
	virtual bool IsLightRigReady_Implementation() const override;
	virtual bool SetLightRigCamera_Implementation(ACameraActor* InCamera) override;
};

/** Held editor fixture; Native still owns speaker binding, camera playback and exit. */
UCLASS()
class UTerritoryLightRigTestDialogue : public UDialogue
{
	GENERATED_BODY()
public:
	virtual bool Initialize(class UTalesComponent* Component, const FDialoguePlayParams Params) override;
	void UseShot(class UTerritoryDialogueShot* Shot);
};

/** Editor-only gameplay harness. Uses the public Native factory and camera binding API. */
UCLASS()
class ATerritoryLightRigTestDriver : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Test") TObjectPtr<ANarrativeLevelSequenceActor> TestSequence;
	UPROPERTY(BlueprintReadOnly, Category="Test") TObjectPtr<UTerritoryCinematicLightRigComponent> TestSession;
	UPROPERTY(BlueprintReadOnly, Category="Test") TObjectPtr<ACameraActor> OverrideCamera;
	UFUNCTION(BlueprintCallable, Category="Test") bool StartTest(APlayerController* Viewer, AActor* Subject,
		UTerritoryCinematicLightRigProfile* Profile, class ULevelSequence* Sequence);
	UFUNCTION(BlueprintCallable, Category="Test") bool ChangeCamera();
	UFUNCTION(BlueprintCallable, Category="Test") bool StartInPIEWorld(int32 PIEInstance, FName SubjectName,
		UTerritoryCinematicLightRigProfile* Profile, class ULevelSequence* Sequence);
	UFUNCTION(BlueprintCallable, Category="Test") bool StartDialogueTest(class UTalesComponent* Tales, class ANarrativeNPCCharacter* Speaker,
		UTerritoryCinematicLightRigProfile* Profile, class ULevelSequence* Sequence);
	UFUNCTION(BlueprintCallable, Category="Test") bool ReplayDialogueTest(class UTalesComponent* Tales, bool UseListener);
	UFUNCTION(BlueprintCallable, Category="Test") void PauseTest();
	UFUNCTION(BlueprintCallable, Category="Test") void ResumeTest();
	UFUNCTION(BlueprintCallable, Category="Test") void StopTest();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
