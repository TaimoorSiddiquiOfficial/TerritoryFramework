#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UObject/StrongObjectPtr.h"

bool UTerritoryEconomySubsystem::TryGetCurrencyCapitalMultiplier(const ATerritoryProperty* Property,
	float& OutMultiplier) const
{
	OutMultiplier = 1.f;
	if (!IsValid(Property)) return false;
	FTerritoryProductionSiteRecord Site = ProductionSites.FindRef(Property->GetTerritoryGUID());
	Site.TerritoryGUID = Property->GetTerritoryGUID();
	Site.TerritoryTag = Property->GetTerritoryTag();
	Site.ParentTerritoryTag = Property->GetParentTerritoryTag();
	if (!ResolveCurrencyCapitalPolicy(Site)) return false;
	OutMultiplier = Site.CurrencyCapitalMultiplier;
	return true;
}

void UTerritoryEconomySubsystem::RefreshCurrencyInputs(const ATerritoryProperty* Property,
	FTerritoryProductionSiteRecord& Site) const
{
	const FTerritoryOwnershipData Data = Property->GetOwnershipData();
	Site.CurrencyRatesVersion = 1;
	Site.BaseCurrencyIncome = FMath::Max(0, Data.PeriodicIncome);
	Site.CurrencyIncomePerUpgrade = FMath::Max(0, Property->IncomeBonusPerLevel);
	Site.CurrencyGuardCost = FMath::Max(0, Data.GuardCost);
	Site.DesiredGarrison = Property->GetDesiredGuardCount();
	Site.bAllowCurrencyIncome = Property->GetActiveStateGameplayRules().bAllowPeriodicIncome;
	Site.CurrencyMigrationIssue = FText::GetEmpty();
}

bool UTerritoryEconomySubsystem::ResolveCurrencyCapitalPolicy(FTerritoryProductionSiteRecord& Site) const
{
	if (!Site.ParentTerritoryTag.IsValid())
	{
		Site.CurrencyCapitalMultiplier = 1.f;
		Site.bCurrencyCapitalPolicyKnown = true;
		return true;
	}
	const UWorld* World = GetWorld();
	const auto* Registry = World ? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	const auto* District = Registry ? Cast<ATerritoryDistrict>(Registry->GetTerritoryByTag(Site.ParentTerritoryTag)) : nullptr;
	const auto AuthorsSite = [&Site](const UTerritoryDistrictDefinition* Definition)
	{
		return Definition && Definition->TerritoryTag == Site.ParentTerritoryTag
			&& Definition->Places.ContainsByPredicate([&Site](const UTerritoryPlaceDefinition* Child)
			{
				return Child && Child->StableTerritoryGUID == Site.TerritoryGUID
					&& Child->TerritoryTag == Site.TerritoryTag && Child->DerivedParentTerritoryTag == Site.ParentTerritoryTag;
			});
	};
	if (District)
	{
		if (!AuthorsSite(Cast<UTerritoryDistrictDefinition>(District->GetTerritoryDefinition())))
		{
			Site.bCurrencyCapitalPolicyKnown = false;
			return false;
		}
		Site.CurrencyCapitalMultiplier = District->IsCapitalDistrict() ? District->CapitalIncomeMultiplier : 1.f;
		Site.bCurrencyCapitalPolicyKnown = true;
	}
	else if (const auto* Directory = ATerritoryWorldState::FindTerritoryWorldState(World))
	{
		if (const auto* Definition = Cast<UTerritoryDistrictDefinition>(Directory->FindTerritoryDefinition(Site.ParentTerritoryTag)))
		{
			if (!AuthorsSite(Definition))
			{
				Site.bCurrencyCapitalPolicyKnown = false;
				return false;
			}
			Site.CurrencyCapitalMultiplier = Definition->bIsCapital ? FMath::Max(1.f, Definition->CapitalIncomeMultiplier) : 1.f;
			Site.bCurrencyCapitalPolicyKnown = true;
		}
	}
	return Site.bCurrencyCapitalPolicyKnown;
}

bool UTerritoryEconomySubsystem::MigrateCurrencyInputs(FTerritoryProductionSiteRecord& Site) const
{
	if (Site.CurrencyRatesVersion == 1) return true;
	const auto Fail = [&Site](const TCHAR* Reason)
	{
		Site.CurrencyMigrationIssue = FText::FromString(Reason);
		return false;
	};
	if (Site.CurrencyRatesVersion != 0) return Fail(TEXT("Unsupported currency rate version; migration is required."));
	UWorld* World = GetWorld();
	UNarrativeSaveSubsystem* Save = World ? World->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
	const auto* SaveObject = Save ? Save->GetSaveObject() : nullptr;
	const FNarrativeActorRecord* SavedRecord = SaveObject ? SaveObject->RecordMap.Find(Site.TerritoryGUID) : nullptr;
	if (!SavedRecord || SavedRecord->ByteData.IsEmpty())
		return Fail(TEXT("Currency migration needs the exact Native saved Place record; contribution is withheld."));
	// Asset loading can run callbacks. Own the record rather than retaining a map reference.
	const FNarrativeActorRecord Record = *SavedRecord;
	const UTerritoryDefinition* Definition = Site.TerritoryDefinition.LoadSynchronous();
	if (!Definition)
	{
		if (const auto* Directory = ATerritoryWorldState::FindTerritoryWorldState(World))
			Definition = Directory->FindTerritoryDefinition(Site.TerritoryTag);
	}
	if (!Definition || !Definition->IsA<UTerritoryPlaceDefinition>()
		|| Definition->StableTerritoryGUID != Site.TerritoryGUID || Record.ActorGUID != Site.TerritoryGUID)
		return Fail(TEXT("Currency migration needs a matching Place Definition and saved identity."));
	UClass* ActorClass = Record.ActorSoftClass.LoadSynchronous();
	if (!ActorClass || !ActorClass->IsChildOf(ATerritoryProperty::StaticClass()) || ActorClass->HasAnyClassFlags(CLASS_Abstract))
		return Fail(TEXT("Currency migration cannot read this Native saved actor class."));
	// Decode Native's existing actor archive into a detached transient reader. No world,
	// actor spawn, BeginPlay, Native Load callback, inventory mutation or campaign write.
	TStrongObjectPtr<ATerritoryProperty> ReaderActor(NewObject<ATerritoryProperty>(GetTransientPackage(), ActorClass));
	if (!Definition->ApplyToTerritory(ReaderActor.Get())) return Fail(TEXT("Currency migration Definition was rejected."));
	FMemoryReader Reader(Record.ByteData);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;
	ReaderActor->Serialize(Archive);
	if (Archive.IsError()) return Fail(TEXT("Currency migration could not decode the Native actor record."));
	const FTerritoryOwnershipData Data = ReaderActor->GetOwnershipData();
	Site.OwnerFaction = Data.OwningFaction;
	Site.TerritoryState = Data.State;
	Site.Availability = Data.Availability;
	Site.UpgradeLevel = FMath::Max(0, ReaderActor->UpgradeLevel);
	Site.ParentTerritoryTag = Definition->DerivedParentTerritoryTag;
	Site.TerritoryDefinition = const_cast<UTerritoryDefinition*>(Definition);
	Site.StateRulesVersion = 1;
	RefreshCurrencyInputs(ReaderActor.Get(), Site);
	return true;
}
