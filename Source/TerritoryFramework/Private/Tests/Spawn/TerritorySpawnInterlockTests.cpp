#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnExecutorActor.h"
#include "Spawn/TerritorySpawnPolicyStatic.h"
#include "Spawn/TerritorySpawnSubsystem.h"
#include "Spawn/TerritorySpawnTags.h"
#include "Tests/Spawn/TerritorySpawnTestFixtures.h"

/**
 * B3: the safety interlock and leases.
 *
 * The property under test is ASYMMETRIC and that is the whole point: a policy may ask for a retire,
 * and the framework may refuse. It can never do the reverse. Every case below has a policy that WANTS
 * the entity gone and a framework that must not allow it, so a passing run proves the veto is real
 * rather than that the fixture cooperated.
 *
 * A lease that can be granted but never expires would be a permanently dead slot, so expiry has its
 * own case: the only honest way to test "the lease will not pin this forever" is to grant one with a
 * short lifetime and observe the slot act again afterwards.
 */
namespace TerritorySpawnInterlockTests
{
	UWorld* MakeWorld()
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

	void TeardownWorld(UWorld* World)
	{
		if (World)
		{
			World->BeginTearingDown();
			World->DestroyWorld(false);
		}
	}

	int32 CountSpawnedEntities(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ATargetPoint> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	void InitializeAnchor(ATerritorySpawnAnchor* Anchor)
	{
		if (!Anchor) return;
		Anchor->EnsurePersistentAnchorGUID();
		Anchor->DispatchBeginPlay();
	}

	/** Build an anchor whose policy will demand a retire on every sweep. */
	ATerritorySpawnAnchor* MakeRetireDemandingAnchor(UWorld* World)
	{
		ATerritorySpawnAnchor* Anchor = World->SpawnActor<ATerritorySpawnAnchor>();
		if (!Anchor) return nullptr;

		InitializeAnchor(Anchor);

		UTerritorySpawnTestFixturePolicy* Policy = NewObject<UTerritorySpawnTestFixturePolicy>(Anchor);
		Policy->EntityTag = TerritorySpawnTags::EntityActor;
		Policy->ActorClass = ATargetPoint::StaticClass();
		Policy->RecoveryMode = ESpawnPolicyRecoveryMode::InitialSpawn;
		Policy->bRequestRetireInTest = true;
		Anchor->SetPolicyForTesting(Policy);

		return Anchor;
	}

	UTerritorySpawnTestFixturePolicy* GetFixturePolicy(ATerritorySpawnAnchor* Anchor)
	{
		return Anchor ? Cast<UTerritorySpawnTestFixturePolicy>(Anchor->GetPolicy()) : nullptr;
	}

	/**
	 * Bring an anchor to Live with exactly one entity, and confirm the spawn itself was not vetoed.
	 * Every interlock case starts here so the retire has something real to protect.
	 */
	bool SpawnOneEntity(ATerritorySpawnAnchor* Anchor, UWorld* World, FAutomationTestBase& Test)
	{
		UTerritorySpawnTestFixturePolicy* Policy = GetFixturePolicy(Anchor);
		if (!Policy) return false;

		// The first evaluation must spawn, so the retire demand is switched off until Live.
		Policy->bRequestRetireInTest = false;
		UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
		if (!Subsystem) return false;
		Subsystem->RequestImmediateEvaluation(Anchor);

		const bool bLive = Anchor->GetLifecycleState() == ESpawnLifecycleState::Live
			&& Anchor->GetSpawnedActor() != nullptr
			&& CountSpawnedEntities(World) == 1;
		Test.TestTrue(TEXT("Fixture reached Live with exactly one entity before the veto case"), bLive);

		Policy->bRequestRetireInTest = true;
		return bLive;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// 1. A live lease vetoes a retire that the policy demanded
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnLeaseVetoesRetire,
	"TerritoryFramework.Spawn.Interlock.LiveLeaseVetoesRetire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnLeaseVetoesRetire::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnInterlockTests;

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	ATerritorySpawnAnchor* Anchor = MakeRetireDemandingAnchor(World);
	UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Anchor || !Subsystem)
	{
		AddError(TEXT("Anchor or spawn subsystem missing."));
		TeardownWorld(World);
		return false;
	}

	if (!SpawnOneEntity(Anchor, World, *this))
	{
		TeardownWorld(World);
		return false;
	}

	AActor* const Entity = Anchor->GetSpawnedActor();

	// A mission borrows the slot. From this moment the bay's own policy may want it gone, and must
	// lose: the objective holding the ambulance outranks the restock rule.
	UObject* Borrower = NewObject<UTerritorySpawnTestLeaseHolder>(World);
	Subsystem->AcquireLease(Anchor->GetAnchorGuid(), Borrower, TerritorySpawnTags::LeaseMission, 600.f);
	TestTrue(TEXT("The lease is live immediately after it is granted"),
		Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), World->GetTimeSeconds()));

	Subsystem->RequestImmediateEvaluation(Anchor);

	TestEqual(TEXT("A leased slot is still Live after the policy demands a retire"),
		static_cast<int32>(Anchor->GetLifecycleState()), static_cast<int32>(ESpawnLifecycleState::Live));
	TestEqual(TEXT("The entity survived the vetoed retire"), Anchor->GetSpawnedActor(), Entity);
	TestEqual(TEXT("Exactly one entity still exists"), CountSpawnedEntities(World), 1);
	TestTrue(TEXT("The record still reports an instance"), Anchor->GetRecord().bHasInstance);

	// Control: release the lease and the same demand must now succeed. Without this half the test
	// would pass even if the policy were broken and never asked for a retire at all.
	//
	// Released by ANCHOR, not by lease id, because that is the intent the caller actually has: "unpin
	// this slot". A caller forced to carry the id would leak the pin on any early-out.
	Subsystem->ReleaseLeasesForAnchor(Anchor->GetAnchorGuid());
	TestFalse(TEXT("The lease is gone after release"),
		Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), World->GetTimeSeconds()));

	Subsystem->RequestImmediateEvaluation(Anchor);

	TestEqual(TEXT("With no lease the same demand is honoured"),
		static_cast<int32>(Anchor->GetLifecycleState()), static_cast<int32>(ESpawnLifecycleState::Dormant));
	TestNull(TEXT("The entity is gone once the retire is allowed"), Anchor->GetSpawnedActor());
	TestEqual(TEXT("No entity remains after the allowed retire"), CountSpawnedEntities(World), 0);

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 2. An external owner vetoes a retire the policy demanded
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnExternalOwnerVetoesRetire,
	"TerritoryFramework.Spawn.Interlock.ExternalOwnerVetoesRetire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnExternalOwnerVetoesRetire::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnInterlockTests;

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	ATerritorySpawnAnchor* Anchor = MakeRetireDemandingAnchor(World);
	UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Anchor || !Subsystem)
	{
		AddError(TEXT("Anchor or spawn subsystem missing."));
		TeardownWorld(World);
		return false;
	}

	if (!SpawnOneEntity(Anchor, World, *this))
	{
		TeardownWorld(World);
		return false;
	}

	// This is the case the whole interlock exists for: a player is driving the car. The executor
	// answers it by looking at the pawn's controller, so the test uses a REAL pawn and a REAL
	// controller — a mock that agreed with the code would prove nothing.
	//
	// The slot is repointed at the pawn class first, because the claim predicate is evaluated against
	// whatever the anchor actually holds. Without the repoint the test would be asserting about the
	// target point, which is a different (and correctly unclaimed) case.
	UTerritorySpawnTestFixturePolicy* Policy = GetFixturePolicy(Anchor);
	if (!Policy)
	{
		AddError(TEXT("Fixture policy missing."));
		TeardownWorld(World);
		return false;
	}

	// Tear the existing entity down first so the next evaluation spawns the pawn class instead. The
	// retire marks the record as destroyed, and a static policy in InitialSpawn mode correctly refuses
	// to bring a one-shot back — so the mode has to change with the class, otherwise the setup would be
	// testing the one-shot rule instead of the possession veto.
	Policy->bRequestRetireInTest = true;
	Subsystem->RequestImmediateEvaluation(Anchor);
	TestEqual(TEXT("Setup: the previous entity was retired"), CountSpawnedEntities(World), 0);

	Policy->ActorClass = ATerritorySpawnTestPawn::StaticClass();
	Policy->RecoveryMode = ESpawnPolicyRecoveryMode::RebuildLost;
	Policy->RespawnDelaySeconds = 0.f;
	Policy->bRequestRetireInTest = false;
	Subsystem->RequestImmediateEvaluation(Anchor);

	APawn* const SpawnedPawn = Cast<APawn>(Anchor->GetSpawnedActor());
	if (!SpawnedPawn)
	{
		AddError(TEXT("The anchor did not spawn the pawn class."));
		TeardownWorld(World);
		return false;
	}

	APlayerController* const DriverController = World->SpawnActor<APlayerController>();
	if (!DriverController)
	{
		AddError(TEXT("Could not spawn the possession controller."));
		TeardownWorld(World);
		return false;
	}
	// Possess() rather than Pawn->PossessedBy(): PossessedBy sets only the pawn's Controller pointer and
	// leaves the controller's Pawn pointer null, so a later UnPossess() is a no-op and the pawn stays
	// "driven" forever. Possess() links both halves, which is what the control case below depends on.
	DriverController->Possess(SpawnedPawn);

	TestTrue(TEXT("The executor treats a possessed pawn as externally claimed"),
		Anchor->GetExecutor() && Anchor->GetExecutor()->IsClaimedByExternalOwner(SpawnedPawn));

	// Now the real assertion: a policy that WANTS the driven pawn gone must lose.
	Policy->bRequestRetireInTest = true;
	const int32 BeforeVeto = CountSpawnedEntities(World);
	Subsystem->RequestImmediateEvaluation(Anchor);

	TestEqual(TEXT("A possessed pawn keeps the slot Live despite the retire demand"),
		static_cast<int32>(Anchor->GetLifecycleState()), static_cast<int32>(ESpawnLifecycleState::Live));
	// Both sides are cast to AActor*: TestEqual cannot resolve a comparison between APawn* and
	// AActor*, and the identity being asserted is the actor, not the pawn-ness.
	TestEqual(TEXT("The driven pawn survived the vetoed retire"),
		Anchor->GetSpawnedActor(), static_cast<AActor*>(SpawnedPawn));
	TestEqual(TEXT("No entity was removed while the pawn was driven"),
		CountSpawnedEntities(World), BeforeVeto);

	// Control: unpossess and the identical demand now goes through, which proves the veto measured
	// possession and not something incidental about the class.
	DriverController->UnPossess();
	Subsystem->RequestImmediateEvaluation(Anchor);

	TestEqual(TEXT("With nobody driving the same demand is honoured"),
		static_cast<int32>(Anchor->GetLifecycleState()), static_cast<int32>(ESpawnLifecycleState::Dormant));

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 3. The authored grace period vetoes a retire
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnGraceVetoesRetire,
	"TerritoryFramework.Spawn.Interlock.MinimumLiveSecondsVetoesRetire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnGraceVetoesRetire::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnInterlockTests;

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	ATerritorySpawnAnchor* Anchor = MakeRetireDemandingAnchor(World);
	UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	UTerritorySpawnTestFixturePolicy* Policy = GetFixturePolicy(Anchor);
	if (!Anchor || !Subsystem || !Policy)
	{
		AddError(TEXT("Anchor, subsystem or fixture policy missing."));
		TeardownWorld(World);
		return false;
	}

	if (!SpawnOneEntity(Anchor, World, *this))
	{
		TeardownWorld(World);
		return false;
	}

	// A generous grace makes the veto deterministic: the entity was created microseconds ago, so any
	// positive grace must still be in force. The framework owns the RULE that a grace exists; the
	// policy owns only the number, and this number is 60 seconds.
	Policy->MinimumLiveSecondsInTest = 60.f;

	const int32 BeforeVeto = CountSpawnedEntities(World);
	Subsystem->RequestImmediateEvaluation(Anchor);

	TestEqual(TEXT("A retire inside the grace window leaves the slot Live"),
		static_cast<int32>(Anchor->GetLifecycleState()), static_cast<int32>(ESpawnLifecycleState::Live));
	TestEqual(TEXT("No entity was removed during the grace window"),
		CountSpawnedEntities(World), BeforeVeto);

	// Control: with the grace switched off the same demand goes through, so the previous assertion
	// measured the grace and not an unrelated block.
	Policy->MinimumLiveSecondsInTest = 0.f;
	Subsystem->RequestImmediateEvaluation(Anchor);

	TestEqual(TEXT("With no grace the same demand is honoured"),
		static_cast<int32>(Anchor->GetLifecycleState()), static_cast<int32>(ESpawnLifecycleState::Dormant));

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 4. A short lease expires and the slot becomes actionable again
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnLeaseExpires,
	"TerritoryFramework.Spawn.Interlock.LeaseExpiresAndReleasesTheSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnLeaseExpires::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnInterlockTests;

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	ATerritorySpawnAnchor* Anchor = MakeRetireDemandingAnchor(World);
	UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Anchor || !Subsystem)
	{
		AddError(TEXT("Anchor or spawn subsystem missing."));
		TeardownWorld(World);
		return false;
	}

	if (!SpawnOneEntity(Anchor, World, *this))
	{
		TeardownWorld(World);
		return false;
	}

	UObject* Borrower = NewObject<UTerritorySpawnTestLeaseHolder>(World);
	const double Now = World->GetTimeSeconds();

	// A lease with zero duration must be REFUSED, not silently promoted to a default. A lease that
	// never expires is a permanently dead slot, and "no duration supplied" versus "the default
	// duration" are different intentions — a caller that passed zero by accident has to find out here
	// rather than six hours later when the bay will not release.
	const FGuid ZeroLease = Subsystem->AcquireLease(
		Anchor->GetAnchorGuid(), Borrower, TerritorySpawnTags::LeaseMission, 0.f);
	TestFalse(TEXT("A zero-duration lease is refused outright"), ZeroLease.IsValid());
	TestFalse(TEXT("A zero-duration lease is not live"), Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), Now));

	// A negative duration is the same case.
	const FGuid NegativeLease = Subsystem->AcquireLease(
		Anchor->GetAnchorGuid(), Borrower, TerritorySpawnTags::LeaseMission, -5.f);
	TestFalse(TEXT("A negative-duration lease is refused outright"), NegativeLease.IsValid());
	TestFalse(TEXT("A negative-duration lease is not live"),
		Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), Now));

	// A real lease that has already expired by the clock reads as expired without waiting for a sweep.
	Subsystem->AcquireLease(Anchor->GetAnchorGuid(), Borrower, TerritorySpawnTags::LeaseMission, 1.f);
	TestTrue(TEXT("A one-second lease is live now"), Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), Now));
	TestFalse(TEXT("The same lease reads as expired well after its expiry"),
		Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), Now + 3600.0));

	// Releasing the holder's leases must clear them, which is the disconnect path: a mission that ends
	// without calling ReleaseLease individually must not pin the bay.
	Subsystem->ReleaseLeasesForHolder(Borrower);
	TestFalse(TEXT("Releasing by holder clears the lease"),
		Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), Now));

	// And releasing by anchor clears whatever is left, which is the "this bay is free again" path.
	Subsystem->AcquireLease(Anchor->GetAnchorGuid(), Borrower, TerritorySpawnTags::LeaseMission, 600.f);
	TestEqual(TEXT("Releasing by anchor clears exactly one lease"),
		Subsystem->ReleaseLeasesForAnchor(Anchor->GetAnchorGuid()), 1);
	TestFalse(TEXT("The anchor has no live lease afterwards"),
		Subsystem->HasLiveLease(Anchor->GetAnchorGuid(), Now));

	TeardownWorld(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
