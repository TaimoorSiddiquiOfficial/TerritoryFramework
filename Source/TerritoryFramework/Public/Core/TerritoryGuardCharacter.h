#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TerritoryGuardCharacter.generated.h"

/**
 * Territory-specific NPC character that properly implements GetActorGUID
 * by returning SpawnInfo.SpawnAssignedSaveGUID. This prevents the
 * NarrativeStableActor assertion crash when spawned outside of
 * Narrative's UNPCSpawnComponent framework.
 *
 * Assign this class (or a Blueprint child of it) as the NPCClassPath
 * in the GuardNPCDefinition asset.
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryGuardCharacter : public ANarrativeNPCCharacter
{
	GENERATED_BODY()

public:
	ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer);

	// INarrativeStableActor — return SpawnAssignedSaveGUID instead of crashing
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;

	// Blueprint-accessible helper to set the save GUID before FinishSpawning
	UFUNCTION(BlueprintCallable, Category = "Territory|Guard")
	void SetTerritorySaveGUID(const FGuid& NewGUID);

	// Blueprint-accessible helper to set the owning territory GUID
	UFUNCTION(BlueprintCallable, Category = "Territory|Guard")
	void SetOwningTerritoryGUID(const FGuid& TerritoryGUID);

private:
	/** Cached fallback GUID — generated once if SpawnAssignedSaveGUID is invalid */
	FGuid CachedFallbackGUID;
};
