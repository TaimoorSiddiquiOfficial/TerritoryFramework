#pragma once

#include "Tales/TalesComponent.h"
#include "Tales/NarrativeDataTask.h"
#include "TerritoryDataTaskProbe.generated.h"

/** Test-only access to Native's real record commit, without registering a project asset globally. */
UCLASS()
class UTerritoryDataTaskProbe : public UTalesComponent
{
	GENERATED_BODY()
public:
	bool Record(const UNarrativeDataTask* Task, const FString& Argument, int32 Quantity)
	{
		if (!HasAuthority()) return false;
		// Exact order in Native CompleteNarrativeDataTask after its asset lookup.
		OnNarrativeDataTaskCompleted.Broadcast(Task, Argument);
		return CompleteNarrativeTask_Internal(Task->MakeTaskString(Argument), false, Quantity);
	}
};
