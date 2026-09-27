// Copyright Territory Framework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractableComponent.h"
#include "TerritoryPatrolSpotTestFixtures.generated.h"

/**
 * A concrete interaction slot behavior whose availability answers the test controls.
 *
 * Narrative Pro's UInteractionSlotBehavior is Abstract, and the shipped Sit behavior is a
 * Blueprint whose answers depend on vendor content defaults, so neither lets a spot-resolver
 * test state its own premise. This fixture does — which is what makes the occupied/stealable
 * cases decidable, instead of the test quietly measuring whatever the shipped seat happens to
 * do. It is test scaffolding only.
 *
 * Deliberately not wrapped in WITH_DEV_AUTOMATION_TESTS: UHT rejects a UCLASS inside a
 * preprocessor block, so the class exists in every build. It adds no behaviour of its own —
 * only the two answers the tests set — and it is never referenced outside Private/Tests.
 * Inline overrides remain defined when WITH_DEV_AUTOMATION_TESTS is false, because UHT's
 * reflected class still needs their vtable entries in a non-test build.
 */
UCLASS()
class UTerritoryTestSlotBehavior : public UInteractionSlotBehavior
{
	GENERATED_BODY()

public:
	/** Answer for IsUsable, for every slot and interactor. */
	UPROPERTY()
	bool bUsableInTest = true;

	/** Answer for IsStealable, for every slot and interactor. */
	UPROPERTY()
	bool bStealableInTest = false;

	virtual bool IsUsable_Implementation(const int32 OurSlot,
		class UNarrativeInteractableComponent* OurInteractable,
		class UNarrativeInteractionComponent* Interactor) const override
	{
		return bUsableInTest;
	}

	virtual bool IsStealable_Implementation(const int32 OurSlot,
		class UNarrativeInteractableComponent* OurInteractable,
		class UNarrativeInteractionComponent* Interactor) const override
	{
		return bStealableInTest;
	}
};
