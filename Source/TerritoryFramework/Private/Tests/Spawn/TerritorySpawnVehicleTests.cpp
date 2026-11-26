#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnTags.h"
#include "Spawn/TerritorySpawnTypes.h"
#include "Tests/Spawn/TerritorySpawnTestFixtures.h"
#include "Vehicles/TerritorySpawnExecutorVehicle.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Vehicles/TerritorySpawnPolicyAmbulance.h"
#include "Vehicles/TerritorySpawnPolicyPersonalVehicle.h"
#include "Vehicles/TerritorySpawnPolicyVehicle.h"

/**
 * Behavioural proof for the vehicle adapter.
 *
 * These tests deliberately do NOT spawn a vehicle. Both Narrative vehicle bases are UCLASS(abstract) and
 * the concrete cars are Blueprints, so a C++ test cannot instantiate one without the editor's asset
 * registry — and a test that needed the editor would not run in the automation harness that gates this
 * work. What is proven here instead is every decision the adapter makes, driven through the real
 * FTerritorySpawnRequest the sweep builds, plus the executor's class contract and occupancy rule.
 *
 * What therefore remains UNPROVEN by this file, and must be proven in PIE with the real Blueprints:
 *   - the car actually appearing at the pad, and the pad's clearance.
 *   - health surviving a real retire/spawn cycle through the Native ASC.
 *   - SetVehicleSaveGuid clearing the vendor record so exactly one save entry exists per car.
 */
namespace TerritorySpawnVehicleTests
{
	/** Build a request that looks like "nothing is here, this slot has never spawned". */
	FTerritorySpawnRequest MakeEmptyRequest(double NowGameTime)
	{
		FTerritorySpawnRequest Request;
		Request.AnchorGuid = FGuid::NewGuid();
		Request.EntityTag = TerritorySpawnTags::EntityActor;
		Request.CurrentState = ESpawnLifecycleState::Dormant;
		Request.NowGameTime = NowGameTime;
		Request.AnchorTransform = FTransform(FRotator::ZeroRotator, FVector(1200.0, 400.0, 10.0));
		Request.ConsecutiveFailures = 0;
		Request.PendingSinceGameTime = -1.0;
		Request.bHasLiveLease = false;
		Request.bHasEntity = false;
		Request.EntityTransform = FTransform::Identity;
		return Request;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Personal vehicle policy
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehiclePersonalCarFirstSpawnTest,
	"TerritoryFramework.Spawn.Vehicle.PersonalCarFirstSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehiclePersonalCarFirstSpawnTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UTerritorySpawnPolicyPersonalVehicle* const Policy = NewObject<UTerritorySpawnPolicyPersonalVehicle>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	Policy->VehicleClass = TSoftClassPtr<AActor>(AActor::StaticClass());
	Policy->ReturnGraceSeconds = 30.f;
	Policy->ReturnPlacement = ESpawnVehicleReturnPlacement::AtAnchor;

	// A bay has never held a car. LastTransitionGameTime is still the default -1, so there is nothing to
	// wait for and the first spawn must not be delayed by the return grace. Getting this wrong makes a
	// freshly loaded district look broken for thirty seconds.
	FTerritorySpawnRequest Request = MakeEmptyRequest(100.0);
	const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);

	TestTrue(TEXT("A never-used bay asks for a spawn"), Decision.Action == ESpawnAction::Spawn);
	TestTrue(TEXT("The first spawn is reported as an initial spawn"),
		Decision.Reason == ESpawnTransitionReason::InitialSpawn);

	// Placement: no usable record transform exists, so the authored pad wins. This is the case a designer
	// actually authored, and a policy that returned Identity here would drop the car at the world origin.
	TestTrue(TEXT("The first spawn lands on the authored pad"),
		Decision.DesiredTransform.GetLocation().Equals(Request.AnchorTransform.GetLocation(), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehiclePersonalCarGraceTest,
	"TerritoryFramework.Spawn.Vehicle.PersonalCarRespectsReturnGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehiclePersonalCarGraceTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UTerritorySpawnPolicyPersonalVehicle* const Policy = NewObject<UTerritorySpawnPolicyPersonalVehicle>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	Policy->ReturnGraceSeconds = 30.f;

	// The car was put away at t=100. At t=110 the grace has not elapsed, so nothing happens; at t=131 it
	// has. Both halves matter: the first is the behaviour, the second is the control that proves the
	// clock is actually being read rather than the slot never respawning at all.
	FTerritorySpawnRequest Request = MakeEmptyRequest(110.0);
	Request.Record.bWasDestroyed = false;
	Request.Record.LastTransitionGameTime = 100.0;

	TestTrue(TEXT("Inside the grace window nothing spawns"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::None);

	Request.NowGameTime = 131.0;
	TestTrue(TEXT("Past the grace window the car comes back"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::Spawn);

	return true;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehiclePersonalCarAsLastLeftTest,
	"TerritoryFramework.Spawn.Vehicle.PersonalCarAsLastLeft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehiclePersonalCarAsLastLeftTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UTerritorySpawnPolicyPersonalVehicle* const Policy = NewObject<UTerritorySpawnPolicyPersonalVehicle>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	Policy->ReturnGraceSeconds = 0.f;
	Policy->ReturnPlacement = ESpawnVehicleReturnPlacement::AsLastLeft;

	// Where the car was left, which is NOT the pad.
	const FTransform LastLeft(FRotator(0.f, 90.f, 0.f), FVector(-2000.0, 750.0, 12.0));

	FTerritorySpawnRequest Request = MakeEmptyRequest(200.0);
	Request.Record.bWasDestroyed = false;
	Request.Record.LastTransitionGameTime = 100.0;
	Request.Record.Transform = LastLeft;
	Request.Record.Generation = 3;   // it has existed before, so "last left" is meaningful

	FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);
	TestTrue(TEXT("AsLastLeft spawns"), Decision.Action == ESpawnAction::Spawn);
	TestTrue(TEXT("AsLastLeft puts the car back where it was left"),
		Decision.DesiredTransform.GetLocation().Equals(LastLeft.GetLocation(), 0.01));

	// The control: Generation 0 means this slot has never produced anything, so the stored transform is
	// not a memory of the car but leftover data. Falling back to the pad is the only correct answer, and
	// without this half the first assertion would pass for the wrong reason.
	Request.Record.Generation = 0;
	Decision = Policy->EvaluateDecision(Request);
	TestTrue(TEXT("Generation 0 falls back to the pad"),
		Decision.DesiredTransform.GetLocation().Equals(Request.AnchorTransform.GetLocation(), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehiclePersonalCarWreckTest,
	"TerritoryFramework.Spawn.Vehicle.PersonalCarWreckReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehiclePersonalCarWreckTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UTerritorySpawnPolicyPersonalVehicle* const Policy = NewObject<UTerritorySpawnPolicyPersonalVehicle>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	Policy->ReturnGraceSeconds = 0.f;
	Policy->bReplaceVehicleWhenWrecked = true;
	Policy->WreckReplacementDelaySeconds = 20.f;

	// A wrecked car is not a car that was put away. It uses the replacement delay, not the return grace,
	// and before that delay expires the player must not be handed a fresh car at the wreck site.
	FTerritorySpawnRequest Request = MakeEmptyRequest(105.0);
	Request.Record.bWasDestroyed = true;
	Request.Record.LastTransitionGameTime = 100.0;

	TestTrue(TEXT("A fresh wreck does not produce a replacement"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::None);

	Request.NowGameTime = 121.0;
	TestTrue(TEXT("After the replacement delay a new car appears"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::Spawn);

	// Permanent wreck: the author turned replacement off, so the slot stays empty forever. This is a
	// distinct authored policy, not a delay, and it must not degrade into "spawn anyway" over time.
	Policy->bReplaceVehicleWhenWrecked = false;
	Request.NowGameTime = 100000.0;
	TestTrue(TEXT("A permanent wreck never comes back"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::None);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Ambulance policy
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehicleAmbulanceFirstSpawnTest,
	"TerritoryFramework.Spawn.Vehicle.AmbulanceSpawnsWithoutGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehicleAmbulanceFirstSpawnTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UTerritorySpawnPolicyAmbulance* const Policy = NewObject<UTerritorySpawnPolicyAmbulance>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	Policy->ReplacementDelaySeconds = 45.f;

	// A district that loads must have its ambulance immediately. The 45 s delay is a consequence of a
	// wreck, not a startup cost; applying it here would leave a hospital forecourt empty for almost a
	// minute after every map load.
	FTerritorySpawnRequest Request = MakeEmptyRequest(0.0);
	TestTrue(TEXT("The first ambulance spawns at once"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::Spawn);

	// A replacement after a wreck does wait.
	Request.NowGameTime = 10.0;
	Request.Record.bWasDestroyed = true;
	Request.Record.LastTransitionGameTime = 0.0;
	TestTrue(TEXT("A replacement waits out the delay"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::None);

	Request.NowGameTime = 50.0;
	TestTrue(TEXT("After the delay the ambulance is rebuilt"),
		Policy->EvaluateDecision(Request).Action == ESpawnAction::Spawn);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehicleAmbulanceLeaseTest,
	"TerritoryFramework.Spawn.Vehicle.AmbulanceLeaseGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehicleAmbulanceLeaseTest::RunTest(const FString& Parameters)
{
	UTerritorySpawnPolicyAmbulance* const Policy = NewObject<UTerritorySpawnPolicyAmbulance>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;

	// Default allows mission leases. This is the whole reason the bay exists.
	TestTrue(TEXT("An ambulance accepts a mission lease by default"),
		Policy->AllowsLease(TerritorySpawnTags::LeaseMission));

	// Turned off, it refuses every pin, including the interaction one. A bay that forbids leases is
	// really saying "nothing depends on this slot", which is what the comment in the policy claims.
	Policy->bAllowMissionLeases = false;
	TestFalse(TEXT("A no-lease ambulance refuses even an interaction pin"),
		Policy->AllowsLease(TerritorySpawnTags::LeaseInteraction));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Ambulance reclaim
// ═══════════════════════════════════════════════════════════════════════════════
//
// The case the whole spawn system exists for: a player drives the district's ambulance across the
// city, parks it outside a shop, and walks away. Without a reclaim the bay believes it still holds a
// vehicle forever, so the next call for help finds an empty forecourt. Every one of the four halves
// below is a way that reclaim could go wrong, and the two vetoes are the ones that would hurt a player.

namespace TerritorySpawnVehicleTests
{
	UWorld* MakeReclaimWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			World->CreateAISystem();
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		return World;
	}

	void TeardownReclaimWorld(UWorld* World)
	{
		if (World)
		{
			World->BeginTearingDown();
			World->DestroyWorld(false);
		}
	}

	/** A request for "the bay claims a vehicle, and that vehicle is somewhere else". */
	FTerritorySpawnRequest MakeAbandonedRequest(const FVector& EntityLocation, double NowGameTime,
		double LastTransitionGameTime)
	{
		FTerritorySpawnRequest Request = MakeEmptyRequest(NowGameTime);
		Request.CurrentState = ESpawnLifecycleState::Live;
		Request.bHasEntity = true;
		Request.EntityTransform = FTransform(FRotator::ZeroRotator, EntityLocation);
		Request.Record.bHasInstance = true;
		Request.Record.Generation = 1;
		Request.Record.Transform = Request.EntityTransform;
		Request.Record.LastTransitionGameTime = LastTransitionGameTime;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehicleAmbulanceReclaimTest,
	"TerritoryFramework.Spawn.Vehicle.AmbulanceReclaimsAbandonedVehicle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehicleAmbulanceReclaimTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UWorld* const World = MakeReclaimWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the reclaim test world."));
		return false;
	}

	// The policy is outered to the world so GetWorld() resolves and IsAnyPlayerNearby can walk the
	// controller list. An instanced policy on a real anchor would reach the same world through its outer.
	UTerritorySpawnPolicyAmbulance* const Policy = NewObject<UTerritorySpawnPolicyAmbulance>(World);
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	Policy->bPersistDamage = false;
	Policy->ReplacementDelaySeconds = 45.f;
	Policy->bReclaimWhenAbandoned = true;
	Policy->ReclaimDistanceCm = 1000.f;
	Policy->ReclaimSafeRadiusCm = 2500.f;
	Policy->ReclaimDelaySeconds = 60.f;

	const FVector BayLocation = MakeEmptyRequest(0.0).AnchorTransform.GetLocation();
	const FVector FarLocation = BayLocation + FVector(20000.0, 0.0, 0.0);

	// ─── Parked on its bay: nothing to fix ───
	// Retiring a vehicle that is already where the reclaim would put it is a destroy-and-respawn with
	// nothing to show for it.
	{
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(BayLocation, 1000.0, 0.0);
		TestTrue(TEXT("An ambulance parked on its bay is left alone"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);
	}

	// ─── Far away, delay elapsed, nobody around: reclaim ───
	{
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 1000.0, 900.0);
		const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);
		TestTrue(TEXT("An abandoned ambulance is reclaimed"),
			Decision.Action == ESpawnAction::Retire);
		TestTrue(TEXT("The reclaim asks for the authored retire mode"),
			Decision.RetireMode == Policy->RetireMode);
		TestTrue(TEXT("The reclaim carries its own settle time to the framework"),
			FMath::IsNearlyEqual(Decision.MinimumLiveSeconds, Policy->ReclaimDelaySeconds));
	}

	// ─── Far away, but the settle delay has not elapsed ───
	{
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 930.0, 900.0);
		TestTrue(TEXT("A just-abandoned ambulance gets its settle time"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);
	}

	// ─── The feature switched off ───
	{
		Policy->bReclaimWhenAbandoned = false;
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 100000.0, 0.0);
		TestTrue(TEXT("A disabled reclaim never retires anything"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);
		Policy->bReclaimWhenAbandoned = true;
	}

	// ─── A player standing beside the ambulance vetoes the reclaim ───
	// This is the half that protects the player, and it cannot be checked with a request alone: the
	// policy walks the world's controller list, so the veto only exists if a real controller with a real
	// pawn is in that list.
	{
		APlayerController* const PC = World->SpawnActor<APlayerController>();
		ATerritorySpawnTestPawn* const Pawn = World->SpawnActor<ATerritorySpawnTestPawn>();
		if (!PC || !Pawn)
		{
			AddError(TEXT("Could not build the nearby-player fixture."));
			TeardownReclaimWorld(World);
			return false;
		}

		Pawn->SetActorLocation(FarLocation + FVector(200.0, 0.0, 0.0));
		PC->Possess(Pawn);

		// Possess links the two halves but does NOT put the controller in the world's controller list.
		// IsAnyPlayerNearby walks World->GetPlayerControllerIterator(), so without this the policy sees
		// an empty city and the veto below would appear to work only because it never ran.
		World->AddController(PC);

		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 1000.0, 900.0);
		TestTrue(TEXT("A player beside the ambulance stops the reclaim"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);

		// The control: the SAME request reclaims once the player is out of range. Without this half the
		// test would pass just as well if the reclaim never worked at all.
		Pawn->SetActorLocation(FarLocation + FVector(5000.0, 0.0, 0.0));
		TestTrue(TEXT("The reclaim proceeds once the player walks away"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::Retire);
	}

	TeardownReclaimWorld(World);
	return true;
}

// Abandoning a personal car is opt-in per bay, unlike the ambulance where the reclaim is on by
// default. The reason is that both behaviours are defensible — a car the player parked is a car they
// are coming back to — so the opt-in itself needs proof, not just the path behind it. The cases below
// are the ways the opt-in could go wrong, and the last two are the ones that would hurt a player: a
// car removed while its owner is standing beside it, or while a second player is.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehiclePersonalCarAbandonTest,
	"TerritoryFramework.Spawn.Vehicle.PersonalCarAbandonVeto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehiclePersonalCarAbandonTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UWorld* const World = MakeReclaimWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the abandon test world."));
		return false;
	}

	// The policy is outered to a real anchor, not to the world, because the abandon decision asks the
	// anchor who claimed the bay. A world-outered policy would answer "nobody" and every case below
	// would pass by never reaching the distance check.
	ATerritorySpawnAnchor* const Anchor = World->SpawnActor<ATerritorySpawnAnchor>();
	if (!Anchor)
	{
		AddError(TEXT("Could not spawn the bay anchor."));
		TeardownReclaimWorld(World);
		return false;
	}

	UTerritorySpawnPolicyPersonalVehicle* const Policy =
		NewObject<UTerritorySpawnPolicyPersonalVehicle>(Anchor);
	Policy->EntityTag = TerritorySpawnTags::EntityActor;
	// SetPolicyForTesting re-resolves the anchor's layers, and resolution demands a class the executor
	// will accept. The abstract base passes that check (SupportsActorClass only asks IsChildOf) while
	// remaining unspawnable, which is exactly what this decision-only test wants: nothing is created, so
	// no spawn-side log can fail the run for a reason that has nothing to do with the veto.
	Policy->VehicleClass = TSoftClassPtr<AActor>(ANarrativeVehicleBase::StaticClass());
	Policy->bDespawnWhenAbandoned = true;
	Policy->AbandonDistanceCm = 6000.f;
	Policy->AbandonSafeRadiusCm = 2500.f;

	// Deliberately NO DispatchBeginPlay, and therefore no spawn.
	//
	// This test measures a decision function: "would this policy ask for a retire, and why not". Beginning
	// play would register the anchor with the world subsystem, whose sweep would then try to create the
	// configured entity through a VEHICLE executor — and that executor correctly refuses a non-vehicle
	// class and logs an Error. An unexpected Error fails an automation test, so the run would report a
	// failure that has nothing to do with the veto under test. The spawn path has its own coverage.
	//
	// GetOwningAnchor still resolves: it walks the outer chain, and the policy was outered to the anchor
	// above. ClaimedByPlayerName is read from the anchor, which is all the abandon decision needs.
	Anchor->SetPolicyForTesting(Policy);

	const FVector BayLocation = MakeEmptyRequest(0.0).AnchorTransform.GetLocation();
	const FVector FarLocation = BayLocation + FVector(20000.0, 0.0, 0.0);

	// ─── Nobody has claimed the bay ───
	// There is no "the owner walked away" to measure, so the only honest answer is to leave the car
	// where it is. Guessing here would delete a car on a bay that was never assigned.
	{
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 100000.0, 0.0);
		TestTrue(TEXT("An unclaimed bay never despawns its car"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);
	}

	// The bay now belongs to Alice.
	TestTrue(TEXT("The bay accepts its configured owner"),
		Anchor->BindOwnerKeyToPlayer(TEXT("Alice")));

	APlayerController* const OwnerPC = World->SpawnActor<APlayerController>();
	ATerritorySpawnTestPawn* const OwnerPawn = World->SpawnActor<ATerritorySpawnTestPawn>();
	if (!OwnerPC || !OwnerPawn)
	{
		AddError(TEXT("Could not build the owner fixture."));
		TeardownReclaimWorld(World);
		return false;
	}

	APlayerState* const OwnerState = World->SpawnActor<APlayerState>();
	OwnerState->SetPlayerName(TEXT("Alice"));
	OwnerPC->SetPlayerState(OwnerState);
	OwnerPC->Possess(OwnerPawn);
	World->AddController(OwnerPC);

	// ─── The owner is beside the car ───
	{
		OwnerPawn->SetActorLocation(FarLocation + FVector(300.0, 0.0, 0.0));
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 100000.0, 0.0);
		TestTrue(TEXT("A car with its owner beside it is not abandoned"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);
	}

	// ─── The owner walked away, nobody else is around: the opt-in acts ───
	{
		OwnerPawn->SetActorLocation(FarLocation + FVector(20000.0, 0.0, 0.0));
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 100000.0, 0.0);
		const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);
		TestTrue(TEXT("An abandoned car is despawned when the bay opts in"),
			Decision.Action == ESpawnAction::Retire);
		TestTrue(TEXT("The despawn uses the authored retire mode"),
			Decision.RetireMode == Policy->AbandonRetireMode);
	}

	// ─── The owner is far, but a second player is standing beside the car ───
	// This is the case the framework's own interlock cannot catch: it asks whether somebody is INSIDE
	// the vehicle, and a bystander answers no. Without this veto the car disappears in their face.
	{
		APlayerController* const OtherPC = World->SpawnActor<APlayerController>();
		ATerritorySpawnTestPawn* const OtherPawn = World->SpawnActor<ATerritorySpawnTestPawn>();
		if (!OtherPC || !OtherPawn)
		{
			AddError(TEXT("Could not build the bystander fixture."));
			TeardownReclaimWorld(World);
			return false;
		}

		APlayerState* const OtherState = World->SpawnActor<APlayerState>();
		OtherState->SetPlayerName(TEXT("Bob"));
		OtherPC->SetPlayerState(OtherState);
		OtherPC->Possess(OtherPawn);
		World->AddController(OtherPC);

		OtherPawn->SetActorLocation(FarLocation + FVector(200.0, 0.0, 0.0));
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 100000.0, 0.0);
		TestTrue(TEXT("A bystander beside the car stops the despawn"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);

		// The control: the same request acts once the bystander also walks away. Without this half the
		// veto above could be passing because the distance check never ran.
		OtherPawn->SetActorLocation(FarLocation + FVector(20000.0, 0.0, 0.0));
		TestTrue(TEXT("The despawn proceeds once nobody is near the car"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::Retire);
	}

	// ─── The feature switched off: the default ───
	{
		Policy->bDespawnWhenAbandoned = false;
		const FTerritorySpawnRequest Request = MakeAbandonedRequest(FarLocation, 100000.0, 0.0);
		TestTrue(TEXT("A bay that did not opt in never despawns its car"),
			Policy->EvaluateDecision(Request).Action == ESpawnAction::None);
		Policy->bDespawnWhenAbandoned = true;
	}

	TeardownReclaimWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// General vehicle policy — the condition switch that used to be dead
// ═══════════════════════════════════════════════════════════════════════════════
//
// PIE proved the defect: bPersistDamage defaulted to false, was never set, and damage still came back
// across a rebuild — because the switch lived on the policy while the decision lived on the executor,
// and the executor's own bRestoreSavedHealth always said yes. A switch nothing reads is worse than no
// switch, because a designer ticks it and believes the game changed.
//
// These halves assert the DECISION the framework now obeys, which is where the switch has to be true
// or false. Whether the executor then honours it is the executor contract below.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehicleConditionSwitchTest,
	"TerritoryFramework.Spawn.Vehicle.ConditionSwitchReachesDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehicleConditionSwitchTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	UTerritorySpawnPolicyVehicle* const Policy = NewObject<UTerritorySpawnPolicyVehicle>();
	Policy->EntityTag = TerritorySpawnTags::EntityActor;

	// A rebuild after destruction, at a time past the replacement delay so the answer is actually Spawn.
	FTerritorySpawnRequest Request = MakeEmptyRequest(1000.0);
	Request.Record.bWasDestroyed = true;
	Request.Record.LastTransitionGameTime = 0.0;
	Request.Record.Generation = 2;

	// ─── Off: the depot serviced it ───
	// This is the default, and the default is a design decision: a player who wrecks the district's one
	// vehicle and then needs a second job expects a working vehicle, not the same wreck.
	{
		Policy->bPersistDamage = false;
		const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);
		TestTrue(TEXT("A destroyed vehicle is rebuilt once the delay elapses"),
			Decision.Action == ESpawnAction::Spawn);
		TestFalse(TEXT("By default a rebuild does NOT restore the captured damage"),
			Decision.bRestoreCheckpoint);
	}

	// ─── On: damage is a lasting consequence ───
	// The control half. Without it, the assertion above would pass just as well if the flag were
	// ignored entirely — which is exactly the bug PIE found.
	{
		Policy->bPersistDamage = true;
		const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);
		TestTrue(TEXT("A destroyed vehicle is still rebuilt"), Decision.Action == ESpawnAction::Spawn);
		TestTrue(TEXT("With Persist Damage on, the rebuild restores the captured damage"),
			Decision.bRestoreCheckpoint);
	}

	// ─── A first spawn has no damage to restore either way ───
	// The flag must not be consulted for a slot that has never held a vehicle: there is no checkpoint,
	// so restoring is a no-op and claiming otherwise would mislead the next reader of this code.
	{
		Policy->bPersistDamage = true;
		const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(MakeEmptyRequest(10.0));
		TestTrue(TEXT("A brand-new bay spawns immediately"), Decision.Action == ESpawnAction::Spawn);
		TestTrue(TEXT("A first spawn is allowed to restore, and the absent checkpoint simply does nothing"),
			Decision.bRestoreCheckpoint);
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// General vehicle policy — it is one rule, not one class per noun
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehiclePolicyGeneralityTest,
	"TerritoryFramework.Spawn.Vehicle.PolicyIsGeneralNotAmbulanceSpecific",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehiclePolicyGeneralityTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnVehicleTests;

	// The general policy manages any vehicle. A fire engine, a taxi and a police cruiser are the same
	// rule with different numbers, so the class may not encode the noun anywhere.
	UTerritorySpawnPolicyVehicle* const General = NewObject<UTerritorySpawnPolicyVehicle>();
	General->EntityTag = TerritorySpawnTags::EntityActor;

	// Identity and executor come from the general class, not from a subclass that names a vehicle type.
	TestTrue(TEXT("The general policy reports its configured entity tag"),
		General->GetEntityTag().MatchesTagExact(TerritorySpawnTags::EntityActor));
	TestTrue(TEXT("The general policy hands out the vehicle executor"),
		General->GetExecutorClass().Get() == UTerritorySpawnExecutorVehicle::StaticClass());

	// Reclaim is default behaviour, not an ambulance feature: any managed bay that loses its vehicle to
	// a joyride must be able to get it back, or the next consumer finds an empty pad.
	TestTrue(TEXT("Reclaim is on by default for every managed vehicle bay"),
		General->bReclaimWhenAbandoned);
	TestTrue(TEXT("A managed bay accepts mission leases by default"), General->bAllowMissionLeases);

	// The ambulance is now only a PRESET over this class. Its defaults must match the general ones, or
	// two classes whose numbers agree are one class plus a name — and a future change to the general
	// default would silently re-tune every hospital in the city.
	UTerritorySpawnPolicyAmbulance* const Ambulance = NewObject<UTerritorySpawnPolicyAmbulance>();
	TestEqual(TEXT("The ambulance preset inherits the general reclaim distance"),
		Ambulance->ReclaimDistanceCm, General->ReclaimDistanceCm);
	TestEqual(TEXT("The ambulance preset inherits the general safe radius"),
		Ambulance->ReclaimSafeRadiusCm, General->ReclaimSafeRadiusCm);
	TestEqual(TEXT("The ambulance preset inherits the general replacement delay"),
		Ambulance->ReplacementDelaySeconds, General->ReplacementDelaySeconds);
	TestEqual(TEXT("The ambulance preset inherits the general settle delay"),
		Ambulance->ReclaimDelaySeconds, General->ReclaimDelaySeconds);

	// The damage policy is a bool, and TestEqual's bool overload is ambiguous against int32 in this
	// harness, so it is compared as text.
	TestTrue(TEXT("The ambulance preset inherits the general damage policy"),
		Ambulance->bPersistDamage == General->bPersistDamage);

	// Both are still the same MECHANISM. If a future edit gives the ambulance its own decision path, this
	// assertion catches it before it becomes a second implementation of one rule.
	TestTrue(TEXT("The ambulance preset is a general vehicle policy, not a parallel implementation"),
		Ambulance->IsA<UTerritorySpawnPolicyVehicle>());

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Executor contract
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehicleExecutorContractTest,
	"TerritoryFramework.Spawn.Vehicle.ExecutorContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehicleExecutorContractTest::RunTest(const FString& Parameters)
{
	UTerritorySpawnExecutorVehicle* const Executor = NewObject<UTerritorySpawnExecutorVehicle>();

	// The executor refuses anything that is not a Narrative vehicle. This is the guard that stops a
	// mis-authored policy from putting a tree in a car bay and then restoring vehicle health on it.
	TestFalse(TEXT("A plain actor is not a vehicle"),
		Executor->SupportsActorClass(AActor::StaticClass()));

	TestFalse(TEXT("A player character is not a vehicle"),
		Executor->SupportsActorClass(APawn::StaticClass()));

	// Both Narrative vehicle bases ARE vehicles, even though they are abstract: the check is against the
	// class hierarchy, not against instantiability.
	UClass* const VehicleBase = FindObject<UClass>(nullptr,
		TEXT("/Script/NarrativeArsenal.NarrativeVehicleBase"));
	if (VehicleBase)
	{
		TestTrue(TEXT("ANarrativeVehicleBase is accepted"), Executor->SupportsActorClass(VehicleBase));
	}
	else
	{
		AddWarning(TEXT("NarrativeVehicleBase was not found; the positive half of this contract is unproven."));
	}

	// Nothing to protect: an invalid actor must not be reported as externally owned, or every retire is
	// vetoed forever. This is the exact defect the interlock tests caught in the actor executor, and the
	// vehicle executor inherits that behaviour, so it is asserted here too rather than assumed.
	TestFalse(TEXT("A null actor is never claimed"),
		Executor->IsClaimedByExternalOwner(nullptr));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Adoption of a hand-placed vehicle
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * A framework vehicle must behave exactly like one dragged out of the Content Browser.
 *
 * This is the regression test for the defect that cost the most time in this whole feature. The spawn
 * pipeline used to snap a vehicle to the ground, hide it while its physics settled, force physics on so
 * the settle could be measured, apply a parking brake, and override its replication rate. Each step had
 * a measurement behind it; together they produced a car that could not be driven the way the level's own
 * ambulance could, and the user said so plainly.
 *
 * The test pins the ABSENCE of those steps, which is what a regression test for a deleted feature looks
 * like. If someone reintroduces a snap or a settle watcher into the vehicle executor, this fails.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnVehicleMinimalExecutorTest,
	"TerritoryFramework.Spawn.Vehicle.ExecutorDoesNotMeddleWithTheVehicle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnVehicleMinimalExecutorTest::RunTest(const FString& Parameters)
{
	UTerritorySpawnExecutorVehicle* const Executor = NewObject<UTerritorySpawnExecutorVehicle>();

	// Only the base placement flags exist on the executor now. The vehicle executor must leave navigation
	// projection alone: a vehicle is spawned at the authored transform and never moved by the framework.
	// The snap and settle fields that used to live beside this one are gone, and their absence is the fix.
	TestFalse(TEXT("A vehicle is not projected onto navigation by the framework"),
		Executor->bProjectToNavigation);

	TestTrue(TEXT("The vehicle executor refuses a non-vehicle class"),
		!Executor->SupportsActorClass(ANarrativeNPCCharacter::StaticClass()));
	TestTrue(TEXT("The vehicle executor accepts the Narrative vehicle base"),
		Executor->SupportsActorClass(ANarrativeVehicleBase::StaticClass()));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
