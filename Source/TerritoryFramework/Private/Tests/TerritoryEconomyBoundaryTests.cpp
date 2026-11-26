#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Economy/TerritoryProductionProfile.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Items/NarrativeItem.h"
#include "Items/InventoryComponent.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
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
	Property->MaxUpgradeLevel = -3;
	Property->SetUpgradeLevel(2);
	TestEqual(TEXT("Invalid upgrade caps cannot create negative saved levels"), Property->GetUpgradeLevel(), 0);
	ATerritoryProperty* Detached = NewObject<ATerritoryProperty>();
	TestNull(TEXT("Detached Property can query its missing District safely"), Detached->GetOwningDistrict());
	TestNull(TEXT("Detached District can query its missing City safely"), NewObject<ATerritoryDistrict>()->GetOwningCity());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGarrisonAdmissionBoundaries,
	"TerritoryFramework.Guards.Regression.AvailabilityAndWorldAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGarrisonAdmissionBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World || !OtherWorld) return false;
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	ATerritoryGuardCharacter* Requester = World->SpawnActor<ATerritoryGuardCharacter>();
	ATerritoryGuardCharacter* ForeignRequester = OtherWorld->SpawnActor<ATerritoryGuardCharacter>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Cast<INarrativeTeamAgentInterface>(Requester)->AddFaction(Heroes);
	Cast<INarrativeTeamAgentInterface>(ForeignRequester)->AddFaction(Heroes);
	FTerritoryOwnershipData State = Territory->GetOwnershipData();
	State.OwningFaction = Heroes;
	State.State = ETerritoryState::Claimed;
	State.DesiredGuardCount = 1;
	Territory->CommitOwnershipData(State);
	FText Reason;
	int32 Cost = 0;
	TestTrue(TEXT("An unlocked owner may reduce a saved target with missing guard posts"),
		Territory->CanSetDesiredGuardCount(Requester, 0, Reason, Cost));
	TestFalse(TEXT("A same-faction actor in another world cannot manage this garrison"),
		Territory->CanSetDesiredGuardCount(ForeignRequester, 0, Reason, Cost));
	State.Availability = ETerritoryAvailability::Locked;
	Territory->CommitOwnershipData(State);
	TestFalse(TEXT("Locked garrison rejects a staffing query"),
		Territory->CanSetDesiredGuardCount(Requester, 0, Reason, Cost));
	TestFalse(TEXT("Locked garrison rejects the actual mutation"),
		Territory->TrySetDesiredGuardCount(Requester, 0).bSuccess);
	TestEqual(TEXT("Rejected staffing preserves the durable target"), Territory->GetDesiredGuardCount(), 1);
	FTerritoryProductionRule Recipe;
	Recipe.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	FTerritoryResourceRate Output;
	Output.ItemClass = UNarrativeItem::StaticClass();
	Output.QuantityPerCycle = 1;
	Recipe.Outputs.Add(Output);
	ForeignRequester->GetInventoryComponent()->SetCapacity(8);
	ForeignRequester->GetInventoryComponent()->SetWeightCapacity(100.f);
	FTerritoryProductionResult RecipeResult;
	TestFalse(TEXT("A recipe cannot mutate a Narrative inventory in another campaign world"),
		World->GetSubsystem<UTerritoryEconomySubsystem>()->ExecuteResourceRecipe(
			ForeignRequester, Heroes, Recipe, 0, 1, FGameplayTag(), RecipeResult));
	TestEqual(TEXT("Foreign inventory rejection occurs before adding items"),
		ForeignRequester->GetInventoryComponent()->GetTotalQuantityOfItemExact(
			TSoftClassPtr<UNarrativeItem>(UNarrativeItem::StaticClass()), false), 0);
	World->DestroyWorld(false);
	OtherWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistrictIncomeBoundaries,
	"TerritoryFramework.Economy.Regression.DistrictIncomeBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistrictIncomeBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("District income world exists"), World)) return false;
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	UTerritoryDistrictDefinition* Definition = NewObject<UTerritoryDistrictDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	Definition->StableTerritoryGUID = FGuid(191, 192, 193, 194);
	Definition->TerritoryActorClass = ATerritoryDistrict::StaticClass();
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>();
	TArray<ATerritoryProperty*> Properties;
	for (const TCHAR* Tag : {TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), TEXT("Territory.HavenReach.CastleHill.Farm")})
	{
		UTerritoryPlaceDefinition* PlaceDefinition = NewObject<UTerritoryPlaceDefinition>();
		PlaceDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(Tag);
		PlaceDefinition->StableTerritoryGUID = FGuid(191, 192, 193, 195 + Properties.Num());
		PlaceDefinition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		PlaceDefinition->DerivedParentTerritoryTag = Definition->TerritoryTag;
		Definition->Places.Add(PlaceDefinition);
		ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>();
		PlaceDefinition->ApplyToTerritory(Property);
		World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Property);
		FTerritoryOwnershipData State = Property->GetOwnershipData();
		State.OwningFaction = Heroes;
		State.State = ETerritoryState::Claimed;
		State.ControlProgress = 1.f;
		State.DesiredGuardCount = 0;
		State.PeriodicIncome = MAX_int32;
		Property->CommitOwnershipData(State);
		Properties.Add(Property);
	}
	Definition->ApplyToTerritory(District);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(District);
	District->SetDerivedControl(Heroes, ETerritoryState::Claimed);
	TestEqual(TEXT("The authored District resolves both loaded Places"), District->GetProperties().Num(), 2);
	TestEqual(TEXT("Two maximum property incomes saturate instead of wrapping negative"), District->GetEffectiveIncome(), MAX_int32);
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative saves the large income"), Save->CreateActorRecord(Properties[0], Record));
	FTerritoryOwnershipData Changed = Properties[0]->GetOwnershipData();
	Changed.PeriodicIncome = 0;
	Properties[0]->CommitOwnershipData(Changed);
	Save->LoadActorFromRecord(Properties[0], Record);
	TestEqual(TEXT("Narrative restores the original property rate"), Properties[0]->GetPeriodicIncome(), MAX_int32);
	TestEqual(TEXT("Reloaded rates preserve the bounded District sum"), District->GetEffectiveIncome(), MAX_int32);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->UnregisterTerritory(Properties[1]);
	TestEqual(TEXT("One remaining loaded Place retains its valid maximum income"), District->GetEffectiveIncome(), MAX_int32);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->UnregisterTerritory(Properties[0]);
	TestEqual(TEXT("An unloaded District has no phantom loaded income"), District->GetEffectiveIncome(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDurableCurrencyStreaming,
	"TerritoryFramework.Economy.Regression.DurableCurrencyAcrossStreaming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDurableCurrencyStreaming::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	auto* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(20261005, 4, 1, 1);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->IncomeBonusPerLevel = 5;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto& Post = Definition->GuardPosts.AddDefaulted_GetRef();
		Post.GuardPostID = FName(*FString::Printf(TEXT("CurrencyPost%d"), Index));
		Post.StableGuardPostGUID = FGuid(20261005, 4, 2, Index + 1);
	}
	auto* Property = World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Property);
	Registry->RegisterTerritory(Property);
	FTerritoryOwnershipData State = Property->GetOwnershipData();
	State.OwningFaction = Heroes;
	State.State = ETerritoryState::Claimed;
	State.ControlProgress = 1.f;
	State.PeriodicIncome = 40;
	State.GuardCost = 3;
	State.DesiredGuardCount = 2;
	TestTrue(TEXT("Currency fixture commits authored ownership and rates"), Property->CommitOwnershipData(State));
	Property->SetUpgradeLevel(2);
	const int32 TransactionsBeforeObservation = Economy->GetAllTransactionHistory().Num();
	// The production observer must refresh a dirty rate even when no recipe/cycle
	// advanced. Do not call RecalculateIncome or the currency settlement manually.
	TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
	World->GetTimerManager().Tick(0.001f);
	++GFrameCounter;
	World->GetTimerManager().Tick(1.01f);
	++GFrameCounter;
	TestEqual(TEXT("Rate observation cannot settle currency"), Economy->GetAllTransactionHistory().Num(), TransactionsBeforeObservation);
	const FTerritoryTreasury Before = Economy->GetFactionEconomy(Heroes);
	TestEqual(TEXT("Loaded income includes the saved upgrade level"), Before.IncomePerTick, 50);
	TestEqual(TEXT("Loaded upkeep uses desired rather than physical guards"), Before.CostsPerTick, 6);
	Registry->UnregisterTerritory(Property);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Unloaded owned Place continues currency income"), Economy->GetIncome(Heroes), Before.IncomePerTick);
	TestEqual(TEXT("Unloaded owned Place continues currency upkeep"), Economy->GetCosts(Heroes), Before.CostsPerTick);
	TestEqual(TEXT("Unloaded owned Place remains in durable economic count"), Economy->GetFactionEconomy(Heroes).TerritoryCount, 1);
	Registry->RegisterTerritory(Property);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Returning actor contributes only once"), Economy->GetIncome(Heroes), Before.IncomePerTick);
	TestEqual(TEXT("Rate refresh alone creates no currency settlement"), Economy->GetTransactionHistory(Heroes, 100).Num(), 0);
	Registry->UnregisterTerritory(Property);
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Save->UpdateSaveObject(true);
	TestTrue(TEXT("Native accepts the legacy fixture's saved actor"), Save->SaveSingleActor(Property));
	TestTrue(TEXT("Native produced the exact actor record before migration"), Save->GetSaveObject()->RecordMap.Contains(Definition->StableTerritoryGUID));
	FTerritoryProductionSiteRecord Legacy = Economy->GetProductionSite(Definition->TerritoryTag);
	Legacy.CurrencyRatesVersion = 0;
	Legacy.BaseCurrencyIncome = Legacy.CurrencyGuardCost = Legacy.DesiredGarrison = 0;
	Legacy.bAllowCurrencyIncome = false;
	const auto* Campaign = Save->GetSaveObject();
	const int32 RecordCount = Campaign->RecordMap.Num();
	Economy->RestoreProductionState({}, {Legacy}, {});
	Economy->RecalculateIncome(Heroes);
	TestTrue(FString::Printf(TEXT("Complete Native record migration has no issue: %s"),
		*Economy->GetProductionSite(Definition->TerritoryTag).CurrencyMigrationIssue.ToString()),
		Economy->GetProductionSite(Definition->TerritoryTag).CurrencyMigrationIssue.IsEmpty());
	TestEqual(TEXT("Legacy site migrates from Native saved ownership and upgrade data"), Economy->GetIncome(Heroes), 50);
	TestEqual(TEXT("Legacy site restores saved desired-garrison upkeep"), Economy->GetCosts(Heroes), 6);
	TestTrue(TEXT("Migration does not replace the campaign save"), Save->GetSaveObject() == Campaign);
	TestEqual(TEXT("Migration does not write Native actor records"), Campaign->RecordMap.Num(), RecordCount);
	TestEqual(TEXT("Migration records its completed version"), Economy->GetProductionSite(Definition->TerritoryTag).CurrencyRatesVersion, 1);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Repeated reconciliation cannot double the migrated contribution"), Economy->GetIncome(Heroes), 50);
	State = Property->GetOwnershipData();
	State.PeriodicIncome = State.GuardCost = State.DesiredGuardCount = 0;
	Property->CommitOwnershipData(State);
	Property->SetUpgradeLevel(0);
	Save->SaveSingleActor(Property);
	Economy->RestoreProductionState({}, {Legacy}, {});
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Legacy migration preserves explicit zero income"), Economy->GetIncome(Heroes), 0);
	TestEqual(TEXT("Legacy migration preserves explicit zero garrison/upkeep"), Economy->GetCosts(Heroes), 0);
	Save->GetSaveObject()->RecordMap.Remove(Definition->StableTerritoryGUID);
	Economy->RestoreProductionState({}, {Legacy}, {});
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Missing Native record cannot invent income"), Economy->GetIncome(Heroes), 0);
	TestFalse(TEXT("Missing Native data is an explicit migration issue"), Economy->GetProductionSite(Definition->TerritoryTag).CurrencyMigrationIssue.IsEmpty());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDurableCurrencyPolicies,
	"TerritoryFramework.Economy.Regression.DurableCurrencyPolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDurableCurrencyPolicies::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	auto* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	auto* Directory = World->SpawnActor<ATerritoryWorldState>();
	const auto Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const auto Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* DistrictDefinition = NewObject<UTerritoryDistrictDefinition>();
	DistrictDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	DistrictDefinition->StableTerritoryGUID = FGuid(20261005, 4, 3, 1);
	DistrictDefinition->TerritoryActorClass = ATerritoryDistrict::StaticClass();
	DistrictDefinition->bIsCapital = true;
	DistrictDefinition->CapitalIncomeMultiplier = 2.f;
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(20261005, 4, 3, 2);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->DerivedParentTerritoryTag = DistrictDefinition->TerritoryTag;
	Definition->IncomeBonusPerLevel = 5;
	DistrictDefinition->Places.Add(Definition);
	Directory->RegisterDefinitionHierarchy(DistrictDefinition);
	auto* District = World->SpawnActor<ATerritoryDistrict>();
	DistrictDefinition->ApplyToTerritory(District);
	Registry->RegisterTerritory(District);
	auto* Property = World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Property);
	Registry->RegisterTerritory(Property);
	auto State = Property->GetOwnershipData();
	State.OwningFaction = Heroes; State.State = ETerritoryState::Claimed;
	State.PeriodicIncome = 40; State.DesiredGuardCount = 0;
	Property->CommitOwnershipData(State);
	Property->SetUpgradeLevel(2);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Capital multiplier applies to base before upgrade bonus"), Economy->GetIncome(Heroes), 90);
	TestEqual(TEXT("Loaded public rate and durable rate share policy"), Property->GetEffectiveIncome(), 90);
	Registry->UnregisterTerritory(Property);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Unloaded child retains capital policy"), Economy->GetIncome(Heroes), 90);
	auto ParentState = District->GetOwnershipData();
	ParentState.Availability = ETerritoryAvailability::Locked;
	District->CommitOwnershipData(ParentState);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Parent lock suspends an unloaded child's currency"), Economy->GetIncome(Heroes), 0);
	ParentState.Availability = ETerritoryAvailability::Unlocked;
	District->CommitOwnershipData(ParentState);
	District->CapitalIncomeMultiplier = 3.f;
	Economy->OnEconomyTick();
	TestEqual(TEXT("Next existing tick refreshes unloaded descendant capital policy"), Economy->GetIncome(Heroes), 130);
	DistrictDefinition->CapitalIncomeMultiplier = 3.f;
	Directory->PublishTerritorySummary(District);
	Registry->UnregisterTerritory(District);
	Economy->RecalculateIncome(Heroes);
	TestEqual(TEXT("Unloaded parent resolves authored policy and retained availability"), Economy->GetIncome(Heroes), 130);
	Registry->RegisterTerritory(Property);
	State.OwningFaction = Bandits;
	Property->CommitOwnershipData(State);
	Economy->RecalculateIncome(Heroes); Economy->RecalculateIncome(Bandits);
	TestEqual(TEXT("Ownership change removes the former owner's contribution"), Economy->GetIncome(Heroes), 0);
	TestEqual(TEXT("New owner receives reset-upgrade rate exactly once"), Economy->GetIncome(Bandits), 120);
	auto* Account = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Bandits);
	auto* Inventory = Account->GetInventoryComponent();
	Inventory->SetCurrency(0);
	Economy->IncomePayoutPolicy = ETerritoryIncomePayoutPolicy::SharedNarrativeAccount;
	Economy->RegisterFactionCurrencyAccount(Bandits, Economy->IncomePayoutPolicy, Account);
	Economy->OnEconomyTick();
	TestEqual(TEXT("Existing Native inventory receives one periodic payment"), Inventory->GetCurrency(), 120);
	Registry->UnregisterTerritory(Property); Registry->RegisterTerritory(Property);
	Economy->RecalculateIncome(Bandits);
	TestEqual(TEXT("Stream reconciliation cannot create another payment"), Inventory->GetCurrency(), 120);
	Economy->OnEconomyTick();
	TestEqual(TEXT("Next independent payout remains enabled"), Inventory->GetCurrency(), 240);
	Account->Destroy();
	const int32 TransactionsBeforeMissingAccount = Economy->GetTransactionHistory(Bandits, 500).Num();
	Economy->OnEconomyTick();
	TestEqual(TEXT("Missing account cannot invent a payout transaction"),
		Economy->GetTransactionHistory(Bandits, 500).Num(), TransactionsBeforeMissingAccount);
	Economy->RetireProductionSite(Definition->StableTerritoryGUID);
	TestTrue(TEXT("Scheduler row alone cannot erase a durable Place"), Economy->GetProductionSite(Definition->TerritoryTag).TerritoryGUID.IsValid());
	Directory->RetiredDirectoryGUIDs.Add(Definition->StableTerritoryGUID);
	Directory->RefreshStrategicDirectory();
	Economy->RecalculateIncome(Bandits);
	TestEqual(TEXT("Explicit directory retirement removes contribution despite a stale actor"), Economy->GetIncome(Bandits), 0);
	TestFalse(TEXT("Retired site cannot be resurrected by loaded actor refresh"), Economy->GetProductionSite(Definition->TerritoryTag).TerritoryGUID.IsValid());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
