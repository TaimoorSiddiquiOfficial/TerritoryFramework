#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/World.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
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
	SavedActiveGuardCount = FMath::Clamp(GetActiveGuardCount(), 0, 1);
	PendingReserveSpawns = FMath::Clamp(PendingReserveSpawns, 0, 1);
}

void ATerritoryGuardSpawnPoint::Load_Implementation()
{
	if (SavedActiveGuardCount > 1 || PendingReserveSpawns > 1)
	{
		UE_LOG(LogTerritory, Log,
			TEXT("GuardSpawnPoint %s migrated legacy multi-slot save state to one active combat slot"),
			*GetPathName());
	}
	SavedActiveGuardCount = FMath::Clamp(SavedActiveGuardCount, 0, 1);
	PendingReserveSpawns = FMath::Clamp(PendingReserveSpawns, 0, 1);
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
	// PIE/world duplication must preserve the editor-authored persistent identity.
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		SpawnPointGUID = FGuid::NewGuid();
		MarkPackageDirty();
	}
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

	SetResolvedTerritory(Territory);
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

	// Persistent identities must be editor-authored. A runtime-generated GUID would
	// silently orphan saved reserve records on every campaign load.
	if (HasAuthority())
	{
		if (!SpawnPointGUID.IsValid())
		{
			UE_LOG(LogTerritory, Error,
				TEXT("GuardSpawnPoint %s has no editor-baked SpawnPointGUID; save/load is disabled for this post"),
				*GetPathName());
		}
		else
		{
			USaveSystemStatics::LoadSingleActor(this);
		}
	}

	ResolveOwningTerritory();
	InitializeReserves();
	if (ATerritoryVolume* Territory = GetOwningTerritory())
	{
		Territory->RefreshGarrisonSnapshot();
	}

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
	if (ATerritoryVolume* Territory = GetOwningTerritory())
	{
		Territory->UnregisterResolvedGuardSpawnPoint(this);
	}
	CachedTerritory.Reset();
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
			SetResolvedTerritory(Territory);
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
		ATerritoryVolume* ResolvedTerritory = FindPlacementOrPatrolTerritory(Registry);
		if (ResolvedTerritory && CachedTerritory.Get() != ResolvedTerritory)
		{
			SetResolvedTerritory(ResolvedTerritory);
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s proximity-bound to territory %s"),
				*GetName(), *ResolvedTerritory->GetTerritoryTag().ToString());
		}
	}
}

ATerritoryVolume* ATerritoryGuardSpawnPoint::ChooseMostSpecificTerritory(
	TConstArrayView<ATerritoryVolume*> Candidates)
{
	ATerritoryVolume* Best = nullptr;
	int32 BestPriority = -1;
	double BestBoundsVolume = TNumericLimits<double>::Max();
	FString BestTag;
	for (ATerritoryVolume* Candidate : Candidates)
	{
		if (!IsValid(Candidate)) continue;
		int32 Priority = 0;
		if (Candidate->IsA<ATerritoryProperty>()) Priority = 3;
		else if (Candidate->IsA<ATerritoryDistrict>()) Priority = 2;
		else if (Candidate->IsA<ATerritoryCity>()) Priority = 1;
		const FVector Size = Candidate->GetTerritoryBounds().GetSize();
		const double BoundsVolume = FMath::Abs(
			static_cast<double>(Size.X) * Size.Y * Size.Z);
		const FString Tag = Candidate->GetTerritoryTag().ToString();
		if (!Best || Priority > BestPriority
			|| (Priority == BestPriority && BoundsVolume < BestBoundsVolume)
			|| (Priority == BestPriority && FMath::IsNearlyEqual(BoundsVolume, BestBoundsVolume)
				&& Tag < BestTag))
		{
			Best = Candidate;
			BestPriority = Priority;
			BestBoundsVolume = BoundsVolume;
			BestTag = Tag;
		}
	}
	return Best;
}

ATerritoryVolume* ATerritoryGuardSpawnPoint::FindPlacementOrPatrolTerritory(
	UTerritoryRegistrySubsystem* Registry) const
{
	if (!Registry) return nullptr;
	TArray<ATerritoryVolume*> Candidates;
	Candidates.Add(Registry->GetTerritoryAtLocation(GetActorLocation()));
	for (const FTerritoryPatrolNode& Node : GetEffectivePatrolRoute())
	{
		Candidates.Add(Registry->GetTerritoryAtLocation(Node.Location));
	}
	return ChooseMostSpecificTerritory(Candidates);
}

#if WITH_EDITOR
void ATerritoryGuardSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GetWorld() && !GetWorld()->IsGameWorld()
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		EnsurePersistentSpawnPointGUID();
	}
}
#endif

void ATerritoryGuardSpawnPoint::SetResolvedTerritory(ATerritoryVolume* Territory)
{
	ATerritoryVolume* PreviousTerritory = CachedTerritory.Get();
	if (PreviousTerritory == Territory)
	{
		if (Territory) Territory->RegisterResolvedGuardSpawnPoint(this);
		return;
	}
	if (PreviousTerritory)
	{
		PreviousTerritory->UnregisterResolvedGuardSpawnPoint(this);
	}
	CachedTerritory = Territory;
	if (Territory)
	{
		Territory->RegisterResolvedGuardSpawnPoint(this);
	}
}

void ATerritoryGuardSpawnPoint::ResolveOwningTerritory()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return;

	// Explicit tags must not fall back to a different territory while their target streams in.
	if (OwnerTerritoryTag.IsValid())
	{
		SetResolvedTerritory(Registry->GetTerritoryByTag(OwnerTerritoryTag));
	}
	else
	{
		SetResolvedTerritory(FindPlacementOrPatrolTerritory(Registry));
	}

	if (CachedTerritory.IsValid())
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s resolved to territory %s"),
			*GetName(), *CachedTerritory->GetTerritoryTag().ToString());
	}
	else
	{
		// A tagged World Partition post can begin play before its Territory actor has
		// registered. It remains subscribed to OnTerritoryRegistered, so this is an
		// expected wait state rather than a broken ownership binding.
		if (OwnerTerritoryTag.IsValid())
		{
			UE_LOG(LogTerritory, Verbose,
				TEXT("GuardSpawnPoint %s is waiting for owning territory %s to register"),
				*GetName(), *OwnerTerritoryTag.ToString());
		}
		else
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("GuardSpawnPoint %s has no owner tag and no registered territory overlapping its placement or patrol route"),
				*GetName());
		}
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
			ScheduleAutomaticReserveSpawn(GetEffectiveReserveSpawnDelay());
		}
	}
	else if (CurrentReserveCount <= 0 && SavedActiveGuardCount <= 0)
	{
		// P2-N15: Read GuardPostDefinition overrides if assigned
		const int32 EffectiveReserveSlots = (GuardPostDefinition && GuardPostDefinition->ReserveSlots > 0)
			? GuardPostDefinition->ReserveSlots : ReserveSlots;
		CurrentReserveCount = EffectiveReserveSlots;
	}
	PendingReserveSpawns = FMath::Clamp(PendingReserveSpawns, 0, 1);
}

FTransform ATerritoryGuardSpawnPoint::GetSpawnTransform() const
{
	return GetActorTransform();
}

bool ATerritoryGuardSpawnPoint::ResolveGuardDeploymentTransform(
	TSubclassOf<ATerritoryGuardCharacter> GuardClass, FTransform& OutTransform) const
{
	if (!GuardClass)
	{
		return false;
	}

	const ATerritoryGuardCharacter* GuardCDO = GuardClass->GetDefaultObject<ATerritoryGuardCharacter>();
	const UCapsuleComponent* Capsule = GuardCDO ? GuardCDO->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return false;
	}

	OutTransform = GetSpawnTransform();
	OutTransform.SetScale3D(FVector::OneVector);
	const FVector MarkerLocation = OutTransform.GetLocation();
	float GroundZ = MarkerLocation.Z;

	// Keep the designer-authored X/Y exact. Navigation is used only to align the
	// character's feet vertically to the local walkable surface.
	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (NavSystem->ProjectPointToNavigation(
			MarkerLocation, ProjectedLocation, FVector(10.f, 10.f, 500.f)))
		{
			GroundZ = ProjectedLocation.Location.Z;
		}
	}

	OutTransform.SetLocation(FVector(
		MarkerLocation.X,
		MarkerLocation.Y,
		GroundZ + Capsule->GetScaledCapsuleHalfHeight() + 2.f));
	return true;
}

bool ATerritoryGuardSpawnPoint::HasAvailableSlot() const
{
	return GetActiveGuardCount() < GetEffectiveMaxGuards();
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
	ActiveGuards.RemoveAllSwap([](const TWeakObjectPtr<ATerritoryGuardCharacter>& Entry)
	{
		return !Entry.IsValid();
	});
	if (ActiveGuards.Contains(Guard)) return;
	if (!HasAvailableSlot())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("GuardSpawnPoint %s rejected a second active guard; each point is one combat slot"),
			*GetPathName());
		return;
	}
	ActiveGuards.AddUnique(Guard);
	SavedActiveGuardCount = FMath::Clamp(ActiveGuards.Num(), 0, 1);
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
	ATerritoryVolume* Territory = GetOwningTerritory();
	const bool bNeedsReplacement = Territory
		&& Territory->GetSpawnedGuardCount() < Territory->GetDesiredGuardCount();
	if (Reason == EGuardRemovalReason::Killed && bNeedsReplacement
		&& CurrentReserveCount > PendingReserveSpawns)
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
	const int32 FreeSlots = FMath::Max(0, GetEffectiveMaxGuards() - GetActiveGuardCount());
	const int32 MaximumPending = FMath::Min(CurrentReserveCount, FreeSlots);
	if (PendingReserveSpawns >= MaximumPending)
	{
		return;
	}

	if (PendingReserveSpawns == 0)
	{
		AutomaticReserveSpawnFailures = 0;
	}
	++PendingReserveSpawns;
	UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: queued reserve deployment (%d pending, %d available)"),
		*GetName(), PendingReserveSpawns, CurrentReserveCount);

	ATerritoryVolume* Territory = GetOwningTerritory();
	if (bAutoSpawnReserves)
	{
		ScheduleAutomaticReserveSpawn(GetEffectiveReserveSpawnDelay());
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
	if (!Territory || !IsOwnerReserveDeploymentStateValid(
		Territory->GetTerritoryState(), Territory->GetOwningFaction(),
		Territory->GetOwnershipData().ContestingFaction))
	{
		CancelPendingReserveSpawns();
		return;
	}

	const bool bRequireCameraAvoidance = AutomaticReserveSpawnFailures
		< FMath::Max(0, ReserveCameraAvoidanceRetryLimit);
	if (TrySpawnReserveGuard(bRequireCameraAvoidance))
	{
		AutomaticReserveSpawnFailures = 0;
		return;
	}

	++AutomaticReserveSpawnFailures;
	if (AutomaticReserveSpawnFailures >= FMath::Max(1, ReserveTotalRetryLimit))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("GuardSpawnPoint %s abandoned %d queued reserve deployment(s) after %d failures; reserves were not consumed."),
			*GetName(), PendingReserveSpawns, AutomaticReserveSpawnFailures);
		CancelPendingReserveSpawns();
		Territory->RefreshGarrisonSnapshot();
		Territory->TryCompleteDefenderDefeat(FTerritoryTransitionContext());
	}
	else
	{
		ScheduleAutomaticReserveSpawn(GetEffectiveReserveRetryInterval());
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
	if (Territory && Territory->GetSpawnedGuardCount() >= Territory->GetDesiredGuardCount())
	{
		CancelPendingReserveSpawns();
		Territory->RefreshGarrisonSnapshot();
		return false;
	}
	if (!Territory || !IsOwnerReserveDeploymentStateValid(
			Territory->GetTerritoryState(), Territory->GetOwningFaction(),
			Territory->GetOwnershipData().ContestingFaction)
		|| !Territory->TrySpawnSingleGuard(this, bRequireCameraAvoidance))
	{
		return false;
	}

	// P1-N12: Decrement happens AFTER TrySpawnSingleGuard returns true, so the
	// reserve is only consumed when a guard is actually placed in the world.
	// This order is correct — do not move the decrement above the spawn call.
	--CurrentReserveCount;
	PendingReserveSpawns = FMath::Max(0, PendingReserveSpawns - 1);
	Territory->RefreshGarrisonSnapshot();
	UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: deployed one reserve (%d remaining, %d pending)"),
		*GetName(), CurrentReserveCount, PendingReserveSpawns);

	if (bAutoSpawnReserves && HasPendingReserveSpawn())
	{
		ScheduleAutomaticReserveSpawn(GetEffectiveReserveSpawnDelay());
	}
	AutomaticReserveSpawnFailures = 0;
	return true;
}

void ATerritoryGuardSpawnPoint::CancelPendingReserveSpawns()
{
	PendingReserveSpawns = 0;
	AutomaticReserveSpawnFailures = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReserveSpawnTimer);
	}
}

void ATerritoryGuardSpawnPoint::HandleOwnershipTransition(EOwnershipTransitionReason Reason)
{
	// P1-03: Only apply reserve ownership policy when ownership actually changes.
	// InitialSpawn and AdminOverride do not trigger the policy.
	if (Reason != EOwnershipTransitionReason::OwnerChanged
		&& Reason != EOwnershipTransitionReason::RevertedToUnclaimed)
	{
		return;
	}

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
		CurrentReserveCount = FMath::Max(0, ReserveSlots);
		break;
	}
	CancelPendingReserveSpawns();
	ActiveGuards.Reset();
	SavedActiveGuardCount = 0;
}

void ATerritoryGuardSpawnPoint::ResetReserveState()
{
	// Initial spawn / load reconcile — always reinitialize reserves to full
	InitializeReserves();
	ActiveGuards.Reset();
}

TArray<FTerritoryPatrolNode> ATerritoryGuardSpawnPoint::GetPatrolRoute() const
{
	return GetEffectivePatrolRoute();
}

ATerritoryVolume* ATerritoryGuardSpawnPoint::GetOwningTerritory() const
{
	return CachedTerritory.IsValid() ? CachedTerritory.Get() : nullptr;
}

bool ATerritoryGuardSpawnPoint::HasPatrolRoute() const
{
	return GetEffectivePatrolRoute().Num() >= 2;
}

TArray<FTransform> ATerritoryGuardSpawnPoint::GetPatrolRouteAsTransforms() const
{
	TArray<FTransform> Transforms;
	const TArray<FTerritoryPatrolNode>& EffectiveRoute = GetEffectivePatrolRoute();
	Transforms.Reserve(EffectiveRoute.Num());
	for (const FTerritoryPatrolNode& Node : EffectiveRoute)
	{
		Transforms.Add(FTransform(Node.Rotation, Node.Location));
	}
	return Transforms;
}

TArray<float> ATerritoryGuardSpawnPoint::GetPatrolWaitTimes() const
{
	TArray<float> Times;
	const TArray<FTerritoryPatrolNode>& EffectiveRoute = GetEffectivePatrolRoute();
	Times.Reserve(EffectiveRoute.Num());
	for (const FTerritoryPatrolNode& Node : EffectiveRoute)
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
	return 1;
}

int32 ATerritoryGuardSpawnPoint::GetEffectiveReserveSlots() const
{
	return (GuardPostDefinition && GuardPostDefinition->ReserveSlots >= 0)
		? GuardPostDefinition->ReserveSlots : ReserveSlots;
}

float ATerritoryGuardSpawnPoint::GetEffectiveReserveSpawnDelay() const
{
	const float AuthoredDelay = (GuardPostDefinition && GuardPostDefinition->ReserveSpawnDelay > 0.f)
		? GuardPostDefinition->ReserveSpawnDelay : ReserveSpawnDelay;
	const ATerritoryVolume* Territory = GetOwningTerritory();
	if (!Territory || Territory->GetTerritoryState() != ETerritoryState::Contested)
	{
		return AuthoredDelay;
	}

	// A faction with deep local influence can mobilize its already-authored finite
	// reserve faster during a live contest. Timing changes, never ownership or force.
	if (const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile())
	{
		if (const FTerritoryFactionAssaultConfig* Force =
			Profile->FindFactionForce(Territory->GetOwningFaction()))
		{
			return UTerritoryCounterAttackSubsystem::CalculateInfluenceAdjustedDelay(
				AuthoredDelay, Force->TerritorialInfluence,
				Profile->MinimumInfluenceTimingScale);
		}
	}
	return AuthoredDelay;
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

const TArray<FTerritoryPatrolNode>& ATerritoryGuardSpawnPoint::GetEffectivePatrolRoute() const
{
	if (!PatrolRoute.IsEmpty()) return PatrolRoute;
	if (GuardPostDefinition && !GuardPostDefinition->PatrolRoute.IsEmpty())
		return GuardPostDefinition->PatrolRoute;
	static const TArray<FTerritoryPatrolNode> Empty;
	return Empty;
}

bool ATerritoryGuardSpawnPoint::GetEffectiveLoopPatrol() const
{
	// The loop policy follows the route authority. An inline route owns its inline
	// loop flag; a route inherited from a GuardPostDefinition owns the asset flag.
	if (!PatrolRoute.IsEmpty()) return bLoopPatrol;
	if (GuardPostDefinition && !GuardPostDefinition->PatrolRoute.IsEmpty())
		return GuardPostDefinition->bLoopPatrol;
	return bLoopPatrol;
}

bool ATerritoryGuardSpawnPoint::IsOwnerReserveDeploymentStateValid(
	ETerritoryState State, const FGameplayTag& OwningFaction,
	const FGameplayTag& ContestingFaction)
{
	if (!OwningFaction.IsValid()) return false;
	if (State == ETerritoryState::Claimed) return true;
	return State == ETerritoryState::Contested
		&& ContestingFaction.IsValid()
		&& ContestingFaction != OwningFaction;
}
