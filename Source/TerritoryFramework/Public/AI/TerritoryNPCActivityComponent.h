#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "TerritoryNPCActivityComponent.generated.h"

/** Uses Narrative's activity system and save records, with bounded generator snapshots. */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent))
class TERRITORYFRAMEWORK_API UTerritoryNPCActivityComponent : public UNPCActivityComponent
{
	GENERATED_BODY()
public:
	/**
	 * Old generator classes to leave out when loading a save. List exact classes only.
	 * First remove them from the NPC's activity configuration and add the replacement
	 * there. This does not stop a generator still used by the current configuration.
	 * Replacement generators keep their own settings; old private data is not copied.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Territory|AI|Save Migration",
		meta=(DisplayName="Old Saved Goal Generators To Ignore"))
	TArray<TSubclassOf<UNPCGoalGenerator>> IgnoredSavedGoalGeneratorClasses;

	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class FTFPerceptionDelivery;
	friend class FTFGuardResponsePolicy;
	struct FDeliveredPerception
	{
		TArray<FAIStimulus> Stimuli;
		ETeamAttitude::Type Attitude = ETeamAttitude::Neutral;
	};
	TMap<TWeakObjectPtr<AActor>, FDeliveredPerception> DeliveredPerception;
	TArray<TWeakObjectPtr<UNPCGoalGenerator>> DeliveryGenerators;
	TWeakObjectPtr<APawn> DeliveryPawn;
	bool bDeliveringPerception = false;
	void RefreshPerceptionAndRescore();
	void RefreshStoredPerception();
};
