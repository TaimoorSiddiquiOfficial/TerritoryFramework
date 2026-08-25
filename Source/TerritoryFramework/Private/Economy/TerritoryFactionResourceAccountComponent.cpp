#include "Economy/TerritoryFactionResourceAccountComponent.h"

#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

UTerritoryFactionResourceAccountComponent::UTerritoryFactionResourceAccountComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTerritoryFactionResourceAccountComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoRegister) TryAutoRegister();
}

void UTerritoryFactionResourceAccountComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegistrationRetryTimer);
	}
	UnregisterResourceAccount();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryFactionResourceAccountComponent::TryAutoRegister()
{
	if (bRegistered || !bAutoRegister) return;

	++RegistrationAttempts;
	if (RegisterResourceAccount()) return;

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client
		|| RegistrationAttempts >= FMath::Max(1, MaxRegistrationAttempts))
	{
		return;
	}
	World->GetTimerManager().SetTimer(RegistrationRetryTimer, this,
		&UTerritoryFactionResourceAccountComponent::TryAutoRegister,
		FMath::Max(0.1f, RegistrationRetryInterval), false);
}

bool UTerritoryFactionResourceAccountComponent::RegisterResourceAccount()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || World->GetNetMode() == NM_Client || !Faction.IsValid())
	{
		return false;
	}
	if (UTerritoryEconomySubsystem* Economy =
		World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		bRegistered = Economy->RegisterFactionResourceAccount(Faction, Owner);
	}
	if (bRegistered)
	{
		RegistrationAttempts = 0;
		World->GetTimerManager().ClearTimer(RegistrationRetryTimer);
	}
	return bRegistered;
}

void UTerritoryFactionResourceAccountComponent::UnregisterResourceAccount()
{
	if (!bRegistered) return;
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (Owner && World && World->GetNetMode() != NM_Client)
	{
		if (UTerritoryEconomySubsystem* Economy =
			World->GetSubsystem<UTerritoryEconomySubsystem>())
		{
			Economy->UnregisterFactionResourceAccount(Faction, Owner);
		}
	}
	bRegistered = false;
}
