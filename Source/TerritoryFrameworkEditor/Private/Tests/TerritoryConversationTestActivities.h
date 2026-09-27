#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/TerritoryConversationGoal.h"
#include "AI/TerritoryPatrolGoal.h"
#include "TerritoryConversationTestActivities.generated.h"

/** A no-navigation activity fixture; Native still owns admission, selection and callback dispatch. */
UCLASS(Transient)
class UTerritoryConversationTestHoldActivity : public UNPCActivity
{
	GENERATED_BODY()
public:
	UTerritoryConversationTestHoldActivity(const FObjectInitializer& Init) : Super(Init)
	{ SupportedGoalType = UTerritoryConversationGoal::StaticClass(); bIsInterruptable = true; }
protected:
	virtual bool RunActivity() override { return true; }
};

UCLASS(Transient)
class UTerritoryConversationTestResumeActivity : public UNPCActivity
{
	GENERATED_BODY()
public:
	UTerritoryConversationTestResumeActivity(const FObjectInitializer& Init) : Super(Init)
	{ SupportedGoalType = UTerritoryPatrolGoal::StaticClass(); bIsInterruptable = true; }
	TFunction<void()> ResumeCallback;
	virtual void ProcessEvent(UFunction* Function, void* Params) override
	{
		Super::ProcessEvent(Function, Params);
		if (Function && Function->GetFName() == TEXT("K2_RunActivity") && ResumeCallback) ResumeCallback();
	}
protected:
	virtual bool RunActivity() override { return true; }
};
