#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "SaveSystemStatics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Crc.h"
#include "Engine/World.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Character/CharacterDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"

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

	// Authored references are authoritative, including spawn points intentionally
	// placed outside this volume's bounds.
	for (ATerritoryGuardSpawnPoint* SpawnPoint : GetGuardSpawnPoints())
	{
		SpawnPoint->BindToTerritory(this);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// P0-01: Register on ALL net modes so clients can resolve territory lookups.
	// Save/load, guards, and authority mutations remain server-only below.
	// ═══════════════════════════════════════════════════════════════════════════
	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		ETerritoryRegistrationResult RegResult = Registry->RegisterTerritory(this);
		if (RegResult != ETerritoryRegistrationResult::Success)
		{
			UE_LOG(LogTerritory, Error, TEXT("%s registration rejected (%d) — aborting gameplay activation"),
				*GetPathName(), static_cast<int32>(RegResult));
			SetActorTickEnabled(false);
			return;
		}
		LastKnownBounds = GetTerritoryBounds();
	}

	if (HasAuthority())
	{
		// GUID must be baked at editor placement time.
		// If missing here, it means the level wasn't saved after GUID baking.
		if (!TerritoryGUID.IsValid())
		{
			UE_LOG(LogTerritory, Error,
				TEXT("%s has no valid TerritoryGUID — save/load will fail. "
					"Open the level in editor, select the actor, move it slightly, "
					"and save the level to bake a GUID."),
				*GetPathName());
		}

		bool bSuccessfullyLoaded = USaveSystemStatics::LoadSingleActor(this);

		if (!bSuccessfullyLoaded && !bLoadedFromSave)
		{
			// Fresh territory — initialize from level defaults
			OwnershipData.MaxConcurrentAttackers = InitialMaxConcurrentAttackers;
			OwnershipData.PeriodicIncome = InitialPeriodicIncome;
			OwnershipData.GuardCost = InitialGuardCost;
			OwnershipData.GuardRecruitmentCost = InitialGuardRecruitmentCost;
			OwnershipData.DesiredGuardCount = FMath::Clamp(GuardSpawnCount, 0, GetMaxGuardCount());

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
			OwnershipData.GuardRecruitmentCost = InitialGuardRecruitmentCost;
			if (OwnershipData.DesiredGuardCount < 0)
			{
				OwnershipData.DesiredGuardCount = FMath::Clamp(GuardSpawnCount, 0, GetMaxGuardCount());
			}
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

		if (!bGuardsReconciled)
		{
			ReconcileGuardsAfterLoad();
			bGuardsReconciled = true;
		}
		RefreshGarrisonSnapshot();
	}

	// Fire BP-exposed initialization event (only reached if registration succeeded)
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
				const TArray<FTerritoryPatrolNode>& Route = SP->GetEffectivePatrolRoute();
				for (int32 i = 0; i < Route.Num(); ++i)
				{
					const FVector& Node = Route[i].Location;
					DrawDebugSphere(World, Node, 15.f, 6, FColor::Cyan, false, 0.f, 0, 0.5f);
					if (i + 1 < Route.Num())
					{
						DrawDebugLine(World, Node, Route[i + 1].Location, FColor::Cyan, false, 0.f, 0, 1.f);
					}
					else if (SP->GetEffectiveLoopPatrol() && Route.Num() > 1)
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
	DOREPLIFETIME(ATerritoryVolume, GarrisonSnapshot);
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
	EnsureCounterAttackApproachIDs();
}

void ATerritoryVolume::EnsureCounterAttackApproachIDs()
{
	TSet<FName> UsedIDs;
	for (const FTerritoryAssaultApproach& Approach : CounterAttackApproaches)
	{
		if (!Approach.ApproachID.IsNone()) UsedIDs.Add(Approach.ApproachID);
	}

	bool bChanged = false;
	for (int32 Index = 0; Index < CounterAttackApproaches.Num(); ++Index)
	{
		FTerritoryAssaultApproach& Approach = CounterAttackApproaches[Index];
		if (!Approach.ApproachID.IsNone()) continue;

		const UEnum* ApproachTypeEnum = StaticEnum<ETerritoryAttackApproachType>();
		const FString TypeName = ApproachTypeEnum
			? ApproachTypeEnum->GetNameStringByValue(static_cast<int64>(Approach.Type))
			: TEXT("Approach");
		int32 Suffix = Index + 1;
		FName Candidate;
		do
		{
			Candidate = FName(*FString::Printf(TEXT("%s_%02d"), *TypeName, Suffix++));
		}
		while (UsedIDs.Contains(Candidate));

		Approach.ApproachID = Candidate;
		UsedIDs.Add(Candidate);
		bChanged = true;
	}
	if (bChanged)
	{
		MarkPackageDirty();
		UE_LOG(LogTerritory, Log, TEXT("Generated stable counterattack Approach IDs for %s"), *GetPathName());
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
		bGuardsReconciled = true;
		if (UTerritoryControlSubsystem* Control = GetWorld()->GetSubsystem<UTerritoryControlSubsystem>())
		{
			Control->RestoreCaptureState(this, OwnershipData.ContestingFaction, OwnershipData.ControlProgress);
		}
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
	const TArray<ATerritoryGuardSpawnPoint*> SpawnPoints = GetGuardSpawnPoints();
	int32 SavedActiveGuards = 0;
	for (ATerritoryGuardSpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (!SpawnPoint) continue;
		// Narrative loads actors independently. Explicitly hydrate referenced posts
		// before reading their saved active counts so BeginPlay order cannot reroll
		// the garrison strength.
		if (bLoadedFromSave)
		{
			USaveSystemStatics::LoadSingleActor(SpawnPoint);
		}
		SavedActiveGuards += SpawnPoint->GetSavedActiveGuardCount();
	}
	SavedActiveGuards = CalculateGuardRestoreCount(bLoadedFromSave, GetDesiredGuardCount(),
		SavedActiveGuards, OwnershipData.DefenderCount);

	// Despawn ALL existing guards (may be stale from BeginPlay initial faction)
	DespawnGuards();

	// Clean stale defender registrations
	RegisteredDefenders.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });
	OwnershipData.DefenderCount = RegisteredDefenders.Num();

	// Spawn guards for the loaded/current owner
	if (OwnershipData.State == ETerritoryState::Claimed
		&& OwnershipData.OwningFaction.IsValid()
		&& ResolveGuardDefinition(OwnershipData.OwningFaction)
		&& SavedActiveGuards > 0)
	{
		SpawnGuardsToCount(SavedActiveGuards);
	}

	// Sync RepNotify cache
	PreviousOwningFaction = OwnershipData.OwningFaction;
	PreviousState = OwnershipData.State;
	RefreshGarrisonSnapshot();
}

void ATerritoryVolume::OnRep_GarrisonSnapshot()
{
	OnGarrisonChanged.Broadcast(this, GarrisonSnapshot);
}

int32 ATerritoryVolume::CalculateGuardRestoreCount(bool bWasLoadedFromSave,
	int32 DesiredGuards, int32 SavedActiveGuards, int32 LegacyDefenderCount)
{
	const int32 BoundedDesired = FMath::Max(0, DesiredGuards);
	if (!bWasLoadedFromSave)
	{
		return BoundedDesired;
	}
	if (SavedActiveGuards <= 0 && LegacyDefenderCount > 0)
	{
		SavedActiveGuards = LegacyDefenderCount;
	}
	return FMath::Clamp(SavedActiveGuards, 0, BoundedDesired);
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
	// P1-N01: Server-authoritative — don't write replicated data on clients
	if (!HasAuthority()) return;
	if (InTerritoryTag == TerritoryTag && OwnershipData.ContestingFaction != ContestingFaction)
	{
		OwnershipData.ContestingFaction = ContestingFaction;
	}
}

void ATerritoryVolume::OnTerritoryUncontested_Implementation(FGameplayTag InTerritoryTag)
{
	// P1-N02: Server-authoritative — don't write replicated data on clients
	if (!HasAuthority()) return;
	if (InTerritoryTag != TerritoryTag) return;
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

int32 ATerritoryVolume::GetMaxGuardCount() const
{
	if (!HasAuthority())
	{
		return FMath::Max(0, GarrisonSnapshot.MaximumGuards);
	}

	const TArray<ATerritoryGuardSpawnPoint*> AuthoredSpawnPoints = GetGuardSpawnPoints();
	return AuthoredSpawnPoints.Num();
}

int32 ATerritoryVolume::GetPostCaptureGuardCount(
	const FTerritoryTransitionContext& TransitionContext) const

{
	return GetPostCaptureGuardCountForOwner(
		TransitionContext, TransitionContext.RequestingFaction);
}

int32 ATerritoryVolume::GetPostCaptureGuardCountForOwner(
	const FTerritoryTransitionContext& TransitionContext, const FGameplayTag& NewOwner) const
{
	const int32 ConfiguredCount = FMath::Clamp(GuardSpawnCount, 0, GetMaxGuardCount());
	switch (PostCaptureGarrisonPolicy)
	{
	case ETerritoryPostCaptureGarrisonPolicy::AlwaysUnstaffed:
		return 0;
	case ETerritoryPostCaptureGarrisonPolicy::PlayerChooses:
	{
		const APlayerController* PlayerController = TransitionContext.PlayerController;
		auto HasExactNarrativeFaction = [&NewOwner](const AActor* Actor)
		{
			const INarrativeTeamAgentInterface* TeamAgent =
				Cast<INarrativeTeamAgentInterface>(Actor);
			return TeamAgent && NewOwner.IsValid()
				&& TeamAgent->GetFactions().HasTagExact(NewOwner);
		};
		const bool bPlayerOwnsNewFaction = PlayerController
			&& (HasExactNarrativeFaction(PlayerController)
				|| HasExactNarrativeFaction(TransitionContext.TargetPawn)
				|| HasExactNarrativeFaction(PlayerController->GetPawn())
				|| HasExactNarrativeFaction(PlayerController->GetPlayerState<APlayerState>()));
		const bool bRequestMatchesOwner = !TransitionContext.RequestingFaction.IsValid()
			|| TransitionContext.RequestingFaction == NewOwner;

		// Hierarchy reducers and legacy Blueprint capture nodes can legitimately lose the
		// original pawn/controller while preserving the authoritative NewOwner. Recover the
		// player-owned-faction fact from every live controller, never from a first-player guess.
		// Exact Narrative membership prevents a parent tag from suppressing an unrelated AI
		// garrison. This makes PlayerChooses durable across Property -> District -> City cascades.
		bool bLivePlayerOwnsNewFaction = false;
		if (NewOwner.IsValid())
		{
			if (const UWorld* World = GetWorld())
			{
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					const APlayerController* LiveController = It->Get();
					if (LiveController
						&& (HasExactNarrativeFaction(LiveController)
							|| HasExactNarrativeFaction(LiveController->GetPawn())
							|| HasExactNarrativeFaction(LiveController->GetPlayerState<APlayerState>())))
					{
						bLivePlayerOwnsNewFaction = true;
						break;
					}
				}
			}
		}
		return ((bPlayerOwnsNewFaction && bRequestMatchesOwner) || bLivePlayerOwnsNewFaction)
			? 0 : ConfiguredCount;
	}
	case ETerritoryPostCaptureGarrisonPolicy::ConfiguredForEveryOwner:
	default:
		return ConfiguredCount;
	}
}

int32 ATerritoryVolume::GetGuardRecruitmentCost(int32 Count) const
{
	const int64 SafeCost = static_cast<int64>(FMath::Max(0, Count))
		* FMath::Max(0, OwnershipData.GuardRecruitmentCost);
	return SafeCost > MAX_int32 ? MAX_int32 : static_cast<int32>(SafeCost);
}

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

ETerritoryControlMode ATerritoryVolume::GetControlMode() const
{
	return ControlMode;
}

void ATerritoryVolume::SetOwningFaction(const FGameplayTag& NewFaction)
{
	SetOwningFactionWithContext(NewFaction, FTerritoryTransitionContext());
}

void ATerritoryVolume::SetOwningFactionWithContext(const FGameplayTag& NewFaction,
	const FTerritoryTransitionContext& TransitionContext)
{
	// P0-04: Thin wrapper — delegate to CommitOwnershipData for single authoritative path
	if (!HasAuthority() || bTransitionInProgress
		|| (ControlMode == ETerritoryControlMode::AggregateOnly && !bApplyingDerivedOwnership)) return;

	FGameplayTag OldOwner = OwnershipData.OwningFaction;
	if (OldOwner == NewFaction) return;

	const ETerritoryState NewState = NewFaction.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;

	// Build candidate and commit atomically
	FTerritoryOwnershipData Candidate = OwnershipData;
	Candidate.OwningFaction = NewFaction;
	Candidate.State = NewState;
	Candidate.ContestingFaction = FGameplayTag();
	Candidate.ControlProgress = NewFaction.IsValid() ? 1.f : 0.f;
	Candidate.DesiredGuardCount = NewFaction.IsValid()
		? GetPostCaptureGuardCountForOwner(TransitionContext, NewFaction)
		: 0;
	if (NewState != ETerritoryState::Locked)
	{
		Candidate.LockReason = FText();
	}

	CommitOwnershipData(Candidate, TransitionContext);
}

void ATerritoryVolume::SetDerivedOwningFaction(const FGameplayTag& NewFaction)
{
	SetDerivedOwningFaction(NewFaction, FTerritoryTransitionContext());
}

void ATerritoryVolume::SetDerivedOwningFaction(const FGameplayTag& NewFaction,
	const FTerritoryTransitionContext& TransitionContext)
{
	if (!HasAuthority()) return;
	const bool bWasApplyingDerivedOwnership = bApplyingDerivedOwnership;
	bApplyingDerivedOwnership = true;
	SetOwningFactionWithContext(NewFaction, TransitionContext);
	bApplyingDerivedOwnership = bWasApplyingDerivedOwnership;
}

void ATerritoryVolume::ForceSetOwningFaction(const FGameplayTag& NewFaction)
{
	ForceSetOwningFactionWithContext(NewFaction, FTerritoryTransitionContext());
}

void ATerritoryVolume::ForceSetOwningFactionWithContext(const FGameplayTag& NewFaction,
	const FTerritoryTransitionContext& TransitionContext)
{
	if (!HasAuthority()) return;
	const bool bWasBypassing = bBypassTransitionConditions;
	const bool bWasApplyingDerivedOwnership = bApplyingDerivedOwnership;
	bBypassTransitionConditions = true;
	bApplyingDerivedOwnership = true;
	SetOwningFactionWithContext(NewFaction, TransitionContext);
	bBypassTransitionConditions = bWasBypassing;
	bApplyingDerivedOwnership = bWasApplyingDerivedOwnership;
}

bool ATerritoryVolume::CommitOwnershipData(const FTerritoryOwnershipData& NewData, const FTerritoryTransitionContext& TransitionContext)
{
	if (!HasAuthority()) return false;

	const FGameplayTag OldOwner = OwnershipData.OwningFaction;
	const ETerritoryState OldState = OwnershipData.State;
	const FGameplayTag NewOwner = NewData.OwningFaction;
	const ETerritoryState NewState = NewData.State;

	// P2-N01: Compare all ownership data fields, not just owner/state/contest/progress
	if (OldOwner == NewOwner && OldState == NewState
		&& OwnershipData.ContestingFaction == NewData.ContestingFaction
		&& FMath::IsNearlyEqual(OwnershipData.ControlProgress, NewData.ControlProgress)
		&& OwnershipData.DesiredGuardCount == NewData.DesiredGuardCount
		&& OwnershipData.PeriodicIncome == NewData.PeriodicIncome
		&& OwnershipData.GuardCost == NewData.GuardCost
		&& OwnershipData.GuardRecruitmentCost == NewData.GuardRecruitmentCost
		&& OwnershipData.MaxConcurrentAttackers == NewData.MaxConcurrentAttackers
		&& OwnershipData.DefenderCount == NewData.DefenderCount
		&& OwnershipData.LockReason.EqualTo(NewData.LockReason))
	{
		return false;
	}

	bTransitionInProgress = true;
	ActiveTransitionContext = TransitionContext;

	// Cache previous values for RepNotify diff
	PreviousOwningFaction = OldOwner;
	PreviousState = OldState;

	// ─── Atomic struct write ───
	OwnershipData = NewData;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[CommitOwnership] %s: owner %s→%s, state %d→%d, progress %.2f"),
			*GetTerritoryTag().ToString(),
			*OldOwner.ToString(), *NewOwner.ToString(),
			static_cast<int32>(OldState), static_cast<int32>(NewState),
			NewData.ControlProgress);
	}

	// Apply the post-owned reserve policy before spawning the new owner's guards.
	// Doing this from OnOwnershipChanged would erase guards registered during SpawnGuards.
	if (OldOwner != NewOwner)
	{
		const EOwnershipTransitionReason Reason = NewOwner.IsValid()
			? EOwnershipTransitionReason::OwnerChanged
			: EOwnershipTransitionReason::RevertedToUnclaimed;
		for (ATerritoryGuardSpawnPoint* SpawnPoint : GetGuardSpawnPoints())
		{
			if (SpawnPoint) SpawnPoint->HandleOwnershipTransition(Reason);
		}
	}

	// ─── Guard lifecycle (BEFORE BP virtuals so BP sees final guard state) ───
	if (OldOwner != NewOwner)
	{
		DespawnGuards();
		if (NewOwner.IsValid() && ResolveGuardDefinition(NewOwner) && NewData.DesiredGuardCount > 0)
		{
			SpawnGuards();
		}
	}
	else if (OldState != NewState)
	{
		if (NewState == ETerritoryState::Locked)
		{
			DespawnGuards();
		}
		else if (NewState == ETerritoryState::Contested && OldState == ETerritoryState::Claimed)
		{
			DespawnGuards();
		}
		else if (NewState == ETerritoryState::Claimed && (OldState == ETerritoryState::Contested || OldState == ETerritoryState::Locked))
		{
			if (ResolveGuardDefinition(OwnershipData.OwningFaction) && NewData.DesiredGuardCount > 0 && GetSpawnedGuardCount() == 0)
			{
				SpawnGuards();
			}
		}
	}

	// ─── ONE ordered event bundle ───
	if (OldState != NewState)
	{
		FireStateEvents(OldState, false, TransitionContext);
		FireStateEvents(NewState, true, TransitionContext);
	}

	if (OldOwner != NewOwner)
	{
		OnOwnershipChanged(OldOwner, NewOwner);
		OnTerritoryOwnershipChanged.Broadcast(this, OldOwner, NewOwner);
	}

	if (OldState != NewState)
	{
		OnStateChanged(OldState, NewState);
		OnTerritoryStateChangedDelegate.Broadcast(this, NewState);
	}

	// P1-N03: Clear any stale capture state in ControlSubsystem when ownership changes
	// P0-01: Clean up runtime capture tracking only — do NOT mutate terminal
	// ownership fields (progress, contesting faction, state). Those were just
	// committed atomically and must not be zeroed by ResetCapture.
	if (OldOwner != NewOwner)
	{
		if (UWorld* W = GetWorld())
		{
			if (UTerritoryControlSubsystem* Control = W->GetSubsystem<UTerritoryControlSubsystem>())
			{
				Control->ClearCaptureTrackingOnly(this);
			}
		}
	}

	RefreshGarrisonSnapshot();
	ForceNetUpdate();
	ActiveTransitionContext = FTerritoryTransitionContext();
	bTransitionInProgress = false;
	return true;
}

void ATerritoryVolume::SetControlProgress(float Progress)
{
	if (!HasAuthority()) return;
	const float ClampedProgress = FMath::Clamp(Progress, 0.f, 1.f);
	// P0-03: No-op detection — skip write if value unchanged (avoids unnecessary replication)
	if (FMath::IsNearlyEqual(OwnershipData.ControlProgress, ClampedProgress)) return;
	OwnershipData.ControlProgress = ClampedProgress;
}

void ATerritoryVolume::SetTerritoryState(ETerritoryState NewState)
{
	if (!HasAuthority() || bTransitionInProgress) return;
	ETerritoryState OldState = OwnershipData.State;
	if (OldState == NewState) return;

	FText ConditionFailure;
	if (!bBypassTransitionConditions && !CheckStateConditions(NewState, ConditionFailure))
	{
		const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
		if (Settings && Settings->ShouldDebugStateTransitions())
		{
			UE_LOG(LogTerritory, Log, TEXT("[StateChange] %s: blocked -> %d — %s"),
				*GetTerritoryTag().ToString(), static_cast<int32>(NewState), *ConditionFailure.ToString());
		}
		return;
	}

	bTransitionInProgress = true;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugStateTransitions())
	{
		UE_LOG(LogTerritory, Log, TEXT("[StateChange] %s: %d → %d"),
			*GetTerritoryTag().ToString(),
			static_cast<int32>(OldState), static_cast<int32>(NewState));
	}

	OwnershipData.State = NewState;

	// Commit the state before running any exit or entry side effects. Rejected
	// transitions therefore cannot fire events or mutate gameplay state.
	FireStateEvents(OldState, false);
	FireStateEvents(NewState, true);

	// Guard lifecycle invariants — run BEFORE BP virtual.
	// Keep the incumbent owner while contested so an abandoned capture can restore
	// Claimed state and capture delegates retain the real defending faction.
	if (NewState == ETerritoryState::Locked)
	{
		DespawnGuards();
	}
	else if (NewState == ETerritoryState::Contested && OldState == ETerritoryState::Claimed)
	{
		DespawnGuards();
	}
	else if (NewState == ETerritoryState::Claimed && (OldState == ETerritoryState::Contested || OldState == ETerritoryState::Locked))
	{
		if (HasAuthority() && ResolveGuardDefinition(OwnershipData.OwningFaction) && GetDesiredGuardCount() > 0 && GetSpawnedGuardCount() == 0)
		{
			SpawnGuards();
		}
	}

	OnStateChanged(OldState, NewState);
	OnTerritoryStateChangedDelegate.Broadcast(this, NewState);
	bTransitionInProgress = false;
}

void ATerritoryVolume::ForceSetTerritoryState(ETerritoryState NewState)
{
	if (!HasAuthority()) return;
	const bool bWasBypassing = bBypassTransitionConditions;
	bBypassTransitionConditions = true;
	SetTerritoryState(NewState);
	bBypassTransitionConditions = bWasBypassing;
}

bool ATerritoryVolume::CheckStateConditions(ETerritoryState State, FText& OutFailureReason, const FTerritoryTransitionContext& TransitionContext) const
{
	const FTerritoryStateConfig* Config = StateConfigs.Find(State);
	if (!Config || Config->EntryConditions.IsEmpty())
	{
		OutFailureReason = FText::GetEmpty();
		return true;
	}

	// Use explicit transition context
	APlayerController* ContextPC = TransitionContext.PlayerController;
	APawn* ContextPawn = TransitionContext.TargetPawn;
	// P2-N02: No fallback to GetFirstPlayerController. If TransitionContext is empty,
	// conditions requiring a pawn/controller will evaluate against null and fail.
	// Callers must provide explicit TransitionContext for player-dependent conditions.

	for (const TObjectPtr<UNarrativeCondition>& Cond : Config->EntryConditions)
	{
		if (!Cond) continue;
		// P0-02: Pass TalesComponent from transition context for full Narrative condition evaluation
		if (!Cond->CheckCondition(ContextPawn, ContextPC, TransitionContext.TalesComponent))
		{
			OutFailureReason = FText::FromString(FString::Printf(TEXT("Condition '%s' not met."),
				*Cond->GetGraphDisplayText()));
			return false;
		}
	}

	OutFailureReason = FText::GetEmpty();
	return true;
}

void ATerritoryVolume::FireStateEvents(ETerritoryState State, bool bEntering, const FTerritoryTransitionContext& TransitionContext)
{
	const FTerritoryStateConfig* Config = StateConfigs.Find(State);
	if (!Config) return;

	const TArray<TObjectPtr<UNarrativeEvent>>* Events = bEntering ? &Config->EntryEvents : &Config->ExitEvents;
	if (!Events || Events->IsEmpty()) return;

	// Use explicit transition context
	APlayerController* ContextPC = TransitionContext.PlayerController;
	APawn* ContextPawn = TransitionContext.TargetPawn;
	// P2-N02: No fallback to GetFirstPlayerController. If TransitionContext is empty,
	// conditions requiring a pawn/controller will evaluate against null and fail.
	// Callers must provide explicit TransitionContext for player-dependent conditions.

	// P0-03: Use Narrative event lifecycle — OnActivate for entry, OnDeactivate for exit
	for (const TObjectPtr<UNarrativeEvent>& Event : *Events)
	{
		if (!Event) continue;
		if (bEntering)
		{
			Event->OnActivate(ContextPawn, ContextPC, TransitionContext.TalesComponent);
		}
		else
		{
			Event->OnDeactivate(ContextPawn, ContextPC, TransitionContext.TalesComponent);
		}
	}
}

// ─── Lock System ───

bool ATerritoryVolume::IsLocked() const
{
	return OwnershipData.State == ETerritoryState::Locked;
}

bool ATerritoryVolume::CanUnlock() const
{
	return CanUnlockWithContext(FTerritoryTransitionContext());
}

bool ATerritoryVolume::CanUnlockWithContext(const FTerritoryTransitionContext& TransitionContext) const
{
	if (!IsLocked()) return true;

	// No lock conditions → always unlockable
	if (LockConditions.Num() == 0) return true;

	for (const TObjectPtr<UNarrativeCondition>& Cond : LockConditions)
	{
		if (!Cond) continue;
		if (!Cond->CheckCondition(
			TransitionContext.TargetPawn,
			TransitionContext.PlayerController,
			TransitionContext.TalesComponent)) return false;
	}
	return true;
}

void ATerritoryVolume::LockTerritory(const FText& Reason)
{
	LockTerritoryWithContext(Reason, FTerritoryTransitionContext());
}

bool ATerritoryVolume::LockTerritoryWithContext(
	const FText& Reason,
	const FTerritoryTransitionContext& TransitionContext)
{
	if (!HasAuthority()) return false;

	FText ConditionFailure;
	if (!CheckStateConditions(ETerritoryState::Locked, ConditionFailure, TransitionContext))
	{
		UE_LOG(LogTerritory, Warning, TEXT("[Lock] %s lock rejected: %s"),
			*GetTerritoryTag().ToString(), *ConditionFailure.ToString());
		return false;
	}

	FTerritoryOwnershipData Candidate = OwnershipData;
	Candidate.State = ETerritoryState::Locked;
	Candidate.ContestingFaction = FGameplayTag();
	Candidate.ControlProgress = 0.f;
	Candidate.LockReason = Reason;
	if (!CommitOwnershipData(Candidate, TransitionContext)) return false;

	UE_LOG(LogTerritory, Log, TEXT("[Lock] %s locked: %s"),
		*GetTerritoryTag().ToString(), *Reason.ToString());
	return true;
}

bool ATerritoryVolume::TryUnlock(bool bForce)
{
	return TryUnlockWithContext(FTerritoryTransitionContext(), bForce);
}

bool ATerritoryVolume::TryUnlockWithContext(
	const FTerritoryTransitionContext& TransitionContext,
	bool bForce)
{
	if (!HasAuthority()) return false;
	if (!IsLocked()) return true;

	if (!bForce && !CanUnlockWithContext(TransitionContext))
	{
		UE_LOG(LogTerritory, Log, TEXT("[Lock] %s unlock blocked — conditions not met"),
			*GetTerritoryTag().ToString());
		return false;
	}

	const ETerritoryState TargetState = OwnershipData.OwningFaction.IsValid()
		? ETerritoryState::Claimed
		: ETerritoryState::Unclaimed;
	FText ConditionFailure;
	if (!bForce && !CheckStateConditions(TargetState, ConditionFailure, TransitionContext))
	{
		UE_LOG(LogTerritory, Log, TEXT("[Lock] %s unlock blocked — state conditions not met: %s"),
			*GetTerritoryTag().ToString(), *ConditionFailure.ToString());
		return false;
	}

	FTerritoryOwnershipData Candidate = OwnershipData;
	Candidate.State = TargetState;
	Candidate.ContestingFaction = FGameplayTag();
	Candidate.ControlProgress = OwnershipData.OwningFaction.IsValid() ? 1.f : 0.f;
	Candidate.LockReason = FText();
	if (!CommitOwnershipData(Candidate, TransitionContext)) return false;

	UE_LOG(LogTerritory, Log, TEXT("[Lock] %s unlocked"),
		*GetTerritoryTag().ToString());
	return true;
}

void ATerritoryVolume::RegisterDefender(AActor* Defender)
{
	if (!Defender || !HasAuthority()) return;

	if (!Cast<IAbilitySystemInterface>(Defender))
	{
		UE_LOG(LogTerritory, Warning, TEXT("RegisterDefender: %s has no AbilitySystemComponent — death will not be detected, defender may become immortal in %s"),
			*GetNameSafe(Defender), *GetTerritoryTag().ToString());
	}

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
		// Defence reaching zero makes the territory vulnerable; ownership remains
		// authoritative until physical attackers complete the existing capture flow.
		OwnershipData.DefenderCount = 0;
		ForceNetUpdate();
	}
}

void ATerritoryVolume::OnTerritoryInitialized_Implementation()
{
}

void ATerritoryVolume::OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC)
{
	// Early return if nothing useful — actor already GC'd or delegate fired with null.
	if (!KilledActor && !KilledASC) return;

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

	// Remove the dead guard before any replacement attempt so capacity checks are accurate.
	SpawnedGuards.RemoveAll([KilledActor](const TWeakObjectPtr<ATerritoryGuardCharacter>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == KilledActor;
	});

	// Queue reserve deployment before broadcasting so a manual Blueprint listener can
	// satisfy the same request without racing a synchronous automatic spawn.
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
	RefreshGarrisonSnapshot();

	// Determine the killer from the guard's tracked last damaging instigator.
	// This is populated by ATerritoryGuardCharacter::TakeDamage.
	AActor* Killer = nullptr;
	if (ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(KilledActor))
	{
		Killer = Guard->LastDamagingInstigator.Get();
	}
	OnGuardKilled.Broadcast(this, KilledActor, Killer, GetDefenderCount());

	// Check ALL registered defenders (includes non-guard defenders registered via
	// RegisterDefender Blueprint API), not just SpawnedGuards — otherwise territory
	// flips to Unclaimed while player/pawn defenders are still alive.
	CleanupInvalidDefenders();
	if (RegisteredDefenders.Num() == 0 && !HasPendingReserveDeployments())
	{
		UE_LOG(LogTerritory, Log, TEXT("[GuardDeath] All defenders defeated in %s"),
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

bool ATerritoryVolume::HasPendingReserveDeployments() const
{
	for (ATerritoryGuardSpawnPoint* SpawnPoint : GetGuardSpawnPoints())
	{
		if (SpawnPoint && SpawnPoint->HasPendingReserveSpawn())
		{
			return true;
		}
	}
	return false;
}

void ATerritoryVolume::CancelPendingReserveDeployments()
{
	for (ATerritoryGuardSpawnPoint* SpawnPoint : GetGuardSpawnPoints())
	{
		if (SpawnPoint)
		{
			SpawnPoint->CancelPendingReserveSpawns();
		}
	}
	RefreshGarrisonSnapshot();
}

void ATerritoryVolume::RefreshGarrisonSnapshot()
{
	if (!HasAuthority() || bGarrisonMutationInProgress)
	{
		return;
	}

	FTerritoryGarrisonSnapshot NewSnapshot;
	NewSnapshot.ActiveGuards = GetSpawnedGuardCount();
	NewSnapshot.DesiredGuards = GetDesiredGuardCount();
	NewSnapshot.MaximumGuards = GetMaxGuardCount();
	for (const ATerritoryGuardSpawnPoint* SpawnPoint : GetGuardSpawnPoints())
	{
		if (!SpawnPoint) continue;
		NewSnapshot.ReserveGuards += SpawnPoint->GetReserveCount();
		NewSnapshot.PendingDeployments += SpawnPoint->GetPendingReserveCount();
	}

	if (NewSnapshot != GarrisonSnapshot)
	{
		GarrisonSnapshot = NewSnapshot;
		OnGarrisonChanged.Broadcast(this, GarrisonSnapshot);
		ForceNetUpdate();
	}
}

void ATerritoryVolume::RemoveGuardWithoutReplacement(ATerritoryGuardCharacter* Guard)
{
	if (!Guard) return;
	if (ATerritoryGuardSpawnPoint* SpawnPoint = Guard->OwningTerritorySpawnPoint)
	{
		SpawnPoint->UnregisterGuard(Guard, EGuardRemovalReason::ManualRemoval);
	}
	UnbindDefenderDeath(Guard);
	RegisteredDefenders.Remove(Guard);
	SpawnedGuards.RemoveAllSwap([Guard](const TWeakObjectPtr<ATerritoryGuardCharacter>& GuardPtr)
	{
		return GuardPtr.Get() == Guard;
	});
	Guard->Destroy();
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

UNPCDefinition* ATerritoryVolume::GetResolvedGuardDefinition(const FGameplayTag& Faction) const
{
	return ResolveGuardDefinition(Faction);
}

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

// ═══════════════════════════════════════════════════════════════════════════════
// P0-07: Unified narrative override resolver
// Cascade: SP inline override → GuardPostDefinition → territory default
// ═══════════════════════════════════════════════════════════════════════════════
void ATerritoryVolume::ResolveSpawnPointNarrativeOverrides(
	ATerritoryGuardSpawnPoint* SpawnPoint,
	UNPCDefinition*& OutDef,
	UClass*& OutNPCClass,
	UNPCActivityConfiguration*& OutActivityConfig,
	const TArray<TSoftObjectPtr<UTriggerSet>>*& OutTriggerSets,
	const TArray<TSoftObjectPtr<UTriggerSet>>& DefaultTriggerSets)
{
	OutActivityConfig = nullptr;
	OutTriggerSets = &DefaultTriggerSets;

	if (!SpawnPoint) return;

	// NPCDefinition cascade: inline > GuardPostDefinition > (territory default already in OutDef)
	if (SpawnPoint->NPCDefinitionOverride)
	{
		OutDef = SpawnPoint->NPCDefinitionOverride;
	}
	else if (SpawnPoint->GuardPostDefinition && SpawnPoint->GuardPostDefinition->NPCDefinition)
	{
		OutDef = SpawnPoint->GuardPostDefinition->NPCDefinition;
	}

	// NPCClass from resolved definition
	if (OutDef)
	{
		UClass* ResolvedClass = OutDef->NPCClassPath.LoadSynchronous();
		if (ResolvedClass && ResolvedClass->IsChildOf(ATerritoryGuardCharacter::StaticClass()))
		{
			OutNPCClass = ResolvedClass;
		}
	}

	// ActivityConfig cascade: inline > GuardPostDefinition > nullptr
	if (SpawnPoint->ActivityConfigurationOverride)
	{
		OutActivityConfig = SpawnPoint->ActivityConfigurationOverride;
	}
	else if (SpawnPoint->GuardPostDefinition && SpawnPoint->GuardPostDefinition->ActivityConfiguration)
	{
		OutActivityConfig = SpawnPoint->GuardPostDefinition->ActivityConfiguration;
	}

	// TriggerSets cascade: inline > GuardPostDefinition > empty default
	if (!SpawnPoint->TriggerSetOverrides.IsEmpty())
	{
		OutTriggerSets = &SpawnPoint->TriggerSetOverrides;
	}
	else if (SpawnPoint->GuardPostDefinition && !SpawnPoint->GuardPostDefinition->TriggerSetOverrides.IsEmpty())
	{
		OutTriggerSets = &SpawnPoint->GuardPostDefinition->TriggerSetOverrides;
	}
}

void ATerritoryVolume::SpawnGuards()
{
	SpawnGuardsToCount(GetDesiredGuardCount());
}

void ATerritoryVolume::SpawnGuardsToCount(int32 RequestedGuardCount)
{
	if (!HasAuthority()) return;
	if (bSpawningGuards)
	{
		UE_LOG(LogTerritory, Warning, TEXT("SpawnGuards: reentrant call blocked for %s"), *GetTerritoryTag().ToString());
		return;
	}
	bSpawningGuards = true;
	struct FScopeGuard { bool& Ref; ~FScopeGuard() { Ref = false; } } GuardRef{bSpawningGuards};

	const FGameplayTag OwnerFaction = OwnershipData.OwningFaction;
	if (!OwnerFaction.IsValid() || !ResolveGuardDefinition(OwnerFaction) || !GetWorld()) return;
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugGuards();

	const int32 TargetGuardCount = FMath::Clamp(RequestedGuardCount, 0, GetMaxGuardCount());
	const int32 ExistingGuardCount = GetSpawnedGuardCount();
	if (TargetGuardCount <= ExistingGuardCount)
	{
		return;
	}

	// The unique spawn-point union defines both placement and capacity: one point,
	// one active guard. There is deliberately no random fallback.
	TArray<ATerritoryGuardSpawnPoint*> SpawnPointActors = GetGuardSpawnPoints();
	SpawnPointActors.Sort([](const ATerritoryGuardSpawnPoint& A, const ATerritoryGuardSpawnPoint& B)
	{
		return A.Priority > B.Priority;
	});
	if (SpawnPointActors.IsEmpty())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("SpawnGuards: %s has no authored guard spawn points; capacity is zero"),
			*GetTerritoryTag().ToString());
		return;
	}

	if (ExistingGuardCount == 0)
	{
		for (ATerritoryGuardSpawnPoint* SpawnPoint : SpawnPointActors)
		{
			if (SpawnPoint)
			{
				SpawnPoint->ResetReserveState();
			}
		}
	}
	if (bDebug)
	{
		UE_LOG(LogTerritory, Log, TEXT("SpawnGuards: %s spawning %d guards, faction=%s, spawn points=%d"),
			*GetTerritoryTag().ToString(), TargetGuardCount, *OwnerFaction.ToString(),
			SpawnPointActors.Num());
	}

	while (GetSpawnedGuardCount() < TargetGuardCount)
	{
		bool bSpawnedAtAuthoredPoint = false;
		for (ATerritoryGuardSpawnPoint* SpawnPoint : SpawnPointActors)
		{
			if (SpawnPoint && SpawnPoint->HasAvailableSlot()
				&& TrySpawnSingleGuard(SpawnPoint, false))
			{
				bSpawnedAtAuthoredPoint = true;
				break;
			}
		}
		if (!bSpawnedAtAuthoredPoint)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("SpawnGuards: %s has no free collision-safe authored slot (%d/%d active)"),
				*GetTerritoryTag().ToString(), GetSpawnedGuardCount(), TargetGuardCount);
			break;
		}
	}
	RefreshGarrisonSnapshot();
}

void ATerritoryVolume::SpawnSingleGuard(ATerritoryGuardSpawnPoint* SpawnPoint)
{
	TrySpawnSingleGuard(SpawnPoint, false);
}

bool ATerritoryVolume::TrySpawnSingleGuard(ATerritoryGuardSpawnPoint* SpawnPoint, bool bRequireConcealment)
{
	if (!HasAuthority() || OwnershipData.State != ETerritoryState::Claimed)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World) return false;

	FGameplayTag OwnerFaction = OwnershipData.OwningFaction;
	if (!OwnerFaction.IsValid()) return false;
	if (GetSpawnedGuardCount() >= GetMaxGuardCount()) return false;

	const TArray<ATerritoryGuardSpawnPoint*> AuthoredSpawnPoints = GetGuardSpawnPoints();
	if (!SpawnPoint || !AuthoredSpawnPoints.Contains(SpawnPoint)
		|| SpawnPoint->GetOwningTerritory() != this || !SpawnPoint->HasAvailableSlot())
	{
		return false;
	}

	// P0-07: Resolve territory-level default, then per-SP overrides via unified cascade
	UNPCDefinition* EffectiveDef = ResolveGuardDefinition(OwnerFaction);
	if (!EffectiveDef) return false;

	const TArray<TSoftObjectPtr<UTriggerSet>> DefaultSingleTriggerSets;
	UClass* NPCClass = EffectiveDef->NPCClassPath.LoadSynchronous();
	if (!NPCClass || !NPCClass->IsChildOf(ATerritoryGuardCharacter::StaticClass()))
	{
		NPCClass = ATerritoryGuardCharacter::StaticClass();
	}

	UNPCActivityConfiguration* SingleActivityConfig = nullptr;
	const TArray<TSoftObjectPtr<UTriggerSet>>* SingleTriggerSetsPtr = &DefaultSingleTriggerSets;
	ResolveSpawnPointNarrativeOverrides(SpawnPoint, EffectiveDef, NPCClass, SingleActivityConfig, SingleTriggerSetsPtr, DefaultSingleTriggerSets);

	FTransform SpawnTransform;
	if (!FindGuardSpawnTransform(SpawnPoint, NPCClass, bRequireConcealment, SpawnTransform))
	{
		return false;
	}

	ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(
		UGameplayStatics::BeginDeferredActorSpawnFromClass(
			this, NPCClass, SpawnTransform,
			ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding,
			this));

	if (!Guard) return false;

	FGuid GuardSaveGUID = FGuid::NewGuid();

	// Determine effective faction: spawn point override > territory owner
	FGameplayTag EffectiveFaction = OwnerFaction;
	if (SpawnPoint && SpawnPoint->GetEffectiveFactionOverride().IsValid())
	{
		EffectiveFaction = SpawnPoint->GetEffectiveFactionOverride();
	}

	// P0-07: Narrative overrides already resolved by unified cascade above

	// Single deterministic entrypoint — fills ALL SpawnInfo fields
	Guard->ConfigureTerritorySpawn(
		EffectiveDef,
		EffectiveFaction,
		TerritoryGUID,
		GuardSaveGUID,
		SpawnTransform,
		SpawnPoint ? SpawnPoint->GetFName() : NAME_None,
		SingleActivityConfig,
		*SingleTriggerSetsPtr);

	// Set territory AI context before FinishSpawningActor
	Guard->OwningTerritory = this;
	Guard->OwningTerritorySpawnPoint = SpawnPoint;

	AActor* FinishedGuard = UGameplayStatics::FinishSpawningActor(Guard, SpawnTransform);
	if (!IsValid(FinishedGuard) || Guard->IsActorBeingDestroyed())
	{
		return false;
	}

	SpawnedGuards.Add(Guard);
	RegisterDefender(Guard);
	if (SpawnPoint)
	{
		SpawnPoint->RegisterSpawnedGuard(Guard);
	}
	RefreshGarrisonSnapshot();

	UE_LOG(LogTerritory, Log, TEXT("[GuardReserve] 1 replacement spawned at %s for %s (faction=%s)"),
		SpawnPoint ? *SpawnPoint->GetName() : TEXT("random"),
		*GetTerritoryTag().ToString(),
		*OwnerFaction.ToString());
	return true;
}

bool ATerritoryVolume::FindGuardSpawnTransform(ATerritoryGuardSpawnPoint* SpawnPoint, UClass* GuardClass,
	bool bRequireConcealment, FTransform& OutTransform) const
{
	if (!SpawnPoint)
	{
		return false;
	}

	if (!bRequireConcealment)
	{
		if (!SpawnPoint->ResolveGuardDeploymentTransform(GuardClass, OutTransform)
			|| !IsGuardSpawnLocationClear(GuardClass, OutTransform.GetLocation()))
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("Guard spawn point %s is blocked at its authored location; no relocation was attempted"),
				*SpawnPoint->GetPathName());
			return false;
		}
		return true;
	}

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;
	if (!World || !NavSystem || !GuardClass)
	{
		return false;
	}

	float CapsuleHalfHeight = 88.f;
	if (const ATerritoryGuardCharacter* GuardCDO = GuardClass->GetDefaultObject<ATerritoryGuardCharacter>())
	{
		if (const UCapsuleComponent* Capsule = GuardCDO->GetCapsuleComponent())
		{
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	struct FPlayerView
	{
		FVector Location;
		FVector Forward;
	};
	TArray<FPlayerView> PlayerViews;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController) continue;

		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FVector ViewForward = ViewRotation.Vector();
		ViewForward.Z = 0.f;
		if (!ViewForward.Normalize()) continue;
		PlayerViews.Add({ViewLocation, ViewForward});
	}

	const FVector Origin = SpawnPoint->GetSpawnTransform().GetLocation();
	const int32 Attempts = FMath::Clamp(SpawnPoint->GetEffectiveCandidateCount(), 1, 64);
	const float Radius = FMath::Max(100.f, SpawnPoint->GetEffectiveReserveRadius());
	const float MinimumPlayerDistanceSq = FMath::Square(FMath::Max(0.f, SpawnPoint->GetEffectiveMinimumPlayerDistance()));
	float BestFacingScore = TNumericLimits<float>::Max();
	bool bFoundCandidate = false;
	FVector BestLocation = FVector::ZeroVector;

	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		FNavLocation NavLocation;
		if (!NavSystem->GetRandomReachablePointInRadius(Origin, Radius, NavLocation))
		{
			continue;
		}

		const FVector Candidate = NavLocation.Location + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
		if (!IsGuardSpawnLocationClear(GuardClass, Candidate))
		{
			continue;
		}

		bool bConcealed = true;
		float WorstFacingScore = -1.f;
		for (const FPlayerView& View : PlayerViews)
		{
			FVector ToCandidate = Candidate - View.Location;
			if (ToCandidate.SizeSquared() < MinimumPlayerDistanceSq)
			{
				bConcealed = false;
				break;
			}

			ToCandidate.Z = 0.f;
			if (!ToCandidate.Normalize())
			{
				bConcealed = false;
				break;
			}

			const float FacingScore = FVector::DotProduct(View.Forward, ToCandidate);
			WorstFacingScore = FMath::Max(WorstFacingScore, FacingScore);
			if (FacingScore > 0.f)
			{
				bConcealed = false;
				break;
			}
		}

		if (bConcealed && WorstFacingScore < BestFacingScore)
		{
			BestFacingScore = WorstFacingScore;
			BestLocation = Candidate;
			bFoundCandidate = true;
		}
	}

	if (!bFoundCandidate)
	{
		return false;
	}

	OutTransform = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), BestLocation);
	return true;
}

bool ATerritoryVolume::IsGuardSpawnLocationClear(UClass* GuardClass, const FVector& Location) const
{
	const UWorld* World = GetWorld();
	const ATerritoryGuardCharacter* GuardCDO = GuardClass
		? GuardClass->GetDefaultObject<ATerritoryGuardCharacter>()
		: nullptr;
	const UCapsuleComponent* Capsule = GuardCDO ? GuardCDO->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TerritoryReserveSpawn), false, this);
	return !World->OverlapBlockingTestByProfile(
		Location,
		FQuat::Identity,
		Capsule->GetCollisionProfileName(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
		QueryParams);
}

int32 ATerritoryVolume::GetGuardPurchaseCost(int32 Count) const
{
	return GetGuardRecruitmentCost(Count);
}

bool ATerritoryVolume::CanPurchaseGuards(const AActor* Requester, int32 Count, FText& OutFailureReason) const
{
	if (Count <= 0 || Count > GetMaxGuardCount() - GetDesiredGuardCount())
	{
		OutFailureReason = FText::FromString(TEXT("The requested guard increase exceeds capacity."));
		return false;
	}
	int32 RecruitmentCost = 0;
	return CanSetDesiredGuardCount(Requester, GetDesiredGuardCount() + Count,
		OutFailureReason, RecruitmentCost);
}

bool ATerritoryVolume::TryPurchaseGuards(AActor* Requester, int32 Count, FText& OutResult)
{
	if (Count <= 0 || Count > GetMaxGuardCount() - GetDesiredGuardCount())
	{
		OutResult = FText::FromString(TEXT("The requested guard increase exceeds capacity."));
		return false;
	}
	const FTerritoryGarrisonMutationResult Result =
		TrySetDesiredGuardCount(Requester, GetDesiredGuardCount() + Count);
	OutResult = Result.Message;
	return Result.bSuccess;
}

bool ATerritoryVolume::CanRemoveGuards(const AActor* Requester, int32 Count, FText& OutFailureReason) const
{
	if (Count <= 0 || Count > GetDesiredGuardCount())
	{
		OutFailureReason = FText::FromString(TEXT("The garrison target cannot be reduced by that amount."));
		return false;
	}
	int32 RecruitmentCost = 0;
	return CanSetDesiredGuardCount(Requester, GetDesiredGuardCount() - Count,
		OutFailureReason, RecruitmentCost);
}

bool ATerritoryVolume::TryRemoveGuards(AActor* Requester, int32 Count, FText& OutResult)
{
	if (Count <= 0 || Count > GetDesiredGuardCount())
	{
		OutResult = FText::FromString(TEXT("The garrison target cannot be reduced by that amount."));
		return false;
	}
	const FTerritoryGarrisonMutationResult Result =
		TrySetDesiredGuardCount(Requester, GetDesiredGuardCount() - Count);
	OutResult = Result.Message;
	return Result.bSuccess;
}

bool ATerritoryVolume::CanSetDesiredGuardCount(const AActor* Requester,
	int32 NewDesiredGuardCount, FText& OutFailureReason, int32& OutRecruitmentCost) const
{
	OutFailureReason = FText::GetEmpty();
	OutRecruitmentCost = 0;
	if (!Requester || NewDesiredGuardCount < 0 || NewDesiredGuardCount > GetMaxGuardCount())
	{
		OutFailureReason = FText::FromString(TEXT("The requested garrison target is invalid."));
		return false;
	}
	if (OwnershipData.State != ETerritoryState::Claimed)
	{
		OutFailureReason = FText::FromString(TEXT("The territory must be claimed."));
		return false;
	}
	if (!UTerritoryBlueprintLibrary::IsActorInFaction(
		this, const_cast<AActor*>(Requester), OwnershipData.OwningFaction))
	{
		OutFailureReason = FText::FromString(TEXT("Only the owning faction can manage this garrison."));
		return false;
	}
	if (NewDesiredGuardCount == GetDesiredGuardCount())
	{
		OutFailureReason = FText::FromString(TEXT("The garrison already has that staffing target."));
		return false;
	}
	// A missing definition must block new recruitment, but it must never trap the
	// player at an already-authored/saved target. Reducing a target is valid even
	// when no guard can currently be spawned (for example after a data migration).
	if (NewDesiredGuardCount > GetDesiredGuardCount()
		&& !ResolveGuardDefinition(OwnershipData.OwningFaction))
	{
		OutFailureReason = FText::FromString(TEXT("No guard NPC definition is configured for this faction."));
		return false;
	}

	const int32 Increase = FMath::Max(0, NewDesiredGuardCount - GetDesiredGuardCount());
	OutRecruitmentCost = GetGuardRecruitmentCost(Increase);
	if (OutRecruitmentCost == MAX_int32)
	{
		OutFailureReason = FText::FromString(TEXT("The recruitment cost exceeds the supported currency range."));
		return false;
	}
	if (OutRecruitmentCost > 0)
	{
		const UWorld* World = GetWorld();
		const UTerritoryEconomySubsystem* Economy = World
			? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
		if (!Economy || !Economy->CanActorAfford(Requester, OutRecruitmentCost))
		{
			OutFailureReason = FText::FromString(TEXT("Your Narrative inventory account cannot afford this staffing target."));
			return false;
		}
	}
	return true;
}

FTerritoryGarrisonMutationResult ATerritoryVolume::TrySetDesiredGuardCount(
	AActor* Requester, int32 NewDesiredGuardCount)
{
	FTerritoryGarrisonMutationResult Result;
	Result.OldDesiredGuards = GetDesiredGuardCount();
	Result.NewDesiredGuards = Result.OldDesiredGuards;
	Result.OldActiveGuards = GetSpawnedGuardCount();
	Result.NewActiveGuards = Result.OldActiveGuards;

	if (!HasAuthority())
	{
		Result.Message = FText::FromString(TEXT("Garrison targets can only be changed by the server."));
		return Result;
	}

	FText FailureReason;
	if (!CanSetDesiredGuardCount(Requester, NewDesiredGuardCount,
		FailureReason, Result.RecruitmentCost))
	{
		Result.Message = FailureReason;
		return Result;
	}

	const int32 Increase = FMath::Max(0, NewDesiredGuardCount - Result.OldDesiredGuards);
	const int32 GuardsToDeploy = FMath::Min(Increase,
		FMath::Max(0, NewDesiredGuardCount - Result.OldActiveGuards));
	const int32 GuardsToWithdraw = FMath::Max(0, Result.OldActiveGuards - NewDesiredGuardCount);

	TArray<ATerritoryGuardCharacter*> WithdrawalPlan;
	WithdrawalPlan.Reserve(GuardsToWithdraw);
	for (int32 Index = SpawnedGuards.Num() - 1;
		Index >= 0 && WithdrawalPlan.Num() < GuardsToWithdraw; --Index)
	{
		if (ATerritoryGuardCharacter* Guard = SpawnedGuards[Index].Get())
		{
			WithdrawalPlan.Add(Guard);
		}
	}
	if (WithdrawalPlan.Num() != GuardsToWithdraw)
	{
		Result.Message = FText::FromString(TEXT("The live garrison changed before the target could be committed."));
		return Result;
	}

	UTerritoryEconomySubsystem* Economy = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
	if (Result.RecruitmentCost > 0)
	{
		const FString Reason = FString::Printf(TEXT("Raised garrison target for %s from %d to %d"),
			*GetTerritoryTag().ToString(), Result.OldDesiredGuards, NewDesiredGuardCount);
		if (!Economy || !Economy->TryDebitCurrency(Requester, Result.RecruitmentCost,
			OwnershipData.OwningFaction, Reason, ETerritoryTransactionType::Purchase))
		{
			Result.Message = FText::FromString(TEXT("The Narrative inventory balance changed before recruitment committed."));
			return Result;
		}
	}

	TSet<ATerritoryGuardCharacter*> GuardsBeforeDeployment;
	for (const TWeakObjectPtr<ATerritoryGuardCharacter>& GuardPtr : SpawnedGuards)
	{
		if (GuardPtr.IsValid()) GuardsBeforeDeployment.Add(GuardPtr.Get());
	}
	bGarrisonMutationInProgress = true;

	TArray<ATerritoryGuardSpawnPoint*> SpawnPoints = GetGuardSpawnPoints();
	SpawnPoints.Sort([](const ATerritoryGuardSpawnPoint& A, const ATerritoryGuardSpawnPoint& B)
	{
		return A.Priority > B.Priority;
	});
	for (int32 Index = 0; Index < GuardsToDeploy; ++Index)
	{
		bool bDeployed = false;
		for (ATerritoryGuardSpawnPoint* Point : SpawnPoints)
		{
			if (Point && Point->HasAvailableSlot()
				&& TrySpawnSingleGuard(Point, false))
			{
				++Result.GuardsDeployed;
				bDeployed = true;
				break;
			}
		}
		if (!bDeployed)
		{
			break;
		}
	}

	if (Result.GuardsDeployed != GuardsToDeploy)
	{
		TArray<ATerritoryGuardCharacter*> RollbackGuards;
		for (const TWeakObjectPtr<ATerritoryGuardCharacter>& GuardPtr : SpawnedGuards)
		{
			if (GuardPtr.IsValid() && !GuardsBeforeDeployment.Contains(GuardPtr.Get()))
			{
				RollbackGuards.Add(GuardPtr.Get());
			}
		}
		for (ATerritoryGuardCharacter* Guard : RollbackGuards)
		{
			RemoveGuardWithoutReplacement(Guard);
		}
		if (Economy && Result.RecruitmentCost > 0)
		{
			Economy->CreditCurrency(Requester, Result.RecruitmentCost, OwnershipData.OwningFaction,
				FString::Printf(TEXT("Rolled back failed garrison target for %s"), *GetTerritoryTag().ToString()),
				ETerritoryTransactionType::Purchase);
		}
		Result.GuardsDeployed = 0;
		Result.NewActiveGuards = GetSpawnedGuardCount();
		Result.Message = FText::FromString(TEXT("The complete garrison deployment could not be placed; no staffing change was committed."));
		bGarrisonMutationInProgress = false;
		RefreshGarrisonSnapshot();
		return Result;
	}

	if (NewDesiredGuardCount < Result.OldDesiredGuards)
	{
		CancelPendingReserveDeployments();
	}
	for (ATerritoryGuardCharacter* Guard : WithdrawalPlan)
	{
		RemoveGuardWithoutReplacement(Guard);
		++Result.GuardsWithdrawn;
	}

	OwnershipData.DesiredGuardCount = NewDesiredGuardCount;
	CleanupInvalidDefenders();
	OwnershipData.DefenderCount = RegisteredDefenders.Num();
	if (Economy)
	{
		Economy->RecalculateIncome(OwnershipData.OwningFaction);
	}
	bGarrisonMutationInProgress = false;
	RefreshGarrisonSnapshot();
	ForceNetUpdate();

	Result.bSuccess = true;
	Result.NewDesiredGuards = NewDesiredGuardCount;
	Result.NewActiveGuards = GetSpawnedGuardCount();
	Result.Message = FText::Format(
		NSLOCTEXT("TerritoryVolume", "GarrisonTargetChanged",
			"{0} staffing target set to {1}. Active {2}; recruitment {3}; upkeep {4} per cycle."),
		GetTerritoryDisplayName(),
		FText::AsNumber(NewDesiredGuardCount),
		FText::AsNumber(Result.NewActiveGuards),
		FText::AsNumber(Result.RecruitmentCost),
		FText::AsNumber(static_cast<int64>(FMath::Max(0, OwnershipData.GuardCost)) * NewDesiredGuardCount));
	return Result;
}

void ATerritoryVolume::DespawnGuards()
{
	TArray<ATerritoryGuardSpawnPoint*> CachedSpawnPoints = GetGuardSpawnPoints();
	for (ATerritoryGuardSpawnPoint* SpawnPoint : CachedSpawnPoints)
	{
		if (SpawnPoint)
		{
			SpawnPoint->CancelPendingReserveSpawns();
		}
	}

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
	RefreshGarrisonSnapshot();

	UE_LOG(LogTerritory, Log, TEXT("Despawned all guards for %s"),
		*GetTerritoryTag().ToString());
}

int32 ATerritoryVolume::GetSpawnedGuardCount() const
{
	if (!HasAuthority())
	{
		return FMath::Max(0, GarrisonSnapshot.ActiveGuards);
	}
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

TArray<ATerritoryGuardSpawnPoint*> ATerritoryVolume::GetGuardSpawnPoints() const
{
	TArray<ATerritoryGuardSpawnPoint*> Result;
	for (const TObjectPtr<AActor>& Ptr : GuardSpawnPoints)
	{
		if (ATerritoryGuardSpawnPoint* SP = Cast<ATerritoryGuardSpawnPoint>(Ptr))
		{
			Result.AddUnique(SP);
		}
	}
	for (const TWeakObjectPtr<ATerritoryGuardSpawnPoint>& SpawnPointPtr : ResolvedGuardSpawnPoints)
	{
		if (ATerritoryGuardSpawnPoint* SpawnPoint = SpawnPointPtr.Get())
		{
			Result.AddUnique(SpawnPoint);
		}
	}
	return Result;
}

void ATerritoryVolume::RegisterResolvedGuardSpawnPoint(ATerritoryGuardSpawnPoint* SpawnPoint)
{
	if (!SpawnPoint) return;
	ResolvedGuardSpawnPoints.RemoveAllSwap([](const TWeakObjectPtr<ATerritoryGuardSpawnPoint>& Entry)
	{
		return !Entry.IsValid();
	});
	if (!ResolvedGuardSpawnPoints.Contains(SpawnPoint))
	{
		ResolvedGuardSpawnPoints.Add(SpawnPoint);
	}

	if (HasAuthority() && HasActorBegunPlay())
	{
		if (OwnershipData.State == ETerritoryState::Claimed
			&& GetSpawnedGuardCount() < GetDesiredGuardCount())
		{
			SpawnGuardsToCount(GetDesiredGuardCount());
		}
		RefreshGarrisonSnapshot();
	}
}

void ATerritoryVolume::UnregisterResolvedGuardSpawnPoint(ATerritoryGuardSpawnPoint* SpawnPoint)
{
	ResolvedGuardSpawnPoints.RemoveAllSwap([SpawnPoint](
		const TWeakObjectPtr<ATerritoryGuardSpawnPoint>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == SpawnPoint;
	});
	if (HasAuthority() && HasActorBegunPlay())
	{
		RefreshGarrisonSnapshot();
	}
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
		GetMaxGuardCount(),
		GetDefenderCount());
}
