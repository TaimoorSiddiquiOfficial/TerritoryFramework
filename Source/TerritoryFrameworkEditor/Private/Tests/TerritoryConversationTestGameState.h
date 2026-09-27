#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "TerritoryConversationTestGameState.generated.h"

/** Concrete Native faction/time service for isolated worlds. The Native base
 * deliberately asserts when its required stable-identity interface is unimplemented. */
UCLASS(Transient)
class ATerritoryConversationTestGameState : public ANarrativeGameState
{
	GENERATED_BODY()
public:
	virtual FGuid GetActorGUID_Implementation() const override { return StableTestGUID; }
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override { StableTestGUID = SavedGUID; }
private:
	UPROPERTY() FGuid StableTestGUID = FGuid::NewGuid();
};
