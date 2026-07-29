#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/World.h"
#include "Components/BillboardComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "SaveSystemStatics.h"

#if WITH_EDITOR
#include "Components/ArrowComponent.h"
#endif

ATerritoryGuardSpawnPoint::ATerritoryGuardSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Visual root — billboard in editor, invisible in game
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;
	Billboard->bHiddenInGame = true;
	Billboard->SetHiddenInGame(true, true);

	CurrentReserveCount = 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// P0-06: INarrativeSavableActor — persist reserve state across save/load
// ═══════════════════════════════════════════════════════════════════════════════

FGuid ATerritoryGuardSpawnPoint::GetActorGUID_Implementation() const
{
	return SpawnPointGUID;
}

void ATerritoryGuardSpawnPoint::SetActorGUID_Implementation(const FGuid& InGUID)
{
	SpawnPointGUID = InGUID;
}

void ATerritoryGuardSpawnPoint::PrepareForSave_Implementation()
{
	// SaveGame UPROPERTYs auto-serialized: CurrentReserveCount, PendingReserveSpawns, SavedActiveGuardCount
	SavedActiveGuardCount = GetActiveGuardCount();
}

void ATerritoryGuardSpawnPoint::Load_Implementation()
{
	// Mark that we loaded from save — prevents InitializeReserves from resetting to full
	bLoadedFromSave = true;
}

bool ATerritoryGuardSpawnPoint::ShouldRespawn_Implementation() const
{
	// Spawn points are level-placed, not dynamically spawned — don't respawn
	return false;
}

void ATerritoryGuardSpawnPoint::EnsurePersistentSpawnPointGUID()
{
	if (!SpawnPointGUID.IsValid())
	{
		SpawnPointGUID = FGuid::NewGuid();
#if WITH_EDITOR
		MarkPackageDirty();
#endif
	}
}

#if WITH_EDITOR
void ATerritoryGuardSpawnPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EnsurePersistentSpawnPointGUID();
}

void ATerritoryGuardSpawnPoint::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	// Invalidate GUID on duplicate — must be re-baked
	SpawnPointGUID.Invalidate();
	EnsurePersistentSpawnPointGUID();
}
#endif

void ATerritoryGuardSpawnPoint::BindToTerritory(ATerritoryVolume* Territory)
{
	if (!Territory) return;

	const FGameplayTag TerritoryTag = Territory->GetTerritoryTag();
	if (OwnerTerritoryTag.IsValid() && OwnerTerritoryTag != TerritoryTag)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("GuardSpawnPoint %s owner tag %s overridden by territory reference %s"),
			*GetName(), *OwnerTerritoryTag.ToString(), *TerritoryTag.ToString());
	}

	CachedTerritory = Territory;
	OwnerTerritoryTag = TerritoryTag;

	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
		}
	}
}

void ATerritoryGuardSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	// P0-06: Ensure GUID is baked; load persisted reserve state if available
	EnsurePersistentSpawnPointGUID();
	if (HasAuthority())
	{
		USaveSystemStatics::LoadSingleActor(this);
	}

	ResolveOwningTerritory();
	InitializeReserves();

	// Untagged points keep listening so a more-specific streamed territory can replace
	// an earlier proximity match (for example, Property replacing City).
	if (!OwnerTerritoryTag.IsValid() || !CachedTerritory.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				Registry->OnTerritoryRegistered.AddDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
			}
		}
	}
}

void ATerritoryGuardSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReserveSpawnTimer);
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ATerritoryGuardSpawnPoint::OnTerritoryRegistered(ATerritoryVolume* Territory, bool bIsNew)
{
	if (!Territory) return;

	if (OwnerTerritoryTag.IsValid())
	{
		if (!CachedTerritory.IsValid() && Territory->GetTerritoryTag() == OwnerTerritoryTag)
		{
			CachedTerritory = Territory;
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s late-bound to territory %s"),
				*GetName(), *OwnerTerritoryTag.ToString());

			if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				Registry->OnTerritoryRegistered.RemoveDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
			}
		}
		return;
	}

	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		ATerritoryVolume* ResolvedTerritory = Registry->GetTerritoryAtLocation(GetActorLocation());
		if (ResolvedTerritory && CachedTerritory.Get() != ResolvedTerritory)
		{
			CachedTerritory = ResolvedTerritory;
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s proximity-bound to territory %s"),
				*GetName(), *ResolvedTerritory->GetTerritoryTag().ToString());
		}
	}
}

#if WITH_EDITOR
void ATerritoryGuardSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Editor visualization is handled by the visual properties
}
#endif

void ATerritoryGuardSpawnPoint::ResolveOwningTerritory()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	// Explicit tags must not fall back to a different territory while their target streams in.
	if (OwnerTerritoryTag.IsValid())
	{
		CachedTerritory = Registry->GetTerritoryByTag(OwnerTerritoryTag);
	}
	else
	{
		CachedTerritory = Registry->GetTerritoryAtLocation(GetActorLocation());
	}

	if (CachedTerritory.IsValid())
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s resolved to territory %s"),
			*GetName(), *CachedTerritory->GetTerritoryTag().ToString());
	}
	else
	{
		UE_LOG(LogTerritory, Warning, TEXT("GuardSpawnPoint %s could not find owning territory"),
			*GetName());
	}
}

void ATerritoryGuardSpawnPoint::InitializeReserves()
{
	// P1-02: Clear old timer FIRST, before any scheduling
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReserveSpawnTimer);
	}

	// P0-06: If loaded from save, preserve the saved reserve state.
	// Only initialize to full on fresh (non-save) start.
	if (bLoadedFromSave)
	{
		// P1-08: Resume pending reserve deployment timer if reserves were pending at save time
		if (bAutoSpawnReserves && PendingReserveSpawns > 0 && CurrentReserveCount > 0)
		{
			const float EffectiveDelay = (GuardPostDefinition && GuardPostDefinition->ReserveSpawnDelay > 0.f)
				? GuardPostDefinition->ReserveSpawnDelay : ReserveSpawnDelay;
			ScheduleAutomaticReserveSpawn(EffectiveDelay);
		}
	}
	else if (CurrentReserveCount <= 0 && SavedActiveGuardCount <= 0)
	{
		// P2-N15: Read GuardPostDefinition overrides if assigned
		const int32 EffectiveReserveSlots = (GuardPostDefinition && GuardPostDefinition->ReserveSlots > 0)
			? GuardPostDefinition->ReserveSlots : ReserveSlots;
		CurrentReserveCount = EffectiveReserveSlots;
	}
	PendingReserveSpawns = FMath::Max(PendingReserveSpawns, 0);
}

FTransform ATerritoryGuardSpawnPoint::GetSpawnTransform() const
{
	FTransform Transform = GetActorTransform();

	// Project spawn location to NavMesh so guards start on walkable ground
	FVector SpawnLoc = Transform.GetLocation();
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLoc;
		if (NavSys->ProjectPointToNavigation(SpawnLoc, ProjectedLoc, FVector(500.f, 500.f, 500.f)))
		{
			Transform.SetLocation(ProjectedLoc.Location);
		}
	}

	return Transform;
}

bool ATerritoryGuardSpawnPoint::HasAvailableSlot() const
{
	return GetActiveGuardCount() < MaxGuards;
}

bool ATerritoryGuardSpawnPoint::HasReserveAvailable() const
{
	return CurrentReserveCount > 0;
}

bool ATerritoryGuardSpawnPoint::HasPendingReserveSpawn() const
{
	return PendingReserveSpawns > 0 && CurrentReserveCount > 0;
}

int32 ATerritoryGuardSpawnPoint::GetActiveGuardCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr : ActiveGuards)
	{
		if (Ptr.IsValid()) ++Count;
	}
	return Count;
}

int32 ATerritoryGuardSpawnPoint::GetReserveCount() const
{
	return CurrentReserveCount;
}

void ATerritoryGuardSpawnPoint::RegisterSpawnedGuard(ATerritoryGuardCharacter* Guard)
{
	if (!HasAuthority() || !Guard) return;
	ActiveGuards.AddUnique(Guard);
	SavedActiveGuardCount = ActiveGuards.Num();
}

void ATerritoryGuardSpawnPoint::UnregisterGuard(ATerritoryGuardCharacter* Guard, EGuardRemovalReason Reason)
{
	if (!HasAuthority() || !Guard) return;

	// CRITICAL FIX: Only process if this spawn point actually tracked this guard.
	// Without this check, a guard dying at SP_1 would also drain reserves at SP_2, SP_3, etc.
	bool bWasTracked = false;
	for (const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr : ActiveGuards)
	{
		if (Ptr.Get() == Guard)
		{
			bWasTracked = true;
			break;
		}
	}

	if (!bWasTracked) return; // Not our guard — no-op

	ActiveGuards.RemoveAll([Guard](const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Guard;
	});
	// P1-N11: Keep saved count in sync after removal so subsequent queries
	// (e.g. delta calculations in economy tick) see the correct active count.
	SavedActiveGuardCount = ActiveGuards.Num();

	// P1-04: Only queue reserve replacement when guard was killed.
	// Manual removal, ownership change, load reconcile, and territory destruction
	// should not consume reserves.
	if (Reason == EGuardRemovalReason::Killed && CurrentReserveCount > PendingReserveSpawns)
	{
		QueueReserveSpawn();
	}
	else if (Reason == EGuardRemovalReason::Killed)
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: guard died, no uncommitted reserves"),
			*GetName());
	}
}

void ATerritoryGuardSpawnPoint::QueueReserveSpawn()
{
	const int32 FreeSlots = FMath::Max(0, MaxGuards - GetActiveGuardCount());
	const int32 MaximumPending = FMath::Min(CurrentReserveCount, FreeSlots);
	if (PendingReserveSpawns >= MaximumPending)
	{
		return;
	}

	++PendingReserveSpawns;
	UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: queued reserve deployment (%d pending, %d available)"),
		*GetName(), PendingReserveSpawns, CurrentReserveCount);

	if (bAutoSpawnReserves)
	{
		// P2-N15: GuardPostDefinition override for ReserveSpawnDelay
		const float EffectiveDelay = (GuardPostDefinition && GuardPostDefinition->ReserveSpawnDelay > 0.f)
			? GuardPostDefinition->ReserveSpawnDelay : ReserveSpawnDelay;
		ScheduleAutomaticReserveSpawn(EffectiveDelay);
	}
}

void ATerritoryGuardSpawnPoint::ScheduleAutomaticReserveSpawn(float Delay)
{
	UWorld* World = GetWorld();
	if (!World || !HasPendingReserveSpawn() || !bAutoSpawnReserves)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(ReserveSpawnTimer);
	World->GetTimerManager().SetTimer(
		ReserveSpawnTimer,
		this,
		&ATerritoryGuardSpawnPoint::TryAutomaticReserveSpawn,
		FMath::Max(0.1f, Delay),
		false);
}

void ATerritoryGuardSpawnPoint::TryAutomaticReserveSpawn()
{
	if (!bAutoSpawnReserves || !HasPendingReserveSpawn())
	{
		return;
	}

	ATerritoryVolume* Territory = GetOwningTerritory();
	if (!Territory || Territory->GetTerritoryState() != ETerritoryState::Claimed)
	{
		CancelPendingReserveSpawns();
		return;
	}

	if (TrySpawnReserveGuard(true))
	{
		ScheduleAutomaticReserveSpawn(ReserveSpawnDelay);
	}
	else
	{
		ScheduleAutomaticReserveSpawn(ReserveSpawnRetryInterval);
	}
}

bool ATerritoryGuardSpawnPoint::SpawnReserveGuard()
{
	return TrySpawnReserveGuard(false);
}

bool ATerritoryGuardSpawnPoint::TrySpawnReserveGuard(bool bRequireCameraAvoidance)
{
	if (!HasAuthority() || CurrentReserveCount <= 0 || !HasAvailableSlot())
	{
		return false;
	}

	ATerritoryVolume* Territory = GetOwningTerritory();
	if (!Territory || Territory->GetTerritoryState() != ETerritoryState::Claimed
		|| !Territory->TrySpawnSingleGuard(this, bRequireCameraAvoidance))
	{
		return false;
	}

	// P1-N12: Decrement happens AFTER TrySpawnSingleGuard returns true, so the
	// reserve is only consumed when a guard is actually placed in the world.
	// This order is correct — do not move the decrement above the spawn call.
	--CurrentReserveCount;
	PendingReserveSpawns = FMath::Max(0, PendingReserveSpawns - 1);
	UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: deployed one reserve (%d remaining, %d pending)"),
		*GetName(), CurrentReserveCount, PendingReserveSpawns);

	if (bAutoSpawnReserves && HasPendingReserveSpawn())
	{
		ScheduleAutomaticReserveSpawn(ReserveSpawnDelay);
	}
	return true;
}

void ATerritoryGuardSpawnPoint::CancelPendingReserveSpawns()
{
	PendingReserveSpawns = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReserveSpawnTimer);
	}
}

void ATerritoryGuardSpawnPoint::ResetReserveState()
{
	// P1-09: Respect reserve ownership policy on ownership change
	switch (ReserveOwnershipPolicy)
	{
	case EReserveOwnershipPolicy::PersistWithPost:
		// Keep current reserve state — don't reset
		break;
	case EReserveOwnershipPolicy::ResetToDefinitionOnOwnerChange:
		CurrentReserveCount = GetEffectiveReserveSlots();
		break;
	case EReserveOwnershipPolicy::RefillOnOwnerChange:
	default:
		InitializeReserves();
		break;
	}
	ActiveGuards.Reset();
}

TArray<FTerritoryPatrolNode> ATerritoryGuardSpawnPoint::GetPatrolRoute() const
{
	return PatrolRoute;
}

ATerritoryVolume* ATerritoryGuardSpawnPoint::GetOwningTerritory() const
{
	return CachedTerritory.IsValid() ? CachedTerritory.Get() : nullptr;
}

bool ATerritoryGuardSpawnPoint::HasPatrolRoute() const
{
	return PatrolRoute.Num() >= 2;
}

TArray<FTransform> ATerritoryGuardSpawnPoint::GetPatrolRouteAsTransforms() const
{
	TArray<FTransform> Transforms;
	Transforms.Reserve(PatrolRoute.Num());
	for (const FTerritoryPatrolNode& Node : PatrolRoute)
	{
		Transforms.Add(FTransform(Node.Rotation, Node.Location));
	}
	return Transforms;
}

TArray<float> ATerritoryGuardSpawnPoint::GetPatrolWaitTimes() const
{
	TArray<float> Times;
	Times.Reserve(PatrolRoute.Num());
	for (const FTerritoryPatrolNode& Node : PatrolRoute)
	{
		Times.Add(Node.WaitTime);
	}
	return Times;
}

// ═══════════════════════════════════════════════════════════════════════════════
// P1-07: Effective Configuration Getters
// GuardPostDefinition overrides inline values when assigned.
// ═══════════════════════════════════════════════════════════════════════════════

int32 ATerritoryGuardSpawnPoint::GetEffectiveMaxGuards() const
{
	return (GuardPostDefinition && GuardPostDefinition->MaxGuards > 0)
		? GuardPostDefinition->MaxGuards : MaxGuards;
}

int32 ATerritoryGuardSpawnPoint::GetEffectiveReserveSlots() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveSlots >= 0)
		? GuardPostDefinition->ReserveSlots : ReserveSlots;
}

float ATerritoryGuardSpawnPoint::GetEffectiveReserveSpawnDelay() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveSpawnDelay > 0.f)
		? GuardPostDefinition->ReserveSpawnDelay : ReserveSpawnDelay;
}

float ATerritoryGuardSpawnPoint::GetEffectiveReserveRetryInterval() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveSpawnRetryInterval > 0.f)
		? GuardPostDefinition->ReserveSpawnRetryInterval : ReserveSpawnRetryInterval;
}

float ATerritoryGuardSpawnPoint::GetEffectiveReserveRadius() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveSpawnRadius > 0.f)
		? GuardPostDefinition->ReserveSpawnRadius : ReserveSpawnRadius;
}

float ATerritoryGuardSpawnPoint::GetEffectiveMinimumPlayerDistance() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveMinimumPlayerDistance >= 0.f)
		? GuardPostDefinition->ReserveMinimumPlayerDistance : ReserveMinimumPlayerDistance;
}

int32 ATerritoryGuardSpawnPoint::GetEffectiveCandidateCount() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveSpawnCandidateCount > 0)
		? GuardPostDefinition->ReserveSpawnCandidateCount : ReserveSpawnCandidateCount;
}

FGameplayTag ATerritoryGuardSpawnPoint::GetEffectiveFactionOverride() const
{
	if (FactionOverride.IsValid()) return FactionOverride;
	if (GuardPostDefinition && GuardPostDefinition->FactionOverride.IsValid())
		return GuardPostDefinition->FactionOverride;
	return FGameplayTag();
}
