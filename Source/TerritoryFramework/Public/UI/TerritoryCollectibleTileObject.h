#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryCollectibleTypes.h"
#include "UObject/Object.h"
#include "TerritoryCollectibleTileObject.generated.h"

/**
 * One collectible handed to a UListView or UTileView.
 *
 * A list entry is an object, not a struct, because UMG's list views hand entries to
 * widgets through IUserObjectListEntry and the entry has to outlive the recycle. This is
 * the collectible counterpart of UTerritoryDossierEntryObject: it carries the presentation
 * snapshot only, so the grid never reads the authority while it draws.
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryCollectibleTileObject : public UObject
{
	GENERATED_BODY()

public:
	/** Set by the shell before the tile is handed to the view. */
	void SetCollectibleView(const FTerritoryCollectibleView& InView) { View = InView; }

	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	FTerritoryCollectibleView GetCollectibleView() const { return View; }

	/** True once a real view has been set, so an emptied entry can be told apart. */
	bool HasCollectibleView() const { return View.CollectibleTag.IsValid(); }

private:
	UPROPERTY(Transient)
	FTerritoryCollectibleView View;
};
