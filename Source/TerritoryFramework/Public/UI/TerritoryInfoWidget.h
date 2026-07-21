#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryInfoWidget.generated.h"

class ATerritoryVolume;
class UTerritoryRegistrySubsystem;

/**
 * Base widget for displaying territory information.
 * Extend in Blueprint to create custom territory HUD elements.
 * Auto-binds to territory ownership and state change delegates.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API UTerritoryInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Bind this widget to a specific territory by tag */
	UFUNCTION(BlueprintCallable, Category = "Territory|UI")
	void BindToTerritory(const FGameplayTag& TerritoryTag);

	/** Bind this widget to the territory at the player's current location */
	UFUNCTION(BlueprintCallable, Category = "Territory|UI")
	void BindToTerritoryAtPlayer();

	/** Unbind from the current territory */
	UFUNCTION(BlueprintCallable, Category = "Territory|UI")
	void UnbindFromTerritory();

	/** Get the currently bound territory actor (may be null) */
	UFUNCTION(BlueprintPure, Category = "Territory|UI")
	ATerritoryVolume* GetBoundTerritory() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Called when the bound territory's ownership changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|UI")
	void OnTerritoryOwnershipChanged(FGameplayTag OldOwner, FGameplayTag NewOwner);

	/** Called when the bound territory's state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|UI")
	void OnTerritoryStateChanged(ETerritoryState NewState);

	/** Called when first bound to a territory — populate initial data */
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory|UI")
	void OnTerritoryBound(ATerritoryVolume* Territory);

private:
	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> BoundTerritory;

	FGameplayTag BoundTerritoryTag;

	UFUNCTION()
	void HandleControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void HandleStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState);

	void BindDelegates();
	void UnbindDelegates();
	void ResolveTerritoryFromTag();
};
