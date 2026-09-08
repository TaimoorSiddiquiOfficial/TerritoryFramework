#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TerritoryCapturePoint.generated.h"

class ATerritoryVolume;
class UNarrativeAbilitySystemComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTerritoryPlaceDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnTerritoryCaptureParticipantChanged,
	AActor*, Participant,
	ATerritoryVolume*, Territory,
	bool, bRegistered);

/**
 * Server-authoritative physical capture zone for an independently capturable Place.
 * It adapts player overlap and Narrative faction/death state into the existing
 * UTerritoryControlSubsystem participant API. It never owns capture progress or ownership.
 */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryCapturePoint : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryCapturePoint();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Internal/editor synchronization hook. OnConstruction applies the serialized binding. */
	bool ApplyPlaceDefinition();
	UTerritoryPlaceDefinition* GetPlaceDefinition() const { return PlaceDefinition; }
	void SetPlaceDefinition(UTerritoryPlaceDefinition* NewDefinition)
	{
		PlaceDefinition = NewDefinition;
	}

	/** Return the stable Territory tag identifying this target. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture")
	FGameplayTag GetTargetTerritoryTag() const { return TargetTerritoryTag; }

	/** Return this physical capture point's radius in centimetres. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture")
	float GetCaptureRadius() const { return CaptureRadius; }

	/** Stable tag of the independent Place controlled by this physical zone. */
	UPROPERTY(Transient)
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(Transient)
	float CaptureRadius = 350.f;

	UPROPERTY(Transient)
	bool bCaptureEnabled = true;

	/** Physical overlap volume used to register eligible participants for automatic flag or hold-zone capture. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Capture")
	TObjectPtr<USphereComponent> CaptureZone;

	/**
	 * Optional world flag, beacon, or other mesh showing the physical capture location.
	 * Assign any project mesh in a Blueprint child. The mesh never owns capture state.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Capture|Presentation")
	TObjectPtr<UStaticMeshComponent> CaptureMarkerMesh;

	/** Hide the world marker while the target Place is story-Locked or unavailable. */
	UPROPERTY(Transient)
	bool bHideMarkerWhileCaptureUnavailable = true;

	/** Called when this capture point adds or removes a participant. Use it to refresh nearby capture UI. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Capture")
	FOnTerritoryCaptureParticipantChanged OnCaptureParticipantChanged;

	/** Find the loaded Territory linked to this capture point. Returns empty while no matching actor is available. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture")
	ATerritoryVolume* ResolveTargetTerritory() const;

	/** Return current capture progress from 0 to 1. A value of 0.5 means half complete. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture")
	float GetCaptureProgress() const;

	/** Return the faction currently represented by this capture contest. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture")
	FGameplayTag GetContestingFaction() const;

	/** Explicit server hook for custom interaction volumes. Faction is always resolved from Narrative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Capture")
	bool TryRegisterCaptureParticipant(AActor* Participant);

	/** Remove this actor's participation at the capture point so it no longer contributes pressure here. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Capture")
	void UnregisterCaptureParticipant(AActor* Participant);

	/** Check whether the supplied actor is already registered at this capture point. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture")
	bool IsCaptureParticipantRegistered(const AActor* Participant) const;

	/** True only when this point has a valid non-story Territory target. */
	UFUNCTION(BlueprintPure, Category="Territory|Capture",
		meta=(DisplayName="Is Automatic Capture Flow Active"))
	bool IsAutomaticCaptureFlowActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Hidden serialized binding maintained by the Definition synchronizer. */
	UPROPERTY()
	TObjectPtr<UTerritoryPlaceDefinition> PlaceDefinition;

	struct FParticipantRegistration
	{
		TWeakObjectPtr<ATerritoryVolume> Territory;
		TWeakObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystem;
		FGameplayTag Faction;
	};

	TSet<TWeakObjectPtr<AActor>> OverlappingParticipants;
	TMap<TWeakObjectPtr<AActor>, FParticipantRegistration> Registrations;

	UFUNCTION()
	void HandleCaptureZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCaptureZoneEndOverlap(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleParticipantDeathStateChanged(AActor* Participant,
		UNarrativeAbilitySystemComponent* AbilitySystem, bool bIsDead);

	void ReconcileOverlappingParticipants();
	UNarrativeAbilitySystemComponent* ResolveParticipantAbilitySystem(AActor* Participant) const;
	bool IsEligiblePlayerParticipant(AActor* Participant) const;
	void RefreshCaptureMarkerVisibility();
};
