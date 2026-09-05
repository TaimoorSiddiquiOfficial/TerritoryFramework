#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/TerritoryAssaultPlanningLimits.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include <limits>

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
	const TArray<int32> FullAuthoredSeats = { 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 8, 8, 8 };
	for (int32 Cars = 0; Cars <= 8; ++Cars)
	{
		for (int32 FiniteForce = 0; FiniteForce <= 64; ++FiniteForce)
		{
			TestEqual(TEXT("Bounded planning preserves the existing answer for every legal car budget"),
				UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(FiniteForce, Cars, Seats),
				UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(FiniteForce, Cars, FullAuthoredSeats));
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
