#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TerritoryAssaultCharacter.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UTriggerSet;
class UTerritoryAssaultParticipantComponent;

/** Narrative NPC used for finite physical Territory counterattacks. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryAssaultCharacter : public ANarrativeNPCCharacter
{
	GENERATED_BODY()

public:
	ATerritoryAssaultCharacter(const FObjectInitializer& ObjectInitializer);

	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;

	void ConfigureAssaultSpawn(UNPCDefinition* Definition, const FGameplayTag& ExactFaction,
		const FGuid& TerritoryGuid, const FGuid& SpawnGuid, const FTransform& SpawnTransform,
		FName SpawnName, UNPCActivityConfiguration* OptionalActivityOverride,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
		const FGuid& AssaultID, const FGameplayTag& TargetTerritory);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Assault")
	TObjectPtr<UTerritoryAssaultParticipantComponent> AssaultParticipant;

protected:
	virtual bool ShouldRespawn_Implementation() const override;
};
