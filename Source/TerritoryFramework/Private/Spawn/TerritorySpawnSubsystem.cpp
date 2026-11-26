#include "Spawn/TerritorySpawnSubsystem.h"

#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnExecutor.h"
#include "Spawn/TerritorySpawnPolicy.h"
#include "TimerManager.h"

namespace TerritorySpawnSubsystemPrivate
{
	/** Sweep cadence. Slow enough to be invisible in a profile, fast enough that a returned car appears while the player still walks back. */
	constexpr float SweepIntervalSeconds = 0.5f;

	/** Ten buckets at a 0.5 s tick gives every anchor a 5 s period. */
	constexpr int32 DefaultSweepBuckets = 10;
}

using namespace TerritorySpawnSubsystemPrivate;

// The retry budget, the pending deadline and the debug gate are shared, not file-local: a second copy
// here would collide with the anchor's the moment unity builds put both translation units together.
using TerritorySpawnLimits::MaxConsecutiveSpawnFailures;
using TerritorySpawnLimits::HardReturnTimeoutSeconds;

bool UTerritorySpawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Game and PIE only. An editor-preview world must not run a sweep or write save records.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTerritorySpawnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SweepBuckets = FMath::Max(1, DefaultSweepBuckets);
	SweepBucketsStorage.Reset();
	SweepBucketsStorage.SetNum(SweepBuckets);
	CurrentBucket = 0;
}

void UTerritorySpawnSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SweepTimerHandle);
	}

	AnchorsByGuid.Reset();
	AnchorsByEntityTag.Reset();
	SweepBucketsStorage.Reset();
	Leases.Reset();
	bSweeping = false;

	// Release every async class load this subsystem requested. A lingering handle keeps the package
	// resident after the world that wanted it is gone, and a cancelled world is the common case here.
	ClassPreload.Reset();

	Super::Deinitialize();
}

void UTerritorySpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Server only, for the same reason the counterattack subsystem is: a client cannot spawn, retire or
	// lease anything, and running the sweep there would only produce decisions nobody can act on.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	InWorld.GetTimerManager().SetTimer(SweepTimerHandle, this, &UTerritorySpawnSubsystem::RunSweep,
		SweepIntervalSeconds, true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Registry
// ═══════════════════════════════════════════════════════════════════════════════

bool UTerritorySpawnSubsystem::RegisterAnchor(ATerritorySpawnAnchor* Anchor)
{
	if (!Anchor) return false;

	const FGuid AnchorGuid = Anchor->GetAnchorGuid();
	if (!AnchorGuid.IsValid())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Refused to register anchor %s: no editor-baked GUID."),
			*Anchor->GetPathName());
		return false;
	}

	if (SweepBucketsStorage.Num() != SweepBuckets)
	{
		SweepBucketsStorage.SetNum(SweepBuckets);
	}

	if (const TWeakObjectPtr<ATerritorySpawnAnchor>* Existing = AnchorsByGuid.Find(AnchorGuid))
	{
		// Re-registering the same actor is a refresh, not an error: BeginPlay can run again after a
		// seamless travel or a re-entrant stream-in. A different actor under the same GUID is an error,
		// because the saved record would then be ambiguous.
		if (Existing->Get() == Anchor)
		{
			return true;
		}

		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Refused to register anchor %s: GUID %s is already claimed by %s. Two anchors with one identity would make the saved record ambiguous."),
			*Anchor->GetPathName(), *AnchorGuid.ToString(), *GetNameSafe(Existing->Get()));
		return false;
	}

	AnchorsByGuid.Add(AnchorGuid, Anchor);

	const FGameplayTag EntityTag = Anchor->GetPolicy() ? Anchor->GetPolicy()->GetEntityTag() : FGameplayTag();
	if (EntityTag.IsValid())
	{
		AnchorsByEntityTag.FindOrAdd(EntityTag).AddUnique(Anchor);
	}

	// Bucket by the durable GUID, so the assignment is identical in every run and survives save/load.
	// GetTypeHash(FGuid) is deterministic; a pointer hash would not be, and would also reshuffle on load.
	const uint32 Hash = GetTypeHash(AnchorGuid);
	const int32 BucketIndex = SweepBuckets > 0 ? static_cast<int32>(Hash % static_cast<uint32>(SweepBuckets)) : 0;

	if (SweepBucketsStorage.IsValidIndex(BucketIndex))
	{
		SweepBucketsStorage[BucketIndex].AddUnique(Anchor);
	}

	OnSpawnRegistryChanged.Broadcast();
	return true;
}

void UTerritorySpawnSubsystem::UnregisterAnchor(ATerritorySpawnAnchor* Anchor)
{
	if (!Anchor) return;

	const FGuid AnchorGuid = Anchor->GetAnchorGuid();
	if (AnchorGuid.IsValid())
	{
		AnchorsByGuid.Remove(AnchorGuid);
	}

	for (TPair<FGameplayTag, TArray<TWeakObjectPtr<ATerritorySpawnAnchor>>>& Pair : AnchorsByEntityTag)
	{
		Pair.Value.RemoveAll([Anchor](const TWeakObjectPtr<ATerritorySpawnAnchor>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Anchor;
		});
	}

	for (TArray<TWeakObjectPtr<ATerritorySpawnAnchor>>& Bucket : SweepBucketsStorage)
	{
		Bucket.RemoveAll([Anchor](const TWeakObjectPtr<ATerritorySpawnAnchor>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Anchor;
		});
	}

	// Leases are granted against a slot. When the slot's actor goes away the leases cannot outlive it,
	// otherwise a returning slot would start already pinned by a mission that has forgotten it.
	if (AnchorGuid.IsValid())
	{
		TArray<FGuid> Orphaned;
		for (const TPair<FGuid, FTerritorySpawnLease>& Pair : Leases)
		{
			if (Pair.Value.AnchorGuid == AnchorGuid)
			{
				Orphaned.Add(Pair.Key);
			}
		}
		for (const FGuid& LeaseId : Orphaned)
		{
			Leases.Remove(LeaseId);
		}
	}

	OnSpawnRegistryChanged.Broadcast();
}

ATerritorySpawnAnchor* UTerritorySpawnSubsystem::FindAnchor(const FGuid& AnchorGuid) const
{
	if (!AnchorGuid.IsValid()) return nullptr;

	const TWeakObjectPtr<ATerritorySpawnAnchor>* Found = AnchorsByGuid.Find(AnchorGuid);
	return Found && Found->IsValid() ? Found->Get() : nullptr;
}

TArray<ATerritorySpawnAnchor*> UTerritorySpawnSubsystem::GetAllAnchors() const
{
	TArray<ATerritorySpawnAnchor*> Result;
	Result.Reserve(AnchorsByGuid.Num());

	for (const TPair<FGuid, TWeakObjectPtr<ATerritorySpawnAnchor>>& Pair : AnchorsByGuid)
	{
		if (Pair.Value.IsValid())
		{
			Result.Add(Pair.Value.Get());
		}
	}

	// Sorted by durable GUID, never by map order. A caller that reports or acts on this list must get
	// the same sequence in every run.
	Result.Sort([](const ATerritorySpawnAnchor& A, const ATerritorySpawnAnchor& B)
	{
		return A.GetAnchorGuid().ToString(EGuidFormats::Digits) < B.GetAnchorGuid().ToString(EGuidFormats::Digits);
	});

	return Result;
}

TArray<ATerritorySpawnAnchor*> UTerritorySpawnSubsystem::GetAnchorsByEntityTag(FGameplayTag EntityTag) const
{
	TArray<ATerritorySpawnAnchor*> Result;
	if (!EntityTag.IsValid()) return Result;

	const TArray<TWeakObjectPtr<ATerritorySpawnAnchor>>* Found = AnchorsByEntityTag.Find(EntityTag);
	if (!Found) return Result;

	Result.Reserve(Found->Num());
	for (const TWeakObjectPtr<ATerritorySpawnAnchor>& Entry : *Found)
	{
		if (Entry.IsValid())
		{
			Result.Add(Entry.Get());
		}
	}

	Result.Sort([](const ATerritorySpawnAnchor& A, const ATerritorySpawnAnchor& B)
	{
		return A.GetAnchorGuid().ToString(EGuidFormats::Digits) < B.GetAnchorGuid().ToString(EGuidFormats::Digits);
	});

	return Result;
}

TArray<ATerritorySpawnAnchor*> UTerritorySpawnSubsystem::GetAnchorsOwnedBy(FName PlayerName) const
{
	TArray<ATerritorySpawnAnchor*> Result;
	if (PlayerName.IsNone()) return Result;

	for (const TPair<FGuid, TWeakObjectPtr<ATerritorySpawnAnchor>>& Pair : AnchorsByGuid)
	{
		ATerritorySpawnAnchor* Anchor = Pair.Value.Get();
		if (Anchor && Anchor->IsOwnedByPlayer(PlayerName))
		{
			Result.Add(Anchor);
		}
	}

	Result.Sort([](const ATerritorySpawnAnchor& A, const ATerritorySpawnAnchor& B)
	{
		return A.GetAnchorGuid().ToString(EGuidFormats::Digits) < B.GetAnchorGuid().ToString(EGuidFormats::Digits);
	});

	return Result;
}

ATerritorySpawnAnchor* UTerritorySpawnSubsystem::FindAnchorOwningActor(const AActor* Actor) const
{
	if (!IsValid(Actor)) return nullptr;

	for (const TPair<FGuid, TWeakObjectPtr<ATerritorySpawnAnchor>>& Pair : AnchorsByGuid)
	{
		ATerritorySpawnAnchor* const Anchor = Pair.Value.Get();
		if (Anchor && Anchor->GetSpawnedActor() == Actor)
		{
			return Anchor;
		}
	}

	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Leases
// ═══════════════════════════════════════════════════════════════════════════════

FGuid UTerritorySpawnSubsystem::AcquireLease(FGuid AnchorGuid, UObject* Holder, FGameplayTag Purpose,
	float DurationSeconds)
{
	if (!AnchorGuid.IsValid() || !Holder) return FGuid();

	const UWorld* World = GetWorld();
	if (!World) return FGuid();

	// A lease always expires. The only alternative is a lease that outlives its holder, which is a
	// permanently blocked slot with no path back. A non-positive duration is therefore rejected rather
	// than silently promoted to the default: "no duration" and "the default duration" are different
	// intentions, and a caller that passed zero by accident must find out here instead of six hours
	// later when the bay will not release.
	if (DurationSeconds <= 0.f)
	{
		if (ShouldLogTerritorySpawn())
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[Spawn] Refused a lease on anchor %s for %s: duration %.2fs is not positive. A lease that never expires is a permanently dead slot."),
				*AnchorGuid.ToString(), *Holder->GetName(), DurationSeconds);
		}
		return FGuid();
	}

	const double NowGameTime = static_cast<double>(World->GetTimeSeconds());
	const double Duration = static_cast<double>(DurationSeconds);

	// Ask the slot's own policy whether it accepts this kind of pin. Done BEFORE the lease is stored,
	// so a refusal leaves no trace for the caller to leak. The lookup is by GUID because the caller
	// holds a GUID, not an anchor pointer, and an anchor that has not registered yet simply has no
	// policy to ask — in which case the lease is granted, because a lease on a slot that does not exist
	// yet is still the correct way for a mission to say "I will need this".
	if (const TWeakObjectPtr<ATerritorySpawnAnchor>* const Found = AnchorsByGuid.Find(AnchorGuid))
	{
		if (const ATerritorySpawnAnchor* const Anchor = Found->Get())
		{
			if (const UTerritorySpawnPolicy* const Policy = Anchor->GetPolicy())
			{
				if (!Policy->AllowsLease(Purpose))
				{
					if (ShouldLogTerritorySpawn())
					{
						UE_LOG(LogTerritory, Log,
							TEXT("[Spawn] Anchor %s refused a lease for %s (purpose %s); its policy does not allow that pin."),
							*AnchorGuid.ToString(), *Holder->GetName(),
							Purpose.IsValid() ? *Purpose.ToString() : TEXT("none"));
					}
					return FGuid();
				}
			}
		}
	}

	FTerritorySpawnLease Lease;
	Lease.LeaseId = FGuid::NewGuid();
	Lease.AnchorGuid = AnchorGuid;
	Lease.Holder = Holder;
	Lease.Purpose = Purpose;
	Lease.ExpiryGameTime = NowGameTime + Duration;

	Leases.Add(Lease.LeaseId, Lease);

	if (ShouldLogTerritorySpawn())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[Spawn] Lease %s granted on anchor %s for %s (%.1fs, purpose %s)"),
			*Lease.LeaseId.ToString(), *AnchorGuid.ToString(), *Holder->GetName(), Duration,
			Purpose.IsValid() ? *Purpose.ToString() : TEXT("none"));
	}

	return Lease.LeaseId;
}

bool UTerritorySpawnSubsystem::ReleaseLease(FGuid LeaseId)
{
	if (!LeaseId.IsValid()) return false;
	return Leases.Remove(LeaseId) > 0;
}

int32 UTerritorySpawnSubsystem::ReleaseLeasesForAnchor(FGuid AnchorGuid)
{
	if (!AnchorGuid.IsValid()) return 0;

	// Collect first, then remove: erasing from a TMap while ranging over it invalidates the iterator.
	// The project has already paid for this lesson in the lease sweep below.
	TArray<FGuid> ToRelease;
	for (const TPair<FGuid, FTerritorySpawnLease>& Pair : Leases)
	{
		if (Pair.Value.AnchorGuid == AnchorGuid)
		{
			ToRelease.Add(Pair.Key);
		}
	}

	int32 Released = 0;
	for (const FGuid& LeaseId : ToRelease)
	{
		Released += Leases.Remove(LeaseId) > 0 ? 1 : 0;
	}
	return Released;
}

int32 UTerritorySpawnSubsystem::ReleaseLeasesForHolder(UObject* Holder)
{
	if (!Holder) return 0;

	TArray<FGuid> ToRelease;
	for (const TPair<FGuid, FTerritorySpawnLease>& Pair : Leases)
	{
		const UObject* LeaseHolder = Pair.Value.Holder.Get();
		if (LeaseHolder == nullptr || LeaseHolder == Holder)
		{
			ToRelease.Add(Pair.Key);
		}
	}

	int32 Released = 0;
	for (const FGuid& LeaseId : ToRelease)
	{
		Released += Leases.Remove(LeaseId) > 0 ? 1 : 0;
	}
	return Released;
}

bool UTerritorySpawnSubsystem::HasLiveLease(FGuid AnchorGuid, double NowGameTime) const
{
	if (!AnchorGuid.IsValid()) return false;

	for (const TPair<FGuid, FTerritorySpawnLease>& Pair : Leases)
	{
		if (Pair.Value.AnchorGuid != AnchorGuid) continue;
		if (Pair.Value.IsValidLease(NowGameTime)) return true;
	}
	return false;
}

TArray<FTerritorySpawnLease> UTerritorySpawnSubsystem::GetLeasesForAnchor(FGuid AnchorGuid, double NowGameTime) const
{
	TArray<FTerritorySpawnLease> Result;
	if (!AnchorGuid.IsValid()) return Result;

	for (const TPair<FGuid, FTerritorySpawnLease>& Pair : Leases)
	{
		if (Pair.Value.AnchorGuid != AnchorGuid) continue;
		if (Pair.Value.IsValidLease(NowGameTime))
		{
			Result.Add(Pair.Value);
		}
	}

	// Lease id order, so a diagnostic listing is reproducible.
	Result.Sort([](const FTerritorySpawnLease& A, const FTerritorySpawnLease& B)
	{
		return A.LeaseId.ToString(EGuidFormats::Digits) < B.LeaseId.ToString(EGuidFormats::Digits);
	});

	return Result;
}

void UTerritorySpawnSubsystem::RequestImmediateEvaluation(ATerritorySpawnAnchor* Anchor)
{
	if (!Anchor || !Anchor->HasAuthority()) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	EvaluateAnchor(Anchor, static_cast<double>(World->GetTimeSeconds()));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sweep
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritorySpawnSubsystem::RunSweep()
{
	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown) return;

	// Re-entrancy guard. An executor can synchronously destroy an actor whose EndPlay calls back into
	// this subsystem; without this the bucket array could be mutated underneath the iteration.
	if (bSweeping) return;
	TGuardValue<bool> SweepGuard(bSweeping, true);

	const double NowGameTime = static_cast<double>(World->GetTimeSeconds());
	ExpireStaleLeases(NowGameTime);

	// Ownership is reconciled once per sweep, before any anchor is evaluated. Doing it afterwards would
	// let a personal-car bay be evaluated in the same pass it was claimed, so the policy's own ownership
	// gate would read the previous pass's answer. Once per pass is also enough: a login is a human
	// timescale event and the sweep period is half a second.
	ReconcilePlayerClaims();

	if (SweepBucketsStorage.Num() == 0) return;

	CurrentBucket = (CurrentBucket + 1) % SweepBucketsStorage.Num();

	// Copy the bucket first. Evaluation can stream a cell out, which unregisters anchors mid-loop, and
	// iterating the live array while it shrinks is how a mid-sweep destroy becomes a crash.
	TArray<TWeakObjectPtr<ATerritorySpawnAnchor>> Work;
	Work.Reserve(SweepBucketsStorage[CurrentBucket].Num());
	for (const TWeakObjectPtr<ATerritorySpawnAnchor>& Entry : SweepBucketsStorage[CurrentBucket])
	{
		if (Entry.IsValid())
		{
			Work.Add(Entry);
		}
	}

	// Deterministic order inside the bucket: durable GUID ascending, never registration order.
	Work.Sort([](const TWeakObjectPtr<ATerritorySpawnAnchor>& A, const TWeakObjectPtr<ATerritorySpawnAnchor>& B)
	{
		const FGuid GuidA = A.IsValid() ? A->GetAnchorGuid() : FGuid();
		const FGuid GuidB = B.IsValid() ? B->GetAnchorGuid() : FGuid();
		return GuidA.ToString(EGuidFormats::Digits) < GuidB.ToString(EGuidFormats::Digits);
	});

	for (const TWeakObjectPtr<ATerritorySpawnAnchor>& Entry : Work)
	{
		ATerritorySpawnAnchor* Anchor = Entry.Get();
		if (!Anchor || Anchor->IsActorBeingDestroyed()) continue;
		EvaluateAnchor(Anchor, NowGameTime);
	}
}

void UTerritorySpawnSubsystem::EvaluateAnchor(ATerritorySpawnAnchor* Anchor, double NowGameTime)
{
	if (!Anchor || Anchor->IsActorBeingDestroyed()) return;

	++EvaluationCount;

	if (!Anchor->IsAnchorConfigured()) return;
	if (Anchor->IsEvaluationBlocked()) return;

	const UTerritorySpawnPolicy* Policy = Anchor->GetPolicy();
	if (!Policy) return;

	UTerritorySpawnExecutor* Executor = Anchor->GetExecutor();
	if (!Executor) return;

	FTerritorySpawnRequest Request;
	Request.AnchorGuid = Anchor->GetAnchorGuid();
	Request.OwnerKey = Anchor->GetOwnerKey();
	Request.EntityTag = Policy->GetEntityTag();
	Request.CurrentState = Anchor->GetLifecycleState();
	Request.NowGameTime = NowGameTime;
	Request.AnchorTransform = Anchor->GetActorTransform();
	Request.ConsecutiveFailures = Anchor->GetConsecutiveFailures();
	Request.PendingSinceGameTime = Anchor->GetPendingSinceGameTime();
	Request.bHasLiveLease = HasLiveLease(Request.AnchorGuid, NowGameTime);
	Request.Record = Anchor->SlotRecord;

	// The live entity, when there is one. Populated from the framework's own tracking rather than from
	// the record, because the record's transform is a placement memory: a car driven across the
	// district still has "at home" written in it. A policy that needs "has it moved" must see reality.
	const AActor* const TrackedActor = Anchor->GetSpawnedActor();
	Request.bHasEntity = TrackedActor != nullptr && !TrackedActor->IsActorBeingDestroyed();
	Request.EntityTransform = Request.bHasEntity ? TrackedActor->GetActorTransform() : FTransform::Identity;

	// ─── Reality correction, BEFORE the policy is asked ───
	//
	// A slot whose record claims an instance, whose actor is gone, was emptied by something outside the
	// framework: a car wrecked by gameplay, a tree felled by a player, an actor removed by a level
	// stream. The record still says "I am holding something", and every policy answers questions about
	// the present tense — so the correction has to happen first or the policy is asked about a state
	// that no longer exists.
	//
	// This used to live inside the retirement branch, which made it reachable only for a policy that
	// asks to retire. A policy that never retires (a static one-shot, an ambulance that restocks
	// itself) would therefore never notice its entity had been destroyed: bHasInstance stayed true
	// forever, and a RebuildLost slot could never rebuild because it never saw the loss.
	//
	// The load case is deliberately NOT part of this. A record restored from a save also arrives with
	// bHasInstance set and nothing in the world, but that is a promise the slot has not delivered yet,
	// not evidence that something was destroyed. Treating it as destruction would mark every restored
	// car as wrecked on the first sweep of every session. Load_Implementation parks such a slot in
	// PendingSpawn, which is how the two cases are told apart here.
	if (Anchor->SlotRecord.bHasInstance && !Request.bHasEntity)
	{
		if (Anchor->GetLifecycleState() == ESpawnLifecycleState::Retired)
		{
			// Retired is terminal for the session, and Retire does not clear bHasInstance. Reconciling
			// would drag a retired slot back to Dormant and hand it a fresh spawn budget — a policy would
			// then get another chance at the exact slot the framework already gave up on.
			return;
		}

		if (Anchor->GetLifecycleState() == ESpawnLifecycleState::PendingSpawn)
		{
			// Owed, not lost. The policy is told so it can ask for the entity.
			Request.bAwaitingRestore = true;
		}
		else
		{
			Anchor->ResetToDormant(ESpawnRemovalReason::LoadReconcile, /*bTreatAsDestroyed=*/true);

			// Re-read the record rather than patching the copy: the policy must see exactly what a later
			// sweep would see, or the two disagree about bWasDestroyed.
			Request.Record = Anchor->SlotRecord;
			Request.CurrentState = Anchor->GetLifecycleState();
		}
	}

	if (!Policy->ShouldEvaluate(Request)) return;

	const FTerritorySpawnDecision Decision = Policy->EvaluateDecision(Request);
	if (!Decision.IsActionable()) return;

	AActor* const Existing = Anchor->GetSpawnedActor();

	// ─── Retirement path ───
	// The policy may only ASK. Every rule below can downgrade its answer and none can upgrade it, which
	// is what makes a mis-authored data asset unable to destroy a car a player is driving.
	if (Decision.Action == ESpawnAction::Retire)
	{
		// A missing actor at this point means the slot genuinely holds nothing and the record already
		// agrees — the "vanished on its own" case was corrected above, before the policy was asked.
		// Asking to retire an empty slot is a policy mistake, not a transition.
		if (!Existing || Existing->IsActorBeingDestroyed()) return;

		// A live lease pins the slot. This is how a mission borrows an ambulance without the bay's own
		// restock rule pulling it out from under the objective.
		if (Request.bHasLiveLease) return;

		// The executor decides what "still in use" means for its family. A vehicle delegates this to the
		// project's existing player-occupancy predicate rather than re-implementing it.
		if (Executor->IsClaimedByExternalOwner(Existing)) return;

		// Authored grace. The framework owns the RULE that a grace exists; the policy owns the NUMBER,
		// which is why leaving the number at zero cannot disable the other checks.
		if (Decision.MinimumLiveSeconds > 0.f && Anchor->SlotRecord.LastTransitionGameTime >= 0.0)
		{
			const double Elapsed = NowGameTime - Anchor->SlotRecord.LastTransitionGameTime;
			if (Elapsed < static_cast<double>(Decision.MinimumLiveSeconds)) return;
		}

		// Everything passed. Capture the condition, then retire, then commit. A crash between teardown
		// and commit leaves a "still has instance" record, which the next load reconciles by re-reading
		// reality rather than by inventing state.
		Executor->CaptureCheckpoint(Anchor, Existing, Anchor->SlotRecord);
		Executor->RetireSpawned(Anchor, Existing, Decision.RetireMode);

		Anchor->SetTrackedActor(nullptr);
		Anchor->SlotRecord.bHasInstance = false;
		Anchor->SlotRecord.bWasDestroyed = Decision.RetireMode == ESpawnRetireMode::Destroy;
		Anchor->CommitTransition(ESpawnLifecycleState::Dormant, Decision.Reason, NowGameTime);
		Anchor->RegisterSpawnSuccess();
		return;
	}

	// ─── Suspension path ───
	if (Decision.Action == ESpawnAction::Suspend)
	{
		if (!Existing || Existing->IsActorBeingDestroyed()) return;
		if (Request.bHasLiveLease) return;
		if (Executor->IsClaimedByExternalOwner(Existing)) return;

		Executor->CaptureCheckpoint(Anchor, Existing, Anchor->SlotRecord);
		if (Executor->SuspendSpawned(Anchor, Existing))
		{
			Anchor->CommitTransition(ESpawnLifecycleState::Suspended, Decision.Reason, NowGameTime);
		}
		return;
	}

	// ─── Resume path ───
	if (Decision.Action == ESpawnAction::Resume)
	{
		if (!Existing || Existing->IsActorBeingDestroyed())
		{
			// Nothing to resume: fall through to a normal spawn next sweep rather than pretending a
			// suspended actor exists.
			Anchor->CommitTransition(ESpawnLifecycleState::Dormant,
				ESpawnTransitionReason::LoadReconcile, NowGameTime);
			return;
		}

		if (Executor->ResumeSpawned(Anchor, Existing))
		{
			Anchor->CommitTransition(ESpawnLifecycleState::Live,
				ESpawnTransitionReason::PolicyRequest, NowGameTime);
		}
		return;
	}

	// ─── Spawn path ───
	if (Decision.Action != ESpawnAction::Spawn) return;

	// Already holding something: not an error, just a stale policy answer. Confirm Live and move on
	// rather than putting a second entity into one slot.
	if (Existing && !Existing->IsActorBeingDestroyed())
	{
		Anchor->RegisterSpawnSuccess();
		Anchor->CommitTransition(ESpawnLifecycleState::Live, ESpawnTransitionReason::PolicyRequest,
			NowGameTime);
		return;
	}

	// Bounded retry. A slot that keeps failing is retired with one error rather than retried forever;
	// an unbounded retry is how a blocked bay silently burns a sweep slot forever.
	if (Anchor->GetConsecutiveFailures() >= TerritorySpawnLimits::MaxConsecutiveSpawnFailures)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s failed to spawn %d times; retiring the slot instead of retrying forever."),
			*Anchor->GetPathName(), TerritorySpawnLimits::MaxConsecutiveSpawnFailures);
		Anchor->CommitTransition(ESpawnLifecycleState::Retired,
			ESpawnTransitionReason::RetryLimitReached, NowGameTime);
		return;
	}

	// Hard deadline. A slot pending past this window is escalated exactly once, because a slot that
	// waits forever with no report is indistinguishable from one that works.
	if (Request.PendingSinceGameTime >= 0.0
		&& (NowGameTime - Request.PendingSinceGameTime) > TerritorySpawnLimits::HardReturnTimeoutSeconds)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s has been pending for %.0fs. Placement stays blocked; nothing was relocated to make it fit."),
			*Anchor->GetPathName(), NowGameTime - Request.PendingSinceGameTime);
		Anchor->CommitTransition(ESpawnLifecycleState::Retired,
			ESpawnTransitionReason::HardTimeout, NowGameTime);
		return;
	}

	const TSoftClassPtr<AActor> PolicyClass = Policy->GetDesiredActorClass();
	const TSoftClassPtr<AActor> WantedClass = !Decision.DesiredClass.IsNull()
		? Decision.DesiredClass
		: (!PolicyClass.IsNull() ? PolicyClass : Anchor->ActorClass);

	// Never LoadSynchronous here. A synchronous class load in a sweep stalls the frame for however
	// long the package takes, and a bay that is approached at speed is exactly when that hurt is most
	// visible. Poll instead: the class parks the slot in PendingSpawn for a sweep or two and nothing
	// blocks. Poll is also why the sweep, not a load delegate, is the commit point - a delegate fires
	// at a moment nobody controls.
	TArray<FSoftObjectPath> WantedPaths;
	WantedPaths.Add(WantedClass.ToSoftObjectPath());
	if (ClassPreload.Poll(WantedPaths) != ETerritoryAssetReadiness::Ready)
	{
		// Pending and Failed are the same answer for this frame: the slot cannot spawn yet. Failed in
		// particular is not a placement failure, so it must NOT consume the retry budget - a missing
		// package is an authoring mistake that a retry counter would eventually turn into a silent
		// retire, which is worse than a slot that stays visibly pending.
		Anchor->CommitTransition(ESpawnLifecycleState::PendingSpawn,
			ESpawnTransitionReason::ClassLoadFailed, NowGameTime);
		return;
	}

	// Poll just proved the package is resident, so this resolves from memory rather than loading. The
	// cast is required because ResolveObject on a soft path returns a bare UObject.
	UClass* const LoadedClass = Cast<UClass>(WantedClass.ToSoftObjectPath().ResolveObject());
	if (!LoadedClass)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s resolved actor class '%s' to null after a Ready poll; the slot stays pending."),
			*Anchor->GetPathName(), *WantedClass.ToString());
		Anchor->RegisterSpawnFailure();
		Anchor->CommitTransition(ESpawnLifecycleState::PendingSpawn,
			ESpawnTransitionReason::ClassLoadFailed, NowGameTime);
		return;
	}

	AActor* Created = Executor->CreateSpawned(Anchor, LoadedClass, Decision.DesiredTransform);

	if (!Created)
	{
		// A blocked authored position defers the spawn. It is never relocated: silently moving a
		// designer's bay is how a car ends up inside a wall and nobody can reproduce it.
		Anchor->RegisterSpawnFailure();
		Anchor->CommitTransition(ESpawnLifecycleState::PendingSpawn,
			ESpawnTransitionReason::PlacementBlocked, NowGameTime);
		return;
	}

	Anchor->SetTrackedActor(Created);
	Anchor->SlotRecord.ActorClass = WantedClass;
	Anchor->SlotRecord.Transform = Created->GetActorTransform();
	Anchor->SlotRecord.bHasInstance = true;
	Anchor->SlotRecord.bWasDestroyed = false;
	Anchor->SlotRecord.Generation = Anchor->SlotRecord.Generation + 1;

	// Restore the saved condition once, immediately after creation, so the consumer controls what
	// "restore" means for its family and the framework never guesses.
	//
	// ADOPTION IS GONE, DELIBERATELY. A previous version of this function searched the level for a
	// hand-placed actor of the same class and took it over instead of creating one. The intent was to
	// stop a hospital forecourt showing two ambulances, and it did — but it also took authority over an
	// actor the level owned, and it attached the vehicle executor's creation-time configuration to it.
	// The user's report afterwards was exactly that: "spawner sy spawn hoi ambulance ko run nhi kr rhi",
	// the framework's vehicle could not be driven like the level's own. A slot that wants a vehicle
	// creates one; a level that ships one keeps it, and the two must not be the same object.
	Executor->ApplyCheckpoint(Anchor, Created, Anchor->SlotRecord, Decision.bRestoreCheckpoint);

	Anchor->RegisterSpawnSuccess();
	Anchor->CommitTransition(ESpawnLifecycleState::Live, ESpawnTransitionReason::InitialSpawn,
		NowGameTime);
}

void UTerritorySpawnSubsystem::ExpireStaleLeases(double NowGameTime)
{
	// A lease whose holder has been garbage collected counts as expired immediately. Waiting for its
	// clock to run out would block the slot for a mission that no longer exists.
	TArray<FGuid> Expired;
	for (const TPair<FGuid, FTerritorySpawnLease>& Pair : Leases)
	{
		if (!Pair.Value.IsValidLease(NowGameTime))
		{
			Expired.Add(Pair.Key);
		}
	}

	for (const FGuid& LeaseId : Expired)
	{
		Leases.Remove(LeaseId);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════════
// Player claims
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritorySpawnSubsystem::ReconcilePlayerClaims()
{
	const UWorld* const World = GetWorld();
	if (!World) return;

	// Collect the connected player names once. Every owned anchor is then one set lookup instead of a
	// walk over the controller list, which matters because this runs on every sweep pass.
	TSet<FName> ConnectedNames;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* const PC = It->Get();
		if (PC && PC->PlayerState)
		{
			// APlayerState::GetPlayerName() returns FString, and the ownership key is FName because it is
			// a designer-authored identity, not a display string. The conversion is deliberate: comparing
			// display text would make "Alice" and "alice" different bays, which is exactly the ambiguity the
			// FName storage removes.
			const FName PlayerName(*PC->PlayerState->GetPlayerName());
			if (!PlayerName.IsNone())
			{
				ConnectedNames.Add(PlayerName);
			}
		}
	}

	for (const TPair<FGuid, TWeakObjectPtr<ATerritorySpawnAnchor>>& Pair : AnchorsByGuid)
	{
		ATerritorySpawnAnchor* const Anchor = Pair.Value.Get();
		if (!Anchor) continue;

		const FName OwnerKey = Anchor->GetOwnerKey();
		const FName ClaimedBy = Anchor->GetClaimedByPlayerName();

		if (!OwnerKey.IsNone() && ConnectedNames.Contains(OwnerKey) && ClaimedBy != OwnerKey)
		{
			// BindOwnerKeyToPlayer refuses to steal a live claim held by a different name, so a second
			// player who happens to share this name cannot take an occupied bay. The refusal is silent on
			// purpose: it is the correct outcome, and logging it every sweep would bury the log.
			Anchor->BindOwnerKeyToPlayer(OwnerKey);
		}
		else if (!ClaimedBy.IsNone() && !ConnectedNames.Contains(ClaimedBy))
		{
			// The claimant left. Only the transient claim is dropped; the record keeps its state so a
			// returning player finds their car exactly as they left it. Releasing the claim rather than
			// deleting the slot is what makes a disconnect recoverable instead of destructive.
			Anchor->ReleaseClaim();
		}
	}
}
