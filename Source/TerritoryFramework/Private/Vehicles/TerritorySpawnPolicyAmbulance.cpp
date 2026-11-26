#include "Vehicles/TerritorySpawnPolicyAmbulance.h"

UTerritorySpawnPolicyAmbulance::UTerritorySpawnPolicyAmbulance()
{
	// These assignments are deliberately identical to the inherited defaults.
	//
	// An earlier draft of this preset claimed to tune anything, and that would have been a lie the
	// details panel tells: two classes whose numbers agree are one class plus a name. They are written
	// out anyway, and pinned by a test, so that a future change to the general policy's default cannot
	// silently re-tune every hospital in the city. The ambulance stays what it is today; moving a bay
	// is an instance edit, not a code change.
	//
	// The one field that is not inherited as an ambulance opinion is EntityTag and VehicleClass, which a
	// designer must fill per bay. An ambulance bay with no entity tag is inert by design.
	bPersistDamage = false;
	ReplacementDelaySeconds = 45.f;
	ReclaimDistanceCm = 1000.f;
	ReclaimSafeRadiusCm = 2500.f;
	ReclaimDelaySeconds = 60.f;
}
