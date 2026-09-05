#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryAuditEventProbe.generated.h"

/** Editor-only receiver for real Blueprint-compatible callback regression tests. */
UCLASS(Transient)
class UTerritoryAuditEventProbe final : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void(ATerritoryVolume*, ETerritoryState)> StateCallback;
	TFunction<void(ATerritoryVolume*, AActor*)> EvidenceCallback;
	TFunction<void(ATerritoryVolume*, AActor*, ETerritoryExposureState)> ExposureCallback;
	TFunction<void(FGameplayTag, FGameplayTag, EDiplomacyState)> DiplomacyCallback;
	TFunction<void(ATerritoryVolume*, bool)> RegistryCallback;
	TFunction<void(const FTerritoryAssaultRecord&)> AssaultCallback;
	TFunction<void(const FTerritoryCounterAttackStateEvent&)> CounterEventCallback;
	TFunction<void(APlayerController*, const FTerritoryAssaultRecord&)> WarningCallback;

	UFUNCTION()
	void AssaultWarning(APlayerController* Controller, const FTerritoryAssaultRecord& Assault)
	{
		if (WarningCallback) WarningCallback(Controller, Assault);
	}

	UFUNCTION()
	void AssaultChanged(const FTerritoryAssaultRecord& Assault)
	{
		if (AssaultCallback) AssaultCallback(Assault);
	}

	UFUNCTION()
	void CounterEvent(const FTerritoryCounterAttackStateEvent& Event)
	{
		if (CounterEventCallback) CounterEventCallback(Event);
	}

	UFUNCTION()
	void RegistryChanged(ATerritoryVolume* Territory, bool bUnregistered)
	{
		if (RegistryCallback) RegistryCallback(Territory, bUnregistered);
	}

	UFUNCTION()
	void DiplomacyChanged(FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState State)
	{
		if (DiplomacyCallback) DiplomacyCallback(FactionA, FactionB, State);
	}

	UFUNCTION()
	void StateChanged(ATerritoryVolume* Territory, ETerritoryState State)
	{
		if (StateCallback) StateCallback(Territory, State);
	}

	UFUNCTION()
	void EvidenceReported(ATerritoryVolume* Territory, AActor* Target,
		ETerritoryStealthEvidence Evidence, const FTerritoryInfiltrationSnapshot& Snapshot)
	{
		if (EvidenceCallback) EvidenceCallback(Territory, Target);
	}

	UFUNCTION()
	void ExposureChanged(ATerritoryVolume* Territory, AActor* Target,
		ETerritoryExposureState OldState, ETerritoryExposureState NewState)
	{
		if (ExposureCallback) ExposureCallback(Territory, Target, NewState);
	}
};

/** Native Narrative condition with a controllable callback for integration regressions. */
UCLASS(Transient)
class UTerritoryAuditCondition final : public UNarrativeCondition
{
	GENERATED_BODY()
public:
	TFunction<bool()> Callback;
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override
	{
		return Callback ? Callback() : false;
	}
};
