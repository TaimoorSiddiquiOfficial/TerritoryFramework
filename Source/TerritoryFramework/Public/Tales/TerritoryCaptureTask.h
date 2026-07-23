#pragma once

#include "CoreMinimal.h"
#include "Tales/QuestTask.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryCaptureTask.generated.h"

class ATerritoryVolume;

UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryCaptureTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task",
		meta = (Categories = "Territory"))
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag RequiredCapturingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task")
	bool bCompleteOnLoss = false;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;

	FText GetTaskDescription_Implementation() const override;
	FText GetTaskProgressText_Implementation() const override;

private:
	UFUNCTION()
	void OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> CachedTerritory;

	/** Faction that owned the territory when the task started (for loss detection) */
	FGameplayTag InitialOwner;

	/** True while subscribed to OnTerritoryRegistered for late binding */
	bool bWaitingForRegistration = false;
};
