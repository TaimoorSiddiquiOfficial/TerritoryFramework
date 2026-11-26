#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "TerritoryCombatSearchProbe.generated.h"

/** Editor-only fixture for the cleanup/start-tree boundary used by Native activities. */
UCLASS(Transient)
class UTerritoryCombatSearchProbe : public UBTService
{
	GENERATED_BODY()
public:
	UTerritoryCombatSearchProbe();
	void SetSearchTick(bool bEnabled) { bCallTickOnSearchStart = bEnabled; }
	static TFunction<void(UBehaviorTreeComponent&)> OnTick;
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
