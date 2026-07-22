#include "Combat/BTTask_ReleaseTerritoryPermission.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "AI/NarrativeNPCController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTTask_ReleaseTerritoryPermission::UBTTask_ReleaseTerritoryPermission()
{
	NodeName = "Release Territory Permission";

	TerritoryKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_ReleaseTerritoryPermission, TerritoryKey), ATerritoryVolume::StaticClass());
}

EBTNodeResult::Type UBTTask_ReleaseTerritoryPermission::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Succeeded;

	ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(AIController);
	if (!NPCController) return EBTNodeResult::Succeeded;

	UTerritoryCombatDirector* Director = AIController->GetWorld()->GetSubsystem<UTerritoryCombatDirector>();
	if (!Director) return EBTNodeResult::Succeeded;

	// Release all permissions for this controller
	Director->ReleaseAllSlots(NPCController);

	return EBTNodeResult::Succeeded;
}

FString UBTTask_ReleaseTerritoryPermission::GetStaticDescription() const
{
	return FString::Printf(TEXT("Territory: %s"), *TerritoryKey.SelectedKeyName.ToString());
}
