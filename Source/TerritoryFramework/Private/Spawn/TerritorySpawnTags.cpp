#include "Spawn/TerritorySpawnTags.h"

namespace TerritorySpawnTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity,
		"Territory.Spawn.Entity",
		"Root of every semantic entity identity used by the generic spawn framework. Consumers declare children; the framework itself only reads this value for bucketing and validation.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EntityActor,
		"Territory.Spawn.Entity.Actor",
		"A spawn family realized as one addressable actor: a vehicle, a unique prop, a mission object. The default actor executor supports this family.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EntityInstanced,
		"Territory.Spawn.Entity.Instanced",
		"A spawn family realized as one member of an instanced set inside a chunk actor rather than as its own actor. Deliberately has no shipped executor yet: an instanced realizer is a later batch, and claiming otherwise in a tag description would be a lie the code does not honour.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lease,
		"Territory.Spawn.Lease",
		"Root of every lease purpose a holder may pin a spawn slot with.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LeaseMission,
		"Territory.Spawn.Lease.Mission",
		"A running mission needs this entity, so the slot must not retire it until the lease ends or expires.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LeaseInteraction,
		"Territory.Spawn.Lease.Interaction",
		"A player interaction currently holds this entity, for example driving it or chopping it.");
}
