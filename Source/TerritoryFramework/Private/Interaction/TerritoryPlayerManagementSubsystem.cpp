#include "Interaction/TerritoryPlayerManagementSubsystem.h"

#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

void UTerritoryPlayerManagementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	FGameModeEvents::OnGameModePostLoginEvent().AddUObject(
		this, &UTerritoryPlayerManagementSubsystem::HandlePlayerPostLogin);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		EnsureManagementComponent(It->Get());
	}
}

void UTerritoryPlayerManagementSubsystem::Deinitialize()
{
	FGameModeEvents::OnGameModePostLoginEvent().RemoveAll(this);
	Super::Deinitialize();
}

void UTerritoryPlayerManagementSubsystem::EnsureManagementComponent(APlayerController* PlayerController) const
{
	if (PlayerController && PlayerController->GetWorld() == GetWorld() && PlayerController->HasAuthority())
	{
		UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController);
	}
}

void UTerritoryPlayerManagementSubsystem::HandlePlayerPostLogin(
	AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (GameMode && GameMode->GetWorld() == GetWorld())
	{
		EnsureManagementComponent(NewPlayer);
	}
}
