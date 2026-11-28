#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryCollectibleTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "Interaction/TerritoryCollectiblePickup.h"
#include "Subsystems/TerritoryCollectibleSubsystem.h"

/**
 * Behavioural tests for the collectible pickup producer.
 *
 * The pickup is the only thing in the game that records a collectible, so these tests drive it
 * the way a player would: spawn the actor, hand it a pawn, and read the authority back. They all
 * build a real world and let it begin play, because the subsystem deliberately reports "not
 * loaded yet" until the world has started, and a test that skipped that would be asserting
 * against a fixture that cannot happen in game.
 */
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestPickupRelic, "Territory.Collectible.Test.BrassRelic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestPickupLetter, "Territory.Collectible.Test.OldLetter");

namespace TestCollectiblePickups
{
	const FGameplayTag Relic = TAG_TestPickupRelic.GetTag();
	const FGameplayTag Letter = TAG_TestPickupLetter.GetTag();

	/** A world that has begun play, so the collectible window reports itself as loaded. */
	UWorld* CreateStartedWorld(UWorld*& OutWorld, FWorldContext*& OutContext, const bool bAsClient)
	{
		OutWorld = UWorld::CreateWorld(EWorldType::Game, false);
		if (OutWorld == nullptr)
		{
			OutContext = nullptr;
			return nullptr;
		}

		OutContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
		OutContext->SetCurrentWorld(OutWorld);

		if (bAsClient)
		{
			// The engine derives the net mode during BeginPlay from the travel URL.
			OutWorld->NextURL = TEXT("127.0.0.1:7777");
		}

		OutWorld->InitializeActorsForPlay(FURL());
		OutWorld->BeginPlay();
		return OutWorld;
	}

	void DestroyStartedWorld(UWorld* World, FWorldContext* Context)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		if (Context)
		{
			GEngine->DestroyWorldContext(World);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectiblePickupRecordsOnce,
	"TerritoryFramework.Collectibles.Producer.PickupRecordsOneCampaignFinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectiblePickupRecordsOnce::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	FWorldContext* Context = nullptr;
	if (TestCollectiblePickups::CreateStartedWorld(World, Context, /*bAsClient=*/false) == nullptr)
	{
		AddError(TEXT("The pickup fixture world could not be created"));
		return false;
	}

	bool bPassed = true;
	ON_SCOPE_EXIT
	{
		TestCollectiblePickups::DestroyStartedWorld(World, Context);
	};

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	if (!TestNotNull(TEXT("The collectible authority exists in the game world"), Collectibles))
	{
		return false;
	}
	TestTrue(TEXT("A started world reports its collectible window as loaded"),
		Collectibles->IsCollectibleLedgerReady());

	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>();
	if (!TestNotNull(TEXT("A pawn stands in for the collecting player"), Pawn))
	{
		return false;
	}

	ATerritoryCollectiblePickup* Pickup = World->SpawnActor<ATerritoryCollectiblePickup>();
	if (!TestNotNull(TEXT("A pickup can be placed in the level"), Pickup))
	{
		return false;
	}
	Pickup->CollectibleTag = TestCollectiblePickups::Relic;
	Pickup->bDestroyWhenCollected = false;

	// 1. A non-pawn overlap (a dropped prop, a projectile) must not record a discovery.
	AActor* Prop = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("A plain actor stands in for a dropped prop"), Prop))
	{
		return false;
	}

	FTerritoryCollectibleMutationResult NotAPawn = Pickup->Collect(Prop);
	TestFalse(TEXT("A non-pawn does not collect"), NotAPawn.bSuccess);
	TestEqual(TEXT("A refused collect leaves no finding"), Collectibles->GetFindingCount(), 0);

	// 2. The real path.
	FTerritoryCollectibleMutationResult Found = Pickup->Collect(Pawn);
	TestTrue(TEXT("A pawn collects the pickup"), Found.bSuccess);
	TestTrue(TEXT("The finding carries the authored collectible tag"),
		Found.Finding.CollectibleTag == TestCollectiblePickups::Relic);
	TestTrue(TEXT("The authority now reports the collectible as found"),
		Collectibles->HasCollectibleBeenFound(TestCollectiblePickups::Relic));
	TestTrue(TEXT("The pickup reports itself as collected"), Pickup->IsCollected());
	TestEqual(TEXT("One pickup records exactly one finding"), Collectibles->GetFindingCount(), 1);

	// 3. The same instance cannot record twice, so a repeated overlap in one frame is not a
	// second discovery.
	FTerritoryCollectibleMutationResult Again = Pickup->Collect(Pawn);
	TestFalse(TEXT("The same pickup does not record twice"), Again.bSuccess);
	TestEqual(TEXT("A refused repeat leaves the window unchanged"), Collectibles->GetFindingCount(), 1);

	// 4. A second pickup of the same collectible is already collected: the record is
	// campaign-wide, not per-actor.
	ATerritoryCollectiblePickup* Twin = World->SpawnActor<ATerritoryCollectiblePickup>();
	if (TestNotNull(TEXT("A second pickup of the same collectible can be placed"), Twin))
	{
		Twin->CollectibleTag = TestCollectiblePickups::Relic;
		Twin->bDestroyWhenCollected = false;

		TestTrue(TEXT("The twin already reads as collected"), Twin->IsCollected());

		FTerritoryCollectibleMutationResult TwinResult = Twin->Collect(Pawn);
		TestTrue(TEXT("Collecting the twin is reported as a repeat, not an error"), TwinResult.bSuccess);
		TestTrue(TEXT("The repeat is flagged as already collected"), TwinResult.bAlreadyCollected);
		TestEqual(TEXT("A repeat adds no second row"), Collectibles->GetFindingCount(), 1);
		TestEqual(TEXT("A repeat does not inflate the recorded counter"),
			Collectibles->GetTotalRecordedFindingCount(), 1);
	}

	// 5. A collected pickup still overlaps nothing, but keeps its actor alive for Blueprint hooks.
	FTerritoryCollectibleMutationResult Other = Pickup->Collect(Pawn);
	TestFalse(TEXT("A collected pickup stays quiet"), Other.bSuccess);

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectiblePickupRefusesUntagged,
	"TerritoryFramework.Collectibles.Producer.PickupWithoutATagRecordsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectiblePickupRefusesUntagged::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	FWorldContext* Context = nullptr;
	if (TestCollectiblePickups::CreateStartedWorld(World, Context, /*bAsClient=*/false) == nullptr)
	{
		AddError(TEXT("The pickup fixture world could not be created"));
		return false;
	}

	ON_SCOPE_EXIT
	{
		TestCollectiblePickups::DestroyStartedWorld(World, Context);
	};

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>();
	ATerritoryCollectiblePickup* Pickup = World->SpawnActor<ATerritoryCollectiblePickup>();
	if (Collectibles == nullptr || Pawn == nullptr || Pickup == nullptr)
	{
		AddError(TEXT("The untagged pickup fixture could not be built"));
		return false;
	}

	Pickup->bDestroyWhenCollected = false;

	// A pickup whose tag is neither authored nor inherited from a definition must refuse: a
	// finding without a collectible would be a row the Dossier can never name.
	TestFalse(TEXT("An untagged pickup reports no collectible tag"), Pickup->GetCollectibleTag().IsValid());

	FTerritoryCollectibleMutationResult Result = Pickup->Collect(Pawn);
	TestFalse(TEXT("An untagged pickup records nothing"), Result.bSuccess);
	TestTrue(TEXT("The refusal names the missing tag"),
		Result.FailureReason.ToString().Contains(TEXT("collectible tag")));
	TestEqual(TEXT("Nothing was written"), Collectibles->GetFindingCount(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectiblePickupRefusesClients,
	"TerritoryFramework.Collectibles.Producer.ClientPickupIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectiblePickupRefusesClients::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	FWorldContext* Context = nullptr;
	if (TestCollectiblePickups::CreateStartedWorld(World, Context, /*bAsClient=*/true) == nullptr)
	{
		AddError(TEXT("The client pickup fixture world could not be created"));
		return false;
	}

	ON_SCOPE_EXIT
	{
		TestCollectiblePickups::DestroyStartedWorld(World, Context);
	};

	TestEqual(TEXT("The fixture world is a client"), static_cast<int32>(World->GetNetMode()),
		static_cast<int32>(NM_Client));

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>();
	ATerritoryCollectiblePickup* Pickup = World->SpawnActor<ATerritoryCollectiblePickup>();
	if (Collectibles == nullptr || Pawn == nullptr || Pickup == nullptr)
	{
		AddError(TEXT("The client pickup fixture could not be built"));
		return false;
	}

	Pickup->CollectibleTag = TestCollectiblePickups::Letter;
	Pickup->bDestroyWhenCollected = false;

	// A client cannot write the saved finding window, so it must fail loudly rather than
	// pretending the collectible was recorded for the campaign. Note which guard answers: a
	// freshly spawned actor's default role is Authority even in a client world, so the pickup's
	// own fast check passes and the subsystem's net-mode check is the one that refuses. Both
	// exist on purpose — the actor one avoids the work, the subsystem one is the authority.
	FTerritoryCollectibleMutationResult Result = Pickup->Collect(Pawn);
	TestFalse(TEXT("A client cannot collect"), Result.bSuccess);
	TestTrue(FString::Printf(TEXT("The refusal explains itself (%s)"), *Result.FailureReason.ToString()),
		!Result.FailureReason.IsEmpty());
	TestEqual(TEXT("A client writes no finding"), Collectibles->GetFindingCount(), 0);
	TestFalse(TEXT("A client cannot fabricate the collectible as found"),
		Collectibles->HasCollectibleBeenFound(TestCollectiblePickups::Letter));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
