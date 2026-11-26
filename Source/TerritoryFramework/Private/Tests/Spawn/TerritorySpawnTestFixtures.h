// Copyright Territory Framework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Spawn/TerritorySpawnPolicyStatic.h"
#include "TerritorySpawnTestFixtures.generated.h"

/**
 * A concrete, non-abstract object used as a lease holder in tests.
 *
 * `NewObject<UObject>` is an ensure failure, not a null return: UObject itself is abstract, and the
 * editor reports "Class which was marked abstract was trying to be loaded". A lease holder only has to
 * be a live UObject that can be weakly referenced, so a plain named subclass is enough.
 */
UCLASS()
class UTerritorySpawnTestLeaseHolder : public UObject
{
	GENERATED_BODY()
};

/**
 * A possessable stand-in for "a vehicle somebody is driving".
 *
 * The interlock's most important case is "the policy wants this gone and a player is using it". That
 * case cannot be tested with a plain actor, because the framework's own claim predicate is "a pawn
 * with a controller". Using a real APawn means the test exercises the actual predicate rather than a
 * mock that agrees with whatever the code happens to do.
 *
 * The root component is NOT optional. A bare APawn has no RootComponent, and AActor::SetActorLocation
 * returns false without moving anything in that case, so a test that spawns one and positions it is
 * silently measuring a pawn standing at the world origin. "The player is far away" and "the player is
 * right here" then produce the same coordinates, and the assertion passes for the wrong reason.
 */
UCLASS()
class ATerritorySpawnTestPawn : public APawn
{
	GENERATED_BODY()

public:
	ATerritorySpawnTestPawn()
	{
		PrimaryActorTick.bCanEverTick = false;

		RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
		SetRootComponent(RootScene);
	}

	UPROPERTY()
	TObjectPtr<USceneComponent> RootScene;
};

/**
 * A policy that can be told what to answer, so the framework's interlocks are decidable in a test.
 *
 * Why this exists: the shipped static policy deliberately NEVER asks for a retire. That is correct
 * behaviour, but it also means the safety interlock — the block that must be able to VETO a retire —
 * is unreachable through the production policy. A test that cannot ask for a retire cannot prove the
 * veto, so the veto would ship unverified. This fixture supplies the missing "malicious policy" case:
 * it can demand a retire on every sweep, and the framework must still refuse whenever a lease, an
 * external owner, or the authored grace is in the way.
 *
 * It extends the static policy rather than reimplementing it, so spawn behaviour stays identical to
 * production and only the retire answer changes. That keeps the interlock tests measuring the veto,
 * not a second spawn path.
 *
 * Deliberately not wrapped in WITH_DEV_AUTOMATION_TESTS: UHT rejects a UCLASS inside a preprocessor
 * block. The class exists in every build, adds no behaviour except the flags below, and is never
 * referenced outside Private/Tests.
 */
UCLASS()
class UTerritorySpawnTestFixturePolicy : public UTerritorySpawnPolicyStatic
{
	GENERATED_BODY()

public:
	/** When true, every evaluation demands that the current instance be retired. */
	UPROPERTY()
	bool bRequestRetireInTest = false;

	/** The mode to demand with that retire. */
	UPROPERTY()
	ESpawnRetireMode RetireModeInTest = ESpawnRetireMode::Destroy;

	/** Grace the framework must honour before an approved retire. Zero disables only this check. */
	UPROPERTY()
	float MinimumLiveSecondsInTest = 0.f;

	virtual FTerritorySpawnDecision EvaluateDecision_Implementation(
		const FTerritorySpawnRequest& Request) const override
	{
		// Only override the answer when the slot is actually holding something. A retire request with
		// nothing to retire is a different case (the reconcile path) and must stay reachable through
		// the base policy's normal spawn answer.
		if (bRequestRetireInTest && Request.Record.bHasInstance)
		{
			FTerritorySpawnDecision Decision;
			Decision.Action = ESpawnAction::Retire;
			Decision.RetireMode = RetireModeInTest;
			Decision.Reason = ESpawnTransitionReason::PolicyRequest;
			Decision.MinimumLiveSeconds = MinimumLiveSecondsInTest;
			return Decision;
		}

		return Super::EvaluateDecision_Implementation(Request);
	}
};
