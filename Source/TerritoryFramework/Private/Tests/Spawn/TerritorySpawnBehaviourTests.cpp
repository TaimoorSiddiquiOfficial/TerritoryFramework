#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnExecutorActor.h"
#include "Spawn/TerritorySpawnPolicyStatic.h"
#include "Spawn/TerritorySpawnSubsystem.h"
#include "Spawn/TerritorySpawnTags.h"

/**
 * Behavioural tests for B2: the default actor executor and the static policy, driven through the
 * subsystem's own evaluation path so they prove the real interlock rather than a private helper.
 *
 * The entity used throughout is ATargetPoint. Two reasons, both learned from a failing run:
 *   - It is a concrete engine class that can never pre-exist in a bare test world, so counting
 *     "actors of exactly this class" is an honest assertion. A bare AActor made the count
 *     meaningless, because the world already contains one.
 *   - A test-only UCLASS would need its own generated header and would put a reflected class into
 *     the shipping module purely to support a test.
 *
 * BeginPlay is dispatched explicitly. A test world created with UWorld::CreateWorld has already run
 * InitializeActorsForPlay by the time an anchor is spawned, so an anchor spawned afterwards never
 * receives BeginPlay on its own and the framework correctly refuses to evaluate it
 * (IsEvaluationBlocked is true while !HasActorBegunPlay). The project's own fixtures do the same.
 *
 * Each test builds its own throwaway world: an anchor registers on BeginPlay and unregisters on
 * EndPlay, so a shared world would let a previous case's registration decide the next one's result.
 */
namespace TerritorySpawnBehaviourTests
{
	/** A world with a spawn subsystem and no net driver, which is the smallest thing that can host a sweep. */
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

	/** Count the entities this slot could have created. */
	int32 CountSpawnedEntities(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ATargetPoint> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	/** Give the anchor the run-state it would have had in a real level: a baked identity and BeginPlay. */
	void InitializeAnchor(ATerritorySpawnAnchor* Anchor)
	{
		if (!Anchor) return;

		// A runtime-spawned anchor has no editor-baked GUID, and the registry deliberately refuses one.
		Anchor->EnsurePersistentAnchorGUID();
		Anchor->DispatchBeginPlay();
	}

	/** Build an anchor with a static policy pointing at ATargetPoint. Returns the anchor or null. */
	ATerritorySpawnAnchor* MakeConfiguredAnchor(UWorld* World, const FVector& Location)
	{
		ATerritorySpawnAnchor* Anchor = World->SpawnActor<ATerritorySpawnAnchor>();
		if (!Anchor) return nullptr;

		Anchor->SetActorLocation(Location);
		InitializeAnchor(Anchor);

		UTerritorySpawnPolicyStatic* Policy = NewObject<UTerritorySpawnPolicyStatic>(Anchor);
		Policy->EntityTag = TerritorySpawnTags::EntityActor;
		Policy->ActorClass = ATargetPoint::StaticClass();
		Policy->RecoveryMode = ESpawnPolicyRecoveryMode::InitialSpawn;
		Anchor->SetPolicyForTesting(Policy);

		return Anchor;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// 1. A configured slot spawns exactly one entity
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnStaticSpawnsOnce,
	"TerritoryFramework.Spawn.Behaviour.StaticPolicySpawnsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnStaticSpawnsOnce::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnBehaviourTests;

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	ATerritorySpawnAnchor* Anchor = MakeConfiguredAnchor(World, FVector(0.f, 0.f, 100.f));
	UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();
	if (!Anchor || !Subsystem)
	{
		AddError(TEXT("Anchor or spawn subsystem missing."));
		TeardownWorld(World);
		return false;
	}

	TestTrue(TEXT("Anchor reports itself begun play"), Anchor->HasActorBegunPlay());
	TestFalse(TEXT("A configured anchor is not evaluation-blocked"), Anchor->IsEvaluationBlocked());
	TestTrue(TEXT("Anchor is configured well enough to run"), Anchor->IsAnchorConfigured());

	// Two evaluations in a row. The second must NOT produce a second actor: a slot that spawns on
	// every sweep is the most likely way to duplicate a vehicle in a shipping build, and a naive
	// "if (SpawnedActor == nullptr)" guard is exactly what would allow it.
	Subsystem->RequestImmediateEvaluation(Anchor);
	const int32 AfterFirst = CountSpawnedEntities(World);
	Subsystem->RequestImmediateEvaluation(Anchor);
	const int32 AfterSecond = CountSpawnedEntities(World);

	TestEqual(TEXT("First evaluation creates exactly one entity"), AfterFirst, 1);
	TestEqual(TEXT("Second evaluation does not create a second entity"), AfterSecond, 1);
	TestNotNull(TEXT("The anchor tracks the spawned actor"), Anchor->GetSpawnedActor());
	TestEqual(TEXT("Slot reaches Live"), static_cast<int32>(Anchor->GetLifecycleState()),
		static_cast<int32>(ESpawnLifecycleState::Live));
	TestTrue(TEXT("The record agrees that an instance exists"), Anchor->GetRecord().bHasInstance);
	TestEqual(TEXT("Generation advanced exactly once"), Anchor->GetRecord().Generation, 1);

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 2. A slot with no usable policy stays inert
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnUnconfiguredIsInert,
	"TerritoryFramework.Spawn.Behaviour.UnconfiguredAnchorIsInert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnUnconfiguredIsInert::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnBehaviourTests;

	// Case B below assigns a policy with no entity tag on purpose, and the anchor reports that as an
	// error. Declaring it keeps the run honest: without this the framework counts the engine log as a
	// test failure, and the case would look broken while proving exactly what it set out to prove.
	AddExpectedError(TEXT("reports an incomplete configuration"), EAutomationExpectedErrorFlags::Contains, 1);

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>();

	// Case A: a begun-play anchor with a valid GUID but no policy. A freshly placed anchor with no
	// policy yet is a normal authoring state, and the slot must be inert rather than spawning
	// something arbitrary. BeginPlay is dispatched so it is the ABSENT POLICY that makes this inert,
	// not an un-begun actor.
	ATerritorySpawnAnchor* Anchor = World->SpawnActor<ATerritorySpawnAnchor>();
	if (Anchor)
	{
		InitializeAnchor(Anchor);
	}
	TestTrue(TEXT("An anchor exists for the inert cases"), Anchor != nullptr);
	TestFalse(TEXT("An anchor with no policy is not configured"), Anchor && Anchor->IsAnchorConfigured());
	if (Subsystem && Anchor)
	{
		Subsystem->RequestImmediateEvaluation(Anchor);
	}
	TestNull(TEXT("An anchor with no policy spawns nothing"), Anchor ? Anchor->GetSpawnedActor() : nullptr);
	TestEqual(TEXT("No entity exists after an inert evaluation"), CountSpawnedEntities(World), 0);

	// Case B: a policy whose entity tag is unset — the "half-authored asset" case that would otherwise
	// spawn into a bucket nobody can query.
	if (Anchor)
	{
		UTerritorySpawnPolicyStatic* HalfAuthored = NewObject<UTerritorySpawnPolicyStatic>(Anchor);
		HalfAuthored->ActorClass = ATargetPoint::StaticClass();
		// EntityTag deliberately left at its zero value.
		Anchor->SetPolicyForTesting(HalfAuthored);

		TestFalse(TEXT("An anchor whose policy has no entity tag is not configured"),
			Anchor->IsAnchorConfigured());
		if (Subsystem)
		{
			Subsystem->RequestImmediateEvaluation(Anchor);
		}
		TestNull(TEXT("A half-authored policy spawns nothing"), Anchor->GetSpawnedActor());
	}

	TestEqual(TEXT("Still no entity after the half-authored case"), CountSpawnedEntities(World), 0);

	TeardownWorld(World);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 3. The default executor refuses Narrative NPC classes
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFSpawnRejectsNarrativeNPC,
	"TerritoryFramework.Spawn.Behaviour.DefaultExecutorRejectsNarrativeNPC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSpawnRejectsNarrativeNPC::RunTest(const FString& Parameters)
{
	using namespace TerritorySpawnBehaviourTests;

	UWorld* World = MakeWorld();
	if (!World)
	{
		AddError(TEXT("Could not create the test world."));
		return false;
	}

	UTerritorySpawnExecutorActor* Executor = NewObject<UTerritorySpawnExecutorActor>(World);

	// A plain actor is supported; a Narrative NPC is not. The exclusion exists because a Narrative
	// character must be removed through the Narrative death path; a plain Destroy on one leaves the
	// ASC, the activity and the save record disagreeing about whether it still exists.
	TestTrue(TEXT("The default executor supports a plain actor"),
		Executor->SupportsActorClass(AActor::StaticClass()));

	UClass* NPCClass = FindObject<UClass>(nullptr, TEXT("/Script/NarrativeArsenal.NarrativeNPCCharacter"));
	if (NPCClass)
	{
		TestFalse(TEXT("The default executor refuses a Narrative NPC class"),
			Executor->SupportsActorClass(NPCClass));
	}
	else
	{
		// Not a silent skip: if the class cannot be found the test has lost its subject, and saying so
		// is better than passing a test that asserted nothing.
		AddWarning(TEXT("NarrativeNPCCharacter was not reflected; the NPC exclusion was not exercised."));
	}

	TeardownWorld(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
