#include "Spawn/TerritorySpawnExecutor.h"

#include "Engine/World.h"
#include "Spawn/TerritorySpawnAnchor.h"

UTerritorySpawnExecutor::UTerritorySpawnExecutor()
{
}

UWorld* UTerritorySpawnExecutor::GetWorld() const
{
	if (const UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}
	return nullptr;
}

bool UTerritorySpawnExecutor::SupportsActorClass_Implementation(TSubclassOf<AActor> ActorClass) const
{
	// The base refusal is "no class at all". Subclasses narrow this further; the default actor
	// executor rejects Narrative NPC classes because their removal has to route through the Narrative
	// death path rather than a plain destroy.
	return ActorClass != nullptr;
}

AActor* UTerritorySpawnExecutor::CreateSpawned_Implementation(ATerritorySpawnAnchor* Anchor,
	TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
	// Deliberately empty rather than a generic SpawnActor. A base implementation that spawned anything
	// would let a consumer skip the family-specific setup and get a silently broken entity; returning
	// null makes the framework report a spawn failure until an executor actually implements creation.
	return nullptr;
}

void UTerritorySpawnExecutor::RetireSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
	ESpawnRetireMode Mode)
{
	// Deliberately empty for the same reason as CreateSpawned. The base class must not assume that a
	// plain Destroy is acceptable for an arbitrary family.
}

bool UTerritorySpawnExecutor::SuspendSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor)
{
	return false;
}

bool UTerritorySpawnExecutor::ResumeSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor)
{
	return false;
}

void UTerritorySpawnExecutor::CaptureCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
	FTerritorySpawnRecord& InOutRecord)
{
	// The base executor has no family-specific condition to record. Leaving this empty is the correct
	// contract rather than a stub: the record simply carries no consumer values until an executor adds
	// them, and the framework never interprets them anyway.
}

void UTerritorySpawnExecutor::ApplyCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
	const FTerritorySpawnRecord& Record, bool bRestoreCondition)
{
	// Same contract as CaptureCheckpoint: nothing family-specific to restore at the base level.
	// bRestoreCondition is accepted and ignored here rather than defaulted away, so an override cannot
	// change the signature out from under the framework.
}

bool UTerritorySpawnExecutor::IsClaimedByExternalOwner_Implementation(AActor* Actor) const
{
	// Unknown must answer true. A wrong false destroys something a player or another system still
	// controls; a wrong true only delays a spawn by one sweep. This asymmetry is intentional.
	return Actor != nullptr;
}

ATerritorySpawnAnchor* UTerritorySpawnExecutor::GetOwningAnchor() const
{
	for (const UObject* Candidate = GetOuter(); Candidate; Candidate = Candidate->GetOuter())
	{
		if (const ATerritorySpawnAnchor* Anchor = Cast<const ATerritorySpawnAnchor>(Candidate))
		{
			return const_cast<ATerritorySpawnAnchor*>(Anchor);
		}
	}
	return nullptr;
}
