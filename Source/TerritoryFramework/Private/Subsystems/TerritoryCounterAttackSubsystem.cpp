#include "Subsystems/TerritoryCounterAttackSubsystem.h"

#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NPCDefinition.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryQuestRules.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Misc/Crc.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

void UTerritoryCounterAttackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
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
	bool bRequireRecurringEligibility) const
{
	OutAttackingFaction = FGameplayTag();
	OutInput = FTerritoryAssaultEvaluationInput();
	OutResult = FTerritoryAssaultEvaluationResult();
	OutReason = FText::GetEmpty();
	if (!Territory || Territory->GetTerritoryState() != ETerritoryState::Claimed
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
		if (!DoesForceMeetStagingRequirement(
			Force, ETerritoryAssaultLaunchMode::StrategicCounterattack))
		{
			++StagingBlockedCount;
			continue;
		}
		FText QuestFailureReason;
		if (!DoStrategicQuestRulesPass(Profile, Force.Faction,
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
			if (!Force.bEnableRecurringStrategicCounters || !Previous
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

bool UTerritoryCounterAttackSubsystem::ScheduleBestCounterAttack(
	ATerritoryVolume* Territory, FGameplayTag PreferredFaction)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Territory
		|| Territory->GetWorld() != World || !Territory->HasAuthority())
	{
		return false;
	}

	FGameplayTag AttackingFaction;
	FTerritoryAssaultEvaluationInput Input;
	FTerritoryAssaultEvaluationResult Result;
	FText Reason;
	return FindBestEligibleAttacker(Territory, PreferredFaction,
		AttackingFaction, Input, Result, Reason)
		&& ScheduleCounterAttack(Territory, AttackingFaction);
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
	return ScheduleAssault(Territory, AttackingFaction,
		ETerritoryAssaultLaunchMode::StoryPursuit);
}

bool UTerritoryCounterAttackSubsystem::ScheduleAssault(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction,
	ETerritoryAssaultLaunchMode LaunchMode)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client
		|| !Cast<ANarrativeGameState>(World->GetGameState()) || !Territory
		|| Territory->GetWorld() != World || !Territory->HasAuthority()
		|| !Territory->GetTerritoryGUID().IsValid()
		|| !Territory->GetTerritoryTag().IsValid()
		|| Territory->GetTerritoryState() != ETerritoryState::Claimed
		|| !AttackingFaction.IsValid() || !Territory->GetOwningFaction().IsValid()
		|| Territory->GetOwningFaction() == AttackingFaction)
	{
		return false;
	}

	UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const FTerritoryFactionAssaultConfig* ForceConfig = Profile
		? Profile->FindFactionForce(AttackingFaction) : nullptr;
	if (!Profile || !ForceConfig || ForceConfig->PlannedForce <= 0 || ForceConfig->WaveSize <= 0
		|| ForceConfig->MilitaryPower <= 0.f)
	{
		return false;
	}
	if (!DoesForceMeetStagingRequirement(*ForceConfig, LaunchMode)) return false;
	if (LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& !DoStrategicQuestRulesPass(Profile, AttackingFaction,
			Territory->GetOwningFaction()))
	{
		return false;
	}
	FText SpawnClassFailure;
	if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
		ForceConfig->AttackerDefinition, ForceConfig->PlannedForce, SpawnClassFailure))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack definition %s is not physically spawn-ready: %s"),
			*GetNameSafe(ForceConfig->AttackerDefinition), *SpawnClassFailure.ToString());
		return false;
	}
	FTerritoryAssaultRecord AdmissionRecord;
	AdmissionRecord.AttackingFaction = AttackingFaction;
	AdmissionRecord.DefendingFaction = Territory->GetOwningFaction();
	if (IsDiplomacyBlocked(AdmissionRecord, Territory)) return false;

	if (HasNonTerminalAssaultForTerritory(Territory)) return false;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && (CountNonTerminalAssaults() >= Settings->MaxConcurrentScheduledAssaults
		|| CountNonTerminalAssaults(&AttackingFaction) >= Settings->MaxConcurrentAssaultsPerFaction))
	{
		return false;
	}

	const int32 EvaluationCycle = ReserveNextEvaluationCycle(
		Territory->GetTerritoryGUID(), AttackingFaction);
	if (EvaluationCycle <= 0) return false;

	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid::NewGuid();
	Record.TargetTerritoryGUID = Territory->GetTerritoryGUID();
	Record.TargetTerritory = Territory->GetTerritoryTag();
	Record.AttackingFaction = AttackingFaction;
	Record.DefendingFaction = Territory->GetOwningFaction();
	Record.LaunchMode = LaunchMode;
	Record.State = ETerritoryAssaultState::Grace;
	Record.EvaluationCycle = EvaluationCycle;
	Record.DecisionSeed = MakeDecisionSeed(Territory, AttackingFaction, EvaluationCycle);
	Record.CapturedGameTime = GetCampaignGameTime();
	Record.GraceEndsGameTime = Record.CapturedGameTime + CalculateInfluenceAdjustedDelay(
		Profile->GracePeriodGameTime, ForceConfig->TerritorialInfluence,
		Profile->MinimumInfluenceTimingScale);
	Assaults.Add(Record.AssaultID, Record);
	BroadcastChanged(Record);
	return true;
}

bool UTerritoryCounterAttackSubsystem::CancelAssault(
	FGuid AssaultID, ETerritoryAssaultResolution Reason)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return false;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	if (!Assault || Assault->IsTerminal()) return false;
	ResolveAssault(*Assault, ETerritoryAssaultState::Cancelled, Reason);
	return true;
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
		TEXT("Target=%s Attack=%s Defend=%s LaunchMode=%s SecureDistricts=%d Guards=%d/%d/%d Reserve=%d AttackPower=%.2f Influence=%.2f DefencePower=%.2f PowerRatio=%.3f Strategic=%.2f Priority=%.3f LaunchP=%.3f SuccessP=%.3f Force(P/A/R/K/W)=%d/%d/%d/%d/%d SpawnFailures=%d Approaches=%s State=%s Notification=%s Proximity=%s GraceEnd=%.2f Resolved=%.2f Roll=%.4f Reason=%s"),
		*Assault->TargetTerritory.ToString(), *Assault->AttackingFaction.ToString(),
		*Assault->DefendingFaction.ToString(), *LaunchModeName,
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
	if (!Faction.IsValid()) return 0;
	UWorld* World = GetWorld();
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return 0;
	int32 Count = 0;
	for (const ATerritoryVolume* Territory : Registry->GetAllTerritories())
	{
		const ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Territory);
		const ETerritoryState DistrictState = District
			? District->GetTerritoryState() : ETerritoryState::Unclaimed;
		const bool bSecurelyHeld = DistrictState == ETerritoryState::Claimed
			|| DistrictState == ETerritoryState::Locked;
		if (District && bSecurelyHeld
			&& District->GetOwningFaction() == Faction)
		{
			++Count;
		}
	}
	return Count;
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

	const bool bAuthority = World->GetNetMode() != NM_Client;
	bRestoringState = true;
	if (bAuthority)
	{
		for (auto& Pair : Assaults)
		{
			RetireLiveParticipants(Pair.Value, true);
		}
	}
	LiveParticipants.Empty();
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
	bRestoringState = false;
}

void UTerritoryCounterAttackSubsystem::UpdateAssaults()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || bRestoringState) return;

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
		if (!Target || Target->GetTerritoryState() != ETerritoryState::Claimed
			|| !Target->GetOwningFaction().IsValid())
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
			ScheduleCounterAttack(Target, AttackingFaction);
		}
	}
}

void UTerritoryCounterAttackSubsystem::AdvanceAssault(FTerritoryAssaultRecord& Assault)
{
	if (Assault.IsTerminal()) return;
	ATerritoryVolume* Territory = ResolveTerritory(Assault);
	if (!Territory) return; // World Partition: wait for authoritative actor registration.
	if (ReconcileAssaultTargetIdentity(Assault, Territory))
	{
		// Publish the bounded save migration before any later validation can resolve the assault.
		BroadcastChanged(Assault);
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
			ForceConfig->AttackerDefinition, ForceConfig->PlannedForce, SpawnClassFailure))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}
	const bool bPhysicalAssault = Assault.State == ETerritoryAssaultState::Active
		|| Assault.State == ETerritoryAssaultState::RecaptureCountdown;
	if (Assault.LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
		&& !bPhysicalAssault
		&& !DoesForceMeetStagingRequirement(*ForceConfig, Assault.LaunchMode))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::StagingDistrictUnavailable);
		return;
	}
	if (Assault.LaunchMode == ETerritoryAssaultLaunchMode::StrategicCounterattack
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
	case ETerritoryAssaultState::Grace:
		if (GetCampaignGameTime() >= Assault.GraceEndsGameTime)
		{
			const ETerritoryAssaultState PreviousState = Assault.State;
			Assault.State = ETerritoryAssaultState::Evaluating;
			BroadcastStateTransition(Assault, PreviousState);
			EvaluateAssault(Assault, Territory);
		}
		break;
	case ETerritoryAssaultState::ScheduledWarning:
		NotifyRelevantPlayers(Assault, Territory);
		{
			const ETerritoryAssaultState PreviousState = Assault.State;
			Assault.State = ETerritoryAssaultState::WaitingForPlayerProximity;
			BroadcastStateTransition(Assault, PreviousState);
		}
		break;
	case ETerritoryAssaultState::WaitingForPlayerProximity:
		NotifyRelevantPlayers(Assault, Territory);
		if (Profile)
		{
			if (Territory->GetTerritoryState() == ETerritoryState::Claimed
				&& (!Profile->bRequirePlayerProximityForActivation
					|| HasRelevantPlayerNearby(Assault, Territory, Profile->ActivationRadius)))
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
			if (ShouldDeployActiveReserveWave(Assault, bRelevantPlayerNearby,
				Profile->bContinueFiniteWavesAfterActivation))
			{
				SpawnNextWave(Assault, Territory);
			}
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
	if (!Profile || !ForceConfig || !ForceConfig->AttackerDefinition || ForceConfig->PlannedForce <= 0)
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}
	FText SpawnClassFailure;
	if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
		ForceConfig->AttackerDefinition, ForceConfig->PlannedForce, SpawnClassFailure))
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}

	Assault.EvaluationInput = BuildEvaluationInput(Territory, *ForceConfig);
	Assault.EvaluationResult = CalculateEvaluation(Assault.EvaluationInput, Profile);
	Assault.SelectedApproaches = SelectValidApproaches(
		Territory, Profile, Assault.EvaluationResult.PowerRatio);
	if (Assault.SelectedApproaches.IsEmpty())
	{
		for (const FTerritoryAssaultApproach& Approach : Territory->GetCounterAttackApproaches())
		{
			if (!Approach.bEnabled) continue;
			const FTransform WorldTransform =
				Approach.RelativeSpawnTransform * Territory->GetActorTransform();
			FString RouteFailure;
			FVector Objective;
			FindReachableObjective(Territory, WorldTransform.GetLocation(),
				Objective, &RouteFailure);
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
	if (Assault.DecisionRoll > Assault.EvaluationResult.LaunchProbability)
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::DecisionRollFailed);
		return;
	}

	Assault.PlannedForce = FMath::Max(1, ForceConfig->PlannedForce);
	Assault.AliveForce = 0;
	Assault.PendingReserveForce = Assault.PlannedForce;
	Assault.KilledForce = 0;
	Assault.WithdrawnForce = 0;
	Assault.WaveSize = FMath::Clamp(ForceConfig->WaveSize, 1, Assault.PlannedForce);
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
		|| !Territory || Assault.SelectedApproaches.IsEmpty())
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
	if (!TryCommitProximityActivation(Assault, GetCampaignGameTime())) return false;
	BroadcastStateTransition(Assault, PreviousState);
	SpawnNextWave(Assault, Territory);
	return true;
}

void UTerritoryCounterAttackSubsystem::SpawnNextWave(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	if (Assault.State != ETerritoryAssaultState::Active || !Territory) return;
	UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const FTerritoryFactionAssaultConfig* ForceConfig = Profile
		? Profile->FindFactionForce(Assault.AttackingFaction) : nullptr;
	if (!ForceConfig || !ForceConfig->AttackerDefinition)
	{
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
			ETerritoryAssaultResolution::ConfigurationInvalid);
		return;
	}
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
		if (!FindReachableObjective(Territory, ApproachTransform.GetLocation(), TargetLocation))
		{
			continue;
		}
		const FTransform SpawnTransform = CalculateParticipantDeploymentTransform(
			ApproachTransform, TargetLocation, DeploymentIndex / ApproachCount,
			ParticipantSpacing);
		if (!HasNavigationRoute(SpawnTransform.GetLocation(), TargetLocation)
			|| !IsDeploymentLocationSeparated(
				SpawnTransform.GetLocation(), ParticipantSpacing * 0.8f))
		{
			continue;
		}

		if (ATerritoryAssaultCharacter* Participant = SpawnParticipant(
			Assault, Territory, *ForceConfig, Approach, SpawnTransform,
			OverrideNarrativeLevel))
		{
			if (Spawned == 0 && PresentationPlayer
				&& ForceConfig->ReserveWaveAlertDialogueTag.IsValid())
			{
				Participant->PlayTaggedDialogue(
					ForceConfig->ReserveWaveAlertDialogueTag, PresentationPlayer);
			}
			++SpawnedPerApproach.FindOrAdd(ApproachID);
			++Spawned;
			--Assault.PendingReserveForce;
			++Assault.AliveForce;
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
		if (Assault.ConsecutiveSpawnFailures >= FMath::Max(1, Profile->MaxConsecutiveSpawnFailures))
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
				ETerritoryAssaultResolution::SpawnFailed);
		}
	}
}

ATerritoryAssaultCharacter* UTerritoryCounterAttackSubsystem::SpawnParticipant(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& ForceConfig,
	const FTerritoryAssaultApproach& Approach, const FTransform& SpawnTransform,
	int32 OverrideNarrativeLevel)
{
	FText SpawnClassFailure;
	if (!ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
		ForceConfig.AttackerDefinition, ForceConfig.PlannedForce, SpawnClassFailure))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack definition %s is not physically spawn-ready: %s"),
			*GetNameSafe(ForceConfig.AttackerDefinition), *SpawnClassFailure.ToString());
		return nullptr;
	}

	UNarrativeCharacterSubsystem* CharacterSubsystem =
		GetWorld() ? GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>() : nullptr;
	ATerritoryAssaultCharacter* Participant = ATerritoryAssaultCharacter::SpawnThroughNarrative(
		CharacterSubsystem, ForceConfig.AttackerDefinition, Assault.AttackingFaction,
		Territory->GetTerritoryGUID(), FGuid::NewGuid(), SpawnTransform, Approach.ApproachID,
		ForceConfig.ActivityConfigurationOverride, ForceConfig.TriggerSetOverrides,
		Assault.AssaultID, Assault.TargetTerritory, OverrideNarrativeLevel);
	if (!IsValid(Participant) || Participant->IsActorBeingDestroyed()) return nullptr;
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	const float MinimumSpacing = FMath::Max(
		100.f, Profile ? Profile->ParticipantSpacing * 0.7f : 154.f);
	if (!IsDeploymentLocationSeparated(Participant->GetActorLocation(), MinimumSpacing))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Counterattack participant %s was collision-adjusted into an occupied deployment slot"),
			*GetNameSafe(Participant));
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*Participant);
		Participant->Destroy();
		return nullptr;
	}
	if (!Participant->EnsureNarrativeControllerReady())
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack participant %s failed the verified Narrative controller/activity contract"),
			*GetNameSafe(Participant));
		TerritoryNarrativeDeathSupport::PrepareForRemoval(*Participant);
		Participant->Destroy();
		return nullptr;
	}
	if (OverrideNarrativeLevel > 0 && ForceConfig.PowerScalingEffect)
	{
		if (UAbilitySystemComponent* ASC = Participant->GetAbilitySystemComponent())
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
				ForceConfig.PowerScalingEffect, OverrideNarrativeLevel, Context);
			if (Spec.IsValid())
			{
				if (ForceConfig.PowerScalingMagnitudeTag.IsValid()
					&& ForceConfig.PowerScalingMagnitudePerEnemyLevel > 0.f)
				{
					Spec.Data->SetSetByCallerMagnitude(
						ForceConfig.PowerScalingMagnitudeTag,
						static_cast<float>(FMath::Max(0, OverrideNarrativeLevel - 1))
							* ForceConfig.PowerScalingMagnitudePerEnemyLevel);
				}
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	LiveParticipants.FindOrAdd(Assault.AssaultID).Add(Participant);
	return Participant;
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
	if (bRestoringState) return;
	FTerritoryAssaultRecord* Assault = Assaults.Find(AssaultID);
	if (!Assault) return;

	TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>>* Participants = LiveParticipants.Find(AssaultID);
	if (Participants)
	{
		const int32 Removed = Participants->Remove(Participant);
		if (Removed == 0) return; // exact-once accounting
		if (Participants->IsEmpty()) LiveParticipants.Remove(AssaultID);
	}
	else
	{
		return;
	}

	bool bForceExhausted = false;
	if (!ApplyParticipantRemoval(*Assault, bKilled, bForceExhausted)) return;
	BroadcastChanged(*Assault);

	if (bForceExhausted)
	{
		ResolveAssault(*Assault, ETerritoryAssaultState::Defeated,
			ETerritoryAssaultResolution::AllAttackersRemoved);
	}
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
	bool bContinueAfterActivation)
{
	return Assault.State == ETerritoryAssaultState::Active
		&& Assault.PendingReserveForce > 0
		&& Assault.AliveForce < FMath::Max(1, Assault.WaveSize)
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
	const bool bWasRestoring = bRestoringState;
	bRestoringState = true;
	Assault.State = FinalState;
	Assault.Resolution = Reason;
	Assault.ResolvedGameTime = GetCampaignGameTime();
	Assault.RecaptureEndsGameTime = 0.0;
	RetireLiveParticipants(Assault, true);
	const int32 UnaccountedForce = FMath::Max(0,
		Assault.PlannedForce - Assault.KilledForce - Assault.WithdrawnForce);
	Assault.WithdrawnForce += FMath::Min(UnaccountedForce,
		Assault.AliveForce + Assault.PendingReserveForce);
	Assault.AliveForce = 0;
	Assault.PendingReserveForce = 0;
	bRestoringState = bWasRestoring;
	BroadcastStateTransition(Assault, PreviousState);
}

void UTerritoryCounterAttackSubsystem::RetireLiveParticipants(
	FTerritoryAssaultRecord& Assault, bool bDestroyActors)
{
	TSet<TWeakObjectPtr<ATerritoryAssaultCharacter>> Participants =
		LiveParticipants.FindRef(Assault.AssaultID);
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
				TerritoryNarrativeDeathSupport::PrepareForRemoval(*Participant);
				Participant->Destroy();
			}
		}
	}
	LiveParticipants.Remove(Assault.AssaultID);
}

void UTerritoryCounterAttackSubsystem::BroadcastChanged(const FTerritoryAssaultRecord& Assault)
{
	if (!bRestoringState) OnAssaultChanged.Broadcast(Assault);
}

void UTerritoryCounterAttackSubsystem::BroadcastStateTransition(
	const FTerritoryAssaultRecord& Assault, ETerritoryAssaultState PreviousState)
{
	BroadcastChanged(Assault);
	if (!ShouldEmitCounterHappened(PreviousState, Assault.State, bRestoringState)) return;

	const FTerritoryCounterAttackStateEvent Event = MakeCounterHappenedEvent(
		Assault, PreviousState, GetCampaignGameTime());
	OnCounterHappened.Broadcast(Event);
	NotifyRelevantPlayersOfState(Event, ResolveTerritory(Assault));
}

void UTerritoryCounterAttackSubsystem::NotifyRelevantPlayers(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	UWorld* World = GetWorld();
	UTerritoryCounterAttackProfile* Profile = Territory ? Territory->GetCounterAttackProfile() : nullptr;
	if (!World || !Profile) return;

	TSet<TWeakObjectPtr<APlayerController>>& AlreadyWarned = WarnedControllers.FindOrAdd(Assault.AssaultID);
	bool bWarnedSomeone = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Controller || !Pawn || AlreadyWarned.Contains(Controller)
			|| !IsRelevantPlayer(Controller, Assault, Profile)
			|| FVector::DistSquared(Pawn->GetActorLocation(), Territory->GetActorLocation())
				> FMath::Square(Profile->NotificationRadius))
		{
			continue;
		}
		AlreadyWarned.Add(Controller);
		if (UTerritoryPlayerManagementComponent* Management =
			UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(Controller))
		{
			Management->SendAssaultNotification(Assault);
		}
		OnAssaultWarning.Broadcast(Controller, Assault);
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
		if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Pawn))
		{
			if (const UNarrativeAbilitySystemComponent* ASC =
				Cast<UNarrativeAbilitySystemComponent>(AbilityOwner->GetAbilitySystemComponent()))
			{
				bDead = ASC->IsDead();
			}
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
		if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Pawn))
		{
			if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
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
		if (FindReachableObjective(Territory, WorldTransform.GetLocation(), Objective))
		{
			Valid.AddUnique(Approach.ApproachID);
		}
	}
	const int32 DesiredApproaches = FMath::Clamp(
		1 + FMath::FloorToInt(FMath::Max(0.f, PowerRatio - 1.f)), 1, Profile->MaximumApproaches);
	if (Valid.Num() > DesiredApproaches) Valid.SetNum(DesiredApproaches);
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
	OutWorldTransform = Found->RelativeSpawnTransform * Territory->GetActorTransform();
	FVector Objective;
	return FindReachableObjective(Territory, OutWorldTransform.GetLocation(), Objective);
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
		ScheduleBestCounterAttack(Territory, OldOwner);
	}
}

void UTerritoryCounterAttackSubsystem::HandleDiplomacyChanged(
	FGameplayTag FactionA, FGameplayTag FactionB, EDiplomacyState NewState)
{
	(void)NewState;
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
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
		}
	}
	for (const FGuid& ID : ToCancel)
	{
		CancelAssault(ID, ETerritoryAssaultResolution::DiplomacyBlocked);
	}
}

void UTerritoryCounterAttackSubsystem::HandleTerritoryRegistered(
	ATerritoryVolume* Territory, bool bIsNew)
{
	(void)bIsNew;
	if (!Territory || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
	for (auto& Pair : Assaults)
	{
		if (!Pair.Value.IsTerminal()
			&& DoesAssaultTargetTerritory(Pair.Value, Territory))
		{
			AdvanceAssault(Pair.Value);
		}
	}
}
