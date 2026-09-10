#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFWorldPartitionActorRecords,
	"TerritoryFramework.SaveLoad.Regression.StreamingWritesBeforeGuardCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFWorldPartitionActorRecords::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT
	{
		OtherWorld->DestroyWorld(false);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Save->UpdateSaveObject(true);
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->SetActorGUID_Implementation(FGuid(510, 1, 2, 3));
	State->SubscribeToLiveUpdates();
	ON_SCOPE_EXIT { State->UnsubscribeFromLiveUpdates(); };
	const auto Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const auto Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(510, 4, 5, 6);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->InitialOwningFaction = Heroes;
	Definition->InitialGuardCount = 0;
	FTerritoryGuardPostTemplate Row;
	Row.GuardPostID = TEXT("StreamingPost");
	Row.StableGuardPostGUID = FGuid(510, 7, 8, 9);
	Row.ReserveSlots = 3;
	Row.bAutoSpawnReserves = false;
	Definition->GuardPosts = {Row};
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Place);
	Place->DispatchBeginPlay();
	auto* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	Post->SetDefinitionBinding(Definition, Row.GuardPostID);
	Post->DispatchBeginPlay();
	TestTrue(TEXT("The fixture uses the actual post binding"), Post->GetOwningTerritory() == Place);
	TestEqual(TEXT("Fresh post has three reserves"), Post->GetReserveCount(), 3);

	// First record is deliberately stale. Stream-out must replace it with live state.
	TestTrue(TEXT("Native records the original owner"), Save->SaveSingleActor(Place));
	Place->ForceSetOwningFaction(Bandits);
	Place->LockTerritory(FText::FromString(TEXT("Streaming regression")));
	auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	Post->RegisterSpawnedGuard(Guard);
	TestEqual(TEXT("A live guard occupies the post before removal"), Post->GetActiveGuardCount(), 1);
	FIntProperty* Reserve = FindFProperty<FIntProperty>(Post->GetClass(), TEXT("CurrentReserveCount"));
	Reserve->SetPropertyValue_InContainer(Post, 0);

	FWorldDelegates::PreLevelRemovedFromWorld.Broadcast(OtherWorld->PersistentLevel, OtherWorld);
	TestFalse(TEXT("Another world's removal cannot write this post"), Save->DoesRecordExist(Row.StableGuardPostGUID));
	State->SetRole(ROLE_SimulatedProxy);
	FWorldDelegates::PreLevelRemovedFromWorld.Broadcast(World->PersistentLevel, World);
	TestFalse(TEXT("A client persistence actor cannot write the server's post"), Save->DoesRecordExist(Row.StableGuardPostGUID));
	State->SetRole(ROLE_Authority);
	{
		World->bIsTearingDown = true;
		FWorldDelegates::PreLevelRemovedFromWorld.Broadcast(World->PersistentLevel, World);
		TestFalse(TEXT("Quitting does not overwrite campaign records with teardown state"), Save->DoesRecordExist(Row.StableGuardPostGUID));
		World->bIsTearingDown = false;
	}
	State->SaveStreamingLevel(nullptr, World);
	State->SaveStreamingLevel(OtherWorld->PersistentLevel, World);
	FWorldDelegates::PreLevelRemovedFromWorld.Broadcast(World->PersistentLevel, World);
	if (!TestTrue(TEXT("Real pre-removal delegate creates the post record"), Save->DoesRecordExist(Row.StableGuardPostGUID))) return false;
	const FNarrativeActorRecord PostRecord = Save->GetSaveObject()->RecordMap.FindChecked(Row.StableGuardPostGUID);
	const FNarrativeActorRecord PlaceRecord = Save->GetSaveObject()->RecordMap.FindChecked(Definition->StableTerritoryGUID);

	// Cleanup happens after the engine's pre-removal notification. It must not erase
	// the recorded living slot or provision fresh reserves from the asset defaults.
	Post->UnregisterGuard(Guard, EGuardRemovalReason::ManualRemoval);
	Guard->Destroy();
	TestEqual(TEXT("Cleanup releases the physical guard slot"), Post->GetActiveGuardCount(), 0);
	Reserve->SetPropertyValue_InContainer(Post, 3);
	Place->TryUnlock(true);
	Place->ForceSetOwningFaction(Heroes);
	for (bool bPostFirst : {true, false})
	{
		if (bPostFirst) Save->LoadActorFromRecord(Post, PostRecord);
		Save->LoadActorFromRecord(Place, PlaceRecord);
		if (!bPostFirst) Save->LoadActorFromRecord(Post, PostRecord);
		TestEqual(TEXT("Native restore retains the changed owner in either load order"), Place->GetOwningFaction(), Bandits);
		TestTrue(TEXT("Native restore retains the story lock"), Place->IsLocked());
		TestEqual(TEXT("Exhausted reserves remain exhausted"), Post->GetReserveCount(), 0);
		TestEqual(TEXT("The saved slot reflects life before cleanup"), Post->GetSavedActiveGuardCount(), 1);
	}
	TestFalse(TEXT("Streaming persistence is not a second Blueprint save API"),
		State->GetClass()->FindFunctionByName(TEXT("SaveStreamingLevel")) != nullptr);
	Save->GetSaveObject()->RecordMap.Remove(Row.StableGuardPostGUID);
	State->UnsubscribeFromLiveUpdates();
	FWorldDelegates::PreLevelRemovedFromWorld.Broadcast(World->PersistentLevel, World);
	TestFalse(TEXT("Removing the persistence actor detaches its delegate"), Save->DoesRecordExist(Row.StableGuardPostGUID));
	return true;
}

#endif
