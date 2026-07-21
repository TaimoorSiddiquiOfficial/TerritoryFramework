#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ReleaseTerritoryPermission.generated.h"

/**
 * BT Task: Release attack permission from the territory combat director.
 * Should be paired with BTTask_RequestTerritoryPermission.
 * Always succeeds.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UBTTask_ReleaseTerritoryPermission : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ReleaseTerritoryPermission();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Territory Combat")
	FBlackboardKeySelector TerritoryKey;
};
