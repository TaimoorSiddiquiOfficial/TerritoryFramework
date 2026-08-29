#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerritoryDistractionComponent.generated.h"

/**
 * Add this to a Narrative throwable-projectile child. Its first blocking hit reports
 * a tagged hearing stimulus; Territory guards then choose the nearest investigators.
 */
UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable,
	meta=(BlueprintSpawnableComponent, DisplayName="Territory Distraction"))
class TERRITORYFRAMEWORK_API UTerritoryDistractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerritoryDistractionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Distraction",
		meta=(ToolTip="Automatically report once when the owning Narrative projectile hits an actor or surface."))
	bool bReportOnFirstHit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Distraction",
		meta=(ClampMin="0.0"))
	float Loudness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Distraction",
		meta=(ClampMin="0.0", Units="cm"))
	float MaximumRange = 3000.f;

	/** May also be called explicitly by a Narrative ability or quest object. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Distraction")
	bool ReportDistractionAtLocation(const FVector& WorldLocation);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bHasReported = false;

	UFUNCTION()
	void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor,
		FVector NormalImpulse, const FHitResult& Hit);
};
