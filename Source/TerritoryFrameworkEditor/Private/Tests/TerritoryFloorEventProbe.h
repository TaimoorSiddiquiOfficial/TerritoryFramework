#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryFloorEventProbe.generated.h"

class APlayerController;
class APawn;
class ATerritoryGuardSpawnPoint;
class ATerritoryVolume;
class UTalesComponent;

/**
 * Editor-only receiver for the two Blueprint-assignable floor delegates.
 *
 * ATerritoryVolume's floor callbacks are dynamic multicast delegates, so they can only bind
 * to a UFUNCTION - a lambda cannot observe them. This probe is that receiver, the same way
 * UTerritoryAuditEventProbe is for the garrison and assault callbacks.
 */
UCLASS(Transient)
class UTerritoryFloorEventProbe final : public UObject
{
	GENERATED_BODY()

public:
	/** How many times OnFloorCleared reached this probe, and the floors it named. */
	int32 FloorClearedCount = 0;
	TArray<int32> ClearedFloors;

	/** How many times OnDefenderSpawned reached this probe, and what it named. */
	int32 DefenderSpawnedCount = 0;
	TArray<int32> SpawnedFloors;

	UPROPERTY()
	TObjectPtr<AActor> LastSpawnedGuard = nullptr;

	UFUNCTION()
	void FloorCleared(ATerritoryVolume* Territory, int32 FloorIndex)
	{
		++FloorClearedCount;
		ClearedFloors.Add(FloorIndex);
	}

	UFUNCTION()
	void DefenderSpawned(ATerritoryVolume* Territory, AActor* Guard, int32 FloorIndex)
	{
		++DefenderSpawnedCount;
		SpawnedFloors.Add(FloorIndex);
		LastSpawnedGuard = Guard;
	}
};

/**
 * A Narrative event that records its own execution, so a test can prove the floor-cleared
 * dispatch actually reached the authored event rather than merely iterating the array.
 */
UCLASS(Transient)
class UTerritoryFloorClearedProbeEvent final : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryFloorClearedProbeEvent(const FObjectInitializer& ObjectInitializer);

	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override;

	int32 ExecutionCount = 0;
};

/**
 * A Narrative condition with a switch, so a test can prove a failed condition suppresses the
 * floor-cleared event instead of letting it run unconditionally.
 */
UCLASS(Transient)
class UTerritoryFloorClearedProbeCondition final : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override;

	bool bPasses = false;
};
