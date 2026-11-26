#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryPlayerSaveProbe.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPlayerSaveIsolation,
	"TerritoryFramework.Persistence.Regression.PlayerSaveIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPlayerSaveIsolation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FString Slot = TEXT("TerritoryRegression_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Save->UpdateSaveObject(true);
	TStrongObjectPtr<UNarrativeSave> Campaign(Save->GetSaveObject());
	const FGuid SentinelID = FGuid::NewGuid();
	FNarrativeActorRecord Sentinel;
	Sentinel.ActorName = TEXT("CampaignSentinel");
	Sentinel.ActorGUID = SentinelID;
	Campaign->RecordMap.Add(SentinelID, Sentinel);
	FStrProperty* NameField = FindFProperty<FStrProperty>(Save->GetClass(), TEXT("CurrentSaveName"));
	FIntProperty* SlotField = FindFProperty<FIntProperty>(Save->GetClass(), TEXT("CurrentSaveSlot"));
	NameField->SetPropertyValue_InContainer(Save, TEXT("CampaignIdentity"));
	SlotField->SetPropertyValue_InContainer(Save, 7);
	auto AssertCampaign = [&]()
	{
		TestTrue(TEXT("Campaign object identity is preserved"), Save->GetSaveObject() == Campaign.Get());
		TestEqual(TEXT("Campaign records remain intact"), Campaign->RecordMap.Num(), 1);
		TestTrue(TEXT("Campaign record remains available through Native lookup"), Save->DoesRecordExist(SentinelID));
		TestEqual(TEXT("Campaign slot name remains unchanged"), NameField->GetPropertyValue_InContainer(Save), FString(TEXT("CampaignIdentity")));
		TestEqual(TEXT("Campaign slot index remains unchanged"), SlotField->GetPropertyValue_InContainer(Save), 7);
		TestFalse(TEXT("Loading state is restored after return"), Save->IsLoading());
	};
	auto* PC = World->SpawnActor<APlayerController>();
	auto* PS = World->SpawnActor<APlayerState>();
	auto* Pawn = World->SpawnActor<ATerritoryPlayerSaveProbe>();
	PS->SetPlayerName(Slot);
	PC->SetPlayerState(PS);
	PC->Possess(Pawn);
	TestFalse(TEXT("Missing player slot is a clean failure"), Save->LoadPlayerOnlySave(PC));
	AssertCampaign();
	// Stop the unpatched baseline before later invalid-record cases enter Native's
	// unchecked byte reader. The failed identity assertion already reproduces F1.
	if (Save->GetSaveObject() != Campaign.Get()) return true;
	TStrongObjectPtr<UNarrativeSave> Player(NewObject<UNarrativeSave>(GetTransientPackage(), Campaign->GetClass()));
	Pawn->SavedValue = 42;
	Save->CreateActorRecord(Pawn, Player->PlayerData.PawnData);
	Save->CreateActorRecord(PS, Player->PlayerData.PlayerStateData);
	Save->CreateActorRecord(PC, Player->PlayerData.ControllerData);
	TestTrue(TEXT("Real player fixture saved"), UGameplayStatics::SaveGameToSlot(Player.Get(), Slot, 0));
	int32 Callbacks = 0;
	Pawn->LoadCallback = [&]()
	{
		++Callbacks;
		TestTrue(TEXT("Callback observes campaign, never the player slot"), Save->GetSaveObject() == Campaign.Get());
		TestTrue(TEXT("Callback sees loading scope"), Save->IsLoading());
		TestFalse(TEXT("Invalid reentrant request fails"), Save->LoadPlayerOnlySave(nullptr));
		TestTrue(TEXT("Nested failure retains outer loading scope"), Save->IsLoading());
		CollectGarbage(RF_NoFlags);
	};
	for (int32 Join = 0; Join < 3; ++Join)
	{
		Pawn->SavedValue = -1;
		TestTrue(TEXT("Existing player slot loads"), Save->LoadPlayerOnlySave(PC));
		TestEqual(TEXT("Player record is restored"), Pawn->SavedValue, 42);
		AssertCampaign();
	}
	TestEqual(TEXT("Exactly one load callback per join"), Callbacks, 3);
	Pawn->LoadCallback = nullptr;
	PC->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot restore server player state"), Save->LoadPlayerOnlySave(PC));
	PC->SetRole(ROLE_Authority);
	AssertCampaign();
	Player->PlayerData.PawnData.ActorSoftClass = APlayerController::StaticClass();
	UGameplayStatics::SaveGameToSlot(Player.Get(), Slot, 0);
	TestFalse(TEXT("Incompatible record class rejected before callbacks"), Save->LoadPlayerOnlySave(PC));
	AssertCampaign();
	Player->PlayerData = FNarrativeSavePlayer();
	UGameplayStatics::SaveGameToSlot(Player.Get(), Slot, 0);
	TestFalse(TEXT("Empty player records rejected"), Save->LoadPlayerOnlySave(PC));
	AssertCampaign();
	const TArray<uint8> CorruptBytes = {0, 1, 2, 3};
	UGameplayStatics::SaveDataToSlot(CorruptBytes, Slot, 0);
	TestFalse(TEXT("Corrupt slot fails without invalidating campaign"), Save->LoadPlayerOnlySave(PC));
	AssertCampaign();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFMultiplayerLoadValidation,
	"TerritoryFramework.Persistence.Regression.MultiplayerLoadValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFMultiplayerLoadValidation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	const FString Slot = TEXT("TerritoryTravelRegression_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		World->NextURL.Empty();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	TestFalse(TEXT("Missing slot does not report travel success"), Save->MultiplayerLoad(Slot));
	TestTrue(TEXT("Missing slot does not queue travel"), World->NextURL.IsEmpty());
	Save->UpdateSaveObject(true);
	TStrongObjectPtr<UNarrativeSave> Fixture(NewObject<UNarrativeSave>(GetTransientPackage(), Save->GetSaveObject()->GetClass()));
	Fixture->LevelName = TEXT("/Engine/Maps/Entry");
	UGameplayStatics::SaveGameToSlot(Fixture.Get(), Slot, 0);
	TestFalse(TEXT("Travel requires an authoritative GameMode"), Save->MultiplayerLoad(Slot));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	Fixture->LevelName = TEXT("/Game/MapThatDoesNotExist_TerritoryRegression");
	UGameplayStatics::SaveGameToSlot(Fixture.Get(), Slot, 0);
	TestFalse(TEXT("Invalid destination rejected"), Save->MultiplayerLoad(Slot));
	Fixture->LevelName = TEXT("Entry");
	UGameplayStatics::SaveGameToSlot(Fixture.Get(), Slot, 0);
	TestTrue(TEXT("Existing valid slot with legacy short map name queues travel"), Save->MultiplayerLoad(Slot));
	TestTrue(TEXT("Queued travel resolves the package and retains slot routing"), World->NextURL.Contains(TEXT("/Engine/Maps/Entry?SaveGameName=")));
	TestFalse(TEXT("A second request during pending travel is rejected"), Save->MultiplayerLoad(Slot));
	World->NextURL.Empty();
	const TArray<uint8> CorruptBytes = {0, 1, 2, 3};
	UGameplayStatics::SaveDataToSlot(CorruptBytes, Slot, 0);
	TestFalse(TEXT("Corrupt existing slot is rejected even without map change"), Save->MultiplayerLoad(Slot, false));
	return true;
}

#endif
