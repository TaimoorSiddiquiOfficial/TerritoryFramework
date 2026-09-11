#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NPCDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryStateTask.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIndependentGuardPostStreaming,
	"TerritoryFramework.Guards.Regression.IndependentPostCellsKeepStaffingAndCasualties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIndependentGuardPostStreaming::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Save->UpdateSaveObject(true);
	auto* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("IndependentPostCharacter");
	NPC->NPCID = TEXT("IndependentPostNPC");
	NPC->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(511, 1, 2, 3);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->InitialOwningFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Definition->DefaultGuardDefinition = NPC;
	Definition->InitialGuardCount = 2;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FTerritoryGuardPostTemplate Row;
		Row.GuardPostID = FName(*FString::Printf(TEXT("Post%d"), Index));
		Row.StableGuardPostGUID = FGuid(511, 4, 5, Index + 1);
		Row.ReserveSlots = 3;
		Row.bAutoSpawnReserves = false;
		Definition->GuardPosts.Add(Row);
	}
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(Place);
	Place->DispatchBeginPlay();
	TestEqual(TEXT("Place-first startup retains its authored staffing target"), Place->GetDesiredGuardCount(), 2);
	TestEqual(TEXT("Unloaded authored posts still define capacity"), Place->GetMaxGuardCount(), 2);
	TestEqual(TEXT("Missing post actors cannot create physical guards"), Place->GetSpawnedGuardCount(), 0);
	TestTrue(TEXT("Native records staffing before the post cells arrive"), Save->SaveSingleActor(Place));
	TArray<ATerritoryGuardSpawnPoint*> Posts;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
		Post->SetActorLocation(FVector(5000.f + Index * 1000.f, 0.f, 100.f));
		Post->SetDefinitionBinding(Definition, Definition->GuardPosts[Index].GuardPostID);
		Post->DispatchBeginPlay();
		Posts.Add(Post);
	}
	if (!TestEqual(TEXT("Each arriving post deploys one Native guard"), Place->GetSpawnedGuardCount(), 2)) return false;
	TestEqual(TEXT("Loaded actors do not double-count Definition slots"), Place->GetMaxGuardCount(), 2);
	ATerritoryGuardCharacter* Victim = Posts[0]->ActiveGuards[0].Get();
	UNarrativeAbilitySystemComponent* ASC = Victim->GetNarrativeAbilitySystemComponent();
	Place->SetRole(ROLE_SimulatedProxy);
	Place->OnDefenderDied(Victim, ASC, true);
	TestEqual(TEXT("Client death callback cannot mutate staffing"), Posts[0]->GetActiveGuardCount(), 1);
	Place->SetRole(ROLE_Authority);
	// Exercise the Native ASC death delegate against the actual tag-bound post,
	// deliberately absent from the legacy GuardSpawnPoints array.
	FindFProperty<FBoolProperty>(ASC->GetClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(ASC, true);
	ASC->OnDeathStateChanged.Broadcast(Victim, ASC, true);
	ASC->OnDeathStateChanged.Broadcast(Victim, ASC, true);
	TestEqual(TEXT("Death releases the resolved post immediately"), Posts[0]->GetActiveGuardCount(), 0);
	TestEqual(TEXT("Duplicate Native death queues only one finite reserve"), Posts[0]->GetPendingReserveCount(), 1);
	TestEqual(TEXT("A queued request does not spend a reserve"), Posts[0]->GetReserveCount(), 3);
	TestEqual(TEXT("Other post keeps its occupant"), Posts[1]->GetActiveGuardCount(), 1);
	TestEqual(TEXT("Other post has no casualty request"), Posts[1]->GetPendingReserveCount(), 0);
	TestTrue(TEXT("Native records the casualty before its post streams out"), Save->SaveSingleActor(Posts[0]));
	const FNarrativeActorRecord DeadPostRecord = Save->GetSaveObject()->RecordMap.FindChecked(Definition->GuardPosts[0].StableGuardPostGUID);
	Posts[0]->SetResolvedTerritory(nullptr);
	TestEqual(TEXT("Unregistering the post cannot reduce durable capacity"), Place->GetMaxGuardCount(), 2);
	Save->LoadActorFromRecord(Posts[0], DeadPostRecord);
	Posts[0]->SetResolvedTerritory(Place);
	TestEqual(TEXT("Reloading a dead post does not hire a free replacement"), Place->GetSpawnedGuardCount(), 1);
	TestEqual(TEXT("Dead saved slot stays empty"), Posts[0]->GetActiveGuardCount(), 0);
	TestEqual(TEXT("Pending finite request survives Native load"), Posts[0]->GetPendingReserveCount(), 1);
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	Registry->UnregisterTerritory(Place);
	Posts[0]->bAutoSpawnReserves = true;
	Posts[0]->TryAutomaticReserveSpawn();
	TestEqual(TEXT("Automatic reserve timer waits for an absent owner without cancelling its request"), Posts[0]->GetPendingReserveCount(), 1);
	TestEqual(TEXT("An absent owner cannot spend a reserve"), Posts[0]->GetReserveCount(), 3);
	Posts[0]->bAutoSpawnReserves = false;
	Registry->RegisterTerritory(Place);
	TestTrue(TEXT("Explicit reserve deployment still works"), Posts[0]->SpawnReserveGuard());
	TestEqual(TEXT("Real replacement spends one reserve"), Posts[0]->GetReserveCount(), 2);
	TestEqual(TEXT("Real replacement fills the garrison"), Place->GetSpawnedGuardCount(), 2);
	// The owner leaves while both post actors remain. A later Native save must
	// retain the pre-cleanup occupants, and a different returning actor must bind.
	TestTrue(TEXT("Native records the owner before stream-out"), Save->SaveSingleActor(Place));
	for (ATerritoryGuardSpawnPoint* Post : Posts) Save->SaveSingleActor(Post);
	Place->EndPlay(EEndPlayReason::RemovedFromWorld);
	for (ATerritoryGuardSpawnPoint* Post : Posts)
	{
		TestNull(TEXT("An unregistered old owner is not a live deployment target"), Post->GetOwningTerritory());
		TestEqual(TEXT("Owner stream-out removes physical occupants"), Post->GetActiveGuardCount(), 0);
		TestTrue(TEXT("Native can save a waiting post"), Save->SaveSingleActor(Post));
		TestEqual(TEXT("Saving while owner is absent keeps the living slot"), Post->GetSavedActiveGuardCount(), 1);
		TestFalse(TEXT("A waiting post cannot spend a reserve"), Post->SpawnReserveGuard());
	}
	auto* ReturnedPlace = World->SpawnActor<ATerritoryProperty>();
	Definition->ApplyToTerritory(ReturnedPlace);
	ReturnedPlace->DispatchBeginPlay();
	TestEqual(TEXT("The replacement owner restores the staffing target"), ReturnedPlace->GetDesiredGuardCount(), 2);
	TestEqual(TEXT("The replacement owner restores exactly two Native guards"), ReturnedPlace->GetSpawnedGuardCount(), 2);
	for (ATerritoryGuardSpawnPoint* Post : Posts)
	{
		TestTrue(TEXT("A still-loaded post binds the newly registered owner"), Post->GetOwningTerritory() == ReturnedPlace);
		TestEqual(TEXT("Each saved living post restores one occupant"), Post->GetActiveGuardCount(), 1);
	}
	TestEqual(TEXT("Streaming does not refill the spent reserve"), Posts[0]->GetReserveCount(), 2);
	TestEqual(TEXT("Streaming leaves the untouched reserve alone"), Posts[1]->GetReserveCount(), 3);
	TestFalse(TEXT("Native load marker adds no Blueprint API"),
		Posts[0]->GetClass()->FindFunctionByName(TEXT("SetLoadedFromSave")) != nullptr);

	// A task reached during the replacement delay must not skip the remaining
	// fight. Exercise real Native death and save records on the streamed owner.
	auto* DefeatTask = NewObject<UTerritoryStateTask>();
	DefeatTask->TargetTerritory = Definition->TerritoryTag;
	DefeatTask->Objective = ETerritoryStateTaskObjective::AllDefendersDefeated;
	TestFalse(TEXT("Living restored defenders block the defeat task"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	for (ATerritoryGuardSpawnPoint* Post : Posts)
	{
		ATerritoryGuardCharacter* Guard = Post->ActiveGuards[0].Get();
		UNarrativeAbilitySystemComponent* GuardASC = Guard->GetNarrativeAbilitySystemComponent();
		FindFProperty<FBoolProperty>(GuardASC->GetClass(), TEXT("bIsDead"))
			->SetPropertyValue_InContainer(GuardASC, true);
		GuardASC->OnDeathStateChanged.Broadcast(Guard, GuardASC, true);
	}
	TestEqual(TEXT("Both restored defenders are removed by Native death"),
		ReturnedPlace->GetDefenderCount(), 0);
	TestEqual(TEXT("Both finite replacements remain pending"),
		ReturnedPlace->GetGarrisonSnapshot().PendingDeployments, 2);
	TestFalse(TEXT("Initial task evaluation waits during the reserve spawn delay"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	TArray<FNarrativeActorRecord> PendingRecords;
	for (ATerritoryGuardSpawnPoint* Post : Posts)
	{
		FNarrativeActorRecord& Record = PendingRecords.AddDefaulted_GetRef();
		TestTrue(TEXT("Native save records the pending defence"), Save->CreateActorRecord(Post, Record));
		Post->CancelPendingReserveSpawns();
	}
	ReturnedPlace->RefreshGarrisonSnapshot();
	TestTrue(TEXT("Unused reserves alone do not block a completed fight"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	for (int32 Index = 0; Index < Posts.Num(); ++Index)
	{
		Save->LoadActorFromRecord(Posts[Index], PendingRecords[Index]);
	}
	ReturnedPlace->RefreshGarrisonSnapshot();
	TestEqual(TEXT("Native restore returns both queued replacements"),
		ReturnedPlace->GetGarrisonSnapshot().PendingDeployments, 2);
	TestFalse(TEXT("A task reached after load still waits for reserves"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	ReturnedPlace->SetRole(ROLE_SimulatedProxy);
	for (ATerritoryGuardSpawnPoint* Post : Posts) Post->CancelPendingReserveSpawns();
	ReturnedPlace->RefreshGarrisonSnapshot();
	TestFalse(TEXT("Client preview uses the server snapshot rather than local post state"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	ReturnedPlace->SetRole(ROLE_Authority);
	ReturnedPlace->RefreshGarrisonSnapshot();
	TestTrue(TEXT("Server cancellation allows the read-only defeat objective"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	TestFalse(TEXT("Missing Territory never satisfies the task"),
		DefeatTask->IsObjectiveSatisfiedBy(nullptr));
	DefeatTask->TargetTerritory = FGameplayTag();
	TestFalse(TEXT("A different target cannot satisfy the task"),
		DefeatTask->IsObjectiveSatisfiedBy(ReturnedPlace));
	return true;
}

#endif
