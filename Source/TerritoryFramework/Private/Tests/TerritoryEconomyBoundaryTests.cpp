#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Engine/World.h"
#include "Items/NarrativeItem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFProductionInt64OverflowRegression,
	"TerritoryFramework.Production.Regression.ScaledQuantityRejectsInt64Overflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFProductionInt64OverflowRegression::RunTest(const FString& Parameters)
{
	FTerritoryResourceRate Rate;
	Rate.ItemClass = UNarrativeItem::StaticClass();
	Rate.QuantityPerCycle = 1;
	Rate.QuantityPerUpgradeLevel = 1 << 30;
	int32 Quantity = 99;
	// (2^60 + 1) * 16 wraps to 16 on the affected build despite exceeding int64.
	TestFalse(TEXT("Overflow cannot wrap an impossible production order into 16 items"),
		UTerritoryProductionProfile::CalculateScaledQuantity(Rate, 1 << 30, 16, Quantity));
	TestEqual(TEXT("Rejected production clears the result"), Quantity, 0);
	Rate.QuantityPerCycle = MAX_int32;
	Rate.QuantityPerUpgradeLevel = 0;
	TestTrue(TEXT("The exact supported maximum remains valid"),
		UTerritoryProductionProfile::CalculateScaledQuantity(Rate, 0, 1, Quantity));
	TestEqual(TEXT("Maximum quantity is preserved"), Quantity, MAX_int32);
	TestFalse(TEXT("Two maximum-size batches are rejected before multiplication"),
		UTerritoryProductionProfile::CalculateScaledQuantity(Rate, 0, 2, Quantity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPropertyEconomyBoundaryRegression,
	"TerritoryFramework.PropertyBenefits.Regression.FreeUpgradeAndIncomeBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPropertyEconomyBoundaryRegression::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Property economy world exists"), World)) return false;
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, Spawn);
	ATerritoryGuardCharacter* Requester = World->SpawnActor<ATerritoryGuardCharacter>(
		ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, Spawn);
	if (!TestNotNull(TEXT("Property exists"), Property)
		|| !TestNotNull(TEXT("Narrative faction requester exists"), Requester))
	{
		World->DestroyWorld(false);
		return false;
	}
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Requester)->AddFaction(Heroes);
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(31, 32, 33, 34);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->UpgradeCostPerLevel = 0;
	Definition->ApplyToTerritory(Property);
	FTerritoryOwnershipData State = Property->GetOwnershipData();
	State.OwningFaction = Heroes;
	State.State = ETerritoryState::Claimed;
	State.ControlProgress = 1.f;
	Property->CommitOwnershipData(State);
	TestEqual(TEXT("An authored free upgrade has zero cost"), Property->GetUpgradeCost(), 0);
	TestTrue(TEXT("A free upgrade succeeds without calling a positive-only debit"), Property->TryUpgrade(Requester));
	TestEqual(TEXT("A free upgrade commits one level"), Property->GetUpgradeLevel(), 1);

	State = Property->GetOwnershipData();
	State.Availability = ETerritoryAvailability::Locked;
	Property->CommitOwnershipData(State);
	TestFalse(TEXT("Story-locked Property rejects even a free upgrade"), Property->TryUpgrade(Requester));
	TestEqual(TEXT("Rejected upgrade preserves the level"), Property->GetUpgradeLevel(), 1);

	Property->UpgradeLevel = 2;
	Property->IncomeBonusPerLevel = MAX_int32;
	TestEqual(TEXT("Upgrade income saturates instead of wrapping negative"),
		Property->GetEffectiveIncome(), MAX_int32);
	World->DestroyWorld(false);
	return true;
}

#endif
