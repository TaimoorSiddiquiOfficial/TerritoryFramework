#pragma once

#include "CoreMinimal.h"
#include "Tales/QuestTask.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryCaptureTask.generated.h"

class ATerritoryVolume;

UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Capture or Lose Territory Task"))
class TERRITORYFRAMEWORK_API UTerritoryCaptureTask : public UNarrativeTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task",
		meta = (Categories = "Territory",
			ToolTip="Place, District, or City watched by this Narrative quest task. Easy example: Territory.HavenReach.MarketSquare.Blacksmith."))
	FGameplayTag TargetTerritoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task",
		meta = (Categories = "Narrative.Factions",
			ToolTip="Faction that must own the Territory. Leave empty to accept the first valid new owner."))
	FGameplayTag RequiredCapturingFaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Territory Task",
		meta=(ToolTip="Complete when the owner present at task start loses control. Use this for a defend-location failure or consequence branch."))
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
