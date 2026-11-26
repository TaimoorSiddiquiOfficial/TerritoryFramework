#include "TerritoryCombatSearchProbe.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

TFunction<void(UBehaviorTreeComponent&)> UTerritoryCombatSearchProbe::OnTick;

UTerritoryCombatSearchProbe::UTerritoryCombatSearchProbe()
{
	bCreateNodeInstance = true;
	Interval = 0.05f;
	RandomDeviation = 0.f;
}

void UTerritoryCombatSearchProbe::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	if (OnTick) OnTick(OwnerComp);
}

#if WITH_DEV_AUTOMATION_TESTS
namespace TerritoryCombatSearchTests
{
// Read-only test access: no Engine state or implementation is replaced.
struct FSearchBoundary : UBehaviorTreeComponent
{
	static bool DefersCleanup(const UBehaviorTreeComponent& BT)
	{
		return (BT.*(&FSearchBoundary::SuspendedBranchActions) & EBTBranchAction::Cleanup) != EBTBranchAction::None;
	}
};

UBehaviorTree* MakeTree(bool bWithExitService, bool bSearchTick)
{
	auto* Tree = NewObject<UBehaviorTree>();
	auto* Root = NewObject<UBTComposite_Sequence>(Tree);
	Tree->RootNode = Root;
	auto* Branch = NewObject<UBTComposite_Sequence>(Tree);
	FBTCompositeChild BranchChild;
	BranchChild.ChildComposite = Branch;
	Root->Children.Add(BranchChild);
	FBTCompositeChild TaskChild;
	TaskChild.ChildTask = NewObject<UBTTask_Wait>(Tree);
	Branch->Children.Add(TaskChild);
	if (bWithExitService)
	{
		auto* Service = NewObject<UTerritoryCombatSearchProbe>(Tree);
		Service->SetSearchTick(bSearchTick);
		Branch->Services.Add(Service);
	}
	return Tree;
}

void CollectAttackServices(const UBTCompositeNode* Node, const UClass* AttackClass, TArray<const UBTService*>& Out)
{
	if (!Node) return;
	for (const UBTService* Service : Node->Services)
		if (Service && Service->IsA(AttackClass)) Out.Add(Service);
	for (const FBTCompositeChild& Child : Node->Children)
	{
		CollectAttackServices(Child.ChildComposite, AttackClass, Out);
		if (Child.ChildTask)
			for (const UBTService* Service : Child.ChildTask->Services)
				if (Service && Service->IsA(AttackClass)) Out.Add(Service);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryCombatSearchExit,
	"TerritoryFramework.AI.Combat.ActivityExitOutsideSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryCombatSearchExit::RunTest(const FString& Parameters)
{
	using namespace TerritoryCombatSearchTests;
	UClass* AttackClass = LoadClass<UBTService>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Services/BTS_Attack.BTS_Attack_C"));
	FBoolProperty* SearchTick = FindFProperty<FBoolProperty>(UBTService::StaticClass(), TEXT("bCallTickOnSearchStart"));
	if (!TestNotNull(TEXT("Native attack service"), AttackClass)
		|| !TestNotNull(TEXT("Engine search-tick setting"), SearchTick)) return false;

	auto ExerciseBoundary = [&](bool bSearchTick, bool bExpectUnsafe, const FString& Label)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestNotNull(Label + TEXT(": world"), World)) return;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->CreateAISystem();
		auto* Controller = World->SpawnActor<AAIController>();
		UBehaviorTree* Initial = MakeTree(true, bSearchTick);
		UBehaviorTree* Replacement = MakeTree(false, false);
		int32 Calls = 0, UnsafeCalls = 0, Replacements = 0;
		UTerritoryCombatSearchProbe::OnTick = [&](UBehaviorTreeComponent& BT)
		{
			if (++Calls > 1) return;
			if (!FSearchBoundary::DefersCleanup(BT))
			{
				++UnsafeCalls;
				// Do not deliberately corrupt heap memory in the regression suite.
				// A separate isolated -stompmalloc reproduction covers this branch.
				return;
			}
			// Native BTS_Attack can RemoveGoal when its target expires. Native
			// activity selection then uses this public cleanup/start-tree boundary.
			BT.Cleanup();
			BT.StartTree(*Replacement);
			++Replacements;
		};
		TestTrue(Label + TEXT(": initial tree runs"), Controller && Controller->RunBehaviorTree(Initial));
		auto* BT = Controller ? Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent()) : nullptr;
		for (int32 Tick = 0; BT && Tick < 10; ++Tick) BT->TickComponent(0.1f, LEVELTICK_All, nullptr);
		TestTrue(Label + TEXT(": service actually ticks"), Calls > 0);
		TestEqual(Label + TEXT(": unprotected callbacks"), UnsafeCalls, bExpectUnsafe ? 1 : 0);
		if (!bExpectUnsafe)
		{
			TestEqual(Label + TEXT(": one replacement"), Replacements, 1);
			TestTrue(Label + TEXT(": replacement is current"), BT && BT->GetCurrentTree() == Replacement);
		}
		UTerritoryCombatSearchProbe::OnTick = nullptr;
		if (BT) BT->Cleanup();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	// Non-mutating negative control prevents a vacuous pass if the fixture stops ticking.
	ExerciseBoundary(true, true, TEXT("Unsafe search-tick control"));
	for (const TCHAR* Path : {
		TEXT("/TerritoryFramework/AI/Combat/BT_TerritoryAttack_Melee.BT_TerritoryAttack_Melee"),
		TEXT("/TerritoryFramework/AI/Combat/BT_TerritoryAttack_Ranged.BT_TerritoryAttack_Ranged")})
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, Path);
		if (!TestNotNull(Path, Tree)) continue;
		TArray<const UBTService*> Services;
		CollectAttackServices(Tree->RootNode, AttackClass, Services);
		TestTrue(FString(Path) + TEXT(": Native attack service is present"), !Services.IsEmpty());
		for (const UBTService* Service : Services)
			ExerciseBoundary(SearchTick->GetPropertyValue_InContainer(Service), false, Service->GetPathName());
	}
	return true;
}
#endif
