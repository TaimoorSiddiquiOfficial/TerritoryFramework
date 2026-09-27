#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCGoalItem.h"
#include "TerritoryCombatKeyProbe.generated.h"

/** Test seam for a legal project override whose key changes between invocations. */
UCLASS()
class UTerritoryCombatKeyProbe : public UNPCGoalItem
{
	GENERATED_BODY()
public:
	mutable int32 KeyReads = 0;
	UPROPERTY() TObjectPtr<UObject> FirstKey;
	virtual UObject* GetGoalKey_Implementation() const override
	{
		return ++KeyReads == 1 ? FirstKey.Get() : nullptr;
	}
};
