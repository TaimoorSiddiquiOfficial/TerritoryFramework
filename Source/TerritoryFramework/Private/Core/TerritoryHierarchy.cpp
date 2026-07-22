#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryCity
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryCity::ATerritoryCity()
{
}

void ATerritoryCity::BeginPlay()
{
	Super::BeginPlay();

	// Bind to existing child districts
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (Registry)
	{
		// Bind to registry for late-registered districts (World Partition, streaming)
		Registry->OnTerritoryRegistered.AddDynamic(this, &ATerritoryCity::OnTerritoryRegistered);

		// Bind to currently registered districts
		TArray<ATerritoryVolume*> Districts = Registry->GetChildTerritories(GetTerritoryTag());
		for (ATerritoryVolume* District : Districts)
		{
			BindToDistrict(District);
		}
	}
}

void ATerritoryCity::BindToDistrict(ATerritoryVolume* District)
{
	if (!District) return;

	// Check if already bound to avoid double-binding
	District->OnTerritoryControlChanged.AddUniqueDynamic(this, &ATerritoryCity::OnDistrictControlChanged);
}

void ATerritoryCity::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (bWasUnregistered) return;

	// Check if this territory is a child of this city
	FGameplayTag ChildParent = Territory->GetParentTerritoryTag();
	if (ChildParent == GetTerritoryTag())
	{
		BindToDistrict(Territory);
	}
}

TArray<ATerritoryVolume*> ATerritoryCity::GetDistricts() const
{
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return TArray<ATerritoryVolume*>();

	return Registry->GetChildTerritories(GetTerritoryTag());
}

int32 ATerritoryCity::GetDistrictCount() const
{
	return GetDistricts().Num();
}

bool ATerritoryCity::AllDistrictsOwnedBy(FGameplayTag Faction) const
{
	TArray<ATerritoryVolume*> Districts = GetDistricts();
	if (Districts.Num() == 0) return false;

	for (const ATerritoryVolume* District : Districts)
	{
		if (!District->IsOwnedByFaction(Faction))
		{
			return false;
		}
	}
	return true;
}

float ATerritoryCity::GetCityControlPercentage(FGameplayTag Faction) const
{
	TArray<ATerritoryVolume*> Districts = GetDistricts();
	if (Districts.Num() == 0) return 0.f;

	int32 Owned = 0;
	for (const ATerritoryVolume* District : Districts)
	{
		if (District->IsOwnedByFaction(Faction))
		{
			++Owned;
		}
	}
	return static_cast<float>(Owned) / static_cast<float>(Districts.Num());
}

void ATerritoryCity::OnCityFullyCaptured_Implementation(FGameplayTag CapturingFaction)
{
	UE_LOG(LogTerritory, Log, TEXT("City %s fully captured by %s"),
		*GetTerritoryTag().ToString(), *CapturingFaction.ToString());
}

void ATerritoryCity::OnCityLost_Implementation(FGameplayTag PreviousFaction)
{
	UE_LOG(LogTerritory, Log, TEXT("City %s lost by %s"),
		*GetTerritoryTag().ToString(), *PreviousFaction.ToString());
}

void ATerritoryCity::OnDistrictControlChanged(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	// Update city ownership to match majority district owner
	if (NewOwner.IsValid())
	{
		// If all districts now owned by the same faction, the city is fully captured
		if (AllDistrictsOwnedBy(NewOwner))
		{
			FGameplayTag CityOldOwner = GetOwningFaction();
			if (CityOldOwner != NewOwner)
			{
				SetOwningFaction(NewOwner);
				OnCityFullyCaptured(NewOwner);
			}
		}
	}

	// Check if city was lost
	if (OldOwner.IsValid())
	{
		FGameplayTag CityOwner = GetOwningFaction();
		if (CityOwner == OldOwner && !AllDistrictsOwnedBy(OldOwner))
		{
			OnCityLost(OldOwner);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryDistrict
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryDistrict::ATerritoryDistrict()
{
}

ATerritoryCity* ATerritoryDistrict::GetOwningCity() const
{
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return nullptr;

	FGameplayTag ParentTag = GetParentTerritoryTag();
	if (ParentTag.IsValid())
	{
		ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		return Cast<ATerritoryCity>(Parent);
	}

	FGameplayTag MyTag = GetTerritoryTag();
	if (!MyTag.IsValid()) return nullptr;

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (ATerritoryVolume* Volume : All)
	{
		ATerritoryCity* City = Cast<ATerritoryCity>(Volume);
		if (City && MyTag.MatchesTag(City->GetTerritoryTag()) && static_cast<const AActor*>(City) != this)
		{
			return City;
		}
	}
	return nullptr;
}

TArray<ATerritoryVolume*> ATerritoryDistrict::GetProperties() const
{
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return TArray<ATerritoryVolume*>();

	return Registry->GetChildTerritories(GetTerritoryTag());
}

bool ATerritoryDistrict::IsCapitalDistrict() const
{
	return bIsCapital;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryProperty
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryProperty::ATerritoryProperty()
{
}

void ATerritoryProperty::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerritoryProperty, UpgradeLevel);
}

void ATerritoryProperty::OnRep_UpgradeLevel()
{
	OnUpgradeLevelChanged(UpgradeLevel);
}

ATerritoryDistrict* ATerritoryProperty::GetOwningDistrict() const
{
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return nullptr;

	FGameplayTag ParentTag = GetParentTerritoryTag();
	if (ParentTag.IsValid())
	{
		ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		return Cast<ATerritoryDistrict>(Parent);
	}

	FGameplayTag MyTag = GetTerritoryTag();
	if (!MyTag.IsValid()) return nullptr;

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (ATerritoryVolume* Volume : All)
	{
		ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Volume);
		if (District && MyTag.MatchesTag(District->GetTerritoryTag()) && static_cast<const AActor*>(District) != this)
		{
			return District;
		}
	}
	return nullptr;
}

bool ATerritoryProperty::CanUpgrade() const
{
	return UpgradeLevel < MaxUpgradeLevel;
}

int32 ATerritoryProperty::GetUpgradeCost() const
{
	if (!CanUpgrade()) return 0;
	return UpgradeCostPerLevel * (UpgradeLevel + 1);
}

int32 ATerritoryProperty::GetEffectiveIncome() const
{
	return GetPeriodicIncome() + (UpgradeLevel * IncomeBonusPerLevel);
}

bool ATerritoryProperty::TryUpgrade()
{
	if (!HasAuthority() || !CanUpgrade()) return false;

	FGameplayTag OwnerFaction = GetOwningFaction();
	if (!OwnerFaction.IsValid()) return false;

	int32 Cost = GetUpgradeCost();

	UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
	if (!Economy) return false;

	// Check if faction can afford the upgrade
	if (!Economy->CanAfford(OwnerFaction, Cost)) return false;

	// Debit treasury
	FString Reason = FString::Printf(TEXT("Property upgrade %s level %d→%d"),
		*GetTerritoryTag().ToString(), UpgradeLevel, UpgradeLevel + 1);
	if (!Economy->TryDebitTreasury(OwnerFaction, Cost, Reason, ETerritoryTransactionType::UpgradeCost))
	{
		return false;
	}

	// Increment upgrade level (replicated)
	UpgradeLevel++;

	// Recalculate income for the owning faction
	Economy->RecalculateIncome(OwnerFaction);

	UE_LOG(LogTerritory, Log, TEXT("[PropertyUpgrade] %s upgraded to level %d (cost: %d, faction: %s)"),
		*GetTerritoryTag().ToString(), UpgradeLevel, Cost, *OwnerFaction.ToString());

	return true;
}

void ATerritoryProperty::SetUpgradeLevel(int32 NewLevel)
{
	if (!HasAuthority()) return;
	int32 OldLevel = UpgradeLevel;
	UpgradeLevel = FMath::Clamp(NewLevel, 0, MaxUpgradeLevel);

	if (OldLevel != UpgradeLevel)
	{
		FGameplayTag OwnerFaction = GetOwningFaction();
		if (OwnerFaction.IsValid())
		{
			UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
			if (Economy) Economy->RecalculateIncome(OwnerFaction);
		}

		UE_LOG(LogTerritory, Log, TEXT("[PropertyUpgrade] %s set to level %d (was %d)"),
			*GetTerritoryTag().ToString(), UpgradeLevel, OldLevel);
	}
}
