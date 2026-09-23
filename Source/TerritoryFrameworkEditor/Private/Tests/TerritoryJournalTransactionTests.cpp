#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/RichTextBlock.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "UI/TerritoryJournalWidget.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFJournalTransactionRefresh,
	"TerritoryFramework.UI.Regression.JournalRefreshesEqualBalanceHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFJournalTransactionRefresh::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	TArray<UWorld*> Worlds;
	auto MakeViewer = [&](UWorld* World)
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Worlds.Add(World);
		auto* PlayerState = World->SpawnActor<ANarrativePlayerState>();
		PlayerState->AddFaction(Heroes);
		auto* Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
		Character->SetRole(ROLE_Authority);
		Character->SetPlayerState(PlayerState);
		Character->GetInventoryComponent()->SetCurrency(500);
		auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		Controller->SetRole(ROLE_Authority);
		Controller->SetPlayerState(PlayerState);
		Controller->SetOwnedCharacter(Character);
		World->AddController(Controller);
		Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
		auto* Widget = NewObject<UTerritoryJournalWidget>(Controller);
		Widget->SetOwningPlayer(Controller);
		Widget->RichText_EarningsSummary = NewObject<URichTextBlock>(Widget);
		Widget->RichText_LossSummary = NewObject<URichTextBlock>(Widget);
		return Widget;
	};
	UWorld* Server = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Journal world exists"), Server)) return false;
	UTerritoryJournalWidget* Journal = MakeViewer(Server);
	Server->SetGameInstance(NewObject<UGameInstance>(GEngine));
	Server->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	Server->SetGameMode(FURL());
	auto* Economy = Server->GetSubsystem<UTerritoryEconomySubsystem>();
	auto* State = Server->SpawnActor<ATerritoryWorldState>();
	Journal->RefreshDistrictList();
	const FString Original = Journal->RichText_EarningsSummary->GetText().ToString();
	TestFalse(TEXT("No initial credit is rendered"), Original.Contains(TEXT("Audit credit")));
	auto* Controller = Journal->GetOwningPlayer();
	TestTrue(TEXT("Native account receives credit"), Economy->CreditCurrency(Controller, 80, Heroes, TEXT("Audit credit")));
	TestTrue(TEXT("Native account pays equal debit"), Economy->TryDebitCurrency(Controller, 80, Heroes, TEXT("Audit debit")));
	TestEqual(TEXT("Net wallet remains unchanged"), Economy->GetActorCurrency(Controller), 500);
	Journal->RefreshDistrictList();
	const FString WithTransactions = Journal->RichText_EarningsSummary->GetText().ToString();
	TestTrue(TEXT("Equal balance still refreshes credit audit"), WithTransactions.Contains(TEXT("Audit credit")));
	TestTrue(TEXT("Equal balance still refreshes debit audit"), Journal->RichText_LossSummary->GetText().ToString().Contains(TEXT("Audit debit")));
	const int32 Revision = Journal->LastOperationsRevision;
	Journal->RefreshDistrictList();
	TestEqual(TEXT("Unchanged data retains the cache revision"), Journal->LastOperationsRevision, Revision);
	State->ExportPersistentState();
	const TArray<FReplicatedTransaction> SavedProjection = State->ReplicatedTransactions;
	const TArray<FTerritoryTransaction> SavedHistory = Economy->GetAllTransactionHistory();
	Economy->RestoreTransactionHistory({});
	Journal->RefreshDistrictList();
	TestFalse(TEXT("An empty restored history removes old audit lines"),
		Journal->RichText_EarningsSummary->GetText().ToString().Contains(TEXT("Audit credit")));
	Economy->RestoreTransactionHistory(SavedHistory);
	Journal->RefreshDistrictList();
	TestEqual(TEXT("History restoration refreshes the same presentation"),
		Journal->RichText_EarningsSummary->GetText().ToString(), WithTransactions);

	// Separate client worlds exercise the replicated read path; socket transport
	// remains an integration gate, not a claim made by this fixture.
	for (int32 ClientIndex = 0; ClientIndex < 2; ++ClientIndex)
	{
		UWorld* Client = UWorld::CreateWorld(EWorldType::Game, false);
		auto* ClientJournal = MakeViewer(Client);
		auto* ClientState = Client->SpawnActor<ATerritoryWorldState>();
		Client->NextURL = TEXT("127.0.0.1");
		TestEqual(TEXT("Fixture uses the client read path"), Client->GetNetMode(), NM_Client);
		ClientJournal->RefreshDistrictList();
		ClientState->ReplicatedTransactions = SavedProjection;
		ClientJournal->RefreshDistrictList();
		TestTrue(TEXT("A joining client's journal refreshes replicated history"),
			ClientJournal->RichText_EarningsSummary->GetText().ToString().Contains(TEXT("Audit credit")));
		TestFalse(TEXT("Client journal cannot settle currency"), Client->GetSubsystem<UTerritoryEconomySubsystem>()->CreditCurrency(
			ClientJournal->GetOwningPlayer(), 80, Heroes, TEXT("Rejected client credit")));
		TestEqual(TEXT("Rendering cannot alter the received ledger"), ClientState->ReplicatedTransactions.Num(), SavedProjection.Num());
	}
	for (UWorld* World : Worlds)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	return true;
}

#endif
