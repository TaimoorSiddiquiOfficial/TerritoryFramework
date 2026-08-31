#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Core/TerritoryStealthProfile.h"
#include "GameplayEffectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerritoryDisguiseSubsystem.generated.h"

class ATerritoryVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnTerritoryDisguiseChanged,
	AActor*, Target,
	ETerritoryDisguiseChange, Change,
	FGameplayTag, ObserverFaction,
	ATerritoryVolume*, Territory,
	const FTerritoryDisguiseSnapshot&, Snapshot);

/**
 * Server-authoritative perceived identity layer for Territory guards.
 * Real Narrative factions are read but never mutated.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryDisguiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Activates a uniform identity. SourceObject lets an unequipped item remove only itself. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Disguise")
	bool ActivateDisguise(AActor* Target, UTerritoryDisguiseProfile* Profile,
		UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Disguise")
	bool RemoveDisguise(AActor* Target, UObject* SourceObject = nullptr);

	/** Burn the identity for one faction. Empty Observer Faction burns it for everyone. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Disguise")
	bool CompromiseDisguise(AActor* Target,
		UPARAM(meta=(Categories="Narrative.Factions")) FGameplayTag ObserverFaction,
		ATerritoryVolume* Territory = nullptr);

	/** Restore one burned cover identity. Empty Observer Faction restores it globally. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Disguise")
	bool RestoreDisguise(AActor* Target,
		UPARAM(meta=(Categories="Narrative.Factions")) FGameplayTag ObserverFaction);

	UFUNCTION(BlueprintPure, Category="Territory|Disguise")
	bool GetDisguiseSnapshot(const AActor* Target,
		FTerritoryDisguiseSnapshot& OutSnapshot) const;

	/** Returns true when this observer should use the perceived faction instead of the real one. */
	UFUNCTION(BlueprintPure, Category="Territory|Disguise")
	bool IsDisguiseAccepted(const AActor* Target, const ATerritoryVolume* Territory,
		UPARAM(meta=(Categories="Narrative.Factions")) FGameplayTag ObserverFaction,
		FText& OutReason) const;

	/** Pure policy function for previews, validation, and deterministic tests. */
	UFUNCTION(BlueprintPure, Category="Territory|Disguise")
	static bool EvaluateDisguisePolicy(const UTerritoryDisguiseProfile* Disguise,
		const UTerritoryStealthProfile* TerritoryPolicy,
		UPARAM(meta=(Categories="Narrative.Factions")) FGameplayTag OwningFaction,
		UPARAM(meta=(Categories="Narrative.Factions")) FGameplayTag ObserverFaction,
		bool bCompromised, FText& OutReason);

	/** Explicit checkpoint. Sends a GAS event and may burn a failed disguise. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Disguise")
	bool PerformIdentityCheck(AActor* Target, ATerritoryVolume* Territory,
		UPARAM(meta=(Categories="Narrative.Factions")) FGameplayTag ObserverFaction,
		FText& OutReason);

	/** Factions used only for this observer's attitude/dialogue calculation. */
	FGameplayTagContainer ResolvePerceivedFactions(const AActor* Target,
		const ATerritoryVolume* Territory, FGameplayTag ObserverFaction) const;

	/** Central evidence gate. True means ordinary sight is masked by a valid uniform. */
	bool ProcessStealthEvidence(AActor* Target, ATerritoryVolume* Territory,
		AActor* Observer, ETerritoryStealthEvidence Evidence,
		bool bConfirmedIdentity);

	UPROPERTY(BlueprintAssignable, Category="Territory|Disguise")
	FOnTerritoryDisguiseChanged OnDisguiseChanged;

private:
	struct FDisguiseRuntime
	{
		TObjectPtr<UTerritoryDisguiseProfile> Profile = nullptr;
		TWeakObjectPtr<UObject> SourceObject;
		TSet<FGameplayTag> CompromisedFactions;
		bool bCompromisedForEveryone = false;
		FActiveGameplayEffectHandle ActiveTagHandle;
		FActiveGameplayEffectHandle CompromisedTagHandle;
	};

	TMap<TWeakObjectPtr<AActor>, FDisguiseRuntime> ActiveDisguises;

	FGameplayTag ResolveObserverFaction(const ATerritoryVolume* Territory,
		const AActor* Observer) const;
	bool IsCompromisedForFaction(const FDisguiseRuntime& Runtime,
		FGameplayTag ObserverFaction) const;
	FTerritoryDisguiseSnapshot MakeSnapshot(const AActor* Target,
		const FDisguiseRuntime& Runtime) const;
	void RefreshStateTags(AActor* Target, FDisguiseRuntime& Runtime);
	void RemoveStateTags(FDisguiseRuntime& Runtime);
	void EmitChange(AActor* Target, FDisguiseRuntime& Runtime,
		ETerritoryDisguiseChange Change, FGameplayTag ObserverFaction,
		ATerritoryVolume* Territory);
};
