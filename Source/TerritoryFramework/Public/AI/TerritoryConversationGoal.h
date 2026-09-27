#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCGoalItem.h"
#include "TerritoryConversationGoal.generated.h"

class ATerritoryConversationMeetingPoint;

/** Transient, interruptible movement/hold request owned by one meeting. */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryConversationGoal : public UNPCGoalItem
{
	GENERATED_BODY()
public:
	UTerritoryConversationGoal(const FObjectInitializer& Initializer);
	UPROPERTY(Transient) TWeakObjectPtr<ATerritoryConversationMeetingPoint> Meeting;
	UPROPERTY(Transient) FGuid SessionID;
	UPROPERTY(Transient) FVector Destination = FVector::ZeroVector;
	UPROPERTY(Transient) float ArrivalRadius = 80.f;
	virtual bool ShouldCleanup_Implementation() const override;
	virtual float GetGoalScore_Implementation() const override;
};
