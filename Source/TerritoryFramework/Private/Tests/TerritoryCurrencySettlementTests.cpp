#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Items/InventoryComponent.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCurrencySettlementCapacity,
	"TerritoryFramework.Economy.Regression.MemberCapacitySettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCurrencySettlementCapacity::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Currency world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	TArray<ANarrativePlayerController*> Controllers;
	TArray<ANarrativePlayerCharacter*> Players;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		// Native Narrative player classes leave stable identity to project subclasses.
		ANarrativePlayerController* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		ANarrativePlayerCharacter* Player = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel,
			FName(*FString::Printf(TEXT("SettlementPlayer%d"), Index)));
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
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	UNarrativeInventoryComponent* First = Players[0]->GetInventoryComponent();
	UNarrativeInventoryComponent* Second = Players[1]->GetInventoryComponent();
	First->SetCurrency(0);
	Second->SetCurrency(MAX_int32);
	const ETerritoryIncomePayoutPolicy Split = ETerritoryIncomePayoutPolicy::EqualSplitOnlineMembers;
	TestEqual(TEXT("Full later account cannot strand a payable remainder"),
		Economy->CreditCurrencyToFaction(Heroes, 10, Split, TEXT("Capacity regression")), 10);
	TestEqual(TEXT("Remaining eligible account receives all income"), First->GetCurrency(), 10);
	First->SetCurrency(0);
	Second->SetCurrency(MAX_int32 - 8);
	TestEqual(TEXT("A partially capped later account redistributes its excess"),
		Economy->CreditCurrencyToFaction(Heroes, 20, Split, TEXT("Partial capacity regression")), 20);
	TestEqual(TEXT("Redistribution returns to the earlier account with capacity"), First->GetCurrency(), 12);
	TestEqual(TEXT("The constrained account fills exactly"), Second->GetCurrency(), MAX_int32);
	First->SetCurrency(0);
	Second->SetCurrency(0);
	TestEqual(TEXT("Maximum payout does not overflow ceiling division"),
		Economy->CreditCurrencyToFaction(Heroes, MAX_int32, Split, TEXT("Maximum regression")), MAX_int32);
	TestEqual(TEXT("Every unit of the maximum payout is stored in real Narrative inventories"),
		static_cast<int64>(First->GetCurrency()) + Second->GetCurrency(), static_cast<int64>(MAX_int32));
	TestTrue(TEXT("Unconstrained split differs by at most one"), FMath::Abs(First->GetCurrency() - Second->GetCurrency()) <= 1);
	First->SetCurrency(MAX_int32);
	Second->SetCurrency(0);
	TestEqual(TEXT("Capacity routing also works in reversed account order"),
		Economy->CreditCurrencyToFaction(Heroes, 10, Split, TEXT("Reverse regression")), 10);
	TestEqual(TEXT("Second account receives available payout"), Second->GetCurrency(), 10);
	First->SetCurrency(100);
	Controllers[0]->SetPawn(World->SpawnActor<APawn>());
	TestTrue(TEXT("Driving controller debits its retained Narrative character account"),
		Economy->TryDebitCurrency(Controllers[0], 10, Heroes, TEXT("Driving regression")));
	TestEqual(TEXT("Driving debit reaches the player inventory"), First->GetCurrency(), 90);
	Players[0]->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Server bridge cannot claim a debit on a non-authoritative inventory"),
		Economy->TryDebitCurrency(Players[0], 10, Heroes, TEXT("Role regression")));
	TestEqual(TEXT("Rejected client inventory debit preserves its balance"), First->GetCurrency(), 90);
	Players[0]->SetRole(ROLE_Authority);
	First->PrepareForSave_Implementation();
	First->SetCurrency(0);
	First->Load_Implementation();
	TestEqual(TEXT("Narrative save/load preserves the settled player balance"), First->GetCurrency(), 90);
	for (ANarrativePlayerController* Controller : Controllers) World->RemoveController(Controller);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
