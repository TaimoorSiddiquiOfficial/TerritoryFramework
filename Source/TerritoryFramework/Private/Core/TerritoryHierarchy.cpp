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
	District->OnTerritoryOwnershipChanged.AddUniqueDynamic(this, &ATerritoryCity::OnDistrictControlChanged);
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

FGameplayTag ATerritoryCity::GetMajorityOwner() const
{
	TArray<ATerritoryVolume*> Districts = GetDistricts();
	if (Districts.Num() == 0) return FGameplayTag();

	TMap<FGameplayTag, int32> Counts;
	for (const ATerritoryVolume* District : Districts)
	{
		FGameplayTag DistrictOwner = District->GetOwningFaction();
		if (DistrictOwner.IsValid())
		{
			int32& Count = Counts.FindOrAdd(DistrictOwner);
			++Count;
		}
	}

	FGameplayTag BestFaction;
	int32 BestCount = 0;
	for (const auto& Pair : Counts)
	{
		if (Pair.Value > BestCount)
		{
			BestCount = Pair.Value;
			BestFaction = Pair.Key;
		}
	}

	// Only return majority if > 50%
	if (BestCount * 2 > Districts.Num())
	{
		return BestFaction;
	}
	return FGameplayTag();
}

bool ATerritoryCity::IsFullyCaptured() const
{
	FGameplayTag CityOwner = GetOwningFaction();
	return CityOwner.IsValid() && AllDistrictsOwnedBy(CityOwner);
}

FGameplayTag ATerritoryCity::GetCapturingFaction() const
{
	if (IsFullyCaptured())
	{
		return GetOwningFaction();
	}
	return FGameplayTag();
}

int32 ATerritoryCity::GetCapitalDistrictCount() const
{
	TArray<ATerritoryVolume*> Districts = GetDistricts();
	int32 Count = 0;
	for (ATerritoryVolume* District : Districts)
	{
		ATerritoryDistrict* D = Cast<ATerritoryDistrict>(District);
		if (D && D->IsCapitalDistrict())
		{
			++Count;
		}
	}
	return Count;
}

bool ATerritoryCity::HasCapitalDistrict() const
{
	return GetCapitalDistrictCount() > 0;
}

void ATerritoryCity::OnCityFullyCaptured_Implementation(FGameplayTag CapturingFaction)
{
	UE_LOG(LogTerritory, Log, TEXT("[CityCapture] %s fully captured by %s"),
		*GetTerritoryTag().ToString(), *CapturingFaction.ToString());

	// Recalculate income for both the capturing faction and the losing faction
	UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
	if (Economy)
	{
		Economy->MarkFactionDirty(CapturingFaction);
	}

	// Economy bonus for capturing a city with a capital district
	if (HasCapitalDistrict())
	{
		if (Economy)
		{
			Economy->AddToTreasury(CapturingFaction, 1000, TEXT("Capital city captured"),
				ETerritoryTransactionType::Reward);
			UE_LOG(LogTerritory, Log, TEXT("[CityCapture] Capital bonus: 1000 gold to %s"),
				*CapturingFaction.ToString());
		}
	}
}

void ATerritoryCity::OnCityLost_Implementation(FGameplayTag PreviousFaction)
{
	UE_LOG(LogTerritory, Log, TEXT("[CityCapture] %s lost by %s"),
		*GetTerritoryTag().ToString(), *PreviousFaction.ToString());

	// Only unclaim if the old owner holds NO districts. If they still hold some,
	// transition to Contested — the city is partially held, not fully lost.
	if (HasAuthority() && !IsLocked())
	{
		if (AllDistrictsOwnedBy(PreviousFaction))
		{
			// Old owner still holds all districts — shouldn't happen, but guard
		}
		else
		{
			// Check if any other faction has all districts
			TArray<ATerritoryVolume*> Districts = GetDistricts();
			bool bAnyFactionOwnsAll = false;
			for (ATerritoryVolume* District : Districts)
			{
				FGameplayTag DistrictOwner = District->GetOwningFaction();
				if (DistrictOwner.IsValid() && DistrictOwner != PreviousFaction)
				{
					if (AllDistrictsOwnedBy(DistrictOwner))
					{
						// Another faction fully captured — they own the city now
						SetOwningFaction(DistrictOwner);
						bAnyFactionOwnsAll = true;
						break;
					}
				}
			}

			if (!bAnyFactionOwnsAll)
			{
				// No faction owns all districts — city is contested
				if (OwnershipData.State == ETerritoryState::Claimed)
				{
					SetTerritoryState(ETerritoryState::Contested);
				}
			}
		}
	}

	// Recalculate income for the faction that lost the city
	UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
	if (Economy && PreviousFaction.IsValid())
	{
		Economy->MarkFactionDirty(PreviousFaction);
	}
}

void ATerritoryCity::OnDistrictCapturedInCity_Implementation(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	UE_LOG(LogTerritory, Log, TEXT("[CityCapture] District %s captured in city %s: %s → %s"),
		*District->GetTerritoryTag().ToString(),
		*GetTerritoryTag().ToString(),
		*OldOwner.ToString(), *NewOwner.ToString());
}

void ATerritoryCity::OnDistrictControlChanged(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (!District) return;

	// Fire the BP-exposed hook for any district capture within this city
	OnDistrictCapturedInCity(District, OldOwner, NewOwner);

	// Server-only mutations from here
	if (!HasAuthority()) return;

	// Cascade ownership change to child properties of the district
	CascadeCaptureToProperties(District, NewOwner);

	// ─── Hierarchy Lock Propagation ───
	// If any district still has enemy-held properties, the district stays contested
	// and the city can't be fully captured. Lock propagates bottom-up.

	if (NewOwner.IsValid())
	{
		// Check if all districts now owned by the same faction
		if (AllDistrictsOwnedBy(NewOwner))
		{
			FGameplayTag CityOldOwner = GetOwningFaction();
			if (CityOldOwner != NewOwner)
			{
				// City fully captured — unlock if it was locked
				if (IsLocked())
				{
					TryUnlock(true);
				}
				SetOwningFaction(NewOwner);
				OnCityFullyCaptured(NewOwner);
				OnCityCapturedDelegate.Broadcast(this, NewOwner);
			}
		}
		else
		{
			// Not all districts owned by one faction — city stays contested
			if (OwnershipData.State == ETerritoryState::Claimed)
			{
				SetTerritoryState(ETerritoryState::Contested);
			}
		}
	}

	// Check if city was lost — the previous owner no longer controls all districts
	if (OldOwner.IsValid())
	{
		FGameplayTag CityOwner = GetOwningFaction();
		if (CityOwner == OldOwner && !AllDistrictsOwnedBy(OldOwner))
		{
			OnCityLost(OldOwner);
			OnCityLostDelegate.Broadcast(this, OldOwner);
		}
	}
}

void ATerritoryCity::CascadeCaptureToProperties(ATerritoryVolume* District, FGameplayTag NewOwner)
{
	if (!District || !NewOwner.IsValid() || !HasAuthority()) return;

	ATerritoryDistrict* D = Cast<ATerritoryDistrict>(District);
	if (!D) return;

	// Hierarchy collapse: when a district changes owner, all child properties
	// auto-reassign to the new district owner
	TArray<ATerritoryVolume*> Properties = D->GetProperties();
	for (ATerritoryVolume* Property : Properties)
	{
		if (!Property) continue;

		FGameplayTag PropOwner = Property->GetOwningFaction();
		if (PropOwner != NewOwner)
		{
			UE_LOG(LogTerritory, Log, TEXT("[HierarchyCollapse] Property %s auto-reassigned: %s → %s (district %s captured)"),
				*Property->GetTerritoryTag().ToString(),
				*PropOwner.ToString(), *NewOwner.ToString(),
				*District->GetTerritoryTag().ToString());

			// SetOwningFaction → OnOwnershipChanged → OnPropertyCaptured + delegate broadcast.
			// Do NOT call OnPropertyCaptured/Broadcast again here — that would fire events twice.
			Property->SetOwningFaction(NewOwner);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryDistrict
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryDistrict::ATerritoryDistrict()
{
}

void ATerritoryDistrict::BeginPlay()
{
	Super::BeginPlay();

	// Bind to child properties for hierarchy collapse
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (Registry)
	{
		Registry->OnTerritoryRegistered.AddDynamic(this, &ATerritoryDistrict::OnTerritoryRegistered);

		TArray<ATerritoryVolume*> Properties = Registry->GetChildTerritories(GetTerritoryTag());
		for (ATerritoryVolume* Property : Properties)
		{
			BindToProperty(Property);
		}
	}
}

void ATerritoryDistrict::BindToProperty(ATerritoryVolume* Property)
{
	if (!Property) return;
	Property->OnTerritoryOwnershipChanged.AddUniqueDynamic(this, &ATerritoryDistrict::OnPropertyControlChanged);
}

void ATerritoryDistrict::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (bWasUnregistered) return;

	FGameplayTag ChildParent = Territory->GetParentTerritoryTag();
	if (ChildParent == GetTerritoryTag())
	{
		BindToProperty(Territory);
	}
}

void ATerritoryDistrict::OnPropertyControlChanged(ATerritoryVolume* Property, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	// Server-only mutations
	if (!HasAuthority()) return;

	// Mark factions dirty for economy recalculation — deferred to next economy tick
	// to avoid redundant O(N) scans from property → district → city cascade.
	UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
	if (Economy)
	{
		if (OldOwner.IsValid()) Economy->MarkFactionDirty(OldOwner);
		if (NewOwner.IsValid()) Economy->MarkFactionDirty(NewOwner);
	}

	// ─── Hierarchy Capture Policy: Unanimity ───
	// A district is captured ONLY when ALL its properties are owned by the same faction.
	// No majority capture, no partial control — clean and predictable.
	// Designers can use ForceCapture/SetOwningFaction for scripted overrides.
	if (NewOwner.IsValid())
	{
		FGameplayTag DistrictOwner = GetOwningFaction();
		if (DistrictOwner != NewOwner)
		{
			if (AllPropertiesOwnedBy(NewOwner))
			{
				// All properties aligned — district captured
				if (IsLocked())
				{
					TryUnlock(true);
				}
				SetOwningFaction(NewOwner);
				OnDistrictFullyCaptured(NewOwner);
				OnDistrictCapturedDelegate.Broadcast(this, DistrictOwner, NewOwner);
			}
			else if (DistrictOwner.IsValid() && !AllPropertiesOwnedBy(DistrictOwner))
			{
				// District owner no longer holds all properties — contest it
				if (OwnershipData.State == ETerritoryState::Claimed)
				{
					SetTerritoryState(ETerritoryState::Contested);
				}
			}
		}
	}
}

void ATerritoryDistrict::OnDistrictFullyCaptured_Implementation(FGameplayTag CapturingFaction)
{
	UE_LOG(LogTerritory, Log, TEXT("[DistrictCapture] %s fully captured by %s"),
		*GetTerritoryTag().ToString(), *CapturingFaction.ToString());

	// Capital district bonus
	if (bIsCapital)
	{
		UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
		if (Economy)
		{
			Economy->AddToTreasury(CapturingFaction, 500, TEXT("Capital district captured"),
				ETerritoryTransactionType::Reward);
		}
	}
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

int32 ATerritoryDistrict::GetPropertyCountForFaction(FGameplayTag Faction) const
{
	TArray<ATerritoryVolume*> Properties = GetProperties();
	int32 Count = 0;
	for (const ATerritoryVolume* Prop : Properties)
	{
		if (Prop->IsOwnedByFaction(Faction))
		{
			++Count;
		}
	}
	return Count;
}

bool ATerritoryDistrict::AllPropertiesOwnedBy(FGameplayTag Faction) const
{
	TArray<ATerritoryVolume*> Properties = GetProperties();
	// A district with no properties cannot be "fully captured" by any faction —
	// returning true would let any attacker trivially capture empty districts.
	if (Properties.Num() == 0) return false;

	for (const ATerritoryVolume* Prop : Properties)
	{
		if (!Prop->IsOwnedByFaction(Faction))
		{
			return false;
		}
	}
	return true;
}

FGameplayTag ATerritoryDistrict::GetMajorityPropertyOwner() const
{
	TArray<ATerritoryVolume*> Properties = GetProperties();
	if (Properties.Num() == 0) return FGameplayTag();

	TMap<FGameplayTag, int32> Counts;
	for (const ATerritoryVolume* Prop : Properties)
	{
		FGameplayTag PropOwner = Prop->GetOwningFaction();
		if (PropOwner.IsValid())
		{
			int32& C = Counts.FindOrAdd(PropOwner);
			++C;
		}
	}

	FGameplayTag Best;
	int32 BestCount = 0;
	for (const auto& Pair : Counts)
	{
		if (Pair.Value > BestCount)
		{
			BestCount = Pair.Value;
			Best = Pair.Key;
		}
	}
	// Only return majority if > 50%
	if (BestCount * 2 > Properties.Num()) return Best;
	return FGameplayTag();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryProperty
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryProperty::ATerritoryProperty()
{
}

void ATerritoryProperty::BeginPlay()
{
	Super::BeginPlay();

	// Only sync to district owner if property has NO owner (first-time init).
	// Do NOT overwrite saved ownership — SaveSystem already restored it in Super::BeginPlay.
	if (HasAuthority() && !GetOwningFaction().IsValid())
	{
		ATerritoryDistrict* District = GetOwningDistrict();
		if (District)
		{
			FGameplayTag DistrictOwner = District->GetOwningFaction();
			if (DistrictOwner.IsValid())
			{
				SetOwningFaction(DistrictOwner);
			}
		}
	}
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

void ATerritoryProperty::OnPropertyCaptured_Implementation(FGameplayTag NewOwner)
{
	UE_LOG(LogTerritory, Log, TEXT("[PropertyCapture] %s captured by %s"),
		*GetTerritoryTag().ToString(), *NewOwner.ToString());

	// Reset upgrade level on capture by a new faction — use SetUpgradeLevel to
	// ensure income recalculation and logging are triggered.
	if (HasAuthority() && UpgradeLevel > 0)
	{
		SetUpgradeLevel(0);
	}
}

void ATerritoryProperty::OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	// Invoke property-specific side effects on every ownership change path
	if (NewOwner.IsValid() && OldOwner != NewOwner)
	{
		OnPropertyCaptured(NewOwner);
		OnPropertyCapturedDelegate.Broadcast(this, NewOwner);
	}
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
	int32 BaseIncome = GetPeriodicIncome();

	// Capital district income multiplier
	ATerritoryDistrict* District = GetOwningDistrict();
	if (District && District->IsCapitalDistrict())
	{
		BaseIncome = static_cast<int32>(BaseIncome * District->CapitalIncomeMultiplier);
	}

	return BaseIncome + (UpgradeLevel * IncomeBonusPerLevel);
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
	Economy->MarkFactionDirty(OwnerFaction);

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
			if (Economy) Economy->MarkFactionDirty(OwnerFaction);
		}

		UE_LOG(LogTerritory, Log, TEXT("[PropertyUpgrade] %s set to level %d (was %d)"),
			*GetTerritoryTag().ToString(), UpgradeLevel, OldLevel);
	}
}