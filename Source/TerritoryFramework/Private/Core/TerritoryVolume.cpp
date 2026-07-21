#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "SaveSystemStatics.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Core/TerritoryGuardCharacter.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Character/CharacterDefinition.h"
#include "Kismet/GameplayStatics.h"

ATerritoryVolume::ATerritoryVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BoundsShape = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsShape"));
	RootComponent = BoundsShape;

	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		Box->SetBoxExtent(FVector(500.f, 500.f, 200.f));
		// Invisible during gameplay — editor-only visualization
		Box->SetHiddenInGame(true, true);
		// No collision at all — weapon traces, projectiles, fist attacks,
		// and pawn movement must pass through freely. The box is purely
		// an editor visualization and a bounds reference for spawn logic.
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Box->SetCanEverAffectNavigation(false);
		Box->bHiddenInGame = true;
		Box->bVisibleInReflectionCaptures = false;
	}

	// NOTE: OwnershipData defaults are synced from Initial* properties in BeginPlay,
	// not here in the constructor. The constructor runs with CDO default values before
	// Blueprint instance overrides are applied, causing Initial* edits to be ignored.
}

void ATerritoryVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!TerritoryGUID.IsValid())
		{
			TerritoryGUID = FGuid::NewGuid();
		}

		// Sync Initial* → OwnershipData.* using instance values (not CDO defaults).
		// This must happen in BeginPlay, not the constructor, because Blueprint CDO
		// overrides for InitialPeriodicIncome/InitialMaxConcurrentAttackers/InitialGuardCost
		// are not yet applied when the constructor runs.
		OwnershipData.MaxConcurrentAttackers = InitialMaxConcurrentAttackers;
		OwnershipData.PeriodicIncome = InitialPeriodicIncome;
		OwnershipData.GuardCost = InitialGuardCost;

		if (OwnershipData.State == ETerritoryState::Unclaimed && InitialOwningFaction.IsValid())
		{
			OwnershipData.OwningFaction = InitialOwningFaction;
			OwnershipData.State = ETerritoryState::Claimed;
		}

		if (bStartsLocked && OwnershipData.State == ETerritoryState::Unclaimed)
		{
			OwnershipData.State = ETerritoryState::Locked;
		}

		// Load saved data — overrides the Initial* defaults above if a save exists
		USaveSystemStatics::LoadSingleActor(this);

		// Spawn initial guards if territory is claimed and has a guard definition
		if (OwnershipData.State == ETerritoryState::Claimed
			&& OwnershipData.OwningFaction.IsValid()
			&& GuardNPCDefinition
			&& GuardSpawnCount > 0
			&& SpawnedGuards.Num() == 0)
		{
			SpawnGuards();
		}
	}

	if (UTerritoryRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->RegisterTerritory(this);
	}
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
	// Broadcast delegates so client-side UI, markers, and AI can react
	OnTerritoryControlChanged.Broadcast(this, FGameplayTag(), OwnershipData.OwningFaction);
	OnTerritoryStateChanged.Broadcast(this, OwnershipData.State);

	UE_LOG(LogTerritory, Verbose, TEXT("[Client] %s ownership rep'd: %s, state=%d, progress=%.2f"),
		*GetTerritoryTag().ToString(),
		*OwnershipData.OwningFaction.ToString(),
		static_cast<int32>(OwnershipData.State),
		OwnershipData.ControlProgress);
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
			*GetActorLabel(), *TerritoryGUID.ToString());
	}
}

void ATerritoryVolume::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// Duplicated actors must get a NEW GUID to prevent save/load conflicts
	TerritoryGUID = FGuid::NewGuid();
	UE_LOG(LogTerritory, Log, TEXT("Assigned new GUID to duplicated territory %s: %s"),
		*GetActorLabel(), *TerritoryGUID.ToString());
}
#endif

FGuid ATerritoryVolume::GetActorGUID_Implementation() const { return TerritoryGUID; }
void ATerritoryVolume::SetActorGUID_Implementation(const FGuid& NewGUID) { TerritoryGUID = NewGUID; }
void ATerritoryVolume::PrepareForSave_Implementation() { /* OwnershipData auto-saved via SaveGame */ }

void ATerritoryVolume::Load_Implementation()
{
	for (const TWeakObjectPtr<AActor>& DefenderPtr : RegisteredDefenders)
	{
		if (DefenderPtr.IsValid())
		{
			BindDefenderDeath(DefenderPtr.Get());
		}
	}
}

bool ATerritoryVolume::ShouldRespawn_Implementation() const { return false; }

FGameplayTag ATerritoryVolume::GetOwningFaction() const { return OwnershipData.OwningFaction; }
ETerritoryState ATerritoryVolume::GetTerritoryState() const { return OwnershipData.State; }
float ATerritoryVolume::GetControlProgress() const { return OwnershipData.ControlProgress; }
bool ATerritoryVolume::IsContested() const { return OwnershipData.State == ETerritoryState::Contested; }
FGameplayTag ATerritoryVolume::GetTerritoryTag() const { return TerritoryTag; }
FText ATerritoryVolume::GetTerritoryDisplayName() const { return TerritoryDisplayName; }
int32 ATerritoryVolume::GetMaxConcurrentAttackers() const { return OwnershipData.MaxConcurrentAttackers; }
int32 ATerritoryVolume::GetDefenderCount() const { return RegisteredDefenders.Num(); }
int32 ATerritoryVolume::GetPeriodicIncome() const { return OwnershipData.PeriodicIncome; }

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
		FVector ClosestPoint;
		float DistanceSq;
		Box->GetSquaredDistanceToCollision(WorldPoint, DistanceSq, ClosestPoint);
		return DistanceSq == 0.f;
	}
	if (BoundsShape)
	{
		FBoxSphereBounds Bounds = BoundsShape->CalcBounds(BoundsShape->GetComponentTransform());
		return Bounds.GetBox().IsInside(WorldPoint);
	}
	return false;
}

void ATerritoryVolume::SetOwningFaction(const FGameplayTag& NewFaction)
{
	if (!HasAuthority()) return;

	FGameplayTag OldOwner = OwnershipData.OwningFaction;
	if (OldOwner == NewFaction) return;

	OwnershipData.OwningFaction = NewFaction;
	OwnershipData.State = NewFaction.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
	OwnershipData.ContestingFaction = FGameplayTag();
	OwnershipData.ControlProgress = NewFaction.IsValid() ? 1.f : 0.f;

	OnOwnershipChanged(OldOwner, NewFaction);
	OnTerritoryControlChanged.Broadcast(this, OldOwner, NewFaction);
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
	OwnershipData.State = NewState;
	OnStateChanged(OldState, NewState);
	OnTerritoryStateChanged.Broadcast(this, NewState);
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
	// Despawn old guards, spawn new ones for the new owner
	DespawnGuards();

	if (NewOwner.IsValid() && GuardNPCDefinition && GuardSpawnCount > 0)
	{
		SpawnGuards();
	}
}

void ATerritoryVolume::OnStateChanged_Implementation(ETerritoryState OldState, ETerritoryState NewState)
{
}

void ATerritoryVolume::OnDefenderDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC)
{
	UnregisterDefender(KilledActor);
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

// ═══════════════════════════════════════════════════════════════════════════════
// Guard Spawning
// ═══════════════════════════════════════════════════════════════════════════════

void ATerritoryVolume::SpawnGuards()
{
	if (!HasAuthority() || !GuardNPCDefinition) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Determine NPC class from definition, default to ATerritoryGuardCharacter
	// which properly implements GetActorGUID to prevent save system assertion crash
	UClass* NPCClass = GuardNPCDefinition->NPCClassPath.Get();
	if (!NPCClass || !NPCClass->IsChildOf(ATerritoryGuardCharacter::StaticClass()))
	{
		NPCClass = ATerritoryGuardCharacter::StaticClass();
	}

	// Read faction from NPC Definition's DefaultFactions
	FGameplayTagContainer DefFactions = GuardNPCDefinition->DefaultFactions;

	for (int32 i = 0; i < GuardSpawnCount; ++i)
	{
		FVector SpawnLoc = GetRandomSpawnPoint();
		FTransform SpawnTransform(FRotator(0, FMath::FRandRange(0.f, 360.f), 0), SpawnLoc);

		// Deferred spawning: BeginDeferred creates actor WITHOUT calling PostSpawnInitialize
		// so OnActorSpawned does NOT fire yet — giving us a window to set the GUID
		ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(
			UGameplayStatics::BeginDeferredActorSpawnFromClass(
				this, NPCClass, SpawnTransform,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn,
				this));

		if (!Guard)
		{
			UE_LOG(LogTerritory, Warning, TEXT("Failed to begin deferred spawn for guard %d/%d of %s"),
				i + 1, GuardSpawnCount, *GetTerritoryTag().ToString());
			continue;
		}

		// Set save GUID BEFORE FinishSpawning triggers OnActorSpawned
		FGuid GuardSaveGUID = FGuid::NewGuid();
		Guard->SetTerritorySaveGUID(GuardSaveGUID);
		Guard->SetOwningTerritoryGUID(TerritoryGUID);

		// Set NPC definition
		Guard->SetNPCDefinition(GuardNPCDefinition);

		// FinishSpawning triggers PostSpawnInitialize → OnActorFinishedSpawning
		// → OnActorSpawned → SaveSubsystem looks up GUID → GetActorGUID returns valid GUID
		UGameplayStatics::FinishSpawningActor(Guard, SpawnTransform);

		// Add factions from NPC Definition's DefaultFactions
		if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Guard))
		{
			for (const FGameplayTag& FactionTag : DefFactions)
			{
				TeamAgent->AddFaction(FactionTag);
			}
		}

		SpawnedGuards.Add(Guard);
		RegisterDefender(Guard);

		UE_LOG(LogTerritory, Log, TEXT("Spawned guard %d/%d for %s (GUID: %s)"),
			i + 1, GuardSpawnCount,
			*GetTerritoryTag().ToString(),
			*GuardSaveGUID.ToString());
	}
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

	if (UBoxComponent* Box = Cast<UBoxComponent>(BoundsShape))
	{
		FVector Extent = Box->GetScaledBoxExtent();
		return Center + FVector(
			FMath::FRandRange(-Extent.X, Extent.X),
			FMath::FRandRange(-Extent.Y, Extent.Y),
			0.f);
	}

	return Center + FVector(
		FMath::FRandRange(-GuardSpawnRadius, GuardSpawnRadius),
		FMath::FRandRange(-GuardSpawnRadius, GuardSpawnRadius),
		0.f);
}
