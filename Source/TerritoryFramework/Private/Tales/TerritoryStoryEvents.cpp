#include "Tales/TerritoryStoryEvents.h"

#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryMutationTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "UnrealFramework/NarrativePlayerState.h"

namespace
{
	ANarrativePlayerState* ResolveExplicitNarrativePlayerState(
		APawn* Target, APlayerController* Controller)
	{
		if (Target)
		{
			if (ANarrativePlayerState* PlayerState =
				Target->GetPlayerState<ANarrativePlayerState>())
			{
				return PlayerState;
			}
		}
		return Controller
			? Controller->GetPlayerState<ANarrativePlayerState>() : nullptr;
	}

	AActor* ResolveExplicitRequester(APawn* Target, APlayerController* Controller)
	{
		if (Target) return Target;
		return Controller ? static_cast<AActor*>(Controller->GetPawn().Get()) : nullptr;
	}

	ATerritoryVolume* ResolveTerritory(UWorld* World, const FGameplayTag& TerritoryTag)
	{
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
		const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
		if (Settings && Settings->ShouldDebugTales()
			&& Settings->IsDebugLevelEnabled(6))
		{
			UE_LOG(LogTerritory, Log, TEXT("[TalesEvent] %s skipped because condition '%s' failed"),
				*GetNameSafe(Event), *FailedCondition);
		}
		return false;
	}

	void CollectLoadedHierarchy(UTerritoryRegistrySubsystem* Registry,
		ATerritoryVolume* Root, TArray<ATerritoryVolume*>& OutTerritories)
	{
		if (!Registry || !Root || OutTerritories.Contains(Root)) return;
		OutTerritories.Add(Root);
		TArray<ATerritoryVolume*> Children =
			Registry->GetChildTerritories(Root->GetTerritoryTag());
		Children.Sort([](const ATerritoryVolume& A, const ATerritoryVolume& B)
		{
			return A.GetTerritoryTag().ToString() < B.GetTerritoryTag().ToString();
		});
		for (ATerritoryVolume* Child : Children)
		{
			CollectLoadedHierarchy(Registry, Child, OutTerritories);
		}
	}
}

UTerritorySetNarrativePlayerFactionsEvent::
	UTerritorySetNarrativePlayerFactionsEvent(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

bool UTerritorySetNarrativePlayerFactionsEvent::ApplyToPlayerState(
	ANarrativePlayerState* PlayerState) const
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority()
		|| !NewFactions.IsValid())
	{
		return false;
	}

	if (bReplaceExistingFactions)
	{
		PlayerState->SetFactions(NewFactions);
	}
	else
	{
		TArray<FGameplayTag> Factions;
		NewFactions.GetGameplayTagArray(Factions);
		for (const FGameplayTag& Faction : Factions)
		{
			PlayerState->AddFaction(Faction);
		}
	}
	PlayerState->ForceNetUpdate();
	return true;
}

void UTerritorySetNarrativePlayerFactionsEvent::ExecuteEvent_Implementation(
	APawn* Target, APlayerController* Controller,
	UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client) return;

	ANarrativePlayerState* PlayerState =
		ResolveExplicitNarrativePlayerState(Target, Controller);
	if (!ApplyToPlayerState(PlayerState))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[SetNarrativePlayerFactions] Requires an explicit authoritative Narrative player and at least one valid Narrative faction"));
	}
}

FString UTerritorySetNarrativePlayerFactionsEvent::
	GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Player factions: %s %s"),
		bReplaceExistingFactions ? TEXT("replace with") : TEXT("add"),
		*NewFactions.ToStringSimple());
}

UTerritoryHierarchyStoryOverrideEvent::UTerritoryHierarchyStoryOverrideEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryHierarchyStoryOverrideEvent::ExecuteEvent_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client || !RootTerritory.IsValid()) return;

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	UTerritoryControlSubsystem* Control =
		World->GetSubsystem<UTerritoryControlSubsystem>();
	ATerritoryVolume* Root = Registry
		? Registry->GetTerritoryByTag(RootTerritory) : nullptr;
	if (!Registry || !Control || !Root)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[HierarchyStoryOverride] Root %s is not loaded and registered"),
			*RootTerritory.ToString());
		return;
	}
	if (Operation == ETerritoryHierarchyStoryOperation::ClaimForFaction
		&& !ClaimingFaction.IsValid())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[HierarchyStoryOverride] Claim operation for %s requires an exact Narrative faction"),
			*RootTerritory.ToString());
		return;
	}

	FTerritoryTransitionContext Context;
	Context.Instigator = Target;
	Context.TargetPawn = Target;
	Context.PlayerController = Controller;
	Context.TalesComponent = NarrativeComponent;
	Context.RequestingFaction = ClaimingFaction;

	TArray<ATerritoryVolume*> Hierarchy;
	CollectLoadedHierarchy(Registry, Root, Hierarchy);
	int32 ChangedCount = 0;
	int32 RejectedCount = 0;
	for (ATerritoryVolume* Territory : Hierarchy)
	{
		if (!Territory) continue;
		if (Operation == ETerritoryHierarchyStoryOperation::Lock)
		{
			ChangedCount += Territory->LockTerritoryWithContext(LockReason, Context) ? 1 : 0;
			continue;
		}
		if (Operation == ETerritoryHierarchyStoryOperation::Unlock)
		{
			ChangedCount += Territory->TryUnlockWithContext(
				Context, bForceStoryOverride) ? 1 : 0;
			continue;
		}

		// Aggregate parents are intentionally never directly assigned. Their existing
		// child unanimity reducer will commit them when the final leaf changes.
		if (Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly) continue;

		FTerritoryMutationRequest Request;
		Request.Territory = Territory;
		Request.NewOwner = Operation == ETerritoryHierarchyStoryOperation::ClaimForFaction
			? ClaimingFaction : FGameplayTag();
		Request.DesiredState = Operation == ETerritoryHierarchyStoryOperation::ClaimForFaction
			? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Request.TransitionContext = Context;
		Request.bBypassConditions = bForceStoryOverride;
		Request.bBypassDiplomacy = bForceStoryOverride;
		Request.bBypassLock = bForceStoryOverride;
		Request.bBypassDefenders = bForceStoryOverride;
		const FTerritoryMutationResponse Response = Control->ApplyTerritoryMutation(Request);
		if (Response.Result == ETerritoryMutationResult::Success
			|| (Territory->GetOwningFaction() == Request.NewOwner
				&& Territory->GetTerritoryState() == Request.DesiredState))
		{
			++ChangedCount;
		}
		else
		{
			++RejectedCount;
			UE_LOG(LogTerritory, Warning,
				TEXT("[HierarchyStoryOverride] %s rejected: %s"),
				*Territory->GetTerritoryTag().ToString(),
				*Response.Explanation.ToString());
		}
	}

	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugTales())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[HierarchyStoryOverride] %s applied to %d loaded hierarchy actors (%d rejected). World Partition descendants must be loaded when this one-shot event runs."),
			*RootTerritory.ToString(), ChangedCount, RejectedCount);
	}
}

FString UTerritoryHierarchyStoryOverrideEvent::GetGraphDisplayText_Implementation()
{
	const UEnum* OperationEnum = StaticEnum<ETerritoryHierarchyStoryOperation>();
	const FString OperationText = OperationEnum
		? OperationEnum->GetDisplayNameTextByValue(static_cast<int64>(Operation)).ToString()
		: TEXT("Hierarchy Story Override");
	return FString::Printf(TEXT("%s: %s"), *OperationText, *RootTerritory.ToString());
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
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client) return;
	ATerritoryVolume* Territory = ResolveTerritory(World, TargetTerritory);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Territory || !Counter)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[EnemyWaveEvent] Target Territory '%s' is not loaded or registered"),
			*TargetTerritory.ToString());
		return;
	}
	FText FailureReason;
	bool bScheduled = false;
	if (LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit)
	{
		FTerritoryStoryPursuitOptions LegacyOptions;
		LegacyOptions.bAllowsTerritoryCapture = true;
		LegacyOptions.bUseStrategicDecisionRoll = true;
		LegacyOptions.GracePeriodOverrideGameTime = -1.f;
		bScheduled = Counter->TryScheduleAssaultAdvancedWithReason(Territory,
			AttackingFaction, LaunchMode, LegacyOptions,
			bStartImmediately, FailureReason);
	}
	else if (bChooseBestEligibleAttacker)
	{
		FGameplayTag ChosenFaction;
		FTerritoryAssaultEvaluationInput PreviewInput;
		FTerritoryAssaultEvaluationResult PreviewResult;
		if (Counter->GetBestAuthoredWaveAttackerPreview(Territory,
			AttackingFaction, ChosenFaction, PreviewInput, PreviewResult,
			FailureReason))
		{
			FTerritoryStoryPursuitOptions IgnoredStoryOptions;
			bScheduled = Counter->TryScheduleAssaultAdvancedWithReason(
				Territory, ChosenFaction, LaunchMode, IgnoredStoryOptions,
				bStartImmediately, FailureReason);
		}
	}
	else
	{
		FTerritoryStoryPursuitOptions IgnoredStoryOptions;
		bScheduled = Counter->TryScheduleAssaultAdvancedWithReason(Territory,
			AttackingFaction, LaunchMode, IgnoredStoryOptions,
			bStartImmediately, FailureReason);
	}
	if (!bScheduled)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[EnemyWaveEvent] No Territory assault was scheduled for %s: %s"),
			*TargetTerritory.ToString(), *FailureReason.ToString());
	}
}

FString UTerritoryScheduleEnemyWaveEvent::GetGraphDisplayText_Implementation()
{
	const bool bStoryPursuit = LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit;
	return FString::Printf(TEXT("Enemy wave (%s%s): schedule %s against %s"),
		bStoryPursuit ? TEXT("one story pursuit") : TEXT("profile-governed strategic response"),
		bStartImmediately ? TEXT(", immediate deployment") : TEXT(""),
		!bStoryPursuit && bChooseBestEligibleAttacker
			? TEXT("best eligible faction") : *AttackingFaction.ToString(),
		*TargetTerritory.ToString());
}

UTerritoryStartBossChaseEvent::UTerritoryStartBossChaseEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
	PursuitOptions.PlannedForceOverride = 1;
	PursuitOptions.WaveSizeOverride = 1;
	PursuitOptions.bAllowsTerritoryCapture = false;
	PursuitOptions.bUseStrategicDecisionRoll = false;
	PursuitOptions.GracePeriodOverrideGameTime = 0.f;
}

void UTerritoryStartBossChaseEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!CanRunTerritoryEvent(this, Target, Controller, NarrativeComponent)) return;
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client || !PursuingFaction.IsValid()) return;
	ATerritoryVolume* Territory = ResolveTerritory(World, TargetTerritory);
	UTerritoryCounterAttackSubsystem* Counter =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	FTerritoryStoryPursuitOptions RuntimeOptions = PursuitOptions;
	AActor* StoryTarget = ResolveExplicitRequester(Target, Controller);
	if (RuntimeOptions.StoryFocusLocation.IsNearlyZero() && StoryTarget)
	{
		RuntimeOptions.StoryFocusLocation = StoryTarget->GetActorLocation();
	}
	FText FailureReason;
	if (!Territory)
	{
		FailureReason = FText::Format(NSLOCTEXT("TerritoryStoryEvent",
			"BossTargetUnavailable", "Target Territory {0} is not loaded or registered."),
			FText::FromName(TargetTerritory.GetTagName()));
	}
	else if (!Counter)
	{
		FailureReason = NSLOCTEXT("TerritoryStoryEvent",
			"BossCounterSubsystemUnavailable", "The Territory Counterattack subsystem is unavailable.");
	}
	const bool bScheduled = Territory && Counter
		&& Counter->TryScheduleAssaultWithReason(Territory, PursuingFaction,
			ETerritoryAssaultLaunchMode::StoryPursuit, RuntimeOptions,
			FailureReason);
	if (!bScheduled)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[BossChaseEvent] Could not start %s against %s: %s"),
			*PursuingFaction.ToString(), *TargetTerritory.ToString(),
			*FailureReason.ToString());
	}
}

FString UTerritoryStartBossChaseEvent::GetGraphDisplayText_Implementation()
{
	const bool bEnemyEscapes = PursuitOptions.Direction ==
		ETerritoryStoryPursuitDirection::PlayerChasesEnemy;
	return FString::Printf(TEXT("Boss chase: %s %s at %s%s"),
		*PursuingFaction.ToString(),
		bEnemyEscapes ? TEXT("escapes while player pursues") : TEXT("drives in and pursues player"),
		*TargetTerritory.ToString(),
		PursuitOptions.bAllowsTerritoryCapture ? TEXT(" (capture enabled)") : TEXT(" (story only)"));
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
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client || !TargetTerritory.IsValid()) return;
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Counter) return;
	const ATerritoryVolume* LoadedTerritory = ResolveTerritory(World, TargetTerritory);
	const TArray<FTerritoryAssaultRecord> MatchingAssaults = LoadedTerritory
		? Counter->GetAssaultsForTerritoryActor(LoadedTerritory)
		: Counter->GetAssaultsForTerritory(TargetTerritory);
	int32 CancelledCount = 0;
	for (const FTerritoryAssaultRecord& Assault : MatchingAssaults)
	{
		if (Assault.IsTerminal()) continue;
		if (AttackingFaction.IsValid() && Assault.AttackingFaction != AttackingFaction) continue;
		if (!bIncludePhysicallyActiveAssaults
			&& (Assault.State == ETerritoryAssaultState::Active
				|| Assault.State == ETerritoryAssaultState::RecaptureCountdown)) continue;
		if (Counter->CancelAssault(Assault.AssaultID, ETerritoryAssaultResolution::ManuallyCancelled))
		{
			++CancelledCount;
		}
	}
	if (CancelledCount == 0)
	{
		if (const UTerritoryDeveloperSettings* Settings =
			GetDefault<UTerritoryDeveloperSettings>();
			Settings && Settings->ShouldDebugTales()
			&& Settings->IsDebugLevelEnabled(6))
		{
			UE_LOG(LogTerritory, Log, TEXT("[CancelEnemyWaveEvent] No matching non-terminal assault for %s"),
				*TargetTerritory.ToString());
		}
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
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client) return;
	ATerritoryVolume* Territory = ResolveTerritory(World, TargetTerritory);
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
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client) return;
	ATerritoryProperty* Property = Cast<ATerritoryProperty>(
		ResolveTerritory(World, TargetProperty));
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
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
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
