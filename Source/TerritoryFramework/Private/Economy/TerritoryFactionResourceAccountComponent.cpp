#include "Economy/TerritoryFactionResourceAccountComponent.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

UTerritoryFactionResourceAccountComponent::UTerritoryFactionResourceAccountComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTerritoryFactionResourceAccountComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTerritoryFactionResourceAccountComponent, bRegistered);
	DOREPLIFETIME(UTerritoryFactionResourceAccountComponent, RegistrationFaction);
}

void UTerritoryFactionResourceAccountComponent::BeginPlay()
{
	Super::BeginPlay();
	bWantsRegistration = bAutoRegister;
	BindIdentitySources();
	if (bWantsRegistration) TryAutoRegister();
}

void UTerritoryFactionResourceAccountComponent::BindIdentitySources()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || bEndingPlay) return;
	AActor* IdentityActor = GetOwner();
	// Native NPC removal and SetOwnedCharacter do not always broadcast. Observe
	// identity changes while active; this is separate from bounded startup retries.
	if (bWantsRegistration && !World->GetTimerManager().IsTimerActive(IdentityObservationTimer))
	{
		World->GetTimerManager().SetTimer(IdentityObservationTimer, this,
			&UTerritoryFactionResourceAccountComponent::ObserveIdentity, 1.f, true);
	}
	if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this,
			&UTerritoryFactionResourceAccountComponent::OnPossessedPawnChanged);
		IdentityActor = FTerritoryNarrativeProAdapter::ResolvePlayerCharacter(Controller);
	}
	ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(IdentityActor);
	if (BoundCharacter.Get() != Character)
	{
		if (BoundCharacter.IsValid()) BoundCharacter->OnFactionUpdated.RemoveDynamic(
			this, &UTerritoryFactionResourceAccountComponent::OnNarrativeIdentityChanged);
		BoundCharacter = Character;
		if (Character) Character->OnFactionUpdated.AddUniqueDynamic(
			this, &UTerritoryFactionResourceAccountComponent::OnNarrativeIdentityChanged);
	}
	if (UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>())
	{
		Save->OnFinishedLoad.AddUniqueDynamic(this,
			&UTerritoryFactionResourceAccountComponent::OnNarrativeIdentityChanged);
	}
	if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		Economy->OnFactionResourceAccountChanged.AddUniqueDynamic(this,
			&UTerritoryFactionResourceAccountComponent::OnResourceAccountChanged);
	}
}

void UTerritoryFactionResourceAccountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (BoundCharacter.IsValid()) BoundCharacter->OnFactionUpdated.RemoveDynamic(
		this, &UTerritoryFactionResourceAccountComponent::OnNarrativeIdentityChanged);
	BoundCharacter.Reset();
	if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this,
			&UTerritoryFactionResourceAccountComponent::OnPossessedPawnChanged);
	}
	if (UWorld* World = GetWorld())
	{
		if (UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>())
			Save->OnFinishedLoad.RemoveDynamic(this,
				&UTerritoryFactionResourceAccountComponent::OnNarrativeIdentityChanged);
		if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
			Economy->OnFactionResourceAccountChanged.RemoveDynamic(this,
				&UTerritoryFactionResourceAccountComponent::OnResourceAccountChanged);
	}
	UnregisterResourceAccount();
	Super::EndPlay(EndPlayReason);
}

FGameplayTag UTerritoryFactionResourceAccountComponent::GetResourceAccountFaction() const
{
	return BindingMode == ETerritoryResourceAccountBinding::OwnerPrimaryFaction
		? UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, GetOwner()) : Faction;
}

bool UTerritoryFactionResourceAccountComponent::IsResourceAccountRegistered() const
{
	const UWorld* World = GetWorld();
	if (!World || RegistrationFaction != GetResourceAccountFaction()) return false;
	if (World->GetNetMode() == NM_Client) return bRegistered;
	const UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	return Economy && Economy->IsFactionResourceAccountSelected(RegistrationFaction, GetOwner());
}

void UTerritoryFactionResourceAccountComponent::RefreshRegistrationStatus()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	bRegistered = IsResourceAccountRegistered();
	if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
}

void UTerritoryFactionResourceAccountComponent::TryAutoRegister()
{
	if (!bWantsRegistration || bEndingPlay) return;
	++RegistrationAttempts;
	if (RegisterResourceAccount()) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client
		|| RegistrationAttempts >= FMath::Max(1, MaxRegistrationAttempts)) return;
	World->GetTimerManager().SetTimer(RegistrationRetryTimer, this,
		&UTerritoryFactionResourceAccountComponent::TryAutoRegister,
		FMath::Max(0.1f, RegistrationRetryInterval), false);
}

bool UTerritoryFactionResourceAccountComponent::RegisterResourceAccount()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !World || World->GetNetMode() == NM_Client || bEndingPlay) return false;
	bWantsRegistration = true;
	BindIdentitySources();
	const FGameplayTag DesiredFaction = GetResourceAccountFaction();
	ObservedFactions = UTerritoryBlueprintLibrary::GetActorFactions(this, Owner);
	ObservedDesiredFaction = DesiredFaction;
	if (!UTerritoryBlueprintLibrary::IsPoliticalFactionTag(DesiredFaction)
		|| !ObservedFactions.HasTagExact(DesiredFaction))
	{
		RemoveCurrentRegistration();
		RefreshRegistrationStatus();
		return false;
	}
	if (RegistrationFaction != DesiredFaction) RemoveCurrentRegistration();
	RegistrationFaction = DesiredFaction;
	if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		Economy->RegisterFactionResourceAccount(DesiredFaction, Owner, AccountPriority);
	}
	RefreshRegistrationStatus();
	if (bRegistered)
	{
		RegistrationAttempts = 0;
		World->GetTimerManager().ClearTimer(RegistrationRetryTimer);
	}
	return bRegistered;
}

void UTerritoryFactionResourceAccountComponent::RemoveCurrentRegistration()
{
	const FGameplayTag PreviousFaction = RegistrationFaction;
	RegistrationFaction = FGameplayTag();
	bRegistered = false;
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client && PreviousFaction.IsValid())
	{
		if (UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
			Economy->UnregisterFactionResourceAccount(PreviousFaction, GetOwner());
	}
}

void UTerritoryFactionResourceAccountComponent::UnregisterResourceAccount()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	bWantsRegistration = false;
	RegistrationAttempts = 0;
	World->GetTimerManager().ClearTimer(RegistrationRetryTimer);
	World->GetTimerManager().ClearTimer(IdentityObservationTimer);
	RemoveCurrentRegistration();
	RefreshRegistrationStatus();
}

void UTerritoryFactionResourceAccountComponent::OnNarrativeIdentityChanged()
{
	if (bEndingPlay) return;
	BindIdentitySources();
	RegistrationAttempts = 0;
	if (bWantsRegistration) TryAutoRegister();
}

void UTerritoryFactionResourceAccountComponent::ObserveIdentity()
{
	if (!bWantsRegistration || bEndingPlay) return;
	AActor* IdentityActor = GetOwner();
	if (APlayerController* Controller = Cast<APlayerController>(IdentityActor))
		IdentityActor = FTerritoryNarrativeProAdapter::ResolvePlayerCharacter(Controller);
	if (BoundCharacter.Get() != Cast<ANarrativeCharacter>(IdentityActor)
		|| ObservedDesiredFaction != GetResourceAccountFaction()
		|| ObservedFactions != UTerritoryBlueprintLibrary::GetActorFactions(this, GetOwner()))
	{
		OnNarrativeIdentityChanged();
	}
	else if (bRegistered != IsResourceAccountRegistered())
	{
		if (UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>())
			Economy->RefreshFactionResourceAccount(RegistrationFaction);
		RefreshRegistrationStatus();
	}
}

void UTerritoryFactionResourceAccountComponent::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	OnNarrativeIdentityChanged();
}

void UTerritoryFactionResourceAccountComponent::OnResourceAccountChanged(FGameplayTag ChangedFaction)
{
	if (!bEndingPlay && ChangedFaction == RegistrationFaction) RefreshRegistrationStatus();
}
