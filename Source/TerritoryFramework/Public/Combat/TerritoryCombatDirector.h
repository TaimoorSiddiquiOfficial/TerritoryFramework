#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TerritoryCombatDirector.generated.h"

class ATerritoryVolume;
class ANarrativeNPCController;

UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryCombatDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	bool RequestAttackPermission(ATerritoryVolume* Territory, ANarrativeNPCController* Controller);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	void ReleaseAttackPermission(ATerritoryVolume* Territory, ANarrativeNPCController* Controller);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Territory|Combat")
	void ReleaseAllPermissions(ANarrativeNPCController* Controller);

	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	bool HasAttackPermission(const ATerritoryVolume* Territory, const ANarrativeNPCController* Controller) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	int32 GetGrantedPermissions(const ATerritoryVolume* Territory) const;

	UFUNCTION(BlueprintPure, Category = "Territory|Combat")
	int32 GetAvailableSlots(const ATerritoryVolume* Territory) const;

private:
	struct FPerTerritoryPermissions
	{
		TArray<TWeakObjectPtr<ANarrativeNPCController>> GrantedControllers;
	};

	TMap<TWeakObjectPtr<ATerritoryVolume>, FPerTerritoryPermissions> PermissionMap;

	void CleanupInvalidControllers(FPerTerritoryPermissions& Permissions);
};
