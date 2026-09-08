#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryRoadNetworkEditorLibrary.generated.h"

class UPhysicalMaterial;

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryRoadBuildReport
{
	GENERATED_BODY()
	/** Whether the requested operation completed successfully; also inspect warnings and detailed results. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") bool bSucceeded = false;
	/** Number of road segments included in the build or inspection. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") int32 Roads = 0;
	/** Number of road junctions included in the build or inspection. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") int32 Junctions = 0;
	/** Road mouths that have no matching connection in the inspected network. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") int32 OpenEnds = 0;
	/** Collision samples used to identify roads in the bounded bake area. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") int32 RoadSamples = 0;
	/** Number of Native ZoneGraph road lanes found after building or inspecting the network. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") int32 NativeRoadLanes = 0;
	/** Separate connected groups found in the loaded road graph. More than one can indicate disconnected routes. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") int32 ConnectedRoadGroups = 0;
	/** Problems that must be fixed before this operation or asset can be considered valid. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") TArray<FString> Errors;
	/** Non-blocking issues to review before relying on the result. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Road") TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryRoadSurfaceBakeSettings
{
	GENERATED_BODY()
	/** One loaded height band. Bake bridges and the street below separately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road") FBox Bounds = FBox(ForceInit);
	/** Collision-sampling grid spacing in centimetres. Smaller spacing captures narrower roads but increases work and sample count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road", meta=(ClampMin="50", ClampMax="500")) float SampleSpacing = 200.f;
	/** Steepest surface angle accepted as driveable road during collision sampling, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road", meta=(ClampMin="1", ClampMax="30")) float MaximumSlopeDegrees = 15.f;
	/** Hard cap on collision samples in this bake. Reduce the bounds or increase spacing if the required grid exceeds it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road", meta=(ClampMin="100", ClampMax="1000000")) int32 MaximumSamples = 250000;
	/** Existing Native ZoneGraph lane-profile name used by the generated road shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road") FName LaneProfileName = TEXT("Road");
	/** Stable editor-authored identity for this bake area; reuse it when rebuilding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road") FGuid BakeID;
	/** Physical material identifying road collision on meshes or landscape layers for this bake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory|Road") TObjectPtr<UPhysicalMaterial> RoadPhysicalMaterial;
};

/** Editor adapter producing Native ZoneShapes/ZoneGraph; never a second runtime graph. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryRoadNetworkEditorLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Import existing generator curves and Socket1/Socket2/... road mouths. No extra spline authoring.
	 * Source actor GUIDs identify generated shapes. Updates only the supplied loaded actors.
	 * Generator geometry is authoritative here. Use BakeRoadSurfaces for collision-driven authoring.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Road|Editor")
	static FTerritoryRoadBuildReport BuildFromRoadActors(UWorld* World,
		const TArray<AActor*>& RoadActors, const TArray<AActor*>& JunctionActors,
		FName LaneProfileName, float EndpointSnapDistance = 300.f);

	/** Detect mesh/landscape road collision surfaces in a loaded bounded area, extract
	 * centre lines and junctions, then bake Native lanes. No manually drawn spline.
	 * Rejects incomplete World Partition coverage and narrow/discontinuous paths.
	 */
	UFUNCTION(BlueprintCallable, Category="Territory|Road|Editor")
	static FTerritoryRoadBuildReport BakeRoadSurfaces(UWorld* World,
		const FTerritoryRoadSurfaceBakeSettings& Settings);

	/** Inspect baked Native lanes, loaded graph groups and unmatched authoring mouths. */
	UFUNCTION(BlueprintCallable, Category="Territory|Road|Editor")
	static FTerritoryRoadBuildReport InspectRoadNetwork(UWorld* World);

	/** Preview the exact existing counterattack road route without spawning or reserving force. */
	UFUNCTION(BlueprintCallable, Category="Territory|Road|Editor")
	static bool PreviewVehicleRoute(UWorld* World, FVector Start, FVector End,
		TArray<FVector>& OutRoute, FString& OutFailure);

	/** JSON of Native lane points and links for offline connectivity/clearance inspection. */
	UFUNCTION(BlueprintCallable, Category="Territory|Road|Editor")
	static FString ExportRoadLaneDiagnostics(UWorld* World);
};
