#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryInterfaces.h"
#include "TerritoryVolume.generated.h"

class UNarrativeAbilitySystemComponent;
class UShapeComponent;
class UNPCDefinition;
class ATerritoryGuardCharacter;
class ATerritoryGuardSpawnPoint;
class UTerritoryNavigationMarkerComponent;

UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryVolume : public AActor, public INarrativeSavableActor, public ITerritoryOwnershipInterface, public ITerritoryEventReceiverInterface
{
	GENERATED_BODY()
	friend class UTerritoryDataValidator;

public:
	ATerritoryVolume();

	// ─── INarrativeSavableActor ───
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

	// ─── ITerritoryOwnershipInterface ───
	virtual FGameplayTag GetTerritoryOwner_Implementation() const override;
	virtual float GetTerritoryControlProgress_Implementation() const override;
	virtual bool IsTerritoryContested_Implementation() const override;
	virtual FGameplayTag GetContestingFaction_Implementation() const override;

	// ─── ITerritoryEventReceiverInterface ───
	virtual void OnTerritoryControlChanged_Implementation(FGameplayTag TerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner) override;
	virtual void OnTerritoryContested_Implementation(FGameplayTag TerritoryTag, FGameplayTag ContestingFaction) override;
	virtual void OnTerritoryUncontested_Implementation(FGameplayTag TerritoryTag) override;
	virtual void OnTerritoryStateChanged_Implementation(FGameplayTag TerritoryTag, ETerritoryState NewState) override;

	// ─── Query API (BlueprintPure) ───

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
	int32 GetGuardCost() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FBox GetTerritoryBounds() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	bool ContainsPoint(const FVector& WorldPoint) const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FGameplayTag GetParentTerritoryTag() const;

	UFUNCTION(BlueprintPure, Category = "Territory")
	FGameplayTag GetInitialOwningFaction() const;

	// ─── Mutation API (BlueprintAuthorityOnly) ───

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

	/** Set contesting faction (authority only). Used by capture subsystem. */
	void SetContestingFaction(const FGameplayTag& Faction) { OwnershipData.ContestingFaction = Faction; }

	// ─── Blueprint Events (BlueprintNativeEvent) ───

	UFUNCTION(BlueprintNativeEvent, Category = "Territory")
	void OnOwnershipChanged(FGameplayTag OldOwner, FGameplayTag NewOwner);
	virtual void OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION(BlueprintNativeEvent, Category = "Territory")
	void OnStateChanged(ETerritoryState OldState, ETerritoryState NewState);
	virtual void OnStateChanged_Implementation(ETerritoryState OldState, ETerritoryState NewState);

	UFUNCTION(BlueprintNativeEvent, Category = "Territory|Guards")
	void OnAllGuardsDefeated();
	virtual void OnAllGuardsDefeated_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Territory")
	void OnTerritoryInitialized();
	virtual void OnTerritoryInitialized_Implementation();

	// ─── Blueprint Delegates ───

	UPROPERTY(BlueprintAssignable, Category = "Territory")
	FOnTerritoryControlChanged OnTerritoryOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory")
	FOnTerritoryStateChanged OnTerritoryStateChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Guards")
	FOnAllGuardsDefeated OnAllGuardsDefeatedDelegate;

	// ─── Lock System API ───

	/** Returns true if the territory is currently locked (can't be captured). */
	UFUNCTION(BlueprintPure, Category = "Territory|Lock")
	bool IsLocked() const;

	/** Lock the territory. Server-only. Optional reason shown in UI. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Lock")
	void LockTerritory(const FText& Reason = FText());

	/** Unlock the territory if all LockConditions pass (or bForce=true). Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Lock")
	bool TryUnlock(bool bForce = false);

	/** Check if all LockConditions currently pass without actually unlocking. */
	UFUNCTION(BlueprintPure, Category = "Territory|Lock")
	bool CanUnlock() const;

	/** Get the lock reason text. Returns empty if not locked. */
	UFUNCTION(BlueprintPure, Category = "Territory|Lock")
	FText GetLockReason() const { return OwnershipData.LockReason; }

	// ─── Guard Spawning API ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Guards")
	void SpawnGuards();

	/** Spawn exactly one guard at the given spawn point (reserve replacement). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Guards")
	void SpawnSingleGuard(class ATerritoryGuardSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Guards")
	void DespawnGuards();

	UFUNCTION(BlueprintPure, Category = "Territory|Guards")
	int32 GetSpawnedGuardCount() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Guards")
	bool HasGuardsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Territory|Guards")
	TArray<ATerritoryGuardSpawnPoint*> GetGuardSpawnPoints() const;

	/** Returns the auto-created navigation marker component, if any. */
	UFUNCTION(BlueprintPure, Category = "Territory|Visual")
	UTerritoryNavigationMarkerComponent* GetMapMarkerComponent() const;

	/** Returns a human-readable debug string for this territory (owner, state, progress, guards). */
	UFUNCTION(BlueprintPure, Category = "Territory|Debug")
	FString GetDebugString() const;

	/** Re-indexes spatial grid if bounds have changed. Called by registry poll. */
	void CheckBoundsForReindex();

	/** Resolve the NPC definition for a given faction — checks FactionGuardDefinitions first. */
	UNPCDefinition* ResolveGuardDefinition(const FGameplayTag& Faction) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	UFUNCTION()
	void OnRep_OwnershipData();

	// ─── Editable Properties (BlueprintReadWrite for BP access) ───

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory",
		meta = (Categories = "Territory"))
	FGameplayTag TerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory")
	FText TerritoryDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag InitialOwningFaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory")
	int32 InitialMaxConcurrentAttackers = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory")
	int32 InitialPeriodicIncome = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory")
	int32 InitialGuardCost = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory")
	bool bStartsLocked = false;

	// ─── Lock System ───

	/** Narrative conditions that must ALL pass for this territory to be unlockable.
	 *  Checked by TryUnlock(). If empty, territory can always be unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Territory|Lock",
		meta = (EditCondition = "bStartsLocked"))
	TArray<TObjectPtr<class UNarrativeCondition>> LockConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Hierarchy",
		meta = (Categories = "Territory"))
	FGameplayTag ParentTerritoryTag;

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_OwnershipData, BlueprintReadWrite)
	FTerritoryOwnershipData OwnershipData;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Territory|Identity",
		meta = (DisplayName = "Territory GUID (auto-generated)"))
	FGuid TerritoryGUID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Visual")
	TObjectPtr<UShapeComponent> BoundsShape;

	/** Navigation marker component — manages map marker, auto-refreshes on state changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Territory|Visual")
	TObjectPtr<UTerritoryNavigationMarkerComponent> MapMarkerComponent;

	// ─── Guard Configuration ───

	/** Default guard definition — used when no faction-specific entry matches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards",
		meta = (AllowedClasses = "/Script/NarrativeArsenal.NPCDefinition"))
	TObjectPtr<UNPCDefinition> GuardNPCDefinition;

	/** Per-faction guard definitions. When territory owner changes, guards
	 *  spawn using the definition for the new owner's faction. Falls back
	 *  to GuardNPCDefinition if no matching entry exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards",
		meta = (TitleProperty = "{Faction}"))
	TArray<FTerritoryFactionGuardDefinition> FactionGuardDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards")
	int32 GuardSpawnCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards")
	float GuardSpawnRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory|Guards",
		meta = (AllowedClasses = "/Script/TerritoryFramework.TerritoryGuardSpawnPoint"))
	TArray<TObjectPtr<AActor>> GuardSpawnPoints;

	// ─── Guard Spawn Point Delegate (Blueprint) ───

	UPROPERTY(BlueprintAssignable, Category = "Territory|Guards")
	FOnGuardKilled OnGuardKilled;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RegisteredDefenders;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATerritoryGuardCharacter>> SpawnedGuards;

	FGameplayTag PreviousOwningFaction;
	ETerritoryState PreviousState = ETerritoryState::Unclaimed;

	/** Cached bounds for change detection. */
	FBox LastKnownBounds;

	UFUNCTION()
	void OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC);

	void BindDefenderDeath(AActor* Defender);
	void UnbindDefenderDeath(AActor* Defender);
	void CleanupInvalidDefenders();

	FVector GetRandomSpawnPoint() const;
};
