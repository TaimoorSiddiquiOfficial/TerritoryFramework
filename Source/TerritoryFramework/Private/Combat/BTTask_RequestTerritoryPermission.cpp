#include "Combat/BTTask_RequestTerritoryPermission.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
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

	UWorld* World = AIController->GetWorld();
	if (!World) return EBTNodeResult::Failed;

	UTerritoryCombatDirector* Director = World->GetSubsystem<UTerritoryCombatDirector>();
	if (!Director) return EBTNodeResult::Failed;

	// Validate BB keys are configured — unconfigured keys write to NAME_None (no-op)
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB || bPermissionGrantedKey.SelectedKeyName == NAME_None)
	{
		UE_LOG(LogTerritory, Warning, TEXT("BTTask_RequestTerritoryPermission: bPermissionGrantedKey not configured"));
		return EBTNodeResult::Failed;
	}

	// Get territory from blackboard or from current location
	ATerritoryVolume* Territory = TerritoryKey.SelectedKeyName != NAME_None
		? Cast<ATerritoryVolume>(BB->GetValueAsObject(TerritoryKey.SelectedKeyName))
		: nullptr;

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
			BB->SetValueAsBool(bPermissionGrantedKey.SelectedKeyName, true);
			return EBTNodeResult::Succeeded;
		}

		// Write resolved territory back to BB so the paired release task doesn't need
		// to re-resolve from location (the NPC may have moved since request).
		if (TerritoryKey.SelectedKeyName != NAME_None)
		{
			BB->SetValueAsObject(TerritoryKey.SelectedKeyName, Territory);
		}
	}

	bool bGranted = Director->RequestAssaultSlot(Territory, NPCController);
	BB->SetValueAsBool(bPermissionGrantedKey.SelectedKeyName, bGranted);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->IsDebugEnabled())
	{
		UE_LOG(LogTerritory, Log, TEXT("[BT] RequestPermission: %s → %s (%s)"),
			*Territory->GetTerritoryTag().ToString(),
			bGranted ? TEXT("GRANTED") : TEXT("DENIED"),
			*AIController->GetName());
	}

	return bGranted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

void UBTTask_RequestTerritoryPermission::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	// Note: permission release is handled by BTTask_ReleaseTerritoryPermission
	// Do NOT auto-release here — the NPC may continue attacking across multiple BT ticks
}

EBTNodeResult::Type UBTTask_RequestTerritoryPermission::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::AbortTask(OwnerComp, NodeMemory);

	// If the BT subtree aborts between request and release, the slot would leak until NPC death.
	// Release the specific territory slot to prevent slot exhaustion.
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Aborted;

	ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(AIController);
	if (!NPCController) return EBTNodeResult::Aborted;

	UWorld* World = AIController->GetWorld();
	if (!World) return EBTNodeResult::Aborted;

	UTerritoryCombatDirector* Director = World->GetSubsystem<UTerritoryCombatDirector>();
	if (!Director) return EBTNodeResult::Aborted;

	// Get the territory from the blackboard (stored in ExecuteTask at line 63-66)
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	ATerritoryVolume* Territory = nullptr;
	if (BB && TerritoryKey.SelectedKeyName != NAME_None)
	{
		Territory = Cast<ATerritoryVolume>(BB->GetValueAsObject(TerritoryKey.SelectedKeyName));
	}

	// Release the specific territory slot, not all slots
	if (Territory)
	{
		Director->ReleaseSlot(Territory, NPCController);

		const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
		if (Settings && Settings->IsDebugEnabled())
		{
			UE_LOG(LogTerritory, Log, TEXT("[BT] RequestPermission ABORTED — released slot for %s (%s)"),
				*Territory->GetTerritoryTag().ToString(),
				*AIController->GetName());
		}
	}
	else
	{
		// Fallback: if territory not in BB, release all slots (shouldn't happen in normal flow)
		Director->ReleaseAllSlots(NPCController);
		UE_LOG(LogTerritory, Warning, TEXT("[BT] RequestPermission ABORTED — territory not in BB, released all slots for %s"),
			*AIController->GetName());
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_RequestTerritoryPermission::GetStaticDescription() const
{
	return FString::Printf(TEXT("Territory: %s, Result: %s"),
		*TerritoryKey.SelectedKeyName.ToString(),
		*bPermissionGrantedKey.SelectedKeyName.ToString());
}
