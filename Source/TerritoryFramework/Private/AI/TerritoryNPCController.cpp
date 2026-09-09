#include "AI/TerritoryNPCController.h"
#include "AI/TerritoryNPCActivityComponent.h"

ATerritoryNPCController::ATerritoryNPCController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTerritoryNPCActivityComponent>(TEXT("NPCActivityComponent")))
{
}
