#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
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

	// Bind to all child districts' ownership changes
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (Registry)
	{
		FGameplayTag CityTag = GetTerritoryTag();
		TArray<ATerritoryVolume*> Districts = Registry->GetChildTerritories(CityTag);
		for (ATerritoryVolume* District : Districts)
		{
			District->OnTerritoryControlChanged.AddDynamic(this, &ATerritoryCity::OnDistrictControlChanged);
		}
	}
}

TArray<ATerritoryVolume*> ATerritoryCity::GetDistricts() const
{
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return TArray<ATerritoryVolume*>();

	// Use registry's indexed child lookup instead of O(N) full scan
	FGameplayTag CityTag = GetTerritoryTag();
	return Registry->GetChildTerritories(CityTag);
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
	// Check if all districts are now owned by the same faction
	FGameplayTag CurrentOwner = GetOwningFaction();
	if (CurrentOwner.IsValid() && AllDistrictsOwnedBy(CurrentOwner))
	{
		OnCityFullyCaptured(CurrentOwner);
	}
	else if (OldOwner.IsValid() && OldOwner == CurrentOwner)
	{
		OnCityLost(OldOwner);
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

	// Use ParentTerritoryTag for direct lookup instead of O(N) scan
	FGameplayTag ParentTag = GetParentTerritoryTag();
	if (ParentTag.IsValid())
	{
		ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		return Cast<ATerritoryCity>(Parent);
	}

	// Fallback: walk up via tag matching
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

	// Use registry's indexed child lookup instead of O(N) full scan
	FGameplayTag DistrictTag = GetTerritoryTag();
	return Registry->GetChildTerritories(DistrictTag);
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

ATerritoryDistrict* ATerritoryProperty::GetOwningDistrict() const
{
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return nullptr;

	// Use ParentTerritoryTag for direct lookup instead of O(N) scan
	FGameplayTag ParentTag = GetParentTerritoryTag();
	if (ParentTag.IsValid())
	{
		ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		return Cast<ATerritoryDistrict>(Parent);
	}

	// Fallback: walk up via tag matching
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
