#pragma once

#include "CoreMinimal.h"
#include "Tales/QuestTask.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryCaptureTask.generated.h"

class ATerritoryVolume;

UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Capture or Lose Territory Task",
		ToolTip="Narrative Task that completes when a Place, District, or City is captured by the requested faction, or when its starting owner loses it."))
class TERRITORYFRAMEWORK_API UTerritoryCaptureTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task|Target",
		meta = (Categories = "Territory",
			ToolTip="Place, District, or City watched by this Narrative quest task. Easy example: Territory.HavenReach.MarketSquare.Blacksmith."))
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task|Ownership",
		meta = (Categories = "Narrative.Factions",
			ToolTip="Faction that must own the Territory. Leave empty to accept any valid owner. Easy example: Narrative.Factions.Heroes means only a Heroes capture completes the task."))
	FGameplayTag RequiredCapturingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task|Ownership",
		meta=(ToolTip="Complete when the owner present at task start loses control. Easy example: start this while Bandits own Blacksmith, then complete when Bandits lose it."))
	bool bCompleteOnLoss = false;

protected:
	virtual void BeginTask() override;
	virtual void EndTask() override;

	FText GetTaskDescription_Implementation() const override;
	FText GetTaskProgressText_Implementation() const override;
	FVector GetNavigationMarkerLocation_Implementation() const override;
	AActor* GetNavigationMarkerAttachActor_Implementation() const override;

private:
	UFUNCTION()
	void OnTerritoryControlChanged(ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner);

	UFUNCTION()
	void OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UFUNCTION()
	void OnTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered);

	UPROPERTY()
	TWeakObjectPtr<ATerritoryVolume> CachedTerritory;

	/** Faction that owned the territory when the task started (for loss detection) */
	FGameplayTag InitialOwner;

	/** True while the runtime actor is streamed out and the task waits for its replacement. */
	bool bWaitingForRegistration = false;
};
