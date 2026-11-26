#include "Spawn/TerritorySpawnExecutorActor.h"

#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

namespace TerritorySpawnExecutorActorPrivate
{
	/** Applied after a HideThenDestroy retire, matching the project's existing teardown grace. */
	constexpr float HideThenDestroyLifeSpanSeconds = 0.75f;
}

UTerritorySpawnExecutorActor::UTerritorySpawnExecutorActor()
{
}

bool UTerritorySpawnExecutorActor::SupportsActorClass_Implementation(TSubclassOf<AActor> ActorClass) const
{
	if (!Super::SupportsActorClass_Implementation(ActorClass))
	{
		return false;
	}

	// Narrative characters are excluded here, not in the caller. Their removal has to go through
	// TerritoryNarrativeDeathSupport::ScheduleRemoval so the ASC, the activity and the save record are
	// torn down in the right order; a plain Destroy leaves a corpse the rest of the project still
	// believes in. Returning false makes the anchor refuse the class at BeginPlay and makes the editor
	// validator say so, which is louder than a runtime assert nobody reads.
	return !ActorClass->IsChildOf(ANarrativeNPCCharacter::StaticClass());
}

AActor* UTerritorySpawnExecutorActor::CreateSpawned_Implementation(ATerritorySpawnAnchor* Anchor,
	TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
	UWorld* const World = GetWorld();
	if (!World || !ActorClass)
	{
		return nullptr;
	}

	// Server only. A client that reached this point would create an actor the server does not know
	// about, and the next replication pass would delete it again — with a duplicate log on the way.
	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	FTransform SpawnTransform = Transform;
	if (!ResolveSpawnTransform(Transform, SpawnTransform))
	{
		// Projection was asked for and found nothing. Report it as a failed attempt; do not silently
		// spawn at the unprojected transform, which is the failure mode the flag exists to prevent.
		if (ShouldLogTerritorySpawn())
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[Spawn] %s found no navigation within %.0fcm below %s; no spawn was attempted."),
				*GetNameSafe(Anchor), NavigationProjectionExtent, *Transform.GetLocation().ToString());
		}
		return nullptr;
	}

	FActorSpawnParameters Params;
	// Owner is deliberately left unset: a parked actor that carries an owner gets a net connection,
	// which changes which RPCs and owner-only properties reach which client. Identity for this
	// framework is the anchor GUID, never an actor owner.
	Params.Owner = nullptr;
	Params.Instigator = nullptr;
	Params.SpawnCollisionHandlingOverride = bSpawnEvenIfBlocked
		? ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
		: ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	ConfigureSpawnParameters(Params);

	AActor* const Created = World->SpawnActor<AActor>(ActorClass, SpawnTransform, Params);
	if (!Created)
	{
		// Blocked placement is a normal, expected outcome: the framework counts it and retries after
		// the policy's delay, and the record keeps saying "this slot should hold something".
		if (ShouldLogTerritorySpawn())
		{
			UE_LOG(LogTerritory, Verbose,
				TEXT("[Spawn] %s could not place %s at %s; collision handling refused. No relocation was attempted."),
				*GetNameSafe(Anchor), *GetNameSafe(ActorClass), *SpawnTransform.GetLocation().ToString());
		}
		return nullptr;
	}

	ConfigureSpawnedActor(Created);

	// ─── AND THAT IS THE WHOLE SPAWN ───
	//
	// The actor was created at the authored transform and handed to its family's ConfigureSpawnedActor.
	// Nothing here moves it, hides it, wakes its physics, or changes its replication rate, and the
	// absence of those steps is the design rather than an omission.
	//
	// A previous version of this function snapped the actor to the ground, re-snapped it one tick later,
	// then hid it until its physics settled and forced physics on so the settle could be measured. Each
	// step had a measurement behind it and the combination did not work: spawn-frame colliding bounds
	// are degenerate on a skeletal vehicle, the corrected bounds change again once the suspension loads,
	// and the height the settle watcher finally froze on was the height produced by the previous
	// teleport. The car the user drove could not be driven the way the same Blueprint placed by hand
	// could, and that was the honest description of the defect.
	//
	// A designer places an actor at the transform it belongs at. A bay is authored the same way. If an
	// authored Z is wrong, that is visible in the editor and fixed by the person who can see it.

	return Created;
}

void UTerritorySpawnExecutorActor::RetireSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor,
	ESpawnRetireMode Mode)
{
	if (!IsValid(Actor))
	{
		return;
	}

	switch (Mode)
	{
	case ESpawnRetireMode::HideThenDestroy:
		// Matches the project's existing campaign-restore teardown: collision off first so nothing
		// trips over an actor that is about to vanish, then a short life span so replication catches
		// up before the actor is gone.
		Actor->SetActorEnableCollision(false);
		Actor->SetActorHiddenInGame(true);
		Actor->SetLifeSpan(TerritorySpawnExecutorActorPrivate::HideThenDestroyLifeSpanSeconds);
		Actor->ForceNetUpdate();
		return;

	case ESpawnRetireMode::SuspendForStreaming:
		// Suspension is supposed to go through SuspendSpawned, which keeps the actor. A policy that
		// asked for a streaming suspend with an actor it also wants destroyed gets the destroy, because
		// keeping an unreferenced actor alive is worse than the mode mismatch.
		Actor->Destroy();
		return;

	case ESpawnRetireMode::ReleaseToWorld:
		// Hand the actor over: strip the framework's tracking without destroying. The caller owns it
		// from here, and the anchor's record will say it has no instance.
		return;

	case ESpawnRetireMode::Destroy:
	default:
		Actor->Destroy();
		return;
	}
}

bool UTerritorySpawnExecutorActor::SuspendSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	// Hide and disable, but keep the actor. Suspension exists for streaming and out-of-world parking,
	// where the durable identity must survive the actor going quiet.
	Actor->SetActorEnableCollision(false);
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorTickEnabled(false);
	Actor->ForceNetUpdate();
	return true;
}

bool UTerritorySpawnExecutorActor::ResumeSpawned_Implementation(ATerritorySpawnAnchor* Anchor, AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
	Actor->SetActorTickEnabled(true);
	Actor->ForceNetUpdate();
	return true;
}

bool UTerritorySpawnExecutorActor::IsClaimedByExternalOwner_Implementation(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		// Nothing to protect. Answering true here would make a slot with a stale pointer unfixable,
		// so "no actor" is the one case where false is safe: there is nothing to destroy.
		return false;
	}

	// A player-controlled pawn is owned by somebody by definition. This is the generic form of the
	// vehicle occupancy rule; the vehicle executor overrides it with the project's own predicate.
	if (const APawn* const Pawn = Cast<APawn>(Actor))
	{
		if (Pawn->GetController() != nullptr)
		{
			return true;
		}
	}

	// A plain, unpossessed actor is NOT claimed. This deliberately does not fall through to the base
	// answer: the base says "unknown must answer true", which is correct for an abstract executor that
	// genuinely cannot know, but this executor CAN know. Deferring here made every prop, pickup and
	// unoccupied vehicle permanently un-retirable, so the safety interlock vetoed every retire the
	// framework ever asked for — silently, because a veto looks exactly like "nothing to do" in a log.
	return false;
}

bool UTerritorySpawnExecutorActor::ResolveSpawnTransform(const FTransform& Wanted, FTransform& OutTransform) const
{
	OutTransform = Wanted;

	if (!bProjectToNavigation)
	{
		return true;
	}

	UWorld* const World = GetWorld();
	UNavigationSystemV1* const NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSystem)
	{
		// Projection was requested and the world has no navigation. Failing loudly is correct: the
		// alternative is spawning at the authored Z and letting the entity clip into the floor.
		return false;
	}

	FNavLocation Projected;
	const FVector QueryExtent(10.f, 10.f, NavigationProjectionExtent);
	if (!NavSystem->ProjectPointToNavigation(Wanted.GetLocation(), Projected, QueryExtent))
	{
		return false;
	}

	// Z only. The authored X/Y survives exactly, because nudging a prop sideways to reach walkable
	// ground is the same silent relocation the framework forbids in its placement rules.
	FVector NewLocation = Wanted.GetLocation();
	NewLocation.Z = Projected.Location.Z;
	OutTransform.SetLocation(NewLocation);
	return true;
}

void UTerritorySpawnExecutorActor::ConfigureSpawnParameters(FActorSpawnParameters& Params) const
{
	// Base does nothing: the collision handling set by CreateSpawned is the whole policy for a plain
	// prop. A family that needs RF_Transient or a different name overrides this.
}

void UTerritorySpawnExecutorActor::ConfigureSpawnedActor(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	if (NetCullDistanceOverrideCm > 0.f)
	{
		const float Squared = NetCullDistanceOverrideCm * NetCullDistanceOverrideCm;
		Actor->SetNetCullDistanceSquared(Squared);
	}
}
