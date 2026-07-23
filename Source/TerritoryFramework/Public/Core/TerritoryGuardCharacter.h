#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TerritoryGuardCharacter.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;

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

	/**
	 * Single entrypoint for deterministic territory guard configuration.
	 * Sets up SpawnInfo.SpawnParams BEFORE SetNPCDefinition so Narrative's
	 * definition initialization reads the correct faction, GUIDs, and overrides.
	 *
	 * Call this during deferred spawn (between BeginDeferredActorSpawnFromClass
	 * and FinishSpawningActor), NOT after FinishSpawningActor.
	 */
	void ConfigureTerritorySpawn(
		UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction,
		const FGuid& TerritoryGuid,
		const FGuid& SaveGuid,
		UNPCActivityConfiguration* OptionalActivityOverride = nullptr,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides = {});

protected:
	virtual void BeginPlay() override;

	// Prevent Narrative save system from restoring stale guards on load.
	// TerritoryVolume manages guard lifecycle via SpawnGuards/DespawnGuards.
	virtual bool ShouldRespawn_Implementation() const override;

private:
	/** Cached fallback GUID — generated once if SpawnAssignedSaveGUID is invalid */
	FGuid CachedFallbackGUID;
};
