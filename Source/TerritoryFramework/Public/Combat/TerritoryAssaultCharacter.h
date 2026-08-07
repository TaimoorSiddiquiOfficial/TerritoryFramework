#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TerritoryAssaultCharacter.generated.h"

class UNPCDefinition;
class UNPCActivityConfiguration;
class UNarrativeCharacterSubsystem;
class UTriggerSet;
class UTerritoryAssaultParticipantComponent;

/** Narrative NPC used for finite physical Territory counterattacks. */
UCLASS(BlueprintType, Blueprintable)
class TERRITORYFRAMEWORK_API ATerritoryAssaultCharacter : public ANarrativeNPCCharacter
{
	GENERATED_BODY()

public:
	ATerritoryAssaultCharacter(const FObjectInitializer& ObjectInitializer);

	/**
	 * Verifies the complete Narrative Pro controller contract required by a dynamically
	 * spawned physical assault pawn. Runtime scheduling and editor validation share
	 * this check so an unusable NPC class cannot reserve or consume finite force.
	 */
	static bool ValidateNarrativeSpawnClass(const UClass* CandidateClass,
		FText& OutFailureReason);

	/**
	 * Validates both the pawn contract and Narrative's multiple-instance policy for
	 * the complete finite force. Narrative rejects the second spawn when a reusable
	 * assault definition does not opt into multiple instances.
	 */
	static bool ValidateNarrativeSpawnDefinition(const UNPCDefinition* Definition,
		int32 PlannedForce, FText& OutFailureReason);

	/**
	 * Spawns through Narrative's character subsystem so CharacterMap/NPCMap remain
	 * authoritative. The scoped context is consumed by SetNPCDefinition before the
	 * definition is applied, satisfying Narrative activity and save identity ordering.
	 */
	static ATerritoryAssaultCharacter* SpawnThroughNarrative(
		UNarrativeCharacterSubsystem* CharacterSubsystem, UNPCDefinition* Definition,
		const FGameplayTag& ExactFaction, const FGuid& TerritoryGuid,
		const FGuid& SpawnGuid, const FTransform& SpawnTransform, FName SpawnName,
		UNPCActivityConfiguration* OptionalActivityOverride,
		const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides,
		const FGuid& AssaultID, const FGameplayTag& TargetTerritory);

	/** Ensures this finished server spawn has its configured Narrative controller/activity. */
	bool EnsureNarrativeControllerReady();

	/** True once Narrative's definition, appearance, controller, and activity are usable. */
	UFUNCTION(BlueprintPure, Category="Territory|Assault")
	bool IsNarrativeSpawnReady() const;

	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void SetNPCDefinition(UNPCDefinition* Definition) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Territory|Assault")
	TObjectPtr<UTerritoryAssaultParticipantComponent> AssaultParticipant;

protected:
	virtual void BeginPlay() override;
	virtual bool ShouldRespawn_Implementation() const override;
};
