#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "SaveSystemStatics.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Crc.h"
#include "Engine/World.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Character/CharacterDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "Tales/NarrativeCondition.h"

ATerritoryVolume::ATerritoryVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	BoundsShape = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsShape"));
	RootComponent = BoundsShape;

	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		Box->SetBoxExtent(FVector(500.f, 500.f, 200.f));
		Box->SetHiddenInGame(true, true);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Box->SetCanEverAffectNavigation(false);
		Box->bHiddenInGame = true;
		Box->bVisibleInReflectionCaptures = false;
	}

	// Proper subcomponent — visible in BP editor Components panel
	MapMarkerComponent = CreateDefaultSubobject<UTerritoryNavigationMarkerComponent>(TEXT("MapMarkerComponent"));
}

void ATerritoryVolume::BeginPlay()
{
	Super::BeginPlay();

	// Force-disable collision on the BoundShape at runtime.
	// Blueprint CDO may override the constructor's NoCollision setting,
	// causing guards/enemies to collide with the volume and float on hit.
	if (BoundsShape)
	{
		BoundsShape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoundsShape->SetGenerateOverlapEvents(false);
		BoundsShape->SetCanEverAffectNavigation(false);
		BoundsShape->SetVisibility(false, true); // Hide in game
	}

	if (HasAuthority())
	{
		// GUID must be baked at editor placement time.
		// If missing here, it means the level wasn't saved after GUID baking.
		if (!TerritoryGUID.IsValid())
		{
			if (TerritoryTag.IsValid())
			{
				// Deterministic fallback — NOT ideal but prevents total breakage
				uint32 Hash = FCrc::StrCrc_DEPRECATED(*TerritoryTag.ToString());
				TerritoryGUID = FGuid(Hash, 0, 0, 0);
			}
			UE_LOG(LogTerritory, Error,
				TEXT("%s has no TerritoryGUID. Open the level in editor, select the actor, "
					"move it slightly, and save the level to bake a GUID."),
				*GetPathName());
		}

		bool bSuccessfullyLoaded = USaveSystemStatics::LoadSingleActor(this);

		if (!bSuccessfullyLoaded && !bLoadedFromSave)
		{
			// Fresh territory — initialize from level defaults
			OwnershipData.MaxConcurrentAttackers = InitialMaxConcurrentAttackers;
			OwnershipData.PeriodicIncome = InitialPeriodicIncome;
			OwnershipData.GuardCost = InitialGuardCost;

			if (InitialOwningFaction.IsValid())
			{
				OwnershipData.OwningFaction = InitialOwningFaction;
				OwnershipData.State = ETerritoryState::Claimed;
			}

			if (bStartsLocked)
			{
				OwnershipData.State = ETerritoryState::Locked;
			}
		}
		else
		{
			// Save loaded — sync level-config settings only
			OwnershipData.MaxConcurrentAttackers = InitialMaxConcurrentAttackers;
			OwnershipData.PeriodicIncome = InitialPeriodicIncome;
			OwnershipData.GuardCost = InitialGuardCost;
		}

		PreviousOwningFaction = OwnershipData.OwningFaction;
		PreviousState = OwnershipData.State;

		{
			const UTerritoryDeveloperSettings* DevSettings = GetDefault<UTerritoryDeveloperSettings>();
			if (DevSettings && DevSettings->ShouldDebugSaveLoad())
			{
				UE_LOG(LogTerritory, Log, TEXT("[SaveLoad] %s BeginPlay: owner=%s, state=%d, loaded=%d"),
					*GetTerritoryTag().ToString(),
					*OwnershipData.OwningFaction.ToString(),
					static_cast<int32>(OwnershipData.State),
					bSuccessfullyLoaded || bLoadedFromSave ? 1 : 0);
			}
		}

		ReconcileGuardsAfterLoad();
	}

	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->RegisterTerritory(this);
		LastKnownBounds = GetTerritoryBounds();
	}

	// Fire BP-exposed initialization event
	OnTerritoryInitialized();

	// Enable ticking only when debug visual draw is enabled (PIE only)
#if ENABLE_DRAW_DEBUG
	const UTerritoryDeveloperSettings* DebugSettings = GetDefault<UTerritoryDeveloperSettings>();
	if (DebugSettings && DebugSettings->IsDebugEnabled()
		&& (DebugSettings->bDrawTerritoryBounds || DebugSettings->bDrawOwnershipOverlay
			|| DebugSettings->bDrawCaptureProgress || DebugSettings->bDrawGuardSpawnPoints
			|| DebugSettings->bDrawSpatialGrid))
	{
		SetActorTickEnabled(true);
	}
#endif
}

void ATerritoryVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->UnregisterTerritory(this);
	}

	// Clean up spawned guards
	for (TWeakObjectPtr<ATerritoryGuardCharacter>& GuardPtr : SpawnedGuards)
	{
		if (GuardPtr.IsValid())
		{
			GuardPtr->Destroy();
		}
	}
	SpawnedGuards.Empty();

	for (const TWeakObjectPtr<AActor>& DefenderPtr : RegisteredDefenders)
	{
		if (DefenderPtr.IsValid())
		{
			UnbindDefenderDeath(DefenderPtr.Get());
		}
	}
	RegisteredDefenders.Empty();

	Super::EndPlay(EndPlayReason);
}

void ATerritoryVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (!Settings || !Settings->IsDebugEnabled()) return;

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;

	const FVector Center = GetActorLocation();
	const FBox Bounds = GetTerritoryBounds();
	FQuat BoxRotation = FQuat::Identity;
	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		BoxRotation = Box->GetComponentQuat();
	}

	// Draw territory bounds
	if (Settings->bDrawTerritoryBounds)
	{
		DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), BoxRotation,
			FColor::White, false, 0.f, 0, 1.f);
	}

	// Draw ownership color overlay
	if (Settings->bDrawOwnershipOverlay && OwnershipData.OwningFaction.IsValid())
	{
		DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), BoxRotation,
			FColor::Green, false, 0.f, 1, 2.f);
	}

	// Draw capture progress bar above contested territories
	if (Settings->bDrawCaptureProgress && OwnershipData.State == ETerritoryState::Contested)
	{
		const FVector TopCenter(Center.X, Center.Y, Bounds.Max.Z + 100.f);
		const float BarWidth = 200.f;
		const float Progress = OwnershipData.ControlProgress;
		DrawDebugLine(World, TopCenter - FVector(BarWidth * 0.5f, 0, 0),
			TopCenter + FVector(BarWidth * Progress - BarWidth * 0.5f, 0, 0),
			FColor::Red, false, 0.f, 0, 3.f);
		DrawDebugLine(World, TopCenter - FVector(BarWidth * 0.5f, 0, 0),
			TopCenter + FVector(BarWidth * 0.5f, 0, 0),
			FColor::White, false, 0.f, 1, 1.f);
		DrawDebugString(World, TopCenter + FVector(0, 0, 20.f),
			FString::Printf(TEXT("%.0f%%"), Progress * 100.f),
			nullptr, FColor::White, 0.f, true);
	}

	// Draw guard spawn points and patrol routes
	if (Settings->bDrawGuardSpawnPoints)
	{
		for (const TObjectPtr<AActor>& SPActor : GuardSpawnPoints)
		{
			if (ATerritoryGuardSpawnPoint* SP = Cast<ATerritoryGuardSpawnPoint>(SPActor))
			{
				DrawDebugSphere(World, SP->GetActorLocation(), 30.f, 8, FColor::Yellow, false, 0.f, 0, 1.f);

				// Draw patrol route
				const TArray<FTerritoryPatrolNode>& Route = SP->PatrolRoute;
				for (int32 i = 0; i < Route.Num(); ++i)
				{
					const FVector& Node = Route[i].Location;
					DrawDebugSphere(World, Node, 15.f, 6, FColor::Cyan, false, 0.f, 0, 0.5f);
					if (i + 1 < Route.Num())
					{
						DrawDebugLine(World, Node, Route[i + 1].Location, FColor::Cyan, false, 0.f, 0, 1.f);
					}
					else if (SP->bLoopPatrol && Route.Num() > 1)
					{
						DrawDebugLine(World, Node, Route[0].Location, FColor::Turquoise, false, 0.f, 0, 0.5f);
					}
				}
			}
		}
	}
#endif
}

void ATerritoryVolume::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerritoryVolume, OwnershipData);
}

// ═══════════════════════════════════════════════════════════════════════════════
// RepNotify — Clients receive replicated ownership changes here
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritoryVolume::OnRep_OwnershipData()
{
	// Diff against cached values — only fire events for fields that actually changed
	if (PreviousOwningFaction != OwnershipData.OwningFaction)
	{
		// Fire cosmetic BP event on clients — invariants already ran on authority
		if (!HasAuthority())
		{
			OnOwnershipChanged(PreviousOwningFaction, OwnershipData.OwningFaction);
		}
		OnTerritoryOwnershipChanged.Broadcast(this, PreviousOwningFaction, OwnershipData.OwningFaction);
		UE_LOG(LogTerritory, Verbose, TEXT("[Client] %s ownership: %s → %s"),
			*GetTerritoryTag().ToString(),
			*PreviousOwningFaction.ToString(),
			*OwnershipData.OwningFaction.ToString());
		PreviousOwningFaction = OwnershipData.OwningFaction;
	}

	if (PreviousState != OwnershipData.State)
	{
		// Fire cosmetic BP event on clients — invariants already ran on authority
		if (!HasAuthority())
		{
			OnStateChanged(PreviousState, OwnershipData.State);
		}
		OnTerritoryStateChangedDelegate.Broadcast(this, OwnershipData.State);
		UE_LOG(LogTerritory, Verbose, TEXT("[Client] %s state → %d"),
			*GetTerritoryTag().ToString(), static_cast<int32>(OwnershipData.State));
		PreviousState = OwnershipData.State;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Editor Hooks — Generate stable GUIDs at edit time
// ═══════════════════════════════════════════════════════════════════════════════

#if WITH_EDITOR
void ATerritoryVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-generate GUID if not yet assigned (first time placed or edited)
	if (!TerritoryGUID.IsValid())
	{
		TerritoryGUID = FGuid::NewGuid();
		UE_LOG(LogTerritory, Log, TEXT("Generated editor-stable GUID for %s: %s"),
			*GetName(), *TerritoryGUID.ToString());
	}
}

void ATerritoryVolume::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// PIE world creation uses StaticDuplicateObject — must NOT regenerate GUID.
	// Only regenerate for actual editor duplication (user Ctrl+D).
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		TerritoryGUID = FGuid::NewGuid();
		MarkPackageDirty();
	}
}

void ATerritoryVolume::PostActorCreated()
{
	Super::PostActorCreated();

	// Only generate GUID in editor (not during PIE world duplication)
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		EnsurePersistentTerritoryGUID();
	}
}

void ATerritoryVolume::PostEditImport()
{
	Super::PostEditImport();

	// Only regenerate in editor
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		TerritoryGUID = FGuid::NewGuid();
		MarkPackageDirty();
	}
}
#endif

void ATerritoryVolume::EnsurePersistentTerritoryGUID()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	if (!TerritoryGUID.IsValid())
	{
		TerritoryGUID = FGuid::NewGuid();

#if WITH_EDITOR
		Modify();
		MarkPackageDirty();
#endif
	}
}

FGuid ATerritoryVolume::GetActorGUID_Implementation() const { return TerritoryGUID; }
void ATerritoryVolume::SetActorGUID_Implementation(const FGuid& NewGUID) { TerritoryGUID = NewGUID; }
void ATerritoryVolume::PrepareForSave_Implementation() { /* OwnershipData auto-saved via SaveGame */ }

void ATerritoryVolume::Load_Implementation()
{
	bLoadedFromSave = true;

	// Narrative's Serialize(Ar) just restored OwnershipData from the save.
	// Reconcile guards — despawn any stale BeginPlay guards, respawn for loaded owner.
	if (HasAuthority())
	{
		ReconcileGuardsAfterLoad();
	}

	const UTerritoryDeveloperSettings* DevSettings = GetDefault<UTerritoryDeveloperSettings>();
	if (DevSettings && DevSettings->ShouldDebugSaveLoad())
	{
		UE_LOG(LogTerritory, Log, TEXT("[SaveLoad] %s Load_Implementation: owner=%s, state=%d, guards=%d"),
			*GetTerritoryTag().ToString(),
			*OwnershipData.OwningFaction.ToString(),
			static_cast<int32>(OwnershipData.State),
			SpawnedGuards.Num());
	}
}

void ATerritoryVolume::ReconcileGuardsAfterLoad()
{
	if (!HasAuthority()) return;

	// Despawn ALL existing guards (may be stale from BeginPlay initial faction)
	DespawnGuards();

	// Clean stale defender registrations
	RegisteredDefenders.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });
	OwnershipData.DefenderCount = RegisteredDefenders.Num();

	// Spawn guards for the loaded/current owner
	if (OwnershipData.State == ETerritoryState::Claimed
		&& OwnershipData.OwningFaction.IsValid()
		&& ResolveGuardDefinition(OwnershipData.OwningFaction)
		&& GuardSpawnCount > 0)
	{
		SpawnGuards();
	}

	// Sync RepNotify cache
	PreviousOwningFaction = OwnershipData.OwningFaction;
	PreviousState = OwnershipData.State;
}

bool ATerritoryVolume::ShouldRespawn_Implementation() const { return false; }

// ─── ITerritoryOwnershipInterface ───

FGameplayTag ATerritoryVolume::GetTerritoryOwner_Implementation() const { return OwnershipData.OwningFaction; }
float ATerritoryVolume::GetTerritoryControlProgress_Implementation() const { return OwnershipData.ControlProgress; }
bool ATerritoryVolume::IsTerritoryContested_Implementation() const { return OwnershipData.State == ETerritoryState::Contested; }
FGameplayTag ATerritoryVolume::GetContestingFaction_Implementation() const { return OwnershipData.ContestingFaction; }

// ─── ITerritoryEventReceiverInterface ───

void ATerritoryVolume::OnTerritoryControlChanged_Implementation(FGameplayTag InTerritoryTag, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	// Self ownership change handled by OnOwnershipChanged; this is for external territory events
}

void ATerritoryVolume::OnTerritoryContested_Implementation(FGameplayTag InTerritoryTag, FGameplayTag ContestingFaction)
{
	if (InTerritoryTag == TerritoryTag && OwnershipData.ContestingFaction != ContestingFaction)
	{
		OwnershipData.ContestingFaction = ContestingFaction;
	}
}

void ATerritoryVolume::OnTerritoryUncontested_Implementation(FGameplayTag InTerritoryTag)
{
	OwnershipData.ContestingFaction = FGameplayTag();
}

void ATerritoryVolume::OnTerritoryStateChanged_Implementation(FGameplayTag InTerritoryTag, ETerritoryState NewState)
{
	// External state change notification — local state is managed by SetTerritoryState
}

FGameplayTag ATerritoryVolume::GetOwningFaction() const { return OwnershipData.OwningFaction; }
ETerritoryState ATerritoryVolume::GetTerritoryState() const { return OwnershipData.State; }
float ATerritoryVolume::GetControlProgress() const { return OwnershipData.ControlProgress; }
bool ATerritoryVolume::IsContested() const { return OwnershipData.State == ETerritoryState::Contested; }
FGameplayTag ATerritoryVolume::GetTerritoryTag() const { return TerritoryTag; }
FText ATerritoryVolume::GetTerritoryDisplayName() const { return TerritoryDisplayName; }
int32 ATerritoryVolume::GetMaxConcurrentAttackers() const { return OwnershipData.MaxConcurrentAttackers; }
int32 ATerritoryVolume::GetDefenderCount() const { return OwnershipData.DefenderCount; }
int32 ATerritoryVolume::GetPeriodicIncome() const { return OwnershipData.PeriodicIncome; }
int32 ATerritoryVolume::GetGuardCost() const { return OwnershipData.GuardCost; }

bool ATerritoryVolume::IsOwnedByFaction(const FGameplayTag& Faction) const
{
	return OwnershipData.State == ETerritoryState::Claimed && OwnershipData.OwningFaction == Faction;
}

FBox ATerritoryVolume::GetTerritoryBounds() const
{
	return BoundsShape ? BoundsShape->Bounds.GetBox() : FBox(ForceInit);
}

bool ATerritoryVolume::ContainsPoint(const FVector& WorldPoint) const
{
	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		// Transform-space containment — does NOT depend on collision geometry.
		// Transforms the world point into the box's local space and compares
		// against the unscaled box extent. Handles rotated boxes correctly.
		const FTransform& BoxTransform = Box->GetComponentTransform();
		const FVector LocalPoint = BoxTransform.InverseTransformPosition(WorldPoint);
		const FVector Extent = Box->GetUnscaledBoxExtent();
		return FMath::Abs(LocalPoint.X) <= Extent.X
			&& FMath::Abs(LocalPoint.Y) <= Extent.Y
			&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
	}
	if (BoundsShape)
	{
		FBoxSphereBounds Bounds = BoundsShape->CalcBounds(BoundsShape->GetComponentTransform());
		return Bounds.GetBox().IsInside(WorldPoint);
	}
	return false;
}

FGameplayTag ATerritoryVolume::GetParentTerritoryTag() const
{
	return ParentTerritoryTag;
}

FGameplayTag ATerritoryVolume::GetInitialOwningFaction() const
{
	return InitialOwningFaction;
}

void ATerritoryVolume::SetOwningFaction(const FGameplayTag& NewFaction)
{
	if (!HasAuthority()) return;

	FGameplayTag OldOwner = OwnershipData.OwningFaction;
	if (OldOwner == NewFaction) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Ownership] %s: %s → %s"),
			*GetTerritoryTag().ToString(), *OldOwner.ToString(), *NewFaction.ToString());

		if (Settings->IsDebugEnabled())
		{
			const FString Msg = FString::Printf(TEXT("[Territory] %s: %s → %s"),
				*GetTerritoryDisplayName().ToString(),
				*OldOwner.ToString(), *NewFaction.ToString());
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, Msg);
		}
	}

	// Cache previous owner for RepNotify
	PreviousOwningFaction = OldOwner;

	OwnershipData.OwningFaction = NewFaction;
	OwnershipData.State = NewFaction.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
	OwnershipData.ContestingFaction = FGameplayTag();
	OwnershipData.ControlProgress = NewFaction.IsValid() ? 1.f : 0.f;

	// Guard lifecycle invariants — run BEFORE BP virtual so BP can react to final state.
	// Not overridable: despawn old owner guards, spawn new owner guards.
	DespawnGuards();
	if (NewFaction.IsValid() && ResolveGuardDefinition(NewFaction) && GuardSpawnCount > 0)
	{
		SpawnGuards();
	}

	OnOwnershipChanged(OldOwner, NewFaction);
	OnTerritoryOwnershipChanged.Broadcast(this, OldOwner, NewFaction);
}

void ATerritoryVolume::SetControlProgress(float Progress)
{
	if (!HasAuthority()) return;
	OwnershipData.ControlProgress = FMath::Clamp(Progress, 0.f, 1.f);
}

void ATerritoryVolume::SetTerritoryState(ETerritoryState NewState)
{
	if (!HasAuthority()) return;
	ETerritoryState OldState = OwnershipData.State;
	if (OldState == NewState) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugStateTransitions())
	{
		UE_LOG(LogTerritory, Log, TEXT("[StateChange] %s: %d → %d"),
			*GetTerritoryTag().ToString(),
			static_cast<int32>(OldState), static_cast<int32>(NewState));
	}

	OwnershipData.State = NewState;

	// Guard lifecycle invariants — run BEFORE BP virtual.
	// Not overridable: despawn on Contested/Locked, respawn on Claimed-from-Contested.
	if (NewState == ETerritoryState::Locked)
	{
		DespawnGuards();
	}
	else if (NewState == ETerritoryState::Contested && OldState == ETerritoryState::Claimed)
	{
		DespawnGuards();
	}
	else if (NewState == ETerritoryState::Claimed && OldState == ETerritoryState::Contested)
	{
		if (HasAuthority() && ResolveGuardDefinition(OwnershipData.OwningFaction) && GuardSpawnCount > 0 && SpawnedGuards.Num() == 0)
		{
			SpawnGuards();
		}
	}

	OnStateChanged(OldState, NewState);
	OnTerritoryStateChangedDelegate.Broadcast(this, NewState);
}

// ─── Lock System ───

bool ATerritoryVolume::IsLocked() const
{
	return OwnershipData.State == ETerritoryState::Locked;
}

bool ATerritoryVolume::CanUnlock() const
{
	// No lock conditions → always unlockable
	if (LockConditions.Num() == 0) return true;

	// Resolve player context for conditions that need it (e.g., quest completion checks)
	APlayerController* ContextPC = nullptr;
	APawn* ContextPawn = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			ContextPC = PC;
			ContextPawn = PC->GetPawn();
		}
	}

	for (const TObjectPtr<UNarrativeCondition>& Cond : LockConditions)
	{
		if (!Cond) continue;
		if (!Cond->CheckCondition(ContextPawn, ContextPC, nullptr)) return false;
	}
	return true;
}

void ATerritoryVolume::LockTerritory(const FText& Reason)
{
	if (!HasAuthority()) return;

	OwnershipData.LockReason = Reason;
	if (OwnershipData.State != ETerritoryState::Locked)
	{
		SetTerritoryState(ETerritoryState::Locked);
	}

	UE_LOG(LogTerritory, Log, TEXT("[Lock] %s locked: %s"),
		*GetTerritoryTag().ToString(), *Reason.ToString());
}

bool ATerritoryVolume::TryUnlock(bool bForce)
{
	if (!HasAuthority()) return false;
	if (!IsLocked()) return true;

	if (!bForce && !CanUnlock())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Lock] %s unlock blocked — conditions not met"),
			*GetTerritoryTag().ToString());
		return false;
	}

	OwnershipData.LockReason = FText();
	SetTerritoryState(OwnershipData.OwningFaction.IsValid()
		? ETerritoryState::Claimed
		: ETerritoryState::Unclaimed);

	UE_LOG(LogTerritory, Log, TEXT("[Lock] %s unlocked"),
		*GetTerritoryTag().ToString());
	return true;
}

void ATerritoryVolume::RegisterDefender(AActor* Defender)
{
	if (!Defender || !HasAuthority()) return;

	RegisteredDefenders.AddUnique(Defender);
	OwnershipData.DefenderCount = RegisteredDefenders.Num();
	BindDefenderDeath(Defender);
}

void ATerritoryVolume::UnregisterDefender(AActor* Defender)
{
	if (!Defender || !HasAuthority()) return;

	UnbindDefenderDeath(Defender);
	RegisteredDefenders.Remove(Defender);
	CleanupInvalidDefenders();
	OwnershipData.DefenderCount = RegisteredDefenders.Num();
}

TArray<AActor*> ATerritoryVolume::GetRegisteredDefenders() const
{
	TArray<AActor*> Result;
	for (const TWeakObjectPtr<AActor>& Ptr : RegisteredDefenders)
	{
		if (Ptr.IsValid())
		{
			Result.Add(Ptr.Get());
		}
	}
	return Result;
}

void ATerritoryVolume::OnOwnershipChanged_Implementation(FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	// Guard lifecycle invariants are handled in SetOwningFaction (non-virtual).
	// This virtual exists for BP subclasses to add behavior — calling Super is optional.
}

void ATerritoryVolume::OnStateChanged_Implementation(ETerritoryState OldState, ETerritoryState NewState)
{
	// Guard lifecycle invariants are handled in SetTerritoryState (non-virtual).
	// This virtual exists for BP subclasses to add behavior — calling Super is optional.
}

void ATerritoryVolume::OnAllGuardsDefeated_Implementation()
{
	UE_LOG(LogTerritory, Log, TEXT("[GuardDeath] All guards defeated in %s — territory is now undefended"),
		*GetTerritoryTag().ToString());

	if (HasAuthority())
	{
		// Territory becomes unclaimed — owner lost all defenders.
		// Marker turns red. Player (or any faction) can now claim it.
		SetOwningFaction(FGameplayTag());
		SetControlProgress(0.f);

		// Transition to Unclaimed so capture flow can begin
		if (OwnershipData.State != ETerritoryState::Locked)
		{
			SetTerritoryState(ETerritoryState::Unclaimed);
		}

		// Do NOT respawn guards here — the territory is undefended.
		// Guards only respawn when a new faction claims the territory.
	}
}

void ATerritoryVolume::OnTerritoryInitialized_Implementation()
{
}

void ATerritoryVolume::OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC)
{
	UnregisterDefender(KilledActor);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugDeaths = Settings && Settings->ShouldDebugGuardDeaths();

	if (bDebugDeaths)
	{
		UE_LOG(LogTerritory, Log, TEXT("[GuardDeath] %s died in %s (remaining: %d)"),
			KilledActor ? *KilledActor->GetName() : TEXT("null"),
			*GetTerritoryTag().ToString(),
			GetDefenderCount());
	}

	// Broadcast purpose-specific guard death delegate
	AActor* Killer = nullptr;
	if (KilledASC && KilledASC->AbilityActorInfo.IsValid())
	{
		if (AActor* Avatar = KilledASC->AbilityActorInfo->AvatarActor.Get())
		{
			// The ASC's LastDamageInstigator may be available via GameplayEffectSpec
			// For now use the avatar as a best-effort killer attribution
		}
	}
	OnGuardKilled.Broadcast(this, KilledActor, Killer, GetDefenderCount());

	// Notify spawn points that a guard died (triggers reserve replacement)
	if (ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(KilledActor))
	{
		for (const TObjectPtr<AActor>& SPActor : GuardSpawnPoints)
		{
			if (ATerritoryGuardSpawnPoint* SP = Cast<ATerritoryGuardSpawnPoint>(SPActor))
			{
				SP->UnregisterGuard(Guard);
			}
		}
	}

	// Remove from spawned guards list
	SpawnedGuards.RemoveAll([KilledActor](const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == KilledActor;
	});

	// If all guards are dead, fire OnAllGuardsDefeated
	if (SpawnedGuards.Num() == 0 && HasGuardsAlive() == false)
	{
		UE_LOG(LogTerritory, Log, TEXT("[GuardDeath] All guards defeated in %s"),
			*GetTerritoryTag().ToString());
		OnAllGuardsDefeated();
		OnAllGuardsDefeatedDelegate.Broadcast(this);
	}
}

void ATerritoryVolume::BindDefenderDeath(AActor* Defender)
{
	if (!Defender) return;
	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Defender))
	{
		if (UNarrativeAbilitySystemComponent* ASC =
			Cast<UNarrativeAbilitySystemComponent>(ASCInterface->GetAbilitySystemComponent()))
		{
			ASC->OnDied.AddUniqueDynamic(this, &ATerritoryVolume::OnDefenderDied);
		}
	}
}

void ATerritoryVolume::UnbindDefenderDeath(AActor* Defender)
{
	if (!Defender) return;
	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Defender))
	{
		if (UNarrativeAbilitySystemComponent* ASC =
			Cast<UNarrativeAbilitySystemComponent>(ASCInterface->GetAbilitySystemComponent()))
		{
			ASC->OnDied.RemoveDynamic(this, &ATerritoryVolume::OnDefenderDied);
		}
	}
}

void ATerritoryVolume::CleanupInvalidDefenders()
{
	RegisteredDefenders.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });
}

void ATerritoryVolume::CheckBoundsForReindex()
{
	FBox CurrentBounds = GetTerritoryBounds();
	if (!CurrentBounds.Equals(LastKnownBounds))
	{
		LastKnownBounds = CurrentBounds;
		if (UWorld* World = GetWorld())
		{
			if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				Registry->UpdateTerritoryBounds(this);
			}
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Guard Spawning
// ═══════════════════════════════════════════════════════════════════════════════

UNPCDefinition* ATerritoryVolume::ResolveGuardDefinition(const FGameplayTag& Faction) const
{
	// Check faction-specific definitions first
	for (const FTerritoryFactionGuardDefinition& Entry : FactionGuardDefinitions)
	{
		if (Entry.Faction == Faction && Entry.NPCDefinition)
		{
			return Entry.NPCDefinition;
		}
	}
	// Fall back to default
	return GuardNPCDefinition;
}

void ATerritoryVolume::SpawnGuards()
{
	if (!HasAuthority()) return;

	// Resolve definition for current owner faction
	FGameplayTag OwnerFaction = OwnershipData.OwningFaction;
	UNPCDefinition* EffectiveDef = ResolveGuardDefinition(OwnerFaction);
	if (!EffectiveDef) return;

	// Prevent double-spawn if guards already exist (check live count, not array size —
	// array may contain stale weak pointers from destroyed guards)
	if (GetSpawnedGuardCount() > 0) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugGuards();

	// Determine NPC class from resolved definition — sync load
	UClass* NPCClass = EffectiveDef->NPCClassPath.LoadSynchronous();
	if (!NPCClass || !NPCClass->IsChildOf(ATerritoryGuardCharacter::StaticClass()))
	{
		NPCClass = ATerritoryGuardCharacter::StaticClass();
	}

	if (!OwnerFaction.IsValid())
	{
		UE_LOG(LogTerritory, Warning, TEXT("SpawnGuards: territory %s has no OwningFaction, skipping"),
			*GetTerritoryTag().ToString());
		return;
	}

	// Resolve GuardSpawnPoints
	TArray<ATerritoryGuardSpawnPoint*> SpawnPointActors = GetGuardSpawnPoints();
	// Sort by priority (higher = fills first)
	SpawnPointActors.Sort([](const ATerritoryGuardSpawnPoint& A, const ATerritoryGuardSpawnPoint& B)
	{
		return A.Priority > B.Priority;
	});
	int32 NextSPIdx = 0;

	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("SpawnGuards: %s spawning %d guards, faction=%s, spawn points=%d"),
			*GetTerritoryTag().ToString(), GuardSpawnCount, *OwnerFaction.ToString(),
			SpawnPointActors.Num());
	}

	for (int32 i = 0; i < GuardSpawnCount; ++i)
	{
		FTransform SpawnTransform;
		ATerritoryGuardSpawnPoint* UsedSP = nullptr;

		// Use GuardSpawnPoints if available, otherwise random within bounds
		if (SpawnPointActors.Num() > 0)
		{
			// Find next available spawn point — active slots only, NOT reserves
			for (int32 j = 0; j < SpawnPointActors.Num(); ++j)
			{
				ATerritoryGuardSpawnPoint* SP = SpawnPointActors[(NextSPIdx + j) % SpawnPointActors.Num()];
				if (SP->HasAvailableSlot())
				{
					UsedSP = SP;
					SpawnTransform = SP->GetSpawnTransform();
					NextSPIdx += j + 1;
					break;
				}
			}
			// Fallback: all points full, use random
			if (!UsedSP)
			{
				if (bDebug) UE_LOG(LogTerritory, Warning, TEXT("  All spawn points full, using random"));
				SpawnTransform = FTransform(FRotator(0, FMath::FRandRange(0.f, 360.f), 0), GetRandomSpawnPoint());
			}
		}
		else
		{
			SpawnTransform = FTransform(FRotator(0, FMath::FRandRange(0.f, 360.f), 0), GetRandomSpawnPoint());
		}

		// Deferred spawning for save system GUID safety
		ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(
			UGameplayStatics::BeginDeferredActorSpawnFromClass(
				this, NPCClass, SpawnTransform,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
				this));

		if (!Guard)
		{
			UE_LOG(LogTerritory, Warning, TEXT("Failed deferred spawn guard %d/%d of %s"),
				i + 1, GuardSpawnCount, *GetTerritoryTag().ToString());
			continue;
		}

		FGuid GuardSaveGUID = FGuid::NewGuid();

		// Determine effective faction: spawn point override > territory owner
		FGameplayTag EffectiveFaction = OwnerFaction;
		if (UsedSP && UsedSP->FactionOverride.IsValid())
		{
			EffectiveFaction = UsedSP->FactionOverride;
		}

		// Single deterministic entrypoint — fills ALL SpawnInfo fields
		Guard->ConfigureTerritorySpawn(
			EffectiveDef,
			EffectiveFaction,
			TerritoryGUID,
			GuardSaveGUID,
			SpawnTransform,
			UsedSP ? UsedSP->GetFName() : NAME_None);

		// Set territory AI context before FinishSpawningActor
		Guard->OwningTerritory = this;
		Guard->OwningTerritorySpawnPoint = UsedSP;

		UGameplayStatics::FinishSpawningActor(Guard, SpawnTransform);

		SpawnedGuards.Add(Guard);
		RegisterDefender(Guard);

		if (UsedSP)
		{
			UsedSP->RegisterSpawnedGuard(Guard);
		}

		if (bDebug)
		{
			UE_LOG(LogTerritory, Log, TEXT("  Guard %d/%d spawned for %s (faction=%s, GUID=%s, SP=%s)"),
				i + 1, GuardSpawnCount,
				*GetTerritoryTag().ToString(),
				*EffectiveFaction.ToString(),
				*GuardSaveGUID.ToString(),
				UsedSP ? *UsedSP->GetName() : TEXT("random"));
		}
	}
}

void ATerritoryVolume::SpawnSingleGuard(ATerritoryGuardSpawnPoint* SpawnPoint)
{
	if (!HasAuthority() || !GuardNPCDefinition || !SpawnPoint) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FGameplayTag OwnerFaction = OwnershipData.OwningFaction;
	if (!OwnerFaction.IsValid()) return;

	UNPCDefinition* EffectiveDef = ResolveGuardDefinition(OwnerFaction);
	if (!EffectiveDef) return;

	UClass* NPCClass = EffectiveDef->NPCClassPath.LoadSynchronous();
	if (!NPCClass || !NPCClass->IsChildOf(ATerritoryGuardCharacter::StaticClass()))
	{
		NPCClass = ATerritoryGuardCharacter::StaticClass();
	}

	FTransform SpawnTransform = SpawnPoint->GetSpawnTransform();

	ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(
		UGameplayStatics::BeginDeferredActorSpawnFromClass(
			this, NPCClass, SpawnTransform,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
			this));

	if (!Guard) return;

	FGuid GuardSaveGUID = FGuid::NewGuid();

	// Determine effective faction: spawn point override > territory owner
	FGameplayTag EffectiveFaction = OwnerFaction;
	if (SpawnPoint->FactionOverride.IsValid())
	{
		EffectiveFaction = SpawnPoint->FactionOverride;
	}

	// Single deterministic entrypoint — fills ALL SpawnInfo fields
	Guard->ConfigureTerritorySpawn(
		EffectiveDef,
		EffectiveFaction,
		TerritoryGUID,
		GuardSaveGUID,
		SpawnTransform,
		SpawnPoint->GetFName());

	// Set territory AI context before FinishSpawningActor
	Guard->OwningTerritory = this;
	Guard->OwningTerritorySpawnPoint = SpawnPoint;

	UGameplayStatics::FinishSpawningActor(Guard, SpawnTransform);

	SpawnedGuards.Add(Guard);
	RegisterDefender(Guard);
	SpawnPoint->RegisterSpawnedGuard(Guard);

	UE_LOG(LogTerritory, Log, TEXT("[GuardReserve] 1 replacement spawned at %s for %s (faction=%s)"),
		*SpawnPoint->GetName(),
		*GetTerritoryTag().ToString(),
		*OwnerFaction.ToString());
}

void ATerritoryVolume::DespawnGuards()
{
	for (TWeakObjectPtr<ATerritoryGuardCharacter>& GuardPtr : SpawnedGuards)
	{
		if (GuardPtr.IsValid())
		{
			UnbindDefenderDeath(GuardPtr.Get());
			RegisteredDefenders.Remove(GuardPtr);
			GuardPtr->Destroy();
		}
	}
	SpawnedGuards.Empty();
	CleanupInvalidDefenders();
	OwnershipData.DefenderCount = RegisteredDefenders.Num();

	UE_LOG(LogTerritory, Log, TEXT("Despawned all guards for %s"),
		*GetTerritoryTag().ToString());
}

int32 ATerritoryVolume::GetSpawnedGuardCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr : SpawnedGuards)
	{
		if (Ptr.IsValid()) ++Count;
	}
	return Count;
}

bool ATerritoryVolume::HasGuardsAlive() const
{
	return GetSpawnedGuardCount() > 0;
}

FVector ATerritoryVolume::GetRandomSpawnPoint() const
{
	FVector Center = GetActorLocation();
	FQuat Rotation = GetActorQuat();

	FVector LocalOffset(0.f);
	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		FVector Extent = Box->GetScaledBoxExtent();
		LocalOffset = FVector(
			FMath::FRandRange(-Extent.X, Extent.X),
			FMath::FRandRange(-Extent.Y, Extent.Y),
			0.f);
	}
	else
	{
		LocalOffset = FVector(
			FMath::FRandRange(-GuardSpawnRadius, GuardSpawnRadius),
			FMath::FRandRange(-GuardSpawnRadius, GuardSpawnRadius),
			0.f);
	}

	// Transform local offset by actor rotation so rotated volumes spawn correctly
	FVector SpawnLoc = Center + Rotation.RotateVector(LocalOffset);

	// Project to NavMesh so guards spawn on walkable ground, not floating at volume Z
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLoc;
		if (NavSys->ProjectPointToNavigation(SpawnLoc, ProjectedLoc, FVector(500.f, 500.f, 500.f)))
		{
			SpawnLoc = ProjectedLoc.Location;
		}
	}

	return SpawnLoc;
}

TArray<ATerritoryGuardSpawnPoint*> ATerritoryVolume::GetGuardSpawnPoints() const
{
	TArray<ATerritoryGuardSpawnPoint*> Result;
	for (const TObjectPtr<AActor>& Ptr : GuardSpawnPoints)
	{
		if (ATerritoryGuardSpawnPoint* SP = Cast<ATerritoryGuardSpawnPoint>(Ptr))
		{
			Result.Add(SP);
		}
	}
	return Result;
}

UTerritoryNavigationMarkerComponent* ATerritoryVolume::GetMapMarkerComponent() const
{
	return MapMarkerComponent;
}

FString ATerritoryVolume::GetDebugString() const
{
	return FString::Printf(TEXT("%s | Owner=%s | State=%d | Progress=%.2f | Guards=%d/%d | Defenders=%d"),
		*TerritoryTag.ToString(),
		*OwnershipData.OwningFaction.ToString(),
		static_cast<int32>(OwnershipData.State),
		OwnershipData.ControlProgress,
		GetSpawnedGuardCount(),
		GuardSpawnCount,
		GetDefenderCount());
}
