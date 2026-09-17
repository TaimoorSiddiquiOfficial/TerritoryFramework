#pragma once
#include "Cinematics/TerritoryCinematicLightRigAdapter.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "TerritoryLightElementProbe.generated.h"

UINTERFACE()
class UTerritoryLightElementProbeInterface : public UInterface { GENERATED_BODY() };
class ITerritoryLightElementProbeInterface { GENERATED_BODY() };

/** A raw element deliberately lacking the Territory whole-rig interface. */
UCLASS()
class ATerritoryLightElementProbe : public AActor, public ITerritoryLightElementProbeInterface
{
	GENERATED_BODY()
public:
	bool bConstructed = false;
	virtual void OnConstruction(const FTransform& Transform) override { Super::OnConstruction(Transform); bConstructed = true; }
};

UCLASS()
class UTerritoryLightElementAdapterProbe : public UTerritoryCinematicLightRigAdapter
{
	GENERATED_BODY()
public:
	UTerritoryLightElementAdapterProbe();
	UPROPERTY(EditAnywhere, Category="Test") FName SceneTag;
	UPROPERTY(Transient) bool bPreparedBeforeConstruction = false;
	UPROPERTY(Transient) bool bActive = false;
	UPROPERTY(Transient) int32 CameraChanges = 0;
	UPROPERTY(Transient) int32 Updates = 0;
	UPROPERTY(Transient) int32 Releases = 0;
	virtual bool PrepareRig_Implementation() override;
	virtual bool ActivateRig_Implementation() override;
	virtual bool IsRigReady_Implementation() const override;
	virtual bool UpdateCamera_Implementation() override;
	virtual bool UpdateRig_Implementation(float DeltaSeconds) override;
	virtual void ReleaseRig_Implementation() override;
};
