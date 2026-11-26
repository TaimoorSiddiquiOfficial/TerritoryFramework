#pragma once

#include "GameFramework/Pawn.h"
#include "NarrativeSavableActor.h"
#include "TerritoryPlayerSaveProbe.generated.h"

/** Exercises the real Native record callback, including reentrant failure. */
UCLASS()
class ATerritoryPlayerSaveProbe : public APawn, public INarrativeSavableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame)
	int32 SavedValue = 0;
	FGuid StableID = FGuid::NewGuid();
	virtual FGuid GetActorGUID_Implementation() const override { return StableID; }
	virtual void SetActorGUID_Implementation(const FGuid& Value) override { StableID = Value; }
	TFunction<void()> LoadCallback;
	virtual void Load_Implementation() override
	{
		if (LoadCallback) LoadCallback();
	}
};
