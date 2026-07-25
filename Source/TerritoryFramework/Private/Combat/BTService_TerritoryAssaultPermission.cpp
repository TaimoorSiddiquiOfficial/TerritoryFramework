#include "Combat/BTService_TerritoryAssaultPermission.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_TerritoryAssaultPermission::UBTService_TerritoryAssaultPermission()
{
	NodeName = TEXT("Request / Release Territory Slot");
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	bNotifyTick = true;
	Interval = 0.5f;
	RandomDeviation = 0.f;
	TerritoryKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_TerritoryAssaultPermission, TerritoryKey), ATerritoryVolume::StaticClass());
	PermissionGrantedKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_TerritoryAssaultPermission, PermissionGrantedKey));
}

void UBTService_TerritoryAssaultPermission::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	UpdatePermission(OwnerComp);
}

void UBTService_TerritoryAssaultPermission::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UpdatePermission(OwnerComp);
}

void UBTService_TerritoryAssaultPermission::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ReleasePermission(OwnerComp);
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

ATerritoryVolume* UBTService_TerritoryAssaultPermission::ResolveTerritory(UBehaviorTreeComponent& OwnerComp) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (Blackboard && TerritoryKey.SelectedKeyType)
	{
		if (ATerritoryVolume* Territory = Cast<ATerritoryVolume>(Blackboard->GetValueAsObject(TerritoryKey.SelectedKeyName)))
		{
			return Territory;
		}
	}

	ANarrativeNPCController* Controller = Cast<ANarrativeNPCController>(OwnerComp.GetAIOwner());
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (bPreferGuardOwningTerritory)
	{
		if (ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(Pawn))
		{
			if (ATerritoryVolume* Territory = Guard->GetOwningTerritory())
			{
				return Territory;
			}
		}
	}

	if (Pawn)
	{
		if (UTerritoryRegistrySubsystem* Registry = Pawn->GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			return Registry->GetTerritoryAtLocation(Pawn->GetActorLocation());
		}
	}
	return nullptr;
}

void UBTService_TerritoryAssaultPermission::UpdatePermission(UBehaviorTreeComponent& OwnerComp)
{
	ANarrativeNPCController* Controller = Cast<ANarrativeNPCController>(OwnerComp.GetAIOwner());
	ATerritoryVolume* Territory = ResolveTerritory(OwnerComp);
	if (GrantedTerritory.Get() != Territory || GrantedController.Get() != Controller)
	{
		ReleasePermission(OwnerComp);
	}

	bool bGranted = false;
	if (Controller && Territory)
	{
		if (UTerritoryCombatDirector* Director = Controller->GetWorld()->GetSubsystem<UTerritoryCombatDirector>())
		{
			bGranted = Director->RequestAssaultSlot(Territory, Controller);
			if (bGranted)
			{
				GrantedTerritory = Territory;
				GrantedController = Controller;
			}
		}
	}

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent(); Blackboard && PermissionGrantedKey.SelectedKeyType)
	{
		Blackboard->SetValueAsBool(PermissionGrantedKey.SelectedKeyName, bGranted);
	}
}

void UBTService_TerritoryAssaultPermission::ReleasePermission(UBehaviorTreeComponent& OwnerComp)
{
	if (GrantedTerritory.IsValid() && GrantedController.IsValid())
	{
		if (UTerritoryCombatDirector* Director = GrantedController->GetWorld()->GetSubsystem<UTerritoryCombatDirector>())
		{
			Director->ReleaseAssaultSlot(GrantedTerritory.Get(), GrantedController.Get());
		}
	}
	GrantedTerritory = nullptr;
	GrantedController = nullptr;

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent(); Blackboard && PermissionGrantedKey.SelectedKeyType)
	{
		Blackboard->SetValueAsBool(PermissionGrantedKey.SelectedKeyName, false);
	}
}
