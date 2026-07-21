#include "Combat/BTTask_RequestTerritoryPermission.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTTask_RequestTerritoryPermission::UBTTask_RequestTerritoryPermission()
{
	NodeName = "Request Territory Permission";

	TerritoryKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_RequestTerritoryPermission, TerritoryKey), ATerritoryVolume::StaticClass());
	bPermissionGrantedKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_RequestTerritoryPermission, bPermissionGrantedKey));
}

EBTNodeResult::Type UBTTask_RequestTerritoryPermission::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(AIController);
	if (!NPCController) return EBTNodeResult::Failed;

	UTerritoryCombatDirector* Director = AIController->GetWorld()->GetSubsystem<UTerritoryCombatDirector>();
	if (!Director) return EBTNodeResult::Failed;

	// Get territory from blackboard or from current location
	ATerritoryVolume* Territory = Cast<ATerritoryVolume>(
		OwnerComp.GetBlackboardComponent()->GetValueAsObject(TerritoryKey.SelectedKeyName));

	if (!Territory)
	{
		// Fallback: find territory at NPC's current location using spatial index
		APawn* Pawn = AIController->GetPawn();
		if (Pawn)
		{
			UTerritoryRegistrySubsystem* Registry = AIController->GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
			if (Registry)
			{
				Territory = Registry->GetTerritoryAtLocation(Pawn->GetActorLocation());
			}
		}

		// No territory at location — wilderness, no restriction
		if (!Territory)
		{
			OwnerComp.GetBlackboardComponent()->SetValueAsBool(bPermissionGrantedKey.SelectedKeyName, true);
			return EBTNodeResult::Succeeded;
		}
	}

	bool bGranted = Director->RequestAttackPermission(Territory, NPCController);
	OwnerComp.GetBlackboardComponent()->SetValueAsBool(bPermissionGrantedKey.SelectedKeyName, bGranted);

	return bGranted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

void UBTTask_RequestTerritoryPermission::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	// Note: permission release is handled by BTTask_ReleaseTerritoryPermission
	// Do NOT auto-release here — the NPC may continue attacking across multiple BT ticks
}

FString UBTTask_RequestTerritoryPermission::GetStaticDescription() const
{
	return FString::Printf(TEXT("Territory: %s, Result: %s"),
		*TerritoryKey.SelectedKeyName.ToString(),
		*bPermissionGrantedKey.SelectedKeyName.ToString());
}
