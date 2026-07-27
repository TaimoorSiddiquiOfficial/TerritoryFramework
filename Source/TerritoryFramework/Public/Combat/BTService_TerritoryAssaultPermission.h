#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BTService_TerritoryAssaultPermission.generated.h"

class ATerritoryVolume;
class ANarrativeNPCController;
class UBehaviorTree;

/** Holds a Territory strategic assault slot for the lifetime of an attack BT branch. */
UCLASS(meta=(DisplayName="Territory Assault Permission"))
class TERRITORYFRAMEWORK_API UBTService_TerritoryAssaultPermission : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_TerritoryAssaultPermission();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	/** Territory or target Actor whose current Territory owns the assault limit. */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TerritoryKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector PermissionGrantedKey;

	UPROPERTY(EditAnywhere, Category="Territory")
	bool bPreferGuardOwningTerritory = true;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	TWeakObjectPtr<ATerritoryVolume> GrantedTerritory;
	TWeakObjectPtr<ANarrativeNPCController> GrantedController;

	ATerritoryVolume* ResolveTerritory(UBehaviorTreeComponent& OwnerComp) const;
	void UpdatePermission(UBehaviorTreeComponent& OwnerComp);
	void ReleasePermission(UBehaviorTreeComponent& OwnerComp);
};
