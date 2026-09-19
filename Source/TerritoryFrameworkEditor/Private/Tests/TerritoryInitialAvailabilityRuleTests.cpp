#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryTypes.h"

/**
 * Guards the one shared answer to "what availability does a new campaign start this Territory with?".
 *
 * This rule used to be written out identically at four separate sites. The test exists because the
 * surviving legacy branch is easy to delete by accident and the damage is invisible: dropping it
 * silently unlocks every Territory authored before `Initial Availability` existed, and nothing else
 * in the suite would notice.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFInitialAvailabilityRule,
	"TerritoryFramework.Core.InitialAvailabilityRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFInitialAvailabilityRule::RunTest(const FString& Parameters)
{
	const ETerritoryInitialState AllStates[] = {
		ETerritoryInitialState::Automatic,
		ETerritoryInitialState::Unclaimed,
		ETerritoryInitialState::Claimed,
		ETerritoryInitialState::Locked,
	};
	const ETerritoryAvailability AllAvailabilities[] = {
		ETerritoryAvailability::Unlocked,
		ETerritoryAvailability::Locked,
	};

	// The whole rule, stated once, over the entire input space. If someone changes the resolution
	// logic, one of these cells disagrees and this test is the thing that says so.
	for (const ETerritoryInitialState State : AllStates)
	{
		for (const ETerritoryAvailability Availability : AllAvailabilities)
		{
			const ETerritoryAvailability Expected =
				State == ETerritoryInitialState::Locked
					? ETerritoryAvailability::Locked
					: Availability;
			TestEqual(
				FString::Printf(TEXT("InitialState=%d with InitialAvailability=%d resolves correctly"),
					static_cast<int32>(State), static_cast<int32>(Availability)),
				static_cast<int32>(TerritoryResolveInitialAvailability(State, Availability)),
				static_cast<int32>(Expected));
		}
	}

	// The two facts that matter, named individually so a failure reads as a sentence rather than
	// as a row in a table. Both directions are asserted: the legacy value must win, and everything
	// else must be left alone. Asserting only the first would let "always return Locked" pass.

	// 1. The legacy value wins. An asset that stored "starts locked" back when Initial State was the
	//    only property keeps starting locked, whatever Initial Availability happens to say.
	TestEqual(TEXT("Legacy Initial State = Locked keeps a Territory locked even when Initial Availability says Unlocked"),
		static_cast<int32>(TerritoryResolveInitialAvailability(
			ETerritoryInitialState::Locked, ETerritoryAvailability::Unlocked)),
		static_cast<int32>(ETerritoryAvailability::Locked));

	// 2. Everything else passes Initial Availability straight through, including Unlocked. This is
	//    the direction that catches a "just always return Locked" simplification.
	TestEqual(TEXT("Modern Initial Availability = Unlocked is honoured when Initial State is not the legacy Locked value"),
		static_cast<int32>(TerritoryResolveInitialAvailability(
			ETerritoryInitialState::Automatic, ETerritoryAvailability::Unlocked)),
		static_cast<int32>(ETerritoryAvailability::Unlocked));

	// 3. Locked availability set the modern way is still Locked. The two properties agree here, so
	//    this also proves the helper does not treat a modern lock as "not the legacy branch" and
	//    hand back Unlocked by accident.
	TestEqual(TEXT("Initial Availability = Locked is honoured when Initial State is Automatic"),
		static_cast<int32>(TerritoryResolveInitialAvailability(
			ETerritoryInitialState::Automatic, ETerritoryAvailability::Locked)),
		static_cast<int32>(ETerritoryAvailability::Locked));

	// 4. The three modern Initial State values are interchangeable as far as availability goes —
	//    availability is the availability property's business, not the political starting state's.
	//    A change that made Claimed or Unclaimed imply Locked would fail here.
	for (const ETerritoryInitialState State : AllStates)
	{
		if (State == ETerritoryInitialState::Locked) continue;
		TestEqual(
			FString::Printf(TEXT("Initial State = %d does not alter availability"), static_cast<int32>(State)),
			static_cast<int32>(TerritoryResolveInitialAvailability(State, ETerritoryAvailability::Unlocked)),
			static_cast<int32>(ETerritoryAvailability::Unlocked));
	}

	return true;
}

/**
 * Guards the shared answer to "does a new campaign start this Territory owned or unowned?".
 *
 * The sibling of the availability rule above, and duplicated the same way it was — a switch on the
 * placed actor, a second switch in the editor analyzer, and a hand-written expression in the
 * replicated world state. The invariant worth protecting here is stated in the old code's comment:
 * never produce "Claimed with no owner".
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFInitialPoliticalStateRule,
	"TerritoryFramework.Core.InitialPoliticalStateRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFInitialPoliticalStateRule::RunTest(const FString& Parameters)
{
	const ETerritoryInitialState AllStates[] = {
		ETerritoryInitialState::Automatic,
		ETerritoryInitialState::Unclaimed,
		ETerritoryInitialState::Claimed,
		ETerritoryInitialState::Locked,
	};

	// The whole rule over the whole input space: four states by "is a faction set".
	for (const ETerritoryInitialState State : AllStates)
	{
		for (const bool bHasOwner : { false, true })
		{
			// Unclaimed means unclaimed. Everything else starts owned exactly when a faction is set.
			const bool bExpectedClaimed = (State != ETerritoryInitialState::Unclaimed) && bHasOwner;
			const ETerritoryState Expected =
				bExpectedClaimed ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
			TestEqual(
				FString::Printf(TEXT("InitialState=%d with owner set=%s resolves correctly"),
					static_cast<int32>(State), bHasOwner ? TEXT("true") : TEXT("false")),
				static_cast<int32>(TerritoryResolveInitialPoliticalState(State, bHasOwner)),
				static_cast<int32>(Expected));
		}
	}

	// The one invariant the old comments called out by name. Asserted on its own so that a regression
	// reads as "the game invented an ownerless claimed Territory" rather than as a table mismatch.
	for (const ETerritoryInitialState State : AllStates)
	{
		TestFalse(
			FString::Printf(TEXT("InitialState=%d with no faction never starts Claimed"), static_cast<int32>(State)),
			TerritoryResolveInitialPoliticalState(State, /*bHasInitialOwningFaction=*/false)
				== ETerritoryState::Claimed);
	}

	// Explicit Unclaimed beats a filled faction — the documented meaning of that option, and the one
	// place where this rule is not simply "an owner means Claimed".
	TestEqual(TEXT("Explicit Unclaimed wins even when a faction is set"),
		static_cast<int32>(TerritoryResolveInitialPoliticalState(
			ETerritoryInitialState::Unclaimed, /*bHasInitialOwningFaction=*/true)),
		static_cast<int32>(ETerritoryState::Unclaimed));

	// The legacy Locked value must still preserve political ownership: an asset that stored "starts
	// locked" keeps its owner, and only its availability changes.
	TestEqual(TEXT("Legacy Locked preserves the authored owner"),
		static_cast<int32>(TerritoryResolveInitialPoliticalState(
			ETerritoryInitialState::Locked, /*bHasInitialOwningFaction=*/true)),
		static_cast<int32>(ETerritoryState::Claimed));

	// A filled faction with Automatic starts owned. This is the direction that catches a "always
	// return Unclaimed" simplification.
	TestEqual(TEXT("Automatic with a faction starts Claimed"),
		static_cast<int32>(TerritoryResolveInitialPoliticalState(
			ETerritoryInitialState::Automatic, /*bHasInitialOwningFaction=*/true)),
		static_cast<int32>(ETerritoryState::Claimed));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
