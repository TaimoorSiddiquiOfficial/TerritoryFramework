#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "TerritorySavableData.generated.h"

/**
 * Persistent actor that saves economy + diplomacy state.
 * Place one instance in the level. Implements INarrativeSavableActor
 * so Narrative's save system picks it up automatically.
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API ATerritorySavableData : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ATerritorySavableData();

	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& NewGUID) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category = "Territory|Identity",
		meta = (DisplayName = "Savable Data GUID (auto-generated)"))
	FGuid SavableDataGUID;

	UPROPERTY(SaveGame)
	TMap<FGameplayTag, FTerritoryTreasury> SavedTreasuries;

	UPROPERTY(SaveGame)
	TArray<FTreatyRecord> SavedTreaties;

	UPROPERTY(SaveGame)
	TMap<FGameplayTag, int32> SavedReputation;

	UPROPERTY(SaveGame)
	TArray<FDiplomacyEvent> SavedDiplomacyHistory;

private:
	void SaveToSelf();
	void LoadFromSelf();
};
