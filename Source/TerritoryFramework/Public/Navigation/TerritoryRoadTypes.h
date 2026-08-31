#pragma once

#include "CoreMinimal.h"
#include "TerritoryRoadTypes.generated.h"

/** Which side of an authored Territory road spline a vehicle follows. */
UENUM(BlueprintType)
enum class ETerritoryRoadLaneSide : uint8
{
	Center UMETA(ToolTip="Follow the authored spline centre. Useful for a one-lane road or a path already drawn on the exact lane centre."),
	Left UMETA(ToolTip="Follow the left lane relative to the current direction of travel."),
	Right UMETA(ToolTip="Follow the right lane relative to the current direction of travel. Recommended for Narrative's default two-way Road profile.")
};

/** Physical awareness used by possessed Narrative vehicles on Territory road missions. */
USTRUCT(BlueprintType)
struct FTerritoryVehicleAwarenessSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="200.0", ToolTip="How far the centre and left/right vehicle probes look ahead. Narrative Mass traffic has its own obstacle grid; this setting protects the possessed mission vehicle."))
	float ForwardProbeDistance = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="50.0", ToolTip="Half width of each forward awareness box. Keep it close to half the vehicle width so adjacent traffic does not cause false emergency stops."))
	float ProbeHalfWidth = 125.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="25.0", ToolTip="Half height of the awareness boxes. Multi-level roads are ignored when they are outside this height."))
	float ProbeHalfHeight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="0.0", ToolTip="Vehicle brakes fully when a blocking vehicle, pawn, world object, or destructible is this close."))
	float EmergencyStopDistance = 325.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="1.0", ToolTip="Vehicle progressively slows inside this distance. It must be greater than Emergency Stop Distance."))
	float BrakingDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="0.0", ToolTip="Side-probe offset used to understand the left and right space around a blocked lane."))
	float SideProbeOffset = 275.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ToolTip="When the centre is blocked, apply a small steering correction toward a clear side. Disable for narrow roads where braking is safer than changing position."))
	bool bAllowSideAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="0.0", ClampMax="0.5", ToolTip="Maximum temporary steering correction selected by the left/right probes."))
	float MaximumAvoidanceSteering = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Awareness",
		meta=(ClampMin="0.0", ToolTip="A player-chased target may abandon a blocked vehicle after this many seconds and continue to the Road Guide final-fight point. Zero disables the blocked-vehicle handoff."))
	float AbandonAfterBlockedSeconds = 12.f;
};

/** Cleanup policy for temporary reinforcement and story vehicles. */
USTRUCT(BlueprintType)
struct FTerritoryVehicleRetirementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Retirement",
		meta=(ClampMin="0.0", ToolTip="Minimum time an empty mission vehicle remains after its assault resolves. This avoids cars disappearing during the final animation."))
	float EarliestRetirementDelay = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Retirement",
		meta=(ClampMin="0.0", ToolTip="An empty vehicle is kept while any player is within this distance. It can still retire after Hard Retirement Timeout."))
	float PlayerKeepAliveDistance = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road|Retirement",
		meta=(ClampMin="1.0", ToolTip="Maximum cleanup wait for an empty mission vehicle. A vehicle currently possessed by a player is released from Territory management and is never destroyed by this timeout."))
	float HardRetirementTimeout = 120.f;
};
