#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Core/TerritoryCollectibleTypes.h"
#include "TerritoryCollectibleTileWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UWidget;

/**
 * One collectible tile in the Dossier's Collectibles grid.
 *
 * The tile is a view of FTerritoryCollectibleView: authored name, description and icon
 * joined with the campaign's finding. A collectible the campaign has not found is drawn as
 * hidden rather than as an empty tile, because "not found yet" is the interesting state in
 * a collection screen, and an empty slot that looks identical to a broken one is a bug the
 * player cannot report precisely.
 *
 * It implements IUserObjectListEntry so the same Blueprint can be the entry widget of a
 * UTileView or UListView and still work as a hand-built tile.
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryCollectibleTileWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** Populate this tile from an authored definition joined with the campaign's finding. */
	UFUNCTION(BlueprintCallable, Category = "Territory|Dossier")
	void InitializeCollectibleView(const FTerritoryCollectibleView& InView);

	/** Presentation snapshot this tile currently shows. */
	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	FTerritoryCollectibleView GetCollectibleView() const { return View; }

	/** Name to print: the authored name, or the tag when the definition is missing. */
	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	static FText GetCollectibleNameText(const FTerritoryCollectibleView& InView);

	/** Status line: where and when it was found, or that it is still out there. */
	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	static FText GetCollectibleStatusText(const FTerritoryCollectibleView& InView);

	/** Body text: the authored description, or an honest line when there is none. */
	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	static FText GetCollectibleDescriptionText(const FTerritoryCollectibleView& InView);

	/** Category chip text, or empty when the definition names no family. */
	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	static FText GetCollectibleCategoryText(const FTerritoryCollectibleView& InView);

	/** Wash colour of the tile surface: brighter once the campaign found it. */
	UFUNCTION(BlueprintPure, Category = "Territory|Dossier")
	static FLinearColor GetCollectibleTileColor(const FTerritoryCollectibleView& InView);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	/**
	 * ListView hand-off. A tile used as a view entry receives its item object here; a tile
	 * placed by hand never sees this call and relies on InitializeCollectibleView().
	 */
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** Authored icon slot. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** Authored name. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Where and when it was found, or that it is still hidden. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** Optional family chip. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CategoryText;

	/** Optional body text under the name. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** Tile surface, tinted by GetCollectibleTileColor(). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TileSurface;

	/**
	 * Optional "not found yet" veil (an overlay, a desaturated image, a lock glyph). The
	 * tile shows it for an unfound collectible and collapses it otherwise; a tile authored
	 * without one still reads correctly from its text.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HiddenOverlay;

private:
	UPROPERTY(Transient)
	FTerritoryCollectibleView View;

	void RefreshView();
};
