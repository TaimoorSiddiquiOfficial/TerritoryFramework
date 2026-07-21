#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "SaveSystemStatics.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ATerritoryVolume::ATerritoryVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BoundsShape = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsShape"));
	RootComponent = BoundsShape;

	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		Box->SetBoxExtent(FVector(500.f, 500.f, 200.f));
		Box->SetCollisionProfileName(FName("OverlapAllDynamic"));
	}

	OwnershipData.MaxConcurrentAttackers = InitialMaxConcurrentAttackers;
	OwnershipData.PeriodicIncome = InitialPeriodicIncome;
	OwnershipData.GuardCost = InitialGuardCost;
}

void ATerritoryVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!TerritoryGUID.IsValid())
		{
			TerritoryGUID = FGuid::NewGuid();
		}

		if (OwnershipData.State == ETerritoryState::Unclaimed && InitialOwningFaction.IsValid())
		{
			OwnershipData.OwningFaction = InitialOwningFaction;
			OwnershipData.State = ETerritoryState::Claimed;
		}

		if (bStartsLocked && OwnershipData.State == ETerritoryState::Unclaimed)
		{
			OwnershipData.State = ETerritoryState::Locked;
		}

		USaveSystemStatics::LoadSingleActor(this);
	}

	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->RegisterTerritory(this);
	}
}

void ATerritoryVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->UnregisterTerritory(this);
	}

	for (const TWeakObjectPtr<AActor>& DefenderPtr : RegisteredDefenders)
	{
		if (DefenderPtr.IsValid())
		{
			UnbindDefenderDeath(DefenderPtr.Get());
		}
	}
	RegisteredDefenders.Empty();

	Super::EndPlay(EndPlayReason);
}

void ATerritoryVolume::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerritoryVolume, OwnershipData);
}

FGuid ATerritoryVolume::GetActorGUID_Implementation() const { return TerritoryGUID; }
void ATerritoryVolume::SetActorGUID_Implementation(const FGuid& NewGUID) { TerritoryGUID = NewGUID; }
void ATerritoryVolume::PrepareForSave_Implementation() { /* OwnershipData auto-saved via SaveGame */ }

void ATerritoryVolume::Load_Implementation()
{
	for (const TWeakObjectPtr<AActor>& DefenderPtr : RegisteredDefenders)
	{
		if (DefenderPtr.IsValid())
		{
			BindDefenderDeath(DefenderPtr.Get());
		}
	}
}

bool ATerritoryVolume::ShouldRespawn_Implementation() const { return false; }

FGameplayTag ATerritoryVolume::GetOwningFaction() const { return OwnershipData.OwningFaction; }
ETerritoryState ATerritoryVolume::GetTerritoryState() const { return OwnershipData.State; }
float ATerritoryVolume::GetControlProgress() const { return OwnershipData.ControlProgress; }
bool ATerritoryVolume::IsContested() const { return OwnershipData.State == ETerritoryState::Contested; }
FGameplayTag ATerritoryVolume::GetTerritoryTag() const { return TerritoryTag; }
FText ATerritoryVolume::GetTerritoryDisplayName() const { return TerritoryDisplayName; }
int32 ATerritoryVolume::GetMaxConcurrentAttackers() const { return OwnershipData.MaxConcurrentAttackers; }
int32 ATerritoryVolume::GetDefenderCount() const { return RegisteredDefenders.Num(); }
int32 ATerritoryVolume::GetPeriodicIncome() const { return OwnershipData.PeriodicIncome; }

bool ATerritoryVolume::IsOwnedByFaction(const FGameplayTag& Faction) const
{
	return OwnershipData.State == ETerritoryState::Claimed && OwnershipData.OwningFaction == Faction;
}

FBox ATerritoryVolume::GetTerritoryBounds() const
{
	return BoundsShape ? BoundsShape->Bounds.GetBox() : FBox(ForceInit);
}

bool ATerritoryVolume::ContainsPoint(const FVector& WorldPoint) const
{
	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		FVector ClosestPoint;
		float DistanceSq;
		Box->GetSquaredDistanceToCollision(WorldPoint, DistanceSq, ClosestPoint);
		return DistanceSq == 0.f;
	}
	if (BoundsShape)
	{
		FBoxSphereBounds Bounds = BoundsShape->CalcBounds(BoundsShape->GetComponentTransform());
		return Bounds.GetBox().IsInside(WorldPoint);
	}
	return false;
}

void ATerritoryVolume::SetOwningFaction(const FGameplayTag& NewFaction)
{
	if (!HasAuthority()) return;

	FGameplayTag OldOwner = OwnershipData.OwningFaction;
	if (OldOwner == NewFaction) return;

	OwnershipData.OwningFaction = NewFaction;
	OwnershipData.State = NewFaction.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
	OwnershipData.ContestingFaction = FGameplayTag();
	OwnershipData.ControlProgress = NewFaction.IsValid() ? 1.f : 0.f;

	OnOwnershipChanged(OldOwner, NewFaction);
	OnTerritoryControlChanged.Broadcast(this, OldOwner, NewFaction);
}

void ATerritoryVolume::SetControlProgress(float Progress)
{
	if (!HasAuthority()) return;
	OwnershipData.ControlProgress = FMath::Clamp(Progress, 0.f, 1.f);
}

void ATerritoryVolume::SetTerritoryState(ETerritoryState NewState)
{
	if (!HasAuthority()) return;
	ETerritoryState OldState = OwnershipData.State;
	if (OldState == NewState) return;
	OwnershipData.State = NewState;
	OnStateChanged(OldState, NewState);
	OnTerritoryStateChanged.Broadcast(this, NewState);
}

void ATerritoryVolume::RegisterDefender(AActor* Defender)
{
	if (!Defender || !HasAuthority()) return;

	RegisteredDefenders.AddUnique(Defender);
	OwnershipData.DefenderCount = RegisteredDefenders.Num();
	BindDefenderDeath(Defender);
}

void ATerritoryVolume::UnregisterDefender(AActor* Defender)
{
	if (!Defender || !HasAuthority()) return;

	UnbindDefenderDeath(Defender);
	RegisteredDefenders.Remove(Defender);
	CleanupInvalidDefenders();
	OwnershipData.DefenderCount = RegisteredDefenders.Num();
}

TArray<AActor*> ATerritoryVolume::GetRegisteredDefenders() const
{
	TArray<AActor*> Result;
	for (const TWeakObjectPtr<AActor>& Ptr : RegisteredDefenders)
	{
		if (Ptr.IsValid())
		{
			Result.Add(Ptr.Get());
		}
	}
	return Result;
}

void ATerritoryVolume::OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner)
{
}

void ATerritoryVolume::OnStateChanged_Implementation(ETerritoryState OldState, ETerritoryState NewState)
{
}

void ATerritoryVolume::OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC)
{
	UnregisterDefender(KilledActor);
}

void ATerritoryVolume::BindDefenderDeath(AActor* Defender)
{
	if (!Defender) return;
	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Defender))
	{
		if (UNarrativeAbilitySystemComponent* ASC =
			Cast<UNarrativeAbilitySystemComponent>(ASCInterface->GetAbilitySystemComponent()))
		{
			ASC->OnDied.AddUniqueDynamic(this, &ATerritoryVolume::OnDefenderDied);
		}
	}
}

void ATerritoryVolume::UnbindDefenderDeath(AActor* Defender)
{
	if (!Defender) return;
	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Defender))
	{
		if (UNarrativeAbilitySystemComponent* ASC =
			Cast<UNarrativeAbilitySystemComponent>(ASCInterface->GetAbilitySystemComponent()))
		{
			ASC->OnDied.RemoveDynamic(this, &ATerritoryVolume::OnDefenderDied);
		}
	}
}

void ATerritoryVolume::CleanupInvalidDefenders()
{
	RegisteredDefenders.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });
}
