#include "Combat/BTService_TerritoryAssaultPermission.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

UBTService_TerritoryAssaultPermission::UBTService_TerritoryAssaultPermission()
{
	NodeName = TEXT("Request / Release Territory Slot");
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	bNotifyTick = true;
	Interval = 0.5f;
	RandomDeviation = 0.f;
	TerritoryKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_TerritoryAssaultPermission, TerritoryKey), AActor::StaticClass());
	PermissionGrantedKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_TerritoryAssaultPermission, PermissionGrantedKey));
}

void UBTService_TerritoryAssaultPermission::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TerritoryKey.ResolveSelectedKey(*BlackboardAsset);
		PermissionGrantedKey.ResolveSelectedKey(*BlackboardAsset);
	}
	else
	{
		TerritoryKey.InvalidateResolvedKey();
		PermissionGrantedKey.InvalidateResolvedKey();
	}
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
	if (Blackboard && TerritoryKey.IsSet())
	{
		AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TerritoryKey.SelectedKeyName));
		if (ATerritoryVolume* Territory = Cast<ATerritoryVolume>(TargetActor))
		{
			return Territory;
		}
		if (TargetActor)
		{
			if (UTerritoryRegistrySubsystem* Registry = TargetActor->GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				if (ATerritoryVolume* Territory = Registry->GetTerritoryAtLocation(TargetActor->GetActorLocation()))
				{
					return Territory;
				}
			}
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
			const APawn* Pawn = Controller->GetPawn();
			const bool bPhysicalAssaultParticipant =
				UTerritoryCombatDirector::RequiresStrategicAssaultSlot(Pawn);
			// Ordinary guards still use Narrative attack tokens and must not consume
			// the strategic force budget. A pawn carrying the participant component
			// must pass the director's complete assault identity validation.
			bGranted = !bPhysicalAssaultParticipant
				|| Director->RequestAssaultSlot(Territory, Controller);
			if (bGranted && bPhysicalAssaultParticipant)
			{
				GrantedTerritory = Territory;
				GrantedController = Controller;
			}
		}
	}

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent(); Blackboard && PermissionGrantedKey.IsSet())
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

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent(); Blackboard && PermissionGrantedKey.IsSet())
	{
		Blackboard->SetValueAsBool(PermissionGrantedKey.SelectedKeyName, false);
	}
}
