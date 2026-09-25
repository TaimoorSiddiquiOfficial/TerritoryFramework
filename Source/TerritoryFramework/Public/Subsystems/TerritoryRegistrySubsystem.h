#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritorySpatialIndex.h"
#include "TerritoryRegistrySubsystem.generated.h"

class ATerritoryVolume;
class ATerritoryFloorVolume;

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
	 * Returns Success if registered, or a failure reason if rejected (duplicate tag/GUID, null).
	 *
	 * Binds the volume's ownership + state delegates and adds it to the spatial index.
	 * Repeated registration with the same identity refreshes bounds without replaying events.
	 * Remove an actor before changing its identity. Admission events may withdraw it again.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Registry", meta=(DisplayName="Register Territory"))
	ETerritoryRegistrationResult RegisterTerritory(ATerritoryVolume* Territory);

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
	/**
	 * Every world maintains its own spatial cache. Replicated movement is reindexed
	 * by the periodic local bounds check; call UpdateTerritoryBounds for immediate refresh.
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

	// ═══════════════════════════════════════════════════════════════════════════
	// Floor regions (authored geometry for a Place's floor rows)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Register an authored floor region. Called automatically by ATerritoryFloorVolume::BeginPlay.
	 *
	 * Rejected, with a logged reason, when the volume has no resolved Place tag, no editor-baked
	 * FloorVolumeGUID, a negative FloorIndex, or a FloorVolumeGUID another registered volume already
	 * claims. Rejection is deliberate rather than lenient: a negative index would be indistinguishable
	 * from GetFloorAtLocation's "unresolved" answer, and a duplicated GUID would make the overlap
	 * tie-break depend on iteration order.
	 *
	 * Returns false when the volume was not admitted.
	 */
	bool RegisterFloorVolume(ATerritoryFloorVolume* FloorVolume);

	/** Unregister an authored floor region. Safe on null or unregistered volumes — no-op in those cases. */
	void UnregisterFloorVolume(ATerritoryFloorVolume* FloorVolume);

	/**
	 * Resolves which authored floor of Place contains a world location.
	 *
	 * Returns INDEX_NONE when the location is on no authored floor region of that Place - including
	 * the ordinary case of a Place that declares floor rows but has no floor volumes authored at all.
	 * INDEX_NONE therefore means "no floor separation is declared here", and callers must treat it as
	 * permission to proceed unchanged rather than as an error.
	 *
	 * A Place is never inferred from the location; it must be named, because a City or District volume
	 * spanning the same space owns no floors and would otherwise mask the Place's answer.
	 *
	 * When two regions of the same Place overlap the location, the smaller region wins, with the
	 * volume GUID breaking an exact tie. That order is total and independent of actor iteration,
	 * which World Partition streaming does not preserve.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Floor At Location"))
	int32 GetFloorAtLocation(const ATerritoryVolume* Place, const FVector& WorldLocation) const;

	/** Every registered floor region claiming a floor of this Place. Empty when none are authored. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get Floor Volumes For Place"))
	TArray<ATerritoryFloorVolume*> GetFloorVolumesForPlace(const ATerritoryVolume* Place) const;

	/** True when this Place has at least one registered floor region, i.e. floor separation is enforced. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Has Authored Floor Volumes"))
	bool HasAuthoredFloorVolumes(const ATerritoryVolume* Place) const;

	/** Every registered floor region, for validation and diagnostics. */
	UFUNCTION(BlueprintPure, Category="Territory|Registry", meta=(DisplayName="Get All Floor Volumes"))
	TArray<ATerritoryFloorVolume*> GetAllFloorVolumes() const;

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
	// P2-N06: Weak references to avoid blocking World Partition unload
	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryVolume>> RegisteredTerritories;

	TMap<FGameplayTag, TWeakObjectPtr<ATerritoryVolume>> TagToTerritoryMap;
	TMap<FGuid, TWeakObjectPtr<ATerritoryVolume>> GUIDToTerritoryMap;
	FTerritorySpatialIndex SpatialIndex;

	// Authored floor regions, keyed by the Place tag they claim. Weak references for the same reason
	// the territory lists are weak: World Partition stream-out must not leave a stale entry behind.
	// Keyed by tag rather than by actor because a floor region resolves its Place through the tag, the
	// way ATerritoryGuardSpawnPoint resolves its owner, and never by level-actor identity.
	TMap<FGameplayTag, TArray<TWeakObjectPtr<ATerritoryFloorVolume>>> FloorVolumesByPlaceTag;

	FTimerHandle BoundsCheckTimerHandle;
	void PollBoundsChanges();
};
