#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryUpgradeDefinition.h"
#include "Core/TerritoryUpgradeTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "Interaction/TerritoryUpgradeBroker.h"
#include "SkillTrees/TreePerk.h"
#include "SkillTrees/TreeSkill.h"
#include "Skills/TerritoryUpgradePerk.h"
#include "Skills/TerritoryUpgradeSkill.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryUpgradeSubsystem.h"
#include "Tales/TerritoryUpgradeEvents.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

namespace
{
	FTerritoryUpgradeCostEntry MakeCost(ETerritoryUpgradePayment Payment, bool bOptional,
		int32 CurrencyAmount = 0)
	{
		FTerritoryUpgradeCostEntry Entry;
		Entry.Payment = Payment;
		Entry.bOptional = bOptional;
		Entry.CurrencyAmount = CurrencyAmount;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeDefinitionContract,
	"TerritoryFramework.Upgrade.DefinitionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeDefinitionContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UTerritoryUpgradeDefinition* Definition = NewObject<UTerritoryUpgradeDefinition>();
	FTerritoryUpgradeTier Tier;
	Tier.Level = 2;
	Tier.RequiredFactionTier = 2;
	Definition->Tiers.Add(Tier);

	FTerritoryUpgradeTier Found;
	TestTrue(TEXT("Authored tier is found by target level"), Definition->FindTier(2, Found));
	TestEqual(TEXT("Tier requirement survives the lookup"), Found.RequiredFactionTier, 2);
	TestFalse(TEXT("A missing tier is not invented"), Definition->FindTier(1, Found));
	TestNull(TEXT("Const lookup also rejects a missing tier"), Definition->FindTierConst(3));
	TestTrue(TEXT("Perk-tier enforcement is on by default"), Definition->bRequireFactionPerkTier);

	const UTerritoryUpgradeBalanceProfile* Balance = GetDefault<UTerritoryUpgradeBalanceProfile>();
	TestNotNull(TEXT("Balance profile CDO exists"), Balance);
	if (Balance)
	{
		TestTrue(TEXT("AI catch-up is opt-in by default"), Balance->bAutoUpgradeAIFactions);
		TestEqual(TEXT("Default AI offset is one tier behind the player"), Balance->TierOffset, -1);
		TestEqual(TEXT("Default maximum auto tier is three"), Balance->MaxAutoTier, 3);
		TestTrue(TEXT("AI Place upgrades are enabled by default"), Balance->bUpgradeAIPlaces);
		TestTrue(TEXT("Evaluation interval is coarse, not per tick"), Balance->EvaluationInterval >= 1.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradePaymentRoutes,
	"TerritoryFramework.Upgrade.PaymentRoutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradePaymentRoutes::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FTerritoryUpgradeTier FreeTier;
	FreeTier.Costs.Add(MakeCost(ETerritoryUpgradePayment::Free, false));
	TerritoryUpgrade::FTerritoryUpgradePaymentPlan Plan;
	FString Failure;
	TestTrue(TEXT("A Free route passes without a requester"),
		TerritoryUpgrade::BuildPaymentPlan(FreeTier, nullptr, nullptr, nullptr, nullptr, Failure, Plan));
	TestTrue(TEXT("A Free route produces a free plan"), Plan.bFree);

	FTerritoryUpgradeTier CurrencyTier;
	CurrencyTier.Costs.Add(MakeCost(ETerritoryUpgradePayment::Currency, false, 500));
	TestFalse(TEXT("A required currency route needs an account"),
		TerritoryUpgrade::BuildPaymentPlan(CurrencyTier, nullptr, nullptr, nullptr, nullptr, Failure, Plan));
	TestTrue(TEXT("The missing-currency failure names the cost"), Failure.Contains(TEXT("currency")));

	FTerritoryUpgradeTier OptionalCurrencyOnly;
	OptionalCurrencyOnly.Costs.Add(MakeCost(ETerritoryUpgradePayment::Currency, true, 500));
	TestFalse(TEXT("Optional routes still need one satisfiable path"),
		TerritoryUpgrade::BuildPaymentPlan(OptionalCurrencyOnly, nullptr, nullptr, nullptr, nullptr, Failure, Plan));

	FTerritoryUpgradeTier OptionalQuest;
	OptionalQuest.Costs.Add(MakeCost(ETerritoryUpgradePayment::ConditionOnly, true));
	TestTrue(TEXT("A condition-only route satisfies the optional rule without material cost"),
		TerritoryUpgrade::BuildPaymentPlan(OptionalQuest, nullptr, nullptr, nullptr, nullptr, Failure, Plan));
	TestTrue(TEXT("Condition-only leaves the plan free"), Plan.bFree);

	FTerritoryUpgradeTier Mixed;
	Mixed.Costs.Add(MakeCost(ETerritoryUpgradePayment::Free, false));
	Mixed.Costs.Add(MakeCost(ETerritoryUpgradePayment::ConditionOnly, true));
	Mixed.Costs.Add(MakeCost(ETerritoryUpgradePayment::Currency, true, 900));
	TestTrue(TEXT("Required Free plus one satisfied optional route passes"),
		TerritoryUpgrade::BuildPaymentPlan(Mixed, nullptr, nullptr, nullptr, nullptr, Failure, Plan));
	TestTrue(TEXT("The satisfied optional path adds no currency"), Plan.CurrencyTotal == 0);

	FTerritoryUpgradeTier MissingItem;
	FTerritoryUpgradeCostEntry Item = MakeCost(ETerritoryUpgradePayment::Item, false);
	Item.ItemClass = nullptr;
	Item.ItemQuantity = 3;
	MissingItem.Costs.Add(Item);
	TestFalse(TEXT("An item route without an item class is rejected"),
		TerritoryUpgrade::BuildPaymentPlan(MissingItem, nullptr, nullptr, nullptr, nullptr, Failure, Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradePlaceTierGate,
	"TerritoryFramework.Upgrade.PlaceTierGateAndScriptedOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradePlaceTierGate::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Upgrade world"), World))
	{
		return false;
	}
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	const FGameplayTag PlaceTag =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));

	UTerritoryUpgradeDefinition* UpgradeDefinition = NewObject<UTerritoryUpgradeDefinition>();
	FTerritoryUpgradeTier TierOne;
	TierOne.Level = 1;
	TierOne.RequiredFactionTier = 1;
	TierOne.Costs.Add(MakeCost(ETerritoryUpgradePayment::Free, false));
	UpgradeDefinition->Tiers.Add(TierOne);

	UTerritoryPlaceDefinition* PlaceDefinition = NewObject<UTerritoryPlaceDefinition>();
	PlaceDefinition->TerritoryTag = PlaceTag;
	PlaceDefinition->StableTerritoryGUID = FGuid(411, 412, 413, 414);
	PlaceDefinition->MaxUpgradeLevel = 3;
	PlaceDefinition->UpgradeDefinition = UpgradeDefinition;

	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	if (!TestNotNull(TEXT("Place actor"), Place))
	{
		return false;
	}
	PlaceDefinition->ApplyToTerritory(Place);
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Claimed);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Place);

	UTerritoryUpgradeSubsystem* Upgrades = World->GetSubsystem<UTerritoryUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Upgrade subsystem"), Upgrades))
	{
		return false;
	}
	TestNotNull(TEXT("Place carries the authored definition"), Place->GetUpgradeDefinition());

	FString Failure;
	TestFalse(TEXT("Level 1 is blocked before the faction perks unlock tier 1"),
		Place->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The rejected upgrade leaves level 0"), Place->GetUpgradeLevel(), 0);

	TestTrue(TEXT("Faction tier can be granted on the server"), Upgrades->SetFactionTier(Heroes, 1));
	TestTrue(TEXT("A free level 1 upgrade commits after the perk gate"), 
		Place->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The committed upgrade persists level 1"), Place->GetUpgradeLevel(), 1);
	TestFalse(TEXT("A level with no authored tier is rejected"),
		Place->ApplyScriptedUpgrade(2, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("A missing tier does not advance the level"), Place->GetUpgradeLevel(), 1);

	ATerritoryProperty* ForeignPlace = World->SpawnActor<ATerritoryProperty>();
	PlaceDefinition->ApplyToTerritory(ForeignPlace);
	FTerritoryOwnershipData BanditClaim;
	BanditClaim.OwningFaction = Bandits;
	BanditClaim.State = ETerritoryState::Claimed;
	ForeignPlace->CommitOwnershipData(BanditClaim);

	TestFalse(TEXT("A faction cannot upgrade a foreign Place by default"),
		ForeignPlace->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestTrue(TEXT("A scripted foreign upgrade is allowed when explicitly authored"),
		ForeignPlace->ApplyScriptedUpgrade(1, Heroes, true, true, nullptr, Failure));
	TestEqual(TEXT("The foreign scripted upgrade commits the level"), ForeignPlace->GetUpgradeLevel(), 1);

	UTerritoryPlaceCapturedEvent* CaptureEvent = NewObject<UTerritoryPlaceCapturedEvent>();
	CaptureEvent->MinOldLevel = 1;
	PlaceDefinition->PlaceCapturedEvents.Add(CaptureEvent);
	FTerritoryOwnershipData Recaptured;
	Recaptured.OwningFaction = Heroes;
	Recaptured.State = ETerritoryState::Claimed;
	ForeignPlace->CommitOwnershipData(Recaptured);
	TestEqual(TEXT("Capture retains the level-0 reset"), ForeignPlace->GetUpgradeLevel(), 0);
	TestEqual(TEXT("Capture event carries the pre-reset level"), CaptureEvent->OldUpgradeLevel, 1);
	TestEqual(TEXT("Capture event carries the captured Place"),
		CaptureEvent->CapturedPlace.Get(), static_cast<ATerritoryVolume*>(ForeignPlace));
	TestEqual(TEXT("Capture event carries the new owner"), CaptureEvent->NewOwner, Heroes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeWorldStateRoundTrip,
	"TerritoryFramework.Upgrade.WorldStateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeWorldStateRoundTrip::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Upgrade save world"), World))
	{
		return false;
	}
	ATerritoryWorldState* WorldState = World->SpawnActor<ATerritoryWorldState>();
	UTerritoryUpgradeSubsystem* Upgrades = World->GetSubsystem<UTerritoryUpgradeSubsystem>();
	if (!TestNotNull(TEXT("World state actor"), WorldState) || !TestNotNull(TEXT("Upgrade subsystem"), Upgrades))
	{
		return false;
	}
	const FGameplayTag Bandits =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	TestTrue(TEXT("Server tier write"), Upgrades->SetFactionTier(Bandits, 2));
	TestEqual(TEXT("WorldState projection receives the tier"),
		WorldState->GetFactionUpgradeTier(Bandits), 2);

	WorldState->PrepareForSave_Implementation();
	Upgrades->ImportFactionTiers({});
	TestEqual(TEXT("Import clears the live map"), Upgrades->GetFactionTier(Bandits), 0);
	WorldState->Load_Implementation();
	TestEqual(TEXT("Save/load restores the faction tier"), Upgrades->GetFactionTier(Bandits), 2);
	TestEqual(TEXT("The replicated array has one faction row"),
		WorldState->GetAllFactionUpgradeTiers().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradePerkAndCaptureContract,
	"TerritoryFramework.Upgrade.PerkAndCaptureEventContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradePerkAndCaptureContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("Upgrade perk extends the vendor perk"),
		UTerritoryUpgradePerk::StaticClass()->IsChildOf(UTreePerk::StaticClass()));
	const UTerritoryUpgradePerk* Perk = GetDefault<UTerritoryUpgradePerk>();
	TestNotNull(TEXT("Upgrade perk CDO"), Perk);
	if (Perk)
	{
		TestEqual(TEXT("Upgrade I is the default unlocked tier"), Perk->UnlockedTier, 1);
		TestEqual(TEXT("A tier perk is single-level"), Perk->MaxLevels, 1);
	}
	TestNotNull(TEXT("SetPerkLevel remains the vendor extension point"),
		UTerritoryUpgradePerk::StaticClass()->FindFunctionByName(TEXT("SetPerkLevel")));
	const UTerritoryUpgradePerk_II* PerkTwo = GetDefault<UTerritoryUpgradePerk_II>();
	const UTerritoryUpgradePerk_III* PerkThree = GetDefault<UTerritoryUpgradePerk_III>();
	if (TestNotNull(TEXT("Upgrade II CDO"), PerkTwo) && TestNotNull(TEXT("Upgrade III CDO"), PerkThree))
	{
		TestEqual(TEXT("Upgrade II unlocks tier 2"), PerkTwo->UnlockedTier, 2);
		TestEqual(TEXT("Upgrade III unlocks tier 3"), PerkThree->UnlockedTier, 3);
		TestEqual(TEXT("Upgrade II requires Upgrade I"),
			PerkTwo->PrerequisitePerk.Get(), UTerritoryUpgradePerk_I::StaticClass());
		TestEqual(TEXT("Upgrade III requires Upgrade II"),
			PerkThree->PrerequisitePerk.Get(), UTerritoryUpgradePerk_II::StaticClass());
	}
	const UTerritoryUpgradeSkill* Skill = GetDefault<UTerritoryUpgradeSkill>();
	if (TestNotNull(TEXT("Territory skill CDO"), Skill))
	{
		TestEqual(TEXT("Territory skill authors three tiers"), Skill->Perks.Num(), 3);
		if (Skill->Perks.Num() == 3)
		{
			TestEqual(TEXT("Tier I perk class"),
				Skill->Perks[0].Perk.Get(), UTerritoryUpgradePerk_I::StaticClass());
			TestEqual(TEXT("Tier II perk class"),
				Skill->Perks[1].Perk.Get(), UTerritoryUpgradePerk_II::StaticClass());
			TestEqual(TEXT("Tier III perk class"),
				Skill->Perks[2].Perk.Get(), UTerritoryUpgradePerk_III::StaticClass());
			TestTrue(TEXT("Tier I links to Tier II"),
				Skill->Perks[0].LinkedTo.Contains(UTerritoryUpgradePerk_II::StaticClass()));
			TestTrue(TEXT("Tier II links to Tier III"),
				Skill->Perks[1].LinkedTo.Contains(UTerritoryUpgradePerk_III::StaticClass()));
		}
	}

	UTerritoryPlaceCapturedEvent* Event = NewObject<UTerritoryPlaceCapturedEvent>();
	Event->PlaceTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Event->NewOwnerFilter = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Event->MinOldLevel = 1;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ATerritoryProperty* Place = World ? World->SpawnActor<ATerritoryProperty>() : nullptr;
	if (!TestNotNull(TEXT("Capture payload Place"), Place))
	{
		return false;
	}
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = Event->PlaceTag;
	Definition->StableTerritoryGUID = FGuid(511, 512, 513, 514);
	Definition->ApplyToTerritory(Place);
	Event->SetPayload(Place,
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits")),
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes")), 2);
	TestTrue(TEXT("The capture payload matches its filters"), Event->Matches(Place));
	TestEqual(TEXT("The payload carries the old level"), Event->OldUpgradeLevel, 2);
	Event->MinOldLevel = 3;
	TestFalse(TEXT("The minimum old-level filter rejects a lower tier"), Event->Matches(Place));
	Event->ClearPayload();
	TestNull(TEXT("Clearing removes the transient Place"), Event->CapturedPlace.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeGateOffMigration,
	"TerritoryFramework.Upgrade.GateOffMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeGateOffMigration::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Migration world"), World))
	{
		return false;
	}
	const FGameplayTag PlaceTag =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));

	UTerritoryUpgradeDefinition* UpgradeDefinition = NewObject<UTerritoryUpgradeDefinition>();
	UpgradeDefinition->bRequireFactionPerkTier = false;
	FTerritoryUpgradeTier TierOne;
	TierOne.Level = 1;
	TierOne.RequiredFactionTier = 1;
	TierOne.Costs.Add(MakeCost(ETerritoryUpgradePayment::Free, false));
	UpgradeDefinition->Tiers.Add(TierOne);

	UTerritoryPlaceDefinition* PlaceDefinition = NewObject<UTerritoryPlaceDefinition>();
	PlaceDefinition->TerritoryTag = PlaceTag;
	PlaceDefinition->StableTerritoryGUID = FGuid(611, 612, 613, 614);
	PlaceDefinition->MaxUpgradeLevel = 3;
	PlaceDefinition->UpgradeDefinition = UpgradeDefinition;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	UTerritoryUpgradeSubsystem* Upgrades = World->GetSubsystem<UTerritoryUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Migration registry"), Registry)
		|| !TestNotNull(TEXT("Migration upgrade subsystem"), Upgrades))
	{
		return false;
	}

	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	if (!TestNotNull(TEXT("Migration Place"), Place))
	{
		return false;
	}
	PlaceDefinition->ApplyToTerritory(Place);
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Claimed);
	Registry->RegisterTerritory(Place);

	FString Failure;
	TestTrue(TEXT("Gate off lets a tier-0 faction reach level 1"),
		Place->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The gate-off upgrade commits level 1"), Place->GetUpgradeLevel(), 1);
	TestEqual(TEXT("The gate-off path does not fabricate a faction tier"),
		Upgrades->GetFactionTier(Heroes), 0);

	UpgradeDefinition->bRequireFactionPerkTier = true;
	// The registry rejects a second Place with the same tag in one world, so the gated
	// phase runs in its own world and keeps the two assertions independent.
	UWorld* GatedWorld = UWorld::CreateWorld(EWorldType::Game, false);
	UTerritoryRegistrySubsystem* GatedRegistry =
		GatedWorld ? GatedWorld->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	UTerritoryUpgradeSubsystem* GatedUpgrades =
		GatedWorld ? GatedWorld->GetSubsystem<UTerritoryUpgradeSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("Gated world"), GatedWorld)
		|| !TestNotNull(TEXT("Gated registry"), GatedRegistry)
		|| !TestNotNull(TEXT("Gated upgrade subsystem"), GatedUpgrades))
	{
		return false;
	}
	ATerritoryProperty* Blocked = GatedWorld->SpawnActor<ATerritoryProperty>();
	if (!TestNotNull(TEXT("Gated Place"), Blocked))
	{
		return false;
	}
	PlaceDefinition->ApplyToTerritory(Blocked);
	Blocked->CommitOwnershipData(Claimed);
	GatedRegistry->RegisterTerritory(Blocked);

	TestFalse(TEXT("Gate on blocks the same tier-0 faction"),
		Blocked->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The rejected gated upgrade leaves level 0"), Blocked->GetUpgradeLevel(), 0);
	TestTrue(TEXT("The rejection explains the faction tier gate"),
		Failure.Contains(TEXT("needs upgrade tier 1")));

	TestTrue(TEXT("The server grants the faction perk tier"),
		GatedUpgrades->SetFactionTier(Heroes, 1));
	TestTrue(TEXT("Gate on allows the upgrade after the perk tier"),
		Blocked->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The gated upgrade commits level 1"), Blocked->GetUpgradeLevel(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeMigrationWindow,
	"TerritoryFramework.Upgrade.MigrationWindowPreservesLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeMigrationWindow::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Legacy save world"), World))
	{
		return false;
	}
	const FGameplayTag PlaceTag =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));

	UTerritoryUpgradeDefinition* UpgradeDefinition = NewObject<UTerritoryUpgradeDefinition>();
	UpgradeDefinition->bRequireFactionPerkTier = true;
	for (int32 Level = 1; Level <= 3; ++Level)
	{
		FTerritoryUpgradeTier Tier;
		Tier.Level = Level;
		Tier.RequiredFactionTier = Level >= 3 ? 1 : 0;
		Tier.Costs.Add(MakeCost(ETerritoryUpgradePayment::Free, false));
		UpgradeDefinition->Tiers.Add(Tier);
	}

	UTerritoryPlaceDefinition* PlaceDefinition = NewObject<UTerritoryPlaceDefinition>();
	PlaceDefinition->TerritoryTag = PlaceTag;
	PlaceDefinition->StableTerritoryGUID = FGuid(621, 622, 623, 624);
	PlaceDefinition->MaxUpgradeLevel = 3;
	PlaceDefinition->UpgradeDefinition = UpgradeDefinition;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	UTerritoryUpgradeSubsystem* Upgrades = World->GetSubsystem<UTerritoryUpgradeSubsystem>();
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	if (!TestNotNull(TEXT("Legacy registry"), Registry)
		|| !TestNotNull(TEXT("Legacy upgrade subsystem"), Upgrades)
		|| !TestNotNull(TEXT("Legacy Place"), Place))
	{
		return false;
	}
	PlaceDefinition->ApplyToTerritory(Place);
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Claimed);
	Registry->RegisterTerritory(Place);

	// A save written before the faction gate existed can hold a Place level with faction tier 0.
	Place->SetUpgradeLevel(2);
	TestEqual(TEXT("The legacy save restores its Place level"), Place->GetUpgradeLevel(), 2);
	TestEqual(TEXT("The legacy save has no faction tier"), Upgrades->GetFactionTier(Heroes), 0);

	FString Failure;
	TestFalse(TEXT("Gate on blocks the next level for the legacy faction"),
		Place->ApplyScriptedUpgrade(3, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The rejected migration keeps the existing level"), Place->GetUpgradeLevel(), 2);
	TestTrue(TEXT("The migration rejection is explicit"),
		Failure.Contains(TEXT("needs upgrade tier 1")));

	UpgradeDefinition->bRequireFactionPerkTier = false;
	TestTrue(TEXT("The migration window allows the legacy Place to reach level 3"),
		Place->ApplyScriptedUpgrade(3, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The migration window commits level 3"), Place->GetUpgradeLevel(), 3);
	TestEqual(TEXT("The migration window does not grant the perk tier"),
		Upgrades->GetFactionTier(Heroes), 0);

	FTerritoryOwnershipData Captured;
	Captured.OwningFaction = Bandits;
	Captured.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Captured);
	TestEqual(TEXT("Capture still resets the upgrade level to zero"), Place->GetUpgradeLevel(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeAuthorityReplication,
	"TerritoryFramework.Upgrade.AuthorityAndReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeAuthorityReplication::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Authority world"), World))
	{
		return false;
	}
	const FGameplayTag Bandits =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));

	ATerritoryUpgradeBroker* Broker = World->SpawnActor<ATerritoryUpgradeBroker>();
	if (!TestNotNull(TEXT("Upgrade broker actor"), Broker))
	{
		return false;
	}
	const ATerritoryUpgradeBroker* BrokerDefaults = GetDefault<ATerritoryUpgradeBroker>();
	TestTrue(TEXT("The broker CDO replicates to clients"),
		BrokerDefaults && BrokerDefaults->GetIsReplicated());
	TestFalse(TEXT("The broker has no owner faction before possession"),
		Broker->GetOwnerFaction().IsValid());
	TestEqual(TEXT("The broker reports tier 0 without an owning faction"),
		Broker->GetOwnerFactionTier(), 0);

	TestFalse(TEXT("Tier 0 is rejected before any RPC"), Broker->RequestUpgradeTier(0));
	TestFalse(TEXT("Tier 4 is rejected before any RPC"), Broker->RequestUpgradeTier(4));
	TestFalse(TEXT("Validation rejects a null perk class"),
		Broker->ServerBuyPerk_Validate(TSubclassOf<UTerritoryUpgradePerk>()));
	TestFalse(TEXT("Validation rejects a vendor perk that is not a Territory perk"),
		Broker->ServerBuyPerk_Validate(UTreePerk::StaticClass()));
	TestTrue(TEXT("Validation accepts Territory Upgrade I"),
		Broker->ServerBuyPerk_Validate(UTerritoryUpgradePerk_I::StaticClass()));

	const FProperty* TiersProperty = ATerritoryWorldState::StaticClass()
		->FindPropertyByName(TEXT("ReplicatedFactionUpgradeTiers"));
	if (TestNotNull(TEXT("The world state exposes the faction tier array"), TiersProperty))
	{
		TestTrue(TEXT("Faction tiers are replicated"),
			TiersProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("Faction tiers run the OnRep projection"),
			TiersProperty->HasAnyPropertyFlags(CPF_RepNotify));
		TestTrue(TEXT("Faction tiers are saved"),
			TiersProperty->HasAnyPropertyFlags(CPF_SaveGame));
		TestTrue(TEXT("Faction tiers are a Blueprint read model"),
			TiersProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
	}
	TestNotNull(TEXT("The tier projection callback is reflected"),
		ATerritoryWorldState::StaticClass()->FindFunctionByName(TEXT("OnRep_UpgradeTiers")));

	UTerritoryUpgradeSubsystem* Upgrades = World->GetSubsystem<UTerritoryUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Authority upgrade subsystem"), Upgrades))
	{
		return false;
	}
	FReplicatedFactionUpgradeTier Imported;
	Imported.Faction = Bandits;
	Imported.Tier = 2;
	Upgrades->ImportFactionTiers({Imported});
	TestEqual(TEXT("Import projects the replicated tier to the read model"),
		Upgrades->GetFactionTier(Bandits), 2);
	TestEqual(TEXT("Import replaces the local map"),
		Upgrades->GetAllFactionTiers().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeAIBalanceContract,
	"TerritoryFramework.Upgrade.AIBalanceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeAIBalanceContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("AI balance world"), World))
	{
		return false;
	}
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));

	ATerritoryWorldState* WorldState = World->SpawnActor<ATerritoryWorldState>();
	UTerritoryUpgradeSubsystem* Upgrades = World->GetSubsystem<UTerritoryUpgradeSubsystem>();
	if (!TestNotNull(TEXT("AI balance world state"), WorldState)
		|| !TestNotNull(TEXT("AI balance upgrade subsystem"), Upgrades))
	{
		return false;
	}

	// The balance profile is an authored WorldState property; wire it through reflection because
	// this test does not own the protected serialized member.
	UTerritoryUpgradeBalanceProfile* Profile = NewObject<UTerritoryUpgradeBalanceProfile>();
	FObjectProperty* BalanceProperty = CastField<FObjectProperty>(
		ATerritoryWorldState::StaticClass()->FindPropertyByName(TEXT("UpgradeBalanceProfile")));
	if (TestNotNull(TEXT("The world state authors a balance profile"), BalanceProperty))
	{
		BalanceProperty->SetObjectPropertyValue_InContainer(WorldState, Profile);
	}
	TestTrue(TEXT("The test profile is wired to the world state"),
		WorldState->GetUpgradeBalanceProfile() == Profile);

	TestTrue(TEXT("Server tier write for the AI faction"), Upgrades->SetFactionTier(Bandits, 5));
	TestFalse(TEXT("A repeated identical tier is an idempotent no-op"),
		Upgrades->SetFactionTier(Bandits, 5));
	TestTrue(TEXT("An oversized tier write is clamped"), Upgrades->SetFactionTier(Bandits, 200));
	TestEqual(TEXT("The tier is clamped to 99"), Upgrades->GetFactionTier(Bandits), 99);
	TestTrue(TEXT("Server tier write for the player faction"), Upgrades->SetFactionTier(Heroes, 1));

	const TArray<FReplicatedFactionUpgradeTier> First = Upgrades->GetAllFactionTiers();
	const TArray<FReplicatedFactionUpgradeTier> Second = Upgrades->GetAllFactionTiers();
	TestEqual(TEXT("Two factions are exported"), First.Num(), 2);
	if (First.Num() == 2 && Second.Num() == 2)
	{
		TestEqual(TEXT("Export order is deterministic"), Second[0].Faction, First[0].Faction);
		TestEqual(TEXT("Export tier is deterministic"), Second[0].Tier, First[0].Tier);
		TestEqual(TEXT("Export order is deterministic"), Second[1].Faction, First[1].Faction);
		TestEqual(TEXT("Export tier is deterministic"), Second[1].Tier, First[1].Tier);
		TestTrue(TEXT("Export is sorted by faction tag"),
			First[0].Faction.ToString() < First[1].Faction.ToString());
	}

	FReplicatedFactionUpgradeTier Invalid;
	Invalid.Tier = 7;
	FReplicatedFactionUpgradeTier DuplicateOld;
	DuplicateOld.Faction = Bandits;
	DuplicateOld.Tier = 2;
	FReplicatedFactionUpgradeTier DuplicateNew;
	DuplicateNew.Faction = Bandits;
	DuplicateNew.Tier = 3;
	Upgrades->ImportFactionTiers({Invalid, DuplicateOld, DuplicateNew});
	TestEqual(TEXT("Invalid faction rows are dropped"), Upgrades->GetAllFactionTiers().Num(), 1);
	TestEqual(TEXT("A duplicate row keeps the last value"), Upgrades->GetFactionTier(Bandits), 3);

	Upgrades->EvaluateAIBalance();
	TestEqual(TEXT("AI catch-up does not invent tiers without a player faction"),
		Upgrades->GetFactionTier(Bandits), 3);
	TestEqual(TEXT("AI catch-up does not add rows without a player faction"),
		Upgrades->GetAllFactionTiers().Num(), 1);

	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	if (TestNotNull(TEXT("AI balance Place"), Place))
	{
		Upgrades->NotifyPlaceCaptured(Place);
		TestEqual(TEXT("A capture notification is a safe no-op without a player faction"),
			Upgrades->GetFactionTier(Bandits), 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradePaymentExecutionRollback,
	"TerritoryFramework.Upgrade.PaymentExecutionRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradePaymentExecutionRollback::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	FString Failure;

	TerritoryUpgrade::FTerritoryUpgradePaymentPlan FreePlan;
	TestTrue(TEXT("A free plan executes without a requester"),
		TerritoryUpgrade::ExecutePaymentPlan(FreePlan, nullptr, Heroes, TEXT("test"), Failure));

	Failure.Reset();
	TerritoryUpgrade::FTerritoryUpgradePaymentPlan CurrencyPlan;
	CurrencyPlan.bFree = false;
	CurrencyPlan.CurrencyTotal = 500;
	TestFalse(TEXT("A currency plan without a requester fails cleanly"),
		TerritoryUpgrade::ExecutePaymentPlan(CurrencyPlan, nullptr, Heroes, TEXT("test"), Failure));
	TestTrue(TEXT("The currency failure is explicit"), Failure.Contains(TEXT("currency")));

	Failure.Reset();
	TerritoryUpgrade::FTerritoryUpgradePaymentPlan ItemPlan;
	ItemPlan.bFree = false;
	ItemPlan.Items.Emplace(TSubclassOf<UNarrativeItem>(), 2);
	TestFalse(TEXT("An item plan without an inventory fails cleanly"),
		TerritoryUpgrade::ExecutePaymentPlan(ItemPlan, nullptr, Heroes, TEXT("test"), Failure));
	TestTrue(TEXT("The item failure is explicit"), Failure.Contains(TEXT("item")));

	TerritoryUpgrade::RefundPaymentPlan(CurrencyPlan, nullptr, Heroes, TEXT("rollback"));
	TerritoryUpgrade::RefundPaymentPlan(ItemPlan, nullptr, Heroes, TEXT("rollback"));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Payment world"), World))
	{
		return false;
	}
	const FGameplayTag PlaceTag =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	UTerritoryUpgradeDefinition* UpgradeDefinition = NewObject<UTerritoryUpgradeDefinition>();
	UpgradeDefinition->bRequireFactionPerkTier = false;
	FTerritoryUpgradeTier TierOne;
	TierOne.Level = 1;
	TierOne.RequiredFactionTier = 0;
	TierOne.Costs.Add(MakeCost(ETerritoryUpgradePayment::Currency, false, 500));
	UpgradeDefinition->Tiers.Add(TierOne);

	UTerritoryPlaceDefinition* PlaceDefinition = NewObject<UTerritoryPlaceDefinition>();
	PlaceDefinition->TerritoryTag = PlaceTag;
	PlaceDefinition->StableTerritoryGUID = FGuid(631, 632, 633, 634);
	PlaceDefinition->MaxUpgradeLevel = 3;
	PlaceDefinition->UpgradeDefinition = UpgradeDefinition;

	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	if (!TestNotNull(TEXT("Payment Place"), Place))
	{
		return false;
	}
	PlaceDefinition->ApplyToTerritory(Place);
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Claimed);

	Failure.Reset();
	TestFalse(TEXT("A paid upgrade without a payable requester is rejected"),
		Place->TryUpgradeToLevel(1, nullptr, false, false, Failure));
	TestEqual(TEXT("The rejected payment does not advance the level"), Place->GetUpgradeLevel(), 0);
	TestTrue(TEXT("The payment rejection is explicit"), Failure.Contains(TEXT("currency")));

	TestTrue(TEXT("The free scripted path bypasses the authored currency cost"),
		Place->ApplyScriptedUpgrade(1, Heroes, false, true, nullptr, Failure));
	TestEqual(TEXT("The free scripted upgrade commits level 1"), Place->GetUpgradeLevel(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUpgradeBlueprintContract,
	"TerritoryFramework.Upgrade.BlueprintContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUpgradeBlueprintContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	auto CheckProperty = [this](const UStruct* Struct, FName Name, EPropertyFlags Flags,
		const TCHAR* Label)
	{
		const FProperty* Property = Struct ? Struct->FindPropertyByName(Name) : nullptr;
		if (TestNotNull(Label, Property))
		{
			TestTrue(Label, Property->HasAnyPropertyFlags(Flags));
		}
	};
	auto CheckFunction = [this](UClass* Class, FName Name, EFunctionFlags Flags, const TCHAR* Label)
	{
		const UFunction* Function = Class ? Class->FindFunctionByName(Name) : nullptr;
		if (TestNotNull(Label, Function))
		{
			TestTrue(Label, Function->HasAnyFunctionFlags(Flags));
		}
	};

	CheckProperty(UTerritoryUpgradeDefinition::StaticClass(), TEXT("Tiers"),
		CPF_Edit | CPF_BlueprintVisible | CPF_BlueprintReadOnly,
		TEXT("Upgrade tiers are authored and Blueprint-readable"));
	CheckProperty(UTerritoryUpgradeDefinition::StaticClass(), TEXT("bRequireFactionPerkTier"),
		CPF_Edit | CPF_BlueprintVisible | CPF_BlueprintReadOnly,
		TEXT("The migration switch is authored and Blueprint-readable"));
	CheckProperty(FTerritoryUpgradeTier::StaticStruct(), TEXT("Level"),
		CPF_Edit | CPF_BlueprintReadOnly, TEXT("A tier level is authored and Blueprint-readable"));
	CheckProperty(FTerritoryUpgradeTier::StaticStruct(), TEXT("RequiredFactionTier"),
		CPF_Edit | CPF_BlueprintReadOnly,
		TEXT("A tier faction requirement is authored and Blueprint-readable"));
	CheckProperty(FTerritoryUpgradeCostEntry::StaticStruct(), TEXT("Payment"),
		CPF_Edit | CPF_BlueprintReadOnly, TEXT("A cost payment type is authored and Blueprint-readable"));
	CheckProperty(FReplicatedFactionUpgradeTier::StaticStruct(), TEXT("Faction"),
		CPF_SaveGame | CPF_BlueprintReadOnly, TEXT("A persisted faction tier row is saved"));
	CheckProperty(FReplicatedFactionUpgradeTier::StaticStruct(), TEXT("Tier"),
		CPF_SaveGame | CPF_BlueprintReadOnly, TEXT("A persisted faction tier value is saved"));

	CheckProperty(ATerritoryProperty::StaticClass(), TEXT("UpgradeDefinition"),
		CPF_Transient | CPF_BlueprintReadOnly,
		TEXT("The runtime upgrade definition stays transient and readable"));
	CheckProperty(ATerritoryProperty::StaticClass(), TEXT("UpgradeLevel"),
		CPF_SaveGame | CPF_RepNotify,
		TEXT("The Place level is saved and replicated with a notify"));

	CheckFunction(ATerritoryProperty::StaticClass(), TEXT("TryUpgradeToLevel"),
		FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly,
		TEXT("TryUpgradeToLevel is a Blueprint server mutation"));
	CheckFunction(ATerritoryProperty::StaticClass(), TEXT("TryUpgrade"),
		FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly,
		TEXT("TryUpgrade is a Blueprint server mutation"));
	CheckFunction(ATerritoryProperty::StaticClass(), TEXT("GetUpgradeLevel"),
		FUNC_BlueprintPure, TEXT("GetUpgradeLevel is a side-effect-free query"));

	CheckFunction(UTerritoryUpgradeSubsystem::StaticClass(), TEXT("SetFactionTier"),
		FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly,
		TEXT("SetFactionTier is a Blueprint server mutation"));
	CheckFunction(UTerritoryUpgradeSubsystem::StaticClass(), TEXT("GetFactionTier"),
		FUNC_BlueprintPure, TEXT("GetFactionTier is a side-effect-free query"));
	CheckFunction(UTerritoryUpgradeSubsystem::StaticClass(), TEXT("ApplyScriptedUpgrade"),
		FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly,
		TEXT("ApplyScriptedUpgrade is a Blueprint server mutation"));
	CheckFunction(UTerritoryUpgradeSubsystem::StaticClass(), TEXT("RequestBuyPerk"),
		FUNC_BlueprintCallable, TEXT("RequestBuyPerk is the Blueprint client entry point"));
	CheckFunction(UTerritoryUpgradeSubsystem::StaticClass(), TEXT("GetUpgradeBroker"),
		FUNC_BlueprintPure, TEXT("GetUpgradeBroker is a side-effect-free query"));

	CheckFunction(ATerritoryUpgradeBroker::StaticClass(), TEXT("RequestUpgradeTier"),
		FUNC_BlueprintCallable, TEXT("RequestUpgradeTier is the Blueprint UI entry point"));
	CheckFunction(ATerritoryUpgradeBroker::StaticClass(), TEXT("GetOwnerFaction"),
		FUNC_BlueprintPure, TEXT("GetOwnerFaction is a side-effect-free query"));
	CheckFunction(ATerritoryUpgradeBroker::StaticClass(), TEXT("GetOwnerFactionTier"),
		FUNC_BlueprintPure, TEXT("GetOwnerFactionTier is a side-effect-free query"));
	CheckFunction(ATerritoryUpgradeBroker::StaticClass(), TEXT("ServerBuyPerk"),
		FUNC_Net | FUNC_NetReliable, TEXT("The perk purchase is a reliable server RPC"));

	CheckFunction(UTerritoryUpgradeDefinition::StaticClass(), TEXT("FindTier"),
		FUNC_BlueprintPure, TEXT("FindTier is a side-effect-free query"));
	CheckFunction(ATerritoryWorldState::StaticClass(), TEXT("GetFactionUpgradeTier"),
		FUNC_BlueprintPure, TEXT("GetFactionUpgradeTier is a side-effect-free query"));
	CheckFunction(ATerritoryWorldState::StaticClass(), TEXT("GetAllFactionUpgradeTiers"),
		FUNC_BlueprintPure, TEXT("GetAllFactionUpgradeTiers is a side-effect-free query"));
	TestNull(TEXT("The world state tier writer is a native server path, not a Blueprint RPC"),
		ATerritoryWorldState::StaticClass()->FindFunctionByName(TEXT("SetFactionUpgradeTier")));
	return true;
}

#endif
