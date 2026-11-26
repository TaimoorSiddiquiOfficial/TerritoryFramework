#pragma once

#include "CoreMinimal.h"

#include "Vehicles/TerritorySpawnPolicyVehicle.h"

#include "TerritorySpawnPolicyAmbulance.generated.h"

/**
 * Ambulance bay. A thin preset over UTerritorySpawnPolicyVehicle, kept as its own class so an existing
 * level that already references it keeps working after the framework generalised.
 *
 * Everything about how an ambulance behaves — rebuild, reclaim, condition, leases — is inherited. This
 * header once carried all of that logic itself, which is the shape the user objected to: a managed
 * vehicle type should be a data configuration, not a C++ class named after one noun. A fire engine,
 * a police cruiser or a taxi now needs no code at all; only the numbers differ.
 *
 * The class survives as a PRESET so that:
 *
 *   - existing anchors and Blueprints that reference it do not silently lose their policy, and
 *   - the hospital's own tuning has a name a designer can recognise in the dropdown.
 *
 * Its constructor only sets defaults that differ from the general policy's, and it adds no fields.
 * If a designer needs a different ambulance, they change the instance, not this class.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced,
	meta = (DisplayName = "Spawn Policy (Ambulance Bay)"))
class TERRITORYFRAMEWORK_API UTerritorySpawnPolicyAmbulance : public UTerritorySpawnPolicyVehicle
{
	GENERATED_BODY()

public:
	UTerritorySpawnPolicyAmbulance();
};
