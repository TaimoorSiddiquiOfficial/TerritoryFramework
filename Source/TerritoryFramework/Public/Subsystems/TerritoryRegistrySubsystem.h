#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritorySpatialIndex.h"
#include "TerritoryRegistrySubsystem.generated.h"

class ATerritoryVolume;

/**
 * Territory registry subsystem — the authoritative list of all registered territories.
 *
 * Every ATerritoryVolume registers itself in BeginPlay and unregisters in EndPlay.
 * The registry owns the spatial index (grid-based AABB lookup) and both tag/GUID maps.
 *
 * Use to:
 *   - Look up territories by tag, GUID, or world location
 *   - Query territorial ownership for a faction
 *   - Subscribe to registration/unregistration events
 *
 * Quick start:
 *   Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>()
 *   Volume   = Registry->GetTerritoryByTag(Territory.Marketplace)
 *   Count    = Registry->GetTerritoryCount()
 *
 * Or from Blueprint:
 *   GetTerritoryRegistry(Self)->GetTerritoryByTag(Tag) -> Volume
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Registration (BlueprintCallable — mutates registry)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Register a territory volume. Called automatically by ATerritoryVolume::BeginPlay.
	 * Safe to call multiple times — duplicate registrations are ignored.
	 *
	 * Binds the volume's ownership + state delegates and adds it to the spatial index.
	 * Emits OnTerritoryRegistered delegate.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Registry", meta=(DisplayName="Register Territory"))
	void RegisterTerritory(ATerritoryVolume* Territory);

	/**
	 * Unregister a territory volume. Called automatically by ATerritoryVolume::EndPlay.
	 * Safe to call on null or unregistered volumes — no-op in those cases.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Registry", meta=(DisplayName="Unregister Territory"))
	void UnregisterTerritory(ATerritoryVolume* Territory);

	/**
	 * Re-indexes a territory's spatial entry after it has been moved or resized at runtime.
	 * Call this if you modify the volume's BoundsShape or actor transform in-game.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Registry", meta=(DisplayName="Update Territory Bounds"))
	void UpdateTerritoryBounds(ATerritoryVolume* Territory);

	// ═══════════════════════════════════════════════════════════════════════════
	// Read-Only Queries (BlueprintPure — no exec pin, can be used anywhere)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Lookup territory by its tag. Returns null if not found.
	 * Example: Volume = Registry->GetTerritoryByTag(Territory.Marketplace)
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territory By Tag"))
	ATerritoryVolume* GetTerritoryByTag(const FGameplayTag& TerritoryTag) const;

	/** Lookup territory by its stable GUID. Returns null if not found. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territory By GUID"))
	ATerritoryVolume* GetTerritoryByGUID(const FGuid& GUID) const;

	/**
	 * Returns the territory containing a world location. Uses spatial index for O(1) lookup.
	 * If multiple territories overlap, returns the highest-priority one.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territory At Location"))
	ATerritoryVolume* GetTerritoryAtLocation(const FVector& WorldLocation) const;

	/** Returns all territories whose bounds contain the world location (for overlapping volumes). */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territories At Location"))
	TArray<ATerritoryVolume*> GetTerritoriesAtLocation(const FVector& WorldLocation) const;

	/** Returns all territories whose bounds intersect the given world box. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territories In Box"))
	TArray<ATerritoryVolume*> GetTerritoriesInBox(const FBox& QueryBox) const;

	/** Returns all territories owned by the given faction. O(N) linear scan. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territories Owned By Faction"))
	TArray<ATerritoryVolume*> GetTerritoriesOwnedByFaction(const FGameplayTag& Faction) const;

	/** Returns all registered territories in the world. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get All Territories"))
	TArray<ATerritoryVolume*> GetAllTerritories() const;

	/** Total number of registered territories. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territory Count"))
	int32 GetTerritoryCount() const;

	/** Returns the number of territories owned by a specific faction. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Territory Count For Faction"))
	int32 GetTerritoryCountForFaction(const FGameplayTag& Faction) const;

	/**
	 * Returns all territories whose ParentTerritoryTag matches ParentTag.
	 * Used for city->district and district->property lookups.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Child Territories"))
	TArray<ATerritoryVolume*> GetChildTerritories(const FGameplayTag& ParentTag) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// Delegates — bind from Blueprint to receive registration events
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Fired when a territory is registered (volume BeginPlay).
	 * Params: Territory, bWasUnregistered (true if this replaces a stale entry)
	 */
	UPROPERTY(BlueprintAssignable, Category="Territory|Registry", meta=(DisplayName="On Territory Registered"))
	FOnTerritoryRegistered OnTerritoryRegistered;

	/**
	 * Fired when a territory is unregistered (volume EndPlay).
	 * Params: Territory, bWasUnregistered (always true here)
	 */
	UPROPERTY(BlueprintAssignable, Category="Territory|Registry", meta=(DisplayName="On Territory Unregistered"))
	FOnTerritoryRegistered OnTerritoryUnregistered;

private:
	UPROPERTY()
	TArray<TObjectPtr<ATerritoryVolume>> RegisteredTerritories;

	TMap<FGameplayTag, TWeakObjectPtr<ATerritoryVolume>> TagToTerritoryMap;
	TMap<FGuid, TWeakObjectPtr<ATerritoryVolume>> GUIDToTerritoryMap;
	FTerritorySpatialIndex SpatialIndex;

	FTimerHandle BoundsCheckTimerHandle;
	void PollBoundsChanges();
};
