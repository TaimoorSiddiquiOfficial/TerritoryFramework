#pragma once

#include "CoreMinimal.h"
#include "AI/TerritoryInvestigationActivity.h"
#include "TerritoryReserveTestActivity.generated.h"

/** Runs Native's real K2_RunActivity dispatch without navigation or a saved test Blueprint. */
UCLASS(Transient)
class UTerritoryReserveTestActivity : public UTerritoryInvestigationActivity
{
	GENERATED_BODY()
public:
	TFunction<void()> RunCallback;
	virtual void ProcessEvent(UFunction* Function, void* Params) override
	{
		Super::ProcessEvent(Function, Params);
		if (Function && Function->GetFName() == TEXT("K2_RunActivity") && RunCallback) RunCallback();
	}
protected:
	virtual bool RunActivity() override { return true; }
};
