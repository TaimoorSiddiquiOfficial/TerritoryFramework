#include "Subsystems/TerritoryCounterAttackSubsystem.h"

#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryCommandTags.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Navigation/TerritoryRoadGuide.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/NPCDefinition.h"
#include "NarrativeGameplayTags.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryQuestRules.h"
#include "Vehicles/MountComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "AbilitySystemComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "ZoneGraphAStar.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphTypes.h"

namespace
{
	bool HasPlayerVehicleOccupant(const ANarrativeVehicleBase* Vehicle)
	{
		if (!IsValid(Vehicle)) return false;
		if (Cast<APlayerController>(Vehicle->GetController())) return true;
		const UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
		if (!Mount) return false;
		for (const FActiveInteractionSlot& Slot : Mount->SlotStatuses)
		{
			if (Slot.SlotStatus == EInteractionSlotStatus::ISS_Free || !IsValid(Slot.SlotUser)) continue;
			// Narrative's player interaction component lives on the controller.
			if (Cast<APlayerController>(Slot.SlotUser->GetOwner())) return true;
			const APawn* Occupant = Cast<APawn>(Slot.SlotUser->GetOwner());
			// A mounted Narrative player can be temporarily unpossessed. The native
			// slot still owns the relationship, including passengers and entry transitions.
			if (IsValid(Occupant) && (Cast<ANarrativePlayerCharacter>(Occupant)
				|| Occupant->IsPlayerControlled())) return true;
		}
		return false;
	}

	void RemoveVehicleForCampaignRestore(ANarrativeVehicleBase* Vehicle)
	{
		if (!IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed()
			|| !Vehicle->HasAuthority() || HasPlayerVehicleOccupant(Vehicle)) return;
		// A new campaign snapshot reconstructs surviving forces at authored spawn pads.
		// Clear the old car immediately; retain the actor briefly for Narrative's
		// latent mount/dismount callbacks, as with retired assault NPCs.
		Vehicle->SetActorEnableCollision(false);
		Vehicle->SetActorHiddenInGame(true);
		Vehicle->SetLifeSpan(0.75f);
		Vehicle->ForceNetUpdate();
	}

	ENarrativeGameplayDifficulty GetCurrentNarrativeDifficulty()
	{
		UNarrativeGameUserSettings* Settings =
			Cast<UNarrativeGameUserSettings>(
				UGameUserSettings::GetGameUserSettings());
		return Settings ? Settings->GetGameplayDifficulty()
			: ENarrativeGameplayDifficulty::Easy;
	}

	UNPCDefinition* ResolveAssaultDefinition(const FTerritoryAssaultRecord& Assault,
		const FTerritoryFactionAssaultConfig& ForceConfig)
	{
		return Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
			&& !Assault.StoryAttackerDefinitionOverride.IsNull()
			? Assault.StoryAttackerDefinitionOverride.LoadSynchronous()
			: ForceConfig.AttackerDefinition.Get();
	}

	int32 ResolveAssaultPlannedForce(const FTerritoryAssaultRecord& Assault,
		const FTerritoryFactionAssaultConfig& ForceConfig)
	{
		return Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
			&& Assault.StoryPlannedForceOverride > 0
			? Assault.StoryPlannedForceOverride : ForceConfig.PlannedForce;
	}

	int32 ResolveAssaultWaveSize(const FTerritoryAssaultRecord& Assault,
		const FTerritoryFactionAssaultConfig& ForceConfig, int32 PlannedForce)
	{
		const int32 Requested = Assault.LaunchMode ==
			ETerritoryAssaultLaunchMode::StoryPursuit
			&& Assault.StoryWaveSizeOverride > 0
			? Assault.StoryWaveSizeOverride : ForceConfig.WaveSize;
		return FMath::Clamp(Requested, 1, FMath::Max(1, PlannedForce));
	}

	bool UsesOnlyNarrativeVehicleApproaches(const FTerritoryAssaultRecord& Assault,
		const ATerritoryVolume* Territory)
	{
		if (!Territory || Assault.SelectedApproaches.IsEmpty()) return false;
		for (const FName ApproachID : Assault.SelectedApproaches)
		{
			const FTerritoryAssaultApproach* Approach =
				Territory->GetCounterAttackApproaches().FindByPredicate(
					[ApproachID](const FTerritoryAssaultApproach& Candidate)
					{
						return Candidate.ApproachID == ApproachID;
					});
			if (!Approach || Approach->EntryType !=
				ETerritoryAssaultEntryType::NarrativeVehicle)
			{
				return false;
			}
		}
		return true;
	}
}

int32 UTerritoryCounterAttackSubsystem::ResolveVehicleOccupantCount(
	const int32 RemainingWaveSlots, const int32 ApproachWaveLimit,
	const int32 ConfiguredVehicleCapacity, const int32 NarrativeMountSeatCount)
{
	return FMath::Max(0, FMath::Min(
		FMath::Min(RemainingWaveSlots, ApproachWaveLimit),
		FMath::Min(ConfiguredVehicleCapacity, NarrativeMountSeatCount)));
}

int32 UTerritoryCounterAttackSubsystem::ResolveVehicleOnlyPlannedForce(
	const int32 RequestedForce, const int32 MaximumVehicleDeployments,
	const TArray<int32>& VehicleDeploymentCapacities)
{
	if (RequestedForce <= 0 || MaximumVehicleDeployments <= 0
		|| VehicleDeploymentCapacities.IsEmpty())
	{
		return 0;
	}

	TArray<int32> SortedCapacities = VehicleDeploymentCapacities;
	SortedCapacities.Sort(TGreater<int32>());
	int64 AvailableSeats = 0;
	const int32 DeploymentCount = FMath::Min(
		MaximumVehicleDeployments, SortedCapacities.Num());
	for (int32 Index = 0; Index < DeploymentCount; ++Index)
	{
		AvailableSeats += FMath::Max(0, SortedCapacities[Index]);
		if (AvailableSeats >= RequestedForce) return RequestedForce;
	}
	return static_cast<int32>(AvailableSeats);
}

void UTerritoryCounterAttackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Assault scheduling subscribes to all three systems during initialization.
	// Explicit dependencies prevent missed early ownership or diplomacy events.
	Collection.InitializeDependency<UTerritoryRegistrySubsystem>();
	Collection.InitializeDependency<UTerritoryControlSubsystem>();
	Collection.InitializeDependency<UTerritoryDiplomacySubsystem>();
	UWorld* World = GetWorld();
	if (!World) return;

	if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		Control->OnTerritoryControlChanged.AddUniqueDynamic(
			this, &UTerritoryCounterAttackSubsystem::HandleTerritoryControlChanged);
	}
	if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		Diplomacy->OnDiplomacyStateChanged.AddUniqueDynamic(
			this, &UTerritoryCounterAttackSubsystem::HandleDiplomacyChanged);
	}
	if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		Registry->OnTerritoryRegistered.AddUniqueDynamic(
			this, &UTerritoryCounterAttackSubsystem::HandleTerritoryRegistered);
	}
}

void UTerritoryCounterAttackSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const float Interval = FMath::Max(0.1f,
		Settings ? Settings->CounterAttackUpdateInterval : 2.f);
	InWorld.GetTimerManager().SetTimer(UpdateTimer, this,
		&UTerritoryCounterAttackSubsystem::UpdateAssaults, Interval, true);
}

void UTerritoryCounterAttackSubsystem::Deinitialize()
{
	++RestoreGeneration;
	bRestoringState = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
		if (UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
		{
			Control->OnTerritoryControlChanged.RemoveDynamic(
				this, &UTerritoryCounterAttackSubsystem::HandleTerritoryControlChanged);
		}
		if (UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>())
		{
			Diplomacy->OnDiplomacyStateChanged.RemoveDynamic(
				this, &UTerritoryCounterAttackSubsystem::HandleDiplomacyChanged);
		}
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(
				this, &UTerritoryCounterAttackSubsystem::HandleTerritoryRegistered);
		}
	}
	LiveParticipants.Empty();
	LiveAssaultVehicles.Empty();
	LiveVehicleRetirementRules.Empty();
	for (auto& Pair : LiveAssaultRoadGuides)
	{
		for (const TWeakObjectPtr<ATerritoryRoadGuide>& WeakGuide : Pair.Value)
		{
			if (ATerritoryRoadGuide* Guide = WeakGuide.Get()) Guide->EndMissionTraffic();
		}
	}
	LiveAssaultRoadGuides.Empty();
	RetiringVehicles.Empty();
	WarnedControllers.Empty();
	Assaults.Empty();
	EvaluationCycleHighWater.Empty();
	bRestoringState = false;
	Super::Deinitialize();
}

FTerritoryAssaultEvaluationResult UTerritoryCounterAttackSubsystem::CalculateEvaluation(
	const FTerritoryAssaultEvaluationInput& Input,
	const UTerritoryCounterAttackProfile* Profile)
{
	FTerritoryAssaultEvaluationResult Result;
	if (!Profile) return Result;

	const float Active = FMath::Max(0, Input.ActiveGuards);
	const float Desired = FMath::Max(0, Input.DesiredGuards);
	const float Maximum = FMath::Max(1, Input.MaximumGuards);
	const float Reserve = FMath::Max(0, Input.ReserveGuards);
	const float Quality = FMath::Max(0.f, Input.GuardQuality);
	const float Fortification = FMath::Max(0.f, Input.Fortification);
	const float AlliedSupport = FMath::Max(0.f, Input.NearbyAlliedSupport);
	const float AttackPower = FMath::Max(0.f, Input.AttackingMilitaryPower);

	const float ActiveRatio = FMath::Clamp(Active / Maximum, 0.f, 1.f);
	const float ShortfallRatio = FMath::Clamp((Desired - Active) / Maximum, 0.f, 1.f);
	Result.DistrictDefencePower = Active * FMath::Max(0.01f, Quality)
		+ Reserve * FMath::Max(0.01f, Quality) * 0.5f
		+ Fortification + AlliedSupport;
	Result.PowerRatio = AttackPower / FMath::Max(1.f, Result.DistrictDefencePower);

	const float NormalizedDefence = Result.DistrictDefencePower
		/ (Result.DistrictDefencePower + 100.f);
	const float NormalizedAttack = AttackPower / (AttackPower + 100.f);
	const float NormalizedStrategicValue = FMath::Max(0.f, Input.StrategicValue)
		/ (FMath::Max(0.f, Input.StrategicValue) + 1.f);
	const float Readiness = FMath::Clamp(
		(Input.EconomyReadiness + Input.SupplyReadiness) * 0.5f, 0.f, 1.f);
	const float Momentum = FMath::Clamp(Input.RecentMomentum, -1.f, 1.f);
	const float Influence = FMath::Clamp(Input.FactionInfluence, 0.f, 1.f);

	const float RawProbability = Profile->BaseLaunchProbability
		+ Profile->AttackerPowerWeight * NormalizedAttack
		+ Profile->StrategicValueWeight * NormalizedStrategicValue
		+ Profile->ReadinessWeight * Readiness
		+ Profile->InfluenceWeight * Influence
		+ 0.20f * ShortfallRatio
		+ 0.05f * Momentum
		- Profile->DefenceDeterrenceWeight * (0.60f * ActiveRatio + 0.40f * NormalizedDefence);
	Result.LaunchProbability = Active <= 0.f
		? FMath::Clamp(Profile->UnguardedLaunchProbability, 0.f, 1.f)
		: FMath::Clamp(RawProbability,
			Profile->MinimumLaunchProbability, Profile->MaximumLaunchProbability);
	Result.EstimatedSuccessProbability = AttackPower
		/ FMath::Max(1.f, AttackPower + Result.DistrictDefencePower);
	Result.AttackPriority = FMath::Max(0.f,
		100.f * (Result.LaunchProbability + 0.25f * NormalizedStrategicValue
			+ 0.15f * Result.EstimatedSuccessProbability));
	return Result;
}

int32 UTerritoryCounterAttackSubsystem::CalculateEffectiveReserveGuards(
	int32 RawReserveGuards, int32 DesiredGuardCount)
{
	return FMath::Min(FMath::Max(0, RawReserveGuards),
		FMath::Max(0, DesiredGuardCount));
}

float UTerritoryCounterAttackSubsystem::CalculateInfluenceAdjustedDelay(
	float BaseDelay, float FactionInfluence, float MinimumTimingScale)
{
	const float SafeBaseDelay = FMath::Max(0.f, BaseDelay);
	const float Influence = FMath::Clamp(FactionInfluence, 0.f, 1.f);
	const float MinimumScale = FMath::Clamp(MinimumTimingScale, 0.05f, 1.f);
	return SafeBaseDelay * FMath::Lerp(1.f, MinimumScale, Influence);
}

bool UTerritoryCounterAttackSubsystem::IsNarrativeTimeInWindow(
	float TimeOfDay, float WindowStart, float WindowEnd)
{
	auto NormalizeNarrativeTime = [](float Value)
	{
		const float Wrapped = FMath::Fmod(Value, 2400.f);
		return Wrapped < 0.f ? Wrapped + 2400.f : Wrapped;
	};
	const float Now = NormalizeNarrativeTime(TimeOfDay);
	const float Start = NormalizeNarrativeTime(WindowStart);
	const float End = NormalizeNarrativeTime(WindowEnd);
	if (FMath::IsNearlyEqual(Start, End)) return true;
	return Start < End ? Now >= Start && Now < End : Now >= Start || Now < End;
}

bool UTerritoryCounterAttackSubsystem::CanContinueSchedule(
	ETerritoryCounterScheduleMode ScheduleMode, int32 PreviousOccurrence,
	int32 MaximumScheduledAssaults)
{
	const int32 SafePreviousOccurrence = FMath::Max(1, PreviousOccurrence);
	switch (ScheduleMode)
	{
	case ETerritoryCounterScheduleMode::FiniteSeries:
		return SafePreviousOccurrence < FMath::Max(1, MaximumScheduledAssaults);
	case ETerritoryCounterScheduleMode::UnlimitedSeries:
		return true;
	case ETerritoryCounterScheduleMode::SingleAssault:
	default:
		return false;
	}
}

bool UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
	ETerritoryAssaultState AssaultState, ETerritoryState TerritoryState,
	const FGameplayTag& ContestingFaction, const FGameplayTag& AttackingFaction)
{
	if (TerritoryState == ETerritoryState::Claimed)
	{
		return true;
	}
	return (AssaultState == ETerritoryAssaultState::Active
			|| AssaultState == ETerritoryAssaultState::RecaptureCountdown)
		&& TerritoryState == ETerritoryState::Contested
		&& AttackingFaction.IsValid()
		&& ContestingFaction == AttackingFaction;
}

bool UTerritoryCounterAttackSubsystem::ShouldEmitCounterHappened(
	ETerritoryAssaultState PreviousState, ETerritoryAssaultState NewState,
	bool bIsRestoringState)
{
	return !bIsRestoringState && PreviousState != NewState;
}

FTerritoryCounterAttackStateEvent UTerritoryCounterAttackSubsystem::MakeCounterHappenedEvent(
	const FTerritoryAssaultRecord& Assault, ETerritoryAssaultState PreviousState,
	double EventGameTime)
{
	FTerritoryCounterAttackStateEvent Event;
	Event.Assault = Assault;
	Event.PreviousState = PreviousState;
	Event.NewState = Assault.State;
	Event.Resolution = Assault.Resolution;
	Event.EventGameTime = EventGameTime;
	return Event;
}

bool UTerritoryCounterAttackSubsystem::FindBestEligibleAttacker(
	const ATerritoryVolume* Territory, const FGameplayTag& PreferredFaction,
	FGameplayTag& OutAttackingFaction, FTerritoryAssaultEvaluationInput& OutInput,
	FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason,
	bool bRequireRecurringEligibility, bool bExplicitNarrativeRequest) const
{
	OutAttackingFaction = FGameplayTag();
	OutInput = FTerritoryAssaultEvaluationInput();
	OutResult = FTerritoryAssaultEvaluationResult();
	OutReason = FText::GetEmpty();
	if (!Territory || Territory->GetControlMode() != ETerritoryControlMode::Independent
		|| !Territory->IsAvailableForGameplay()
		|| Territory->GetTerritoryState() != ETerritoryState::Claimed
		|| !Territory->GetOwningFaction().IsValid())
	{
		OutReason = NSLOCTEXT("TerritoryCounterAttack", "PreviewInvalidTerritory",
			"Counterattack evaluation requires a securely claimed territory.");
		return false;
	}

	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	if (!Profile || Profile->FactionForces.IsEmpty())
	{
		OutReason = NSLOCTEXT("TerritoryCounterAttack", "PreviewNoProfile",
			"No counterattack force profile is assigned to this territory.");
		return false;
	}

	int32 ConfiguredCandidateCount = 0;
	int32 DiplomacyBlockedCount = 0;
	int32 InvalidSpawnClassCount = 0;
	int32 StagingBlockedCount = 0;
	int32 RecurringBlockedCount = 0;
	int32 QuestBlockedCount = 0;
	int32 CapabilityBlockedCount = 0;
	float BestMilitaryPower = -1.f;
	for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
	{
		if (!Force.Faction.IsValid() || Force.Faction == Territory->GetOwningFaction()
			|| !Force.AttackerDefinition || Force.PlannedForce <= 0 || Force.WaveSize <= 0
			|| Force.MilitaryPower <= 0.f)
		{
			continue;
		}
		++ConfiguredCandidateCount;
		if (!bExplicitNarrativeRequest && !DoesForceMeetStagingRequirement(
			Force, ETerritoryAssaultLaunchMode::StrategicCounterattack))
		{
			++StagingBlockedCount;
			continue;
		}
		if (!bExplicitNarrativeRequest
			&& !DoesFactionMeetReinforcementCapabilityRequirement(Profile,
			Force.Faction, ETerritoryAssaultLaunchMode::StrategicCounterattack))
		{
			++CapabilityBlockedCount;
			continue;
		}
		FText QuestFailureReason;
		if (!bExplicitNarrativeRequest
			&& !DoStrategicQuestRulesPass(Profile, Force.Faction,
			Territory->GetOwningFaction(), &QuestFailureReason))
		{
			++QuestBlockedCount;
			continue;
		}
		if (bRequireRecurringEligibility)
		{
			const FTerritoryAssaultRecord* Previous = nullptr;
			for (const auto& Pair : Assaults)
			{
				const FTerritoryAssaultRecord& Candidate = Pair.Value;
				if (!Candidate.IsTerminal()
					|| Candidate.LaunchMode != ETerritoryAssaultLaunchMode::StrategicCounterattack
					|| !DoesAssaultTargetTerritory(Candidate, Territory)
					|| Candidate.AttackingFaction != Force.Faction
					|| Candidate.DefendingFaction != Territory->GetOwningFaction())
				{
					continue;
				}
				if (!Previous || Candidate.ResolvedGameTime > Previous->ResolvedGameTime)
				{
					Previous = &Candidate;
				}
			}
			if (!Previous
				|| !CanContinueSchedule(Force.ScheduleMode,
					Previous->ScheduleOccurrence, Force.MaximumScheduledAssaults)
				|| !IsRecurringCooldownComplete(*Previous, GetCampaignGameTime(),
					Force.RecurringCounterCooldownGameTime))
			{
				++RecurringBlockedCount;
				continue;
			}
		}
		FText SpawnClassFailure;
		if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
			Force.AttackerDefinition, Force.PlannedForce, SpawnClassFailure))
		{
			++InvalidSpawnClassCount;
			continue;
		}

		FTerritoryAssaultRecord AdmissionRecord;
		AdmissionRecord.AttackingFaction = Force.Faction;
		AdmissionRecord.DefendingFaction = Territory->GetOwningFaction();
		if (IsDiplomacyBlocked(AdmissionRecord, Territory))
		{
			++DiplomacyBlockedCount;
			continue;
		}

		const FTerritoryAssaultEvaluationInput CandidateInput =
			BuildEvaluationInput(Territory, Force);
		const FTerritoryAssaultEvaluationResult CandidateResult =
			CalculateEvaluation(CandidateInput, Profile);
		const bool bHasBest = OutAttackingFaction.IsValid();
		const bool bHigherPriority = !bHasBest
			|| CandidateResult.AttackPriority > OutResult.AttackPriority + KINDA_SMALL_NUMBER;
		const bool bPriorityTie = bHasBest
			&& FMath::IsNearlyEqual(CandidateResult.AttackPriority, OutResult.AttackPriority);
		const bool bHigherSuccess = bPriorityTie
			&& CandidateResult.EstimatedSuccessProbability
				> OutResult.EstimatedSuccessProbability + KINDA_SMALL_NUMBER;
		const bool bSuccessTie = bPriorityTie
			&& FMath::IsNearlyEqual(CandidateResult.EstimatedSuccessProbability,
				OutResult.EstimatedSuccessProbability);
		const bool bHigherPower = bSuccessTie
			&& Force.MilitaryPower > BestMilitaryPower + KINDA_SMALL_NUMBER;
		const bool bPowerTie = bSuccessTie
			&& FMath::IsNearlyEqual(Force.MilitaryPower, BestMilitaryPower);
		const bool bPreferredTieBreak = bPowerTie
			&& Force.Faction == PreferredFaction && OutAttackingFaction != PreferredFaction;
		const bool bStableTagTieBreak = bPowerTie
			&& (Force.Faction == PreferredFaction) == (OutAttackingFaction == PreferredFaction)
			&& Force.Faction.ToString() < OutAttackingFaction.ToString();

		if (bHigherPriority || bHigherSuccess || bHigherPower
			|| bPreferredTieBreak || bStableTagTieBreak)
		{
			OutAttackingFaction = Force.Faction;
			OutInput = CandidateInput;
			OutResult = CandidateResult;
			BestMilitaryPower = Force.MilitaryPower;
		}
	}

	if (!OutAttackingFaction.IsValid())
	{
		OutReason = ConfiguredCandidateCount > 0
			&& StagingBlockedCount == ConfiguredCandidateCount
			? NSLOCTEXT("TerritoryCounterAttack", "PreviewStagingBlocked",
				"No configured opposing faction owns the secure District required to stage a strategic counterattack.")
			: ConfiguredCandidateCount > 0
				&& CapabilityBlockedCount + StagingBlockedCount == ConfiguredCandidateCount
				? NSLOCTEXT("TerritoryCounterAttack", "PreviewCapabilityBlocked",
					"No configured opposing faction currently holds the Reinforcements Territory capability needed to prepare a counterattack.")
			: ConfiguredCandidateCount > 0 && bRequireRecurringEligibility
				&& RecurringBlockedCount + StagingBlockedCount == ConfiguredCandidateCount
				? NSLOCTEXT("TerritoryCounterAttack", "PreviewRecurringBlocked",
					"No configured opposing faction has an eligible resolved counterattack whose recurring cooldown is complete.")
			: ConfiguredCandidateCount > 0
				&& QuestBlockedCount + StagingBlockedCount + RecurringBlockedCount
					== ConfiguredCandidateCount
				? NSLOCTEXT("TerritoryCounterAttack", "PreviewQuestBlocked",
					"Narrative quest rules currently block every otherwise configured strategic counterattack.")
			: ConfiguredCandidateCount > 0
			&& DiplomacyBlockedCount == ConfiguredCandidateCount
			? NSLOCTEXT("TerritoryCounterAttack", "PreviewDiplomacyBlocked",
				"Every configured opposing faction is blocked by diplomacy or Narrative attitude.")
			: ConfiguredCandidateCount > 0
				&& InvalidSpawnClassCount == ConfiguredCandidateCount
				? NSLOCTEXT("TerritoryCounterAttack", "PreviewInvalidSpawnClass",
					"Every configured opposing force has an invalid Narrative assault pawn/controller contract.")
				: NSLOCTEXT("TerritoryCounterAttack", "PreviewNoValidForce",
					"No configured opposing faction has a valid finite force for this territory.");
		return false;
	}

	OutReason = OutInput.ActiveGuards <= 0
		? NSLOCTEXT("TerritoryCounterAttack", "PreviewUnguarded",
			"Diplomacy permits an assault and the complete local defence cascade has no active guards.")
		: NSLOCTEXT("TerritoryCounterAttack", "PreviewEligible",
			"Diplomacy permits an assault; the strongest configured eligible faction is shown.");
	return true;
}

bool UTerritoryCounterAttackSubsystem::GetBestEligibleAttackerPreview(
	const ATerritoryVolume* Territory, FGameplayTag PreferredFaction,
	FGameplayTag& OutAttackingFaction, FTerritoryAssaultEvaluationInput& OutInput,
	FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason) const
{
	return FindBestEligibleAttacker(Territory, PreferredFaction,
		OutAttackingFaction, OutInput, OutResult, OutReason);
}

bool UTerritoryCounterAttackSubsystem::GetBestAuthoredWaveAttackerPreview(
	const ATerritoryVolume* Territory, FGameplayTag PreferredFaction,
	FGameplayTag& OutAttackingFaction,
	FTerritoryAssaultEvaluationInput& OutInput,
	FTerritoryAssaultEvaluationResult& OutResult, FText& OutReason) const
{
	return FindBestEligibleAttacker(Territory, PreferredFaction,
		OutAttackingFaction, OutInput, OutResult, OutReason,
		false, true);
}

bool UTerritoryCounterAttackSubsystem::ScheduleBestCounterAttack(
	ATerritoryVolume* Territory, FGameplayTag PreferredFaction)
{
	FText FailureReason;
	return TryScheduleBestCounterAttackWithReason(
		Territory, PreferredFaction, FailureReason);
}

bool UTerritoryCounterAttackSubsystem::TryScheduleBestCounterAttackWithReason(
	ATerritoryVolume* Territory, FGameplayTag PreferredFaction,
	FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	UWorld* World = GetWorld();
	if (!World)
	{
		OutFailureReason = NSLOCTEXT("TerritoryCounterAttack", "NoWorld",
			"The Territory Counterattack subsystem has no World.");
		return false;
	}
	if (World->GetNetMode() == NM_Client)
	{
		OutFailureReason = NSLOCTEXT("TerritoryCounterAttack", "ClientCannotSchedule",
			"Only the authoritative server can schedule an assault.");
		return false;
	}
	if (!IsValid(Territory) || Territory->GetWorld() != World)
	{
		OutFailureReason = NSLOCTEXT("TerritoryCounterAttack", "InvalidTargetTerritory",
			"The target Territory is invalid, unloaded, or belongs to another World.");
		return false;
	}
	if (!Territory->HasAuthority())
	{
		OutFailureReason = NSLOCTEXT("TerritoryCounterAttack", "TargetNotAuthority",
			"The loaded target Territory is not authoritative on this machine.");
		return false;
	}

	FGameplayTag AttackingFaction;
	FTerritoryAssaultEvaluationInput Input;
	FTerritoryAssaultEvaluationResult Result;
	if (!FindBestEligibleAttacker(Territory, PreferredFaction,
		AttackingFaction, Input, Result, OutFailureReason))
	{
		return false;
	}
	return ScheduleAssault(Territory, AttackingFaction,
		ETerritoryAssaultLaunchMode::StrategicCounterattack,
		false, nullptr, &OutFailureReason, false, false);
}

bool UTerritoryCounterAttackSubsystem::TryScheduleAssaultWithReason(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
	ETerritoryAssaultLaunchMode LaunchMode,
	const FTerritoryStoryPursuitOptions& StoryOptions,
	FText& OutFailureReason)
{
	return TryScheduleAssaultAdvancedWithReason(Territory, AttackingFaction,
		LaunchMode, StoryOptions, false, OutFailureReason);
}

bool UTerritoryCounterAttackSubsystem::TryScheduleAssaultAdvancedWithReason(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
	ETerritoryAssaultLaunchMode LaunchMode,
	const FTerritoryStoryPursuitOptions& StoryOptions,
	bool bStartImmediately, FText& OutFailureReason)
{
	OutFailureReason = FText::GetEmpty();
	const FTerritoryStoryPursuitOptions* Options =
		LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		? &StoryOptions : nullptr;
	return ScheduleAssault(Territory, AttackingFaction, LaunchMode,
		false, Options, &OutFailureReason, true, bStartImmediately);
}

bool UTerritoryCounterAttackSubsystem::ScheduleCounterAttack(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction)

{
	return ScheduleAssault(Territory, AttackingFaction,
		ETerritoryAssaultLaunchMode::StrategicCounterattack);
}

bool UTerritoryCounterAttackSubsystem::ScheduleStoryPursuit(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction)
{
	FTerritoryStoryPursuitOptions LegacyOptions;
	LegacyOptions.bAllowsTerritoryCapture = true;
	LegacyOptions.bUseStrategicDecisionRoll = true;
	LegacyOptions.GracePeriodOverrideGameTime = -1.f;
	return ScheduleAssault(Territory, AttackingFaction,
		ETerritoryAssaultLaunchMode::StoryPursuit, false, &LegacyOptions,
		nullptr, true, false);
}

bool UTerritoryCounterAttackSubsystem::ScheduleStoryPursuitWithOptions(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
	const FTerritoryStoryPursuitOptions& Options)
{
	return ScheduleAssault(Territory, AttackingFaction,
		ETerritoryAssaultLaunchMode::StoryPursuit, false, &Options,
		nullptr, true, false);
}

bool UTerritoryCounterAttackSubsystem::ScheduleAssault(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
	ETerritoryAssaultLaunchMode LaunchMode, bool bContinueExistingSchedule,
	const FTerritoryStoryPursuitOptions* StoryOptions,
	FText* OutFailureReason, bool bQuestOverrideAuthorized,
	bool bStartImmediately)
{
	if (OutFailureReason) *OutFailureReason = FText::GetEmpty();
	const auto Reject = [OutFailureReason](const FText& Reason)
	{
		if (OutFailureReason) *OutFailureReason = Reason;
		return false;
	};

	UWorld* World = GetWorld();
	if (!World) return Reject(NSLOCTEXT("TerritoryCounterAttack", "ScheduleNoWorld",
		"The Territory Counterattack subsystem has no World."));
	if (World->GetNetMode() == NM_Client) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "ScheduleClient", "Only the authoritative server can schedule an assault."));
	if (bRestoringState) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "ScheduleDuringCleanup", "An assault cannot be scheduled during campaign restoration or assault cleanup."));
	if (!Cast<ANarrativeGameState>(World->GetGameState())) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "MissingNarrativeGameState", "The active Game State is not a Narrative Game State."));
	if (!IsValid(Territory) || Territory->GetWorld() != World) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "ScheduleInvalidTerritory", "The target Territory is invalid, unloaded, or belongs to another World."));
	if (!Territory->HasAuthority()) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "ScheduleNoAuthority", "The target Territory is not authoritative on this machine."));
	if (Territory->GetControlMode() != ETerritoryControlMode::Independent) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "InheritedControlTarget", "The target uses inherited control. Schedule the assault against an independently controlled Place."));
	if (!Territory->IsAvailableForGameplay()) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "TargetUnavailable", "The target Territory or one of its ancestors is locked."));
	if (!Territory->GetTerritoryGUID().IsValid()) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "MissingTargetGuid", "The target Territory has no stable GUID. Assign a Territory Definition and save it."));
	if (!Territory->GetTerritoryTag().IsValid()) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "MissingTargetTag", "The target Territory has no valid Territory gameplay tag."));
	if (Territory->GetTerritoryState() != ETerritoryState::Claimed) return Reject(FText::Format(
		NSLOCTEXT("TerritoryCounterAttack", "TargetNotClaimed", "The target Territory must be Claimed before an assault can be scheduled. Current state: {0}."),
		UEnum::GetDisplayValueAsText(Territory->GetTerritoryState())));
	if (!AttackingFaction.IsValid()) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "MissingAttackerFaction", "No valid attacking faction was supplied."));
	if (!Territory->GetOwningFaction().IsValid()) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "MissingDefenderFaction", "The claimed target has no valid owning/defending faction."));
	if (Territory->GetOwningFaction() == AttackingFaction) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "AttackerOwnsTarget", "The attacking faction already owns the target Territory."));

	UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const FTerritoryFactionAssaultConfig* ForceConfig = Profile
		? Profile->FindFactionForce(AttackingFaction) : nullptr;
	const int32 RequestedForce = StoryOptions && StoryOptions->PlannedForceOverride > 0
		? StoryOptions->PlannedForceOverride : (ForceConfig ? ForceConfig->PlannedForce : 0);
	const UNPCDefinition* RequestedDefinition = StoryOptions
		&& !StoryOptions->AttackerDefinitionOverride.IsNull()
		? StoryOptions->AttackerDefinitionOverride.LoadSynchronous()
		: (ForceConfig ? ForceConfig->AttackerDefinition.Get() : nullptr);
	if (!Profile) return Reject(NSLOCTEXT("TerritoryCounterAttack", "MissingProfile",
		"The target Territory Definition has no Counterattack Profile."));
	if (!ForceConfig) return Reject(FText::Format(NSLOCTEXT("TerritoryCounterAttack", "MissingFactionForce",
		"The Counterattack Profile has no force entry for {0}."), FText::FromName(AttackingFaction.GetTagName())));
	if (RequestedForce <= 0) return Reject(NSLOCTEXT("TerritoryCounterAttack", "EmptyPlannedForce",
		"Planned Force must be greater than zero."));
	if (ForceConfig->WaveSize <= 0 && !(StoryOptions && StoryOptions->WaveSizeOverride > 0))
		return Reject(NSLOCTEXT("TerritoryCounterAttack", "EmptyWaveSize",
			"Wave Size must be greater than zero, or the Story Pursuit must provide a Wave Size Override."));
	if (ForceConfig->MilitaryPower <= 0.f) return Reject(NSLOCTEXT("TerritoryCounterAttack", "NoMilitaryPower",
		"The attacking faction's Military Power must be greater than zero."));
	if (!bQuestOverrideAuthorized
		&& !DoesForceMeetStagingRequirement(*ForceConfig, LaunchMode)) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "NoStagingDistrict", "This force requires the attacker to securely hold a complete District before it can stage an assault."));
	FText AdmissionFailure;
	if (!bQuestOverrideAuthorized
		&& !DoesFactionMeetReinforcementCapabilityRequirement(Profile,
		AttackingFaction, LaunchMode, &AdmissionFailure)) return Reject(AdmissionFailure);
	if (!bQuestOverrideAuthorized
		&& LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& !DoStrategicQuestRulesPass(Profile, AttackingFaction,
			Territory->GetOwningFaction(), &AdmissionFailure))
	{
		return Reject(AdmissionFailure);
	}
	FText SpawnClassFailure;
	if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
		RequestedDefinition, RequestedForce, SpawnClassFailure))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack definition %s is not physically spawn-ready: %s"),
			*GetNameSafe(RequestedDefinition), *SpawnClassFailure.ToString());
		return Reject(SpawnClassFailure);
	}
	FTerritoryAssaultRecord AdmissionRecord;
	AdmissionRecord.AttackingFaction = AttackingFaction;
	AdmissionRecord.DefendingFaction = Territory->GetOwningFaction();
	if (IsDiplomacyBlocked(AdmissionRecord, Territory)) return Reject(FText::Format(
		NSLOCTEXT("TerritoryCounterAttack", "DiplomacyBlocksAssault",
			"Diplomacy does not permit {0} to attack the defending faction {1}. Territory counterattacks require War."),
		FText::FromName(AttackingFaction.GetTagName()),
		FText::FromName(Territory->GetOwningFaction().GetTagName())));

	if (HasNonTerminalAssaultForTerritory(Territory)) return Reject(NSLOCTEXT(
		"TerritoryCounterAttack", "AssaultAlreadyPending", "This Territory already has a pending or active assault."));

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && (CountNonTerminalAssaults() >= Settings->MaxConcurrentScheduledAssaults
		|| CountNonTerminalAssaults(&AttackingFaction) >= Settings->MaxConcurrentAssaultsPerFaction))
	{
		return Reject(NSLOCTEXT("TerritoryCounterAttack", "AssaultBudgetFull",
			"The global or per-faction concurrent assault budget is full."));
	}

	const FTerritoryAssaultRecord* PreviousSchedule = nullptr;
	if (bContinueExistingSchedule)
	{
		for (const auto& Pair : Assaults)
		{
			const FTerritoryAssaultRecord& Candidate = Pair.Value;
			if (!Candidate.IsTerminal()
				|| Candidate.LaunchMode != LaunchMode
				|| !DoesAssaultTargetTerritory(Candidate, Territory)
				|| Candidate.AttackingFaction != AttackingFaction
				|| Candidate.DefendingFaction != Territory->GetOwningFaction()) continue;
			if (!PreviousSchedule
				|| Candidate.ResolvedGameTime > PreviousSchedule->ResolvedGameTime)
			{
				PreviousSchedule = &Candidate;
			}
		}
		if (!PreviousSchedule) return Reject(NSLOCTEXT(
			"TerritoryCounterAttack", "MissingPreviousSchedule", "A recurring assault was requested, but no matching completed schedule exists."));
	}

	const int32 EvaluationCycle = ReserveNextEvaluationCycle(
		Territory->GetTerritoryGUID(), AttackingFaction);
	if (EvaluationCycle <= 0) return Reject(NSLOCTEXT("TerritoryCounterAttack", "InvalidEvaluationCycle",
		"The scheduler could not reserve a deterministic evaluation cycle."));

	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid::NewGuid();
	Record.TargetTerritoryGUID = Territory->GetTerritoryGUID();
	Record.TargetTerritory = Territory->GetTerritoryTag();
	Record.AttackingFaction = AttackingFaction;
	Record.DefendingFaction = Territory->GetOwningFaction();
	Record.LaunchMode = LaunchMode;
	Record.bQuestOverrideAuthorized = bQuestOverrideAuthorized;
	Record.bImmediateDeployment = bStartImmediately;
	if (bStartImmediately)
	{
		// An explicit instant Wave is deterministic after admission. Strategic chance
		// belongs to the normal scheduler, not to a designer-authored immediate event.
		Record.bUseStrategicDecisionRoll = false;
	}
	if (LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit && StoryOptions)
	{
		Record.StoryPursuitDirection = StoryOptions->Direction;
		Record.bAllowsTerritoryCapture = StoryOptions->bAllowsTerritoryCapture;
		Record.bUseStrategicDecisionRoll = bStartImmediately
			? false : StoryOptions->bUseStrategicDecisionRoll;
		Record.StoryFocusLocation = StoryOptions->StoryFocusLocation;
		Record.StoryAttackerDefinitionOverride = StoryOptions->AttackerDefinitionOverride;
		Record.StoryPlannedForceOverride = FMath::Max(0, StoryOptions->PlannedForceOverride);
		Record.StoryWaveSizeOverride = FMath::Max(0, StoryOptions->WaveSizeOverride);
		Record.StoryScenarioID = StoryOptions->ScenarioID;
		Record.StoryMaximumChaseDistance = FMath::Max(0.f,
			StoryOptions->MaximumChaseDistance);
		Record.StoryChaseDistanceGraceSeconds = FMath::Max(0.f,
			StoryOptions->ChaseDistanceGraceSeconds);
		Record.bStoryAbandonDamagedVehicleForFinalFight =
			StoryOptions->bAbandonDamagedVehicleForFinalFight;
		Record.StoryVehicleAbandonHealthFraction = FMath::Clamp(
			StoryOptions->VehicleAbandonHealthFraction, 0.01f, 0.95f);
		Record.bStoryActivateRoadMissionTraffic =
			StoryOptions->bActivateRoadMissionTraffic;
		Record.StoryMissionTrafficVehicleCountOverride = FMath::Clamp(
			StoryOptions->MissionTrafficVehicleCountOverride, -1, 200);
	}
	Record.State = ETerritoryAssaultState::Grace;
	Record.EvaluationCycle = EvaluationCycle;
	if (bContinueExistingSchedule)
	{
		Record.ScheduleSeriesID = PreviousSchedule->ScheduleSeriesID.IsValid()
			? PreviousSchedule->ScheduleSeriesID : PreviousSchedule->AssaultID;
		Record.ScheduleOccurrence = FMath::Max(1, PreviousSchedule->ScheduleOccurrence) + 1;
	}
	else
	{
		Record.ScheduleSeriesID = FGuid::NewGuid();
		Record.ScheduleOccurrence = 1;
	}
	Record.DecisionSeed = MakeDecisionSeed(Territory, AttackingFaction, EvaluationCycle);
	Record.CapturedGameTime = GetCampaignGameTime();
	const float BaseGrace = LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		&& StoryOptions && StoryOptions->GracePeriodOverrideGameTime >= 0.f
		? StoryOptions->GracePeriodOverrideGameTime : Profile->GracePeriodGameTime;
	Record.GraceEndsGameTime = Record.CapturedGameTime + CalculateInfluenceAdjustedDelay(
		BaseGrace, ForceConfig->TerritorialInfluence,
		Profile->MinimumInfluenceTimingScale);
	Assaults.Add(Record.AssaultID, Record);
	if (Settings && Settings->ShouldDebugCounterAttacks())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[CounterAttack] scheduled %s series=%s occurrence=%d target=%s attacker=%s defender=%s mode=%d graceEnd=%.2f force=%d"),
			*Record.AssaultID.ToString(), *Record.ScheduleSeriesID.ToString(),
			Record.ScheduleOccurrence, *Record.TargetTerritory.ToString(),
			*Record.AttackingFaction.ToString(), *Record.DefendingFaction.ToString(),
			static_cast<int32>(Record.LaunchMode), Record.GraceEndsGameTime,
			RequestedForce);
	}
	BroadcastChanged(Record);
	const FTerritoryAssaultRecord* Scheduled = Assaults.Find(Record.AssaultID);
	if (!Scheduled || Scheduled->IsTerminal())
	{
		return Reject(NSLOCTEXT("TerritoryCounterAttack", "ScheduleSuperseded",
			"The assault was cancelled or replaced by a scheduling callback."));
	}
	if (bStartImmediately)
	{
		FTerritoryAssaultRecord* Added = Assaults.Find(Record.AssaultID);
		if (!Added || !StartAssaultImmediately(*Added, Territory,
			OutFailureReason))
		{
			return false;
		}
	}
	if (OutFailureReason)
	{
		*OutFailureReason = NSLOCTEXT("TerritoryCounterAttack", "AssaultScheduled",
			"The finite physical assault was scheduled successfully.");
	}
	return true;
}

bool UTerritoryCounterAttackSubsystem::StartAssaultImmediately(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory,
	FText* OutFailureReason)
{
	if (!Territory || Assault.IsTerminal()) return false;
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	if (Assault.State == ETerritoryAssaultState::Grace)
	{
		const ETerritoryAssaultState PreviousState = Assault.State;
		Assault.State = ETerritoryAssaultState::Evaluating;
		BroadcastStateTransition(Assault, PreviousState);
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Evaluating) || !IsValid(Territory)) return false;
		EvaluateAssault(Assault, Territory);
		if (!IsAssaultCurrent(Access)) return false;
	}
	if (Assault.IsTerminal())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FText::Format(NSLOCTEXT(
				"TerritoryCounterAttack", "ImmediateEvaluationFailed",
				"The immediate assault failed during route/force evaluation. Outcome: {0}."),
				UEnum::GetDisplayValueAsText(Assault.Resolution));
		}
		return false;
	}
	if (Assault.State == ETerritoryAssaultState::ScheduledWarning)
	{
		NotifyRelevantPlayers(Assault, Territory);
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::ScheduledWarning) || !IsValid(Territory)) return false;
		const ETerritoryAssaultState PreviousState = Assault.State;
		Assault.State = ETerritoryAssaultState::WaitingForPlayerProximity;
		BroadcastStateTransition(Assault, PreviousState);
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::WaitingForPlayerProximity) || !IsValid(Territory)) return false;
	}
	if (Assault.State == ETerritoryAssaultState::WaitingForPlayerProximity
		&& ActivateAssault(Assault, Territory))
	{
		return true;
	}
	if (OutFailureReason)
	{
		*OutFailureReason = NSLOCTEXT("TerritoryCounterAttack",
			"ImmediateActivationFailed",
			"The assault passed admission but could not deploy immediately. Check its approaches and physical route.");
	}
	return false;
}

bool UTerritoryCounterAttackSubsystem::CancelAssault(
	FGuid AssaultID, ETerritoryAssaultResolution Reason)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return false;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	if (!Assault || Assault->IsTerminal()) return false;
	ResolveAssault(*Assault, ETerritoryAssaultState::Cancelled, Reason);
	// A synchronous story/load callback may supersede the cancellation.
	const FTerritoryAssaultRecord* Final = Assaults.Find(AssaultID);
	return Final && Final->State == ETerritoryAssaultState::Cancelled
		&& Final->Resolution == Reason;
}

bool UTerritoryCounterAttackSubsystem::GetAssault(
	FGuid AssaultID, FTerritoryAssaultRecord& OutAssault) const
{
	if (const FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID))
	{
		OutAssault = *Assault;
		return true;
	}
	return false;
}

TArray<FTerritoryAssaultRecord> UTerritoryCounterAttackSubsystem::GetAllAssaults() const
{
	TArray<FTerritoryAssaultRecord> Result;
	Assaults.GenerateValueArray(Result);
	Result.Sort([](const FTerritoryAssaultRecord& A, const FTerritoryAssaultRecord& B)
	{
		return A.CapturedGameTime < B.CapturedGameTime;
	});
	return Result;
}

TArray<FTerritoryAssaultRecord> UTerritoryCounterAttackSubsystem::GetAssaultsForTerritory(
	FGameplayTag TerritoryTag) const
{
	TArray<FTerritoryAssaultRecord> Result;
	for (const auto& Pair : Assaults)
	{
		if (Pair.Value.TargetTerritory == TerritoryTag) Result.Add(Pair.Value);
	}
	Result.Sort([](const FTerritoryAssaultRecord& A, const FTerritoryAssaultRecord& B)
	{
		return A.CapturedGameTime < B.CapturedGameTime;
	});
	return Result;
}

TArray<FTerritoryAssaultRecord> UTerritoryCounterAttackSubsystem::GetAssaultsForTerritoryActor(
	const ATerritoryVolume* Territory) const
{
	TArray<FTerritoryAssaultRecord> Result;
	if (!Territory) return Result;
	for (const auto& Pair : Assaults)
	{
		if (DoesAssaultTargetTerritory(Pair.Value, Territory))
		{
			Result.Add(Pair.Value);
		}
	}
	Result.Sort([](const FTerritoryAssaultRecord& A, const FTerritoryAssaultRecord& B)
	{
		return A.CapturedGameTime < B.CapturedGameTime;
	});
	return Result;
}

bool UTerritoryCounterAttackSubsystem::IsAssaultActive(FGuid AssaultID) const
{
	const FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	return Assault && (Assault->State == ETerritoryAssaultState::Active
		|| Assault->State == ETerritoryAssaultState::RecaptureCountdown);
}

bool UTerritoryCounterAttackSubsystem::IsAssaultPendingOrActive(FGuid AssaultID) const
{
	const FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	return Assault && !Assault->IsTerminal();
}

FString UTerritoryCounterAttackSubsystem::GetAssaultDebugString(FGuid AssaultID) const
{
	const FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	if (!Assault) return TEXT("Assault not found");
	const FTerritoryAssaultEvaluationInput& Input = Assault->EvaluationInput;
	const FTerritoryAssaultEvaluationResult& Result = Assault->EvaluationResult;
	const ATerritoryVolume* Territory = ResolveTerritory(*Assault);
	const UTerritoryCounterAttackProfile* Profile = Territory
		? Territory->GetCounterAttackProfile() : nullptr;
	const bool bRelevantPlayerNearby = Territory && Profile
		&& HasRelevantPlayerNearby(*Assault, Territory, Profile->ActivationRadius);
	const UEnum* StateEnum = StaticEnum<ETerritoryAssaultState>();
	const UEnum* ResolutionEnum = StaticEnum<ETerritoryAssaultResolution>();
	const UEnum* LaunchModeEnum = StaticEnum<ETerritoryAssaultLaunchMode>();
	const FString StateName = StateEnum
		? StateEnum->GetNameStringByValue(static_cast<int64>(Assault->State)) : TEXT("Unknown");
	const FString ResolutionName = ResolutionEnum
		? ResolutionEnum->GetNameStringByValue(static_cast<int64>(Assault->Resolution)) : TEXT("Unknown");
	const FString LaunchModeName = LaunchModeEnum
		? LaunchModeEnum->GetNameStringByValue(static_cast<int64>(Assault->LaunchMode)) : TEXT("Unknown");
	return FString::Printf(
		TEXT("Target=%s Attack=%s Defend=%s LaunchMode=%s Schedule=%s#%d SecureDistricts=%d Guards=%d/%d/%d Reserve=%d AttackPower=%.2f Influence=%.2f DefencePower=%.2f PowerRatio=%.3f Strategic=%.2f Priority=%.3f LaunchP=%.3f SuccessP=%.3f Force(P/A/R/K/W)=%d/%d/%d/%d/%d SpawnFailures=%d Approaches=%s State=%s Notification=%s Proximity=%s GraceEnd=%.2f Resolved=%.2f Roll=%.4f Reason=%s"),
		*Assault->TargetTerritory.ToString(), *Assault->AttackingFaction.ToString(),
		*Assault->DefendingFaction.ToString(), *LaunchModeName,
		*Assault->ScheduleSeriesID.ToString(), Assault->ScheduleOccurrence,
		GetSecureDistrictCountForFaction(Assault->AttackingFaction),
		Input.ActiveGuards, Input.DesiredGuards,
		Input.MaximumGuards, Input.ReserveGuards, Input.AttackingMilitaryPower,
		Input.FactionInfluence,
		Result.DistrictDefencePower, Result.PowerRatio, Input.StrategicValue,
		Result.AttackPriority, Result.LaunchProbability, Result.EstimatedSuccessProbability,
		Assault->PlannedForce, Assault->AliveForce, Assault->PendingReserveForce,
		Assault->KilledForce, Assault->WithdrawnForce, Assault->ConsecutiveSpawnFailures,
		*FString::JoinBy(Assault->SelectedApproaches, TEXT(","),
			[](FName Name) { return Name.ToString(); }), *StateName,
		Assault->bNotificationSent ? TEXT("sent") : TEXT("pending"),
		bRelevantPlayerNearby ? TEXT("inside") : TEXT("outside"), Assault->GraceEndsGameTime,
		Assault->ResolvedGameTime, Assault->DecisionRoll, *ResolutionName);
}

int32 UTerritoryCounterAttackSubsystem::GetSecureDistrictCountForFaction(
	FGameplayTag Faction) const
{
	return UTerritoryBlueprintLibrary::GetFactionDistrictCount(this, Faction);
}

bool UTerritoryCounterAttackSubsystem::CanFactionStageStrategicCounterAttack(
	FGameplayTag Faction) const
{
	return GetSecureDistrictCountForFaction(Faction) > 0;
}

bool UTerritoryCounterAttackSubsystem::DoesForceMeetStagingRequirement(
	const FTerritoryFactionAssaultConfig& ForceConfig,
	ETerritoryAssaultLaunchMode LaunchMode) const
{
	if (ForceConfig.StagingRequirement == ETerritoryAssaultStagingRequirement::None
		|| CanFactionStageStrategicCounterAttack(ForceConfig.Faction))
	{
		return true;
	}
	return LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		&& ForceConfig.bAllowStoryPursuitWithoutStagingDistrict;
}

bool UTerritoryCounterAttackSubsystem::DoesFactionMeetReinforcementCapabilityRequirement(
	const UTerritoryCounterAttackProfile* Profile, const FGameplayTag& Faction,
	const ETerritoryAssaultLaunchMode LaunchMode, FText* OutFailureReason) const
{
	if (OutFailureReason) *OutFailureReason = FText::GetEmpty();
	if (LaunchMode != ETerritoryAssaultLaunchMode::StrategicCounterattack
		|| !Profile
		|| !Profile->bRequireReinforcementCapabilityForStrategicCounterattacks)
	{
		return true;
	}

	FText FailureReason;
	const bool bAllowed = UTerritoryBlueprintLibrary::CanFactionUseCommandCapability(
		this, Faction, TerritoryCommandTags::Reinforcements, FailureReason);
	if (!bAllowed && OutFailureReason)
	{
		*OutFailureReason = FailureReason.IsEmpty()
			? NSLOCTEXT("TerritoryCounterAttack", "MissingReinforcementCapability",
				"The attacking faction does not hold the Reinforcements Territory capability.")
			: FailureReason;
	}
	return bAllowed;
}

bool UTerritoryCounterAttackSubsystem::DoStrategicQuestRulesPass(
	const UTerritoryCounterAttackProfile* Profile,
	const FGameplayTag& AttackingFaction, const FGameplayTag& DefendingFaction,
	FText* OutFailureReason) const
{
	if (OutFailureReason) *OutFailureReason = FText::GetEmpty();
	if (!Profile) return false;
	if (Profile->QuestRules.IsEmpty()) return true;

	UWorld* World = GetWorld();
	if (!World) return false;
	for (const FTerritoryCounterAttackQuestRule& Rule : Profile->QuestRules)
	{
		if (!Rule.QuestClass)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT("TerritoryCounterAttack", "QuestRuleMissingQuest",
					"A counterattack quest rule has no Narrative Quest selected.");
			}
			return false;
		}

		bool bAnyScopedPlayerMatches = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const ANarrativePlayerController* PC =
				Cast<ANarrativePlayerController>(It->Get());
			if (!PC) continue;

			bool bInScope = Rule.PlayerScope
				== ETerritoryCounterQuestPlayerScope::AnyOnlinePlayer;
			if (!bInScope)
			{
				const ANarrativePlayerState* PS =
					PC->GetPlayerState<ANarrativePlayerState>();
				const FGameplayTag ScopedFaction = Rule.PlayerScope
					== ETerritoryCounterQuestPlayerScope::AttackingFactionPlayers
					? AttackingFaction : DefendingFaction;
				bInScope = PS && ScopedFaction.IsValid()
					&& PS->GetFactions().HasTagExact(ScopedFaction);
			}
			if (!bInScope) continue;

			if (UTerritoryQuestRulesLibrary::DoesQuestStateMatch(
				PC->GetTalesComponent(), Rule.QuestClass, Rule.QuestState))
			{
				bAnyScopedPlayerMatches = true;
				break;
			}
		}

		if (!UTerritoryCounterAttackProfile::DoesQuestRulePass(
			Rule.Action, bAnyScopedPlayerMatches))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = Rule.Action
					== ETerritoryCounterQuestRuleAction::BlockWhenMatched
					? NSLOCTEXT("TerritoryCounterAttack", "QuestRuleMatchedBlock",
						"An online player's Narrative quest state blocks this strategic counterattack.")
					: NSLOCTEXT("TerritoryCounterAttack", "QuestRuleRequiredMissing",
						"No scoped online player currently matches the required Narrative quest state.");
			}
			return false;
		}
	}
	return true;
}

TArray<FTerritoryAssaultRecord> UTerritoryCounterAttackSubsystem::GetPersistentState() const
{
	return GetAllAssaults();
}

TArray<FTerritoryAssaultCycleRecord> UTerritoryCounterAttackSubsystem::GetPersistentCycleState() const
{
	TArray<FTerritoryAssaultCycleRecord> Result;
	for (const auto& TerritoryPair : EvaluationCycleHighWater)
	{
		for (const auto& FactionPair : TerritoryPair.Value)
		{
			if (!TerritoryPair.Key.IsValid() || !FactionPair.Key.IsValid() || FactionPair.Value <= 0)
			{
				continue;
			}
			FTerritoryAssaultCycleRecord Record;
			Record.TargetTerritoryGUID = TerritoryPair.Key;
			Record.AttackingFaction = FactionPair.Key;
			Record.HighestEvaluationCycle = FactionPair.Value;
			Result.Add(Record);
		}
	}
	Result.Sort([](const FTerritoryAssaultCycleRecord& A,
		const FTerritoryAssaultCycleRecord& B)
	{
		const FString AGuid = A.TargetTerritoryGUID.ToString(EGuidFormats::Digits);
		const FString BGuid = B.TargetTerritoryGUID.ToString(EGuidFormats::Digits);
		return AGuid == BGuid
			? A.AttackingFaction.ToString() < B.AttackingFaction.ToString()
			: AGuid < BGuid;
	});
	return Result;
}

void UTerritoryCounterAttackSubsystem::RestorePersistentState(
	const TArray<FTerritoryAssaultRecord>& Records)
{
	RestorePersistentState(Records, {});
}

void UTerritoryCounterAttackSubsystem::RestorePersistentState(
	const TArray<FTerritoryAssaultRecord>& Records,
	const TArray<FTerritoryAssaultCycleRecord>& CycleRecords)
{
	UWorld* World = GetWorld();
	if (!World) return;
	const uint64 ThisRestoreGeneration = ++RestoreGeneration;

	const bool bAuthority = World->GetNetMode() != NM_Client;
	TGuardValue<bool> RestoreGuard(bRestoringState, true);
	if (bAuthority)
	{
		// Terminal assaults can already have cars awaiting ordinary scenic cleanup.
		// They must not obstruct the replacement campaign's physical reconstruction.
		const TArray<FRetiringVehicle> PreviousRetiringVehicles = MoveTemp(RetiringVehicles);
		RetiringVehicles.Reset();
		for (const FRetiringVehicle& Entry : PreviousRetiringVehicles)
		{
			RemoveVehicleForCampaignRestore(Entry.Vehicle.Get());
		}
		if (RestoreGeneration != ThisRestoreGeneration) return;
		TArray<FGuid> PreviousAssaultIDs;
		Assaults.GetKeys(PreviousAssaultIDs);
		for (const FGuid& PreviousID : PreviousAssaultIDs)
		{
			if (const FTerritoryAssaultRecord* Previous = Assaults.Find(PreviousID))
			{
				FTerritoryAssaultRecord Snapshot = *Previous;
				RetireLiveParticipants(Snapshot, true);
				if (RestoreGeneration != ThisRestoreGeneration) return;
				RetireLiveVehicles(PreviousID, true, true);
				if (RestoreGeneration != ThisRestoreGeneration) return;
			}
		}
	}
	LiveParticipants.Empty();
	LiveAssaultVehicles.Empty();
	LiveVehicleRetirementRules.Empty();
	const auto PreviousRoadGuides = MoveTemp(LiveAssaultRoadGuides);
	LiveAssaultRoadGuides.Reset();
	for (const auto& Pair : PreviousRoadGuides)
	{
		for (const TWeakObjectPtr<ATerritoryRoadGuide>& WeakGuide : Pair.Value)
		{
			if (ATerritoryRoadGuide* Guide = WeakGuide.Get()) Guide->EndMissionTraffic();
		}
	}
	if (RestoreGeneration != ThisRestoreGeneration) return;
	WarnedControllers.Empty();
	Assaults.Empty();
	EvaluationCycleHighWater.Empty();

	for (FTerritoryAssaultRecord Record : Records)
	{
		if (!Record.AssaultID.IsValid()
			|| (!Record.TargetTerritoryGUID.IsValid() && !Record.TargetTerritory.IsValid()))
		{
			continue;
		}
		if (Record.IsTerminal() && Record.ResolvedGameTime <= 0.0)
		{
			// Bounded migration for records saved before terminal timestamps existed.
			// Starting the cooldown now avoids an immediate recurring reroll on load.
			Record.ResolvedGameTime = GetCampaignGameTime();
		}
		// Bounded migration for saves created before schedule-series identity existed.
		if (!Record.ScheduleSeriesID.IsValid())
		{
			Record.ScheduleSeriesID = Record.AssaultID;
		}
		Record.ScheduleOccurrence = FMath::Max(1, Record.ScheduleOccurrence);
		Record.PlannedForce = FMath::Max(0, Record.PlannedForce);
		Record.ConsecutiveSpawnFailures = FMath::Max(0, Record.ConsecutiveSpawnFailures);
		Record.KilledForce = FMath::Clamp(Record.KilledForce, 0, Record.PlannedForce);
		Record.WithdrawnForce = FMath::Clamp(
			Record.WithdrawnForce, 0, Record.PlannedForce - Record.KilledForce);
		if (bAuthority && (Record.State == ETerritoryAssaultState::Active
			|| Record.State == ETerritoryAssaultState::RecaptureCountdown))
		{
			// Live assault pawns are intentionally not campaign state. Reconstruct only
			// saved survivors; casualties remain consumed and the decision is not rerolled.
			Record.AliveForce = 0;
			Record.PendingReserveForce = FMath::Max(0,
				Record.PlannedForce - Record.KilledForce - Record.WithdrawnForce);
		}
		else
		{
			const int32 MaximumRemaining = FMath::Max(0,
				Record.PlannedForce - Record.KilledForce - Record.WithdrawnForce);
			Record.AliveForce = FMath::Clamp(Record.AliveForce, 0, MaximumRemaining);
			Record.PendingReserveForce = FMath::Clamp(
				Record.PendingReserveForce, 0, MaximumRemaining - Record.AliveForce);
		}
		Assaults.Add(Record.AssaultID, Record);
		if (Record.TargetTerritoryGUID.IsValid() && Record.AttackingFaction.IsValid()
			&& Record.EvaluationCycle > 0)
		{
			int32& HighWater = EvaluationCycleHighWater.FindOrAdd(
				Record.TargetTerritoryGUID).FindOrAdd(Record.AttackingFaction);
			HighWater = FMath::Max(HighWater, Record.EvaluationCycle);
		}
	}
	for (const FTerritoryAssaultCycleRecord& Cycle : CycleRecords)
	{
		if (!Cycle.TargetTerritoryGUID.IsValid() || !Cycle.AttackingFaction.IsValid()
			|| Cycle.HighestEvaluationCycle <= 0)
		{
			continue;
		}
		int32& HighWater = EvaluationCycleHighWater.FindOrAdd(
			Cycle.TargetTerritoryGUID).FindOrAdd(Cycle.AttackingFaction);
		HighWater = FMath::Max(HighWater, Cycle.HighestEvaluationCycle);
	}
}

void UTerritoryCounterAttackSubsystem::UpdateAssaults()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || bRestoringState || bUpdatingAssaults) return;
	TGuardValue<bool> UpdatingGuard(bUpdatingAssaults, true);

	UpdateRetiringVehicles();
	TArray<FGuid> IDs;
	Assaults.GetKeys(IDs);
	IDs.Sort([](const FGuid& A, const FGuid& B) { return A.ToString() < B.ToString(); });
	for (const FGuid& ID : IDs)
	{
		if (FTerritoryAssaultRecord* Assault = Assaults.Find(ID))
		{
			AdvanceAssault(*Assault);
		}
	}
	TryScheduleRecurringStrategicAssaults();
	TrimTerminalHistory();
}

void UTerritoryCounterAttackSubsystem::TryScheduleRecurringStrategicAssaults()
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return;

	TArray<ATerritoryVolume*> Targets = Registry->GetAllTerritories();
	Targets.Sort([](const ATerritoryVolume& A, const ATerritoryVolume& B)
	{
		return A.GetTerritoryTag().ToString() < B.GetTerritoryTag().ToString();
	});
	for (ATerritoryVolume* Target : Targets)
	{
		if (!Target || Target->GetControlMode() != ETerritoryControlMode::Independent
			|| !Target->IsAvailableForGameplay()
			|| Target->GetTerritoryState() != ETerritoryState::Claimed
			|| !Target->GetOwningFaction().IsValid()
			|| Target->IsPrimaryRuntimeRuleSuspendedWithContext(
				ETerritoryQuestOverrideEffect::AutomaticCounterattacks, nullptr))
		{
			continue;
		}
		if (HasNonTerminalAssaultForTerritory(Target))
		{
			continue;
		}

		FGameplayTag AttackingFaction;
		FTerritoryAssaultEvaluationInput Input;
		FTerritoryAssaultEvaluationResult Result;
		FText Reason;
		if (FindBestEligibleAttacker(Target, FGameplayTag(), AttackingFaction,
			Input, Result, Reason, true))
		{
			ScheduleAssault(Target, AttackingFaction,
				ETerritoryAssaultLaunchMode::StrategicCounterattack, true);
		}
	}
}

UTerritoryCounterAttackSubsystem::FAssaultAccess
UTerritoryCounterAttackSubsystem::CaptureAssaultAccess(const FTerritoryAssaultRecord& Assault) const
{
	return {Assault.AssaultID, &Assault, RestoreGeneration};
}

bool UTerritoryCounterAttackSubsystem::IsAssaultCurrent(const FAssaultAccess& Access) const
{
	return Access.Generation == RestoreGeneration && Assaults.Find(Access.ID) == Access.Address;
}

bool UTerritoryCounterAttackSubsystem::IsAssaultCurrent(
	const FAssaultAccess& Access, ETerritoryAssaultState ExpectedState) const
{
	return IsAssaultCurrent(Access) && Access.Address->State == ExpectedState;
}

void UTerritoryCounterAttackSubsystem::AdvanceAssault(FTerritoryAssaultRecord& Assault)
{
	if (Assault.IsTerminal()) return;
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	const ETerritoryAssaultState InitialState = Assault.State;
	ATerritoryVolume* Territory = ResolveTerritory(Assault);
	if (!Territory) return; // World Partition: wait for authoritative actor registration.
	if (Territory->GetControlMode() != ETerritoryControlMode::Independent
		|| !Territory->IsAvailableForGameplay())
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::InvalidTerritory);
		return;
	}
	if (ReconcileAssaultTargetIdentity(Assault, Territory))
	{
		// Publish the bounded save migration before any later validation can resolve the assault.
		BroadcastChanged(Assault);
		if (!IsAssaultCurrent(Access, InitialState) || !IsValid(Territory)) return;
	}
	const FGameplayTag ContestingFaction =
		Territory->GetOwnershipData().ContestingFaction;
	if (!IsTerritoryControlStateValidForAssault(
		Assault.State, Territory->GetTerritoryState(), ContestingFaction,
		Assault.AttackingFaction))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::InvalidTerritory);
		return;
	}

	UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const FTerritoryFactionAssaultConfig* ForceConfig = Profile
		? Profile->FindFactionForce(Assault.AttackingFaction) : nullptr;
	FText SpawnClassFailure;
	if (!Profile || !ForceConfig
		|| !ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
			ResolveAssaultDefinition(Assault, *ForceConfig),
			ResolveAssaultPlannedForce(Assault, *ForceConfig), SpawnClassFailure))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}
	const bool bPhysicalAssault = Assault.State == ETerritoryAssaultState::Active
		|| Assault.State == ETerritoryAssaultState::RecaptureCountdown;
	if (!Assault.bQuestOverrideAuthorized
		&& Assault.LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& !bPhysicalAssault
		&& !DoesForceMeetStagingRequirement(*ForceConfig, Assault.LaunchMode))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::StagingDistrictUnavailable);
		return;
	}
	const bool bPhysicalAssaultAlreadyActive =
		Assault.State == ETerritoryAssaultState::Active
		|| Assault.State == ETerritoryAssaultState::RecaptureCountdown;
	if (!Assault.bQuestOverrideAuthorized && !bPhysicalAssaultAlreadyActive
		&& Territory->IsPrimaryRuntimeRuleSuspendedWithContext(
			ETerritoryQuestOverrideEffect::AutomaticCounterattacks, nullptr))
	{
		// Freeze grace/warning/waiting. Campaign time may pass, so the primary
		// schedule can continue promptly after the Quest finishes. A battle that
		// is already physical is never despawned by a story-state change.
		return;
	}
	if (!Assault.bQuestOverrideAuthorized
		&& Assault.LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& !bPhysicalAssault
		&& !DoesFactionMeetReinforcementCapabilityRequirement(Profile,
			Assault.AttackingFaction, Assault.LaunchMode))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ReinforcementCapabilityLost);
		return;
	}
	if (!Assault.bQuestOverrideAuthorized
		&& Assault.LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& !bPhysicalAssault
		&& !DoStrategicQuestRulesPass(Profile, Assault.AttackingFaction,
			Assault.DefendingFaction))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::QuestRuleBlocked);
		return;
	}

	if (Territory->GetOwningFaction() != Assault.DefendingFaction)
	{
		if (Territory->GetOwningFaction() == Assault.AttackingFaction)
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Succeeded,
				ETerritoryAssaultResolution::CaptureCompleted);
		}
		else
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
				ETerritoryAssaultResolution::OwnershipChanged);
		}
		return;
	}
	if (IsDiplomacyBlocked(Assault, Territory))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::DiplomacyBlocked);
		return;
	}

	switch (Assault.State)
	{
	case ETerritoryAssaultState::Evaluating:
		// A save or a callback that relocates the map may interrupt the Grace transition.
		// Resume the same seeded evaluation instead of leaving a durable record stuck.
		EvaluateAssault(Assault, Territory);
		break;
	case ETerritoryAssaultState::Grace:
		if (GetCampaignGameTime() >= Assault.GraceEndsGameTime
			&& (Assault.LaunchMode != ETerritoryAssaultLaunchMode::StrategicCounterattack
				|| IsForceTimeWindowOpen(*ForceConfig)))
		{
			const ETerritoryAssaultState PreviousState = Assault.State;
			Assault.State = ETerritoryAssaultState::Evaluating;
			BroadcastStateTransition(Assault, PreviousState);
			if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Evaluating) || !IsValid(Territory)) return;
			EvaluateAssault(Assault, Territory);
		}
		break;
	case ETerritoryAssaultState::ScheduledWarning:
		NotifyRelevantPlayers(Assault, Territory);
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::ScheduledWarning) || !IsValid(Territory)) return;
		{
			const ETerritoryAssaultState PreviousState = Assault.State;
			Assault.State = ETerritoryAssaultState::WaitingForPlayerProximity;
			BroadcastStateTransition(Assault, PreviousState);
		}
		break;
	case ETerritoryAssaultState::WaitingForPlayerProximity:
		NotifyRelevantPlayers(Assault, Territory);
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::WaitingForPlayerProximity) || !IsValid(Territory)) return;
		if (Profile)
		{
			const bool bRelevantPlayerNearby = !Profile->bRequirePlayerProximityForActivation
				|| HasRelevantPlayerNearby(Assault, Territory, Profile->ActivationRadius);
			if (ShouldActivateWaitingAssault(Assault.bAllowsTerritoryCapture,
				Territory->GetTerritoryState(), Profile->bRequirePlayerProximityForActivation,
				bRelevantPlayerNearby))
			{
				ActivateAssault(Assault, Territory);
			}
		}
		break;
	case ETerritoryAssaultState::Active:
	case ETerritoryAssaultState::RecaptureCountdown:
		if (Assault.AliveForce + Assault.PendingReserveForce <= 0)
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Defeated,
				ETerritoryAssaultResolution::AllAttackersRemoved);
			return;
		}
		if (Profile && Assault.State == ETerritoryAssaultState::Active)
		{
			const bool bRelevantPlayerNearby = HasRelevantPlayerNearby(
				Assault, Territory, Profile->ActivationRadius);
			const bool bVehicleOnlyForce =
				UsesOnlyNarrativeVehicleApproaches(Assault, Territory);
			if (ShouldDeployActiveReserveWave(Assault, bRelevantPlayerNearby,
				Profile->bContinueFiniteWavesAfterActivation, bVehicleOnlyForce))
			{
				SpawnNextWave(Assault, Territory);
				if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Active) || !IsValid(Territory)) return;
			}
		}

		if (!Assault.bAllowsTerritoryCapture)
		{
			// Optional boss/chase encounters may fight in a Territory but can never
			// start or finish strategic recapture. Repair an old in-memory/save state
			// defensively if it was produced before this capture-policy guard existed.
			if (Assault.State == ETerritoryAssaultState::RecaptureCountdown)
			{
				const ETerritoryAssaultState PreviousState = Assault.State;
				Assault.State = ETerritoryAssaultState::Active;
				Assault.RecaptureEndsGameTime = 0.0;
				BroadcastStateTransition(Assault, PreviousState);
			}
			break;
		}

		if (Profile)
		{
			bool bDefendersRemain = !TerritoryAssaultTargetPolicy::
				CollectRegisteredDefenders(Territory).IsEmpty();
			if (!bDefendersRemain)
			{
				for (ATerritoryVolume* Defence :
					TerritoryAssaultTargetPolicy::BuildDefenceFront(Territory))
				{
					if (!Defence || (Defence != Territory
						&& Defence->GetOwningFaction() != Assault.DefendingFaction))
					{
						continue;
					}
					for (const ATerritoryGuardSpawnPoint* Post : Defence->GetGuardSpawnPoints())
					{
						if (Post && Post->HasPendingReserveSpawn())
						{
							bDefendersRemain = true;
							break;
						}
					}
					if (bDefendersRemain) break;
				}
			}

			bool bAliveDefendingPlayerInside = false;
			bool bDeadDefendingPlayerInside = false;
			GetDefendingPlayerPresence(Assault, Territory,
				bAliveDefendingPlayerInside, bDeadDefendingPlayerInside);
			const double Now = GetCampaignGameTime();
			const ERecaptureDecision Decision = EvaluateRecaptureDecision(
				bDefendersRemain, HasPhysicalAttackerInside(Assault, Territory),
				bAliveDefendingPlayerInside, bDeadDefendingPlayerInside,
				Assault.State == ETerritoryAssaultState::RecaptureCountdown,
				Assault.RecaptureEndsGameTime > 0.0 && Now >= Assault.RecaptureEndsGameTime,
				Profile->bUseUnattendedRecaptureHandover,
				Profile->bConcedeWhenDefendingPlayerDies);

			switch (Decision)
			{
			case ERecaptureDecision::StartCountdown:
			{
				const ETerritoryAssaultState PreviousState = Assault.State;
				Assault.State = ETerritoryAssaultState::RecaptureCountdown;
				Assault.RecaptureEndsGameTime = Now
					+ FMath::Max(1.f, Profile->UnattendedRecaptureDelayGameTime);
				BroadcastStateTransition(Assault, PreviousState);
				break;
			}
			case ERecaptureDecision::CompleteHandover:
				CompleteRecapture(Assault, Territory);
				return;
			case ERecaptureDecision::ContinueFight:
			case ERecaptureDecision::NoAction:
				if (Assault.State == ETerritoryAssaultState::RecaptureCountdown
					&& (bDefendersRemain || bAliveDefendingPlayerInside
						|| !HasPhysicalAttackerInside(Assault, Territory)))
				{
					const ETerritoryAssaultState PreviousState = Assault.State;
					Assault.State = ETerritoryAssaultState::Active;
					Assault.RecaptureEndsGameTime = 0.0;
					BroadcastStateTransition(Assault, PreviousState);
				}
				break;
			}
		}
		break;
	default:
		break;
	}
}

void UTerritoryCounterAttackSubsystem::EvaluateAssault(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	UTerritoryCounterAttackProfile* Profile = Territory ? Territory->GetCounterAttackProfile() : nullptr;
	const FTerritoryFactionAssaultConfig* ForceConfig = Profile
		? Profile->FindFactionForce(Assault.AttackingFaction) : nullptr;
	UNPCDefinition* AttackerDefinition = ForceConfig
		? ResolveAssaultDefinition(Assault, *ForceConfig) : nullptr;
	const int32 PlannedForce = ForceConfig
		? ResolveAssaultPlannedForce(Assault, *ForceConfig) : 0;
	if (!Profile || !ForceConfig || !AttackerDefinition || PlannedForce <= 0)
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}
	FText SpawnClassFailure;
	if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
		AttackerDefinition, PlannedForce, SpawnClassFailure))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}

	Assault.EvaluationInput = BuildEvaluationInput(Territory, *ForceConfig);
	Assault.EvaluationResult = CalculateEvaluation(Assault.EvaluationInput, Profile);
	Assault.SelectedApproaches = SelectValidApproaches(
		Territory, Profile, Assault.EvaluationResult.PowerRatio);
	Assault.SelectedApproaches.RemoveAll(
		[Territory, ForceConfig](const FName ApproachID)
		{
			const FTerritoryAssaultApproach* Approach =
				Territory->GetCounterAttackApproaches().FindByPredicate(
					[ApproachID](const FTerritoryAssaultApproach& Candidate)
					{
						return Candidate.ApproachID == ApproachID;
					});
			if (!Approach || Approach->EntryType !=
				ETerritoryAssaultEntryType::NarrativeVehicle)
			{
				return false;
			}
			UClass* VehicleClass =
				UTerritoryCounterAttackProfile::ResolveVehicleClass(
					*ForceConfig, *Approach).LoadSynchronous();
			return !VehicleClass
				|| !VehicleClass->IsChildOf(ANarrativeVehicleBase::StaticClass());
		});
	if (Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		&& Assault.StoryPursuitDirection ==
			ETerritoryStoryPursuitDirection::PlayerChasesEnemy)
	{
		Assault.SelectedApproaches.RemoveAll([Territory](const FName ApproachID)
		{
			for (const FTerritoryAssaultApproach& Candidate :
				Territory->GetCounterAttackApproaches())
			{
				if (Candidate.ApproachID == ApproachID)
				{
					return Candidate.EntryType !=
						ETerritoryAssaultEntryType::NarrativeVehicle;
				}
			}
			return true;
		});
	}
	if (Assault.SelectedApproaches.IsEmpty())
	{
		for (const FTerritoryAssaultApproach& Approach : Territory->GetCounterAttackApproaches())
		{
			if (!Approach.bEnabled) continue;
			const FTransform WorldTransform =
				Approach.RelativeSpawnTransform * Territory->GetActorTransform();
			FString RouteFailure;
			FVector Objective;
			FTransform FootDeploymentTransform;
			ResolveApproachObjective(Territory, Approach, WorldTransform,
				Objective, FootDeploymentTransform, nullptr, &RouteFailure);
			UE_LOG(LogTerritory, Warning,
				TEXT("[CounterAttack] %s approach '%s' is unusable at %s: %s"),
				*Territory->GetTerritoryTag().ToString(), *Approach.ApproachID.ToString(),
				*WorldTransform.GetLocation().ToCompactString(), *RouteFailure);
		}
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::InvalidApproachOrRoute);
		return;
	}

	FRandomStream DecisionStream(Assault.DecisionSeed);
	Assault.DecisionRoll = DecisionStream.GetFraction();
	if (Assault.bUseStrategicDecisionRoll
		&& Assault.DecisionRoll > Assault.EvaluationResult.LaunchProbability)
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::DecisionRollFailed);
		return;
	}

	Assault.NarrativeDifficultyAtLaunch = GetCurrentNarrativeDifficulty();
	int32 AuthoredRoadMaximum = 0;
	bool bHasOnFootApproach = false;
	TArray<int32> VehicleDeploymentCapacities;
	for (const FName ApproachID : Assault.SelectedApproaches)
	{
		const FTerritoryAssaultApproach* Approach =
			Territory->GetCounterAttackApproaches().FindByPredicate(
				[ApproachID](const FTerritoryAssaultApproach& Candidate)
				{
					return Candidate.ApproachID == ApproachID;
				});
		if (Approach && Approach->EntryType ==
			ETerritoryAssaultEntryType::NarrativeVehicle)
		{
			const int32 AuthoredDeployments = FMath::Max(0,
				Approach->MaximumVehicleDeployments);
			AuthoredRoadMaximum += AuthoredDeployments;
			const int32 Capacity = FMath::Max(0, FMath::Min(
				Approach->MaxWaveSize, Approach->VehicleOccupantCapacity));
			for (int32 Index = 0; Index < AuthoredDeployments; ++Index)
			{
				VehicleDeploymentCapacities.Add(Capacity);
			}
		}
		else if (Approach)
		{
			bHasOnFootApproach = true;
		}
	}
	Assault.MaximumVehicleDeployments =
		UTerritoryCounterAttackProfile::ResolveVehicleCountForDifficulty(
			*ForceConfig, Assault.NarrativeDifficultyAtLaunch,
			AuthoredRoadMaximum, PlannedForce);
	Assault.PlannedForce = FMath::Max(1, PlannedForce);
	if (!bHasOnFootApproach && !VehicleDeploymentCapacities.IsEmpty())
	{
		const int32 TransportedForce = ResolveVehicleOnlyPlannedForce(
			Assault.PlannedForce, Assault.MaximumVehicleDeployments,
			VehicleDeploymentCapacities);
		if (TransportedForce <= 0)
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
				ETerritoryAssaultResolution::ConfigurationInvalid);
			return;
		}
		if (TransportedForce < Assault.PlannedForce)
		{
			UE_LOG(LogTerritory, Display,
				TEXT("[CounterAttack] %s finite force reduced from %d to %d so every attacker arrives in one of the %d difficulty-authorized Narrative vehicles"),
				*Territory->GetTerritoryTag().ToString(), Assault.PlannedForce,
				TransportedForce, Assault.MaximumVehicleDeployments);
			Assault.PlannedForce = TransportedForce;
		}
	}
	Assault.AliveForce = 0;
	Assault.PendingReserveForce = Assault.PlannedForce;
	Assault.KilledForce = 0;
	Assault.WithdrawnForce = 0;
	Assault.WaveSize = ResolveAssaultWaveSize(
		Assault, *ForceConfig, Assault.PlannedForce);
	Assault.VehicleDeploymentsUsed = 0;
	Assault.VehicleDeploymentsByApproach.Reset();
	Assault.ScheduledGameTime = GetCampaignGameTime();
	const ETerritoryAssaultState PreviousState = Assault.State;
	Assault.State = ETerritoryAssaultState::ScheduledWarning;
	Assault.Resolution = ETerritoryAssaultResolution::None;
	BroadcastStateTransition(Assault, PreviousState);
}

bool UTerritoryCounterAttackSubsystem::ActivateAssault(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	if (Assault.State != ETerritoryAssaultState::WaitingForPlayerProximity
		|| !Territory || Territory->GetControlMode() != ETerritoryControlMode::Independent
		|| !Territory->IsAvailableForGameplay() || Assault.SelectedApproaches.IsEmpty())
	{
		return false;
	}

	for (FName ApproachID : Assault.SelectedApproaches)
	{
		FTerritoryAssaultApproach Approach;
		FTransform Transform;
		if (!ResolveApproach(Territory, ApproachID, Approach, Transform))
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
				ETerritoryAssaultResolution::InvalidApproachOrRoute);
			return false;
		}
	}

	// Commit activation before spawning. Multiple players entering during this frame
	// cannot activate or duplicate the force a second time.
	const ETerritoryAssaultState PreviousState = Assault.State;
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	if (!TryCommitProximityActivation(Assault, GetCampaignGameTime())) return false;
	BroadcastStateTransition(Assault, PreviousState);
	if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Active) || !IsValid(Territory)) return false;
	SpawnNextWave(Assault, Territory);
	return IsAssaultCurrent(Access, ETerritoryAssaultState::Active);
}

void UTerritoryCounterAttackSubsystem::SpawnNextWave(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	if (bSpawningAssaultWave || !GetWorld() || GetWorld()->GetNetMode() == NM_Client
		|| Assault.State != ETerritoryAssaultState::Active || !IsValid(Territory) || !Territory->HasAuthority()) return;
	TGuardValue<bool> WaveGuard(bSpawningAssaultWave, true);
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const FTerritoryFactionAssaultConfig* AuthoredForce = Profile
		? Profile->FindFactionForce(Assault.AttackingFaction) : nullptr;
	const FTerritoryFactionAssaultConfig ForceSnapshot = AuthoredForce
		? *AuthoredForce : FTerritoryFactionAssaultConfig();
	const FTerritoryFactionAssaultConfig* ForceConfig = AuthoredForce ? &ForceSnapshot : nullptr;
	UNPCDefinition* AttackerDefinition = ForceConfig
		? ResolveAssaultDefinition(Assault, *ForceConfig) : nullptr;
	if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Active) || !IsValid(Territory)) return;
	if (!ForceConfig || !AttackerDefinition)
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}
	// One Narrative vehicle squad owns ingress until its driver and passengers mount,
	// drive, park and dismount. Do not pop later reserves into the scene or let them
	// overtake the active reinforcement car.
	if (HasPendingVehicleIngress(Assault.AssaultID)) return;
	const int32 MaximumSpawnFailures = FMath::Max(1, Profile->MaxConsecutiveSpawnFailures);
	if (Assault.SelectedApproaches.IsEmpty())
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::InvalidApproachOrRoute);
		return;
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const int32 GlobalAvailable = Settings
		? FMath::Max(0, Settings->MaxLiveCounterAttackNPCs - CountLiveParticipants())
		: Assault.PendingReserveForce;
	int32 ToSpawn = FMath::Min3(Assault.PendingReserveForce,
		FMath::Max(0, Assault.WaveSize - Assault.AliveForce), GlobalAvailable);
	if (ToSpawn <= 0) return;

	const int32 AlreadyDeployed = Assault.PlannedForce - Assault.PendingReserveForce;
	APawn* PresentationPlayer = FindNearestRelevantPlayer(Assault, Territory,
		FMath::Max(Profile->NotificationRadius, Profile->ReserveMaximumPlayerDistance));
	TArray<FName> WaveApproaches = Assault.SelectedApproaches;
	if (Profile->bUsePlayerRelativeReserveStaging && PresentationPlayer)
	{
		const FVector PlayerLocation = PresentationPlayer->GetActorLocation();
		const FVector PlayerForward = PresentationPlayer->GetBaseAimRotation().Vector();
		WaveApproaches.Sort([this, Territory, Profile, PlayerLocation, PlayerForward](
			const FName& A, const FName& B)
		{
			FTerritoryAssaultApproach ApproachA;
			FTerritoryAssaultApproach ApproachB;
			FTransform TransformA;
			FTransform TransformB;
			const bool bResolvedA = ResolveApproach(Territory, A, ApproachA, TransformA);
			const bool bResolvedB = ResolveApproach(Territory, B, ApproachB, TransformB);
			if (bResolvedA != bResolvedB) return bResolvedA;
			if (!bResolvedA) return A.LexicalLess(B);

			const float ScoreA = CalculatePlayerRelativeApproachScore(
				TransformA.GetLocation(), PlayerLocation, PlayerForward,
				Profile->ReserveMinimumPlayerDistance,
				Profile->ReservePreferredPlayerDistance,
				Profile->ReserveMaximumPlayerDistance,
				Profile->PreferredCameraEdgeDot,
				Profile->SameFloorHeightTolerance);
			const float ScoreB = CalculatePlayerRelativeApproachScore(
				TransformB.GetLocation(), PlayerLocation, PlayerForward,
				Profile->ReserveMinimumPlayerDistance,
				Profile->ReservePreferredPlayerDistance,
				Profile->ReserveMaximumPlayerDistance,
				Profile->PreferredCameraEdgeDot,
				Profile->SameFloorHeightTolerance);
			return FMath::IsNearlyEqual(ScoreA, ScoreB)
				? A.LexicalLess(B) : ScoreA > ScoreB;
		});
	}
	const int32 OverrideNarrativeLevel = ResolveScaledEnemyLevel(
		Assault, Territory, *ForceConfig);
	int32 Spawned = 0;
	TMap<FName, int32> SpawnedPerApproach;
	const int32 PlacementAttempts = FMath::Clamp(
		Profile->SpawnPlacementAttemptsPerParticipant, 1, 16);
	const float ParticipantSpacing = FMath::Max(100.f, Profile->ParticipantSpacing);
	const int32 ApproachCount = WaveApproaches.Num();
	const int32 MaximumAttempts = ToSpawn * ApproachCount * PlacementAttempts;
	for (int32 Attempt = 0; Attempt < MaximumAttempts && Spawned < ToSpawn; ++Attempt)
	{
		const int32 DeploymentIndex = AlreadyDeployed + Attempt;
		const FName ApproachID = WaveApproaches[DeploymentIndex % ApproachCount];
		FTerritoryAssaultApproach Approach;
		FTransform ApproachTransform;
		if (!ResolveApproach(Territory, ApproachID, Approach, ApproachTransform)) continue;
		if (SpawnedPerApproach.FindRef(ApproachID) >= FMath::Max(1, Approach.MaxWaveSize)) continue;
		FVector TargetLocation;
		FTransform FootDeploymentTransform;
		FTransform VehicleDropOffTransform;
		if (!ResolveApproachObjective(Territory, Approach, ApproachTransform,
			TargetLocation, FootDeploymentTransform, &VehicleDropOffTransform))
		{
			continue;
		}
		FTerritoryVehicleDeploymentCount* VehicleDeployment =
			Assault.VehicleDeploymentsByApproach.FindByPredicate(
				[ApproachID](const FTerritoryVehicleDeploymentCount& Entry)
				{
					return Entry.ApproachID == ApproachID;
				});
		if (!VehicleDeployment)
		{
			VehicleDeployment = &Assault.VehicleDeploymentsByApproach.AddDefaulted_GetRef();
			VehicleDeployment->ApproachID = ApproachID;
		}
		const int32 VehicleDeploymentCount = VehicleDeployment->Count;
		const bool bUseNarrativeVehicle =
			Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle
			&& Assault.VehicleDeploymentsUsed
				< FMath::Max(0, Assault.MaximumVehicleDeployments)
			&& VehicleDeploymentCount
				< FMath::Max(1, Approach.MaximumVehicleDeployments);
		// A road-only reinforcement must physically arrive in its authored car.
		// Exhausting the difficulty/car budget must never teleport later attackers
		// to the drop-off as if this were an on-foot approach.
		if (Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle
			&& !bUseNarrativeVehicle)
		{
			continue;
		}
		const bool bReverseStoryEscape = bUseNarrativeVehicle
			&& Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
			&& Assault.StoryPursuitDirection ==
				ETerritoryStoryPursuitDirection::PlayerChasesEnemy;
		const FTransform VehicleSpawnTransform = bReverseStoryEscape
			? VehicleDropOffTransform : ApproachTransform;
		const FTransform EffectiveDropOffTransform = bReverseStoryEscape
			? ApproachTransform : VehicleDropOffTransform;
		if (bReverseStoryEscape)
		{
			FText GuideFailure;
			TArray<FVector> GuidePoints;
			const ATerritoryRoadGuide* Guide = ResolveRoadGuide(Approach);
			const bool bReverseRouteValid = Guide
				? Guide->BuildRoutePoints(true, Approach.RoadLaneSide,
					GuidePoints, GuideFailure)
				: ValidateNarrativeVehicleRoute(GetWorld(),
					VehicleSpawnTransform.GetLocation(),
					EffectiveDropOffTransform.GetLocation());
			if (!bReverseRouteValid) continue;
		}

		FTransform SpawnTransform = CalculateParticipantDeploymentTransform(
			bUseNarrativeVehicle ? VehicleSpawnTransform : FootDeploymentTransform,
			TargetLocation, DeploymentIndex / ApproachCount, ParticipantSpacing);
		if (bUseNarrativeVehicle)
		{
			// Keep the driver beside the car before Narrative's mount interaction runs.
			SpawnTransform.SetLocation(VehicleSpawnTransform.GetLocation()
				+ VehicleSpawnTransform.GetRotation().GetRightVector()
					* FMath::Max(250.f, ParticipantSpacing));
		}
		else if (!HasNavigationRoute(SpawnTransform.GetLocation(), TargetLocation))
		{
			continue;
		}
		if (!IsDeploymentLocationSeparated(
			SpawnTransform.GetLocation(), ParticipantSpacing * 0.8f))
		{
			continue;
		}
		FVector VehicleWalkDestination = FVector::ZeroVector;
		if (bUseNarrativeVehicle && !bReverseStoryEscape)
		{
			const FVector RequestedWalkDestination = Assault.StoryFocusLocation.IsNearlyZero()
				? TargetLocation : Assault.StoryFocusLocation;
			if (HasNavigationRoute(EffectiveDropOffTransform.GetLocation(),
				RequestedWalkDestination))
			{
				VehicleWalkDestination = RequestedWalkDestination;
			}
			else if (HasNavigationRoute(EffectiveDropOffTransform.GetLocation(),
				TargetLocation))
			{
				// A quest focus may be above the navmesh or temporarily unreachable. The
				// reinforcement must still complete its authored ingress and fight for the
				// Territory instead of leaving the vehicle ingress permanently active.
				VehicleWalkDestination = TargetLocation;
			}
			else
			{
				// Dismount at the validated road drop-off; Territory combat takes over there.
				VehicleWalkDestination = EffectiveDropOffTransform.GetLocation();
			}
		}
		else if (bUseNarrativeVehicle && bReverseStoryEscape)
		{
			if (const ATerritoryRoadGuide* Guide = ResolveRoadGuide(Approach))
			{
				VehicleWalkDestination = Guide->GetFinalFightTransform().GetLocation();
			}
			else
			{
				// Without a placed guide the fallback final fight is the road exit.
				VehicleWalkDestination = EffectiveDropOffTransform.GetLocation();
			}
		}

		TArray<ATerritoryAssaultCharacter*> SpawnedParticipants;
		if (bUseNarrativeVehicle)
		{
			SpawnedParticipants = SpawnNarrativeVehicleParticipants(
				Assault, Territory, *ForceConfig, AttackerDefinition, Approach,
				VehicleSpawnTransform, SpawnTransform, EffectiveDropOffTransform,
				VehicleWalkDestination, ToSpawn - Spawned, OverrideNarrativeLevel);
		}
		else if (ATerritoryAssaultCharacter* Participant = SpawnParticipant(
			Assault, Territory, *ForceConfig, AttackerDefinition, Approach,
			SpawnTransform, OverrideNarrativeLevel))
		{
			SpawnedParticipants.Add(Participant);
		}
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Active) || !IsValid(Territory)) return;
		if (!SpawnedParticipants.IsEmpty())
		{
			if (bUseNarrativeVehicle)
			{
				FTerritoryVehicleDeploymentCount* CurrentDeployment =
					Assault.VehicleDeploymentsByApproach.FindByPredicate(
						[ApproachID](const FTerritoryVehicleDeploymentCount& Entry) { return Entry.ApproachID == ApproachID; });
				if (!CurrentDeployment) return;
				++CurrentDeployment->Count;
				++Assault.VehicleDeploymentsUsed;
			}
			ATerritoryAssaultCharacter** Speaker = SpawnedParticipants.FindByPredicate(
				[](const ATerritoryAssaultCharacter* NPC)
				{
					return IsValid(NPC) && !NPC->IsActorBeingDestroyed()
						&& NPC->AssaultParticipant && !NPC->AssaultParticipant->HasRetired();
				});
			if (Spawned == 0 && Speaker && IsValid(PresentationPlayer)
				&& ForceConfig->ReserveWaveAlertDialogueTag.IsValid())
			{
				(*Speaker)->PlayTaggedDialogue(
					ForceConfig->ReserveWaveAlertDialogueTag, PresentationPlayer);
				if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Active) || !IsValid(Territory)) return;
			}
			const int32 ParticipantCount = SpawnedParticipants.Num();
			SpawnedPerApproach.FindOrAdd(ApproachID) += ParticipantCount;
			Spawned += ParticipantCount;
			if (bUseNarrativeVehicle) break;
		}
	}
	if (Spawned > 0)
	{
		Assault.ConsecutiveSpawnFailures = 0;
		BroadcastChanged(Assault);
	}
	else
	{
		++Assault.ConsecutiveSpawnFailures;
		BroadcastChanged(Assault);
		if (!IsAssaultCurrent(Access, ETerritoryAssaultState::Active)) return;
		if (Assault.ConsecutiveSpawnFailures >= MaximumSpawnFailures)
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
				ETerritoryAssaultResolution::SpawnFailed);
		}
	}
}

ATerritoryAssaultCharacter* UTerritoryCounterAttackSubsystem::SpawnParticipant(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& InForceConfig,
	UNPCDefinition* AttackerDefinition,
	const FTerritoryAssaultApproach& Approach, const FTransform& SpawnTransform,
	int32 OverrideNarrativeLevel)
{
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	const FTerritoryAssaultRecord SpawnRecord = Assault;
	const FTerritoryFactionAssaultConfig ForceConfig = InForceConfig;
	const auto IsSpawnCurrent = [this, &Access, Territory]()
	{
		return GetWorld() && GetWorld()->GetNetMode() != NM_Client
			&& IsAssaultCurrent(Access, ETerritoryAssaultState::Active)
			&& IsValid(Territory) && Territory->HasAuthority() && !Territory->IsActorBeingDestroyed()
			&& Territory->GetWorld() == GetWorld();
	};
	if (!IsSpawnCurrent() || ConstructingParticipant || Assault.PendingReserveForce <= 0) return nullptr;
	FText SpawnClassFailure;
	if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
		AttackerDefinition, SpawnRecord.PlannedForce, SpawnClassFailure))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack definition %s is not physically spawn-ready: %s"),
			*GetNameSafe(AttackerDefinition), *SpawnClassFailure.ToString());
		return nullptr;
	}

	FConstructingParticipant Construction;
	Construction.Access = Access;
	Construction.SpawnGUID = FGuid::NewGuid();
	TGuardValue<FConstructingParticipant*> ConstructionGuard(ConstructingParticipant, &Construction);
	UNarrativeCharacterSubsystem* CharacterSubsystem =
		GetWorld() ? GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>() : nullptr;
	if (!IsSpawnCurrent()) return nullptr;
	ATerritoryAssaultCharacter* Participant = ATerritoryAssaultCharacter::SpawnThroughNarrative(
		CharacterSubsystem, AttackerDefinition, SpawnRecord.AttackingFaction,
		Territory->GetTerritoryGUID(), Construction.SpawnGUID, SpawnTransform, Approach.ApproachID,
		ForceConfig.ActivityConfigurationOverride, ForceConfig.TriggerSetOverrides,
		SpawnRecord.AssaultID, SpawnRecord.TargetTerritory, OverrideNarrativeLevel);
	bool bAdmitted = false;
	ON_SCOPE_EXIT
	{
		// Validation cleanup must not consume reserve as a real withdrawal.
		Construction.bAcceptRemoval = false;
		if (!bAdmitted && IsValid(Participant) && !Participant->IsActorBeingDestroyed())
		{
			if (Participant->AssaultParticipant) Participant->AssaultParticipant->Retire(false);
			if (IsValid(Participant)) TerritoryNarrativeDeathSupport::PrepareForRemoval(*Participant);
			if (IsValid(Participant)) Participant->Destroy();
		}
	};
	if (!IsSpawnCurrent()) return nullptr;
	if (!IsValid(Participant) || Participant->IsActorBeingDestroyed()) return nullptr;
	if (!Participant->AssaultParticipant || Participant->AssaultParticipant->HasRetired()) return nullptr;
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const float MinimumSpacing = FMath::Max(
		100.f, Profile ? Profile->ParticipantSpacing * 0.7f : 154.f);
	if (!IsDeploymentLocationSeparated(Participant->GetActorLocation(), MinimumSpacing))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Counterattack participant %s was collision-adjusted into an occupied deployment slot"),
			*GetNameSafe(Participant));
		return nullptr;
	}
	if (Participant->AssaultParticipant)
	{
		const bool bTakeoverPriority = SpawnRecord.LaunchMode ==
			ETerritoryAssaultLaunchMode::StrategicCounterattack
			&& SpawnRecord.bAllowsTerritoryCapture
			&& (!Profile || Profile->bPrioritizeTerritoryTakeover);
		Participant->AssaultParticipant->ConfigureAssaultRules(
			SpawnRecord.bAllowsTerritoryCapture, bTakeoverPriority,
			Profile ? Profile->DefendingPlayerEngagementPadding : 800.f,
			ForceConfig.TakeoverStartedDialogueTag,
			ForceConfig.FinalFightDialogueTag);
	}
	if (!Participant->EnsureNarrativeControllerReady())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack participant %s failed the verified Narrative controller/activity contract"),
			*GetNameSafe(Participant));
		return nullptr;
	}
	if (!IsSpawnCurrent() || !IsValid(Participant) || Participant->IsActorBeingDestroyed()) return nullptr;
	if (OverrideNarrativeLevel > 0 && ForceConfig.PowerScalingEffect)
	{
		UNarrativeAbilitySystemComponent* ASC =
			FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Participant);
		if (!ASC)
		{
			UE_LOG(LogTerritory, Error,
				TEXT("Counterattack participant %s has no Narrative ASC for its configured power-scaling effect"),
				*GetNameSafe(Participant));
			return nullptr;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			ForceConfig.PowerScalingEffect, OverrideNarrativeLevel, Context);
		if (!Spec.IsValid())
		{
			UE_LOG(LogTerritory, Error,
				TEXT("Counterattack participant %s could not create configured Narrative power-scaling effect %s"),
				*GetNameSafe(Participant),
				*GetNameSafe(ForceConfig.PowerScalingEffect));
			return nullptr;
		}
		if (FMath::IsFinite(ForceConfig.PowerScalingMagnitudePerEnemyLevel)
			&& ForceConfig.PowerScalingMagnitudePerEnemyLevel > 0.f)
		{
			Spec.Data->SetSetByCallerMagnitude(
				FNarrativeGameplayTags::Get().SetByCaller_AttackDamage,
				static_cast<float>(FMath::Max(0, OverrideNarrativeLevel - 1))
					* ForceConfig.PowerScalingMagnitudePerEnemyLevel);
		}
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	if (!IsSpawnCurrent() || !IsValid(Participant) || Participant->IsActorBeingDestroyed()
		|| Participant->AssaultParticipant->HasRetired() || Assault.PendingReserveForce <= 0) return nullptr;
	// Commit admission before another NPC's construction or wave dialogue can
	// synchronously remove this participant.
	--Assault.PendingReserveForce;
	++Assault.AliveForce;
	LiveParticipants.FindOrAdd(Access.ID).Add(Participant);
	bAdmitted = true;
	return Participant;
}

TArray<ATerritoryAssaultCharacter*>
UTerritoryCounterAttackSubsystem::SpawnNarrativeVehicleParticipants(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& InForceConfig,
	UNPCDefinition* AttackerDefinition,
	const FTerritoryAssaultApproach& Approach,
	const FTransform& VehicleSpawnTransform,
	const FTransform& DriverSpawnTransform,
	const FTransform& DropOffTransform, const FVector& WalkDestination,
	const int32 RequestedOccupants, const int32 OverrideNarrativeLevel)
{
	TArray<ATerritoryAssaultCharacter*> Participants;
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	const FTerritoryFactionAssaultConfig ForceConfig = InForceConfig;
	const auto IsDeploymentCurrent = [this, &Access, Territory]()
	{
		return GetWorld() && GetWorld()->GetNetMode() != NM_Client
			&& IsAssaultCurrent(Access, ETerritoryAssaultState::Active)
			&& IsValid(Territory) && Territory->HasAuthority() && !Territory->IsActorBeingDestroyed()
			&& Territory->GetWorld() == GetWorld();
	};
	if (!IsDeploymentCurrent()) return Participants;
	UWorld* World = GetWorld();
	UClass* VehicleClass = UTerritoryCounterAttackProfile::ResolveVehicleClass(
		ForceConfig, Approach).LoadSynchronous();
	if (!IsDeploymentCurrent()) return Participants;
	if (!World || !VehicleClass
		|| !VehicleClass->IsChildOf(ANarrativeVehicleBase::StaticClass()))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[CounterAttack] vehicle approach '%s' has no valid Narrative vehicle class"),
			*Approach.ApproachID.ToString());
		return Participants;
	}

	FActorSpawnParameters VehicleSpawnParameters;
	VehicleSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	VehicleSpawnParameters.ObjectFlags |= RF_Transient;
	ANarrativeVehicleBase* Vehicle = World->SpawnActor<ANarrativeVehicleBase>(
		VehicleClass, VehicleSpawnTransform, VehicleSpawnParameters);
	TArray<TWeakObjectPtr<ATerritoryAssaultCharacter>> StagedParticipants;
	bool bVehicleAdmitted = false;
	ON_SCOPE_EXIT
	{
		if (!bVehicleAdmitted)
		{
			for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& Staged : StagedParticipants)
			{
				if (ATerritoryAssaultCharacter* NPC = Staged.Get())
				{
					if (NPC->AssaultParticipant) NPC->AssaultParticipant->Retire(false);
					if (IsValid(NPC)) TerritoryNarrativeDeathSupport::ScheduleRemoval(*NPC);
				}
			}
			if (IsValid(Vehicle) && !Vehicle->IsActorBeingDestroyed()) Vehicle->Destroy();
		}
	};
	if (!IsDeploymentCurrent()) return Participants;
	if (!IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed())
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[CounterAttack] Narrative vehicle could not spawn at approach '%s'"),
			*Approach.ApproachID.ToString());
		return Participants;
	}
	// Assault cars are runtime deployment actors, not independent campaign vehicles.
	Vehicle->SetVehicleSaveGuid(FGuid());

	UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
	const int32 OccupantCount = ResolveVehicleOccupantCount(
		RequestedOccupants, Approach.MaxWaveSize, Approach.VehicleOccupantCapacity,
		Mount ? Mount->InteractionSlots.Num() : 0);
	if (!Mount || OccupantCount <= 0)
	{
		UE_LOG(LogTerritory, Error,
			TEXT("[CounterAttack] vehicle approach '%s' requested %d occupants but %s exposes no usable Narrative mount seats"),
			*Approach.ApproachID.ToString(), RequestedOccupants, *GetNameSafe(Vehicle));
		Vehicle->Destroy();
		return Participants;
	}

	const bool bReverseStoryEscape = Assault.LaunchMode ==
		ETerritoryAssaultLaunchMode::StoryPursuit
		&& Assault.StoryPursuitDirection ==
			ETerritoryStoryPursuitDirection::PlayerChasesEnemy;
	ATerritoryRoadGuide* RoadGuide = ResolveRoadGuide(Approach);
	TArray<FVector> VehicleRoutePoints;
	FString VehicleRouteFailure;
	bool bHasVehicleRoute = false;
	if (RoadGuide)
	{
		FText GuideFailure;
		bHasVehicleRoute = RoadGuide->BuildRoutePoints(bReverseStoryEscape,
			Approach.RoadLaneSide, VehicleRoutePoints, GuideFailure);
		VehicleRouteFailure = GuideFailure.ToString();
	}
	else
	{
		bHasVehicleRoute = BuildNarrativeVehicleRoute(World,
			VehicleSpawnTransform.GetLocation(), DropOffTransform.GetLocation(),
			VehicleRoutePoints, &VehicleRouteFailure);
	}
	if (!bHasVehicleRoute)
	{
		Vehicle->Destroy();
		UE_LOG(LogTerritory, Error,
			TEXT("[CounterAttack] approach '%s' could not build its ZoneGraph driving route (%s); the finite force was not consumed"),
			*Approach.ApproachID.ToString(), *VehicleRouteFailure);
		return Participants;
	}
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const float PassengerRadius = FMath::Max(350.f,
		Profile ? Profile->ParticipantSpacing : 220.f);
	auto BuildSeatStagingTransform = [&VehicleSpawnTransform, &DriverSpawnTransform,
		PassengerRadius, OccupantCount](const int32 SeatIndex)
	{
		const float Angle = PI * 0.5f + 2.f * PI
			* static_cast<float>(SeatIndex) / static_cast<float>(OccupantCount);
		const FVector Radial = VehicleSpawnTransform.GetRotation().GetForwardVector()
			* FMath::Cos(Angle) + VehicleSpawnTransform.GetRotation().GetRightVector()
			* FMath::Sin(Angle);
		FTransform Result = DriverSpawnTransform;
		Result.SetLocation(VehicleSpawnTransform.GetLocation()
			+ Radial.GetSafeNormal2D() * PassengerRadius);
		Result.SetRotation((VehicleSpawnTransform.GetLocation()
			- Result.GetLocation()).Rotation().Quaternion());
		return Result;
	};
	ATerritoryAssaultCharacter* Participant = SpawnParticipant(
		Assault, Territory, ForceConfig, AttackerDefinition, Approach,
		BuildSeatStagingTransform(0), OverrideNarrativeLevel);
	if (IsValid(Participant)) StagedParticipants.Add(Participant);
	if (!IsDeploymentCurrent() || !IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed()) return {};
	if (!Participant || !Participant->AssaultParticipant)
	{
		Vehicle->Destroy();
		UE_LOG(LogTerritory, Error,
			TEXT("[CounterAttack] approach '%s' could not admit a living Narrative vehicle driver"),
			*Approach.ApproachID.ToString());
		return Participants;
	}
	const bool bEscapeOnArrival = bReverseStoryEscape;
	const FTransform WalkTransform = WalkDestination.IsNearlyZero()
		? FTransform::Identity
		: FTransform((WalkDestination - DropOffTransform.GetLocation()).Rotation(),
			WalkDestination);
	Participant->AssaultParticipant->ConfigureNarrativeVehicleIngress(
		Vehicle, VehicleRoutePoints, DropOffTransform, WalkTransform,
		Approach.VehicleMaximumDriveSpeed, Approach.VehicleIngressTimeoutSeconds,
		bEscapeOnArrival, Approach.VehicleAwareness,
		bEscapeOnArrival ? Assault.StoryMaximumChaseDistance : 0.f,
		bEscapeOnArrival ? Assault.StoryChaseDistanceGraceSeconds : 0.f,
		bEscapeOnArrival && Assault.bStoryAbandonDamagedVehicleForFinalFight,
		Assault.StoryVehicleAbandonHealthFraction);
	Participants.Add(Participant);

	for (int32 SeatIndex = 1; SeatIndex < OccupantCount; ++SeatIndex)
	{
		ATerritoryAssaultCharacter* Passenger = SpawnParticipant(
			Assault, Territory, ForceConfig, AttackerDefinition, Approach,
			BuildSeatStagingTransform(SeatIndex), OverrideNarrativeLevel);
		if (IsValid(Passenger)) StagedParticipants.Add(Passenger);
		if (!IsDeploymentCurrent() || !IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed()) return {};
		if (!Passenger || !Passenger->AssaultParticipant)
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[CounterAttack] %s could not fill Narrative vehicle seat %d; the unspawned finite force remains in reserve"),
				*Approach.ApproachID.ToString(), SeatIndex);
			continue;
		}
		Passenger->AssaultParticipant->ConfigureNarrativeVehiclePassenger(
			Participant->AssaultParticipant, Vehicle, SeatIndex, VehicleRoutePoints,
			DropOffTransform, WalkTransform, Approach.VehicleIngressTimeoutSeconds,
			bEscapeOnArrival);
		Participants.Add(Passenger);
	}

	LiveAssaultVehicles.FindOrAdd(Assault.AssaultID).Add(Vehicle);
	LiveVehicleRetirementRules.Add(Vehicle, Approach.VehicleRetirement);
	bVehicleAdmitted = true;
	if (RoadGuide && Assault.bStoryActivateRoadMissionTraffic)
	{
		TSet<TWeakObjectPtr<ATerritoryRoadGuide>>& Guides =
			LiveAssaultRoadGuides.FindOrAdd(Assault.AssaultID);
		if (!Guides.Contains(RoadGuide))
		{
			Guides.Add(RoadGuide);
			RoadGuide->BeginMissionTraffic(
				Assault.StoryMissionTrafficVehicleCountOverride);
			if (!IsDeploymentCurrent()) return {};
		}
	}
	UE_LOG(LogTerritory, Display,
		TEXT("[CounterAttack] deployed %d/%d finite assault participants in %s (driver included)"),
		Participants.Num(), OccupantCount, *GetNameSafe(Vehicle));
	return Participants;
}

FTransform UTerritoryCounterAttackSubsystem::CalculateParticipantDeploymentTransform(
	const FTransform& ApproachTransform, const FVector& TargetLocation,
	int32 FormationSlot, float ParticipantSpacing)
{
	const float Spacing = FMath::Max(100.f, ParticipantSpacing);
	const int32 Slot = FMath::Max(0, FormationSlot);
	const int32 Column = Slot % 3;
	const int32 Row = Slot / 3;
	const float LateralMultiplier = Column == 0 ? 0.f : (Column == 1 ? 1.f : -1.f);

	FVector Forward = TargetLocation - ApproachTransform.GetLocation();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = ApproachTransform.GetRotation().GetForwardVector();
		Forward.Z = 0.f;
		if (!Forward.Normalize()) Forward = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Location = ApproachTransform.GetLocation()
		+ Right * LateralMultiplier * Spacing
		- Forward * static_cast<float>(Row) * Spacing;
	return FTransform(Forward.Rotation(), Location, ApproachTransform.GetScale3D());
}

float UTerritoryCounterAttackSubsystem::CalculatePlayerRelativeApproachScore(
	const FVector& ApproachLocation, const FVector& PlayerLocation,
	const FVector& PlayerViewForward, float MinimumDistance,
	float PreferredDistance, float MaximumDistance,
	float PreferredCameraEdgeDot, float SameFloorHeightTolerance)
{
	const float MinDistance = FMath::Max(100.f, MinimumDistance);
	const float Preferred = FMath::Max(MinDistance, PreferredDistance);
	const float MaxDistance = FMath::Max(Preferred, MaximumDistance);
	const FVector Offset = ApproachLocation - PlayerLocation;
	FVector HorizontalOffset(Offset.X, Offset.Y, 0.f);
	const float Distance = HorizontalOffset.Size();
	HorizontalOffset = HorizontalOffset.GetSafeNormal();
	FVector ViewForward(PlayerViewForward.X, PlayerViewForward.Y, 0.f);
	ViewForward = ViewForward.GetSafeNormal();
	const float ViewDot = HorizontalOffset.IsNearlyZero() || ViewForward.IsNearlyZero()
		? 0.f : FVector::DotProduct(ViewForward, HorizontalOffset);
	const float EdgeTarget = FMath::Clamp(PreferredCameraEdgeDot, 0.f, 1.f);
	const float DistanceRange = FMath::Max(1.f, MaxDistance - MinDistance);

	float Score = 4.f - 3.f * FMath::Abs(Distance - Preferred) / DistanceRange;
	Score += 2.f * (1.f - FMath::Abs(FMath::Abs(ViewDot) - EdgeTarget));
	if (Distance < MinDistance) Score -= 6.f;
	if (Distance > MaxDistance) Score -= 2.f;
	if (ViewDot < -0.1f) Score -= 5.f;       // never prefer a surprise directly behind
	if (ViewDot > 0.85f) Score -= 2.5f;      // avoid obvious center-screen pop-in

	const float FloorTolerance = FMath::Max(1.f, SameFloorHeightTolerance);
	const float HeightDifference = FMath::Abs(Offset.Z);
	Score += HeightDifference <= FloorTolerance
		? 2.f : -FMath::Min(4.f, HeightDifference / FloorTolerance);
	return Score;
}

void UTerritoryCounterAttackSubsystem::NotifyParticipantRemoved(
	FGuid AssaultID, ATerritoryAssaultCharacter* Participant, bool bKilled)
{
	if (bRestoringState || !GetWorld() || GetWorld()->GetNetMode() == NM_Client
		|| !IsValid(Participant) || !Participant->HasAuthority()
		|| Participant->GetWorld() != GetWorld()) return;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	if (!Assault || Assault->IsTerminal()) return;

	TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants = LiveParticipants.Find(AssaultID);
	const bool bRemovedLive = Participants && Participants->Remove(Participant) > 0;
	if (bRemovedLive)
	{
		if (Participants->IsEmpty()) LiveParticipants.Remove(AssaultID);
	}
	else
	{
		// Narrative can report removal before SpawnNPC returns. Only this exact
		// construction may consume reserve; an arbitrary same-assault NPC cannot.
		if (!ConstructingParticipant || !ConstructingParticipant->bAcceptRemoval
			|| ConstructingParticipant->bRemovalReported
			|| ConstructingParticipant->Access.ID != AssaultID
			|| !IsAssaultCurrent(ConstructingParticipant->Access, ETerritoryAssaultState::Active)
			|| Participant->GetActorGUID_Implementation() != ConstructingParticipant->SpawnGUID
			|| Assault->PendingReserveForce <= 0) return;
		ConstructingParticipant->bRemovalReported = true;
		// One native commit, with no callback exposing an intermediate living count.
		--Assault->PendingReserveForce;
		++Assault->AliveForce;
	}

	bool bForceExhausted = false;
	if (!ApplyParticipantRemoval(*Assault, bKilled, bForceExhausted)) return;
	const FAssaultAccess Access = CaptureAssaultAccess(*Assault);
	BroadcastChanged(*Assault);

	if (bForceExhausted && IsAssaultCurrent(Access) && !Assault->IsTerminal())
	{
		ResolveAssault(*Assault, ETerritoryAssaultState::Defeated,
			ETerritoryAssaultResolution::AllAttackersRemoved);
	}
}

bool UTerritoryCounterAttackSubsystem::ShouldActivateWaitingAssault(
	bool bAllowsTerritoryCapture, ETerritoryState TerritoryState,
	bool bRequirePlayerProximity, bool bRelevantPlayerNearby)
{
	const bool bTerritoryStateAllowsActivation = !bAllowsTerritoryCapture
		|| TerritoryState == ETerritoryState::Claimed;
	const bool bProximityAllowsActivation = !bRequirePlayerProximity
		|| bRelevantPlayerNearby;
	return bTerritoryStateAllowsActivation && bProximityAllowsActivation;
}

void UTerritoryCounterAttackSubsystem::NotifyVehicleStoryTargetEscaped(
	FGuid AssaultID, ATerritoryAssaultCharacter* Participant)
{
	if (bRestoringState || !IsValid(Participant)) return;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	const TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants =
		LiveParticipants.Find(AssaultID);
	if (!Assault || Assault->IsTerminal() || !Participants
		|| !Participants->Contains(Participant)
		|| Assault->LaunchMode != ETerritoryAssaultLaunchMode::StoryPursuit
		|| Assault->StoryPursuitDirection !=
			ETerritoryStoryPursuitDirection::PlayerChasesEnemy)
	{
		return;
	}
	ResolveAssault(*Assault, ETerritoryAssaultState::Succeeded,
		ETerritoryAssaultResolution::TargetEscaped);
}

void UTerritoryCounterAttackSubsystem::NotifyVehicleStoryTargetLostByDistance(
	FGuid AssaultID, ATerritoryAssaultCharacter* Participant)
{
	if (bRestoringState || !IsValid(Participant)) return;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	const TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants =
		LiveParticipants.Find(AssaultID);
	if (!Assault || Assault->IsTerminal() || !Participants
		|| !Participants->Contains(Participant)
		|| Assault->LaunchMode != ETerritoryAssaultLaunchMode::StoryPursuit
		|| Assault->StoryPursuitDirection !=
			ETerritoryStoryPursuitDirection::PlayerChasesEnemy)
	{
		return;
	}
	ResolveAssault(*Assault, ETerritoryAssaultState::Succeeded,
		ETerritoryAssaultResolution::ChaseDistanceLost);
}

void UTerritoryCounterAttackSubsystem::NotifyVehicleStoryTargetAbandoned(
	FGuid AssaultID, ATerritoryAssaultCharacter* Participant)
{
	if (bRestoringState || !IsValid(Participant)) return;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	const TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants =
		LiveParticipants.Find(AssaultID);
	if (!Assault || Assault->IsTerminal() || !Participants
		|| !Participants->Contains(Participant)
		|| Assault->LaunchMode != ETerritoryAssaultLaunchMode::StoryPursuit
		|| Assault->StoryPursuitDirection !=
			ETerritoryStoryPursuitDirection::PlayerChasesEnemy
		|| Assault->bStoryTargetAbandonedVehicle)
	{
		return;
	}
	Assault->bStoryTargetAbandonedVehicle = true;
	BroadcastChanged(*Assault);
}

bool UTerritoryCounterAttackSubsystem::HasPendingVehicleIngress(
	const FGuid& AssaultID) const
{
	const TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants =
		LiveParticipants.Find(AssaultID);
	if (!Participants) return false;
	for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& WeakParticipant : *Participants)
	{
		const ATerritoryAssaultCharacter* Participant = WeakParticipant.Get();
		if (Participant && Participant->AssaultParticipant
			&& Participant->AssaultParticipant->IsVehicleIngressPending())
		{
			return true;
		}
	}
	return false;
}

bool UTerritoryCounterAttackSubsystem::TryCommitProximityActivation(
	FTerritoryAssaultRecord& Assault, double ActivatedGameTime)
{
	if (Assault.State != ETerritoryAssaultState::WaitingForPlayerProximity)
	{
		return false;
	}
	Assault.State = ETerritoryAssaultState::Active;
	Assault.ActivatedGameTime = ActivatedGameTime;
	return true;
}

bool UTerritoryCounterAttackSubsystem::ApplyParticipantRemoval(
	FTerritoryAssaultRecord& Assault, bool bKilled, bool& bOutForceExhausted)
{
	bOutForceExhausted = false;
	if (Assault.IsTerminal() || Assault.AliveForce <= 0
		|| Assault.KilledForce + Assault.WithdrawnForce >= Assault.PlannedForce)
	{
		return false;
	}

	--Assault.AliveForce;
	if (bKilled) ++Assault.KilledForce;
	else ++Assault.WithdrawnForce;
	bOutForceExhausted = (Assault.State == ETerritoryAssaultState::Active
			|| Assault.State == ETerritoryAssaultState::RecaptureCountdown)
		&& Assault.AliveForce + Assault.PendingReserveForce <= 0;
	return true;
}

bool UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(
	const FTerritoryAssaultRecord& Assault, bool bRelevantPlayerNearby,
	bool bContinueAfterActivation, bool bWaitForCurrentWaveToEnd)
{
	return Assault.State == ETerritoryAssaultState::Active
		&& Assault.PendingReserveForce > 0
		&& Assault.AliveForce < FMath::Max(1, Assault.WaveSize)
		&& (!bWaitForCurrentWaveToEnd || Assault.AliveForce == 0)
		&& (bContinueAfterActivation || bRelevantPlayerNearby);
}

UTerritoryCounterAttackSubsystem::ERecaptureDecision
UTerritoryCounterAttackSubsystem::EvaluateRecaptureDecision(
	bool bDefendersRemain, bool bPhysicalAttackerInside,
	bool bAliveDefendingPlayerInside, bool bDeadDefendingPlayerInside,
	bool bCountdownActive, bool bCountdownExpired,
	bool bAllowUnattendedCountdown, bool bConcedeOnPlayerDeath)
{
	if (bDefendersRemain || bAliveDefendingPlayerInside)
	{
		return ERecaptureDecision::ContinueFight;
	}
	if (!bPhysicalAttackerInside)
	{
		return ERecaptureDecision::NoAction;
	}
	if (bDeadDefendingPlayerInside && bConcedeOnPlayerDeath)
	{
		return ERecaptureDecision::CompleteHandover;
	}
	if (bCountdownActive)
	{
		return bCountdownExpired
			? ERecaptureDecision::CompleteHandover
			: ERecaptureDecision::NoAction;
	}
	return bAllowUnattendedCountdown
		? ERecaptureDecision::StartCountdown
		: ERecaptureDecision::ContinueFight;
}

bool UTerritoryCounterAttackSubsystem::IsRecurringCooldownComplete(
	const FTerritoryAssaultRecord& PreviousAssault, double CurrentGameTime,
	float CooldownGameTime)
{
	return PreviousAssault.IsTerminal()
		&& PreviousAssault.LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& PreviousAssault.ResolvedGameTime > 0.0
		&& CurrentGameTime >= PreviousAssault.ResolvedGameTime
			+ FMath::Max(1.f, CooldownGameTime);
}

void UTerritoryCounterAttackSubsystem::ResolveAssault(
	FTerritoryAssaultRecord& Assault, ETerritoryAssaultState FinalState,
	ETerritoryAssaultResolution Reason)
{
	if (Assault.IsTerminal()) return;
	const ETerritoryAssaultState PreviousState = Assault.State;
	const uint64 ResolutionGeneration = RestoreGeneration;
	Assault.State = FinalState;
	Assault.Resolution = Reason;
	Assault.ResolvedGameTime = GetCampaignGameTime();
	Assault.RecaptureEndsGameTime = 0.0;
	const int32 UnaccountedForce = FMath::Max(0,
		Assault.PlannedForce - Assault.KilledForce - Assault.WithdrawnForce);
	Assault.WithdrawnForce += FMath::Min(UnaccountedForce,
		Assault.AliveForce + Assault.PendingReserveForce);
	Assault.AliveForce = 0;
	Assault.PendingReserveForce = 0;
	// Commit the complete terminal record before Narrative removal callbacks run.
	// Cleanup receives a value snapshot, so a callback cannot invalidate its map reference.
	FTerritoryAssaultRecord Snapshot = Assault;
	{
		TGuardValue<bool> CleanupGuard(bRestoringState, true);
		RetireLiveParticipants(Snapshot, true);
		if (RestoreGeneration != ResolutionGeneration) return;
		RetireLiveVehicles(Snapshot.AssaultID, true);
		if (RestoreGeneration != ResolutionGeneration) return;
	}
	const FTerritoryAssaultRecord* Final = Assaults.Find(Snapshot.AssaultID);
	if (Final && Final->State == Snapshot.State && Final->Resolution == Snapshot.Resolution)
	{
		BroadcastStateTransition(*Final, PreviousState);
	}
}

void UTerritoryCounterAttackSubsystem::RetireLiveParticipants(
	FTerritoryAssaultRecord& Assault, bool bDestroyActors)
{
	// Detach before Narrative activity/overlap callbacks can restore a campaign
	// with this same ID. Cleanup owns only these old actors, never the new map entry.
	TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>> Participants;
	LiveParticipants.RemoveAndCopyValue(Assault.AssaultID, Participants);
	for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& WeakParticipant : Participants)
	{
		if (ATerritoryAssaultCharacter* Participant = WeakParticipant.Get())
		{
			if (Participant->AssaultParticipant)
			{
				Participant->AssaultParticipant->Retire(false);
			}
			if (bDestroyActors && IsValid(Participant))
			{
				TerritoryNarrativeDeathSupport::ScheduleRemoval(*Participant);
			}
		}
	}
}

void UTerritoryCounterAttackSubsystem::RetireLiveVehicles(
	const FGuid& AssaultID, bool bDestroyActors, bool bForCampaignRestore)
{
	TSet<TWeakObjectPtr<ANarrativeVehicleBase>> Vehicles;
	LiveAssaultVehicles.RemoveAndCopyValue(AssaultID, Vehicles);
	TSet<TWeakObjectPtr<ATerritoryRoadGuide>> Guides;
	LiveAssaultRoadGuides.RemoveAndCopyValue(AssaultID, Guides);
	for (const TWeakObjectPtr<ANarrativeVehicleBase>& WeakVehicle : Vehicles)
	{
		FTerritoryVehicleRetirementSettings Rules;
		LiveVehicleRetirementRules.RemoveAndCopyValue(WeakVehicle, Rules);
		if (ANarrativeVehicleBase* Vehicle = WeakVehicle.Get();
			bDestroyActors && IsValid(Vehicle))
		{
			if (bForCampaignRestore)
			{
				RemoveVehicleForCampaignRestore(Vehicle);
				continue;
			}
			const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			FRetiringVehicle Retiring;
			Retiring.Vehicle = Vehicle;
			Retiring.EarliestRetirementTime = Now
				+ FMath::Max(0.f, Rules.EarliestRetirementDelay);
			Retiring.HardRetirementTime = Now
				+ FMath::Max(Rules.EarliestRetirementDelay + 1.f,
					Rules.HardRetirementTimeout);
			Retiring.PlayerKeepAliveDistance = FMath::Max(0.f,
				Rules.PlayerKeepAliveDistance);
			RetiringVehicles.Add(MoveTemp(Retiring));
		}
	}
	for (const TWeakObjectPtr<ATerritoryRoadGuide>& WeakGuide : Guides)
	{
		if (ATerritoryRoadGuide* Guide = WeakGuide.Get()) Guide->EndMissionTraffic();
	}
}

void UTerritoryCounterAttackSubsystem::UpdateRetiringVehicles()
{
	UWorld* World = GetWorld();
	if (!World) return;
	const double Now = World->GetTimeSeconds();
	for (int32 Index = RetiringVehicles.Num() - 1; Index >= 0; --Index)
	{
		FRetiringVehicle& Entry = RetiringVehicles[Index];
		ANarrativeVehicleBase* Vehicle = Entry.Vehicle.Get();
		if (!IsValid(Vehicle) || Vehicle->IsActorBeingDestroyed())
		{
			RetiringVehicles.RemoveAtSwap(Index);
			continue;
		}
		// A player who kept or carjacked the mission vehicle owns it now. Territory
		// releases cleanup authority instead of destroying a car under that player.
		if (HasPlayerVehicleOccupant(Vehicle))
		{
			if (Now >= Entry.HardRetirementTime)
			{
				RetiringVehicles.RemoveAtSwap(Index);
			}
			continue;
		}
		if (Now < Entry.EarliestRetirementTime) continue;

		bool bPlayerNearby = false;
		const float KeepAliveSquared = FMath::Square(Entry.PlayerKeepAliveDistance);
		if (KeepAliveSquared > 0.f)
		{
			for (TActorIterator<APlayerController> It(World); It; ++It)
			{
				const APawn* Pawn = It->GetPawn();
				if (IsValid(Pawn) && FVector::DistSquared(
					Pawn->GetActorLocation(), Vehicle->GetActorLocation())
					<= KeepAliveSquared)
				{
					bPlayerNearby = true;
					break;
				}
			}
		}
		if (!bPlayerNearby || Now >= Entry.HardRetirementTime)
		{
			// A short life span lets a pending Narrative dismount/possession callback
			// finish before the transient vehicle becomes pending-kill.
			Vehicle->SetLifeSpan(0.25f);
			RetiringVehicles.RemoveAtSwap(Index);
		}
	}
}

void UTerritoryCounterAttackSubsystem::BroadcastChanged(const FTerritoryAssaultRecord& Assault)
{
	if (!bRestoringState)
	{
		const FTerritoryAssaultRecord Snapshot = Assault;
		OnAssaultChanged.Broadcast(Snapshot);
	}
}

void UTerritoryCounterAttackSubsystem::BroadcastStateTransition(
	const FTerritoryAssaultRecord& Assault, ETerritoryAssaultState PreviousState)
{
	const FTerritoryAssaultRecord Snapshot = Assault;
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	BroadcastChanged(Snapshot);
	const FTerritoryAssaultRecord* Final = Assaults.Find(Snapshot.AssaultID);
	if (Access.Generation != RestoreGeneration || !Final || Final->State != Snapshot.State
		|| Final->Resolution != Snapshot.Resolution
		|| !ShouldEmitCounterHappened(PreviousState, Snapshot.State, bRestoringState)) return;
	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugCounterAttacks())
	{
		UE_LOG(LogTerritory, Log,
			TEXT("[CounterAttack] %s target=%s state %d -> %d resolution=%d alive=%d reserve=%d killed=%d withdrawn=%d"),
			*Snapshot.AssaultID.ToString(), *Snapshot.TargetTerritory.ToString(),
			static_cast<int32>(PreviousState), static_cast<int32>(Snapshot.State),
			static_cast<int32>(Snapshot.Resolution), Snapshot.AliveForce,
			Snapshot.PendingReserveForce, Snapshot.KilledForce, Snapshot.WithdrawnForce);
	}

	const FTerritoryCounterAttackStateEvent Event = MakeCounterHappenedEvent(
		Snapshot, PreviousState, GetCampaignGameTime());
	OnCounterHappened.Broadcast(Event);
	Final = Assaults.Find(Snapshot.AssaultID);
	if (Access.Generation == RestoreGeneration && Final && Final->State == Snapshot.State
		&& Final->Resolution == Snapshot.Resolution)
	{
		NotifyRelevantPlayersOfState(Event, ResolveTerritory(Snapshot));
	}
}

void UTerritoryCounterAttackSubsystem::NotifyRelevantPlayers(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	UWorld* World = GetWorld();
	UTerritoryCounterAttackProfile* Profile = Territory ? Territory->GetCounterAttackProfile() : nullptr;
	if (!World || !Profile || Assault.IsTerminal()) return;

	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	const ETerritoryAssaultState InitialState = Assault.State;
	TArray<TWeakObjectPtr<APlayerController>> Controllers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		Controllers.Add(It->Get());
	}
	bool bWarnedSomeone = false;
	for (const TWeakObjectPtr<APlayerController>& WeakController : Controllers)
	{
		if (!IsAssaultCurrent(Access, InitialState) || !IsValid(Territory)) return;
		APlayerController* Controller = WeakController.Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Controller || !Pawn || WarnedControllers.FindOrAdd(Access.ID).Contains(Controller)
			|| !IsRelevantPlayer(Controller, Assault, Profile)
			|| FVector::DistSquared(Pawn->GetActorLocation(), Territory->GetActorLocation())
				> FMath::Square(Profile->NotificationRadius))
		{
			continue;
		}
		WarnedControllers.FindOrAdd(Access.ID).Add(Controller);
		const FTerritoryAssaultRecord Snapshot = Assault;
		if (UTerritoryPlayerManagementComponent* Management =
			UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(Controller))
		{
			Management->SendAssaultNotification(Snapshot);
		}
		if (!IsAssaultCurrent(Access, InitialState)) return;
		OnAssaultWarning.Broadcast(Controller, Snapshot);
		if (!IsAssaultCurrent(Access, InitialState)) return;
		bWarnedSomeone = true;
	}
	if (bWarnedSomeone && !Assault.bNotificationSent)
	{
		Assault.bNotificationSent = true;
		BroadcastChanged(Assault);
	}
}

void UTerritoryCounterAttackSubsystem::NotifyRelevantPlayersOfState(
	const FTerritoryCounterAttackStateEvent& Event, ATerritoryVolume* Territory)
{
	UWorld* World = GetWorld();
	UTerritoryCounterAttackProfile* Profile = Territory
		? Territory->GetCounterAttackProfile() : nullptr;
	if (!World || !Profile) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Controller || !Pawn || !IsRelevantPlayer(Controller, Event.Assault, Profile)
			|| FVector::DistSquared(Pawn->GetActorLocation(), Territory->GetActorLocation())
				> FMath::Square(Profile->NotificationRadius))
		{
			continue;
		}
		if (UTerritoryPlayerManagementComponent* Management =
			UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(Controller))
		{
			Management->SendCounterHappened(Event);
		}
	}
}

bool UTerritoryCounterAttackSubsystem::DoesAssaultTargetTerritory(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory)
{
	if (!Territory) return false;
	if (Assault.TargetTerritoryGUID.IsValid())
	{
		return Assault.TargetTerritoryGUID == Territory->GetTerritoryGUID();
	}
	return Assault.TargetTerritory.IsValid()
		&& Assault.TargetTerritory == Territory->GetTerritoryTag();
}

bool UTerritoryCounterAttackSubsystem::ReconcileAssaultTargetIdentity(
	FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory)
{
	if (!DoesAssaultTargetTerritory(Assault, Territory)) return false;
	bool bChanged = false;
	if (!Assault.TargetTerritoryGUID.IsValid() && Territory->GetTerritoryGUID().IsValid())
	{
		Assault.TargetTerritoryGUID = Territory->GetTerritoryGUID();
		bChanged = true;
	}
	if (Territory->GetTerritoryTag().IsValid()
		&& Assault.TargetTerritory != Territory->GetTerritoryTag())
	{
		Assault.TargetTerritory = Territory->GetTerritoryTag();
		bChanged = true;
	}
	return bChanged;
}

bool UTerritoryCounterAttackSubsystem::HasNonTerminalAssaultForTerritory(
	const ATerritoryVolume* Territory) const
{
	if (!Territory) return false;
	for (const auto& Pair : Assaults)
	{
		if (!Pair.Value.IsTerminal()
			&& DoesAssaultTargetTerritory(Pair.Value, Territory))
		{
			return true;
		}
	}
	return false;
}

ATerritoryVolume* UTerritoryCounterAttackSubsystem::ResolveTerritory(
	const FTerritoryAssaultRecord& Assault) const
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;
	if (Assault.TargetTerritoryGUID.IsValid())
	{
		// Never retarget a durable assault merely because another actor reused its tag.
		return Registry->GetTerritoryByGUID(Assault.TargetTerritoryGUID);
	}
	return Assault.TargetTerritory.IsValid()
		? Registry->GetTerritoryByTag(Assault.TargetTerritory) : nullptr;
}

bool UTerritoryCounterAttackSubsystem::HasRelevantPlayerNearby(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory, float Radius) const
{
	UWorld* World = GetWorld();
	if (!World || !Territory) return false;
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (Pawn && IsRelevantPlayer(Controller, Assault, Profile)
			&& FVector::DistSquared(Pawn->GetActorLocation(), Territory->GetActorLocation())
			<= FMath::Square(Radius))
		{
			return true;
		}
	}
	return false;
}

void UTerritoryCounterAttackSubsystem::GetDefendingPlayerPresence(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory,
	bool& bOutAliveInside, bool& bOutDeadInside) const
{
	bOutAliveInside = false;
	bOutDeadInside = false;
	UWorld* World = GetWorld();
	if (!World || !Territory || !Assault.DefendingFaction.IsValid()) return;

	const TArray<ATerritoryVolume*> DefenceFront =
		TerritoryAssaultTargetPolicy::BuildDefenceFront(
			const_cast<ATerritoryVolume*>(Territory));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn) continue;
		const bool bDefendingFaction =
			UTerritoryBlueprintLibrary::IsActorInFaction(this, Pawn, Assault.DefendingFaction)
			|| UTerritoryBlueprintLibrary::IsActorInFaction(
				this, Controller, Assault.DefendingFaction);
		if (!bDefendingFaction) continue;

		bool bInside = false;
		for (const ATerritoryVolume* Defence : DefenceFront)
		{
			if (Defence && Defence->ContainsPoint(Pawn->GetActorLocation()))
			{
				bInside = true;
				break;
			}
		}
		if (!bInside) continue;

		bool bDead = false;
		if (const UNarrativeAbilitySystemComponent* ASC =
			FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Pawn))
		{
			bDead = ASC->IsDead();
		}
		bOutDeadInside |= bDead;
		bOutAliveInside |= !bDead;
	}
}

bool UTerritoryCounterAttackSubsystem::HasPhysicalAttackerInside(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory) const
{
	if (!Territory) return false;
	const TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants =
		LiveParticipants.Find(Assault.AssaultID);
	if (!Participants) return false;
	for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& WeakParticipant : *Participants)
	{
		const ATerritoryAssaultCharacter* Participant = WeakParticipant.Get();
		if (IsValid(Participant) && !Participant->IsActorBeingDestroyed()
			&& Territory->ContainsPoint(Participant->GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

bool UTerritoryCounterAttackSubsystem::CompleteRecapture(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	UWorld* World = GetWorld();
	UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Control || !Territory || !Assault.AttackingFaction.IsValid()) return false;

	FTerritoryTransitionContext Context;
	Context.RequestingFaction = Assault.AttackingFaction;
	if (const TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants =
		LiveParticipants.Find(Assault.AssaultID))
	{
		for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& WeakParticipant : *Participants)
		{
			if (ATerritoryAssaultCharacter* Participant = WeakParticipant.Get())
			{
				if (!Territory->ContainsPoint(Participant->GetActorLocation())) continue;
				Context.Instigator = Participant;
				Context.TargetPawn = Participant;
				break;
			}
		}
	}
	return Control->ForceCaptureWithContext(
		Territory, Assault.AttackingFaction, Context);
}

APawn* UTerritoryCounterAttackSubsystem::FindNearestRelevantPlayer(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory,
	float Radius) const
{
	UWorld* World = GetWorld();
	if (!World || !Territory) return nullptr;
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const float RadiusSquared = FMath::Square(FMath::Max(0.f, Radius));
	float BestDistanceSquared = TNumericLimits<float>::Max();
	APawn* BestPawn = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || !IsRelevantPlayer(Controller, Assault, Profile)) continue;
		const float DistanceSquared = FVector::DistSquared(
			Pawn->GetActorLocation(), Territory->GetActorLocation());
		if (DistanceSquared <= RadiusSquared && DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestPawn = Pawn;
		}
	}
	return BestPawn;
}

int32 UTerritoryCounterAttackSubsystem::ResolveScaledEnemyLevel(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& ForceConfig) const
{
	if (!Territory || !ForceConfig.bScaleLevelToRelevantPlayerPower) return INDEX_NONE;
	UWorld* World = GetWorld();
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	if (!World || !Profile) return INDEX_NONE;

	int32 StrongestPlayerPower = 0;
	const float RadiusSquared = FMath::Square(FMath::Max(
		Profile->NotificationRadius, Profile->ReserveMaximumPlayerDistance));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || !IsRelevantPlayer(Controller, Assault, Profile)
			|| FVector::DistSquared(Pawn->GetActorLocation(), Territory->GetActorLocation())
				> RadiusSquared)
		{
			continue;
		}

		if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Pawn))
		{
			StrongestPlayerPower = FMath::Max(
				StrongestPlayerPower, Character->GetCharacterLevel());
		}
		if (const UAbilitySystemComponent* ASC =
			FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Pawn))
		{
			for (const FTerritoryPlayerPowerTier& Tier : ForceConfig.PlayerPowerTiers)
			{
				if (Tier.PlayerPowerTag.IsValid()
					&& ASC->HasMatchingGameplayTag(Tier.PlayerPowerTag))
				{
					StrongestPlayerPower = FMath::Max(
						StrongestPlayerPower, Tier.PlayerPowerLevel);
				}
			}
		}
	}
	if (StrongestPlayerPower <= 0) return INDEX_NONE;
	const int32 Minimum = FMath::Max(1, ForceConfig.MinimumScaledEnemyLevel);
	const int32 Maximum = FMath::Max(Minimum, ForceConfig.MaximumScaledEnemyLevel);
	return FMath::Clamp(StrongestPlayerPower + ForceConfig.EnemyLevelOffset,
		Minimum, Maximum);
}

bool UTerritoryCounterAttackSubsystem::IsRelevantPlayer(
	const APlayerController* Controller, const FTerritoryAssaultRecord& Assault,
	const UTerritoryCounterAttackProfile* Profile) const
{
	if (!Controller || !Profile) return false;
	AActor* PlayerPawn = Controller->GetPawn().Get();
	return !Profile->bNotifyDefendingFactionOnly
		|| UTerritoryBlueprintLibrary::IsActorInFaction(
			this, PlayerPawn, Assault.DefendingFaction)
		|| UTerritoryBlueprintLibrary::IsActorInFaction(
			this, const_cast<APlayerController*>(Controller), Assault.DefendingFaction);
}

bool UTerritoryCounterAttackSubsystem::IsDiplomacyBlocked(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory) const
{
	UWorld* World = GetWorld();
	const UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Control || !Control->CanFactionCaptureTerritory(Territory, Assault.AttackingFaction))
	{
		return true;
	}

	// Physical counterattack NPCs need an explicit rich War relationship.
	// None means Neutral / No Treaty and must never inherit a stale hostile entry
	// from Narrative's map. Territory diplomacy is the campaign authority and its
	// bridge repairs Narrative's ETeamAttitude projection separately.
	if (!Territory || !World) return true;
	const FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (!DefendingFaction.IsValid()) return true;
	if (const UTerritoryDiplomacySubsystem* Diplomacy =
		World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		return !Diplomacy->IsAtWar(Assault.AttackingFaction, DefendingFaction);
	}
	return true;
}

TArray<FName> UTerritoryCounterAttackSubsystem::SelectValidApproaches(
	const ATerritoryVolume* Territory, const UTerritoryCounterAttackProfile* Profile,
	float PowerRatio) const
{
	TArray<FName> Valid;
	if (!Territory || !Profile) return Valid;
	TArray<FTerritoryAssaultApproach> Approaches = Territory->GetCounterAttackApproaches();
	Approaches.Sort([](const FTerritoryAssaultApproach& A, const FTerritoryAssaultApproach& B)
	{
		return A.ApproachID.LexicalLess(B.ApproachID);
	});
	for (const FTerritoryAssaultApproach& Approach : Approaches)
	{
		if (!Approach.bEnabled || Approach.ApproachID.IsNone()) continue;
		const FTransform WorldTransform = Approach.RelativeSpawnTransform * Territory->GetActorTransform();
		FVector Objective;
		FTransform FootDeploymentTransform;
		if (ResolveApproachObjective(Territory, Approach, WorldTransform,
			Objective, FootDeploymentTransform))
		{
			Valid.AddUnique(Approach.ApproachID);
		}
	}
	const int32 DesiredApproaches = FMath::Clamp(
		1 + FMath::FloorToInt(FMath::Max(0.f, PowerRatio - 1.f)), 1, Profile->MaximumApproaches);
	if (Valid.Num() > DesiredApproaches)
	{
		TArray<FName> Selected;
		if (DesiredApproaches >= 2)
		{
			for (ETerritoryAssaultEntryType EntryType : {
				ETerritoryAssaultEntryType::NarrativeVehicle,
				ETerritoryAssaultEntryType::OnFoot })
			{
				if (const FTerritoryAssaultApproach* DiverseApproach = Approaches.FindByPredicate(
					[&Valid, EntryType](const FTerritoryAssaultApproach& Candidate)
					{
						return Candidate.EntryType == EntryType
							&& Valid.Contains(Candidate.ApproachID);
					}))
				{
					Selected.AddUnique(DiverseApproach->ApproachID);
				}
			}
		}
		for (const FName ApproachID : Valid)
		{
			if (Selected.Num() >= DesiredApproaches) break;
			Selected.AddUnique(ApproachID);
		}
		Valid = MoveTemp(Selected);
	}
	return Valid;
}

bool UTerritoryCounterAttackSubsystem::ResolveApproach(
	const ATerritoryVolume* Territory, FName ApproachID,
	FTerritoryAssaultApproach& OutApproach, FTransform& OutWorldTransform) const
{
	if (!Territory || ApproachID.IsNone()) return false;
	const FTerritoryAssaultApproach* Found = Territory->GetCounterAttackApproaches().FindByPredicate(
		[ApproachID](const FTerritoryAssaultApproach& Approach)
		{
			return Approach.bEnabled && Approach.ApproachID == ApproachID;
		});
	if (!Found) return false;
	OutApproach = *Found;
	if (Found->EntryType == ETerritoryAssaultEntryType::NarrativeVehicle)
	{
		if (const ATerritoryRoadGuide* Guide = ResolveRoadGuide(*Found))
		{
			OutWorldTransform = Guide->GetRouteStartTransform(
				false, Found->RoadLaneSide);
		}
		else
		{
			OutWorldTransform = Found->RelativeSpawnTransform
				* Territory->GetActorTransform();
		}
	}
	else
	{
		OutWorldTransform = Found->RelativeSpawnTransform
			* Territory->GetActorTransform();
	}
	FVector Objective;
	FTransform FootDeploymentTransform;
	return ResolveApproachObjective(Territory, *Found, OutWorldTransform,
		Objective, FootDeploymentTransform);
}

bool UTerritoryCounterAttackSubsystem::ResolveApproachObjective(
	const ATerritoryVolume* Territory, const FTerritoryAssaultApproach& Approach,
	const FTransform& ApproachWorldTransform, FVector& OutObjective,
	FTransform& OutFootDeploymentTransform,
	FTransform* OutVehicleDropOffTransform, FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const FString& Message)
	{
		if (OutFailureReason) *OutFailureReason = Message;
		return false;
	};
	if (!Territory || !GetWorld()) return Fail(TEXT("missing Territory world"));

	if (Approach.EntryType == ETerritoryAssaultEntryType::OnFoot)
	{
		OutFootDeploymentTransform = ApproachWorldTransform;
		return FindReachableObjective(Territory,
			ApproachWorldTransform.GetLocation(), OutObjective, OutFailureReason);
	}

	UClass* VehicleClass = Approach.VehicleClass.LoadSynchronous();
	if (!VehicleClass || !VehicleClass->IsChildOf(ANarrativeVehicleBase::StaticClass()))
	{
		bool bHasFactionSignature = false;
		if (const UTerritoryCounterAttackProfile* Profile =
			Territory->GetCounterAttackProfile())
		{
			for (const FTerritoryFactionAssaultConfig& Force : Profile->FactionForces)
			{
				UClass* SignatureClass = Force.SignatureVehicleClass.LoadSynchronous();
				if (SignatureClass
					&& SignatureClass->IsChildOf(ANarrativeVehicleBase::StaticClass()))
				{
					bHasFactionSignature = true;
					break;
				}
			}
		}
		if (!bHasFactionSignature)
		{
			return Fail(TEXT("Narrative Vehicle entry has neither an Approach fallback car nor a valid faction signature car"));
		}
	}

	const ATerritoryRoadGuide* RoadGuide = ResolveRoadGuide(Approach);
	const FTransform DropOffTransform = RoadGuide
		? RoadGuide->GetRouteEndTransform(false, Approach.RoadLaneSide)
		: Approach.RelativeVehicleDropOffTransform * Territory->GetActorTransform();
	if (OutVehicleDropOffTransform) *OutVehicleDropOffTransform = DropOffTransform;
	OutFootDeploymentTransform = DropOffTransform;

	if (RoadGuide)
	{
		TArray<FVector> RoutePoints;
		FText GuideFailure;
		if (!RoadGuide->BuildRoutePoints(false, Approach.RoadLaneSide,
			RoutePoints, GuideFailure))
		{
			return Fail(FString::Printf(TEXT("Road Guide '%s' is invalid: %s"),
				*RoadGuide->GetRoadGuideID().ToString(),
				*GuideFailure.ToString()));
		}
	}
	else if (!ApproachWorldTransform.GetLocation().Equals(
		DropOffTransform.GetLocation(), 100.f))
	{
		if (!ValidateNarrativeVehicleRoute(GetWorld(),
			ApproachWorldTransform.GetLocation(), DropOffTransform.GetLocation()))
		{
			return Fail(TEXT("no complete Narrative ZoneGraph vehicle route from spawn to drop-off"));
		}
	}

	FString WalkFailure;
	if (!FindReachableObjective(Territory, DropOffTransform.GetLocation(),
		OutObjective, &WalkFailure))
	{
		return Fail(FString::Printf(
			TEXT("vehicle reaches drop-off but no NavMesh walk route enters the Place: %s"),
			*WalkFailure));
	}
	if (OutFailureReason) OutFailureReason->Reset();
	return true;
}

ATerritoryRoadGuide* UTerritoryCounterAttackSubsystem::ResolveRoadGuide(
	const FTerritoryAssaultApproach& Approach) const
{
	UWorld* World = GetWorld();
	if (!World || Approach.EntryType != ETerritoryAssaultEntryType::NarrativeVehicle)
	{
		return nullptr;
	}
	const FName RequestedID = Approach.RoadGuideID.IsNone()
		? Approach.ApproachID : Approach.RoadGuideID;
	if (RequestedID.IsNone()) return nullptr;
	ATerritoryRoadGuide* Best = nullptr;
	for (TActorIterator<ATerritoryRoadGuide> It(World); It; ++It)
	{
		ATerritoryRoadGuide* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->GetRoadGuideID() != RequestedID) continue;
		if (!Best || Candidate->GetPathName() < Best->GetPathName()) Best = Candidate;
	}
	return Best;
}

bool UTerritoryCounterAttackSubsystem::FindReachableObjective(
	const ATerritoryVolume* Territory, const FVector& Start, FVector& OutObjective,
	FString* OutFailureReason) const
{
	if (!Territory)
	{
		if (OutFailureReason) *OutFailureReason = TEXT("No target Territory is available");
		return false;
	}
	FString LastFailure = TEXT("No physical objective is configured");
	for (const FVector& Candidate : TerritoryAssaultTargetPolicy::BuildObjectiveLocations(
		const_cast<ATerritoryVolume*>(Territory), true))
	{
		if (ValidateNavigationRoute(GetWorld(), Start, Candidate, &LastFailure))
		{
			OutObjective = Candidate;
			if (OutFailureReason) OutFailureReason->Reset();
			return true;
		}
	}
	if (OutFailureReason) *OutFailureReason = LastFailure;
	return false;
}

bool UTerritoryCounterAttackSubsystem::IsDeploymentLocationSeparated(
	const FVector& Location, float MinimumSpacing) const
{
	const float MinimumDistanceSquared = FMath::Square(FMath::Max(0.f, MinimumSpacing));
	for (const auto& Pair : LiveParticipants)
	{
		for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& WeakParticipant : Pair.Value)
		{
			const ATerritoryAssaultCharacter* Participant = WeakParticipant.Get();
			if (Participant && FVector::DistSquared2D(
				Participant->GetActorLocation(), Location) < MinimumDistanceSquared)
			{
				return false;
			}
		}
	}
	return true;
}

bool UTerritoryCounterAttackSubsystem::HasNavigationRoute(
	const FVector& Start, const FVector& End) const
{
	return ValidateNavigationRoute(GetWorld(), Start, End);
}

bool UTerritoryCounterAttackSubsystem::ValidateNavigationRoute(UWorld* World,
	const FVector& Start, const FVector& End, FString* OutFailureReason)
{
	auto Fail = [OutFailureReason](const TCHAR* Message)
	{
		if (OutFailureReason) *OutFailureReason = Message;
		return false;
	};
	if (!World) return Fail(TEXT("No world is available for navigation validation"));
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav) return Fail(TEXT("No navigation system or built navmesh is available"));
	FNavLocation ProjectedStart;
	FNavLocation ProjectedEnd;
	if (!Nav->ProjectPointToNavigation(Start, ProjectedStart, FVector(500.f)))
	{
		return Fail(TEXT("Approach spawn is farther than 500 cm from navigable ground"));
	}
	if (!Nav->ProjectPointToNavigation(End, ProjectedEnd, FVector(1000.f)))
	{
		return Fail(TEXT("Territory target is farther than 1000 cm from navigable ground"));
	}
	UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
		World, ProjectedStart.Location, ProjectedEnd.Location);
	if (!Path || !Path->IsValid())
	{
		return Fail(TEXT("Navigation could not build a path from the approach to the Territory"));
	}
	if (Path->IsPartial())
	{
		return Fail(TEXT("Only a partial path exists from the approach to the Territory"));
	}
	if (OutFailureReason) OutFailureReason->Reset();
	return true;
}

bool UTerritoryCounterAttackSubsystem::ValidateNarrativeVehicleRoute(
	UWorld* World, const FVector& Start, const FVector& End,
	FString* OutFailureReason)
{
	TArray<FVector> RoutePoints;
	return BuildNarrativeVehicleRoute(
		World, Start, End, RoutePoints, OutFailureReason);
}

bool UTerritoryCounterAttackSubsystem::BuildNarrativeVehicleRoute(
	UWorld* World, const FVector& Start, const FVector& End,
	TArray<FVector>& OutRoutePoints, FString* OutFailureReason)
{
	OutRoutePoints.Reset();
	auto Fail = [OutFailureReason](const TCHAR* Message)
	{
		if (OutFailureReason) *OutFailureReason = Message;
		return false;
	};
	if (!World) return Fail(TEXT("No world is available for ZoneGraph validation"));
	UZoneGraphSubsystem* ZoneGraph = World->GetSubsystem<UZoneGraphSubsystem>();
	if (!ZoneGraph) return Fail(TEXT("No ZoneGraph subsystem is available"));

	FZoneGraphLaneLocation StartLane;
	FZoneGraphLaneLocation EndLane;
	FZoneGraphTagFilter Filter;
	float DistanceSquared = 0.f;
	const FVector SearchExtent(1000.f);
	ZoneGraph->FindNearestLane(FBox::BuildAABB(Start, SearchExtent),
		Filter, StartLane, DistanceSquared);
	ZoneGraph->FindNearestLane(FBox::BuildAABB(End, SearchExtent),
		Filter, EndLane, DistanceSquared);
	if (!StartLane.IsValid() || !EndLane.IsValid())
	{
		return Fail(TEXT("Vehicle spawn or drop-off is farther than 1000 cm from a ZoneGraph lane"));
	}
	if (StartLane.LaneHandle.DataHandle != EndLane.LaneHandle.DataHandle)
	{
		return Fail(TEXT("Vehicle spawn and drop-off belong to disconnected ZoneGraph data"));
	}

	const AZoneGraphData* Data =
		ZoneGraph->GetZoneGraphData(StartLane.LaneHandle.DataHandle);
	if (!Data) return Fail(TEXT("ZoneGraph lane storage is unavailable"));
	const FZoneGraphStorage& Storage = Data->GetStorage();
	FZoneGraphAStarWrapper Graph(Storage);
	FZoneGraphAStar Pathfinder(Graph);
	FZoneGraphAStarNode StartNode(StartLane.LaneHandle.Index, StartLane.Position);
	FZoneGraphAStarNode EndNode(EndLane.LaneHandle.Index, EndLane.Position);
	FZoneGraphPathFilter PathFilter(Storage, StartLane, EndLane, Filter);
	TArray<FZoneGraphAStarWrapper::FNodeRef> ResultPath;
	if (Pathfinder.FindPath(StartNode, EndNode, PathFilter, ResultPath)
		!= EGraphAStarResult::SearchSuccess)
	{
		return Fail(TEXT("ZoneGraph could not build a complete vehicle route"));
	}

	auto AddUniquePoint = [&OutRoutePoints](const FVector& Point)
	{
		if (OutRoutePoints.IsEmpty()
			|| !OutRoutePoints.Last().Equals(Point, 25.f))
		{
			OutRoutePoints.Add(Point);
		}
	};
	AddUniquePoint(Start);
	AddUniquePoint(StartLane.Position);
	constexpr float SampleSpacing = 400.f;
	for (int32 PathIndex = 0; PathIndex < ResultPath.Num(); ++PathIndex)
	{
		const FZoneGraphLaneHandle LaneHandle(ResultPath[PathIndex],
			StartLane.LaneHandle.DataHandle);
		float LaneLength = 0.f;
		if (!ZoneGraph->GetLaneLength(LaneHandle, LaneLength))
		{
			return Fail(TEXT("ZoneGraph route contains an invalid lane"));
		}
		const float LaneStart = PathIndex == 0
			? StartLane.DistanceAlongLane : 0.f;
		const float LaneEnd = PathIndex == ResultPath.Num() - 1
			? EndLane.DistanceAlongLane : LaneLength;
		const float Direction = LaneEnd >= LaneStart ? 1.f : -1.f;
		for (float Distance = LaneStart;
			Direction > 0.f ? Distance < LaneEnd : Distance > LaneEnd;
			Distance += Direction * SampleSpacing)
		{
			FZoneGraphLaneLocation LaneLocation;
			if (ZoneGraph->CalculateLocationAlongLane(
				LaneHandle, FMath::Clamp(Distance, 0.f, LaneLength), LaneLocation))
			{
				AddUniquePoint(LaneLocation.Position);
			}
		}
		FZoneGraphLaneLocation LaneEndLocation;
		if (ZoneGraph->CalculateLocationAlongLane(
			LaneHandle, FMath::Clamp(LaneEnd, 0.f, LaneLength), LaneEndLocation))
		{
			AddUniquePoint(LaneEndLocation.Position);
		}
	}
	AddUniquePoint(EndLane.Position);
	AddUniquePoint(End);
	if (OutRoutePoints.Num() < 2)
	{
		return Fail(TEXT("ZoneGraph route produced fewer than two drive points"));
	}
	if (OutFailureReason) OutFailureReason->Reset();
	return true;
}

FTerritoryAssaultEvaluationInput UTerritoryCounterAttackSubsystem::BuildEvaluationInput(
	const ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& ForceConfig) const
{
	FTerritoryAssaultEvaluationInput Input;
	if (!Territory) return Input;

	// A physical assault still targets exactly one existing capture authority. Strategic
	// defence, however, cascades through its loaded District front: the target, its District,
	// and same-owner sibling Properties. This lets nearby guards/support deter a strike without
	// inventing a second capture system or letting another faction's garrison defend the owner.
	const TArray<ATerritoryVolume*> DefenceTerritories =
		TerritoryAssaultTargetPolicy::BuildDefenceFront(
			const_cast<ATerritoryVolume*>(Territory));

	const FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	float WeightedQuality = 0.f;
	float QualityWeight = 0.f;
	for (const ATerritoryVolume* Defence : DefenceTerritories)
	{
		if (!Defence || (Defence != Territory
			&& Defence->GetOwningFaction() != DefendingFaction))
		{
			continue;
		}
		const int32 Active = FMath::Max(0, Defence->GetSpawnedGuardCount());
		const int32 Desired = FMath::Max(0, Defence->GetDesiredGuardCount());
		int32 RawReserve = 0;
		for (const ATerritoryGuardSpawnPoint* SpawnPoint : Defence->GetGuardSpawnPoints())
		{
			if (SpawnPoint) RawReserve += FMath::Max(0, SpawnPoint->GetReserveCount());
		}
		// A reserve is a finite replacement entitlement for an authorized staffing
		// target, not a hidden defender. Posts outside DesiredGuardCount cannot create
		// capture pressure, so they must not inflate an empty/player-unstaffed District.
		const int32 EffectiveReserve = CalculateEffectiveReserveGuards(
			RawReserve, Desired);
		Input.ActiveGuards += Active;
		Input.DesiredGuards += Desired;
		Input.MaximumGuards += FMath::Max(0, Defence->GetMaxGuardCount());
		Input.ReserveGuards += EffectiveReserve;
		const float Weight = static_cast<float>(Active)
			+ 0.5f * static_cast<float>(EffectiveReserve);
		WeightedQuality += FMath::Max(0.f, Defence->GetGuardQuality()) * Weight;
		QualityWeight += Weight;
		Input.Fortification += FMath::Max(0.f, Defence->GetFortificationStrength());
		Input.NearbyAlliedSupport += FMath::Max(0.f, Defence->GetNearbyAlliedSupport());
		Input.StrategicValue += FMath::Max(0.f, Defence->GetStrategicValue());
	}
	Input.GuardQuality = QualityWeight > KINDA_SMALL_NUMBER
		? WeightedQuality / QualityWeight : FMath::Max(0.f, Territory->GetGuardQuality());
	Input.AttackingMilitaryPower = ForceConfig.MilitaryPower;
	Input.EconomyReadiness = ForceConfig.EconomyReadiness;
	Input.SupplyReadiness = ForceConfig.SupplyReadiness;
	// The default struct value is one; remove it after accumulating authored values so
	// one target with StrategicValue=1 remains one instead of being silently doubled.
	Input.StrategicValue = FMath::Max(0.f, Input.StrategicValue - 1.f);
	Input.RecentMomentum = ForceConfig.RecentMomentum;
	Input.FactionInfluence = ForceConfig.TerritorialInfluence;
	return Input;
}

double UTerritoryCounterAttackSubsystem::GetCampaignGameTime() const
{
	UWorld* World = GetWorld();
	if (!World) return 0.0;
	if (const ANarrativeGameState* GameState = Cast<ANarrativeGameState>(World->GetGameState()))
	{
		return GameState->GetAccumulatedTime();
	}
	return World->GetTimeSeconds();
}

float UTerritoryCounterAttackSubsystem::GetNarrativeTimeOfDay() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;
	if (const ANarrativeGameState* GameState =
		Cast<ANarrativeGameState>(World->GetGameState()))
	{
		return GameState->GetTimeOfDay();
	}
	// ScheduleAssault already requires Narrative Game State. This fallback keeps
	// the pure adapter deterministic during early world initialization.
	return FMath::Fmod(World->GetTimeSeconds(), 2400.f);
}

bool UTerritoryCounterAttackSubsystem::IsForceTimeWindowOpen(
	const FTerritoryFactionAssaultConfig& ForceConfig) const
{
	return ForceConfig.TimePolicy == ETerritoryCounterTimePolicy::AnyTime
		|| IsNarrativeTimeInWindow(GetNarrativeTimeOfDay(),
			ForceConfig.TimeWindowStart, ForceConfig.TimeWindowEnd);
}

int32 UTerritoryCounterAttackSubsystem::MakeDecisionSeed(
	const ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction,
	int32 EvaluationCycle) const
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	uint32 Seed = GetTypeHash(Settings ? Settings->CounterAttackCampaignSeed : 1337);
	const FGuid TerritoryGuid = Territory ? Territory->GetTerritoryGUID() : FGuid();
	Seed = HashCombineFast(Seed, GetTypeHash(TerritoryGuid.A));
	Seed = HashCombineFast(Seed, GetTypeHash(TerritoryGuid.B));
	Seed = HashCombineFast(Seed, GetTypeHash(TerritoryGuid.C));
	Seed = HashCombineFast(Seed, GetTypeHash(TerritoryGuid.D));
	Seed = HashCombineFast(Seed, FCrc::StrCrc32(*AttackingFaction.ToString()));
	Seed = HashCombineFast(Seed, GetTypeHash(EvaluationCycle));
	return static_cast<int32>(Seed);
}

int32 UTerritoryCounterAttackSubsystem::ReserveNextEvaluationCycle(
	const FGuid& TerritoryGUID, const FGameplayTag& AttackingFaction)
{
	if (!TerritoryGUID.IsValid() || !AttackingFaction.IsValid()) return 0;
	int32& HighWater = EvaluationCycleHighWater.FindOrAdd(
		TerritoryGUID).FindOrAdd(AttackingFaction);
	if (HighWater >= MAX_int32) return 0;
	HighWater = FMath::Max(0, HighWater) + 1;
	return HighWater;
}

int32 UTerritoryCounterAttackSubsystem::CountNonTerminalAssaults(
	const FGameplayTag* OptionalFaction) const
{
	int32 Count = 0;
	for (const auto& Pair : Assaults)
	{
		if (!Pair.Value.IsTerminal()
			&& (!OptionalFaction || Pair.Value.AttackingFaction == *OptionalFaction))
		{
			++Count;
		}
	}
	return Count;
}

int32 UTerritoryCounterAttackSubsystem::CountLiveParticipants() const
{
	int32 Count = 0;
	for (const auto& Pair : LiveParticipants)
	{
		for (const TWeakObjectPtr<ATerritoryAssaultCharacter>& Participant : Pair.Value)
		{
			if (Participant.IsValid()) ++Count;
		}
	}
	return Count;
}

void UTerritoryCounterAttackSubsystem::TrimTerminalHistory()
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const int32 Maximum = Settings ? Settings->MaxRetainedAssaultRecords : 100;
	if (Assaults.Num() <= Maximum) return;

	TArray<FTerritoryAssaultRecord> Terminal;
	for (const auto& Pair : Assaults)
	{
		if (Pair.Value.IsTerminal()) Terminal.Add(Pair.Value);
	}
	Terminal.Sort([](const FTerritoryAssaultRecord& A, const FTerritoryAssaultRecord& B)
	{
		return A.CapturedGameTime < B.CapturedGameTime;
	});
	for (const FTerritoryAssaultRecord& Oldest : Terminal)
	{
		if (Assaults.Num() <= Maximum) break;
		Assaults.Remove(Oldest.AssaultID);
		WarnedControllers.Remove(Oldest.AssaultID);
	}
}

void UTerritoryCounterAttackSubsystem::HandleTerritoryControlChanged(
	ATerritoryVolume* Territory, FGameplayTag OldOwner, FGameplayTag NewOwner)
{
	if (bRestoringState || !Territory || !GetWorld()
		|| GetWorld()->GetNetMode() == NM_Client) return;

	TArray<FGuid> Matching;
	for (const auto& Pair : Assaults)
	{
		if (!Pair.Value.IsTerminal()
			&& DoesAssaultTargetTerritory(Pair.Value, Territory))
		{
			Matching.Add(Pair.Key);
		}
	}
	for (const FGuid& ID : Matching)
	{
		if (FTerritoryAssaultRecord* Assault = Assaults.Find(ID))
		{
			if (NewOwner == Assault->AttackingFaction)
			{
				ResolveAssault(*Assault, ETerritoryAssaultState::Succeeded,
					ETerritoryAssaultResolution::CaptureCompleted);
			}
			else if (NewOwner != Assault->DefendingFaction)
			{
				ResolveAssault(*Assault, ETerritoryAssaultState::Cancelled,
					ETerritoryAssaultResolution::OwnershipChanged);
			}
		}
	}

	if (OldOwner.IsValid() && NewOwner.IsValid() && OldOwner != NewOwner)
	{
		if (!Territory->IsPrimaryRuntimeRuleSuspendedWithContext(
			ETerritoryQuestOverrideEffect::AutomaticCounterattacks, nullptr))
		{
			ScheduleBestCounterAttack(Territory, OldOwner);
		}
	}
}

void UTerritoryCounterAttackSubsystem::HandleDiplomacyChanged(
	FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	(void)NewState;
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
	const UTerritoryDiplomacySubsystem* Diplomacy =
		GetWorld()->GetSubsystem<UTerritoryDiplomacySubsystem>();
	TArray<FGuid> ToCancel;
	for (const auto& Pair : Assaults)
	{
		const FTerritoryAssaultRecord& Assault = Pair.Value;
		const bool bPairMatches =
			(Assault.AttackingFaction == FactionA && Assault.DefendingFaction == FactionB)
			|| (Assault.AttackingFaction == FactionB && Assault.DefendingFaction == FactionA);
		if (bPairMatches && !Assault.IsTerminal())
		{
			if (ATerritoryVolume* Territory = ResolveTerritory(Assault))
			{
				if (IsDiplomacyBlocked(Assault, Territory)) ToCancel.Add(Pair.Key);
			}
			else if (!Diplomacy || !Diplomacy->IsAtWar(Assault.AttackingFaction, Assault.DefendingFaction))
			{
				// Durable faction identity remains available while the target is streamed out.
				ToCancel.Add(Pair.Key);
			}
		}
	}
	for (const FGuid& ID : ToCancel)
	{
		FTerritoryAssaultRecord* Assault = Assaults.Find(ID);
		ATerritoryVolume* Territory = Assault ? ResolveTerritory(*Assault) : nullptr;
		// Claimed-state diplomacy events can broadcast before the control-change
		// delegate. Ownership is the stronger fact: record a completed recapture
		// instead of cancelling the same assault as diplomacy-blocked.
		if (Assault && Territory
			&& Territory->GetOwningFaction() == Assault->AttackingFaction)
		{
			ResolveAssault(*Assault, ETerritoryAssaultState::Succeeded,
				ETerritoryAssaultResolution::CaptureCompleted);
		}
		else if (Assault && !Assault->IsTerminal()
			&& (Territory ? IsDiplomacyBlocked(*Assault, Territory)
				: (!Diplomacy || !Diplomacy->IsAtWar(Assault->AttackingFaction, Assault->DefendingFaction))))
		{
			CancelAssault(ID, ETerritoryAssaultResolution::DiplomacyBlocked);
		}
	}
}

void UTerritoryCounterAttackSubsystem::HandleTerritoryRegistered(
	ATerritoryVolume* Territory, bool bIsNew)
{
	(void)bIsNew;
	if (!Territory || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
	TArray<FGuid> TargetAssaults;
	for (const auto& Pair : Assaults)
	{
		if (!Pair.Value.IsTerminal()
			&& DoesAssaultTargetTerritory(Pair.Value, Territory))
		{
			TargetAssaults.Add(Pair.Key);
		}
	}
	for (const FGuid& ID : TargetAssaults)
	{
		if (FTerritoryAssaultRecord* Assault = Assaults.Find(ID);
			Assault && !Assault->IsTerminal() && DoesAssaultTargetTerritory(*Assault, Territory))
		{
			AdvanceAssault(*Assault);
		}
	}
}
