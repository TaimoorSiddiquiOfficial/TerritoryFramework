#pragma once

#include "CoreMinimal.h"
#include "AI/NarrativeNPCController.h"
#include "TerritoryNPCController.generated.h"

/** Narrative controller with the Territory save adapter on its existing activity component. */
UCLASS()
class TERRITORYFRAMEWORK_API ATerritoryNPCController : public ANarrativeNPCController
{
	GENERATED_BODY()
public:
	ATerritoryNPCController(const FObjectInitializer& ObjectInitializer);
};
