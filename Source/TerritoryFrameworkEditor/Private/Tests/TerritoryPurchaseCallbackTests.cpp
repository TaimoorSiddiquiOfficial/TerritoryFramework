#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPurchaseCallbacks,
	"TerritoryFramework.Economy.Regression.UpgradePurchaseCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPurchaseCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Purchase world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>();
	Property->SetActorGUID_Implementation(FGuid(171, 172, 173, 174));
	FTerritoryOwnershipData Claimed = Property->GetOwnershipData();
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Claimed.ControlProgress = 1.f;
	Claimed.DesiredGuardCount = 0;
	TestTrue(TEXT("Property is claimed through the existing authority"), Property->CommitOwnershipData(Claimed));
	ATerritoryGuardCharacter* Account = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
	UNarrativeInventoryComponent* Inventory = Account->GetInventoryComponent();
	Inventory->SetCurrency(2000);
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	Inventory->OnCurrencyChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord SavedProperty;
	bool bCallbackRan = false;
	bool bNestedPurchase = true;
	bool bCompetingCommit = true;
	int32 CallbackLevel = INDEX_NONE;
	Probe->CurrencyCallback = [&]()
	{
		if (bCallbackRan) return;
		bCallbackRan = true;
		CallbackLevel = Property->GetUpgradeLevel();
		TestTrue(TEXT("Narrative saves the property inside the currency callback"), Save->CreateActorRecord(Property, SavedProperty));
		Inventory->PrepareForSave_Implementation();
		bNestedPurchase = Property->TryUpgrade(Account);
		Property->SetUpgradeLevel(3);
		FTerritoryOwnershipData Changed = Claimed;
		Changed.PeriodicIncome = 777;
		bCompetingCommit = Property->CommitOwnershipData(Changed);
	};
	TestTrue(TEXT("The paid upgrade completes"), Property->TryUpgrade(Account));
	TestTrue(TEXT("The real Narrative currency callback ran"), bCallbackRan);
	TestEqual(TEXT("Currency observers see the committed purchased level"), CallbackLevel, 1);
	TestFalse(TEXT("A callback cannot buy another level at the old price"), bNestedPurchase);
	TestFalse(TEXT("A callback cannot commit competing territory data during the purchase"), bCompetingCommit);
	TestEqual(TEXT("One request purchases exactly one level"), Property->GetUpgradeLevel(), 1);
	TestEqual(TEXT("One request debits exactly the quoted cost"), Inventory->GetCurrency(), 1500);
	Probe->CurrencyCallback = nullptr;
	Property->SetUpgradeLevel(0);
	Inventory->SetCurrency(0);
	Save->LoadActorFromRecord(Property, SavedProperty);
	Inventory->Load_Implementation();
	TestEqual(TEXT("Save inside the currency callback restores the purchased level"), Property->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Save inside the currency callback restores the same debit"), Inventory->GetCurrency(), 1500);
	TestTrue(TEXT("A later independent upgrade remains available"), Property->TryUpgrade(Account));
	TestEqual(TEXT("The later level uses its higher cost"), Inventory->GetCurrency(), 500);
	TestFalse(TEXT("Insufficient funds leave the next level unpurchased"), Property->TryUpgrade(Account));
	TestEqual(TEXT("Failure does not alter the level"), Property->GetUpgradeLevel(), 2);
	TestEqual(TEXT("Failure does not alter the account"), Inventory->GetCurrency(), 500);
	Property->SetRole(ROLE_SimulatedProxy);
	Property->UpgradeCostPerLevel = 0;
	TestFalse(TEXT("Even free purchases require server authority"), Property->TryUpgrade(Account));
	Property->SetRole(ROLE_Authority);
	TestTrue(TEXT("A free server upgrade remains supported"), Property->TryUpgrade(Account));
	TestEqual(TEXT("Free upgrade preserves the Narrative balance"), Inventory->GetCurrency(), 500);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
