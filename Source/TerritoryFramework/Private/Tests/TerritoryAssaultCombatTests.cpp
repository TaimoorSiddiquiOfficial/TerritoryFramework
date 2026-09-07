#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/NarrativeNPCController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Combat/BTService_TerritoryAssaultPermission.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/World.h"
#include "Items/InventoryComponent.h"
#include "Items/MeleeWeaponItem.h"
#include "Items/RangedWeaponItem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultPermissionTargetIdentity,
    "TerritoryFramework.CounterAttack.Regression.CombatPermissionFollowsSavedTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultPermissionTargetIdentity::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("World created"), World)) return false;
    World->CreateAISystem();
    auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
    auto* Director = World->GetSubsystem<UTerritoryCombatDirector>();
    auto* Target = World->SpawnActor<ATerritoryProperty>();
    auto* Foreign = World->SpawnActor<ATerritoryProperty>();
    auto* Pawn = World->SpawnActor<ATerritoryAssaultCharacter>();
    auto* Controller = World->SpawnActor<ANarrativeNPCController>();
    if (!Registry || !Director || !Target || !Foreign || !Pawn || !Controller)
    {
        AddError(TEXT("Combat permission fixture could not be created"));
        World->DestroyWorld(false);
        return false;
    }
    const auto Tag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.CastleHill.Farm"));
    const auto Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
    const auto TargetID = FGuid::NewGuid();
    auto* TagProperty = FindFProperty<FStructProperty>(ATerritoryVolume::StaticClass(), TEXT("TerritoryTag"));
    auto SetIdentity = [TagProperty, Tag](ATerritoryVolume* Volume, const FGuid& ID)
    {
        Volume->SetActorGUID_Implementation(ID);
        *TagProperty->ContainerPtrToValuePtr<FGameplayTag>(Volume) = Tag;
    };
    SetIdentity(Target, TargetID);
    SetIdentity(Foreign, FGuid::NewGuid());
    Registry->RegisterTerritory(Target);
    Controller->SetPawn(Pawn);
    auto* Participant = Pawn->FindComponentByClass<UTerritoryAssaultParticipantComponent>();
    Participant->Configure(FGuid::NewGuid(), TargetID, Tag, Faction);

    auto* Blackboard = NewObject<UBlackboardComponent>(Controller);
    Blackboard->RegisterComponent();
    auto* Data = NewObject<UBlackboardData>();
    FBlackboardEntry& TargetKey = Data->Keys.AddDefaulted_GetRef();
    TargetKey.EntryName = TEXT("AttackTarget");
    TargetKey.KeyType = NewObject<UBlackboardKeyType_Object>(Data);
    FBlackboardEntry& PermissionKey = Data->Keys.AddDefaulted_GetRef();
    PermissionKey.EntryName = TEXT("TerritoryAssaultGranted");
    PermissionKey.KeyType = NewObject<UBlackboardKeyType_Bool>(Data);
    TestTrue(TEXT("Native blackboard initializes with an AI system"), Blackboard->InitializeBlackboard(*Data));
    auto* Tree = NewObject<UBehaviorTreeComponent>(Controller);
    Tree->RegisterComponent();
    Tree->CacheBlackboardComponent(Blackboard);
    auto* Asset = NewObject<UBehaviorTree>();
    Asset->BlackboardAsset = Data;
    auto* Service = NewObject<UBTService_TerritoryAssaultPermission>(Asset);
    Service->TerritoryKey.SelectedKeyName = TEXT("AttackTarget");
    Service->PermissionGrantedKey.SelectedKeyName = TEXT("TerritoryAssaultGranted");
    Service->InitializeFromAsset(*Asset);
    Blackboard->SetValueAsObject(TEXT("AttackTarget"), Foreign);
    Pawn->SetActorLocation(FVector(50000.f, 50000.f, 100.f));

    TestEqual(TEXT("A foreign combat target and distant pawn cannot redirect the assault slot"),
        Service->ResolveTerritory(*Tree), static_cast<ATerritoryVolume*>(Target));
    Service->UpdatePermission(*Tree);
    TestTrue(TEXT("Existing director grants the saved target slot"), Director->HasAssaultSlot(Target, Controller));
    TestTrue(TEXT("Native behavior tree receives permission"), Blackboard->GetValueAsBool(TEXT("TerritoryAssaultGranted")));
    TestFalse(TEXT("Foreign territory consumes no capacity"), Director->HasAssaultSlot(Foreign, Controller));
    Service->UpdatePermission(*Tree);
    TestEqual(TEXT("Repeated service ticks do not duplicate a slot"), Director->GetGrantedSlots(Target), 1);

    Registry->UnregisterTerritory(Target);
    Registry->RegisterTerritory(Foreign);
    TestNull(TEXT("Streamed-out target cannot fall back to a reused tag or blackboard actor"), Service->ResolveTerritory(*Tree));
    Service->UpdatePermission(*Tree);
    TestFalse(TEXT("Stream-out releases prior capacity"), Director->HasAssaultSlot(Target, Controller));
    TestFalse(TEXT("Stream-out clears the combat gate"), Blackboard->GetValueAsBool(TEXT("TerritoryAssaultGranted")));
    Registry->UnregisterTerritory(Foreign);
    Registry->RegisterTerritory(Target);
    Service->UpdatePermission(*Tree);
    TestTrue(TEXT("Reload of the same durable target restores permission"), Director->HasAssaultSlot(Target, Controller));
    Participant->Configure(FGuid(), TargetID, Tag, Faction);
    Service->UpdatePermission(*Tree);
    TestFalse(TEXT("Unconfigured participant cannot bypass strategic limits as an ordinary NPC"), Blackboard->GetValueAsBool(TEXT("TerritoryAssaultGranted")));
    TestEqual(TEXT("Invalidated assault releases its slot"), Director->GetGrantedSlots(Target), 0);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultWeaponActivityEligibility,
    "TerritoryFramework.CounterAttack.Regression.NarrativeActivityRequiresAvailableWeapon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultWeaponActivityEligibility::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Inventory fixture world created"), World)) return false;
    auto* Pawn = World->SpawnActor<ATerritoryAssaultCharacter>();
    auto* Controller = World->SpawnActor<ANarrativeNPCController>();
    UClass* RangedClass = LoadClass<UNPCActivity>(nullptr, TEXT("/Game/TerritoryFramework/AI/Combat/BPA_TerritoryAttack_Ranged_Strafe.BPA_TerritoryAttack_Ranged_Strafe_C"));
    UClass* MeleeClass = LoadClass<UNPCActivity>(nullptr, TEXT("/Game/TerritoryFramework/AI/Combat/BPA_TerritoryAttack_Melee.BPA_TerritoryAttack_Melee_C"));
    UClass* GoalClass = LoadClass<UNPCGoalItem>(nullptr, TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
    if (!Pawn || !Controller || !RangedClass || !MeleeClass || !GoalClass)
    {
        AddError(TEXT("Actual Narrative/project combat classes could not be loaded"));
        World->DestroyWorld(false);
        return false;
    }
    Controller->SetPawn(Pawn);
    auto* Inventory = Pawn->GetInventoryComponent();
    auto MakeActivity = [Controller](UClass* Class)
    {
        auto* Activity = NewObject<UNPCActivity>(Controller, Class);
        Activity->SetOwner(Controller, Controller->GetActivityComponent());
        return Activity;
    };
    auto* Ranged = MakeActivity(RangedClass);
    auto* Melee = MakeActivity(MeleeClass);
    auto* Goal = NewObject<UNPCGoalItem>(Controller, GoalClass);
    Goal->OwnerController = Controller;
    auto* Defender = World->SpawnActor<ATerritoryAssaultCharacter>();
    FindFProperty<FObjectProperty>(GoalClass, TEXT("TargetToAttack"))->SetObjectPropertyValue_InContainer(Goal, Defender);
    Goal->DefaultScore = 4.f;
    auto* Alert = FindFProperty<FBoolProperty>(GoalClass, TEXT("IsAlert"));
    auto* AlertTime = FindFProperty<FDoubleProperty>(GoalClass, TEXT("AlertChangedTime"));
    if (!Alert || !AlertTime || !Inventory)
    {
        AddError(TEXT("Narrative attack goal/inventory contract changed"));
        World->DestroyWorld(false);
        return false;
    }
    Alert->SetPropertyValue_InContainer(Goal, true);
    AlertTime->SetPropertyValue_InContainer(Goal, -10.0);
    auto Score = [this, Goal](UNPCActivity* Activity)
    {
        UFunction* Function = Activity->FindFunction(TEXT("ScoreGoalItem"));
        FStructOnScope Params(Function);
        auto* Input = FindFProperty<FObjectProperty>(Function, TEXT("Goal"));
        auto* Output = FindFProperty<FFloatProperty>(Function, TEXT("ReturnValue"));
        if (!Input || !Output) { AddError(TEXT("Native scorer signature changed")); return -1.f; }
        Input->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), Goal);
        Activity->ProcessEvent(Function, Params.GetStructMemory());
        return Output->GetPropertyValue_InContainer(Params.GetStructMemory());
    };
    TestEqual(TEXT("Empty inventory cannot select ranged combat"), Score(Ranged), 0.f);
    TestEqual(TEXT("Empty inventory cannot select melee combat"), Score(Melee), 0.f);
    Inventory->TryAddItemFromClass(UMeleeWeaponItem::StaticClass(), 1, false);
    TestEqual(TEXT("Sword-only attacker rejects ranged activity"), Score(Ranged), 0.f);
    TestTrue(TEXT("Sword-only attacker selects melee activity"), Score(Melee) > 0.f);
    Inventory->ConsumeItemsOfClass(UMeleeWeaponItem::StaticClass(), 1);
    Inventory->TryAddItemFromClass(URangedWeaponItem::StaticClass(), 1, false);
    TestTrue(TEXT("Ranged weapon enables the existing ranged scorer"), Score(Ranged) > 0.f);
    TestEqual(TEXT("Gun-only inventory rejects melee activity"), Score(Melee), 0.f);
    TestTrue(TEXT("Recreated activity derives eligibility from current Narrative inventory"), Score(MakeActivity(RangedClass)) > 0.f);
    Inventory->ConsumeItemsOfClass(URangedWeaponItem::StaticClass(), 1);
    TestEqual(TEXT("Removing the final weapon immediately invalidates ranged combat"), Score(Ranged), 0.f);
    World->DestroyWorld(false);
    return true;
}

#endif
