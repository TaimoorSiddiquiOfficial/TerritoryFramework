#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryVolume.generated.h"

class UNarrativeAbilitySystemComponent;
class UShapeComponent;
class UNPCDefinition;
class ATerritoryGuardCharacter;

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryVolume : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ATerritoryVolume();

	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FGameplayTag GetOwningFaction() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	ETerritoryState GetTerritoryState() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	float GetControlProgress() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	bool IsContested() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	bool IsOwnedByFaction(const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FGameplayTag GetTerritoryTag() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FText GetTerritoryDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	int32 GetMaxConcurrentAttackers() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	int32 GetDefenderCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	int32 GetPeriodicIncome() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FBox GetTerritoryBounds() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	bool ContainsPoint(const FVector& WorldPoint) const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FGameplayTag GetParentTerritoryTag() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory")
	void SetOwningFaction(const FGameplayTag& NewFaction);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory")
	void SetControlProgress(float Progress);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory")
	void SetTerritoryState(ETerritoryState NewState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory")
	void RegisterDefender(AActor* Defender);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory")
	void UnregisterDefender(AActor* Defender);

	UFUNCTION(BlueprintCallable, Category = "Territory")
	TArray<AActor*> GetRegisteredDefenders() const;

	UPROPERTY(BlueprintAssignable, Category = "Territory")
	FOnTerritoryControlChanged OnTerritoryControlChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory")
	FOnTerritoryStateChanged OnTerritoryStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	UFUNCTION()
	void OnRep_OwnershipData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory",
		meta = (Categories = "Territory"))
	FGameplayTag TerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory")
	FText TerritoryDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag InitialOwningFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory")
	int32 InitialMaxConcurrentAttackers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory")
	int32 InitialPeriodicIncome = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory")
	int32 InitialGuardCost = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory")
	bool bStartsLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Hierarchy",
		meta = (Categories = "Territory"))
	FGameplayTag ParentTerritoryTag;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_OwnershipData)
	FTerritoryOwnershipData OwnershipData;

	/** Cached previous owner — set before rep overwrites OwnershipData, used in OnRep */
	FGameplayTag PreviousOwningFaction;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Territory|Identity",
		meta = (DisplayName = "Territory GUID (auto-generated)"))
	FGuid TerritoryGUID;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredDefenders;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Visual")
	TObjectPtr<UShapeComponent> BoundsShape;

	// ─── Guard Spawning ───

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Guards",
		meta = (AllowedClasses = "/Script/NarrativeArsenal.NPCDefinition"))
	TObjectPtr<UNPCDefinition> GuardNPCDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Guards")
	int32 GuardSpawnCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Guards")
	float GuardSpawnRadius = 500.f;

	/** Guard spawn points within this territory. If empty, uses random positions within BoundsShape */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory|Guards",
		meta = (AllowedClasses = "/Script/TerritoryFramework.TerritoryGuardSpawnPoint"))
	TArray<TObjectPtr<AActor>> GuardSpawnPoints;

public:
	UFUNCTION(BlueprintCallable, Category = "Territory|Guards")
	void SpawnGuards();

	UFUNCTION(BlueprintCallable, Category = "Territory|Guards")
	void DespawnGuards();

	UFUNCTION(BlueprintPure, Category = "Territory|Guards")
	int32 GetSpawnedGuardCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Guards")
	bool HasGuardsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Guards")
	TArray<class ATerritoryGuardSpawnPoint*> GetGuardSpawnPoints() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Territory")
	void OnOwnershipChanged(FGameplayTag OldOwner, FGameplayTag NewOwner);
	virtual void OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION(BlueprintNativeEvent, Category = "Territory")
	void OnStateChanged(ETerritoryState OldState, ETerritoryState NewState);
	virtual void OnStateChanged_Implementation(ETerritoryState OldState, ETerritoryState NewState);

private:
	UFUNCTION()
	void OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC);

	void BindDefenderDeath(AActor* Defender);
	void UnbindDefenderDeath(AActor* Defender);
	void CleanupInvalidDefenders();

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryGuardCharacter>> SpawnedGuards;

	FVector GetRandomSpawnPoint() const;
};
