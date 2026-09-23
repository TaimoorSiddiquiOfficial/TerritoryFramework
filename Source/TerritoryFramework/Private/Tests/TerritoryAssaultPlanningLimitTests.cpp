#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/TerritoryAssaultPlanningLimits.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include <limits>

// A second expression of the planned-force rule, written from the contract rather than by
// calling the planner: the force a column can move is the request capped by the seats of
// the largest deployments the budget authorises. It sums every usable deployment and caps
// once, where the planner accumulates and returns early, so the two disagree if that early
// return is wrong. It is a second statement of the same rule, not an oracle.
static int32 ReferenceVehicleOnlyPlannedForce(const int32 RequestedForce,
	const int32 MaximumVehicleDeployments, const TArray<int32>& VehicleDeploymentCapacities)
{
	if (RequestedForce <= 0 || MaximumVehicleDeployments <= 0
		|| VehicleDeploymentCapacities.IsEmpty())
	{
		return 0;
	}

	TArray<int32> SortedCapacities = VehicleDeploymentCapacities;
	SortedCapacities.Sort(TGreater<int32>());
	const int32 UsableDeployments = FMath::Min(
		MaximumVehicleDeployments, SortedCapacities.Num());
	int64 AvailableSeats = 0;
	for (int32 Index = 0; Index < UsableDeployments; ++Index)
	{
		AvailableSeats += FMath::Max(0, SortedCapacities[Index]);
	}
	return static_cast<int32>(FMath::Min(static_cast<int64>(RequestedForce), AvailableSeats));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultPlanningLimits,
	"TerritoryFramework.CounterAttack.Regression.BoundedAuthoredPlanning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultPlanningLimits::RunTest(const FString& Parameters)
{
	using namespace TerritoryAssaultPlanning;
	TestEqual(TEXT("Huge finite power reaches the authored approach cap"),
		ResolveApproachCount(std::numeric_limits<float>::max(), 8), 8);
	TestEqual(TEXT("An out-of-range configured limit cannot bypass eight approaches"),
		ResolveApproachCount(100.f, MAX_int32), 8);
	TestEqual(TEXT("A malformed negative limit still permits one valid route"),
		ResolveApproachCount(2.f, MIN_int32), 1);
	int32 RoadMaximum = 0;
	TArray<int32> Seats;
	AccumulateVehicleCapacity(1000, 4, RoadMaximum, Seats);
	TestEqual(TEXT("Authored counts are bounded before allocation"), Seats.Num(), 8);
	TestEqual(TEXT("Authored car total honors the existing eight-car budget"), RoadMaximum, 8);
	TestEqual(TEXT("Positive infinity saturates at the authored cap"),
		ResolveApproachCount(std::numeric_limits<float>::infinity(), 3), 3);
	TestEqual(TEXT("NaN uses one valid approach"),
		ResolveApproachCount(std::numeric_limits<float>::quiet_NaN(), 3), 1);
	TestEqual(TEXT("Negative infinity uses one valid approach"),
		ResolveApproachCount(-std::numeric_limits<float>::infinity(), 3), 1);
	for (int32 Limit = 1; Limit <= 8; ++Limit)
	{
		int32 PreviousCount = 1;
		for (int32 PowerStep = 0; PowerStep <= 128; ++PowerStep)
		{
			const int32 Count = ResolveApproachCount(PowerStep / 8.f, Limit);
			TestTrue(TEXT("More finite attack power never selects fewer approaches"), Count >= PreviousCount);
			TestTrue(TEXT("Approach selection stays inside its authored limit"), Count >= 1 && Count <= Limit);
			PreviousCount = Count;
		}
	}
	AccumulateVehicleCapacity(MAX_int32, MAX_int32, RoadMaximum, Seats);
	TestEqual(TEXT("Maximum integer car count remains bounded"), Seats.Num(), 8);
	TestEqual(TEXT("Maximum integer seat counts cannot overflow finite force"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(MAX_int32, 8, Seats), MAX_int32);
	AccumulateVehicleCapacity(MIN_int32, MIN_int32, RoadMaximum, Seats);
	TestEqual(TEXT("Negative car counts cannot remove an existing valid budget"), RoadMaximum, 8);

	RoadMaximum = 0;
	Seats.Reset();
	AccumulateVehicleCapacity(6, 2, RoadMaximum, Seats);
	AccumulateVehicleCapacity(5, 4, RoadMaximum, Seats);
	AccumulateVehicleCapacity(3, 8, RoadMaximum, Seats);
	TestEqual(TEXT("Only the best eight authored cars contribute to seat planning"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(100, 8, Seats), 44);
	TArray<int32> ReorderedSeats;
	int32 ReorderedMaximum = 0;
	AccumulateVehicleCapacity(3, 8, ReorderedMaximum, ReorderedSeats);
	AccumulateVehicleCapacity(5, 4, ReorderedMaximum, ReorderedSeats);
	AccumulateVehicleCapacity(6, 2, ReorderedMaximum, ReorderedSeats);
	TestTrue(TEXT("Approach order cannot reroll vehicle capacity planning"), ReorderedSeats == Seats);
	// The authored list is descending; this hand written one is the same seats in the
	// opposite order. The planner sorts, so the two must plan identically.
	const TArray<int32> FullAuthoredSeats = { 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 8, 8, 8 };
	TestEqual(TEXT("Capacity list order cannot change the planned force"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(100, 8, Seats),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(100, 8, FullAuthoredSeats));

	// Sweep every legal budget and request, asserting the contract rather than comparing the
	// planner with itself. The reference states the rule independently; the invariants below
	// hold for any correct planner without restating the rule, so a shared misreading of the
	// rule cannot satisfy them.
	int32 TotalAuthoredSeats = 0;
	for (const int32 Capacity : Seats)
	{
		TotalAuthoredSeats += FMath::Max(0, Capacity);
	}
	TArray<int32> PreviousCarsForce;
	PreviousCarsForce.Init(0, 65);
	for (int32 Cars = 0; Cars <= 8; ++Cars)
	{
		int32 PreviousForce = 0;
		for (int32 FiniteForce = 0; FiniteForce <= 64; ++FiniteForce)
		{
			const int32 Planned = UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(
				FiniteForce, Cars, Seats);
			TestEqual(TEXT("Bounded planning matches the authored rule at every legal car budget"),
				Planned, ReferenceVehicleOnlyPlannedForce(FiniteForce, Cars, Seats));
			TestTrue(TEXT("Planning never transports more force than was requested"),
				Planned >= 0 && Planned <= FiniteForce);
			TestTrue(TEXT("Planning never invents more seats than were authored"),
				Planned <= TotalAuthoredSeats);
			TestTrue(TEXT("More requested force never transports less"),
				Planned >= PreviousForce);
			TestTrue(TEXT("A larger car budget never transports less"),
				Planned >= PreviousCarsForce[FiniteForce]);
			PreviousForce = Planned;
			PreviousCarsForce[FiniteForce] = Planned;
		}
	}
	FTerritoryFactionAssaultConfig Force;
	Force.bScaleVehicleCountByNarrativeDifficulty = true;
	const int32 MediumCars = UTerritoryCounterAttackProfile::ResolveVehicleCountForDifficulty(
		Force, ENarrativeGameplayDifficulty::Medium, RoadMaximum, 100);
	const int32 HardCars = UTerritoryCounterAttackProfile::ResolveVehicleCountForDifficulty(
		Force, ENarrativeGameplayDifficulty::Hard, RoadMaximum, 100);
	TestEqual(TEXT("Native Medium difficulty still transports the best single car"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(100, MediumCars, Seats), 8);
	TestEqual(TEXT("Native Hard difficulty still transports the best two cars"),
		UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(100, HardCars, Seats), 16);
	return true;
}

#endif
