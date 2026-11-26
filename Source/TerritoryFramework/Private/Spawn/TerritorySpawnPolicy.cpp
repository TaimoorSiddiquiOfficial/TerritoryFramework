#include "Spawn/TerritorySpawnPolicy.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnExecutor.h"

UTerritorySpawnPolicy::UTerritorySpawnPolicy()
{
}

UWorld* UTerritorySpawnPolicy::GetWorld() const
{
	// Instanced subobjects have no world of their own, so the outer chain is walked instead. This is
	// the pattern every instanced Territory adapter already uses; without it a policy that needs the
	// world in an editor context would assert.
	if (const UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}
	return nullptr;
}

FGameplayTag UTerritorySpawnPolicy::GetEntityTag_Implementation() const
{
	// Zero tag by default. An unset tag is what makes an anchor refuse to run, which is safer than
	// silently bucketing an unconfigured slot under whatever tag happens to be first.
	return FGameplayTag();
}

TSubclassOf<UTerritorySpawnExecutor> UTerritorySpawnPolicy::GetExecutorClass_Implementation() const
{
	return nullptr;
}

TSoftClassPtr<AActor> UTerritorySpawnPolicy::GetDesiredActorClass_Implementation() const
{
	return TSoftClassPtr<AActor>();
}

FTerritorySpawnDecision UTerritorySpawnPolicy::EvaluateDecision_Implementation(const FTerritorySpawnRequest& Request) const
{
	// None is the zero value on purpose: a subclass that forgets to override this produces an inert
	// slot rather than an accidental spawn. That mirrors the project's existing "AnyFloor is the zero
	// value on purpose" convention.
	return FTerritorySpawnDecision();
}

bool UTerritorySpawnPolicy::ShouldEvaluate_Implementation(const FTerritorySpawnRequest& Request) const
{
	return true;
}

bool UTerritorySpawnPolicy::AllowsLease_Implementation(FGameplayTag Purpose) const
{
	// Per default yes, and the reasoning is asymmetric on purpose: a lease stops the framework from
	// retiring something. A slot that wrongly accepts one holds an entity alive for one extra lease
	// duration; a slot that wrongly refuses one destroys an entity a mission was still using.
	return true;
}

ATerritorySpawnAnchor* UTerritorySpawnPolicy::GetOwningAnchor() const
{
	// Walk the outer chain rather than caching: an instanced policy can be re-outered when an anchor
	// is duplicated or when a definition asset is re-instanced, and a cached pointer would go stale.
	for (const UObject* Candidate = GetOuter(); Candidate; Candidate = Candidate->GetOuter())
	{
		if (const ATerritorySpawnAnchor* Anchor = Cast<const ATerritorySpawnAnchor>(Candidate))
		{
			return const_cast<ATerritorySpawnAnchor*>(Anchor);
		}
	}
	return nullptr;
}

bool UTerritorySpawnPolicy::IsPolicyConfigured() const
{
	// The minimum a policy must supply for the framework to do anything useful: an identity to bucket
	// and report, and an executor to act through. Everything else is the subclass's business.
	return GetEntityTag().IsValid() && GetExecutorClass() != nullptr;
}

double UTerritorySpawnPolicy::GetNowGameTime() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

bool UTerritorySpawnPolicy::IsAnyPlayerNearby(const FVector& Location, float RadiusCm) const
{
	const UWorld* const World = GetWorld();
	if (!World) return false;

	const double RadiusSquared = static_cast<double>(RadiusCm) * static_cast<double>(RadiusCm);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* const PC = It->Get();
		if (!PC) continue;

		// A controller with no pawn is a player mid-respawn or sitting in a menu. Neither of them is
		// standing next to the vehicle, so neither should keep it alive.
		const APawn* const Pawn = PC->GetPawn();
		if (!IsValid(Pawn)) continue;

		if (FVector::DistSquared(Pawn->GetActorLocation(), Location) <= RadiusSquared)
		{
			return true;
		}
	}

	return false;
}
