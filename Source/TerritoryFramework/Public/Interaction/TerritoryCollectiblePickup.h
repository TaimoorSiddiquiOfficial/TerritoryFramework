#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryCollectibleTypes.h"
#include "TerritoryCollectiblePickup.generated.h"

class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTerritoryCollectibleDefinition;

/** Raised on the server after the campaign recorded this pickup (or found it already collected). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerritoryCollectiblePickedUp, AActor*, Collector,
	const FTerritoryCollectibleFinding&, Finding);

/**
 * One collectible in the world.
 *
 * The pickup owns no collection state of its own: the campaign-wide authority is
 * UTerritoryCollectibleSubsystem, and this actor only reports "the player reached me" to it.
 * That split matters on load — a save reload puts the collectible back on the map, and the
 * actor hides itself again because the authority already holds the finding, instead of asking
 * designers to remember a per-level "already taken" flag.
 *
 * Easy example: place it on a rooftop, set Collectible Tag to Territory.Collectible.CityMap,
 * and leave Source Territory empty when the Dossier should only show the discovery time.
 */
UCLASS(BlueprintType, Blueprintable, Placeable,
	meta=(DisplayName="Territory Collectible Pickup",
		ToolTip="A collectible the campaign records once. The authoritative record lives in the Territory collectible subsystem, which is saved and replicated; this actor only reports the pickup and hides itself after the finding exists."))
class TERRITORYFRAMEWORK_API ATerritoryCollectiblePickup : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryCollectiblePickup();

	/**
	 * Which authored collectible this actor stands for. Must be a child of Territory.Collectible;
	 * a tag outside that family is refused instead of silently recorded.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Collectible",
		meta=(Categories="Territory.Collectible",
			ToolTip="Authored collectible tag, e.g. Territory.Collectible.CityMap. Easy example: leave this empty and set Definition instead, and the definition's tag is used."))
	FGameplayTag CollectibleTag;

	/**
	 * Optional authored definition. When set it supplies the tag (and therefore the Dossier's
	 * name, description and icon) so a level never has to retype the tag by hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Collectible",
		meta=(ToolTip="Optional collectible definition asset. Easy example: DA_Collectible_CityMap; the definition's Collectible Tag is used when Collectible Tag above is empty."))
	TObjectPtr<UTerritoryCollectibleDefinition> Definition;

	/**
	 * Territory tag shown in the Dossier as where the item was found. Leave empty when the
	 * discovery time alone is the honest answer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Collectible",
		meta=(Categories="Territory",
			ToolTip="Optional territory tag recorded as the find location, e.g. Territory.HavenReach.MarketSquare.Blacksmith. Easy example: fill this when the Dossier should say 'Found in Blacksmith'."))
	FGameplayTag SourceTerritory;

	/** Collect when a player pawn overlaps the pickup sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Collectible",
		meta=(ToolTip="On by default. Disable when a Blueprint or interaction UI should call Collect instead, for example a pickup that needs a key press."))
	bool bCollectOnOverlap = true;

	/**
	 * Destroy the actor after a successful pickup. Turn this off when the level should keep the
	 * actor (for example to play a VFX or hand out a Blueprint-driven reward first).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Collectible",
		meta=(ToolTip="On by default. Disable when the pickup should stay in the level after being recorded, for example to fade out under Blueprint control."))
	bool bDestroyWhenCollected = true;

	/** Recorded after a successful pickup, so Blueprints can react (VFX, sound, UI). */
	UPROPERTY(BlueprintAssignable, Category="Territory|Collectible")
	FOnTerritoryCollectiblePickedUp OnCollected;

	/**
	 * Records this collectible for the campaign. Server only: a client call fails instead of
	 * pretending, because the caller has no authority over the saved finding window.
	 *
	 * A repeat (the campaign already holds this collectible) reports bAlreadyCollected and also
	 * bSuccess, because the end state the caller asked for already holds; it writes nothing,
	 * broadcasts nothing and does not destroy the actor.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Collectible",
		meta=(ToolTip="Records this pickup on the server. Safe to call more than once; a repeat reports bAlreadyCollected instead of adding a second finding."))
	FTerritoryCollectibleMutationResult Collect(AActor* Collector);

	/** True when the campaign already holds this collectible's finding. */
	UFUNCTION(BlueprintPure, Category="Territory|Collectible")
	bool IsCollected() const;

	/** The tag this pickup records: the authored tag, or the definition's tag when it is empty. */
	UFUNCTION(BlueprintPure, Category="Territory|Collectible")
	FGameplayTag GetCollectibleTag() const;

	/** Whether this collector is a pawn the pickup would record for. */
	UFUNCTION(BlueprintPure, Category="Territory|Collectible")
	bool CanBeCollectedBy(const AActor* Collector) const;

	/** Hides or shows the pickup body to match the campaign's finding window. */
	UFUNCTION(BlueprintCallable, Category="Territory|Collectible",
		meta=(ToolTip="Refreshes the visible body from the saved findings. Called on BeginPlay; call it again after a Blueprint changes the tag or definition at runtime."))
	void RefreshCollectedState();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	/** Visible body: a mesh the designer can replace with any art. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Collectible", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Overlap volume that decides "the player reached it". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Collectible", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> PickupSphere;

	/** True once this instance recorded a finding, so the sphere cannot fire twice in one frame. */
	bool bRecordedThisInstance = false;
};
