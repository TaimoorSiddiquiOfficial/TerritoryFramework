#pragma once

#include "CoreMinimal.h"
#include "ContextualAnimSceneActorComponent.h"
#include "TerritoryContextualAnimComponent.generated.h"

/** Supplies Narrative's execution contract and resumes a surviving NPC after interruption. */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryContextualAnimComponent : public UContextualAnimSceneActorComponent
{
	GENERATED_BODY()

protected:
	virtual void OnLeaveScene(const FContextualAnimSceneBinding& Binding) override;
};
