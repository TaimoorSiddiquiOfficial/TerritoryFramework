#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "TerritoryDiplomacySubsystem.generated.h"

class ANarrativeGameState;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryDiplomacySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void DeclareWar(FGameplayTag FactionA, FGameplayTag FactionB);

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void DeclarePeace(FGameplayTag FactionA, FGameplayTag FactionB);

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void FormAlliance(FGameplayTag FactionA, FGameplayTag FactionB);

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void BreakAlliance(FGameplayTag FactionA, FGameplayTag FactionB);

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void SignTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB, float DurationGameTime = -1.f);

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void SetDiplomacyState(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	EDiplomacyState GetDiplomacyState(FGameplayTag FactionA, FGameplayTag FactionB) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	bool IsAtWar(FGameplayTag FactionA, FGameplayTag FactionB) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	bool IsAllied(FGameplayTag FactionA, FGameplayTag FactionB) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	bool HasTradeAgreement(FGameplayTag FactionA, FGameplayTag FactionB) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void AddReputation(FGameplayTag Faction, int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void SetReputation(FGameplayTag Faction, int32 Value);

	UFUNCTION(BlueprintPure, Category = "Territory|Diplomacy")
	int32 GetReputation(FGameplayTag Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	TArray<FTreatyRecord> GetAllTreaties() const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	TArray<FTreatyRecord> GetTreatiesForFaction(FGameplayTag Faction) const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	TArray<FDiplomacyEvent> GetDiplomacyHistory() const;

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void SyncToGameState();

	UFUNCTION(BlueprintCallable, Category = "Territory|Diplomacy")
	void LoadFromGameState();

	/** Direct Narrative attitude setter — Narrative is sole authority for AI attitudes */
	void SetNarrativeAttitude(FGameplayTag FactionA, FGameplayTag FactionB, ETeamAttitude::Type Attitude);

	/** Convert Narrative attitude back to treaty state */
	EDiplomacyState AttitudeToDiplomacyState(ETeamAttitude::Type Attitude) const;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Diplomacy")
	FOnDiplomacyStateChanged OnDiplomacyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Diplomacy")
	FOnDiplomacyEvent OnDiplomacyEvent;

	UPROPERTY(BlueprintAssignable, Category = "Territory|Diplomacy")
	FOnReputationChanged OnReputationChanged;

protected:
	UPROPERTY(SaveGame)
	TArray<FTreatyRecord> ActiveTreaties;

	UPROPERTY(SaveGame)
	TMap<FGameplayTag, int32> FactionReputation;

	UPROPERTY(SaveGame)
	TArray<FDiplomacyEvent> DiplomacyHistory;

private:
	FTreatyRecord* FindTreaty(FGameplayTag FactionA, FGameplayTag FactionB);
	const FTreatyRecord* FindTreaty(FGameplayTag FactionA, FGameplayTag FactionB) const;
	void RemoveTreaty(FGameplayTag FactionA, FGameplayTag FactionB);
	void RecordEvent(EDiplomacyEventType EventType, FGameplayTag FactionA, FGameplayTag FactionB);
	ANarrativeGameState* GetNarrativeGameState() const;
	ETeamAttitude::Type DiplomacyStateToAttitude(EDiplomacyState State) const;

	UFUNCTION()
	void OnFactionAttitudeChanged(FGameplayTag Faction, FGameplayTag OtherFaction, ETeamAttitude::Type NewAttitude);

	void CheckTreatyExpirations();

	FTimerHandle TreatyExpirationTimerHandle;

	UFUNCTION()
	void OnTreatyExpirationTick();
};
