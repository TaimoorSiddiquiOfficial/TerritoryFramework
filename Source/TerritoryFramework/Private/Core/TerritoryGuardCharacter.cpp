#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "AI/TerritoryPatrolGoal.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Tales/TriggerSet.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATerritoryGuardCharacter::ATerritoryGuardCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PatrolGoalClass = UTerritoryPatrolGoal::StaticClass();
}

bool ATerritoryGuardCharacter::ShouldRespawn_Implementation() const
{
	return false;
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
	}
}

void ATerritoryGuardCharacter::ApplyActivityConfig_Implementation(UNPCActivityConfiguration* NPCActivityConfig)
{
	Super::ApplyActivityConfig_Implementation(NPCActivityConfig);
	InitializeTerritoryPatrolGoal();
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

	// Apply the definition — Narrative reads SpawnParams during this call
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
