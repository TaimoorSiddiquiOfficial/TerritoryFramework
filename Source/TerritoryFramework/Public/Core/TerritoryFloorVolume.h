#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TerritoryFloorVolume.generated.h"

class UBoxComponent;
class UTerritoryPlaceDefinition;

/**
 * Authored level region that claims one floor of one Place.
 *
 * A floor row on UTerritoryPlaceDefinition is only an integer grouping key. FTerritoryFloorTemplate
 * has no volume, bounds, box, anchor or transform, so a floor row by itself cannot answer "is this
 * actor standing on that floor". This actor supplies exactly that missing geometry: a row plus at
 * least one volume bound to it is what turns a floor from a label into an enforced separation.
 *
 * Binding mirrors the guard-post row/actor split. The author picks the Place asset and a floor
 * index here; the transient OwnerTerritoryTag and the registry entry are derived from that choice,
 * never authored directly.
 *
 * Runtime behaviour is inert until a volume exists. A Place with floor rows and no bound volume
 * resolves every location to INDEX_NONE, and the engagement gate treats an unresolved floor as
 * "no separation declared" and lets the fight proceed exactly as it does today.
 */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Territory Floor Volume"))
class TERRITORYFRAMEWORK_API ATerritoryFloorVolume : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryFloorVolume();

	/**
	 * Region this floor occupies. Containment is transform-space, so a rotated floor volume is
	 * honoured exactly like ATerritoryVolume::BoundsShape and needs no collision geometry.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Floor")
	TObjectPtr<UBoxComponent> FloorBounds;

	/**
	 * Which Place this region belongs to. Floors are authored on a Place and nowhere else, so the
	 * property type is the Place subclass rather than the shared base: a City or a District cannot
	 * be bound here at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Floor",
		meta=(ToolTip="The Place Definition whose floor this region claims. Floors exist only on Places; a City or District cannot own defenders and so cannot own a floor."))
	TObjectPtr<UTerritoryPlaceDefinition> PlaceDefinition;

	/**
	 * Which authored floor row of PlaceDefinition this region claims. Zero is ground; upper floors
	 * are positive.
	 *
	 * Deliberately not clamped. An index naming no row is an authoring error that data validation
	 * reports, and the volume keeps the floor it names rather than being silently moved to ground -
	 * the same rule ATerritoryGuardSpawnPoint::ApplyTerritoryDefinition applies to a post.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Floor",
		meta=(ToolTip="Authored floor row this region claims. Must name a row in the Place Definition's Floors list, or data validation errors."))
	int32 FloorIndex = 0;

	/**
	 * Stable editor-baked identity. It is read for one thing: a deterministic total order when two
	 * volumes claim the same floor of the same Place and their regions overlap, so the resolved
	 * answer cannot depend on actor iteration order, which World Partition streaming does not
	 * preserve. It is NOT save state - a floor volume persists nothing, and this GUID is never
	 * written to or read from a campaign save.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Floor")
	FGuid FloorVolumeGUID;

	/**
	 * Resolved at runtime from PlaceDefinition. Invalid until ApplyFloorVolumeDefinition runs, and
	 * the volume is not registered while it is invalid.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory|Floor")
	FGameplayTag OwnerTerritoryTag;

	// ─── Binding ───

	/**
	 * Derives OwnerTerritoryTag from PlaceDefinition. Returns false when no Place asset is bound or
	 * it carries no TerritoryTag, which is an authoring error the caller reports.
	 */
	bool ApplyFloorVolumeDefinition();

	UTerritoryPlaceDefinition* GetPlaceDefinition() const { return PlaceDefinition; }
	FGameplayTag GetOwnerTerritoryTag() const { return OwnerTerritoryTag; }

	// ─── Geometry ───

	/**
	 * Transform-space containment. Returns false when FloorBounds is unset, matching
	 * ATerritoryVolume::ContainsPoint's behaviour for a missing BoundsShape: an unshaped volume
	 * claims no space rather than all of it.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Floor", meta=(DisplayName="Contains Point"))
	bool ContainsPoint(const FVector& WorldPoint) const;

	/** Unrotated box size. Zero when unshaped. */
	FVector GetFloorBoundsSize() const;

	/** Signed box volume, used as the registry's "most specific region wins" key. */
	double GetFloorBoundsVolume() const;

	/**
	 * The one rule for "which authored region decides a point's floor", over an explicit candidate
	 * set. Two callers share it so they cannot disagree: the registry, over the volumes it has
	 * registered at runtime, and the authoring validator, over this level's actors - which is the
	 * only form available before Play, because BeginPlay is what registers them.
	 *
	 * Most specific region wins - smallest bounds volume - with the volume GUID breaking an exact
	 * tie. That order is total and independent of actor iteration, which World Partition streaming
	 * does not preserve. Returns null when no candidate contains the point.
	 */
	static const ATerritoryFloorVolume* SelectMostSpecificRegion(
		const TArray<const ATerritoryFloorVolume*>& Candidates, const FVector& WorldLocation);

	/**
	 * SelectMostSpecificRegion's floor index, or INDEX_NONE when no candidate contains the point.
	 * INDEX_NONE is the answer that means "no separation declared": every caller treats it as
	 * permission to proceed rather than as a refusal, which is what keeps an unauthored Place
	 * defending itself exactly as it did before floors could be enforced.
	 */
	static int32 ResolveFloorAtLocation(
		const TArray<const ATerritoryFloorVolume*>& Candidates, const FVector& WorldLocation);

	/** Gives this volume its editor-baked identity when it has none yet. */
	void EnsurePersistentFloorVolumeGUID();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
};
