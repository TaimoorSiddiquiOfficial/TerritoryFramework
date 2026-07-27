#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "TerritoryPlayerController.generated.h"

/** Project-owned Narrative controller extension. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryPlayerController : public ANarrativePlayerController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupInputComponent() override;

private:
	void EnsureGameplayHUDClass();
};
