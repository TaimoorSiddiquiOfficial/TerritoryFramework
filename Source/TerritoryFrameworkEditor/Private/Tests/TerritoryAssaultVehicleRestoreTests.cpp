#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Vehicles/MountComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultVehicleRestoreCleanup,
	"TerritoryFramework.CounterAttack.Regression.VehicleReloadClearsSpawnPad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultVehicleRestoreCleanup::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Vehicle restore world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	UClass* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	if (!TestNotNull(TEXT("The actual Narrative sedan Blueprint loads"), VehicleClass))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(211, 212, 213, 214);
	Record.TargetTerritoryGUID = FGuid(215, 216, 217, 218);
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 4;
	Record.AliveForce = 3;
	Record.KilledForce = 1;
	const auto AddVehicle = [&]()
	{
		Counter->RestorePersistentState({Record});
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANarrativeVehicleBase* Vehicle = World->SpawnActor<ANarrativeVehicleBase>(
			VehicleClass, FVector(1000.f, 1000.f, 200.f), FRotator::ZeroRotator, Spawn);
		Counter->LiveAssaultVehicles.FindOrAdd(Record.AssaultID).Add(Vehicle);
		Counter->LiveVehicleRetirementRules.Add(Vehicle, FTerritoryVehicleRetirementSettings());
		return Vehicle;
	};
	ANarrativeVehicleBase* OldVehicle = AddVehicle();
	TestTrue(TEXT("The old assault car initially blocks the pad"), OldVehicle->GetActorEnableCollision());
	Counter->RestorePersistentState({Record});
	TestFalse(TEXT("Reload immediately removes the old car's collision"), OldVehicle->GetActorEnableCollision());
	TestTrue(TEXT("Reload hides the superseded car"), OldVehicle->IsHidden());
	TestTrue(TEXT("The old car survives briefly for Narrative latent dismount callbacks"),
		IsValid(OldVehicle) && !OldVehicle->IsActorBeingDestroyed() && OldVehicle->GetLifeSpan() > 0.f
		&& OldVehicle->GetLifeSpan() <= 0.75f);
	TestTrue(TEXT("Reload does not apply the ordinary twenty-second retirement delay"), Counter->RetiringVehicles.IsEmpty());
	TestEqual(TEXT("Reload preserves the finite casualty record"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 1);
	TestEqual(TEXT("Only the three saved survivors await reconstruction"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 3);

	ANarrativeVehicleBase* TerminalVehicle = AddVehicle();
	Counter->RetireLiveVehicles(Record.AssaultID, true);
	TestTrue(TEXT("Ordinary assault resolution retains scenic retirement"), TerminalVehicle->GetActorEnableCollision());
	TestEqual(TEXT("Ordinary retirement still uses its configured delay"), Counter->RetiringVehicles.Num(), 1);
	Counter->RestorePersistentState({Record});
	TestFalse(TEXT("Reload also clears cars retired by an earlier terminal assault"), TerminalVehicle->GetActorEnableCollision());

	ANarrativeVehicleBase* PassengerVehicle = AddVehicle();
	ANarrativePlayerCharacter* Passenger = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
	Passenger->SetRole(ROLE_Authority);
	ANarrativePlayerController* PassengerController = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	PassengerController->SetRole(ROLE_Authority);
	PassengerController->SetOwnedCharacter(Passenger);
	UMountComponent* Mount = PassengerVehicle->FindComponentByClass<UMountComponent>();
	UNarrativeInteractionComponent* Interaction = PassengerController->FindComponentByClass<UNarrativeInteractionComponent>();
	if (TestNotNull(TEXT("Narrative sedan has native mount slots"), Mount)
		&& TestNotNull(TEXT("Narrative player has its native interaction component"), Interaction))
	{
		FActiveInteractionSlot Slot;
		Slot.SlotStatus = EInteractionSlotStatus::ISS_Occupied;
		Slot.SlotUser = Interaction;
		Mount->SlotStatuses.Add(Slot);
		Counter->RestorePersistentState({Record});
		TestTrue(TEXT("Reload preserves collision under an unpossessed player passenger"), PassengerVehicle->GetActorEnableCollision());
		TestFalse(TEXT("Reload preserves the passenger's visible car"), PassengerVehicle->IsHidden());
		TestEqual(TEXT("Reload does not schedule the passenger's car for destruction"), PassengerVehicle->GetLifeSpan(), 0.f);
		Counter->LiveAssaultVehicles.FindOrAdd(Record.AssaultID).Add(PassengerVehicle);
		Counter->RetireLiveVehicles(Record.AssaultID, true);
		Counter->RetiringVehicles.Last().EarliestRetirementTime = -2.;
		Counter->RetiringVehicles.Last().HardRetirementTime = -1.;
		Counter->UpdateRetiringVehicles();
		TestEqual(TEXT("Ordinary hard timeout also preserves player passengers"), PassengerVehicle->GetLifeSpan(), 0.f);
		TestTrue(TEXT("Hard timeout releases cleanup authority over the player's car"), Counter->RetiringVehicles.IsEmpty());
		Mount->SlotStatuses.Reset();
	}

	ANarrativeVehicleBase* DriverVehicle = AddVehicle();
	APlayerController* Driver = World->SpawnActor<APlayerController>();
	Driver->Possess(DriverVehicle);
	Counter->RestorePersistentState({Record});
	TestTrue(TEXT("Reload preserves a player-driven car"), DriverVehicle->GetActorEnableCollision());
	TestEqual(TEXT("Reload does not schedule a player-driven car for destruction"), DriverVehicle->GetLifeSpan(), 0.f);
	Driver->UnPossess();

	ANarrativeVehicleBase* ClientVehicle = AddVehicle();
	ClientVehicle->SetRole(ROLE_SimulatedProxy);
	Counter->RestorePersistentState({Record});
	TestTrue(TEXT("Campaign cleanup does not mutate a non-authoritative vehicle"), ClientVehicle->GetActorEnableCollision());
	TestEqual(TEXT("Authority rejection preserves the vehicle lifetime"), ClientVehicle->GetLifeSpan(), 0.f);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
