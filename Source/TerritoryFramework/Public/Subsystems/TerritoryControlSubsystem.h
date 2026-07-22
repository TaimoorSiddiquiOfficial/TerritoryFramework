#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryControlSubsystem.generated.h"

class ATerritoryVolume;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryControlSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ─── Capture API (authority-only mutations) ───

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	ECaptureResult AttemptCapture(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void ResetCapture(ATerritoryVolume* Territory);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void AddCaptureProgress(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction, float ProgressDelta);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void ForceCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner);

	/** Register an actor as an attacker for a faction. Identity-based — duplicates ignored. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	/** Unregister an actor. Removes identity, decrements count only if actor was registered. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Capture")
	void UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction);

	// ─── Query API (read-only, no authority needed) ───

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool IsCaptureInProgress(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	float GetCaptureProgress(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	FGameplayTag GetContestingFaction(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	bool HasAttackBudget(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Capture")
	int32 GetActiveAttackers(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Capture")
	FOnTerritoryControlChanged OnTerritoryControlChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Capture")
	FOnCaptureAttempted OnCaptureAttempted;

private:
	/** Per-faction capture state — attacker tracking is actor-based, not count-based */
	struct FPerTerritoryState
	{
		/** Actor sets per faction — prevents count inflation from duplicate registrations */
		TMap<FGameplayTag, TSet<TWeakObjectPtr<AActor>>> AttackersByFaction;
		TMap<FGameplayTag, float> CaptureProgressByFaction;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritoryState> TerritoryCaptureState;

	/** Deferred commands to apply AFTER iteration to avoid map mutation during range-for */
	struct FDeferredCommand
	{
		enum EType { Complete, Reset, RemoveInvalid };
		EType Type;
		TWeakObjectPtr<ATerritoryVolume> Territory;
		FGameplayTag Faction;
	};
	TArray<FDeferredCommand> DeferredCommands;

	FTimerHandle CaptureTickTimerHandle;

	UFUNCTION()
	void OnCaptureTick();

	void EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime);
	void CompleteCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner);
};
