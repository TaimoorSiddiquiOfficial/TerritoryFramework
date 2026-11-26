#include "Spawn/TerritorySpawnAnchor.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritorySaveSerialization.h"
#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "SaveSystemStatics.h"
#include "Spawn/TerritorySpawnExecutor.h"
#include "Spawn/TerritorySpawnPolicy.h"
#include "Spawn/TerritorySpawnSubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

// The numeric limits and the debug-log gate both live in TerritorySpawnTypes.h /
// TerritorySpawnTypes.cpp. Nothing file-local is needed here, and deliberately so: unity builds paste
// several .cpp files into one translation unit, where two same-named file-local constants or helpers
// become ambiguous symbols rather than two independent copies.
using TerritorySpawnLimits::MaxConsecutiveSpawnFailures;
using TerritorySpawnLimits::HardReturnTimeoutSeconds;

ATerritorySpawnAnchor::ATerritorySpawnAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	// A visual root in the editor and nothing at all in game. The anchor owns no art of its own: the
	// spawned entity brings it. Keeping this a billboard is what lets a car anchor be bIsSpatiallyLoaded
	// false for a few hundred bytes.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		Billboard->SetupAttachment(Root);
		Billboard->bIsScreenSizeScaled = true;
		Billboard->ScreenSize = 0.0025f;
	}
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// INarrativeSavableActor
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritorySpawnAnchor::Serialize(FArchive& Ar)
{
	// Every Territory save path wraps its serialization in this scope: it replaces live values with
	// the class defaults before reading a delta, and forces complete values on write.
	FTerritorySaveSerializationScope SaveScope(*this, Ar);
	Super::Serialize(Ar);
}

FGuid ATerritorySpawnAnchor::GetActorGUID_Implementation() const
{
	return AnchorGuid;
}

void ATerritorySpawnAnchor::SetActorGUID_Implementation(const FGuid& InGUID)
{
	// Only the save system should ever write this, and only when restoring a dynamic actor. A placed
	// anchor keeps the GUID that was baked into the level.
	AnchorGuid = InGUID;
}

void ATerritorySpawnAnchor::PrepareForSave_Implementation()
{
	if (!HasAuthority()) return;

	// The durable half of the record must be stable byte-for-byte across saves, otherwise unrelated
	// commits look like changes and the migration checks cannot tell a real edit from reordering.
	SlotRecord.PrepareScalarsForSave();

	// A slot that streamed out mid-transition must not advertise an instance it no longer tracks.
	if (bRetiringForStreaming && !SlotRecord.bHasInstance)
	{
		SlotRecord.ResetInstanceState();
	}
}

void ATerritorySpawnAnchor::Load_Implementation()
{
	if (!HasAuthority()) return;

	if (!SlotRecord.IsSchemaReadable())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s carries record schema %d; this build reads %d..%d. Leaving the slot dormant rather than overwriting a newer save."),
			*GetPathName(), SlotRecord.SchemaVersion,
			FTerritorySpawnRecord::MinimumSupportedSchemaVersion,
			FTerritorySpawnRecord::CurrentSchemaVersion);
		SlotRecord.bHasInstance = false;
		LifecycleState = ESpawnLifecycleState::Dormant;
		return;
	}

	// A record whose transform cannot be trusted is a corrupt record. Refuse the instance and report
	// it; fabricating a position would put an entity somewhere no designer chose.
	if (SlotRecord.bHasInstance && !SlotRecord.IsTransformUsable())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s restored an unusable transform (%s). Clearing the instance instead of placing it at an invented position."),
			*GetPathName(), *SlotRecord.Transform.ToHumanReadableString());
		SlotRecord.bHasInstance = false;
		SlotRecord.Transform = FTransform::Identity;
		LifecycleState = ESpawnLifecycleState::Dormant;
		return;
	}

	// The record's word is the slot's starting state. Whether an entity is actually created is the
	// sweep's decision, which is why nothing is spawned here.
	LifecycleState = SlotRecord.bHasInstance
		? ESpawnLifecycleState::PendingSpawn
		: ESpawnLifecycleState::Dormant;
	PendingSinceGameTime = SlotRecord.bHasInstance && GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	ConsecutiveFailures = 0;

	if (ShouldLogTerritorySpawn())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[Spawn] Anchor %s loaded schema %d, has instance %d, generation %d"),
			*GetPathName(), SlotRecord.SchemaVersion, SlotRecord.bHasInstance ? 1 : 0, SlotRecord.Generation);
	}
}

bool ATerritorySpawnAnchor::ShouldRespawn_Implementation() const
{
	// An anchor is level-placed. The save system must never try to recreate it: it owns its children,
	// so a duplicated anchor would spawn a second entity into the same slot.
	return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Editor identity
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritorySpawnAnchor::EnsurePersistentAnchorGUID()
{
	if (AnchorGuid.IsValid()) return;

	AnchorGuid = FGuid::NewGuid();
#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

#if WITH_EDITOR
void ATerritorySpawnAnchor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Bake the durable identity at edit time, never at construction of the class default object.
	//
	// The CDO and archetype guards are not defensive noise: without them every class default would get
	// a GUID, and every level that inherited from it would then carry the same baked identity. The
	// !IsGameWorld() guard is the other half: a runtime-spawned anchor must never bake itself a
	// persistent GUID, because a GUID minted in a game world would differ per run and the saved record
	// could never be found again.
	if (GetWorld() && !GetWorld()->IsGameWorld()
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		EnsurePersistentAnchorGUID();
	}
}

void ATerritorySpawnAnchor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EnsurePersistentAnchorGUID();

	// Re-resolve the layers so an author who swaps a policy immediately sees whether the executor
	// agrees with it, instead of finding out at BeginPlay in PIE.
	if (Policy)
	{
		ResolveLayers();
	}
}

bool ATerritorySpawnAnchor::IsInAuthoringDraft() const
{
#if WITH_EDITOR
	// True while a designer is still filling the slot in. Two cases, and both are legitimate:
	//
	//   1. The world is not a game world, so this is the editor acting on a placed anchor.
	//   2. Play has not begun, so nothing can have been spawned or saved yet.
	//
	// The distinction exists because ResolveLayers logs an ERROR for an incomplete policy, and those
	// errors are correct at runtime but wrong at authoring time. A designer who drops an anchor on the
	// map, then assigns the policy, then assigns the tag, would otherwise produce two red lines in the
	// log for a slot that is working perfectly — and the log is a release gate, so the noise is not
	// cosmetic. It is the same reasoning the GUID bake already uses: do not complain about a workflow
	// state the engine itself creates.
	const UWorld* const World = GetWorld();
	return World != nullptr && (!World->IsGameWorld() || !HasActorBegunPlay());
#else
	return false;
#endif
}

void ATerritorySpawnAnchor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// Always regenerate, unconditionally. A duplicated anchor that kept its GUID would write into the
	// original's saved record, and the save system would then have two actors claiming one identity.
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		AnchorGuid = FGuid::NewGuid();
		MarkPackageDirty();
	}
}
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritorySpawnAnchor::BeginPlay()
{
	bRetiringForStreaming = false;
	Super::BeginPlay();

	if (!AnchorGuid.IsValid())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s has no editor-baked AnchorGuid; save/load and the sweep registry are disabled for this slot."),
			*GetPathName());
		bAnchorReady = false;
		return;
	}

	if (!ResolveLayers())
	{
		// Resolution already logged the reason. A half-configured anchor stays registered so the
		// validator and the debug trace can see it, but it never acts.
		bAnchorReady = false;
		RegisterWithSubsystem();
		return;
	}

	bAnchorReady = true;
	RegisterWithSubsystem();
}

void ATerritorySpawnAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bRetiringForStreaming = EndPlayReason == EEndPlayReason::RemovedFromWorld;

	// Streaming retirement is not a destroy: the record must be written so the slot comes back with
	// its condition when the cell returns. A real removal clears the record instead, otherwise a
	// deleted anchor leaves an orphaned record behind forever.
	if (HasAuthority() && AnchorGuid.IsValid())
	{
		if (EndPlayReason == EEndPlayReason::RemovedFromWorld)
		{
			// Never save during a load: the record on disk is newer than anything this actor holds.
			bool bIsLoading = false;
			if (UWorld* World = GetWorld())
			{
				if (UNarrativeSaveSubsystem* SaveSubsystem = World->GetSubsystem<UNarrativeSaveSubsystem>())
				{
					bIsLoading = SaveSubsystem->IsLoading();
				}
			}

			if (!bIsLoading)
			{
				PrepareForSave_Implementation();
				if (!USaveSystemStatics::SaveSingleActor(this))
				{
					UE_LOG(LogTerritory, Error,
						TEXT("[Spawn] Anchor %s streamed out but its record could not be written; the slot's condition will be lost."),
						*GetPathName());
				}
			}
		}
		else if (EndPlayReason == EEndPlayReason::Destroyed)
		{
			USaveSystemStatics::RemoveSingleActor(this);
		}
	}

	UnregisterFromSubsystem();
	bAnchorReady = false;
	ResolvedExecutor = nullptr;
	SpawnedActor.Reset();

	Super::EndPlay(EndPlayReason);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Layers
// ═══════════════════════════════════════════════════════════════════════════════

UTerritorySpawnExecutor* GetExecutorForResolution(ATerritorySpawnAnchor* Anchor, UTerritorySpawnPolicy* Policy,
	UTerritorySpawnExecutor* Override)
{
	if (Override) return Override;
	if (!Policy) return nullptr;

	const TSubclassOf<UTerritorySpawnExecutor> ExecutorClass = Policy->GetExecutorClass();
	if (!ExecutorClass) return nullptr;

	// A per-anchor instance, never the class-default object. Two anchors sharing one CDO would share
	// every piece of executor scratch state, which is a bug that produces no crash and no log.
	return NewObject<UTerritorySpawnExecutor>(Anchor, ExecutorClass);
}

bool ATerritorySpawnAnchor::ResolveLayers()
{
	ResolvedExecutor = nullptr;

	if (!Policy)
	{
		// Null policy is legal and inert. Warning rather than error: a freshly placed anchor has not
		// been configured yet, and erroring on every unconfigured anchor would bury real failures.
		UE_LOG(LogTerritory, Warning,
			TEXT("[Spawn] Anchor %s has no policy. The slot is inert until one is assigned."),
			*GetPathName());
		return false;
	}

	if (!Policy->IsPolicyConfigured())
	{
		// Severity depends on WHEN. An anchor being filled in by a designer is mid-workflow and gets a
		// warning; the same policy at runtime is an authoring failure that would silently disable the
		// slot, so it stays an error. This used to be an unconditional Error, which meant every
		// intermediate details-panel edit wrote a red line into a log that is a release gate — and six
		// of them were sitting in the PIE log for a slot that worked perfectly.
		if (IsInAuthoringDraft())
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[Spawn] Anchor %s policy %s is not configured yet (missing entity tag or executor). It cannot run until one is assigned."),
				*GetPathName(), *GetNameSafe(Policy));
		}
		else
		{
			UE_LOG(LogTerritory, Error,
				TEXT("[Spawn] Anchor %s policy %s reports an incomplete configuration (missing entity tag or executor)."),
				*GetPathName(), *GetNameSafe(Policy));
		}
		return false;
	}

	UTerritorySpawnExecutor* Candidate = GetExecutorForResolution(this, Policy, ExecutorOverride);
	if (!Candidate)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s could not instantiate an executor for policy %s."),
			*GetPathName(), *GetNameSafe(Policy));
		return false;
	}

	// The class the slot will create must be one this executor can actually make. Catching it here
	// turns a silent null spawn at runtime into a loud authoring error.
	//
	// LoadSynchronous() hands back a mutable UClass*, and TSubclassOf has no const-ref overload, so
	// this must stay a non-const pointer. Declaring it const here is a compile error, not a style nit.
	const TSoftClassPtr<AActor> DesiredClass = Policy->GetDesiredActorClass();
	UClass* const DesiredResolved = DesiredClass.IsNull()
		? ActorClass.LoadSynchronous()
		: DesiredClass.LoadSynchronous();

	if (!DesiredResolved)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s resolves no actor class: neither the policy nor the anchor names one, or the class failed to load."),
			*GetPathName());
		// The executor is kept even though the class is unresolved. The sweep polls the class every
		// pass, so a class that is merely not loaded yet must not lose its executor.
		ResolvedExecutor = Candidate;
		return false;
	}

	if (!Candidate->SupportsActorClass(DesiredResolved))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] Anchor %s executor %s refuses actor class %s. The slot stays inert rather than spawning something the executor cannot manage."),
			*GetPathName(), *GetNameSafe(Candidate), *DesiredResolved->GetName());
		return false;
	}

	ResolvedExecutor = Candidate;
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void ATerritorySpawnAnchor::SetPolicyForTesting(UTerritorySpawnPolicy* InPolicy)
{
	Policy = InPolicy;

	// BeginPlay has already run by the time a test assigns a policy, and it set bAnchorReady to false
	// because a null policy is a legal inert state. Re-resolve here and adopt readiness only when the
	// layers genuinely resolve, so this helper cannot make a broken anchor look configured.
	bAnchorReady = ResolveLayers();
}
#endif

bool ATerritorySpawnAnchor::IsAnchorConfigured() const
{
	return bAnchorReady && AnchorGuid.IsValid() && Policy != nullptr && GetExecutor() != nullptr;
}

bool ATerritorySpawnAnchor::IsEvaluationBlocked() const
{
	// Saving or loading is the one window where a spawned or destroyed actor would corrupt the
	// snapshot being written or restore state into an actor that is about to be replaced.
	if (UWorld* World = GetWorld())
	{
		if (World->bIsTearingDown) return true;
		if (UNarrativeSaveSubsystem* SaveSubsystem = World->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			if (SaveSubsystem->IsLoading()) return true;
		}
	}

	return !bAnchorReady || !HasActorBegunPlay();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Record commits
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritorySpawnAnchor::CommitTransition(ESpawnLifecycleState NewState,
	ESpawnTransitionReason Reason, double NowGameTime)
{
	const ESpawnLifecycleState PreviousState = LifecycleState;

	// Retired is terminal for the session. Re-entering it would log the same reason twice and, worse,
	// let a later policy answer quietly revive a slot the framework already gave up on.
	if (PreviousState == ESpawnLifecycleState::Retired && NewState != ESpawnLifecycleState::Retired)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Spawn] Anchor %s is retired; ignoring a transition to state %d from reason %d."),
			*GetPathName(), static_cast<int32>(NewState), static_cast<int32>(Reason));
		return;
	}

	LifecycleState = NewState;
	SlotRecord.LastTransitionGameTime = NowGameTime;

	// PendingSpawn owns a deadline. Every other state clears it, so a stale pending timestamp can never
	// make a healthy slot look like it has been waiting for minutes.
	if (NewState == ESpawnLifecycleState::PendingSpawn)
	{
		if (PendingSinceGameTime < 0.0)
		{
			PendingSinceGameTime = NowGameTime;
		}
	}
	else
	{
		PendingSinceGameTime = -1.0;
	}

	if (ShouldLogTerritorySpawn())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[Spawn] Anchor %s %s -> %s (%s) at %.2f"),
			*GetPathName(),
			*StaticEnum<ESpawnLifecycleState>()->GetNameStringByValue(static_cast<int64>(PreviousState)),
			*StaticEnum<ESpawnLifecycleState>()->GetNameStringByValue(static_cast<int64>(NewState)),
			*StaticEnum<ESpawnTransitionReason>()->GetNameStringByValue(static_cast<int64>(Reason)),
			NowGameTime);
	}
}

void ATerritorySpawnAnchor::SetTrackedActor(AActor* InActor)
{
	SpawnedActor = InActor;
}

void ATerritorySpawnAnchor::ResetToDormant(ESpawnRemovalReason Reason, bool bTreatAsDestroyed)
{
	if (!HasAuthority()) return;

	if (SpawnedActor.IsValid())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Spawn] Anchor %s reset to dormant while still tracking %s; the actor is no longer owned by this slot."),
			*GetPathName(), *GetNameSafe(SpawnedActor.Get()));
	}

	const double NowGameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;

	SpawnedActor.Reset();

	// The two ways a slot empties are different facts and must land in the record differently.
	//
	// ResetInstanceState wipes bWasDestroyed along with the per-instance half, which is right for a
	// clean admin reset or a first-time load but WRONG for an entity that vanished on its own. A prop
	// authored as InitialSpawn must stay gone once it is destroyed; if the wipe erased that flag, the
	// next sweep would read "never spawned" and quietly rebuild it — the exact silent resurrection the
	// recovery mode exists to prevent.
	//
	// So: remember whether the slot believed it held something, wipe, then write the fact back.
	const bool bBelievedItHeldSomething = SlotRecord.bHasInstance;
	SlotRecord.ResetInstanceState();

	if (bTreatAsDestroyed && (bBelievedItHeldSomething || SlotRecord.Generation > 0))
	{
		SlotRecord.bWasDestroyed = true;
		// Keep the timestamp meaningful: the recovery delay is measured from it, and a wiped -1.0
		// would make a rebuild-if-lost slot respawn instantly regardless of its authored delay.
		SlotRecord.LastTransitionGameTime = NowGameTime;
	}

	ConsecutiveFailures = 0;

	if (NowGameTime >= 0.0)
	{
		CommitTransition(ESpawnLifecycleState::Dormant, ESpawnTransitionReason::PolicyRequest,
			NowGameTime);
	}
	else
	{
		LifecycleState = ESpawnLifecycleState::Dormant;
		PendingSinceGameTime = -1.0;
	}

	if (ShouldLogTerritorySpawn())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Spawn] Anchor %s reset to dormant (%s), destroyed %d"),
			*GetPathName(),
			*StaticEnum<ESpawnRemovalReason>()->GetNameStringByValue(static_cast<int64>(Reason)),
			SlotRecord.bWasDestroyed ? 1 : 0);
	}
}

int32 ATerritorySpawnAnchor::RegisterSpawnFailure()
{
	ConsecutiveFailures = FMath::Min(ConsecutiveFailures + 1, TerritorySpawnLimits::MaxConsecutiveSpawnFailures + 1);
	return ConsecutiveFailures;
}

void ATerritorySpawnAnchor::RegisterSpawnSuccess()
{
	ConsecutiveFailures = 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Ownership
// ═══════════════════════════════════════════════════════════════════════════════

bool ATerritorySpawnAnchor::TryClaimForPlayer(FName InPlayerName)
{
	if (!HasAuthority() || InPlayerName.IsNone()) return false;

	if (!ClaimedByPlayerName.IsNone() && ClaimedByPlayerName != InPlayerName)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Spawn] Anchor %s is already claimed by %s; %s was refused rather than stealing the slot."),
			*GetPathName(), *ClaimedByPlayerName.ToString(), *InPlayerName.ToString());
		return false;
	}

	ClaimedByPlayerName = InPlayerName;
	return true;
}

void ATerritorySpawnAnchor::ReleaseClaim()
{
	if (!HasAuthority()) return;

	// Deliberately only the claim moves. The record keeps the slot's condition so a returning player
	// finds their car as they left it; deleting durable state here would be a silent loss.
	ClaimedByPlayerName = FName();
}

bool ATerritorySpawnAnchor::BindOwnerKeyToPlayer(FName InPlayerName)
{
	if (!HasAuthority() || InPlayerName.IsNone()) return false;

	if (!OwnerKey.IsNone() && OwnerKey != InPlayerName)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Spawn] Anchor %s belongs to owner key %s and will not be rebound to %s."),
			*GetPathName(), *OwnerKey.ToString(), *InPlayerName.ToString());
		return false;
	}

	OwnerKey = InPlayerName;
	return TryClaimForPlayer(InPlayerName);
}

bool ATerritorySpawnAnchor::IsOwnedByPlayer(FName InPlayerName) const
{
	if (InPlayerName.IsNone()) return false;
	return !OwnerKey.IsNone() && OwnerKey == InPlayerName;
}

#if WITH_DEV_AUTOMATION_TESTS
void ATerritorySpawnAnchor::SetOwnerKeyForTesting(FName InOwnerKey)
{
	OwnerKey = InOwnerKey;
}

void ATerritorySpawnAnchor::SimulateLoadedRecordForTesting(bool bHadInstance, bool bWasDestroyed)
{
	SlotRecord.ResetInstanceState();
	SlotRecord.Generation = bHadInstance ? 3 : 0;
	SlotRecord.bHasInstance = bHadInstance;
	SlotRecord.bWasDestroyed = bWasDestroyed;
	SlotRecord.Transform = GetActorTransform();
	SlotRecord.LastTransitionGameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	SlotRecord.SchemaVersion = FTerritorySpawnRecord::CurrentSchemaVersion;

	// Run the real path. A save restore goes through Load_Implementation and nothing else, so a test that
	// stopped at the write would be measuring its own setup.
	Load_Implementation();
}
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Registry
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritorySpawnAnchor::RegisterWithSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>())
		{
			Subsystem->RegisterAnchor(this);
		}
	}
}

void ATerritorySpawnAnchor::UnregisterFromSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritorySpawnSubsystem* Subsystem = World->GetSubsystem<UTerritorySpawnSubsystem>())
		{
			Subsystem->UnregisterAnchor(this);
		}
	}
}
