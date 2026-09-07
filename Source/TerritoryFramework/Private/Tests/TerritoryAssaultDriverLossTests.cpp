#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeNPCController.h"
#include "Character/NarrativeCharacterVisual.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Engine/World.h"
#include "Interaction/NPCInteractionComponent.h"
#include "UObject/UnrealType.h"
#include "Vehicles/MountComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultPassengerDriverLoss,
	"TerritoryFramework.CounterAttack.Regression.PassengersSurviveDriverLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultPassengerDriverLoss::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Driver loss world exists"), World)) return false;
	World->CreateAISystem();
	auto* Driver = World->SpawnActor<ATerritoryAssaultCharacter>();
	auto* Passenger = World->SpawnActor<ATerritoryAssaultCharacter>();
	auto* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	auto* Vehicle = VehicleClass ? World->SpawnActor<ANarrativeVehicleBase>(VehicleClass) : nullptr;
	if (!Driver || !Passenger || !Vehicle || !Passenger->EnsureNarrativeControllerReady())
	{
		AddError(TEXT("Native passenger/controller/vehicle fixture could not initialize"));
		World->DestroyWorld(false);
		return false;
	}
	// Supply completed Native definition/visual prerequisites without asynchronous
	// appearance loading. Rendered PIE separately exercises the real mount ability.
	FindFProperty<FObjectPropertyBase>(Passenger->GetClass(), TEXT("NPCDefinition"))
		->SetObjectPropertyValue_InContainer(Passenger, NewObject<UNPCDefinition>());
	FindFProperty<FObjectPropertyBase>(Passenger->GetClass(), TEXT("CharVisual"))
		->SetObjectPropertyValue_InContainer(Passenger, World->SpawnActor<ANarrativeCharacterVisual>());
	TestTrue(TEXT("Passenger satisfies Native readiness"), Passenger->IsNarrativeSpawnReady());
	auto* Part = Passenger->AssaultParticipant.Get();
	auto* DriverPart = Driver->AssaultParticipant.Get();
	auto* Interaction = Passenger->GetNPCController()->GetInteractionComponent();
	auto* Mount = Vehicle->FindComponentByClass<UMountComponent>();
	if (!TestNotNull(TEXT("Native interaction exists"), Interaction)
		|| !TestNotNull(TEXT("Native mount exists"), Mount))
	{
		World->DestroyWorld(false);
		return false;
	}
	FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("OwningPawn"))
		->SetObjectPropertyValue_InContainer(Interaction, Passenger);
	const auto ResetPassenger = [&]()
	{
		Part->ConfigureNarrativeVehiclePassenger(DriverPart, Vehicle, 1,
			{FVector::ZeroVector, FVector(1000, 0, 0)}, FTransform(), FTransform(), 120.f, false);
		// Narrative has already attempted entry; no occupied slot represents its
		// completed exit callback (also possible during driver death on entry).
		Part->bVehicleMountRequested = true;
	};
	ResetPassenger();
	TestTrue(TEXT("A healthy driver keeps the passenger waiting"), Part->EnsureNarrativeVehicleIngress());
	TestFalse(TEXT("Normal ingress does not request an early exit"), Part->bVehicleDismountRequested);
	DriverPart->Retire(true);
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		ResetPassenger();
		if (Scenario == 1) Part->NarrativeVehicleDriver.Reset();
		if (Scenario == 2)
		{
			DriverPart->bRemovalReported = false;
			DriverPart->bVehicleIngressFailed = true;
		}
		Passenger->SetRole(ROLE_SimulatedProxy);
		TestFalse(TEXT("Clients cannot advance passenger ingress"), Part->EnsureNarrativeVehicleIngress());
		TestFalse(TEXT("Client call cannot request a dismount"), Part->bVehicleDismountRequested);
		Passenger->SetRole(ROLE_Authority);
		TestTrue(TEXT("Retired, removed and failed drivers permit passenger recovery"), Part->EnsureNarrativeVehicleIngress());
		TestTrue(TEXT("Driver loss requests the existing dismount path"), Part->bVehicleDismountRequested);
		TestFalse(TEXT("Driver loss does not retire a healthy passenger"), Part->HasRetired());
		TestFalse(TEXT("Driver loss does not mark passenger ingress failed"), Part->bVehicleIngressFailed);
		TestFalse(TEXT("Pending dismount adds no capture pressure"), Part->IsCaptureRegistered());
		Part->EnsureNarrativeVehicleIngress();
		TestFalse(TEXT("Native exit completion releases the wave's ingress gate"), Part->IsVehicleIngressPending());
		TestTrue(TEXT("Surviving passenger can resume Narrative combat"), Part->CanEngageCombat());
	}
	ResetPassenger();
	// A still-occupied Native slot without an exitable ability must remain pending.
	// Territory must not forcibly free it or claim an arrival after a rejected exit.
	auto* OccupiedProperty = FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("OccupiedInteractable"));
	OccupiedProperty->SetObjectPropertyValue_InContainer(Interaction, Mount);
	TestTrue(TEXT("Rejected Native exit remains retryable"), Part->EnsureNarrativeVehicleIngress());
	TestFalse(TEXT("Rejected exit cannot claim dismount"), Part->bVehicleDismountRequested);
	TestTrue(TEXT("Narrative retains its occupied slot"), Interaction->HasOccupiedInteractable());
	TestFalse(TEXT("Rejected exit contributes no capture pressure"), Part->IsCaptureRegistered());
	OccupiedProperty->SetObjectPropertyValue_InContainer(Interaction, nullptr);
	if (auto* Movement = Vehicle->FindComponentByClass<UChaosWheeledVehicleMovementComponent>())
	{
		auto* OtherController = World->SpawnActor<AAIController>();
		OtherController->Possess(Vehicle);
		Movement->SetThrottleInput(0.6f);
		Movement->SetBrakeInput(0.f);
		Part->StopVehicleInputs();
		TestEqual(TEXT("A passenger cannot brake another controller's vehicle"), Movement->GetBrakeInput(), 0.f);
		TestEqual(TEXT("A passenger cannot cancel its driver's throttle"), Movement->GetThrottleInput(), 0.6f);
		DriverPart->ConfigureNarrativeVehicleIngress(Vehicle,
			{FVector::ZeroVector, FVector(1000, 0, 0)}, FTransform(), FTransform(), 1000.f, 120.f, false,
			FTerritoryVehicleAwarenessSettings());
		DriverPart->StopVehicleInputs();
		TestEqual(TEXT("An old driver cannot brake a taken-over car"), Movement->GetBrakeInput(), 0.f);
		OtherController->UnPossess();
		Part->StopVehicleInputs();
		TestEqual(TEXT("Passengers brake an abandoned car before exiting"), Movement->GetBrakeInput(), 1.f);
		TestEqual(TEXT("An abandoned car has no stale throttle"), Movement->GetThrottleInput(), 0.f);
	}
	else AddError(TEXT("Authored sedan has no Chaos movement"));
	Part->Retire(false);
	TestFalse(TEXT("Retired passengers cannot resume ingress"), Part->EnsureNarrativeVehicleIngress());
	World->DestroyWorld(false);
	return true;
}

#endif
