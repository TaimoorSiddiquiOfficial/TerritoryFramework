#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritorySaveSerialization.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryStealthProfile.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Navigation/TerritoryPatrolRoute.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "SaveSystemStatics.h"

namespace
{
	bool ShouldLogGuardSpawning()
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		return Settings && Settings->ShouldDebugGuards();
	}

	bool ShouldLogGuardSaveLoad()
	{
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		return Settings && Settings->ShouldDebugSaveLoad();
	}
}

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

void ATerritoryGuardSpawnPoint::Serialize(FArchive& Ar)
{
	FTerritorySaveSerializationScope SaveScope(*this, Ar);
	Super::Serialize(Ar);
}

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
	if (!HasAuthority()) return;
	// SaveGame UPROPERTYs auto-serialized: CurrentReserveCount, PendingReserveSpawns, SavedActiveGuardCount
	// A Place retires its physical guards when its cell leaves. Posts in other
	// cells must keep the saved living slots until that owner returns, including
	// when Native saves the still-loaded posts again in the meantime.
	if (!bRetiringForStreaming && !IsWaitingForOwningTerritory())
	{
		SavedActiveGuardCount = FMath::Clamp(GetActiveGuardCount(), 0, 1);
	}
	PendingReserveSpawns = FMath::Clamp(PendingReserveSpawns, 0, 1);
	ReserveTimingVersion = 1;
	if (GetWorld() && HasPendingReserveSpawn())
	{
		const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(ReserveSpawnTimer);
		if (Remaining >= 0.f) SavedReserveRemainingDelay = Remaining;
	}
	if (!HasPendingReserveSpawn()) SavedReserveRemainingDelay = -1.f;
}

void ATerritoryGuardSpawnPoint::Load_Implementation()
{
	if (!HasAuthority()) return;
	ForgetRecentGuardDamage();
	bLoadedFromSave = true;
	if (SavedActiveGuardCount > 1 || PendingReserveSpawns > 1)
	{
		if (ShouldLogGuardSaveLoad())
		{
			UE_LOG(LogTerritory, Log,
				TEXT("[SaveLoad] GuardSpawnPoint %s migrated legacy multi-slot save state to one active combat slot"),
				*GetPathName());
		}
	}
	SavedActiveGuardCount = FMath::Clamp(SavedActiveGuardCount, 0, 1);
	PendingReserveSpawns = FMath::Clamp(PendingReserveSpawns, 0, 1);
	CurrentReserveCount = FMath::Max(0, CurrentReserveCount);
	PendingReserveSpawns = FMath::Min(PendingReserveSpawns, CurrentReserveCount);
	// Loaded zero is an exhausted reserve, never a request for fresh provisioning.
	bReserveStateInitialized = true;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReserveSpawnTimer);
	if (ReserveTimingVersion == 0 || !FMath::IsFinite(SavedReserveRemainingDelay))
	{
		SavedReserveRemainingDelay = -1.f;
	}
	if (HasActorBegunPlay())
	{
		// Resolve itself can reconcile through RegisterResolvedGuardSpawnPoint.
		// Suppress that callback until the Native record-read lifetime is guarded.
		if (ATerritoryVolume* Territory = GetOwningTerritory())
		{
			TGuardValue<bool> RecordRead(Territory->bDeferGuardDestructionForRecordLoad, true);
			ResolveOwningTerritory();
		}
		else ResolveOwningTerritory();
		if (ATerritoryVolume* Territory = GetOwningTerritory(); Territory && !Territory->bReconcilingGuardRestore)
		{
			TGuardValue<bool> RecordRead(Territory->bDeferGuardDestructionForRecordLoad, true);
			Territory->ReconcileRestoredGuardPost(this);
		}
		ResumePendingReserveSpawn();
	}
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
	if (!Territory->IsA<ATerritoryProperty>())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("GuardSpawnPoint %s rejected aggregate territory %s. Guard posts belong to Place Definitions only."),
			*GetPathName(), *Territory->GetTerritoryTag().ToString());
		return;
	}

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
	bRetiringForStreaming = false;
	Super::BeginPlay();
	if (!ApplyTerritoryDefinition())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Guard Spawn Point %s has no matching Territory Definition row. Legacy Blueprint configuration is disabled."),
			*GetPathName());
		return;
	}

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

	// Even a resolved tagged owner can unload independently and return as a new
	// actor. Keep the registry subscription for the post's entire play lifetime.
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.AddUniqueDynamic(this, &ATerritoryGuardSpawnPoint::OnTerritoryRegistered);
		}
	}
}

void ATerritoryGuardSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bRetiringForStreaming = EndPlayReason == EEndPlayReason::RemovedFromWorld;
	ForgetRecentGuardDamage();
	PauseReserveSpawn();
	if (ATerritoryVolume* Territory = GetOwningTerritory())
	{
		Territory->RetireGuards(EndPlayReason == EEndPlayReason::RemovedFromWorld
			? EGuardRemovalReason::StreamingRetirement : EGuardRemovalReason::ManualRemoval, this);
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
		if (CachedTerritory.Get() != Territory && Territory->GetTerritoryTag() == OwnerTerritoryTag)
		{
			SetResolvedTerritory(Territory);
			if (ShouldLogGuardSpawning())
			{
				UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s late-bound to territory %s"),
					*GetName(), *OwnerTerritoryTag.ToString());
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
			if (ShouldLogGuardSpawning())
			{
				UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s proximity-bound to territory %s"),
					*GetName(), *ResolvedTerritory->GetTerritoryTag().ToString());
			}
		}
	}
}

bool ATerritoryGuardSpawnPoint::ApplyTerritoryDefinition()
{
	if (!TerritoryDefinition || !TerritoryDefinition->IsA<UTerritoryPlaceDefinition>())
	{
		return false;
	}
	const FTerritoryGuardPostTemplate* Template =
		TerritoryDefinition->FindGuardPost(GuardPostID);
	if (!Template) return false;

	OwnerTerritoryTag = TerritoryDefinition->TerritoryTag;
	// Copied through unclamped: an undeclared or negative index is an authoring error that
	// UTerritoryDefinition::IsDataValid reports, and the post keeps the floor it names
	// rather than being silently moved to ground.
	FloorIndex = Template->FloorIndex;
	GuardPostDefinition = Template->GuardPostDefinition;
	NPCDefinitionOverride = Template->NPCDefinitionOverride;
	ActivityConfigurationOverride = Template->ActivityConfigurationOverride;
	TriggerSetOverrides = Template->TriggerSetOverrides;
	FactionOverride = Template->FactionOverride;
	Priority = FMath::Max(0, Template->Priority);
	ReserveSlots = FMath::Max(0, Template->ReserveSlots);
	bAutoSpawnReserves = Template->bAutoSpawnReserves;
	ReserveSpawnDelay = FMath::Max(0.1f, Template->ReserveSpawnDelay);
	ReserveSpawnRetryInterval =
		FMath::Max(0.1f, Template->ReserveSpawnRetryInterval);
	ReserveSpawnRadius = FMath::Max(100.f, Template->ReserveSpawnRadius);
	ReserveMinimumPlayerDistance =
		FMath::Max(0.f, Template->ReserveMinimumPlayerDistance);
	ReserveSpawnCandidateCount =
		FMath::Clamp(Template->ReserveSpawnCandidateCount, 1, 64);
	ReserveCameraAvoidanceRetryLimit =
		FMath::Clamp(Template->ReserveCameraAvoidanceRetryLimit, 0, 20);
	ReserveTotalRetryLimit =
		FMath::Clamp(Template->ReserveTotalRetryLimit, 1, 100);
	ReserveOwnershipPolicy = Template->ReserveOwnershipPolicy;
	bLoopPatrol = Template->bLoopPatrol;
	bUseSpawnTransformAsPatrolStop = Template->bUseSpawnTransformAsPatrolStop;
	// The route actor reference is authored on the row and resolved by ID at runtime, so the
	// placed actor still carries no authoring surface of its own. Re-applying a definition also
	// invalidates the previous lookup: the row may now name a different route, or none.
	PatrolRouteID = Template->PatrolRouteID;
	CachedPatrolRouteActor.Reset();
	CachedRouteActorNodes.Reset();
	if (Template->StableGuardPostGUID.IsValid())
	{
		SpawnPointGUID = Template->StableGuardPostGUID;
	}
	// This is a derived copy of the row, including an empty row. Clearing it also restores
	// the nested GuardPostDefinition fallback or the explicitly opted-in spawn stop.
	PatrolRoute.Reset(Template->PatrolRoute.Num());
	if (!Template->PatrolRoute.IsEmpty())
	{
		for (const FTerritoryGuardPatrolTemplateNode& Source : Template->PatrolRoute)
		{
			const FTransform WorldTransform =
				Source.RelativeTransform * GetActorTransform();
			FTerritoryPatrolNode& Node = PatrolRoute.AddDefaulted_GetRef();
			Node.Location = WorldTransform.GetLocation();
			Node.Rotation = WorldTransform.Rotator();
			Node.WaitTime = FMath::Max(0.f, Source.WaitTime);
			Node.ActivityTag = Source.ActivityTag;
			// The spot is node-relative in every field, so it is carried across verbatim: the
			// world-space conversion above applies to the node's own transform, not to the search
			// around it. A new field here without this assignment would silently stay default,
			// which is why the resolver is exercised against a Place row in the tests.
			Node.Spot = Source.Spot;
		}
	}
	return true;
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
		// City and District are aggregate read models. A placement or patrol overlap
		// may pass through their large bounds, but it must never make either parent
		// the physical owner of a guard post.
		if (!IsValid(Candidate) || !Candidate->IsA<ATerritoryProperty>()) continue;
		constexpr int32 Priority = 3;
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
	ApplyTerritoryDefinition();
	if (GetWorld() && !GetWorld()->IsGameWorld()
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		EnsurePersistentSpawnPointGUID();
	}
}
#endif

void ATerritoryGuardSpawnPoint::SetResolvedTerritory(ATerritoryVolume* Territory)
{
	if (Territory && !Territory->IsA<ATerritoryProperty>())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("GuardSpawnPoint %s refused non-Place owner %s"),
			*GetPathName(), *Territory->GetTerritoryTag().ToString());
		Territory = nullptr;
	}
	ATerritoryVolume* PreviousTerritory = CachedTerritory.Get();
	if (PreviousTerritory == Territory)
	{
		if (Territory) Territory->RegisterResolvedGuardSpawnPoint(this);
		ResumePendingReserveSpawn();
		return;
	}
	if (PreviousTerritory)
	{
		PreviousTerritory->UnregisterResolvedGuardSpawnPoint(this);
	}
	ForgetRecentGuardDamage();
	CachedTerritory = Territory;
	if (Territory)
	{
		Territory->RegisterResolvedGuardSpawnPoint(this);
	}
	ResumePendingReserveSpawn();
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
		if (ShouldLogGuardSpawning())
		{
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s resolved to territory %s"),
				*GetName(), *CachedTerritory->GetTerritoryTag().ToString());
		}
	}
	else
	{
		// A tagged World Partition post can begin play before its Territory actor has
		// registered. It remains subscribed to OnTerritoryRegistered, so this is an
		// expected wait state rather than a broken ownership binding.
		if (OwnerTerritoryTag.IsValid())
		{
			if (ShouldLogGuardSpawning())
			{
				UE_LOG(LogTerritory, Log,
					TEXT("GuardSpawnPoint %s is waiting for owning territory %s to register"),
					*GetName(), *OwnerTerritoryTag.ToString());
			}
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
	if (!HasAuthority()) return;
	if (!bReserveStateInitialized)
	{
		CurrentReserveCount = FMath::Max(0, GetEffectiveReserveSlots());
		bReserveStateInitialized = true;
	}
	PendingReserveSpawns = FMath::Clamp(PendingReserveSpawns, 0,
		FMath::Min(1, FMath::Max(0, CurrentReserveCount)));
	ResumePendingReserveSpawn();
}

void ATerritoryGuardSpawnPoint::PauseReserveSpawn()
{
	if (!HasAuthority() || !GetWorld()) return;
	const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(ReserveSpawnTimer);
	if (Remaining >= 0.f && HasPendingReserveSpawn()) SavedReserveRemainingDelay = Remaining;
	GetWorld()->GetTimerManager().ClearTimer(ReserveSpawnTimer);
}

void ATerritoryGuardSpawnPoint::ResumePendingReserveSpawn()
{
	ATerritoryVolume* Territory = GetOwningTerritory();
	if (!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown || IsActorBeingDestroyed()
		|| !HasPendingReserveSpawn() || !bAutoSpawnReserves || !IsValid(Territory)
		|| Territory->bReconcilingGuardRestore || !Territory->IsAvailableForGameplay()
		|| !IsOwnerReserveDeploymentStateValid(Territory->GetTerritoryState(),
			Territory->GetOwningFaction(), Territory->GetOwnershipData().ContestingFaction)
		|| GetWorld()->GetTimerManager().TimerExists(ReserveSpawnTimer)) return;
	ScheduleAutomaticReserveSpawn(SavedReserveRemainingDelay >= 0.f
		? SavedReserveRemainingDelay : GetEffectiveReserveSpawnDelay());
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
		const ATerritoryGuardCharacter* Guard = Ptr.Get();
		if (IsValid(Guard) && !Guard->IsActorBeingDestroyed()) ++Count;
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
	bInitialGuardDeploymentPending = false;
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
	if (Reason != EGuardRemovalReason::LoadReconcile && Reason != EGuardRemovalReason::StreamingRetirement
		&& (Reason != EGuardRemovalReason::ManualRemoval || !IsWaitingForOwningTerritory()))
	{
		SavedActiveGuardCount = ActiveGuards.Num();
	}

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
		if (ShouldLogGuardSpawning())
		{
			UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: guard died, no uncommitted reserves"),
				*GetName());
		}
	}
}

void ATerritoryGuardSpawnPoint::ForgetRecentGuardDamage()
{
	RecentDamageSource.Reset();
	RecentDamageLocation = FVector::ZeroVector;
	RecentDamageOwner = FGameplayTag();
	RecentDamageExpiresAt = 0.0;
}

void ATerritoryGuardSpawnPoint::RememberGuardDamage(
	ATerritoryGuardCharacter* Witness, AActor* DamageSource)
{
	ATerritoryVolume* Territory = GetOwningTerritory();
	if (!HasAuthority() || !IsValid(Witness) || !Witness->HasAuthority()
		|| !ActiveGuards.Contains(Witness) || !IsValid(Territory)
		|| Witness->GetOwningTerritory() != Territory || !GetWorld()
		|| GetWorld()->bIsTearingDown) return;
	ForgetRecentGuardDamage();
	const INarrativeTeamAgentInterface* SourceTeam = Cast<INarrativeTeamAgentInterface>(DamageSource);
	if (!IsValid(DamageSource) || DamageSource == Witness || DamageSource->IsActorBeingDestroyed()
		|| DamageSource->GetWorld() != GetWorld() || !SourceTeam
		|| SourceTeam->GetFactions().IsEmpty() || Witness->GetFactions().HasAnyExact(SourceTeam->GetFactions())) return;
	const UTerritoryStealthProfile* Profile = Territory->GetActiveStealthProfile();
	if (!Profile) Profile = GetDefault<UTerritoryStealthProfile>();
	if (Profile->MaximumInvestigators <= 0 || !FMath::IsFinite(Profile->InvestigationDuration)
		|| Profile->InvestigationDuration < 0.5f || Witness->GetActorLocation().ContainsNaN()) return;
	RecentDamageSource = DamageSource;
	// A real damage callback identifies the source but is not proof the victim
	// saw its position. Reinforce the known hit location, never track the shooter.
	RecentDamageLocation = Witness->GetActorLocation();
	RecentDamageOwner = Territory->GetOwningFaction();
	// A report is immediate tactical context, even if a story profile authors a
	// much longer search. Never keep its live source beyond one minute.
	RecentDamageExpiresAt = GetWorld()->GetTimeSeconds() + FMath::Min(60.f, Profile->InvestigationDuration);
}

bool ATerritoryGuardSpawnPoint::HasRecentGuardDamageFrom(const AActor* DamageSource, const ATerritoryGuardCharacter* Receiver) const
{
	const ATerritoryVolume* Territory = GetOwningTerritory();
	return HasAuthority() && !IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown
		&& IsValid(Territory) && Territory->IsAvailableForGameplay()
		&& Territory->GetOwningFaction() == RecentDamageOwner
		&& IsValid(DamageSource) && !DamageSource->IsActorBeingDestroyed()
		&& RecentDamageSource.Get() == DamageSource
		&& (!Receiver || ActiveGuards.ContainsByPredicate([Receiver](const TWeakObjectPtr<ATerritoryGuardCharacter>& Guard) { return Guard.Get() == Receiver; }))
		&& GetWorld()->GetTimeSeconds() < RecentDamageExpiresAt;
}

bool ATerritoryGuardSpawnPoint::ApplyRecentGuardDamage(ATerritoryGuardCharacter* Guard)
{
	ATerritoryVolume* Territory = GetOwningTerritory();
	if (!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown
		|| !IsValid(Guard) || !Guard->HasAuthority() || !Guard->IsAlive()
		|| Guard->IsActorBeingDestroyed() || !ActiveGuards.Contains(Guard)
		|| !IsValid(Territory) || !Territory->IsAvailableForGameplay()
		|| Guard->GetOwningTerritory() != Territory
		|| Guard->OwningTerritorySpawnPoint != this
		|| !HasRecentGuardDamageFrom(RecentDamageSource.Get())) return false;
	const float Remaining = static_cast<float>(RecentDamageExpiresAt - GetWorld()->GetTimeSeconds());
	if (Remaining < 0.5f || !Guard->CanInvestigateRecentGuardDamage(RecentDamageSource.Get())) return false;
	const UTerritoryStealthProfile* Profile = Territory->GetActiveStealthProfile();
	if (!Profile) Profile = GetDefault<UTerritoryStealthProfile>();
	if (Profile->MaximumInvestigators <= 0 || !FMath::IsFinite(Profile->InvestigationDuration)
		|| Profile->InvestigationDuration < 0.5f) return false;
	return Guard->RequestTerritoryInvestigation(ETerritoryStealthEvidence::Damage,
		RecentDamageLocation, FVector::ZeroVector, RecentDamageSource.Get(), false, *Profile, true, Remaining);
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
	if (ShouldLogGuardSpawning())
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: queued reserve deployment (%d pending, %d available)"),
			*GetName(), PendingReserveSpawns, CurrentReserveCount);
	}

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
	SavedReserveRemainingDelay = FMath::Max(0.01f, Delay);
	ReserveTimingVersion = 1;
	if (IsWaitingForOwningTerritory()) return;
	World->GetTimerManager().SetTimer(
		ReserveSpawnTimer,
		this,
		&ATerritoryGuardSpawnPoint::TryAutomaticReserveSpawn,
		SavedReserveRemainingDelay,
		false);
}

void ATerritoryGuardSpawnPoint::TryAutomaticReserveSpawn()
{
	if (!bAutoSpawnReserves || !HasPendingReserveSpawn())
	{
		return;
	}
	if (IsWaitingForOwningTerritory())
	{
		PauseReserveSpawn();
		return;
	}

	ATerritoryVolume* Territory = GetOwningTerritory();
	if (!Territory || !Territory->IsAvailableForGameplay()
		|| !IsOwnerReserveDeploymentStateValid(
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
		// Floors as they stood while this post still had a queued deployment. Abandoning that
		// deployment can leave the post with nobody alive, which is a real conclusion of the
		// fight rather than a reinforcement gap, so the floor may now be cleared.
		const TArray<FTerritoryFloorSnapshot> FloorsBeforeLoss = Territory->GetGarrisonSnapshot().Floors;
		CancelPendingReserveSpawns();
		Territory->RefreshGarrisonSnapshot();
		Territory->TryCompleteDefenderDefeat(FTerritoryTransitionContext(), FloorsBeforeLoss);
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
	if (!Territory || !Territory->IsAvailableForGameplay()
		|| !IsOwnerReserveDeploymentStateValid(
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
	if (PendingReserveSpawns == 0) SavedReserveRemainingDelay = -1.f;
	Territory->RefreshGarrisonSnapshot();
	// Admission and finite reserve accounting are complete before Narrative can
	// select an activity or run Blueprint callbacks. The weak callback also lets
	// the guard's normal next-tick activity initialization finish first.
	if (IsValid(this) && !IsActorBeingDestroyed() && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		for (const TWeakObjectPtr<ATerritoryGuardCharacter>& Guard : ActiveGuards)
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Guard]()
			{
				ApplyRecentGuardDamage(Guard.Get());
			}));
		}
	}
	if (ShouldLogGuardSpawning())
	{
		UE_LOG(LogTerritory, Log, TEXT("GuardSpawnPoint %s: deployed one reserve (%d remaining, %d pending)"),
			*GetName(), CurrentReserveCount, PendingReserveSpawns);
	}

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
	SavedReserveRemainingDelay = -1.f;
	AutomaticReserveSpawnFailures = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReserveSpawnTimer);
	}
}

void ATerritoryGuardSpawnPoint::HandleOwnershipTransition(EOwnershipTransitionReason Reason)
{
	ForgetRecentGuardDamage();
	if (!HasAuthority()) return;
	// P1-03: Only apply reserve ownership policy when ownership actually changes.
	// InitialSpawn and AdminOverride do not trigger the policy.
	if (Reason != EOwnershipTransitionReason::OwnerChanged
		&& Reason != EOwnershipTransitionReason::RevertedToUnclaimed)
	{
		return;
	}

	// Territory ownership can initialize before the post's BeginPlay. Provision
	// once before applying the explicit policy so that load order cannot refill it.
	InitializeReserves();
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
	if (!HasAuthority()) return;
	ForgetRecentGuardDamage();
	// The post owns the finite reserve. Rebuilding live registrations is not a refill.
	InitializeReserves();
	ActiveGuards.Reset();
}

TArray<FTerritoryPatrolNode> ATerritoryGuardSpawnPoint::GetPatrolRoute() const
{
	return GetEffectivePatrolRoute();
}

ATerritoryVolume* ATerritoryGuardSpawnPoint::GetOwningTerritory() const
{
	return IsWaitingForOwningTerritory() ? nullptr : CachedTerritory.Get();
}

bool ATerritoryGuardSpawnPoint::IsWaitingForOwningTerritory() const
{
	const UWorld* World = GetWorld();
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!OwnerTerritoryTag.IsValid() || !Registry) return false;
	const ATerritoryVolume* RegisteredOwner = Registry->GetTerritoryByTag(OwnerTerritoryTag);
	return !RegisteredOwner || RegisteredOwner != CachedTerritory.Get();
}

bool ATerritoryGuardSpawnPoint::HasPatrolRoute() const
{
	// One node is a real route: the guard walks to it and holds. A post with no route has no
	// authored duty and reports false; see HasAnyPatrolDuty() for the patrol-in-place case.
	return GetEffectivePatrolRoute().Num() >= 1;
}

bool ATerritoryGuardSpawnPoint::HasMultiStopPatrolRoute() const
{
	return GetEffectivePatrolRoute().Num() >= 2;
}

bool ATerritoryGuardSpawnPoint::GetEffectiveUseSpawnTransformAsPatrolStop() const
{
	// Same precedence shape as the route itself: the value applied to this post, then the
	// nested Guard Post Definition. The post's copy is never *cleared* by the definition,
	// only defaulted, because ApplyTerritoryDefinition assigns it from the Place row.
	if (bUseSpawnTransformAsPatrolStop) return true;
	return GuardPostDefinition && GuardPostDefinition->bUseSpawnTransformAsPatrolStop;
}

bool ATerritoryGuardSpawnPoint::HasImplicitPatrolStop() const
{
	// An authored route always wins: one node is enough to say exactly where the guard stops,
	// and it carries the wait time and activity the implicit stop cannot express.
	return GetEffectiveUseSpawnTransformAsPatrolStop() && GetEffectivePatrolRoute().IsEmpty();
}

bool ATerritoryGuardSpawnPoint::HasAnyPatrolDuty() const
{
	return HasPatrolRoute() || HasImplicitPatrolStop();
}

FTerritoryPatrolNode ATerritoryGuardSpawnPoint::GetImplicitPatrolStop() const
{
	// Built from the struct's own defaults so no wait time or activity is baked in here; a
	// designer who wants either authors one node instead of relying on the implicit stop.
	FTerritoryPatrolNode Stop;
	const FTransform SpawnTransform = GetSpawnTransform();
	Stop.Location = SpawnTransform.GetLocation();
	Stop.Rotation = SpawnTransform.Rotator();
	return Stop;
}

void ATerritoryGuardSpawnPoint::SortForDeployment(TArray<ATerritoryGuardSpawnPoint*>& Posts)
{
	Posts.Sort([](const ATerritoryGuardSpawnPoint& A, const ATerritoryGuardSpawnPoint& B)
	{
		if (A.Priority != B.Priority) return A.Priority > B.Priority;
		// Deliberately HasMultiStopPatrolRoute(), not HasAnyPatrolDuty(). This tie-break means
		// "this post's guard covers ground", so a single stop - authored or implicit - must not
		// win it. Widening this changes which posts and floors deployment fills first for
		// existing content, so it is a decision, not an oversight: do not "fix" it later.
		if (A.HasMultiStopPatrolRoute() != B.HasMultiStopPatrolRoute())
			return A.HasMultiStopPatrolRoute();
		return A.GetPathName() < B.GetPathName();
	});
}

bool ATerritoryGuardSpawnPoint::IsReachedByDeploymentTarget(int32 PostIndex, int32 TargetGuardCount)
{
	// Guard k (1-based) stands on the k-th post the fill accepts, so the k-th accepted post needs
	// k <= TargetGuardCount. A caller passing an *accepted* rank is exact; one passing a raw index
	// over every post is conservative, because refused posts let the fill reach further down.
	return PostIndex >= 0 && TargetGuardCount > 0 && PostIndex < TargetGuardCount;
}

int32 ATerritoryGuardSpawnPoint::PlanFloorClaims(
	const TArray<int32>& ResolvedFloorByRank,
	const TArray<FTerritoryFloorTemplate>& Floors,
	int32 TargetGuardCount,
	TArray<FTerritoryFloorClaim>& OutClaims)
{
	OutClaims.Reset();

	const int32 Budget = FMath::Max(0, TargetGuardCount);
	if (Budget == 0 || Floors.IsEmpty() || ResolvedFloorByRank.IsEmpty())
	{
		return Budget;
	}

	// Each floor's size and its best-placed post, over the posts the fill can accept. A refused post
	// lets the fill reach further down, so a rank is a conservative stand-in for the deployment
	// order - all that is needed, because it only has to order floors against one another.
	TMap<int32, int32> PostCountByFloor;
	TMap<int32, int32> BestRankByFloor;
	for (int32 Rank = 0; Rank < ResolvedFloorByRank.Num(); ++Rank)
	{
		const int32 RankedFloor = ResolvedFloorByRank[Rank];
		++PostCountByFloor.FindOrAdd(RankedFloor);
		int32& BestRank = BestRankByFloor.FindOrAdd(RankedFloor, MAX_int32);
		BestRank = FMath::Min(BestRank, Rank);
	}

	TArray<int32> ClaimingFloors;
	for (const FTerritoryFloorTemplate& Floor : Floors)
	{
		// Only an authored count claims. A zero quota means "every post on this floor" for
		// reporting, which is not a claim on the budget; reading it as one would reserve the whole
		// ceiling of every declared floor and reorder deployment for content that never asked.
		if (!Floor.ClaimsFloorQuota()) continue;
		if (!PostCountByFloor.Contains(Floor.FloorIndex)) continue;
		ClaimingFloors.AddUnique(Floor.FloorIndex);
	}
	ClaimingFloors.Sort([&BestRankByFloor](const int32 A, const int32 B)
	{
		return BestRankByFloor.FindRef(A) < BestRankByFloor.FindRef(B);
	});

	int32 Remaining = Budget;
	for (const int32 ClaimFloor : ClaimingFloors)
	{
		if (Remaining <= 0) break;

		const FTerritoryFloorTemplate* const Floor = Floors.FindByPredicate(
			[ClaimFloor](const FTerritoryFloorTemplate& Row) { return Row.FloorIndex == ClaimFloor; });
		if (!Floor) continue;

		// A claim can never exceed the posts that can hold it, and never the budget left when its
		// turn comes - so a claim is a floor's share of what exists, not a promise the target cannot
		// keep.
		const int32 FloorPosts = PostCountByFloor.FindRef(ClaimFloor);
		const int32 Quota = FTerritoryFloorTemplate::ResolveGuardQuota(Floor->DesiredGuards, FloorPosts);
		const int32 Served = FMath::Clamp(Quota, 0, FMath::Min(Remaining, FloorPosts));
		if (Served <= 0) continue;

		FTerritoryFloorClaim& Claim = OutClaims.AddDefaulted_GetRef();
		Claim.FloorIndex = ClaimFloor;
		Claim.Guards = Served;
		Remaining -= Served;
	}

	return Remaining;
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
// The optional nested GuardPostDefinition supplies reusable defaults behind the
// owning Place Definition row.
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

FName ATerritoryGuardSpawnPoint::GetEffectivePatrolRouteID() const
{
	// The row is the first authority; a nested Guard Post Definition can still name the route when
	// the row leaves it blank, which mirrors how the array route falls back to that asset.
	return !PatrolRouteID.IsNone()
		? PatrolRouteID
		: (GuardPostDefinition ? GuardPostDefinition->PatrolRouteID : NAME_None);
}

ATerritoryPatrolRoute* ATerritoryGuardSpawnPoint::ResolvePatrolRouteActor() const
{
	const FName EffectiveRouteID = GetEffectivePatrolRouteID();

	if (EffectiveRouteID.IsNone())
	{
		CachedPatrolRouteActor.Reset();
		CachedRouteActorNodes.Reset();
		return nullptr;
	}

	// The reference is an ID rather than an actor pointer because the route is placed in a level
	// and the row that names it is a data asset. A cached actor that no longer matches the ID
	// (the level was re-saved with a different route) is dropped instead of silently reused.
	if (ATerritoryPatrolRoute* Cached = CachedPatrolRouteActor.Get())
	{
		if (Cached->GetPatrolRouteID() == EffectiveRouteID)
		{
			return Cached;
		}
		CachedPatrolRouteActor.Reset();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ATerritoryPatrolRoute> It(World); It; ++It)
	{
		ATerritoryPatrolRoute* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->GetPatrolRouteID() != EffectiveRouteID)
		{
			continue;
		}

		CachedPatrolRouteActor = Candidate;
		return Candidate;
	}

	return nullptr;
}

const TArray<FTerritoryPatrolNode>& ATerritoryGuardSpawnPoint::GetEffectivePatrolRoute() const
{
	// Returns *authored* nodes only: an implicit stop is never synthesised here because the
	// fallback below is a function-local static, so a caller holding this reference and
	// comparing or mutating it across frames would be reading shared state. Consumers that
	// need patrol duty ask HasAnyPatrolDuty() and build the stop via GetImplicitPatrolStop().
	//
	// A referenced route actor wins over the array routes: it is the authoring surface the row
	// deliberately points at, and the sampled nodes are cached on this post so the returned
	// reference stays valid for every consumer (guard, validator, spawn comparators).
	if (const ATerritoryPatrolRoute* RouteActor = ResolvePatrolRouteActor())
	{
		CachedRouteActorNodes.Reset();
		FText FailureReason;
		if (RouteActor->BuildPatrolNodes(CachedRouteActorNodes, FailureReason) &&
			!CachedRouteActorNodes.IsEmpty())
		{
			return CachedRouteActorNodes;
		}

		CachedRouteActorNodes.Reset();
		// A referenced route that cannot produce nodes is an authoring error, not a silent
		// demotion: keeping the array route means a broken spline never strips patrol duty.
		UE_LOG(LogTerritory, Warning,
			TEXT("Guard post %s references patrol route '%s' but it produced no nodes (%s); falling back to the authored node array."),
			*GetName(), *PatrolRouteID.ToString(), *FailureReason.ToString());
	}

	if (!PatrolRoute.IsEmpty()) return PatrolRoute;
	if (GuardPostDefinition && !GuardPostDefinition->PatrolRoute.IsEmpty())
		return GuardPostDefinition->PatrolRoute;
	static const TArray<FTerritoryPatrolNode> Empty;
	return Empty;
}

bool ATerritoryGuardSpawnPoint::GetEffectiveLoopPatrol() const
{
	// The loop policy follows the route authority. A referenced route actor owns its own loop
	// flag; a Place Definition row route owns its row loop flag; a nested GuardPostDefinition
	// route owns that asset's flag.
	if (const ATerritoryPatrolRoute* RouteActor = ResolvePatrolRouteActor())
	{
		if (!CachedRouteActorNodes.IsEmpty())
		{
			return RouteActor->IsLooping();
		}
	}

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
