#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "AI/TerritoryPatrolGoal.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Components/EquipmentComponent.h"
#include "Items/WeaponItem.h"
#include "Tales/TriggerSet.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PatrolGoalClass = UTerritoryPatrolGoal::StaticClass();
}

bool ATerritoryGuardCharacter::ShouldRespawn_Implementation() const
{
	return false;
}

float ATerritoryGuardCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// P1-N14: Only update LastDamagingInstigator when actual damage is applied (> 0).
	// When damage is fully absorbed (e.g. shield, invulnerability), we don't want
	// to attribute a "last damager" that had zero effect.
	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage > 0.f)
	{
		if (EventInstigator && EventInstigator->GetPawn())
		{
			LastDamagingInstigator = EventInstigator->GetPawn();
		}
		else if (DamageCauser)
		{
			APawn* InstigatorPawn = DamageCauser->GetInstigator();
			LastDamagingInstigator = InstigatorPawn ? InstigatorPawn : DamageCauser;
		}
	}
	return ActualDamage;
}

void ATerritoryGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			InitializeTerritoryPatrolGoal();
		}));

		// NPC definitions populate inventory after BeginPlay; retry briefly until the
		// first equipped weapon is available, then make it the mainhand weapon.
		GetWorldTimerManager().SetTimer(
			DefaultWeaponWieldTimer,
			this,
			&ATerritoryGuardCharacter::TryWieldDefaultWeapon,
			0.25f,
			true,
			0.25f);
	}
}

void ATerritoryGuardCharacter::TryWieldDefaultWeapon()
{
	if (++DefaultWeaponWieldAttempts > 40)
	{
		GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
		return;
	}

	if (GetWeapon(true))
	{
		return;
	}

	UEquipmentComponent* Equipment = GetEquipmentComponent();
	if (!Equipment)
	{
		return;
	}

	const FGameplayTag MainhandWieldSlot = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Narrative.Equipment.WieldSlot.Mainhand")), false);
	const TArray<FGameplayTag> WeaponSlots =
	{
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.HipLeft")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.HipRight")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.Back")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.BackA")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.BackB")), false),
		FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Equipment.Slot.Weapon.Hip")), false)
	};

	for (const FGameplayTag& EquipSlot : WeaponSlots)
	{
		UWeaponItem* Weapon = Equipment->GetEquippedWeaponAtSlot(EquipSlot);
		if (!Weapon)
		{
			Weapon = Cast<UWeaponItem>(Equipment->GetEquippedItemAtSlot(EquipSlot));
		}
		if (!Weapon)
		{
			continue;
		}

		FWeaponWieldState DesiredWieldState;
		DesiredWieldState.EquipSlots.AddTag(EquipSlot);
		DesiredWieldState.WieldSlots.AddTag(MainhandWieldSlot);
		SetWieldState(DesiredWieldState);
		GetWorldTimerManager().ClearTimer(DefaultWeaponWieldTimer);
		return;
	}

	// P2-N16: Warn if no valid weapon slots found on first attempt
	if (DefaultWeaponWieldAttempts == 1)
	{
		UE_LOG(LogTerritory, Warning, TEXT("GuardCharacter %s: no valid weapon slot tags found — guard will retry wielding 40 times"),
			*GetName());
	}
}

void ATerritoryGuardCharacter::ApplyActivityConfig_Implementation(UNPCActivityConfiguration* NPCActivityConfig)
{
	Super::ApplyActivityConfig_Implementation(NPCActivityConfig);
	InitializeTerritoryPatrolGoal();
	TryWieldDefaultWeapon();
}

void ATerritoryGuardCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATerritoryGuardCharacter, TerritoryHomeTransform);
	DOREPLIFETIME(ATerritoryGuardCharacter, OwningTerritory);
	DOREPLIFETIME(ATerritoryGuardCharacter, OwningTerritorySpawnPoint);
}

void ATerritoryGuardCharacter::ConfigureTerritorySpawn(
	UNPCDefinition* Definition,
	const FGameplayTag& ExactFaction,
	const FGuid& TerritoryGuid,
	const FGuid& SaveGuid,
	const FTransform& InSpawnTransform,
	FName SpawnPointName,
	UNPCActivityConfiguration* OptionalActivityOverride,
	const TArray<TSoftObjectPtr<UTriggerSet>>& OptionalTriggerOverrides)
{
	// Fill ALL SpawnInfo fields that Narrative activities need.
	// BPA_ReturnToSpawn reads SpawnTransform — without it, SetupBlackboard fails.

	SpawnInfo.SpawnAssignedSaveGUID = SaveGuid;
	SpawnInfo.SpawnTransform = InSpawnTransform;
	SpawnInfo.SpawnName = SpawnPointName;

	// TerritoryVolume is not a Narrative NPCSpawnComponent.
	// Mark OwningSpawnerGUID as the territory's GUID for identification,
	// but OwningSpawn is left null since there's no UNPCSpawnComponent.
	SpawnInfo.OwningSpawnerGUID = TerritoryGuid;

	// Store territory context for BP-accessible ReturnToTerritory activity
	TerritoryHomeTransform = InSpawnTransform;

	// Override factions to exactly the territory owner
	if (ExactFaction.IsValid())
	{
		SpawnInfo.SpawnParams.bOverride_DefaultFactions = true;
		SpawnInfo.SpawnParams.DefaultFactions.Reset();
		SpawnInfo.SpawnParams.DefaultFactions.AddTag(ExactFaction);
	}

	// Optional activity configuration override
	if (OptionalActivityOverride)
	{
		SpawnInfo.SpawnParams.bOverride_ActivityConfiguration = true;
		SpawnInfo.SpawnParams.ActivityConfiguration = FSoftObjectPath(OptionalActivityOverride);
	}

	// Optional trigger set overrides
	if (!OptionalTriggerOverrides.IsEmpty())
	{
		SpawnInfo.SpawnParams.bOverride_TriggerSets = true;
		SpawnInfo.SpawnParams.TriggerSets = OptionalTriggerOverrides;
	}

	// P1-N13: SetNPCDefinition must be called here, but OwningTerritory and
	// OwningTerritorySpawnPoint are NOT set in this function — they must be
	// assigned by the caller after ConfigureTerritorySpawn returns. This is
	// by design: the caller owns the territory↔spawn-point binding lifecycle.
	if (Definition)
	{
		SetNPCDefinition(Definition);
	}
}

FGuid ATerritoryGuardCharacter::GetActorGUID_Implementation() const
{
	if (SpawnInfo.SpawnAssignedSaveGUID.IsValid())
	{
		return SpawnInfo.SpawnAssignedSaveGUID;
	}

	// WARNING: This fallback generates a runtime GUID that differs each session.
	// Currently safe because ShouldRespawn_Implementation returns false — guards
	// are never saved. If a subclass overrides ShouldRespawn to return true, the
	// GUID will not match between save and load sessions, silently breaking save/load.
	// Override GetActorGUID in the subclass to provide a stable GUID (e.g. from
	// SpawnAssignedSaveGUID which is set in ConfigureTerritorySpawn).
	if (!CachedFallbackGUID.IsValid())
	{
		const_cast<ATerritoryGuardCharacter*>(this)->CachedFallbackGUID = FGuid::NewGuid();
	}
	return CachedFallbackGUID;
}

void ATerritoryGuardCharacter::SetActorGUID_Implementation(const FGuid& NewGUID)
{
	SpawnInfo.SpawnAssignedSaveGUID = NewGUID;
}

TArray<FTerritoryPatrolNode> ATerritoryGuardCharacter::GetTerritoryPatrolRoute() const
{
	if (IsValid(OwningTerritorySpawnPoint))
	{
		return OwningTerritorySpawnPoint->GetPatrolRoute();
	}
	return TArray<FTerritoryPatrolNode>();
}

bool ATerritoryGuardCharacter::HasTerritoryPatrolRoute() const
{
	if (!IsValid(OwningTerritorySpawnPoint))
	{
		return false;
	}
	return OwningTerritorySpawnPoint->HasPatrolRoute();
}

int32 ATerritoryGuardCharacter::GetPatrolNodeCount() const
{
	if (IsValid(OwningTerritorySpawnPoint))
	{
		return OwningTerritorySpawnPoint->GetPatrolRoute().Num();
	}
	return 0;
}

bool ATerritoryGuardCharacter::GetSafePatrolNode(int32 Index, FTerritoryPatrolNode& OutNode) const
{
	if (!IsValid(OwningTerritorySpawnPoint))
	{
		return false;
	}
	const TArray<FTerritoryPatrolNode>& Route = OwningTerritorySpawnPoint->GetPatrolRoute();
	if (!Route.IsValidIndex(Index))
	{
		return false;
	}
	OutNode = Route[Index];
	return true;
}

FTransform ATerritoryGuardCharacter::GetSpawnTransform() const
{
	return TerritoryHomeTransform;
}

ATerritoryVolume* ATerritoryGuardCharacter::GetOwningTerritory() const
{
	return OwningTerritory;
}

FGameplayTag ATerritoryGuardCharacter::GetGuardFaction() const
{
	TArray<FGameplayTag> Factions;
	GetFactions().GetGameplayTagArray(Factions);
	if (!Factions.IsEmpty())
	{
		return Factions[0];
	}
	return FGameplayTag();
}

bool ATerritoryGuardCharacter::IsSpawnPointGuard() const
{
	return IsValid(OwningTerritorySpawnPoint);
}

bool ATerritoryGuardCharacter::InitializeTerritoryPatrolGoal()
{
	if (!HasAuthority() || !HasTerritoryPatrolRoute() || !PatrolGoalClass)
	{
		return false;
	}

	UNPCActivityComponent* ActivityComponent = GetActivityComponent();
	if (!ActivityComponent)
	{
		return false;
	}

	UObject* GoalKey = OwningTerritorySpawnPoint ? static_cast<UObject*>(OwningTerritorySpawnPoint) : this;
	bool bFoundExisting = false;
	if (UTerritoryPatrolGoal* ExistingGoal = Cast<UTerritoryPatrolGoal>(
		ActivityComponent->GetGoalByKey(PatrolGoalClass, GoalKey, bFoundExisting)))
	{
		ExistingGoal->TerritoryPatrol = GetTerritoryPatrolRoute();
		TerritoryPatrolGoal = ExistingGoal;
		return true;
	}

	UTerritoryPatrolGoal* NewGoal = NewObject<UTerritoryPatrolGoal>(ActivityComponent, PatrolGoalClass);
	if (!NewGoal)
	{
		return false;
	}

	NewGoal->GoalKey = GoalKey;
	NewGoal->TerritoryPatrol = GetTerritoryPatrolRoute();
	NewGoal->bSaveGoal = false;
	NewGoal->bRemoveOnSucceeded = false;
	TerritoryPatrolGoal = Cast<UTerritoryPatrolGoal>(ActivityComponent->AddGoal(NewGoal, true));
	return IsValid(TerritoryPatrolGoal);
}
