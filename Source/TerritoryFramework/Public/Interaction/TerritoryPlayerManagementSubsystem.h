#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerritoryPlayerManagementSubsystem.generated.h"

class AGameModeBase;
class APlayerController;

/**
 * World-owned lifecycle bridge that installs the replicated district-management
 * RPC component on every authoritative player controller. It owns no gameplay state.
 */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryPlayerManagementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void EnsureManagementComponent(APlayerController* PlayerController) const;
	void HandlePlayerPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
};
