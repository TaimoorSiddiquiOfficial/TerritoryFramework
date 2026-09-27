#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/TerritoryPatrolGoal.h"
#include "AI/NarrativeNPCController.h"
#include "Combat/TerritoryWeaponSelectionLibrary.h"
#include "Engine/World.h"
#include "Items/InventoryComponent.h"
#include "Items/MeleeWeaponItem.h"
#include "Items/RangedWeaponItem.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFMeleeDistancePreference,
	"TerritoryFramework.AI.Combat.MeleeDistanceAndHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFMeleeDistancePreference::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) return false;
	World->CreateAISystem();
	UClass* NPCClass = LoadClass<ANarrativeNPCCharacter>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryStoryNPC.BP_TerritoryStoryNPC_C"));
	if (!TestNotNull(TEXT("Concrete Narrative stable NPC"), NPCClass))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Controller = World->SpawnActor<ANarrativeNPCController>();
	auto* Pawn = World->SpawnActor<ANarrativeNPCCharacter>(NPCClass);
	auto* Target = World->SpawnActor<ANarrativeNPCCharacter>(NPCClass);
	if (!Controller || !Pawn || !Target)
	{
		AddError(TEXT("Could not create Narrative combat fixture"));
		World->DestroyWorld(false);
		return false;
	}
	Controller->SetPawn(Pawn);
	Pawn->SetActorLocation(FVector::ZeroVector);
	auto* Activities = Controller->GetActivityComponent();
	UClass* MeleeClass = LoadClass<UNPCActivity>(nullptr,
		TEXT("/TerritoryFramework/AI/Combat/BPA_TerritoryAttack_Melee.BPA_TerritoryAttack_Melee_C"));
	if (!TestNotNull(TEXT("Concrete melee activity"), MeleeClass))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Melee = NewObject<UNPCActivity>(Controller, MeleeClass);
	Melee->SetOwner(Controller, Activities);
	auto* Goal = NewObject<UTerritoryPatrolGoal>(Controller);
	Goal->OwnerController = Controller;
	Goal->GoalKey = Target;
	auto Score = [&](float Base = 3.9f)
	{
		return UTerritoryWeaponSelectionLibrary::PreferMeleeAtCloseRange(Controller, Goal, Melee, Base);
	};
	Target->SetActorLocation(FVector(1200.f, 0.f, 0.f));
	TestEqual(TEXT("Far target retains Native preference for ranged attack"), Score(), 3.9f);
	Target->SetActorLocation(FVector(250.f, 0.f, 0.f));
	TestTrue(TEXT("Nearby eligible melee wins over Native attack score 4"), Score() > 4.f);
	TestEqual(TEXT("Blocked score stays blocked"), Score(0.f), 0.f);
	TestEqual(TEXT("Native unreachable fallback is never promoted"), Score(0.2f), 0.2f);
	TestEqual(TEXT("Expired goal remains removable"), Score(-1.f), -1.f);
	Target->SetActorLocation(FVector(375.f, 0.f, 0.f));
	TestEqual(TEXT("Ranged stays selected between the two thresholds"), Score(), 3.9f);
	// Set only the same two fields used by Native activity admission. No second
	// remembered combat mode is introduced by the preference helper.
	FindFProperty<FObjectPropertyBase>(UNPCActivityComponent::StaticClass(), TEXT("CurrentActivity"))
		->SetObjectPropertyValue_InContainer(Activities, Melee);
	FindFProperty<FObjectPropertyBase>(UNPCActivity::StaticClass(), TEXT("ActivityGoal"))
		->SetObjectPropertyValue_InContainer(Melee, Goal);
	TestTrue(TEXT("Current melee is retained inside its wider exit radius"), Score() > 4.f);
	Target->SetActorLocation(FVector(500.f, 0.f, 0.f));
	TestEqual(TEXT("Escaping target restores ranged preference"), Score(), 3.9f);
	Controller->SetRole(ROLE_SimulatedProxy);
	TestEqual(TEXT("Client cannot score combat"), Score(), 0.f);
	Controller->SetRole(ROLE_Authority);
	Goal->OwnerController = nullptr;
	TestEqual(TEXT("Foreign/unowned goal cannot score combat"), Score(), 0.f);
	FindFProperty<FObjectPropertyBase>(UNPCActivityComponent::StaticClass(), TEXT("CurrentActivity"))
		->SetObjectPropertyValue_InContainer(Activities, nullptr);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFShippedMeleePreference,
	"TerritoryFramework.AI.Combat.ShippedMeleeInheritanceAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFShippedMeleePreference::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Combat world"), World)) return false;
	World->CreateAISystem();
	UClass* NPCClass = LoadClass<ANarrativeNPCCharacter>(nullptr,
		TEXT("/TerritoryFramework/AI/BP_TerritoryStoryNPC.BP_TerritoryStoryNPC_C"));
	UClass* NativeMelee = LoadClass<UNPCActivity>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/MeleeAttack/BPA_Attack_Melee.BPA_Attack_Melee_C"));
	UClass* GoalClass = LoadClass<UNPCGoalItem>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
	auto* Pawn = NPCClass ? World->SpawnActor<ANarrativeNPCCharacter>(NPCClass) : nullptr;
	auto* Target = NPCClass ? World->SpawnActor<ANarrativeNPCCharacter>(NPCClass) : nullptr;
	auto* Controller = World->SpawnActor<ANarrativeNPCController>();
	if (!Pawn || !Target || !Controller || !NativeMelee || !GoalClass)
	{
		AddError(TEXT("Installed Native combat fixture unavailable"));
		World->DestroyWorld(false); return false;
	}
	Controller->SetPawn(Pawn);
	Pawn->SetActorLocation(FVector::ZeroVector);
	auto* Goal = NewObject<UNPCGoalItem>(Controller, GoalClass);
	Goal->OwnerController = Controller;
	Goal->DefaultScore = 4.f;
	FindFProperty<FObjectPropertyBase>(GoalClass, TEXT("TargetToAttack"))->SetObjectPropertyValue_InContainer(Goal, Target);
	const auto SetBool = [&](const TCHAR* Name, bool Value)
	{
		auto* Property = FindFProperty<FBoolProperty>(GoalClass, Name);
		if (TestNotNull(Name, Property)) Property->SetPropertyValue_InContainer(Goal, Value);
	};
	const auto SetTime = [&](const TCHAR* Name)
	{
		auto* Property = FindFProperty<FDoubleProperty>(GoalClass, Name);
		if (TestNotNull(Name, Property)) Property->SetPropertyValue_InContainer(Goal, -10.0);
	};
	SetBool(TEXT("IsAlert"), true); SetTime(TEXT("AlertChangedTime"));
	SetTime(TEXT("VisibilityChangedTime")); SetTime(TEXT("ReachabilityChangedTime"));
	auto* Inventory = Pawn->GetInventoryComponent();
	Inventory->TryAddItemFromClass(UMeleeWeaponItem::StaticClass(), 1, false);
	Inventory->TryAddItemFromClass(URangedWeaponItem::StaticClass(), 1, false);
	const auto MakeActivity = [&](UClass* Class)
	{
		auto* Activity = NewObject<UNPCActivity>(Controller, Class);
		Activity->SetOwner(Controller, Controller->GetActivityComponent());
		return Activity;
	};
	const auto Score = [&](UNPCActivity* Activity)
	{
		UFunction* Function = Activity->FindFunction(TEXT("ScoreGoalItem"));
		FStructOnScope Arguments(Function);
		FindFProperty<FObjectPropertyBase>(Function, TEXT("Goal"))->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), Goal);
		Activity->ProcessEvent(Function, Arguments.GetStructMemory());
		return FindFProperty<FFloatProperty>(Function, TEXT("ReturnValue"))->GetPropertyValue_InContainer(Arguments.GetStructMemory());
	};
	auto* Native = MakeActivity(NativeMelee);
	for (const TCHAR* Mount : {TEXT("/TerritoryFramework"), TEXT("/Game/TerritoryFramework")})
	{
		const FString MeleePath = FString::Printf(TEXT("%s/AI/Combat/BPA_TerritoryAttack_Melee.BPA_TerritoryAttack_Melee_C"), Mount);
		UClass* MeleeClass = LoadClass<UNPCActivity>(nullptr, *MeleePath, nullptr, LOAD_NoWarn);
		if (!MeleeClass && FCString::Strcmp(Mount, TEXT("/Game/TerritoryFramework")) == 0) continue;
		if (!TestNotNull(*MeleePath, MeleeClass)) continue;
		TestEqual(TEXT("Melee inherits installed Native implementation"), MeleeClass->GetSuperClass(), NativeMelee);
		UClass* RangedClass = LoadClass<UNPCActivity>(nullptr, *FString::Printf(
			TEXT("%s/AI/Combat/BPA_TerritoryAttack_Ranged_Strafe.BPA_TerritoryAttack_Ranged_Strafe_C"), Mount));
		if (!TestNotNull(TEXT("Ranged sibling"), RangedClass)) continue;
		auto* Melee = MakeActivity(MeleeClass);
		auto* Ranged = MakeActivity(RangedClass);
		SetBool(TEXT("CanSeeTarget"), true); SetBool(TEXT("CanReachTarget"), true);
		Target->SetActorLocation(FVector(1200, 0, 0));
		TestEqual(TEXT("Far melee preserves Native score"), Score(Melee), Score(Native));
		TestTrue(TEXT("Dual weapon NPC prefers ranged at distance"), Score(Ranged) > Score(Melee));
		Target->SetActorLocation(FVector(200, 0, 0));
		TestTrue(TEXT("Dual weapon NPC prefers melee nearby"), Score(Melee) > Score(Ranged));
		SetBool(TEXT("CanReachTarget"), false);
		TestEqual(TEXT("Installed Native unreachable fallback contract"), Score(Native), 0.2f);
		TestEqual(TEXT("Wrapper preserves unreachable fallback at close range"), Score(Melee), Score(Native));
		SetBool(TEXT("CanSeeTarget"), false);
		TestEqual(TEXT("Hidden target remains ineligible"), Score(Melee), 0.f);
		SetBool(TEXT("CanSeeTarget"), true); SetBool(TEXT("CanReachTarget"), true);
		SetBool(TEXT("IsAlert"), false);
		TestEqual(TEXT("Unalert NPC does not attack nearby actors"), Score(Melee), 0.f);
		SetBool(TEXT("IsAlert"), true);
		Inventory->ConsumeItemsOfClass(UMeleeWeaponItem::StaticClass(), 1);
		TestEqual(TEXT("Close range cannot fabricate a missing melee weapon"), Score(Melee), 0.f);
		Inventory->TryAddItemFromClass(UMeleeWeaponItem::StaticClass(), 1, false);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
