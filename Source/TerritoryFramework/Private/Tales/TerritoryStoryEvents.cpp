#include "Tales/TerritoryStoryEvents.h"

#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

namespace
{
	AActor* ResolveExplicitRequester(APawn* Target, APlayerController* Controller)
	{
		if (Target) return Target;
		return Controller ? static_cast<AActor*>(Controller->GetPawn().Get()) : nullptr;
	}

	ATerritoryVolume* ResolveTerritory(UObject* Context, const FGameplayTag& TerritoryTag)
	{
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		UTerritoryRegistrySubsystem* Registry = World
			? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
		return Registry && TerritoryTag.IsValid()
			? Registry->GetTerritoryByTag(TerritoryTag) : nullptr;
	}

	bool CanRunTerritoryEvent(UNarrativeEvent* Event, APawn* Target,
		APlayerController* Controller, UTalesComponent* NarrativeComponent)
	{
		FString FailedCondition;
		if (TerritoryTales::DoEventConditionsPass(Event, Target, Controller,
			NarrativeComponent, &FailedCondition))
		{
			return true;
		}
		UE_LOG(LogTerritory, Verbose, TEXT("[TalesEvent] %s skipped because condition '%s' failed"),
			*GetNameSafe(Event), *FailedCondition);
		return false;
	}
}

UTerritoryScheduleEnemyWaveEvent::UTerritoryScheduleEnemyWaveEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryScheduleEnemyWaveEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	ATerritoryVolume* Territory = ResolveTerritory(this, TargetTerritory);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Territory || !Counter)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[EnemyWaveEvent] Target Territory '%s' is not loaded or registered"),
			*TargetTerritory.ToString());
		return;
	}
	const bool bScheduled = LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		? (AttackingFaction.IsValid()
			&& Counter->ScheduleStoryPursuit(Territory, AttackingFaction))
		: (bChooseBestEligibleAttacker
			? Counter->ScheduleBestCounterAttack(Territory, AttackingFaction)
			: (AttackingFaction.IsValid()
				&& Counter->ScheduleCounterAttack(Territory, AttackingFaction)));
	if (!bScheduled)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[EnemyWaveEvent] No finite wave was scheduled for %s; check owner, diplomacy, profile, route, cooldown, and budgets"),
			*TargetTerritory.ToString());
	}
}

FString UTerritoryScheduleEnemyWaveEvent::GetGraphDisplayText_Implementation()
{
	const bool bStoryPursuit = LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit;
	return FString::Printf(TEXT("Enemy wave (%s): schedule %s against %s"),
		bStoryPursuit ? TEXT("story pursuit") : TEXT("strategic counter"),
		!bStoryPursuit && bChooseBestEligibleAttacker
			? TEXT("best eligible faction") : *AttackingFaction.ToString(),
		*TargetTerritory.ToString());
}

UTerritoryCancelEnemyWavesEvent::UTerritoryCancelEnemyWavesEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryCancelEnemyWavesEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !TargetTerritory.IsValid()) return;
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Counter) return;
	const ATerritoryVolume* LoadedTerritory = ResolveTerritory(this, TargetTerritory);
	const TArray<FTerritoryAssaultRecord> MatchingAssaults = LoadedTerritory
		? Counter->GetAssaultsForTerritoryActor(LoadedTerritory)
		: Counter->GetAssaultsForTerritory(TargetTerritory);
	int32 CancelledCount = 0;
	for (const FTerritoryAssaultRecord& Assault : MatchingAssaults)
	{
		if (Assault.IsTerminal()) continue;
		if (AttackingFaction.IsValid() && Assault.AttackingFaction != AttackingFaction) continue;
		if (!bIncludePhysicallyActiveAssaults && Assault.State == ETerritoryAssaultState::Active) continue;
		if (Counter->CancelAssault(Assault.AssaultID, ETerritoryAssaultResolution::ManuallyCancelled))
		{
			++CancelledCount;
		}
	}
	if (CancelledCount == 0)
	{
		UE_LOG(LogTerritory, Verbose, TEXT("[CancelEnemyWaveEvent] No matching non-terminal assault for %s"),
			*TargetTerritory.ToString());
	}
}

FString UTerritoryCancelEnemyWavesEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Enemy wave: cancel %s%s"), *TargetTerritory.ToString(),
		bIncludePhysicallyActiveAssaults ? TEXT(" including active attackers") : TEXT(" before physical activation"));
}

UTerritorySetGarrisonTargetEvent::UTerritorySetGarrisonTargetEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritorySetGarrisonTargetEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	ATerritoryVolume* Territory = ResolveTerritory(this, TargetTerritory);
	AActor* Requester = ResolveExplicitRequester(Target, Controller);
	if (!Territory || !Requester)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[GarrisonTargetEvent] %s requires a loaded Territory and explicit Narrative target/controller pawn"),
			*TargetTerritory.ToString());
		return;
	}
	const FTerritoryGarrisonMutationResult Result = Territory->TrySetDesiredGuardCount(
		Requester, FMath::Max(0, DesiredGuards));
	if (!Result.bSuccess)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[GarrisonTargetEvent] %s rejected: %s"),
			*TargetTerritory.ToString(), *Result.Message.ToString());
	}
}

FString UTerritorySetGarrisonTargetEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Garrison: set %s target to %d"),
		*TargetTerritory.ToString(), FMath::Max(0, DesiredGuards));
}

UTerritoryUpgradePropertyEvent::UTerritoryUpgradePropertyEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryUpgradePropertyEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	ATerritoryProperty* Property = Cast<ATerritoryProperty>(ResolveTerritory(this, TargetProperty));
	AActor* Requester = ResolveExplicitRequester(Target, Controller);
	if (!Property || !Requester || !Property->TryUpgrade(Requester))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[UpgradePropertyEvent] %s rejected; it must be a loaded owned Property and the explicit requester must be eligible and able to pay"),
			*TargetProperty.ToString());
	}
}

FString UTerritoryUpgradePropertyEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Property: purchase one upgrade for %s"), *TargetProperty.ToString());
}

UTerritoryExecuteResourceRecipeEvent::UTerritoryExecuteResourceRecipeEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryExecuteResourceRecipeEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	AActor* Requester = ResolveExplicitRequester(Target, Controller);
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	if (!Requester || !Economy)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[ResourceRecipeEvent] An explicit authoritative Narrative inventory target is required"));
		return;
	}
	FTerritoryProductionResult Result;
	if (!Economy->ExecuteResourceRecipe(Requester, Faction, Recipe,
		FMath::Max(0, UpgradeLevel), FMath::Max(1, BatchCount), SourceTerritory, Result))
	{
		UE_LOG(LogTerritory, Warning, TEXT("[ResourceRecipeEvent] Recipe %s rejected: %s"),
			*Recipe.RuleTag.ToString(), *Result.FailureReason.ToString());
	}
}

FString UTerritoryExecuteResourceRecipeEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Resources: execute %s x%d for %s"),
		*Recipe.RuleTag.ToString(), FMath::Max(1, BatchCount), *Faction.ToString());
}
