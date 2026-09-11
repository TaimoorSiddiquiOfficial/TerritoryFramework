#pragma once

#include "AI/Activities/NPCGoalGenerator.h"
#include "Perception/AIPerceptionTypes.h"
#include "TerritoryNPCSaveProbe.generated.h"

/** Concrete generator with real SaveGame state; no competing AI implementation. */
UCLASS()
class UTerritoryNPCSaveProbe : public UNPCGoalGenerator
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame)
	int32 SavedValue = 0;
	UPROPERTY(SaveGame)
	TArray<int32> SavedValues;
	UPROPERTY(Transient)
	int32 LiveValue = 5;
	int32 InitializationCount = 0;
	TArray<float> ReceivedStrengths;
	TFunction<void()> PerceptionCallback;
	UFUNCTION()
	void PerceptionReceived(AActor* Actor, FAIStimulus Stimulus)
	{
		ReceivedStrengths.Add(Stimulus.Strength);
		if (PerceptionCallback) PerceptionCallback();
	}
	virtual void InitializeGoalGenerator_Implementation() override { ++InitializationCount; }
};

UCLASS()
class UTerritoryNPCSaveReplacementProbe : public UTerritoryNPCSaveProbe
{
	GENERATED_BODY()
};
