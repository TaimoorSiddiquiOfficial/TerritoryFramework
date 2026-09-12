#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "TerritoryMountPresentationComponent.generated.h"

class UCapsuleComponent;

/** Stops a remote seated character from pushing its own Native mount. Add to Narrative player and story NPC Blueprints; Territory guards already include it. */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent, DisplayName="Territory Mount Presentation"))
class TERRITORYFRAMEWORK_API UTerritoryMountPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTerritoryMountPresentationComponent();
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

	/** Follow Native's current mount attachment on a client. Restore the previous capsule collision after exit; never claim a seat, drive, or change the server. */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative|Mount")
	void RefreshMountPresentation();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void RestoreCollision();
	void OnRootTransformUpdated(USceneComponent* Updated, EUpdateTransformFlags Flags, ETeleportType Teleport);
	TWeakObjectPtr<USceneComponent> ObservedRoot;
	FDelegateHandle TransformHandle;
	TWeakObjectPtr<UCapsuleComponent> SuppressedCapsule;
	ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::NoCollision;
};
