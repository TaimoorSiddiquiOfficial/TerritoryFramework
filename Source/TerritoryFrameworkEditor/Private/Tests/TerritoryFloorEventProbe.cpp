#include "TerritoryFloorEventProbe.h"

UTerritoryFloorClearedProbeEvent::UTerritoryFloorClearedProbeEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A probe counts what the framework dispatched during the test. Narrative would otherwise
	// re-run the event when the quest loads, which would make the count depend on load order
	// rather than on the floor transition under test.
	bRefireOnLoad = false;
}

void UTerritoryFloorClearedProbeEvent::ExecuteEvent_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	++ExecutionCount;
}

bool UTerritoryFloorClearedProbeCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	return bPasses;
}
