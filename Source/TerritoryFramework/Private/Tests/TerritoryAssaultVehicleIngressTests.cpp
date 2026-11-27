#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeNPCController.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Engine/World.h"
#include "GAS/NarrativeInteractAbility.h"
#include "Interaction/InteractableComponent.h"
#include "Interaction/NPCInteractionComponent.h"
#include "UObject/UnrealType.h"
#include "Vehicles/MountComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"

namespace
{
	/** Names every authored seat of a spawned vehicle that cannot start a Narrative interact ability. */
	FString DescribeUnusableSeats(const UMountComponent* Mount)
	{
		TArray<FString> UnusableSeats;
		for (int32 SeatIndex = 0; SeatIndex < Mount->InteractionSlots.Num(); ++SeatIndex)
		{
			const FInteractionSlotConfig& Slot = Mount->InteractionSlots[SeatIndex];
			const bool bBehaviourUsable = IsValid(Slot.SlotInteractBehavior)
				&& IsValid(Slot.SlotInteractBehavior->GetInteractAbility());
			if (!bBehaviourUsable)
			{
				UnusableSeats.Add(FString::Printf(TEXT("seat=%d behavior=%s ability=%s"), SeatIndex,
					*GetNameSafe(Slot.SlotInteractBehavior),
					*GetNameSafe(Slot.SlotInteractBehavior
						? Slot.SlotInteractBehavior->GetInteractAbility().Get() : nullptr)));
			}
		}
		return FString::Join(UnusableSeats, TEXT("; "));
	}
}

/**
 * Regression: a refused Narrative seat claim is reported once and fails this ingress attempt.
 *
 * The rendered counterattack reproduced "Narrative mount interaction rejected <driver|passenger>
 * <pawn> seat=N vehicle=BPV_Sedan_C" followed in the same frame by "withdrew before vehicle arrival".
 * Narrative answers both interaction calls with a bare bool and logs no reason, so Territory reports
 * the state Narrative consulted exactly once and then latches the same structural ingress failure the
 * component already uses for an unusable vehicle, controller or route.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSeatClaimRetry,
	"TerritoryFramework.CounterAttack.Regression.RejectedSeatClaimFailsIngressOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSeatClaimRetry::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Seat claim world exists"), World)) return false;
	World->CreateAISystem();
	auto* Attacker = World->SpawnActor<ATerritoryAssaultCharacter>();
	auto* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C"));
	auto* Vehicle = VehicleClass
		? World->SpawnActor<ANarrativeVehicleBase>(VehicleClass) : nullptr;
	if (!Attacker || !Vehicle || !Attacker->EnsureNarrativeControllerReady())
	{
		AddError(TEXT("Native attacker/controller/vehicle fixture could not initialize"));
		World->DestroyWorld(false);
		return false;
	}
	// Supply completed Native definition/visual prerequisites without asynchronous appearance
	// loading, matching the other headless counterattack regression fixtures.
	FindFProperty<FObjectPropertyBase>(Attacker->GetClass(), TEXT("NPCDefinition"))
		->SetObjectPropertyValue_InContainer(Attacker, NewObject<UNPCDefinition>());
	FindFProperty<FObjectPropertyBase>(Attacker->GetClass(), TEXT("CharVisual"))
		->SetObjectPropertyValue_InContainer(Attacker,
			World->SpawnActor<ANarrativeCharacterVisual>());
	Attacker->GetCharacterVisual()->bBaseAppearanceLoaded = true;
	TestTrue(TEXT("Attacker satisfies Native readiness"), Attacker->IsNarrativeSpawnReady());
	auto* Part = Attacker->AssaultParticipant.Get();
	auto* Interaction = Attacker->GetNPCController()->GetInteractionComponent();
	auto* Mount = Vehicle->FindComponentByClass<UMountComponent>();
	if (!TestNotNull(TEXT("Native interaction exists"), Interaction)
		|| !TestNotNull(TEXT("Native mount exists"), Mount))
	{
		World->DestroyWorld(false);
		return false;
	}
	FindFProperty<FObjectPropertyBase>(Interaction->GetClass(), TEXT("OwningPawn"))
		->SetObjectPropertyValue_InContainer(Interaction, Attacker);
	Part->ConfigureNarrativeVehicleIngress(Vehicle,
		{FVector::ZeroVector, FVector(1000, 0, 0)}, FTransform(), FTransform(), 1000.f, 120.f,
		false, FTerritoryVehicleAwarenessSettings());
	TestFalse(TEXT("A live vehicle with a route does not pre-fail ingress"),
		Part->bVehicleIngressFailed);
	// Narrative sizes the claimable slot statuses in BeginPlay. This fixture never begins play, so
	// the authoritative claim is refused; assert the fixture premise before the contract it proves.
	TestEqual(TEXT("Fixture relies on Narrative sizing claimable slots at BeginPlay"),
		Mount->SlotStatuses.Num(), 0);
	TestFalse(TEXT("A refused seat claim fails this attempt"),
		Part->EnsureNarrativeVehicleIngress());
	TestTrue(TEXT("A refused seat claim latches the structural ingress failure"),
		Part->bVehicleIngressFailed);
	TestFalse(TEXT("A refused seat claim claims no mount"), Part->bVehicleMountRequested);
	TestTrue(TEXT("A refused seat claim reports the consulted state once"),
		Part->bVehicleSeatClaimRejectionReported);
	TestFalse(TEXT("A retried refusal fails that attempt too"),
		Part->EnsureNarrativeVehicleIngress());
	TestTrue(TEXT("Retrying a refused claim never clears the latched ingress failure"),
		Part->bVehicleIngressFailed);
	TestFalse(TEXT("Retrying a refused claim still claims no mount"),
		Part->bVehicleMountRequested);
	Part->Retire(false);
	TestFalse(TEXT("Retired attackers cannot resume the refused claim"),
		Part->EnsureNarrativeVehicleIngress());
	World->DestroyWorld(false);
	return true;
}

/**
 * Regression: every vehicle the counterattack can author must expose usable Narrative seat behaviours.
 *
 * Narrative Pro 2.5 re-saved the demo sedan so its construction-script seat behaviours no longer reach
 * spawned instances: the mount keeps four authored slots whose SlotInteractBehavior is null, so
 * TargetInteractionSlot succeeds and RunInteractBehavior silently refuses. The counterattack therefore
 * authors /Game/HOPTRENDY/BPV_Sedan, which still instantiates its behaviours. This test reads the
 * shipped profile and place definition, spawns exactly the classes they author, and refuses any class
 * whose seats cannot start an interact ability.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultAuthoredVehicleSeats,
	"TerritoryFramework.CounterAttack.Regression.AuthoredVehiclesExposeSeatBehaviours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultAuthoredVehicleSeats::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Seat authoring world exists"), World)) return false;

	auto CheckVehicleClass = [this, World](const TSoftClassPtr<ANarrativeVehicleBase>& SoftClass,
		const FString& AuthoredBy) -> bool
	{
		UClass* VehicleClass = SoftClass.LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("%s authors a loadable vehicle class"), *AuthoredBy),
			VehicleClass))
		{
			return false;
		}
		auto* Vehicle = World->SpawnActor<ANarrativeVehicleBase>(VehicleClass);
		if (!TestNotNull(*FString::Printf(TEXT("%s vehicle spawns"), *AuthoredBy), Vehicle)) return false;
		UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
		bool bPassed = TestNotNull(*FString::Printf(TEXT("%s vehicle exposes a Narrative mount"),
			*AuthoredBy), Mount) && Mount->InteractionSlots.Num() > 0;
		if (bPassed)
		{
			const FString UnusableSeats = DescribeUnusableSeats(Mount);
			bPassed = TestTrue(*FString::Printf(
				TEXT("%s vehicle %s instantiates a usable interact behaviour on every authored seat (%s)"),
				*AuthoredBy, *GetNameSafe(VehicleClass),
				UnusableSeats.IsEmpty() ? TEXT("all seats usable") : *UnusableSeats),
				UnusableSeats.IsEmpty());
		}
		Vehicle->Destroy();
		return bPassed;
	};

	bool bPassed = true;
	auto* Profile = LoadObject<UTerritoryCounterAttackProfile>(nullptr,
		TEXT("/Game/TerritoryFramework/DA_CounterAttack.DA_CounterAttack"));
	if (TestNotNull(TEXT("Shipped counterattack profile loads"), Profile))
	{
		for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
		{
			if (Force.SignatureVehicleClass.IsNull()) continue;
			bPassed &= CheckVehicleClass(Force.SignatureVehicleClass,
				TEXT("DA_CounterAttack faction signature"));
		}
	}
	auto* Definition = LoadObject<UTerritoryPlaceDefinition>(nullptr,
		TEXT("/Game/TerritoryFramework/Definitions/DA_Place_Blacksmith.DA_Place_Blacksmith"));
	if (TestNotNull(TEXT("Shipped Blacksmith place definition loads"), Definition))
	{
		for (const FTerritoryAssaultApproach& Approach : Definition->CounterAttackApproaches)
		{
			if (Approach.EntryType != ETerritoryAssaultEntryType::NarrativeVehicle) continue;
			bPassed &= CheckVehicleClass(Approach.VehicleClass,
				FString::Printf(TEXT("DA_Place_Blacksmith approach %s"), *Approach.ApproachID.ToString()));
		}
	}
	World->DestroyWorld(false);
	return bPassed;
}

/**
 * Regression: Territory reports an unusable seat behaviour once and keeps the claim retryable.
 *
 * The authored vehicle satisfies the helper without a repair, while a seat whose behaviour is missing
 * (the exact Narrative 2.5 instancing defect) is reported once through bVehicleSeatBehaviourRepairReported
 * instead of silently refusing every attempt.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSeatBehaviourReport,
	"TerritoryFramework.CounterAttack.Regression.MissingSeatBehaviourIsReportedOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSeatBehaviourReport::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Seat behaviour world exists"), World)) return false;
	World->CreateAISystem();
	auto* Attacker = World->SpawnActor<ATerritoryAssaultCharacter>();
	auto* VehicleClass = LoadClass<ANarrativeVehicleBase>(nullptr,
		TEXT("/Game/HOPTRENDY/BPV_Sedan.BPV_Sedan_C"));
	auto* Vehicle = VehicleClass ? World->SpawnActor<ANarrativeVehicleBase>(VehicleClass) : nullptr;
	if (!Attacker || !Vehicle || !Attacker->EnsureNarrativeControllerReady())
	{
		AddError(TEXT("Native attacker/controller/vehicle fixture could not initialize"));
		World->DestroyWorld(false);
		return false;
	}
	FindFProperty<FObjectPropertyBase>(Attacker->GetClass(), TEXT("NPCDefinition"))
		->SetObjectPropertyValue_InContainer(Attacker, NewObject<UNPCDefinition>());
	FindFProperty<FObjectPropertyBase>(Attacker->GetClass(), TEXT("CharVisual"))
		->SetObjectPropertyValue_InContainer(Attacker,
			World->SpawnActor<ANarrativeCharacterVisual>());
	Attacker->GetCharacterVisual()->bBaseAppearanceLoaded = true;
	auto* Part = Attacker->AssaultParticipant.Get();
	UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
	if (!TestNotNull(TEXT("Native mount exists"), Mount)
		|| !TestTrue(TEXT("Fixture sedan exposes authored seats"), Mount->InteractionSlots.Num() > 0))
	{
		World->DestroyWorld(false);
		return false;
	}
	Part->ConfigureNarrativeVehicleIngress(Vehicle,
		{FVector::ZeroVector, FVector(1000, 0, 0)}, FTransform(), FTransform(), 1000.f, 120.f,
		false, FTerritoryVehicleAwarenessSettings());
	const FString AuthoredSeats = DescribeUnusableSeats(Mount);
	TestTrue(*FString::Printf(TEXT("The authored vehicle instantiates every seat behaviour (%s)"),
		AuthoredSeats.IsEmpty() ? TEXT("all seats usable") : *AuthoredSeats), AuthoredSeats.IsEmpty());
	TestTrue(TEXT("An authored seat needs no repair"),
		Part->EnsureNarrativeSeatInteractBehaviour(Mount));
	TestFalse(TEXT("A usable authored seat reports no repair"),
		Part->bVehicleSeatBehaviourRepairReported);
	Mount->InteractionSlots[0].SlotInteractBehavior = nullptr;
	TestFalse(TEXT("A seat without an authored behaviour cannot be repaired"),
		Part->EnsureNarrativeSeatInteractBehaviour(Mount));
	TestTrue(TEXT("An unusable seat is reported once"),
		Part->bVehicleSeatBehaviourRepairReported);
	TestFalse(TEXT("Retrying an unusable seat never latches ingress failure from the repair helper"),
		Part->EnsureNarrativeSeatInteractBehaviour(Mount));
	TestFalse(TEXT("The repair helper never latches the participant ingress failure"),
		Part->bVehicleIngressFailed);
	World->DestroyWorld(false);
	return true;
}

#endif
