#pragma once

#include "NativeGameplayTags.h"

/**
 * Built-in semantic identities for the generic spawn framework.
 *
 * These are IDENTITY tags, not condition tags. A policy returns one of
 * Territory.Spawn.Entity.* from GetEntityTag() to say what family it produces; the framework
 * buckets, logs and validates on that value and never interprets it further. A consumer that
 * needs a new family adds a child tag in its own namespace rather than editing this header,
 * because a framework header that grows a tag per game feature is a header the framework cannot
 * ship without.
 *
 * Deliberately NOT in here: condition tags such as a car being away, wrecked or leased. Those are
 * the consumer's words and they live in the record's StateTags, where the core never reads them.
 */
namespace TerritorySpawnTags
{
	/** Root of every framework spawn identity. Children are declared by consumers. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity);

	/** A single addressable actor that exists or does not exist: a vehicle, a unique prop, a mission object. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EntityActor);

	/** One member of an instanced set that lives inside a chunk actor rather than as its own actor. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EntityInstanced);

	/** Root of every framework spawn lease purpose. A holder names why it is pinning a slot. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lease);

	/** A mission needs this entity until it ends; the slot must not retire it meanwhile. */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LeaseMission);

	/** A player interaction holds this entity (driving, carrying, chopping). */
	TERRITORYFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LeaseInteraction);
}
