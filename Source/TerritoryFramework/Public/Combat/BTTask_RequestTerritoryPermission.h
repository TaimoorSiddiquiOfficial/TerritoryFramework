#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_RequestTerritoryPermission.generated.h"

/**
 * BT Task: Request attack permission from the territory combat director.
 * Succeeds if permission is granted. Fails if no slots available.
 * Stores the territory reference on the blackboard for release on task end.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UBTTask_RequestTerritoryPermission : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_RequestTerritoryPermission();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Territory Combat")
	FBlackboardKeySelector TerritoryKey;

	UPROPERTY(EditAnywhere, Category = "Territory Combat")
	FBlackboardKeySelector bPermissionGrantedKey;
};
