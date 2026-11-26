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
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardRestoreComposition,
	"TerritoryFramework.Guards.Regression.ComposedOwnerPostRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardRestoreComposition::RunTest(const FString& Parameters)
{
	for (const bool bAncestorLast : {false, true})
	for (const bool bContested : {false, true})
	for (const bool bOwnerFirst : {false, true})
	for (const bool bRecreateActors : {false, true})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
		TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
		auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
		Save->UpdateSaveObject(true);
		auto* NPC = NewObject<UNPCDefinition>();
		NPC->CharacterID = TEXT("ComposedRestoreGuard");
		NPC->NPCID = TEXT("ComposedRestoreGuard");
		NPC->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
		NPC->bAllowMultipleInstances = true;
		auto* Definition = NewObject<UTerritoryPlaceDefinition>();
		Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
		Definition->StableTerritoryGUID = FGuid(20261005, 1, 1, 1);
		Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		Definition->InitialOwningFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		Definition->DefaultGuardDefinition = NPC;
		Definition->InitialGuardCount = 2;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			FTerritoryGuardPostTemplate& Row = Definition->GuardPosts.AddDefaulted_GetRef();
			Row.GuardPostID = FName(*FString::Printf(TEXT("Post%d"), Index));
			Row.StableGuardPostGUID = FGuid(20261005, 2, 1, Index + 1);
			Row.ReserveSlots = 1;
			Row.ReserveSpawnDelay = 0.4f;
			Row.ReserveCameraAvoidanceRetryLimit = 0;
			Row.bAutoSpawnReserves = true;
		}
		auto* DistrictDefinition = NewObject<UTerritoryDistrictDefinition>();
		DistrictDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
		DistrictDefinition->StableTerritoryGUID = FGuid(20261005, 3, 1, 1);
		DistrictDefinition->TerritoryActorClass = ATerritoryDistrict::StaticClass();
		DistrictDefinition->Places.Add(Definition);
		auto* CityDefinition = NewObject<UTerritoryCityDefinition>();
		CityDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach"));
		CityDefinition->StableTerritoryGUID = FGuid(20261005, 4, 1, 1);
		CityDefinition->TerritoryActorClass = ATerritoryCity::StaticClass();
		CityDefinition->Districts.Add(DistrictDefinition);
		Definition->DerivedParentTerritoryTag = DistrictDefinition->TerritoryTag;
		DistrictDefinition->DerivedParentTerritoryTag = CityDefinition->TerritoryTag;
		auto BeginAncestors = [&]()
		{
			auto* District = World->SpawnActor<ATerritoryDistrict>();
			DistrictDefinition->ApplyToTerritory(District);
			District->DispatchBeginPlay();
			auto* City = World->SpawnActor<ATerritoryCity>();
			CityDefinition->ApplyToTerritory(City);
			City->DispatchBeginPlay();
		};
		if (!bAncestorLast) BeginAncestors();
		auto* Place = World->SpawnActor<ATerritoryProperty>();
		Definition->ApplyToTerritory(Place);
		Place->DispatchBeginPlay();
		TArray<ATerritoryGuardSpawnPoint*> Posts;
		for (int32 Index = 0; Index < 2; ++Index)
		{
			auto* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
			Post->SetActorLocation(FVector(5000.f + Index * 1000.f, 0.f, 100.f));
			Post->SetDefinitionBinding(Definition, Definition->GuardPosts[Index].GuardPostID);
			Post->DispatchBeginPlay();
			Posts.Add(Post);
		}
		if (bAncestorLast)
		{
			TestEqual(TEXT("Unknown ancestors defer physical deployment"), Place->GetSpawnedGuardCount(), 0);
			BeginAncestors();
		}
		if (!TestEqual(TEXT("Fixture has two real Native guards"), Place->GetSpawnedGuardCount(), 2)) continue;
		auto* Victim = Posts[0]->ActiveGuards[0].Get();
		auto* ASC = Victim->GetNarrativeAbilitySystemComponent();
		FindFProperty<FBoolProperty>(ASC->GetClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(ASC, true);
		ASC->OnDeathStateChanged.Broadcast(Victim, ASC, true);
		TestEqual(TEXT("Real casualty queued one replacement"), Posts[0]->GetPendingReserveCount(), 1);
		Place->ReconcileAvailabilityDependentSystems(false);
		TestEqual(TEXT("Readiness reconciliation never refills a fresh campaign casualty"), Posts[0]->GetActiveGuardCount(), 0);
		TestEqual(TEXT("Readiness reconciliation keeps the finite replacement pending"), Posts[0]->GetPendingReserveCount(), 1);
		// Seed a valid contested save independently of capture registration; the
		// regression is the Native owner/post load composition, not attack admission.
		if (bContested)
		{
			Place->OwnershipData.State = ETerritoryState::Contested;
			Place->OwnershipData.ContestingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
			Place->OwnershipData.ControlProgress = 0.25f;
		}
		TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
		World->GetTimerManager().Tick(0.1f);
		++GFrameCounter;
		World->GetTimerManager().Tick(0.2f);
		++GFrameCounter;
		for (auto* Post : Posts) Save->SaveSingleActor(Post);
		Save->SaveSingleActor(Place);
		const FNarrativeActorRecord OwnerRecord = Save->GetSaveObject()->RecordMap.FindChecked(Definition->StableTerritoryGUID);
		TArray<FNarrativeActorRecord> PostRecords;
		for (auto* Post : Posts) PostRecords.Add(Save->GetSaveObject()->RecordMap.FindChecked(Post->GetActorGUID_Implementation()));
		TestTrue(TEXT("Save records elapsed reserve delay"),
			Posts[0]->SavedReserveRemainingDelay > 0.19f && Posts[0]->SavedReserveRemainingDelay < 0.21f);
		if (bRecreateActors)
		{
			// Exercise actor recreation, not a claim of physical World Partition coverage.
			for (auto* Post : Posts) Post->EndPlay(EEndPlayReason::RemovedFromWorld);
			Place->EndPlay(EEndPlayReason::RemovedFromWorld);
			Save->GetSaveObject()->RecordMap.Add(OwnerRecord.ActorGUID, OwnerRecord);
			for (const auto& Record : PostRecords) Save->GetSaveObject()->RecordMap.Add(Record.ActorGUID, Record);
			Place = World->SpawnActor<ATerritoryProperty>();
			Definition->ApplyToTerritory(Place);
			Posts.Reset();
			if (bOwnerFirst) Place->DispatchBeginPlay();
			for (int32 Index = 0; Index < 2; ++Index)
			{
				auto* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
				Post->SetActorLocation(FVector(5000.f + Index * 1000.f, 0.f, 100.f));
				Post->SetDefinitionBinding(Definition, Definition->GuardPosts[Index].GuardPostID);
				Post->DispatchBeginPlay();
				Posts.Add(Post);
			}
			if (!bOwnerFirst) Place->DispatchBeginPlay();
		}
		else
		{
			if (bOwnerFirst) Save->LoadActorFromRecord(Place, OwnerRecord);
			for (int32 Index = 0; Index < Posts.Num(); ++Index) Save->LoadActorFromRecord(Posts[Index], PostRecords[Index]);
			if (!bOwnerFirst) Save->LoadActorFromRecord(Place, OwnerRecord);
		}
		TestEqual(TEXT("Saved dead post is never exchanged with the surviving post"), Posts[0]->GetActiveGuardCount(), 0);
		TestEqual(TEXT("Saved living post retains one physical guard in Claimed/Contested"), Posts[1]->GetActiveGuardCount(), 1);
		TestEqual(TEXT("Restore retains the finite pending request"), Posts[0]->GetPendingReserveCount(), 1);
		TestEqual(TEXT("Restore does not spend the reserve"), Posts[0]->GetReserveCount(), 1);
		// TimerManager promotes newly scheduled pending handles at the end of its
		// first tick. Begin measuring elapsed time only after that activation frame.
		World->GetTimerManager().Tick(0.001f);
		++GFrameCounter;
		TestTrue(TEXT("Loaded timer uses saved remaining delay, not the full configured delay"),
			FMath::IsNearlyEqual(World->GetTimerManager().GetTimerRemaining(Posts[0]->ReserveSpawnTimer), 0.2f, 0.01f));
		World->GetTimerManager().Tick(0.1f);
		++GFrameCounter;
		TestEqual(TEXT("Remaining delay is not skipped"), Posts[0]->GetActiveGuardCount(), 0);
		World->GetTimerManager().Tick(0.11f);
		++GFrameCounter;
		TestEqual(TEXT("Natural timer progress deploys one replacement after restore"), Posts[0]->GetActiveGuardCount(), 1);
		TestEqual(TEXT("Successful replacement consumes exactly one reserve"), Posts[0]->GetReserveCount(), 0);
		TestEqual(TEXT("Successful replacement clears the pending request"), Posts[0]->GetPendingReserveCount(), 0);
		World->GetTimerManager().Tick(1.f);
		++GFrameCounter;
		TestEqual(TEXT("No duplicate timer can create a second occupant"), Place->GetSpawnedGuardCount(), 2);
		for (auto* Post : Posts)
		{
			Post->CurrentReserveCount = 0;
			if (Post->ActiveGuards.IsEmpty()) continue;
			auto* Guard = Post->ActiveGuards[0].Get();
			auto* GuardASC = Guard->GetNarrativeAbilitySystemComponent();
			FindFProperty<FBoolProperty>(GuardASC->GetClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(GuardASC, true);
			GuardASC->OnDeathStateChanged.Broadcast(Guard, GuardASC, true);
			Save->SaveSingleActor(Post);
		}
		// Deliberately stale aggregate count must not override explicit post zeros.
		Place->OwnershipData.DefenderCount = 1;
		Save->SaveSingleActor(Place);
		Save->LoadActorFromRecord(Place, Save->GetSaveObject()->RecordMap.FindChecked(Definition->StableTerritoryGUID));
		TestEqual(TEXT("Explicit zero occupancy defeats legacy aggregate fallback"), Place->GetSpawnedGuardCount(), 0);
		for (auto* Post : Posts)
		{
			TestEqual(TEXT("Exhausted reserve is not reinitialized"), Post->GetReserveCount(), 0);
			TestEqual(TEXT("Exhausted reserve does not queue a replacement"), Post->GetPendingReserveCount(), 0);
		}
	}
	return true;
}

#endif
