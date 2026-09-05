#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultRetirementCallbacks,
	"TerritoryFramework.CounterAttack.Regression.RetirementCallbackKeepsReplacementCampaign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultRetirementCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Retirement callback world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	UClass* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	FTerritoryAssaultRecord Original;
	Original.AssaultID = FGuid(221, 222, 223, 224);
	Original.TargetTerritoryGUID = FGuid(225, 226, 227, 228);
	Original.State = ETerritoryAssaultState::Active;
	Original.PlannedForce = 4;
	Original.AliveForce = 1;
	Original.PendingReserveForce = 3;
	FTerritoryAssaultRecord Replacement = Original;
	Replacement.State = ETerritoryAssaultState::Grace;
	Replacement.DecisionSeed = 22022;
	Replacement.KilledForce = 2;
	Replacement.AliveForce = 1;
	Replacement.PendingReserveForce = 1;
	for (bool bResolving : {false, true})
	{
		Counter->RestorePersistentState({Original});
		ATerritoryAssaultCharacter* OldNPC = World->SpawnActor<ATerritoryAssaultCharacter>();
		if (!TestTrue(TEXT("Native assault NPC has its Narrative controller and activity component"),
			OldNPC->EnsureNarrativeControllerReady())) continue;
		Counter->LiveParticipants.FindOrAdd(Original.AssaultID).Add(OldNPC);
		UNPCActivityComponent* Activities = OldNPC->GetActivityComponent();
		// Avoid activity selection in this fixture; exercise the real deactivation
		// delegate invoked by Territory's Narrative cleanup adapter.
		Activities->SetActiveFlag(true);
		UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
		Activities->OnComponentDeactivated.AddDynamic(Probe, &UTerritoryAuditEventProbe::ComponentDeactivated);
		ATerritoryAssaultCharacter* NewNPC = nullptr;
		ANarrativeVehicleBase* NewVehicle = nullptr;
		int32 CallbackCount = 0;
		int32 StaleEvents = 0;
		Probe->AssaultCallback = [&](const FTerritoryAssaultRecord&) { ++StaleEvents; };
		Counter->OnAssaultChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::AssaultChanged);
		Probe->ComponentDeactivationCallback = [&]()
		{
			++CallbackCount;
			Counter->RestorePersistentState({Replacement});
			NewNPC = World->SpawnActor<ATerritoryAssaultCharacter>();
			Counter->LiveParticipants.FindOrAdd(Replacement.AssaultID).Add(NewNPC);
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			NewVehicle = World->SpawnActor<ANarrativeVehicleBase>(VehicleClass,
				FVector(5000.f, bResolving ? 5000.f : 0.f, 300.f), FRotator::ZeroRotator, Spawn);
			Counter->LiveAssaultVehicles.FindOrAdd(Replacement.AssaultID).Add(NewVehicle);
		};
		if (bResolving)
		{
			Counter->ResolveAssault(Counter->Assaults.FindChecked(Original.AssaultID),
				ETerritoryAssaultState::Defeated, ETerritoryAssaultResolution::AllAttackersRemoved);
		}
		else
		{
			Counter->RestorePersistentState({Original});
		}
		Activities->OnComponentDeactivated.RemoveAll(Probe);
		Counter->OnAssaultChanged.RemoveAll(Probe);
		TestEqual(TEXT("Actual Narrative cleanup invokes the nested restore once"), CallbackCount, 1);
		TestEqual(TEXT("The replacement campaign keeps its phase"), Counter->Assaults.FindChecked(Original.AssaultID).State, Replacement.State);
		TestEqual(TEXT("The replacement campaign keeps its seeded decision"), Counter->Assaults.FindChecked(Original.AssaultID).DecisionSeed, Replacement.DecisionSeed);
		TestEqual(TEXT("The replacement campaign keeps casualties"), Counter->Assaults.FindChecked(Original.AssaultID).KilledForce, 2);
		TestTrue(TEXT("Old cleanup cannot erase replacement NPC tracking"), Counter->LiveParticipants.FindRef(Original.AssaultID).Contains(NewNPC));
		TestTrue(TEXT("Old cleanup cannot erase replacement vehicle tracking"), Counter->LiveAssaultVehicles.FindRef(Original.AssaultID).Contains(NewVehicle));
		TestTrue(TEXT("The replacement vehicle remains physical"), IsValid(NewVehicle) && NewVehicle->GetActorEnableCollision());
		TestEqual(TEXT("The replacement vehicle has no stale cleanup timer"), NewVehicle ? NewVehicle->GetLifeSpan() : -1.f, 0.f);
		TestFalse(TEXT("The restore guard is released after nested cleanup"), Counter->bRestoringState);
		TestEqual(TEXT("Superseded resolution emits no old success/state event"), StaleEvents, 0);
	}
	Counter->RestorePersistentState({});
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
