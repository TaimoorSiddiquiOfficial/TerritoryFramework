#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardRecordLoadRetirement,
	"TerritoryFramework.Guards.Regression.RecordLoadRetiresAfterReaderReturns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardRecordLoadRetirement::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Territory = World->SpawnActor<ATerritoryProperty>();
	const FGuid TerritoryID(901, 902, 903, 904);
	Territory->SetActorGUID_Implementation(TerritoryID);
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Save->UpdateSaveObject(true);
	FNarrativeActorRecord SavedTerritory;
	TestTrue(TEXT("Native captures the restored territory state"), Save->CreateActorRecord(Territory, SavedTerritory));
	Save->GetSaveObject()->RecordMap.Add(TerritoryID, SavedTerritory);
	auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	Guard->SetActorGUID_Implementation(FGuid(905, 906, 907, 908));
	Territory->SpawnedGuards.Add(Guard);
	Territory->RegisterDefender(Guard);
	auto* Opponent = World->SpawnActor<ANarrativeNPCController>();
	auto* OpponentActivity = Opponent->GetActivityComponent();
	FindFProperty<FObjectProperty>(UNPCActivityComponent::StaticClass(), TEXT("OwnerController"))
		->SetObjectPropertyValue_InContainer(OpponentActivity, Opponent);
	UClass* AttackGoalClass = LoadClass<UNPCGoalItem>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
	TestNotNull(TEXT("The real Native attack goal is available"), AttackGoalClass);
	if (!AttackGoalClass)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	auto* OtherTarget = World->SpawnActor<ATerritoryGuardCharacter>();
	auto AddAttackGoal = [&](ATerritoryGuardCharacter* Target)
	{
		auto* Goal = NewObject<UNPCGoalItem>(OpponentActivity, AttackGoalClass);
		FindFProperty<FObjectProperty>(AttackGoalClass, TEXT("TargetToAttack"))
			->SetObjectPropertyValue_InContainer(Goal, Target);
		return OpponentActivity->AddGoal(Goal);
	};
	UNPCGoalItem* RetiredTargetGoal = AddAttackGoal(Guard);
	UNPCGoalItem* UnrelatedGoal = AddAttackGoal(OtherTarget);
	TestEqual(TEXT("Native starts with both target goals"), OpponentActivity->GetGoals(AttackGoalClass).Goals.Num(), 2);
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	int32 DestructionCallbacks = 0;
	bool bInsideNativeRead = false;
	bool bDestructionDuringRead = false;
	Probe->DestroyedCallback = [&](AActor* Actor)
	{
		++DestructionCallbacks;
		bDestructionDuringRead |= bInsideNativeRead;
		// Reproduce the Native Blueprint EndPlay save behavior observed in HopDistrictTest.
		Save->SaveSingleActor(Actor);
	};
	Guard->OnDestroyed.AddDynamic(Probe, &UTerritoryAuditEventProbe::ActorDestroyed);
	bInsideNativeRead = true;
	Save->LoadActorFromRecord(Territory, Save->GetSaveObject()->RecordMap[TerritoryID]);
	bInsideNativeRead = false;
	TestEqual(TEXT("EndPlay cannot write records inside the Native record reader"), DestructionCallbacks, 0);
	TestEqual(TEXT("Retired guard immediately contributes zero defence"), Territory->GetDefenderCount(), 0);
	TestEqual(TEXT("Retired guard immediately leaves the active cohort"), Territory->GetSpawnedGuardCount(), 0);
	TestTrue(TEXT("Retired guard is hidden during its bounded cleanup grace"), Guard->IsHidden());
	TestFalse(TEXT("Retired guard cannot block or collide with replacements"), Guard->GetActorEnableCollision());
	TestFalse(TEXT("Retirement never fabricates a Native death"), Guard->GetNarrativeAbilitySystemComponent()->IsDead());
	const FNPCGoalContainer RemainingGoals = OpponentActivity->GetGoals(AttackGoalClass);
	TestFalse(TEXT("Native no longer scores a goal targeting the retired guard"), RemainingGoals.Goals.Contains(RetiredTargetGoal));
	TestTrue(TEXT("An unrelated Native target remains available"), RemainingGoals.Goals.Contains(UnrelatedGoal));
	TestFalse(TEXT("Removed attack goals cannot receive a later death callback"),
		Guard->GetNarrativeAbilitySystemComponent()->OnDeathStateChanged.GetAllObjects().Contains(RetiredTargetGoal));
	TestEqual(TEXT("Native reader retains the original territory identity"), Territory->GetTerritoryGUID(), TerritoryID);
	// A second restore before cleanup must not restore or retire the old cohort twice.
	Save->LoadActorFromRecord(Territory, Save->GetSaveObject()->RecordMap[TerritoryID]);
	{
		TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
		World->GetTimerManager().Tick(0.01f);
		++GFrameCounter;
		World->GetTimerManager().Tick(1.f);
	}
	TestEqual(TEXT("Retirement completes once after both readers return"), DestructionCallbacks, 1);
	TestFalse(TEXT("The cleanup never ran inside a Native read"), bDestructionDuringRead);
	TestTrue(TEXT("Deferred Native EndPlay still creates its normal record"), Save->GetSaveObject()->RecordMap.Contains(FGuid(905, 906, 907, 908)));
	Territory->SetRole(ROLE_SimulatedProxy);
	Territory->SpawnedGuards.Add(World->SpawnActor<ATerritoryGuardCharacter>());
	const int32 ClientCohort = Territory->SpawnedGuards.Num();
	Territory->Load_Implementation();
	TestEqual(TEXT("Client load cannot retire server-owned guards"), Territory->SpawnedGuards.Num(), ClientCohort);
	Territory->SetRole(ROLE_Authority);
	Probe->DestroyedCallback = nullptr;
	OpponentActivity->RemoveAllGoals();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
