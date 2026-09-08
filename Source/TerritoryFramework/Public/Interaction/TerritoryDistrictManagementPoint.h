#pragma once

#include "CoreMinimal.h"
#include "Navigation/POIActor.h"
#include "Navigation/MapMarker.h"
#include "Navigation/NavigationMarkerComponent.h"
#include "Interaction/InteractableComponent.h"
#include "GameplayTagContainer.h"
#include "TerritoryDistrictManagementPoint.generated.h"

class ATerritoryDistrict;
class UTerritoryDistrictManagementWidget;
class USphereComponent;
class UTerritoryDefinition;

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryDistrictPOIMarker : public UMapMarker
{
	GENERATED_BODY()

public:
	UTerritoryDistrictPOIMarker(const FObjectInitializer& ObjectInitializer);
	void SetManagementPoint(class ATerritoryDistrictManagementPoint* Point);
	void ClearManagementPoint();
	void RefreshPresentationPolicy();

	virtual FText GetMarkerActionText_Implementation(UNarrativeNavigationComponent* Selector) const override;
	virtual FText GetMarkerDisplayText_Implementation(UNarrativeNavigationComponent* Selector,
		const FGameplayTag& NavigatorType, FText& OutSubtitleText) const override;
	virtual FLinearColor GetMarkerColor_Implementation(UNarrativeNavigationComponent* Selector,
		const FGameplayTag& NavigatorType) const override;
	virtual bool CanInteract_Implementation(UNarrativeNavigationComponent* Selector) const override;
	virtual void OnSelect_Implementation(UNarrativeNavigationComponent* Selector) override;

private:
	TWeakObjectPtr<class ATerritoryDistrictManagementPoint> ManagementPoint;
	bool bRegisteredWithNavigation = false;
};

UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryDistrictNavigationMarkerComponent : public UNavigationMarkerComponent
{
	GENERATED_BODY()

public:
	UTerritoryDistrictNavigationMarkerComponent(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnTerritoryStateChanged(ATerritoryVolume* Territory, ETerritoryState NewState);

	UFUNCTION()
	void OnTerritoryOwnershipChanged(ATerritoryVolume* Territory,
		FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryAvailabilityChanged(ATerritoryVolume* Territory,
		ETerritoryAvailability NewAvailability);

	UFUNCTION()
	void OnRegistryTerritoryChanged(ATerritoryVolume* Territory, bool bWasUnregistered);

private:
	UPROPERTY(Transient)
	TObjectPtr<UTerritoryDistrictPOIMarker> DistrictMarker;

	TWeakObjectPtr<ATerritoryDistrict> BoundDistrict;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryVolume>> BoundAvailabilityAncestors;

	void BindToDistrictIfAvailable();
	void UnbindFromDistrict();
	void RefreshAncestorAvailabilityBindings();
	void RefreshMarkerPolicy();
};

UCLASS(ClassGroup=(Territory), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryDistrictInteractableComponent : public UNarrativeInteractableComponent
{
	GENERATED_BODY()

public:
	virtual FText GetInteractableNameText_Implementation(APawn* Interactor,
		UNarrativeInteractionComponent* InteractionComp) const override;
	virtual FText GetInteractableActionText_Implementation(APawn* Interactor,
		UNarrativeInteractionComponent* InteractionComp) const override;

protected:
	virtual bool CanInteract_Implementation(APawn* Interactor,
		UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText) override;
	virtual void OnInteract_Implementation(APawn* Interactor,
		UNarrativeInteractionComponent* InteractionComp) override;
};

/** Project-owned Narrative POI and interactable used to manage a captured District. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryDistrictManagementPoint : public APOIActor
{
	GENERATED_BODY()

public:
	ATerritoryDistrictManagementPoint(const FObjectInitializer& ObjectInitializer);
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Internal/editor synchronization hook. OnConstruction applies the serialized binding. */
	bool ApplyTerritoryDefinition();
	UTerritoryDefinition* GetTerritoryDefinition() const { return TerritoryDefinition; }
	void SetTerritoryDefinition(UTerritoryDefinition* NewDefinition)
	{
		TerritoryDefinition = NewDefinition;
	}

	/** Return the stable tag of the District managed by this interaction. */
	UFUNCTION(BlueprintPure, Category="Territory|Management")
	FGameplayTag GetManagedDistrictTag() const { return DistrictTag; }

	/** Return the allowed management interaction distance in centimetres. */
	UFUNCTION(BlueprintPure, Category="Territory|Management")
	float GetManagementDistance() const { return ManagementDistance; }

	UPROPERTY(Transient)
	FGameplayTag DistrictTag;

	UPROPERTY(Transient)
	TSubclassOf<UTerritoryDistrictManagementWidget> ManagementWidgetClass;

	/** Narrative gameplay HUD layer used for the management menu. */
	UPROPERTY(Transient)
	FGameplayTag ManagementLayerTag;

	UPROPERTY(Transient)
	float ManagementDistance = 600.f;

	/** Interaction range volume for this Territory management point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Management")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** Narrative interaction component that opens the Territory management UI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Management")
	TObjectPtr<UTerritoryDistrictInteractableComponent> InteractableComponent;

	/** Narrative navigation marker presenting this management point on supported navigation views. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Management")
	TObjectPtr<UTerritoryDistrictNavigationMarkerComponent> DistrictMarkerComponent;

	/** Find the loaded District managed by this interaction. Returns empty if it is not available. */
	UFUNCTION(BlueprintPure, Category="Territory|Management")
	ATerritoryDistrict* ResolveDistrict() const;

	/** Check whether this player can manage the linked District under current ownership and availability rules. */
	UFUNCTION(BlueprintPure, Category="Territory|Management")
	bool CanManage(APawn* Interactor, FText& OutFailureReason) const;

	/** Check whether the supplied actor is close enough to use this management point. */
	UFUNCTION(BlueprintPure, Category="Territory|Management")
	bool IsInteractorInRange(APawn* Interactor) const;

	/** Open the configured District management screen for this player through Narrative's UI stack. */
	UFUNCTION(BlueprintCallable, Category="Territory|Management")
	void OpenManagementWidget(APlayerController* PlayerController);

	void HandleInteraction(APawn* Interactor);

private:
	/** Hidden serialized binding maintained by the Definition synchronizer. */
	UPROPERTY()
	TObjectPtr<UTerritoryDefinition> TerritoryDefinition;
};
