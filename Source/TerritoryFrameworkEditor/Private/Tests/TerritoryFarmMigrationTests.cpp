#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NPCDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Spawners/NPCSpawnComponent.h"
#include "TimerManager.h"
#include "Tales/TerritoryOwnerHandoverEvent.h"
#include "Tales/TerritoryOwnershipCondition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFarmSeedAndCampaignMigration,
    "TerritoryFramework.Story.FarmSeedAndCampaignMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFarmSeedAndCampaignMigration::RunTest(const FString& Parameters)
{
    auto* Authored = LoadObject<UTerritoryPlaceDefinition>(nullptr,
        TEXT("/TerritoryFramework/Definitions/DA_Place_Farm.DA_Place_Farm"));
    if (!TestNotNull(TEXT("Included Farm example loads"), Authored)) return false;
    TestEqual(TEXT("Included Farm starts Locked"), Authored->InitialAvailability, ETerritoryAvailability::Locked);
    const auto* Locked = Authored->StateConfigs.Find(ETerritoryState::Locked);
    if (!TestNotNull(TEXT("Farm has its authored unlock lifecycle"), Locked)) return false;
    int32 OwnerEvents = 0;
    for (UNarrativeEvent* Event : Locked->ExitEvents)
        if (const auto* Reveal = Cast<UTerritoryOwnerHandoverEvent>(Event))
        {
            ++OwnerEvents;
            TestFalse(TEXT("Unlock reveals the owner without auto-starting dialogue"), Reveal->bBeginDialogueImmediately);
        }
    TestEqual(TEXT("Unlock contains exactly one owner reveal"), OwnerEvents, 1);
    const UTerritoryOwnershipCondition* Prerequisite = nullptr;
    for (UNarrativeCondition* Condition : Locked->ExitConditions)
        if (auto* Ownership = Cast<UTerritoryOwnershipCondition>(Condition)) Prerequisite = Ownership;
    if (!TestNotNull(TEXT("Farm retains its ownership prerequisite"), Prerequisite)) return false;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    // Native spawner components auto-activate during Engine component
    // initialization, which DispatchBeginPlay alone deliberately does not do.
    World->InitializeActorsForPlay(FURL());
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
    auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
    Save->UpdateSaveObject(true);
    // Keep the real example's conditions/events/identity. A minimal Native NPC
    // definition avoids making this lifecycle regression depend on its appearance.
    auto* Definition = DuplicateObject(Authored, GetTransientPackage());
    Definition->InitialGuardCount = 0;
    auto* OwnerNPC = NewObject<UNPCDefinition>();
    OwnerNPC->NPCID = OwnerNPC->CharacterID = TEXT("FarmMigrationOwner");
    OwnerNPC->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
    OwnerNPC->bAllowMultipleInstances = false;
    Definition->StoryOwner.NPCDefinition = OwnerNPC;
    auto* Farm = World->SpawnActor<ATerritoryProperty>();
    Definition->ApplyToTerritory(Farm);
    Farm->DispatchBeginPlay();
    auto* Spawner = World->SpawnActor<ATerritoryStoryOwnerSpawner>();
    Spawner->SetPlaceDefinition(Definition);
    Spawner->ApplyPlaceDefinition();
    Spawner->DispatchBeginPlay();
    TestTrue(TEXT("New campaign applies the Locked seed"), Farm->IsLocked());
    TestNull(TEXT("No owner appears before the prerequisite"), Spawner->GetStoryOwner());
    TestFalse(TEXT("Missing Blacksmith rejects unlock"), Farm->TryUnlock(false));

    auto* BlacksmithDefinition = NewObject<UTerritoryPlaceDefinition>();
    BlacksmithDefinition->TerritoryTag = Prerequisite->TerritoryToCheck;
    BlacksmithDefinition->StableTerritoryGUID = FGuid(20261005, 6, 1, 1);
    BlacksmithDefinition->InitialOwningFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
    auto* Blacksmith = World->SpawnActor<ATerritoryProperty>();
    BlacksmithDefinition->ApplyToTerritory(Blacksmith);
    Blacksmith->DispatchBeginPlay();
    TestTrue(TEXT("Claimed Blacksmith permits normal world-context unlock"), Farm->TryUnlock(false));
    const auto* EffectiveLocked = Farm->GetStateGameplayRules(ETerritoryState::Locked, Farm->GetOwningFaction());
    TestTrue(TEXT("The current owner's Locked rule includes the reveal event"), EffectiveLocked && !EffectiveLocked->ExitEvents.IsEmpty());
    TestTrue(TEXT("Native owner spawn component is active"), Spawner->OwnerSpawn->IsActive());
    // The handover adapter intentionally retries Native spawn readiness on its
    // timer. Advance the actual timer, never set the activation result by hand.
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
    for (int32 Tick = 0; Tick < 10 && !Spawner->GetStoryOwner(); ++Tick)
    {
        ++GFrameCounter;
        World->GetTimerManager().Tick(0.11f);
    }
    auto* FirstOwner = Spawner->GetStoryOwner();
    TestNotNull(TEXT("Unlock event creates one Native story owner"), FirstOwner);
    TestTrue(TEXT("Repeated unlock is idempotent"), Farm->TryUnlock(false));
    TestTrue(TEXT("Repeated unlock retains the same owner"), FirstOwner == Spawner->GetStoryOwner());

    Save->SaveSingleActor(Farm);
    const auto Record = Save->GetSaveObject()->RecordMap.FindChecked(Farm->GetActorGUID_Implementation());
    const auto OwnerBefore = Farm->GetOwningFaction();
    Definition->ApplyToTerritory(Farm);
    Save->LoadActorFromRecord(Farm, Record);
    TestFalse(TEXT("Locked new-game authoring cannot relock an existing unlocked campaign"), Farm->IsLocked());
    TestEqual(TEXT("Existing political ownership survives authoring/load"), Farm->GetOwningFaction(), OwnerBefore);
    TestEqual(TEXT("Existing durable Place identity survives"), Farm->GetActorGUID_Implementation(), Record.ActorGUID);
    TestTrue(TEXT("Farm reload does not replay owner reveal"), FirstOwner == Spawner->GetStoryOwner());
    return true;
}
#endif
