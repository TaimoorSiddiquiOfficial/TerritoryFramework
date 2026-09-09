#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCurrencyCallbacks,
	"TerritoryFramework.Economy.Regression.CurrencyRestoreCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCurrencyCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Currency world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	ATerritoryGuardCharacter* Account = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Account)->AddFaction(Heroes);
	UNarrativeInventoryComponent* Inventory = Account->GetInventoryComponent();
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	Inventory->OnCurrencyChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);
	bool bCalled = false;
	bool bNestedAccepted = true;
	Inventory->SetCurrency(500);
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		bNestedAccepted = Economy->CreditCurrency(Account, 50, Heroes);
	};
	TestTrue(TEXT("Outer currency credit completes"), Economy->CreditCurrency(Account, 100, Heroes));
	TestFalse(TEXT("A currency callback cannot start a second Territory settlement"), bNestedAccepted);
	TestEqual(TEXT("One Territory payment credits once"), Inventory->GetCurrency(), 600);

	Probe->CurrencyCallback = nullptr;
	Inventory->SetCurrency(500);
	Economy->RestoreTransactionHistory({});
	bCalled = false;
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		Inventory->AddCurrency(-10); // An independent Native expense after the committed credit.
	};
	TestTrue(TEXT("A separate Native expense does not reject the original credit"), Economy->CreditCurrency(Account, 100, Heroes));
	TestEqual(TEXT("Native keeps its independent expense"), Inventory->GetCurrency(), 590);
	const auto History = Economy->GetAllTransactionHistory();
	if (TestEqual(TEXT("Only the Territory credit enters its ledger"), History.Num(), 1))
		TestEqual(TEXT("Ledger records the balance at the credit, before later Native expense"), History[0].BalanceAfter, 600);

	Probe->CurrencyCallback = nullptr;
	Inventory->SetCurrency(500);
	Inventory->PrepareForSave_Implementation();
	Economy->RestoreTransactionHistory({});
	bCalled = false;
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		Inventory->Load_Implementation();
	};
	TestFalse(TEXT("Native inventory load interrupts the old payment"), Economy->CreditCurrency(Account, 100, Heroes));
	TestEqual(TEXT("The loaded wallet wins"), Inventory->GetCurrency(), 500);
	TestTrue(TEXT("Interrupted payment adds no stale ledger row"), Economy->GetAllTransactionHistory().IsEmpty());

	Probe->CurrencyCallback = nullptr;
	Economy->RestoreTransactionHistory({});
	FTerritoryTransaction Loaded;
	Loaded.TransactionID = FGuid(91, 92, 93, 94);
	Loaded.Faction = Heroes;
	bCalled = false;
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		Economy->RestoreTransactionHistory({Loaded});
	};
	TestFalse(TEXT("Campaign history restore interrupts the old payment"), Economy->CreditCurrency(Account, 100, Heroes));
	const auto LoadedHistory = Economy->GetAllTransactionHistory();
	if (TestEqual(TEXT("The restored ledger is not extended by the old continuation"), LoadedHistory.Num(), 1))
		TestEqual(TEXT("The loaded transaction identity wins"), LoadedHistory[0].TransactionID, Loaded.TransactionID);
	Probe->CurrencyCallback = nullptr;
	Inventory->OnCurrencyChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);

	Economy->RestoreTransactionHistory({});
	ATerritoryWorldState* State = World->SpawnActor<ATerritoryWorldState>();
	Economy->OnTransactionRecorded.AddDynamic(Probe, &UTerritoryAuditEventProbe::TransactionRecorded);
	Probe->TransactionCallback = [&](const FTerritoryTransaction& Tx)
	{
		Economy->RestoreTransactionHistory({});
		State->OnTransactionRecordedLive(Tx);
	};
	TestEqual(TEXT("A history load inside ledger publication interrupts the receipt"),
		Economy->CreditCurrencyWithResult(Account, 10, Heroes).Status, ETerritoryCurrencyMutationStatus::Superseded);
	TestTrue(TEXT("A late WorldState listener cannot project the replaced transaction"), State->GetTransactionHistory(Heroes).IsEmpty());
	Probe->TransactionCallback = [&](const FTerritoryTransaction& Tx)
	{
		State->ExportPersistentState();
		State->OnTransactionRecordedLive(Tx);
	};
	TestTrue(TEXT("Saving a committed ledger during publication is supported"), Economy->CreditCurrency(Account, 10, Heroes));
	TestEqual(TEXT("Saved transaction is not duplicated by a late listener"), State->GetTransactionHistory(Heroes).Num(), 1);
	Probe->TransactionCallback = nullptr;
	Economy->OnTransactionRecorded.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::TransactionRecorded);

	TArray<ANarrativePlayerController*> Controllers;
	TArray<ANarrativePlayerCharacter*> Players;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		ANarrativePlayerController* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		ANarrativePlayerCharacter* Player = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel,
			FName(*FString::Printf(TEXT("CallbackPlayer%d"), Index)));
		ANarrativePlayerState* PlayerState = World->SpawnActor<ANarrativePlayerState>();
		Controller->SetRole(ROLE_Authority);
		Player->SetRole(ROLE_Authority);
		Controller->PlayerState = PlayerState;
		Player->SetPlayerState(PlayerState);
		PlayerState->AddFaction(Heroes);
		Controller->SetOwnedCharacter(Player);
		Controller->SetPawn(Player);
		World->AddController(Controller);
		Controllers.Add(Controller);
		Players.Add(Player);
	}
	UNarrativeInventoryComponent* First = Players[0]->GetInventoryComponent();
	UNarrativeInventoryComponent* Second = Players[1]->GetInventoryComponent();
	First->OnCurrencyChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);
	Economy->OnFactionUpkeepDeficit.AddDynamic(Probe, &UTerritoryAuditEventProbe::UpkeepDeficit);
	int32 Deficit = INDEX_NONE;
	Probe->UpkeepCallback = [&](FGameplayTag Faction, int32 Amount) { Deficit = Amount; };
	FTerritoryTreasury Treasury;
	Treasury.CostsPerTick = 100;
	Economy->RestoreTreasuryState({{Heroes, Treasury}});
	First->SetCurrency(60);
	Second->SetCurrency(60);
	bCalled = false;
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		Second->SetCurrency(0);
	};
	Economy->OnEconomyTick();
	TestEqual(TEXT("A callback expense on the next account is preserved"), Second->GetCurrency(), 0);
	TestEqual(TEXT("Upkeep reports only the unpaid remainder"), Deficit, 40);
	Probe->CurrencyCallback = nullptr;
	First->SetCurrency(60);
	Second->SetCurrency(60);
	Second->PrepareForSave_Implementation();
	bCalled = false;
	Deficit = INDEX_NONE;
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		Second->Load_Implementation();
	};
	Economy->OnEconomyTick();
	TestEqual(TEXT("Loading the later account stops the old upkeep cohort"), Second->GetCurrency(), 60);
	TestEqual(TEXT("Interrupted upkeep does not publish a deficit from old state"), Deficit, INDEX_NONE);
	Probe->CurrencyCallback = nullptr;
	First->SetCurrency(0);
	Second->SetCurrency(0);
	Second->PrepareForSave_Implementation();
	bCalled = false;
	Probe->CurrencyCallback = [&]()
	{
		if (bCalled) return;
		bCalled = true;
		Second->Load_Implementation();
	};
	Economy->CreditCurrencyToFaction(Heroes, 100, ETerritoryIncomePayoutPolicy::EqualSplitOnlineMembers);
	TestEqual(TEXT("A reload stops payouts to later members"), Second->GetCurrency(), 0);
	Probe->CurrencyCallback = nullptr;
	First->OnCurrencyChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::CurrencyChanged);
	Economy->OnFactionUpkeepDeficit.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::UpkeepDeficit);
	for (ANarrativePlayerController* Controller : Controllers) World->RemoveController(Controller);

	Account->SetRole(ROLE_SimulatedProxy);
	TestEqual(TEXT("A client cannot make a payment"), Economy->DebitCurrencyWithResult(Account, 1, Heroes).Status,
		ETerritoryCurrencyMutationStatus::Rejected);
	Account->SetRole(ROLE_Authority);
	Inventory->SetCurrency(MAX_int32);
	TestEqual(TEXT("Currency overflow is rejected before Native writes"), Economy->CreditCurrencyWithResult(Account, 1, Heroes).Status,
		ETerritoryCurrencyMutationStatus::Rejected);
	TestEqual(TEXT("Invalid debit amount is rejected without integer overflow"), Economy->DebitCurrencyWithResult(Account, MIN_int32, Heroes).Status,
		ETerritoryCurrencyMutationStatus::Rejected);
	Inventory->SetCurrency(100);
	Economy->RestoreTransactionHistory({});
	Economy->MaxTransactionHistory = 2;
	for (int32 Index = 0; Index < 3; ++Index) Economy->CreditCurrency(Account, 1, Heroes);
	TestEqual(TEXT("Manual payments cannot exceed the configured history limit between ticks"), Economy->GetAllTransactionHistory().Num(), 2);
	Inventory->PrepareForSave_Implementation();
	Inventory->SetCurrency(0);
	Inventory->Load_Implementation();
	TestEqual(TEXT("A later Native save/load preserves confirmed payments"), Inventory->GetCurrency(), 103);
	for (const FName Name : {FName(TEXT("DebitCurrencyWithResult")), FName(TEXT("CreditCurrencyWithResult"))})
	{
		const UFunction* Function = Economy->FindFunction(Name);
		TestTrue(TEXT("Receipt API is callable from Blueprint and marked server-only"), Function
			&& Function->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
