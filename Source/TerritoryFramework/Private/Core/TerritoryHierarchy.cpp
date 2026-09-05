#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

namespace
{
	bool ShouldLogHierarchyOwnership()
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		return Settings && Settings->ShouldDebugOwnership();
	}

	bool ShouldLogPropertyEconomy()
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		return Settings && Settings->ShouldDebugEconomy();
	}

	struct FChildControlView
	{
		FGameplayTag Owner;
		ETerritoryState State = ETerritoryState::Unclaimed;
		ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked;
	};

	TArray<FGameplayTag> GetExpectedChildTags(const ATerritoryVolume* Parent)
	{
		TArray<FGameplayTag> Result;
		if (!Parent) return Result;
		if (const UTerritoryCityDefinition* CityDefinition =
			Cast<UTerritoryCityDefinition>(Parent->GetTerritoryDefinition()))
		{
			Result.Reserve(CityDefinition->Districts.Num());
			for (const UTerritoryDistrictDefinition* District : CityDefinition->Districts)
			{
				Result.Add(District ? District->TerritoryTag : FGameplayTag());
			}
			return Result;
		}
		if (const UTerritoryDistrictDefinition* DistrictDefinition =
			Cast<UTerritoryDistrictDefinition>(Parent->GetTerritoryDefinition()))
		{
			Result.Reserve(DistrictDefinition->Places.Num());
			for (const UTerritoryPlaceDefinition* Place : DistrictDefinition->Places)
			{
				Result.Add(Place ? Place->TerritoryTag : FGameplayTag());
			}
		}
		return Result;
	}

	bool IsExpectedAuthoredChild(const ATerritoryVolume* Parent,
		const ATerritoryVolume* Child)
	{
		if (!Parent || !Child
			|| Child->GetParentTerritoryTag() != Parent->GetTerritoryTag()
			|| !GetExpectedChildTags(Parent).Contains(Child->GetTerritoryTag()))
		{
			return false;
		}
		if (Parent->IsA<ATerritoryCity>())
		{
			return Child->IsA<ATerritoryDistrict>() && !Child->IsA<ATerritoryCity>();
		}
		if (Parent->IsA<ATerritoryDistrict>())
		{
			return Child->IsA<ATerritoryProperty>();
		}
		return false;
	}

	TArray<ATerritoryVolume*> GetLoadedAuthoredChildren(
		const ATerritoryVolume* Parent)
	{
		TArray<ATerritoryVolume*> Result;
		if (!Parent || !Parent->GetWorld()) return Result;
		const UTerritoryRegistrySubsystem* Registry =
			Parent->GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
		if (!Registry) return Result;

		TSet<FGameplayTag> SeenTags;
		for (const FGameplayTag& ChildTag : GetExpectedChildTags(Parent))
		{
			if (!ChildTag.IsValid() || SeenTags.Contains(ChildTag)) continue;
			SeenTags.Add(ChildTag);
			ATerritoryVolume* Child = Registry->GetTerritoryByTag(ChildTag);
			if (IsExpectedAuthoredChild(Parent, Child)) Result.Add(Child);
		}
		return Result;
	}

	const ATerritoryVolume* FindLoadedExpectedChild(
		TConstArrayView<ATerritoryVolume*> LoadedChildren,
		const FGameplayTag& ExpectedTag)
	{
		if (!ExpectedTag.IsValid()) return nullptr;
		for (const ATerritoryVolume* Child : LoadedChildren)
		{
			if (Child && Child->GetTerritoryTag() == ExpectedTag) return Child;
		}
		return nullptr;
	}

	bool ResolveExpectedChildControl(const ATerritoryVolume* Parent,
		TConstArrayView<ATerritoryVolume*> LoadedChildren,
		const FGameplayTag& ExpectedTag, FChildControlView& OutView)
	{
		if (!Parent || !ExpectedTag.IsValid()) return false;
		if (const ATerritoryVolume* Child = FindLoadedExpectedChild(
			LoadedChildren, ExpectedTag))
		{
			OutView.Owner = Child->GetOwningFaction();
			OutView.State = Child->GetTerritoryState();
			OutView.Availability = Child->IsLocked()
				? ETerritoryAvailability::Locked : ETerritoryAvailability::Unlocked;
			return true;
		}

		// WorldState is the durable read model for an authored child that World
		// Partition has unloaded. Without an exact summary, fail closed.
		if (const ATerritoryWorldState* WorldState =
			ATerritoryWorldState::FindTerritoryWorldState(Parent))
		{
			const FReplicatedCaptureSummary Summary =
				WorldState->GetCaptureSummary(ExpectedTag);
			if (Summary.TerritoryTag == ExpectedTag)
			{
				OutView.Owner = Summary.CurrentOwner;
				OutView.State = Summary.State;
				OutView.Availability = Summary.Availability;
				return true;
			}
		}
		return false;
	}

	FGameplayTag GetSecureClaimedOwner(const FChildControlView& View)
	{
		return View.Availability == ETerritoryAvailability::Unlocked
			&& View.State == ETerritoryState::Claimed && View.Owner.IsValid()
			? View.Owner : FGameplayTag();
	}

	TArray<FGameplayTag> BuildExpectedSecureOwners(const ATerritoryVolume* Parent,
		const TArray<ATerritoryVolume*>& LoadedChildren)
	{
		TArray<FGameplayTag> Owners;
		const TArray<FGameplayTag> ExpectedTags = GetExpectedChildTags(Parent);
		Owners.Reserve(ExpectedTags.Num());
		TSet<FGameplayTag> SeenTags;
		for (const FGameplayTag& ExpectedTag : ExpectedTags)
		{
			if (!ExpectedTag.IsValid() || SeenTags.Contains(ExpectedTag))
			{
				Owners.Add(FGameplayTag());
				continue;
			}
			SeenTags.Add(ExpectedTag);
			FChildControlView View;
			Owners.Add(ResolveExpectedChildControl(Parent, LoadedChildren,
				ExpectedTag, View) ? GetSecureClaimedOwner(View) : FGameplayTag());
		}
		return Owners;
	}

	struct FDerivedHierarchyControl
	{
		FGameplayTag SecuredOwner;
		ETerritoryState State = ETerritoryState::Unclaimed;
	};

	FDerivedHierarchyControl ReduceChildControl(const ATerritoryVolume* Parent,
		const TArray<ATerritoryVolume*>& LoadedChildren)
	{
		FDerivedHierarchyControl Result;
		const TArray<FGameplayTag> ExpectedTags = GetExpectedChildTags(Parent);
		if (ExpectedTags.IsEmpty())
		{
			return Result;
		}

		FGameplayTag CommonOwner;
		bool bAllSecure = true;
		bool bAnyPoliticalControl = false;
		TSet<FGameplayTag> SeenTags;
		for (const FGameplayTag& ExpectedTag : ExpectedTags)
		{
			if (!ExpectedTag.IsValid() || SeenTags.Contains(ExpectedTag))
			{
				bAllSecure = false;
				continue;
			}
			SeenTags.Add(ExpectedTag);
			FChildControlView Child;
			if (!ResolveExpectedChildControl(Parent, LoadedChildren, ExpectedTag, Child))
			{
				// World Partition and invalid hierarchy references fail closed. An
				// unrelated actor with the same parent can never replace this tag.
				bAllSecure = false;
				continue;
			}

			const FGameplayTag ChildOwner = Child.Owner;
			bAnyPoliticalControl |= ChildOwner.IsValid()
				|| Child.State == ETerritoryState::Contested;
			if (Child.Availability == ETerritoryAvailability::Locked
				|| Child.State != ETerritoryState::Claimed
				|| !ChildOwner.IsValid())
			{
				bAllSecure = false;
				continue;
			}

			if (!CommonOwner.IsValid()) CommonOwner = ChildOwner;
			else if (CommonOwner != ChildOwner) bAllSecure = false;
		}

		if (bAllSecure && CommonOwner.IsValid())
		{
			Result.SecuredOwner = CommonOwner;
			Result.State = ETerritoryState::Claimed;
		}
		else if (bAnyPoliticalControl)
		{
			Result.State = ETerritoryState::Contested;
		}
		return Result;
	}
}

FGameplayTag TerritoryHierarchyPolicy::FindStrictMajorityOwner(
	const TArray<FGameplayTag>& ChildOwners)
{
	if (ChildOwners.IsEmpty()) return FGameplayTag();
	TMap<FGameplayTag, int32> Counts;
	for (const FGameplayTag& Owner : ChildOwners)
	{
		if (Owner.IsValid()) ++Counts.FindOrAdd(Owner);
	}

	for (const TPair<FGameplayTag, int32>& Pair : Counts)
	{
		if (Pair.Value * 2 > ChildOwners.Num()) return Pair.Key;
	}
	return FGameplayTag();
}

bool TerritoryHierarchyPolicy::AreAllChildrenOwnedBy(
	const TArray<FGameplayTag>& ChildOwners, const FGameplayTag& Faction)
{
	if (ChildOwners.IsEmpty() || !Faction.IsValid()) return false;
	for (const FGameplayTag& Owner : ChildOwners)
	{
		if (Owner != Faction) return false;
	}
	return true;
}

float TerritoryHierarchyPolicy::CalculateControlFraction(
	const TArray<FGameplayTag>& ChildOwners, const FGameplayTag& Faction)
{
	if (ChildOwners.IsEmpty() || !Faction.IsValid()) return 0.f;
	int32 OwnedCount = 0;
	for (const FGameplayTag& Owner : ChildOwners)
	{
		if (Owner == Faction) ++OwnedCount;
	}
	return static_cast<float>(OwnedCount) / static_cast<float>(ChildOwners.Num());
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryCity
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryCity::ATerritoryCity()
{
	ControlMode = ETerritoryControlMode::AggregateOnly;
}

void ATerritoryCity::BeginPlay()
{
	Super::BeginPlay();

	// Bind to existing child districts
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (Registry)
	{
		// Bind to registry for late-registered districts (World Partition, streaming)
		// P1-N06: AddUniqueDynamic prevents duplicate bindings on PIE restart
		Registry->OnTerritoryRegistered.AddUniqueDynamic(this, &ATerritoryCity::OnTerritoryRegistered);
		Registry->OnTerritoryUnregistered.AddUniqueDynamic(this, &ATerritoryCity::OnTerritoryRegistered);

		// Bind to currently registered districts
		TArray<ATerritoryVolume*> Districts = GetDistricts();
		for (ATerritoryVolume* District : Districts)
		{
			BindToDistrict(District);
		}

		ReconcileDerivedControl();
	}
}

void ATerritoryCity::BindToDistrict(ATerritoryVolume* District)
{
	if (!District) return;

	// Check if already bound to avoid double-binding
	District->OnTerritoryOwnershipChanged.AddUniqueDynamic(this, &ATerritoryCity::OnDistrictControlChanged);
	District->OnTerritoryStateChangedDelegate.AddUniqueDynamic(this, &ATerritoryCity::OnDistrictStateChanged);
	District->OnTerritoryAvailabilityChanged.AddUniqueDynamic(this, &ATerritoryCity::OnDistrictAvailabilityChanged);
}

void ATerritoryCity::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	// Definitions are the only hierarchy authority. A runtime actor cannot join a
	// City merely by claiming its parent tag.
	if (IsExpectedAuthoredChild(this, Territory))
	{
		if (!bWasUnregistered) BindToDistrict(Territory);
		ReconcileDerivedControl(Territory);
	}
}

void ATerritoryCity::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryCity::OnTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(this, &ATerritoryCity::OnTerritoryRegistered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

TArray<ATerritoryVolume*> ATerritoryCity::GetDistricts() const
{
	return GetLoadedAuthoredChildren(this);
}

int32 ATerritoryCity::GetDistrictCount() const
{
	return GetExpectedChildTags(this).Num();
}

bool ATerritoryCity::AllDistrictsOwnedBy(FGameplayTag Faction) const
{
	const TArray<FGameplayTag> Owners = BuildExpectedSecureOwners(this, GetDistricts());
	return TerritoryHierarchyPolicy::AreAllChildrenOwnedBy(Owners, Faction);
}

float ATerritoryCity::GetCityControlPercentage(FGameplayTag Faction) const
{
	const TArray<FGameplayTag> Owners = BuildExpectedSecureOwners(this, GetDistricts());
	return TerritoryHierarchyPolicy::CalculateControlFraction(Owners, Faction);
}

FGameplayTag ATerritoryCity::GetMajorityOwner() const
{
	const TArray<FGameplayTag> Owners = BuildExpectedSecureOwners(this, GetDistricts());
	return TerritoryHierarchyPolicy::FindStrictMajorityOwner(Owners);
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
	if (ShouldLogHierarchyOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[CityCapture] %s fully captured by %s"),
			*GetTerritoryTag().ToString(), *CapturingFaction.ToString());
	}

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
			AActor* PreferredBeneficiary = Economy->IncomePayoutPolicy ==
				ETerritoryIncomePayoutPolicy::CapturingPlayer
				? GetActiveTransitionContext().Instigator.Get() : nullptr;
			Economy->CreditCurrencyToFaction(CapturingFaction, 1000,
				Economy->IncomePayoutPolicy, TEXT("Capital city captured"),
				ETerritoryTransactionType::Reward, PreferredBeneficiary);
			if (ShouldLogPropertyEconomy())
			{
				UE_LOG(LogTerritory, Log, TEXT("[CityCapture] Capital bonus: 1000 gold to %s"),
					*CapturingFaction.ToString());
			}
		}
	}
}

void ATerritoryCity::OnCityLost_Implementation(FGameplayTag PreviousFaction)
{
	if (ShouldLogHierarchyOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[CityCapture] %s lost by %s"),
			*GetTerritoryTag().ToString(), *PreviousFaction.ToString());
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
	if (ShouldLogHierarchyOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[CityCapture] District %s captured in city %s: %s → %s"),
			*District->GetTerritoryTag().ToString(),
			*GetTerritoryTag().ToString(),
			*OldOwner.ToString(), *NewOwner.ToString());
	}
}

void ATerritoryCity::OnDistrictControlChanged(ATerritoryVolume* District, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (!District) return;

	// Fire the BP-exposed hook for any district capture within this city
	OnDistrictCapturedInCity(District, OldOwner, NewOwner);

	ReconcileDerivedControl(District);
}

void ATerritoryCity::OnDistrictStateChanged(ATerritoryVolume* District, ETerritoryState NewState)
{
	ReconcileDerivedControl(District);
}

void ATerritoryCity::OnDistrictAvailabilityChanged(ATerritoryVolume* District,
	ETerritoryAvailability NewAvailability)
{
	ReconcileDerivedControl(District);
}

void ATerritoryCity::ReconcileDerivedControl(ATerritoryVolume* ChangedDistrict)
{
	if (!HasAuthority()) return;
	const FGameplayTag PreviousOwner = GetOwningFaction();
	const ETerritoryState PreviousControlState = GetTerritoryState();
	const FDerivedHierarchyControl Derived = ReduceChildControl(this, GetDistricts());
	const FTerritoryTransitionContext Context = ChangedDistrict
		? ChangedDistrict->GetActiveTransitionContext() : FTerritoryTransitionContext();
	SetDerivedControl(Derived.SecuredOwner, Derived.State, Context);

	if (PreviousOwner.IsValid() && PreviousControlState == ETerritoryState::Claimed
		&& (Derived.State != ETerritoryState::Claimed || Derived.SecuredOwner != PreviousOwner))
	{
		OnCityLost(PreviousOwner);
		OnCityLostDelegate.Broadcast(this, PreviousOwner);
	}
	if (Derived.State == ETerritoryState::Claimed && Derived.SecuredOwner.IsValid()
		&& (PreviousControlState != ETerritoryState::Claimed || PreviousOwner != Derived.SecuredOwner))
	{
		OnCityFullyCaptured(Derived.SecuredOwner);
		OnCityCapturedDelegate.Broadcast(this, Derived.SecuredOwner);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ATerritoryDistrict
// ═══════════════════════════════════════════════════════════════════════════════

ATerritoryDistrict::ATerritoryDistrict()
{
	ControlMode = ETerritoryControlMode::AggregateOnly;
}

void ATerritoryDistrict::BeginPlay()
{
	Super::BeginPlay();

	// Bind to child properties for hierarchy collapse
	UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (Registry)
	{
		// P1-N06: AddUniqueDynamic prevents duplicate bindings
		Registry->OnTerritoryRegistered.AddUniqueDynamic(this, &ATerritoryDistrict::OnTerritoryRegistered);
		Registry->OnTerritoryUnregistered.AddUniqueDynamic(this, &ATerritoryDistrict::OnTerritoryRegistered);

		TArray<ATerritoryVolume*> Properties = GetProperties();
		for (ATerritoryVolume* Property : Properties)
		{
			BindToProperty(Property);
		}

		ReconcileDerivedControl();
	}
}

void ATerritoryDistrict::BindToProperty(ATerritoryVolume* Property)
{
	if (!Property) return;
	Property->OnTerritoryOwnershipChanged.AddUniqueDynamic(this, &ATerritoryDistrict::OnPropertyControlChanged);
	Property->OnTerritoryStateChangedDelegate.AddUniqueDynamic(this, &ATerritoryDistrict::OnPropertyStateChanged);
	Property->OnTerritoryAvailabilityChanged.AddUniqueDynamic(this, &ATerritoryDistrict::OnPropertyAvailabilityChanged);
}

void ATerritoryDistrict::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryDistrict::OnTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(this, &ATerritoryDistrict::OnTerritoryRegistered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ATerritoryDistrict::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	if (IsExpectedAuthoredChild(this, Territory))
	{
		if (!bWasUnregistered) BindToProperty(Territory);
		ReconcileDerivedControl(Territory);
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

	ReconcileDerivedControl(Property);
}

void ATerritoryDistrict::OnPropertyStateChanged(ATerritoryVolume* Property,
	ETerritoryState NewState)
{
	ReconcileDerivedControl(Property);
}

void ATerritoryDistrict::OnPropertyAvailabilityChanged(ATerritoryVolume* Property,
	ETerritoryAvailability NewAvailability)
{
	ReconcileDerivedControl(Property);
}

void ATerritoryDistrict::ReconcileDerivedControl(ATerritoryVolume* ChangedProperty)
{
	if (!HasAuthority()) return;
	const FDerivedHierarchyControl Derived = ReduceChildControl(this, GetProperties());
	const FTerritoryTransitionContext Context = ChangedProperty
		? ChangedProperty->GetActiveTransitionContext() : FTerritoryTransitionContext();
	SetDerivedControl(Derived.SecuredOwner, Derived.State, Context);
}

void ATerritoryDistrict::OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	Super::OnOwnershipChanged_Implementation(OldOwner, NewOwner);
	if (NewOwner.IsValid())
	{
		OnDistrictFullyCaptured(NewOwner);
		OnDistrictCapturedDelegate.Broadcast(this, OldOwner, NewOwner);
	}
}

void ATerritoryDistrict::OnDistrictFullyCaptured_Implementation(FGameplayTag CapturingFaction)
{
	if (ShouldLogHierarchyOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[DistrictCapture] %s fully captured by %s"),
			*GetTerritoryTag().ToString(), *CapturingFaction.ToString());
	}

	// Capital district bonus
	if (bIsCapital)
	{
		UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
		if (Economy)
		{
			AActor* PreferredBeneficiary = Economy->IncomePayoutPolicy ==
				ETerritoryIncomePayoutPolicy::CapturingPlayer
				? GetActiveTransitionContext().Instigator.Get() : nullptr;
			Economy->CreditCurrencyToFaction(CapturingFaction, 500,
				Economy->IncomePayoutPolicy, TEXT("Capital district captured"),
				ETerritoryTransactionType::Reward, PreferredBeneficiary);
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
		ATerritoryCity* City = Cast<ATerritoryCity>(Parent);
		return IsExpectedAuthoredChild(City, this) ? City : nullptr;
	}

	// P2-07: Removed dead fallback — a district without ParentTerritoryTag cannot resolve its city.
	// The tag comparison fallback could never match (district and city have different tags).
	// Require ParentTerritoryTag to be set in the editor.
	return nullptr;
}

TArray<ATerritoryVolume*> ATerritoryDistrict::GetProperties() const
{
	return GetLoadedAuthoredChildren(this);
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
	const TArray<FGameplayTag> Owners = BuildExpectedSecureOwners(this, GetProperties());
	return TerritoryHierarchyPolicy::AreAllChildrenOwnedBy(Owners, Faction);
}

FGameplayTag ATerritoryDistrict::GetMajorityPropertyOwner() const
{
	const TArray<FGameplayTag> Owners = BuildExpectedSecureOwners(this, GetProperties());
	return TerritoryHierarchyPolicy::FindStrictMajorityOwner(Owners);
}

int32 ATerritoryDistrict::GetEffectiveIncome() const
{
	const FGameplayTag DistrictOwner = GetOwningFaction();
	if (!DistrictOwner.IsValid()) return 0;

	int32 TotalIncome = 0;
	for (ATerritoryVolume* Volume : GetProperties())
	{
		if (const ATerritoryProperty* Property = Cast<ATerritoryProperty>(Volume);
			Property && Property->IsOwnedByFaction(DistrictOwner))
		{
			TotalIncome += Property->GetEffectiveIncome();
		}
	}
	return TotalIncome;
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
}

void ATerritoryProperty::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
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
	UTerritoryRegistrySubsystem* Registry = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;

	FGameplayTag ParentTag = GetParentTerritoryTag();
	if (ParentTag.IsValid())
	{
		ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Parent);
		return IsExpectedAuthoredChild(District, this) ? District : nullptr;
	}

	// Never infer hierarchy from a gameplay-tag prefix. The District Definition's
	// Places array is the sole authority and the synchronizer writes this parent tag.
	return nullptr;
}

void ATerritoryProperty::OnPropertyCaptured_Implementation(FGameplayTag NewOwner)
{
	if (ShouldLogHierarchyOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[PropertyCapture] %s captured by %s"),
			*GetTerritoryTag().ToString(), *NewOwner.ToString());
	}

	// Reset upgrade level on capture by a new faction — use SetUpgradeLevel to
	// ensure income recalculation and logging are triggered.
	if (HasAuthority() && UpgradeLevel > 0)
	{
		SetUpgradeLevel(0);
	}
}

void ATerritoryProperty::OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	Super::OnOwnershipChanged_Implementation(OldOwner, NewOwner);

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
	// P2-12: Calculate in int64 to prevent overflow, then clamp to int32 range
	const int64 Cost = static_cast<int64>(UpgradeCostPerLevel) * static_cast<int64>(UpgradeLevel + 1);
	return static_cast<int32>(FMath::Clamp(Cost, 0LL, static_cast<int64>(TNumericLimits<int32>::Max())));
}

int32 ATerritoryProperty::GetEffectiveIncome() const
{
	int64 BaseIncome = FMath::Max(0, GetPeriodicIncome());

	// Capital district income multiplier
	ATerritoryDistrict* District = GetOwningDistrict();
	if (District && District->IsCapitalDistrict())
	{
		const double Multiplier = FMath::IsFinite(District->CapitalIncomeMultiplier)
			? FMath::Max(0.0, static_cast<double>(District->CapitalIncomeMultiplier)) : 0.0;
		BaseIncome = static_cast<int64>(FMath::Clamp(
			static_cast<double>(BaseIncome) * Multiplier, 0.0, static_cast<double>(MAX_int32)));
	}

	const int64 UpgradeIncome = static_cast<int64>(FMath::Max(0, UpgradeLevel))
		* FMath::Max(0, IncomeBonusPerLevel);
	return static_cast<int32>(FMath::Min<int64>(BaseIncome + UpgradeIncome, MAX_int32));
}

bool ATerritoryProperty::TryUpgrade(AActor* Requester)
{
	if (!HasAuthority() || IsGameplayMutationInProgress() || !CanUpgrade() || !IsValid(Requester)
		|| !GetWorld() || Requester->GetWorld() != GetWorld()
		|| !IsAvailableForGameplay() || GetTerritoryState() != ETerritoryState::Claimed) return false;

	FGameplayTag OwnerFaction = GetOwningFaction();
	if (!OwnerFaction.IsValid()) return false;
	if (!UTerritoryBlueprintLibrary::IsActorInFaction(this, Requester, OwnerFaction)) return false;

	int32 Cost = GetUpgradeCost();

	UTerritoryEconomySubsystem* Economy = GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>();
	if (!Economy) return false;

	// Check if faction can afford the upgrade
	if (!Economy->CanActorAfford(Requester, Cost)) return false;

	TGuardValue<bool> PurchaseGuard(bPurchaseInProgress, true);
	const int32 OldLevel = UpgradeLevel;
	const int32 PurchasedLevel = OldLevel + 1;
	// Commit the plugin-owned fields silently before Narrative publishes its debit.
	// No callbacks run between this write and the wallet's final validation/write;
	// a rejected debit can therefore restore the unobserved staged level safely.
	FString Reason = FString::Printf(TEXT("Property upgrade %s level %d→%d"),
		*GetTerritoryTag().ToString(), OldLevel, PurchasedLevel);
	ApplyUpgradeLevel(PurchasedLevel);
	if (Cost > 0 && !Economy->TryDebitCurrency(Requester, Cost, OwnerFaction,
		Reason, ETerritoryTransactionType::UpgradeCost))
	{
		ApplyUpgradeLevel(OldLevel);
		return false;
	}

	// A campaign reload or actor destruction can supersede the transaction from a
	// callback. Do not publish stale upgrade success into that replacement state.
	if (!IsValid(this) || IsActorBeingDestroyed() || UpgradeLevel != PurchasedLevel
		|| GetOwningFaction() != OwnerFaction || GetTerritoryState() != ETerritoryState::Claimed)
	{
		return false;
	}
	PublishUpgradeLevelChange(OldLevel);
	if (!IsValid(this) || IsActorBeingDestroyed() || UpgradeLevel != PurchasedLevel) return false;

	if (ShouldLogPropertyEconomy())
	{
		UE_LOG(LogTerritory, Log, TEXT("[PropertyUpgrade] %s upgraded to level %d (cost: %d, faction: %s)"),
			*GetTerritoryTag().ToString(), UpgradeLevel, Cost, *OwnerFaction.ToString());
	}

	return true;
}

void ATerritoryProperty::SetUpgradeLevel(int32 NewLevel)
{
	if (!HasAuthority() || bPurchaseInProgress) return;
	const int32 OldLevel = UpgradeLevel;
	ApplyUpgradeLevel(NewLevel);
	PublishUpgradeLevelChange(OldLevel);
}

void ATerritoryProperty::ApplyUpgradeLevel(int32 NewLevel)
{
	const int32 OldLevel = UpgradeLevel;
	UpgradeLevel = FMath::Clamp(NewLevel, 0, FMath::Max(0, MaxUpgradeLevel));

	if (OldLevel != UpgradeLevel)
	{
		FGameplayTag OwnerFaction = GetOwningFaction();
		UTerritoryEconomySubsystem* Economy = GetWorld()
			? GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
		if (OwnerFaction.IsValid())
		{
			if (Economy)
			{
				Economy->MarkFactionDirty(OwnerFaction);
			}
		}
		if (Economy)
		{
			Economy->RefreshProductionSite(this);
		}
		ForceNetUpdate();
	}
}

void ATerritoryProperty::PublishUpgradeLevelChange(int32 OldLevel)
{
	if (OldLevel != UpgradeLevel)
	{
		if (ShouldLogPropertyEconomy())
		{
			UE_LOG(LogTerritory, Log, TEXT("[PropertyUpgrade] %s set to level %d (was %d)"),
				*GetTerritoryTag().ToString(), UpgradeLevel, OldLevel);
		}

		OnUpgradeLevelChanged(UpgradeLevel);
	}
}
