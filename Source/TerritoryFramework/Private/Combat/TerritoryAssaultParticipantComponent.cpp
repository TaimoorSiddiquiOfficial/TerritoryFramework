#include "Combat/TerritoryAssaultParticipantComponent.h"

#include "AI/TerritoryAssaultActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/NarrativeNPCController.h"
#include "Interaction/NPCInteractionComponent.h"
#include "Vehicles/MountComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "AbilitySystemInterface.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

UTerritoryAssaultParticipantComponent::UTerritoryAssaultParticipantComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UTerritoryAssaultParticipantComponent::Configure(
	const FGuid& InAssaultID, const FGuid& InTargetTerritoryGUID,
	const FGameplayTag& InTargetTerritory, const FGameplayTag& InAttackingFaction)
{
	AssaultID = InAssaultID;
	TargetTerritoryGUID = InTargetTerritoryGUID;
	TargetTerritory = InTargetTerritory;
	AttackingFaction = InAttackingFaction;
}

void UTerritoryAssaultParticipantComponent::ConfigureAssaultRules(
	const bool bInAllowsTerritoryCapture,
	const bool bInPrioritizeTerritoryTakeover,
	const float InDefendingPlayerEngagementPadding,
	const FGameplayTag InTakeoverStartedDialogueTag,
	const FGameplayTag InFinalFightDialogueTag)
{
	bAllowsTerritoryCapture = bInAllowsTerritoryCapture;
	bPrioritizeTerritoryTakeover = bInPrioritizeTerritoryTakeover;
	DefendingPlayerEngagementPadding = FMath::Max(0.f,
		InDefendingPlayerEngagementPadding);
	TakeoverStartedDialogueTag = InTakeoverStartedDialogueTag;
	FinalFightDialogueTag = InFinalFightDialogueTag;
}

void UTerritoryAssaultParticipantComponent::ConfigureNarrativeVehicleIngress(
	ANarrativeVehicleBase* InVehicle, const TArray<FVector>& InRoutePoints,
	const FTransform& InParkDestination, const FTransform& InWalkDestination,
	const float InMaximumDriveSpeed, const float InTimeoutSeconds,
	const bool bInEscapeOnArrival,
	const FTerritoryVehicleAwarenessSettings& InAwarenessSettings,
	const float InMaximumChaseDistance,
	const float InChaseDistanceGraceSeconds,
	const bool bInAbandonDamagedVehicleForFinalFight,
	const float InVehicleAbandonHealthFraction)
{
	NarrativeIngressVehicle = InVehicle;
	VehicleRoutePoints = InRoutePoints;
	VehicleParkDestination = InParkDestination;
	VehicleWalkDestination = InWalkDestination;
	VehicleMaximumDriveSpeed = FMath::Max(0.f, InMaximumDriveSpeed);
	VehicleIngressTimeoutSeconds = FMath::Clamp(InTimeoutSeconds, 5.f, 600.f);
	bVehicleIngressRequired = true;
	bVehicleIngressComplete = false;
	bVehicleIngressFailed = !IsValid(InVehicle) || VehicleRoutePoints.IsEmpty();
	bVehicleMountRequested = false;
	bVehiclePossessionConfirmed = false;
	bVehicleDriveActive = false;
	bVehicleDismountRequested = false;
	VehicleRoutePointIndex = 0;
	bEscapeOnVehicleArrival = bInEscapeOnArrival;
	bEscapeCompletionReported = false;
	VehicleAwareness = InAwarenessSettings;
	MaximumChaseDistance = FMath::Max(0.f, InMaximumChaseDistance);
	ChaseDistanceGraceSeconds = FMath::Max(0.f, InChaseDistanceGraceSeconds);
	SecondsOutsideChaseRange = 0.f;
	VehicleAbandonHealthFraction = FMath::Clamp(
		InVehicleAbandonHealthFraction, 0.01f, 0.95f);
	VehicleBlockedSeconds = 0.f;
	bAbandonDamagedVehicleForFinalFight =
		bInAbandonDamagedVehicleForFinalFight;
	bVehicleAbandonmentRequested = false;
	bUseVehicleWalkDestination = !InWalkDestination.Equals(FTransform::Identity);
	VehicleIngressDeadline = GetWorld()
		? GetWorld()->GetTimeSeconds() + VehicleIngressTimeoutSeconds : 0.0;
	SetComponentTickEnabled(false);
}

void UTerritoryAssaultParticipantComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ParticipationTimer, this,
			&UTerritoryAssaultParticipantComponent::UpdateParticipation, 0.5f, true, 0.1f);
	}
}

void UTerritoryAssaultParticipantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopVehicleInputs();
	RestoreNarrativeDefenderTargeting(false);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParticipationTimer);
	}
	if (UNarrativeAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->OnDeathStateChanged.RemoveDynamic(
			this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
	}
	if (GetOwner() && GetOwner()->HasAuthority() && !bRemovalReported
		&& EndPlayReason != EEndPlayReason::EndPlayInEditor)
	{
		Retire(false);
	}
	Super::EndPlay(EndPlayReason);
}

void UTerritoryAssaultParticipantComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority() && bVehicleDriveActive)
	{
		UpdateVehicleDriving(DeltaTime);
	}
}

void UTerritoryAssaultParticipantComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, AssaultID);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, TargetTerritoryGUID);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, TargetTerritory);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, AttackingFaction);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, bCaptureRegistered);
	DOREPLIFETIME(UTerritoryAssaultParticipantComponent, bAllowsTerritoryCapture);
}

bool UTerritoryAssaultParticipantComponent::EnsureNarrativeVehicleIngress()
{
	if (!IsVehicleIngressPending()) return true;
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	if (!NPC || !NPC->IsNarrativeSpawnReady()) return false;
	if (!BindNarrativeDeathAfterSpawnReady()) return false;
	ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	ANarrativeNPCController* Controller = NPC->GetNPCController();
	if (!IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed() || !Controller
		|| VehicleRoutePoints.IsEmpty())
	{
		bVehicleIngressFailed = true;
		return false;
	}

	UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
	UNPCInteractionComponent* Interaction = Controller->GetInteractionComponent();
	if (!Mount || !Interaction)
	{
		bVehicleIngressFailed = true;
		return false;
	}

	if (bVehicleDismountRequested)
	{
		if (Vehicle->GetController() != Controller)
		{
			CompleteVehicleIngress();
		}
		return true;
	}

	// Narrative remains authoritative for the seat, ability, animation and possession.
	// Territory only supplies road steering after that authored interaction completes.
	if (!bVehicleMountRequested)
	{
		if (!Interaction->TargetBestInteractionSlot(Mount, false)
			|| !Interaction->RunInteractBehavior(false))
		{
			UE_LOG(LogTerritory, Error,
				TEXT("[CounterAttack] Narrative mount interaction rejected driver %s for vehicle %s"),
				*GetNameSafe(NPC), *GetNameSafe(Vehicle));
			bVehicleIngressFailed = true;
			return false;
		}
		bVehicleMountRequested = true;
		UE_LOG(LogTerritory, Display,
			TEXT("[CounterAttack] Narrative driver %s claimed a mount seat in %s"),
			*GetNameSafe(NPC), *GetNameSafe(Vehicle));
		return true;
	}

	if (Vehicle->GetController() != Controller)
	{
		return true;
	}

	if (!bVehiclePossessionConfirmed)
	{
		bVehiclePossessionConfirmed = true;
		UE_LOG(LogTerritory, Display,
			TEXT("[CounterAttack] Narrative driver %s now controls %s"),
			*GetNameSafe(NPC), *GetNameSafe(Vehicle));
		bVehicleDriveActive = true;
		SetComponentTickEnabled(true);
		UE_LOG(LogTerritory, Display,
			TEXT("[CounterAttack] Narrative driver %s started Territory road pursuit for %s (%d ZoneGraph waypoints)"),
			*GetNameSafe(NPC), *GetNameSafe(Vehicle), VehicleRoutePoints.Num());
	}
	return true;
}

bool UTerritoryAssaultParticipantComponent::EnsureNarrativeActivityAndGoal()
{
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	// Narrative applies the definition and appearance asynchronously. Do not start
	// goal selection while that authoritative initialization is incomplete. Equipment
	// visuals are intentionally not a movement prerequisite: unarmed NPC definitions
	// are valid, and a missing optional weapon visual must not deadlock the assault.
	if (!NPC || !NPC->IsNarrativeSpawnReady())
	{
		return false;
	}
	// Narrative binds the character/controller death handlers while applying the
	// definition. Bind Territory casualty accounting only after that contract is
	// ready so ragdoll/death presentation runs before assault resolution.
	if (!BindNarrativeDeathAfterSpawnReady())
	{
		return false;
	}
	if (AssaultGoal) return true;
	UNPCActivityComponent* ActivityComponent = NPC ? NPC->GetActivityComponent() : nullptr;
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	if (!ActivityComponent || !Controller)
	{
		return false;
	}

	UNPCActivity* Activity = ActivityComponent->GetActivity(UTerritoryAssaultActivity::StaticClass());
	if (!Activity)
	{
		Activity = ActivityComponent->AddActivity(UTerritoryAssaultActivity::StaticClass(), false);
	}
	if (!Activity) return false;

	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory) return false;

	UTerritoryAssaultGoal* NewGoal = NewObject<UTerritoryAssaultGoal>(ActivityComponent);
	NewGoal->GoalKey = this;
	NewGoal->AssaultID = AssaultID;
	NewGoal->TargetTerritoryGUID = TargetTerritoryGUID;
	NewGoal->TargetTerritoryTag = TargetTerritory;
	NewGoal->TargetTerritory = Territory;
	const TArray<FVector> InitialObjectives =
		TerritoryAssaultTargetPolicy::BuildObjectiveLocations(Territory, false);
	NewGoal->TargetLocation = InitialObjectives.IsEmpty()
		? Territory->GetTerritoryBounds().GetCenter() : InitialObjectives[0];
	NewGoal->bSaveGoal = false;
	NewGoal->bRemoveOnSucceeded = false;
	AssaultGoal = Cast<UTerritoryAssaultGoal>(ActivityComponent->AddGoal(NewGoal, true));
	return AssaultGoal != nullptr;
}

bool UTerritoryAssaultParticipantComponent::BindNarrativeDeathAfterSpawnReady()
{
	IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner());
	UNarrativeAbilitySystemComponent* ASC = AbilityOwner
		? Cast<UNarrativeAbilitySystemComponent>(AbilityOwner->GetAbilitySystemComponent())
		: nullptr;
	if (!ASC) return false;

	if (BoundASC.Get() != ASC)
	{
		if (UNarrativeAbilitySystemComponent* Previous = BoundASC.Get())
		{
			Previous->OnDeathStateChanged.RemoveDynamic(
				this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
		}
		BoundASC = ASC;
		ASC->OnDeathStateChanged.AddUniqueDynamic(
			this, &UTerritoryAssaultParticipantComponent::HandleOwnerDied);
	}

	if (ASC->IsDead())
	{
		HandleOwnerDied(GetOwner(), ASC, true);
		return false;
	}
	return true;
}

void UTerritoryAssaultParticipantComponent::UpdateParticipation()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World || bRemovalReported) return;

	UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Counterattacks || !Counterattacks->IsAssaultActive(AssaultID))
	{
		Retire(false);
		if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
		}
		Owner->Destroy();
		return;
	}

	if (IsVehicleIngressPending())
	{
		const bool bTimedOut = VehicleIngressDeadline > 0.0
			&& World->GetTimeSeconds() >= VehicleIngressDeadline;
		if (bVehicleIngressFailed || bTimedOut)
		{
			WithdrawForVehicleIngressFailure(bTimedOut
				? TEXT("Narrative vehicle ingress timed out")
				: TEXT("Narrative vehicle or ZoneGraph drive state became invalid"));
			return;
		}
		if (!EnsureNarrativeVehicleIngress())
		{
			if (bVehicleIngressFailed || ++GoalInitializationAttempts >= 40)
			{
				WithdrawForVehicleIngressFailure(
					TEXT("Narrative vehicle ingress could not initialize"));
			}
			return;
		}
		GoalInitializationAttempts = 0;
		return;
	}

	if (bEscapeOnVehicleArrival && bVehicleIngressComplete
		&& !bEscapeCompletionReported)
	{
		bEscapeCompletionReported = true;
		Counterattacks->NotifyVehicleStoryTargetEscaped(
			AssaultID, Cast<ATerritoryAssaultCharacter>(Owner));
		return;
	}

	if (!EnsureNarrativeActivityAndGoal())
	{
		if (++GoalInitializationAttempts >= 40)
		{
			UE_LOG(LogTerritory, Error,
				TEXT("Assault participant %s could not initialize its Narrative goal/activity"),
				*GetNameSafe(Owner));
			Retire(false);
			if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
			{
				TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
			}
			Owner->Destroy();
		}
		return;
	}
	GoalInitializationAttempts = 0;

	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory)
	{
		// The old weak-keyed capture state is pruned by the control subsystem when
		// a World Partition actor unloads. Clear our local read model so this NPC
		// can register against the replacement actor when it streams back in.
		if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			if (ANarrativeNPCController* Controller = NPC->GetNPCController())
			{
				if (UTerritoryCombatDirector* Director =
					World->GetSubsystem<UTerritoryCombatDirector>())
				{
					Director->ReleaseAllSlots(Controller);
				}
			}
		}
		RestoreNarrativeDefenderTargeting(true);
		bCaptureRegistered = false;
		return;
	}

	const bool bInsideTarget = Territory->ContainsPoint(Owner->GetActorLocation());
	// Keep closing on a registered hostile defender after crossing the boundary.
	// Narrative combat remains authoritative; the Territory adapter only prevents a
	// non-defender combat goal from outranking a still-live registered defender.
	if (!MaintainAssaultMovement(Territory)) return;
	if (!bAllowsTerritoryCapture)
	{
		UnregisterCapturePressure();
		return;
	}
	if (!bInsideTarget)
	{
		UnregisterCapturePressure();
		return;
	}
	if (bCaptureRegistered) return;

	ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner);
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	UTerritoryCombatDirector* Director = World->GetSubsystem<UTerritoryCombatDirector>();
	UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>();
	if (!Controller || !Director || !Control
		|| !Director->RequestAssaultSlot(Territory, Controller))
	{
		return;
	}

	// A counterattack must physically enter, fight, and hold the Territory, but the
	// ordinary capture-meter tick must not decide strategic recapture. The counter
	// subsystem owns the explicit player-present/death/countdown handover policy.
	bCaptureRegistered = Control->TryRegisterContester(Territory, Owner, AttackingFaction);
	if (!bCaptureRegistered)
	{
		Director->ReleaseAssaultSlot(Territory, Controller);
	}
	else if (!bTakeoverStartedDialoguePlayed)
	{
		bTakeoverStartedDialoguePlayed = true;
		PlayMissionDialogue(TakeoverStartedDialogueTag);
	}
}

bool UTerritoryAssaultParticipantComponent::MaintainAssaultMovement(
	ATerritoryVolume* Territory)
{
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	UNPCActivityComponent* ActivityComponent = NPC ? NPC->GetActivityComponent() : nullptr;
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	if (!NPC || !Territory || !ActivityComponent || !Controller || !AssaultGoal)
	{
		return true;
	}

	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const float RetryInterval = FMath::Clamp(
		Profile ? Profile->StalledMovementRetryInterval : 1.5f, 0.25f, 10.f);
	const int32 MaximumFailures = FMath::Clamp(
		Profile ? Profile->MaxStalledMovementRetries : 8, 1, 100);
	const bool bUseNavigation = !Profile || Profile->bUseNavigationAwareObjectives;
	const bool bDistribute = !Profile || Profile->bDistributeParticipantsAcrossObjectives;
	const int32 StableObjectiveSlot = static_cast<int32>(
		GetTypeHash(NPC->GetActorGUID_Implementation()));
	FVector DesiredTarget = Territory->GetTerritoryBounds().GetCenter();
	const TArray<FVector> StaticObjectives =
		TerritoryAssaultTargetPolicy::BuildObjectiveLocations(Territory, false);
	TerritoryAssaultTargetPolicy::SelectObjectiveLocation(
		NPC, StaticObjectives, StableObjectiveSlot, bUseNavigation, bDistribute,
		DesiredTarget);
	if (bUseVehicleWalkDestination)
	{
		const FVector RoadMissionDestination = VehicleWalkDestination.GetLocation();
		if (FVector::DistSquared2D(NPC->GetActorLocation(), RoadMissionDestination)
			<= FMath::Square(250.f))
		{
			bUseVehicleWalkDestination = false;
		}
		else
		{
			DesiredTarget = RoadMissionDestination;
		}
	}
	TArray<AActor*> LiveHostileDefenders = bPrioritizeTerritoryTakeover
		? CollectTakeoverCombatants(Territory)
		: TerritoryAssaultTargetPolicy::CollectRegisteredDefenders(Territory);
	TArray<FVector> LiveDefenderLocations;
	for (int32 DefenderIndex = LiveHostileDefenders.Num() - 1;
		DefenderIndex >= 0; --DefenderIndex)
	{
		AActor* Defender = LiveHostileDefenders[DefenderIndex];
		if (!IsValid(Defender) || Defender == NPC || Defender->IsActorBeingDestroyed())
		{
			LiveHostileDefenders.RemoveAtSwap(DefenderIndex);
			continue;
		}
		if (IAbilitySystemInterface* AbilityDefender = Cast<IAbilitySystemInterface>(Defender))
		{
			if (UNarrativeAbilitySystemComponent* DefenderASC =
				Cast<UNarrativeAbilitySystemComponent>(AbilityDefender->GetAbilitySystemComponent()))
			{
				if (DefenderASC->IsDead())
				{
					LiveHostileDefenders.RemoveAtSwap(DefenderIndex);
					continue;
				}
			}
		}
		const INarrativeTeamAgentInterface* NarrativeTeam =
			Cast<INarrativeTeamAgentInterface>(NPC);
		if (!NarrativeTeam
			|| NarrativeTeam->GetTeamAttitudeTowards(*Defender) != ETeamAttitude::Hostile)
		{
			LiveHostileDefenders.RemoveAtSwap(DefenderIndex);
			continue;
		}
		LiveDefenderLocations.Add(Defender->GetActorLocation());
	}
	if (!LiveDefenderLocations.IsEmpty())
	{
		TerritoryAssaultTargetPolicy::SelectObjectiveLocation(
			NPC, LiveDefenderLocations, StableObjectiveSlot, bUseNavigation,
			bDistribute, DesiredTarget);
	}
	ReconcileNarrativeDefenderTargeting(ActivityComponent, LiveHostileDefenders);
	const bool bMovementTargetChanged = !AssaultGoal->TargetLocation.Equals(DesiredTarget, 200.f);
	AssaultGoal->TargetLocation = DesiredTarget;
	auto RecordFailedRestart = [this, NPC, Territory, MaximumFailures]()
	{
		if (++ConsecutiveMovementRestartFailures < MaximumFailures) return true;
		UE_LOG(LogTerritory, Error,
			TEXT("Assault participant %s withdrew after %d failed movement restarts toward %s"),
			*GetNameSafe(NPC), ConsecutiveMovementRestartFailures,
			*Territory->GetTerritoryTag().ToString());
		Retire(false);
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
		NPC->Destroy();
		return false;
	};

	UNPCGoalItem* CurrentGoal = ActivityComponent->GetCurrentActivityGoal();
	if (!CurrentGoal && Now >= NextMovementRestartTime)
	{
		NextMovementRestartTime = Now + RetryInterval;
		ActivityComponent->PerformActivitySelection(true);
		CurrentGoal = ActivityComponent->GetCurrentActivityGoal();
		if (!CurrentGoal) return RecordFailedRestart();
	}
	// A different current goal is Narrative combat or another deliberate activity.
	// Registered-defender combat remains authoritative and interrupts movement.
	if (CurrentGoal && CurrentGoal != AssaultGoal)
	{
		ConsecutiveMovementRestartFailures = 0;
		return true;
	}
	if (!CurrentGoal) return true;
	if (Controller->GetMoveStatus() != EPathFollowingStatus::Idle && !bMovementTargetChanged)
	{
		ConsecutiveMovementRestartFailures = 0;
		return true;
	}

	if (Now < NextMovementRestartTime) return true;
	NextMovementRestartTime = Now + RetryInterval;

	const EPathFollowingRequestResult::Type Result = Controller->MoveToLocation(
		AssaultGoal->TargetLocation, 150.f, true, true, true, false, nullptr, true);
	if (Result != EPathFollowingRequestResult::Failed)
	{
		ConsecutiveMovementRestartFailures = 0;
		return true;
	}

	return RecordFailedRestart();
}

void UTerritoryAssaultParticipantComponent::ReconcileNarrativeDefenderTargeting(
	UNPCActivityComponent* ActivityComponent,
	TConstArrayView<AActor*> LiveHostileDefenders)
{
	if (!ActivityComponent)
	{
		RestoreNarrativeDefenderTargeting(false);
		return;
	}
	if (!bPrioritizeTerritoryTakeover && LiveHostileDefenders.IsEmpty())
	{
		RestoreNarrativeDefenderTargeting(true);
		return;
	}

	UNPCGoalItem* CurrentGoal = ActivityComponent->GetCurrentActivityGoal();
	if (!NarrativeAttackGoalClass.IsValid() && CurrentGoal && CurrentGoal != AssaultGoal)
	{
		const AActor* GoalTarget = Cast<AActor>(CurrentGoal->GetGoalKey());
		const ANarrativeCharacter* NarrativeTarget = Cast<ANarrativeCharacter>(GoalTarget);
		const INarrativeTeamAgentInterface* TeamAgent =
			Cast<INarrativeTeamAgentInterface>(GetOwner());
		if (NarrativeTarget && TeamAgent
			&& TeamAgent->GetTeamAttitudeTowards(*NarrativeTarget) == ETeamAttitude::Hostile)
		{
			// Narrative's attack goal exposes its target through GetGoalKey(). Cache
			// the configured concrete class instead of hard-coding a Narrative asset path.
			NarrativeAttackGoalClass = CurrentGoal->GetClass();
		}
	}

	UClass* GoalClass = NarrativeAttackGoalClass.Get();
	if (!GoalClass || !GoalClass->IsChildOf(UNPCGoalItem::StaticClass())) return;

	const FNPCGoalContainer AttackGoals = ActivityComponent->GetGoals(GoalClass);
	const FTerritoryDefenderGoalPreferenceResult Preference =
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(
			AttackGoals, LiveHostileDefenders, NarrativeGoalScoreOverrides,
			bPrioritizeTerritoryTakeover);
	if (Preference.bScoresChanged)
	{
		// Reselect through Narrative so its configured activity, behavior tree,
		// perception goal, GAS combat, and tactical tokens remain authoritative.
		ActivityComponent->PerformActivitySelection(true);
	}
}

void UTerritoryAssaultParticipantComponent::RestoreNarrativeDefenderTargeting(
	bool bReselectActivity)
{
	if (!TerritoryAssaultTargetPolicy::RestoreGoalScores(NarrativeGoalScoreOverrides)) return;
	if (!bReselectActivity) return;

	if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		if (UNPCActivityComponent* ActivityComponent = NPC->GetActivityComponent())
		{
			ActivityComponent->PerformActivitySelection(true);
		}
	}
}

void UTerritoryAssaultParticipantComponent::UnregisterCapturePressure()
{
	if (!bCaptureRegistered) return;
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;
	ATerritoryVolume* Territory = ResolveTargetTerritory();
	if (!Territory)
	{
		if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			if (ANarrativeNPCController* Controller = NPC->GetNPCController())
			{
				if (UTerritoryCombatDirector* Director =
					World->GetSubsystem<UTerritoryCombatDirector>())
				{
					Director->ReleaseAllSlots(Controller);
				}
			}
		}
		bCaptureRegistered = false;
		return;
	}
	if (Territory)
	{
		if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
		{
			Control->UnregisterAttacker(Territory, Owner, AttackingFaction);
		}
		if (const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
		{
			if (ANarrativeNPCController* Controller = NPC->GetNPCController())
			{
				if (UTerritoryCombatDirector* Director = World->GetSubsystem<UTerritoryCombatDirector>())
				{
					Director->ReleaseAssaultSlot(Territory, Controller);
				}
			}
		}
	}
	bCaptureRegistered = false;
}

ATerritoryVolume* UTerritoryAssaultParticipantComponent::ResolveTargetTerritory() const
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;
	if (TargetTerritoryGUID.IsValid())
	{
		// Never let a streamed replacement inherit physical capture pressure merely
		// because it reused the old gameplay tag.
		return Registry->GetTerritoryByGUID(TargetTerritoryGUID);
	}
	return TargetTerritory.IsValid()
		? Registry->GetTerritoryByTag(TargetTerritory) : nullptr;
}

bool UTerritoryAssaultParticipantComponent::MatchesTargetTerritory(
	const ATerritoryVolume* Territory) const
{
	if (!Territory) return false;
	if (TargetTerritoryGUID.IsValid())
	{
		return TargetTerritoryGUID == Territory->GetTerritoryGUID();
	}
	return TargetTerritory.IsValid()
		&& TargetTerritory == Territory->GetTerritoryTag();
}

TArray<AActor*> UTerritoryAssaultParticipantComponent::CollectTakeoverCombatants(
	ATerritoryVolume* Territory) const
{
	TArray<AActor*> Result =
		TerritoryAssaultTargetPolicy::CollectRegisteredDefenders(Territory);
	const UWorld* World = GetWorld();
	if (!Territory || !World) return Result;
	const FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (!DefendingFaction.IsValid()) return Result;

	const TArray<ATerritoryVolume*> DefenceFront =
		TerritoryAssaultTargetPolicy::BuildDefenceFront(Territory);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()
			|| !UTerritoryBlueprintLibrary::IsActorInFaction(
				this, Pawn, DefendingFaction))
		{
			continue;
		}
		if (const IAbilitySystemInterface* AbilityPawn =
			Cast<IAbilitySystemInterface>(Pawn))
		{
			if (const UNarrativeAbilitySystemComponent* ASC =
				Cast<UNarrativeAbilitySystemComponent>(
					AbilityPawn->GetAbilitySystemComponent()); ASC && ASC->IsDead())
			{
				continue;
			}
		}

		const FVector Location = Pawn->GetActorLocation();
		const bool bInsideLocalFight = DefenceFront.ContainsByPredicate(
			[this, &Location](const ATerritoryVolume* Defence)
			{
				return Defence && Defence->GetTerritoryBounds()
					.ExpandBy(DefendingPlayerEngagementPadding)
					.IsInsideOrOn(Location);
			});
		if (bInsideLocalFight) Result.AddUnique(Pawn);
	}
	return Result;
}

void UTerritoryAssaultParticipantComponent::PlayMissionDialogue(
	const FGameplayTag& DialogueTag)
{
	ATerritoryAssaultCharacter* NPC =
		Cast<ATerritoryAssaultCharacter>(GetOwner());
	const UWorld* World = GetWorld();
	if (!NPC || !World || !DialogueTag.IsValid()) return;
	APawn* ClosestPlayer = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Candidate = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed()) continue;
		const float DistanceSquared = FVector::DistSquared(
			Candidate->GetActorLocation(), NPC->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestPlayer = Candidate;
		}
	}
	if (ClosestPlayer) NPC->PlayTaggedDialogue(DialogueTag, ClosestPlayer);
}

void UTerritoryAssaultParticipantComponent::UpdateVehicleDriving(const float DeltaTime)
{
	ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	UChaosWheeledVehicleMovementComponent* Movement = Vehicle
		? Vehicle->FindComponentByClass<UChaosWheeledVehicleMovementComponent>() : nullptr;
	if (!Vehicle || !Movement || VehicleRoutePoints.IsEmpty())
	{
		bVehicleDriveActive = false;
		bVehicleIngressFailed = true;
		SetComponentTickEnabled(false);
		return;
	}

	if (bEscapeOnVehicleArrival && MaximumChaseDistance > 0.f)
	{
		const float ClosestPlayerDistance = GetClosestPlayerDistanceToVehicle();
		SecondsOutsideChaseRange = ClosestPlayerDistance > MaximumChaseDistance
			? SecondsOutsideChaseRange + FMath::Max(0.f, DeltaTime) : 0.f;
		if (ShouldFailChaseDistance(SecondsOutsideChaseRange,
			ChaseDistanceGraceSeconds, ClosestPlayerDistance, MaximumChaseDistance))
		{
			bEscapeCompletionReported = true;
			StopVehicleInputs();
			bVehicleDriveActive = false;
			SetComponentTickEnabled(false);
			if (UTerritoryCounterAttackSubsystem* Counterattacks = GetWorld()
				? GetWorld()->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr)
			{
				Counterattacks->NotifyVehicleStoryTargetLostByDistance(
					AssaultID, Cast<ATerritoryAssaultCharacter>(GetOwner()));
			}
			return;
		}
	}

	const float VehicleHealthFraction = GetNarrativeVehicleHealthFraction();
	if (bEscapeOnVehicleArrival && bAbandonDamagedVehicleForFinalFight
		&& ShouldAbandonVehicle(VehicleHealthFraction,
			VehicleAbandonHealthFraction, VehicleBlockedSeconds,
			VehicleAwareness.AbandonAfterBlockedSeconds))
	{
		BeginVehicleAbandonment(VehicleHealthFraction <= VehicleAbandonHealthFraction
			? TEXT("Narrative vehicle health threshold reached")
			: TEXT("vehicle remained blocked"));
	}

	if (bVehicleAbandonmentRequested)
	{
		StopVehicleInputs();
		TryBeginVehicleDismount();
		return;
	}

	const FVector VehicleLocation = Vehicle->GetActorLocation();
	constexpr float WaypointAcceptanceRadius = 300.f;
	while (VehicleRoutePointIndex < VehicleRoutePoints.Num() - 1
		&& FVector::DistSquared2D(VehicleLocation,
			VehicleRoutePoints[VehicleRoutePointIndex])
			<= FMath::Square(WaypointAcceptanceRadius))
	{
		++VehicleRoutePointIndex;
	}

	const bool bFinalWaypoint = VehicleRoutePointIndex >= VehicleRoutePoints.Num() - 1;
	const FVector Target = VehicleRoutePoints[VehicleRoutePointIndex];
	const float Distance = FVector::Dist2D(VehicleLocation, Target);
	const float Speed = Vehicle->GetVelocity().Size2D();
	const float DesiredSpeed = VehicleMaximumDriveSpeed > 0.f
		? VehicleMaximumDriveSpeed : 1400.f;

	if (bFinalWaypoint && Distance <= 375.f)
	{
		StopVehicleInputs();
		if (bEscapeOnVehicleArrival)
		{
			CompleteVehicleIngress();
			return;
		}

		if (!bVehicleDismountRequested && Speed <= 250.f) TryBeginVehicleDismount();
		return;
	}

	FVector ToTarget = Target - VehicleLocation;
	ToTarget.Z = 0.f;
	if (!ToTarget.Normalize()) return;
	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Vehicle->GetActorRightVector().GetSafeNormal2D();
	const float ForwardAlignment = FVector::DotProduct(Forward, ToTarget);
	const float RightAlignment = FVector::DotProduct(Right, ToTarget);
	const float HeadingError = FMath::Atan2(RightAlignment, ForwardAlignment);
	const float Steering = FMath::Clamp(HeadingError / FMath::DegreesToRadians(50.f),
		-1.f, 1.f);
	const bool bApproachingFinal = bFinalWaypoint && Distance < 900.f;
	float CentreObstacleDistance = TNumericLimits<float>::Max();
	const bool bCentreBlocked = QueryVehicleObstacleDistance(
		FVector::ZeroVector, CentreObstacleDistance);
	const float ObstacleSpeedFactor = bCentreBlocked
		? CalculateObstacleSpeedFactor(CentreObstacleDistance,
			VehicleAwareness.EmergencyStopDistance,
			VehicleAwareness.BrakingDistance) : 1.f;
	if (bCentreBlocked && ObstacleSpeedFactor <= 0.05f && Speed < 175.f)
	{
		VehicleBlockedSeconds += FMath::Max(0.f, DeltaTime);
	}
	else
	{
		VehicleBlockedSeconds = FMath::Max(0.f,
			VehicleBlockedSeconds - FMath::Max(0.f, DeltaTime) * 2.f);
	}

	float AvoidanceSteering = 0.f;
	if (bCentreBlocked && VehicleAwareness.bAllowSideAvoidance)
	{
		float LeftDistance = TNumericLimits<float>::Max();
		float RightDistance = TNumericLimits<float>::Max();
		const FVector Side = Vehicle->GetActorRightVector().GetSafeNormal2D()
			* VehicleAwareness.SideProbeOffset;
		const bool bLeftBlocked = QueryVehicleObstacleDistance(-Side, LeftDistance);
		const bool bRightBlocked = QueryVehicleObstacleDistance(Side, RightDistance);
		const float LeftClearance = bLeftBlocked ? LeftDistance : VehicleAwareness.ForwardProbeDistance;
		const float RightClearance = bRightBlocked ? RightDistance : VehicleAwareness.ForwardProbeDistance;
		if (!FMath::IsNearlyEqual(LeftClearance, RightClearance, 100.f))
		{
			AvoidanceSteering = LeftClearance > RightClearance
				? -VehicleAwareness.MaximumAvoidanceSteering
				: VehicleAwareness.MaximumAvoidanceSteering;
		}
	}
	const float ObstacleLimitedSpeed = DesiredSpeed * ObstacleSpeedFactor;
	const bool bOverspeed = Speed > ObstacleLimitedSpeed;
	const float CornerThrottle = FMath::GetMappedRangeValueClamped(
		FVector2D(0.f, 1.5f), FVector2D(0.85f, 0.25f), FMath::Abs(HeadingError));
	const float ArrivalThrottle = bApproachingFinal
		? FMath::Clamp(Distance / 900.f, 0.2f, 0.7f) : 1.f;

	Movement->SetHandbrakeInput(false);
	Movement->SetSteeringInput(FMath::Clamp(Steering + AvoidanceSteering, -1.f, 1.f));
	Movement->SetBrakeInput(ObstacleSpeedFactor <= 0.05f ? 1.f : (bOverspeed ? 0.55f : 0.f));
	Movement->SetThrottleInput(bOverspeed ? 0.f
		: FMath::Min3(CornerThrottle, ArrivalThrottle, ObstacleSpeedFactor));
}

float UTerritoryAssaultParticipantComponent::CalculateObstacleSpeedFactor(
	const float ObstacleDistance, const float EmergencyStopDistance,
	const float BrakingDistance)
{
	const float Emergency = FMath::Max(0.f, EmergencyStopDistance);
	const float Braking = FMath::Max(Emergency + 1.f, BrakingDistance);
	return FMath::Clamp((ObstacleDistance - Emergency) / (Braking - Emergency),
		0.f, 1.f);
}

bool UTerritoryAssaultParticipantComponent::ShouldFailChaseDistance(
	const float SecondsOutsideRange, const float GraceSeconds,
	const float ClosestPlayerDistance, const float MaximumDistance)
{
	return MaximumDistance > 0.f && ClosestPlayerDistance > MaximumDistance
		&& SecondsOutsideRange >= FMath::Max(0.f, GraceSeconds);
}

bool UTerritoryAssaultParticipantComponent::ShouldAbandonVehicle(
	const float HealthFraction, const float HealthThreshold,
	const float BlockedSeconds, const float BlockedTimeout)
{
	const bool bDamaged = HealthThreshold > 0.f
		&& HealthFraction <= FMath::Clamp(HealthThreshold, 0.f, 1.f);
	const bool bBlocked = BlockedTimeout > 0.f && BlockedSeconds >= BlockedTimeout;
	return bDamaged || bBlocked;
}

float UTerritoryAssaultParticipantComponent::GetClosestPlayerDistanceToVehicle() const
{
	const ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	const UWorld* World = GetWorld();
	if (!Vehicle || !World) return TNumericLimits<float>::Max();
	float Closest = TNumericLimits<float>::Max();
	for (TActorIterator<APlayerController> It(World); It; ++It)
	{
		const APawn* Pawn = It->GetPawn();
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()) continue;
		Closest = FMath::Min(Closest,
			FVector::Dist2D(Pawn->GetActorLocation(), Vehicle->GetActorLocation()));
	}
	return Closest;
}

float UTerritoryAssaultParticipantComponent::GetNarrativeVehicleHealthFraction() const
{
	const ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	const UNarrativeAbilitySystemComponent* ASC = Vehicle
		? Cast<UNarrativeAbilitySystemComponent>(Vehicle->GetAbilitySystemComponent()) : nullptr;
	if (!ASC) return 1.f;
	const float MaxHealth = ASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetMaxHealthAttribute());
	if (MaxHealth <= KINDA_SMALL_NUMBER) return ASC->IsDead() ? 0.f : 1.f;
	return FMath::Clamp(ASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetHealthAttribute()) / MaxHealth, 0.f, 1.f);
}

bool UTerritoryAssaultParticipantComponent::QueryVehicleObstacleDistance(
	const FVector& LateralOffset, float& OutDistance) const
{
	const ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	const UWorld* World = GetWorld();
	if (!Vehicle || !World) return false;
	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector Start = Vehicle->GetActorLocation() + LateralOffset
		+ Forward * 150.f + FVector::UpVector * VehicleAwareness.ProbeHalfHeight;
	const FVector End = Start + Forward * FMath::Max(200.f,
		VehicleAwareness.ForwardProbeDistance);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TerritoryVehicleAwareness), false);
	Params.AddIgnoredActor(Vehicle);
	Params.AddIgnoredActor(GetOwner());
	FHitResult Hit;
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(
		FMath::Max(50.f, VehicleAwareness.ProbeHalfWidth),
		FMath::Max(50.f, VehicleAwareness.ProbeHalfWidth),
		FMath::Max(25.f, VehicleAwareness.ProbeHalfHeight)));
	if (!World->SweepSingleByObjectType(Hit, Start, End,
		Vehicle->GetActorQuat(), Objects, Box, Params))
	{
		return false;
	}
	OutDistance = FVector::Dist2D(Start, Hit.ImpactPoint);
	return true;
}

bool UTerritoryAssaultParticipantComponent::TryBeginVehicleDismount()
{
	if (bVehicleDismountRequested) return true;
	ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	ATerritoryAssaultCharacter* NPC = Cast<ATerritoryAssaultCharacter>(GetOwner());
	ANarrativeNPCController* Controller = NPC ? NPC->GetNPCController() : nullptr;
	UNPCInteractionComponent* Interaction = Controller
		? Controller->GetInteractionComponent() : nullptr;
	if (!Vehicle || !NPC || !Interaction)
	{
		bVehicleIngressFailed = true;
		return false;
	}
	if (Vehicle->GetVelocity().Size2D() > 250.f) return false;
	if (!Interaction->StopInteractBehavior(false)) return false;
	bVehicleDismountRequested = true;
	bVehicleDriveActive = false;
	SetComponentTickEnabled(false);
	UE_LOG(LogTerritory, Display,
		TEXT("[CounterAttack] Narrative driver %s stopped %s and began authored dismount"),
		*GetNameSafe(NPC), *GetNameSafe(Vehicle));
	return true;
}

void UTerritoryAssaultParticipantComponent::BeginVehicleAbandonment(
	const TCHAR* Reason)
{
	if (bVehicleAbandonmentRequested) return;
	bVehicleAbandonmentRequested = true;
	bEscapeOnVehicleArrival = false;
	StopVehicleInputs();
	if (UTerritoryCounterAttackSubsystem* Counterattacks = GetWorld()
		? GetWorld()->GetSubsystem<UTerritoryCounterAttackSubsystem>() : nullptr)
	{
		Counterattacks->NotifyVehicleStoryTargetAbandoned(
			AssaultID, Cast<ATerritoryAssaultCharacter>(GetOwner()));
	}
	PlayMissionDialogue(FinalFightDialogueTag);
	UE_LOG(LogTerritory, Display,
		TEXT("[CounterAttack] story target %s is abandoning %s for the final fight: %s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(NarrativeIngressVehicle.Get()),
		Reason ? Reason : TEXT("mission rule"));
}

void UTerritoryAssaultParticipantComponent::StopVehicleInputs()
{
	ANarrativeVehicleBase* Vehicle = NarrativeIngressVehicle.Get();
	if (UChaosWheeledVehicleMovementComponent* Movement = Vehicle
		? Vehicle->FindComponentByClass<UChaosWheeledVehicleMovementComponent>() : nullptr)
	{
		Movement->SetThrottleInput(0.f);
		Movement->SetSteeringInput(0.f);
		Movement->SetBrakeInput(1.f);
	}
}

void UTerritoryAssaultParticipantComponent::CompleteVehicleIngress()
{
	StopVehicleInputs();
	bVehicleDriveActive = false;
	bVehicleIngressComplete = true;
	bVehicleIngressFailed = false;
	SetComponentTickEnabled(false);
	UE_LOG(LogTerritory, Display,
		TEXT("[CounterAttack] Narrative vehicle pursuit completed for %s assault=%s"),
		*GetNameSafe(GetOwner()), *AssaultID.ToString());
}

void UTerritoryAssaultParticipantComponent::WithdrawForVehicleIngressFailure(
	const TCHAR* Reason)
{
	AActor* Owner = GetOwner();
	UE_LOG(LogTerritory, Error,
		TEXT("[CounterAttack] %s withdrew before vehicle arrival: %s"),
		*GetNameSafe(Owner), Reason ? Reason : TEXT("unknown ingress failure"));
	Retire(false);
	if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Owner))
	{
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*NPC);
	}
	if (Owner) Owner->Destroy();
}

void UTerritoryAssaultParticipantComponent::Retire(bool bKilled)
{
	if (bRemovalReported) return;
	bRemovalReported = true;
	RestoreNarrativeDefenderTargeting(false);
	UnregisterCapturePressure();
	StopVehicleInputs();
	bVehicleDriveActive = false;
	SetComponentTickEnabled(false);

	if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		if (UNPCActivityComponent* ActivityComponent = NPC->GetActivityComponent())
		{
			if (AssaultGoal) ActivityComponent->RemoveGoal(AssaultGoal);
			ActivityComponent->RemoveActivity(UTerritoryAssaultActivity::StaticClass());
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryCounterAttackSubsystem* Counterattacks =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			Counterattacks->NotifyParticipantRemoved(
				AssaultID, Cast<ATerritoryAssaultCharacter>(GetOwner()), bKilled);
		}
	}
}

void UTerritoryAssaultParticipantComponent::HandleOwnerDied(
	AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC, const bool bIsDead)
{
	if (!bIsDead)
	{
		return;
	}
	(void)KilledActor;
	(void)KilledASC;
	// Preserve Narrative's physical death presentation even if a project or test
	// changes delegate registration order. This call is idempotent and still uses
	// Narrative's replicated ragdoll authority.
	if (ATerritoryAssaultCharacter* NPC =
		Cast<ATerritoryAssaultCharacter>(GetOwner()))
	{
		NPC->ReconcileNarrativeDeathState(KilledASC, bIsDead);
	}
	Retire(true);
}
