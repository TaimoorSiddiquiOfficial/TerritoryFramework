#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryAssetPreload.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnSubsystem.h"
#include "Spawn/TerritorySpawnTags.h"
#include "Tests/Spawn/TerritorySpawnTestFixtures.h"
#include "UObject/Package.h"

/**
 * The known weak spots of the framework, tested rather than documented as doubts.
 *
 * Each case below exists because the code was believed correct without a proof. They are written as
 * tests instead of TODO comments so that a regression in one of them fails the sweep gate rather than
 * being rediscovered by a player.
 */
namespace TerritorySpawnWeaknessTests
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

	void InitializeAnchor(ATerritorySpawnAnchor* Anchor)
	{
		if (!Anchor) return;
		Anchor->EnsurePersistentAnchorGUID();
		Anchor->DispatchBeginPlay();
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

	/** An anchor whose policy spawns once and never asks for a retire, with a caller-chosen tag. */
	ATerritorySpawnAnchor* MakeAnchor(UWorld* World, FGameplayTag EntityTag, FName OwnerKey = FName())
	{
		ATerritorySpawnAnchor* Anchor = World->SpawnActor<ATerritorySpawnAnchor>();
		if (!Anchor) return nullptr;

		InitializeAnchor(Anchor);

		UTerritorySpawnTestFixturePolicy* Policy = NewObject<UTerritorySpawnTestFixturePolicy>(Anchor);
		Policy->EntityTag = EntityTag.IsValid() ? EntityTag : TerritorySpawnTags::EntityActor;
		Policy->ActorClass = ATargetPoint::StaticClass();
		Policy->RecoveryMode = ESpawnPolicyRecoveryMode::InitialSpawn;
		Policy->bRequestRetireInTest = false;
		Anchor->SetPolicyForTesting(Policy);

		if (!OwnerKey.IsNone())
		{
			Anchor->SetOwnerKeyForTesting(OwnerKey);
		}

		return Anchor;
	}

	/**
	 * Register a player so the sweep's ownership reconcile can see a name.
	 *
	 * The controller must be in the world's controller list, not merely spawned: ReconcilePlayerClaims
	 * iterates World->GetPlayerControllerIterator(). A spawned-but-unregistered controller would make a
	 * claim test pass vacuously, because nothing would ever be found to claim — and the "before" half of
	 * every claim test would then be indistinguishable from the "after" half.
	 */
	APlayerController* MakePlayer(UWorld* World, const FString& PlayerName)
	{
		APlayerController* const PC = World->SpawnActor<APlayerController>();
		if (!PC) return nullptr;

		APlayerState* const State = World->SpawnActor<APlayerState>();
		if (!State) return nullptr;

		State->SetPlayerName(PlayerName);
		PC->SetPlayerState(State);
		World->AddController(PC);
		return PC;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// 1. The preload's Failed path
// ═══════════════════════════════════════════════════════════════════════════════
//
// The spawn path treats "Failed" differently from "Pending": a failed class load must NOT consume the
// retry budget, because a missing package is an authoring mistake and a retry counter would turn it
// into a silent retire. That asymmetry was written from reasoning, never observed. This test observes
// it: a class that cannot load leaves the slot PendingSpawn with bWasDestroyed still false, so a slot
// with a mis-typed class sits visibly inert instead of quietly removing itself from the world.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnWeaknessUnloadableClassTest,
	"TerritoryFramework.Spawn.Weakness.UnloadableClassStaysInert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnWeaknessUnloadableClassTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnWeaknessTests;

	UWorld* const World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	ATerritorySpawnAnchor* const Anchor = MakeAnchor(World, TerritorySpawnTags::EntityActor);
	if (!Anchor)
	{
		AddError(TEXT("Could not spawn the anchor."));
		TeardownWorld(World);
		return false;
	}

	UTerritorySpawnTestFixturePolicy* const Policy = Cast<UTerritorySpawnTestFixturePolicy>(Anchor->GetPolicy());
	if (!Policy)
	{
		AddError(TEXT("The fixture policy did not resolve."));
		TeardownWorld(World);
		return false;
	}

	// A path that cannot resolve. Deliberately a well-formed object path to a package that does not
	// exist, not a malformed string: a malformed path is rejected before the preload runs, so it would
	// test the wrong layer.
	Policy->ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/ThisDoesNotExist/BP_Nowhere.BP_Nowhere_C")));

	UTerritorySpawnSubsystem* const Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Subsystem)
	{
		AddError(TEXT("The spawn subsystem was not created."));
		TeardownWorld(World);
		return false;
	}

	// The load is async, so the first passes may legitimately be Pending. The assertion is about what
	// the slot must NEVER become: it must not be retired, and it must not claim a destroyed instance.
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		Subsystem->RequestImmediateEvaluation(Anchor);
	}

	TestEqual(TEXT("Nothing was created"), CountSpawnedEntities(World), 0);
	TestFalse(TEXT("A slot whose class cannot load is never marked destroyed"),
		Anchor->GetRecord().bWasDestroyed);
	TestFalse(TEXT("A slot whose class cannot load holds no instance"),
		Anchor->GetRecord().bHasInstance);
	TestTrue(TEXT("The slot reports a pending spawn rather than a silent give-up"),
		Anchor->GetLifecycleState() == ESpawnLifecycleState::PendingSpawn);

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 2. Player ownership claim
// ═══════════════════════════════════════════════════════════════════════════════
//
// A personal-car bay is addressed by a durable player name, and the framework binds it during the
// sweep. Before this existed, an owner-keyed anchor was never claimed, so bRequireOwnerClaim left every
// personal car permanently dormant. The test runs the sweep's own reconcile, not a shortcut.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnWeaknessPlayerClaimTest,
	"TerritoryFramework.Spawn.Weakness.PlayerClaimBindsOwnerKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnWeaknessPlayerClaimTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnWeaknessTests;

	UWorld* const World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	UTerritorySpawnSubsystem* const Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Subsystem)
	{
		AddError(TEXT("The spawn subsystem was not created."));
		TeardownWorld(World);
		return false;
	}

	ATerritorySpawnAnchor* const Anchor = MakeAnchor(World, TerritorySpawnTags::EntityActor, TEXT("Alice"));
	if (!Anchor)
	{
		AddError(TEXT("Could not spawn the anchor."));
		TeardownWorld(World);
		return false;
	}

	// Before anyone logs in, the bay is unclaimed. This is the "before" half that makes the claim below
	// meaningful; without it a test that only asserts the claimed state cannot tell a working reconcile
	// from one that claims everything unconditionally.
	TestTrue(TEXT("An owner-keyed bay starts unclaimed"),
		Anchor->GetClaimedByPlayerName().IsNone());

	// A different player must not claim it. The bay belongs to Alice by authoring, not to whoever logs in.
	MakePlayer(World, TEXT("Bob"));
	Subsystem->ReconcilePlayerClaimsForTesting();
	TestTrue(TEXT("A different player does not take the bay"),
		Anchor->GetClaimedByPlayerName().IsNone());

	// Alice logs in and the sweep claims the bay for her.
	APlayerController* const Alice = MakePlayer(World, TEXT("Alice"));
	Subsystem->ReconcilePlayerClaimsForTesting();
	TestEqual(TEXT("The owning player claims the bay"),
		Anchor->GetClaimedByPlayerName(), FName(TEXT("Alice")));

	// The bay now serves Alice: one evaluation puts her car in the world. The durable state is produced
	// by the framework's own spawn path rather than forced by the test, so the "state survives" assertion
	// below is about a real record and not a value the test planted.
	Subsystem->RequestImmediateEvaluation(Anchor);
	TestTrue(TEXT("The claimed bay served its entity"), Anchor->GetRecord().bHasInstance);

	// Alice leaves: only the transient claim is dropped, and the record survives so she finds her car
	// where she left it.
	World->RemoveController(Alice);
	Subsystem->ReconcilePlayerClaimsForTesting();
	TestTrue(TEXT("A departed player's claim is released"),
		Anchor->GetClaimedByPlayerName().IsNone());
	TestTrue(TEXT("Releasing a claim keeps the slot's durable state"),
		Anchor->GetRecord().bHasInstance);

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 3. A loaded record is a promise, not a wreck
// ═══════════════════════════════════════════════════════════════════════════════
//
// Load and destruction look identical from the outside: a record with bHasInstance set and nothing in the
// world. They are opposite facts. Load means "an entity was promised and has not been delivered yet";
// destruction means "the entity existed and is gone". Before bAwaitingRestore existed, every policy
// answered None to both, so a restored car was never recreated — and simply treating the record as a lie
// instead turned every restored car into a wreck on the first sweep of every session. This test drives
// each case through the real load path and asserts the two land in opposite places.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnWeaknessRestoreTest,
	"TerritoryFramework.Spawn.Weakness.LoadedRecordRestoresEntity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnWeaknessRestoreTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnWeaknessTests;

	// ─── Half 1: a loaded record is served ───
	{
		UWorld* const World = MakeWorld();
		if (!World)
		{
			AddError(TEXT("Could not create the test world."));
			return false;
		}

		UTerritorySpawnSubsystem* const Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();

		ATerritorySpawnAnchor* const Anchor = MakeAnchor(World, TerritorySpawnTags::EntityActor);
		if (!Subsystem || !Anchor)
		{
			AddError(TEXT("Could not build the loaded-record fixture."));
			TeardownWorld(World);
			return false;
		}

		// A save was written while the prop was alive: the record promises one, nothing is in the world.
		Anchor->SimulateLoadedRecordForTesting(/*bHadInstance=*/true, /*bWasDestroyed=*/false);
		TestTrue(TEXT("A loaded record parks the slot as pending"),
			Anchor->GetLifecycleState() == ESpawnLifecycleState::PendingSpawn);

		Subsystem->RequestImmediateEvaluation(Anchor);

		TestEqual(TEXT("The load promise is honoured by creating the entity"),
			CountSpawnedEntities(World), 1);
		TestTrue(TEXT("The restored slot reaches Live"),
			Anchor->GetLifecycleState() == ESpawnLifecycleState::Live);
		TestFalse(TEXT("A load is never misread as a destruction"),
			Anchor->GetRecord().bWasDestroyed);

		TeardownWorld(World);
	}

	// ─── Half 2: a destroyed record stays destroyed ───
	{
		UWorld* const World = MakeWorld();
		if (!World)
		{
			AddError(TEXT("Could not create the second test world."));
			return false;
		}

		UTerritorySpawnSubsystem* const Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();

		ATerritorySpawnAnchor* const Anchor = MakeAnchor(World, TerritorySpawnTags::EntityActor);
		if (!Subsystem || !Anchor)
		{
			AddError(TEXT("Could not build the destroyed-record fixture."));
			TeardownWorld(World);
			return false;
		}

		// The prop was destroyed and the save recorded that. Load_Implementation parks it Dormant, so the
		// reconcile path must not be what decides its fate.
		Anchor->SimulateLoadedRecordForTesting(/*bHadInstance=*/false, /*bWasDestroyed=*/true);
		TestTrue(TEXT("A record that holds nothing loads as Dormant"),
			Anchor->GetLifecycleState() == ESpawnLifecycleState::Dormant);

		Subsystem->RequestImmediateEvaluation(Anchor);

		TestEqual(TEXT("A one-shot slot does not rebuild what the save says is gone"),
			CountSpawnedEntities(World), 0);
		TestTrue(TEXT("The destruction fact survives the load"),
			Anchor->GetRecord().bWasDestroyed);

		TeardownWorld(World);
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 4. Reconcile preserves the destruction fact
// ═══════════════════════════════════════════════════════════════════════════════
//
// The highest-severity bug found in this framework: an entity that vanished on its own arrived at the
// reconcile with bHasInstance set, and the reconcile wiped the record — erasing the fact that it had
// been destroyed. A one-shot policy would then rebuild it on the next sweep. This test drives the exact
// sequence: Live, actor destroyed externally, reconcile, then the policy must refuse to rebuild.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnWeaknessReconcileKeepsDestructionTest,
	"TerritoryFramework.Spawn.Weakness.ReconcilePreservesDestruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnWeaknessReconcileKeepsDestructionTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnWeaknessTests;

	UWorld* const World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	UTerritorySpawnSubsystem* const Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Subsystem)
	{
		AddError(TEXT("The spawn subsystem was not created."));
		TeardownWorld(World);
		return false;
	}

	ATerritorySpawnAnchor* const Anchor = MakeAnchor(World, TerritorySpawnTags::EntityActor);
	if (!Anchor)
	{
		AddError(TEXT("Could not spawn the anchor."));
		TeardownWorld(World);
		return false;
	}

	// Reach Live.
	Subsystem->RequestImmediateEvaluation(Anchor);
	TestTrue(TEXT("The slot reached Live"), Anchor->GetLifecycleState() == ESpawnLifecycleState::Live);
	TestEqual(TEXT("Exactly one entity exists"), CountSpawnedEntities(World), 1);

	// The entity is destroyed by gameplay, not by the framework. This is the wrecked-car case.
	AActor* const Entity = Anchor->GetSpawnedActor();
	if (!Entity)
	{
		AddError(TEXT("The anchor did not track its spawned entity."));
		TeardownWorld(World);
		return false;
	}
	World->DestroyActor(Entity);
	TestEqual(TEXT("The external destroy removed the entity"), CountSpawnedEntities(World), 0);

	// The next evaluation must reconcile, not rebuild.
	Subsystem->RequestImmediateEvaluation(Anchor);

	TestTrue(TEXT("The reconcile recorded that the entity was destroyed"),
		Anchor->GetRecord().bWasDestroyed);
	TestEqual(TEXT("A one-shot policy did not rebuild the destroyed entity"),
		CountSpawnedEntities(World), 0);

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 5. A retired slot cannot be un-retired by the reconcile
// ═══════════════════════════════════════════════════════════════════════════════
//
// Retired is terminal for the session, and retiring does not clear bHasInstance. Without an explicit
// guard the reconcile hoisted above would drag a retired slot back to Dormant and hand it a fresh retry
// budget, so a policy would get another attempt at the exact slot the framework already gave up on —
// which is an infinite retry loop wearing a different name. The guard is easy to lose in a refactor and
// invisible in a log, so it gets a test.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritorySpawnWeaknessRetiredStaysRetiredTest,
	"TerritoryFramework.Spawn.Weakness.RetiredStaysTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritorySpawnWeaknessRetiredStaysRetiredTest::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnWeaknessTests;

	UWorld* const World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	UTerritorySpawnSubsystem* const Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Subsystem)
	{
		AddError(TEXT("The spawn subsystem was not created."));
		TeardownWorld(World);
		return false;
	}

	ATerritorySpawnAnchor* const Anchor = MakeAnchor(World, TerritorySpawnTags::EntityActor);
	if (!Anchor)
	{
		AddError(TEXT("Could not spawn the anchor."));
		TeardownWorld(World);
		return false;
	}

	// Reach Live, then retire through the framework's own terminal transition.
	Subsystem->RequestImmediateEvaluation(Anchor);
	TestTrue(TEXT("The slot reached Live"), Anchor->GetLifecycleState() == ESpawnLifecycleState::Live);

	Anchor->CommitTransition(ESpawnLifecycleState::Retired, ESpawnTransitionReason::RetryLimitReached,
		World->GetTimeSeconds());
	TestTrue(TEXT("The slot is retired"),
		Anchor->GetLifecycleState() == ESpawnLifecycleState::Retired);

	// The entity goes away while retired. The record keeps bHasInstance because retiring is not the same
	// as forgetting; the reconcile must still refuse to touch a terminal slot.
	AActor* const Entity = Anchor->GetSpawnedActor();
	if (Entity)
	{
		World->DestroyActor(Entity);
	}
	TestTrue(TEXT("Retiring did not clear the record's instance flag"),
		Anchor->GetRecord().bHasInstance);

	Subsystem->RequestImmediateEvaluation(Anchor);

	TestTrue(TEXT("The reconcile leaves a retired slot retired"),
		Anchor->GetLifecycleState() == ESpawnLifecycleState::Retired);
	TestEqual(TEXT("A retired slot produces no replacement entity"),
		CountSpawnedEntities(World), 0);

	TeardownWorld(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
