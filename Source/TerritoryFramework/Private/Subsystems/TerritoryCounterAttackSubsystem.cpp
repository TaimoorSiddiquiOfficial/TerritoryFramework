#include "Subsystems/TerritoryCounterAttackSubsystem.h"

#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "AI/NPCDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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

	const float RawProbability = Profile->BaseLaunchProbability
		+ Profile->AttackerPowerWeight * NormalizedAttack
		+ Profile->StrategicValueWeight * NormalizedStrategicValue
		+ Profile->ReadinessWeight * Readiness
		+ 0.20f * ShortfallRatio
		+ 0.05f * Momentum
		- Profile->DefenceDeterrenceWeight * (0.60f * ActiveRatio + 0.40f * NormalizedDefence);
	Result.LaunchProbability = FMath::Clamp(RawProbability,
		Profile->MinimumLaunchProbability, Profile->MaximumLaunchProbability);
	Result.EstimatedSuccessProbability = AttackPower
		/ FMath::Max(1.f, AttackPower + Result.DistrictDefencePower);
	Result.AttackPriority = FMath::Max(0.f,
		100.f * (Result.LaunchProbability + 0.25f * NormalizedStrategicValue
			+ 0.15f * Result.EstimatedSuccessProbability));
	return Result;
}

bool UTerritoryCounterAttackSubsystem::ScheduleCounterAttack(
	ATerritoryVolume* Territory, FGameplayTag AttackingFaction)
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
	if (!Profile || !Profile->FindFactionForce(AttackingFaction)) return false;

	for (const auto& Pair : Assaults)
	{
		if (!Pair.Value.IsTerminal() && Pair.Value.TargetTerritory == Territory->GetTerritoryTag())
		{
			return false;
		}
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && (CountNonTerminalAssaults() >= Settings->MaxConcurrentScheduledAssaults
		|| CountNonTerminalAssaults(&AttackingFaction) >= Settings->MaxConcurrentAssaultsPerFaction))
	{
		return false;
	}

	int32 EvaluationCycle = 1;
	for (const auto& Pair : Assaults)
	{
		if (Pair.Value.TargetTerritoryGUID == Territory->GetTerritoryGUID()
			&& Pair.Value.AttackingFaction == AttackingFaction)
		{
			EvaluationCycle = FMath::Max(EvaluationCycle, Pair.Value.EvaluationCycle + 1);
		}
	}

	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid::NewGuid();
	Record.TargetTerritoryGUID = Territory->GetTerritoryGUID();
	Record.TargetTerritory = Territory->GetTerritoryTag();
	Record.AttackingFaction = AttackingFaction;
	Record.DefendingFaction = Territory->GetOwningFaction();
	Record.State = ETerritoryAssaultState::Grace;
	Record.EvaluationCycle = EvaluationCycle;
	Record.DecisionSeed = MakeDecisionSeed(Territory, AttackingFaction, EvaluationCycle);
	Record.CapturedGameTime = GetCampaignGameTime();
	Record.GraceEndsGameTime = Record.CapturedGameTime + Profile->GracePeriodGameTime;
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

bool UTerritoryCounterAttackSubsystem::IsAssaultActive(FGuid AssaultID) const
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
	return FString::Printf(
		TEXT("Target=%s Attack=%s Defend=%s Guards=%d/%d/%d Reserve=%d AttackPower=%.2f DefencePower=%.2f PowerRatio=%.3f Strategic=%.2f Priority=%.3f LaunchP=%.3f SuccessP=%.3f Force=%d/%d/%d/%d Approaches=%s State=%d Notification=%s GraceEnd=%.2f Roll=%.4f Reason=%d"),
		*Assault->TargetTerritory.ToString(), *Assault->AttackingFaction.ToString(),
		*Assault->DefendingFaction.ToString(), Input.ActiveGuards, Input.DesiredGuards,
		Input.MaximumGuards, Input.ReserveGuards, Input.AttackingMilitaryPower,
		Result.DistrictDefencePower, Result.PowerRatio, Input.StrategicValue,
		Result.AttackPriority, Result.LaunchProbability, Result.EstimatedSuccessProbability,
		Assault->PlannedForce, Assault->AliveForce, Assault->PendingReserveForce,
		Assault->KilledForce, *FString::JoinBy(Assault->SelectedApproaches, TEXT(","),
			[](FName Name) { return Name.ToString(); }), static_cast<int32>(Assault->State),
		Assault->bNotificationSent ? TEXT("sent") : TEXT("pending"), Assault->GraceEndsGameTime,
		Assault->DecisionRoll, static_cast<int32>(Assault->Resolution));
}

TArray<FTerritoryAssaultRecord> UTerritoryCounterAttackSubsystem::GetPersistentState() const
{
	return GetAllAssaults();
}

void UTerritoryCounterAttackSubsystem::RestorePersistentState(
	const TArray<FTerritoryAssaultRecord>& Records)
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

	for (FTerritoryAssaultRecord Record : Records)
	{
		if (!Record.AssaultID.IsValid() || !Record.TargetTerritory.IsValid()) continue;
		Record.PlannedForce = FMath::Max(0, Record.PlannedForce);
		Record.KilledForce = FMath::Clamp(Record.KilledForce, 0, Record.PlannedForce);
		Record.WithdrawnForce = FMath::Clamp(
			Record.WithdrawnForce, 0, Record.PlannedForce - Record.KilledForce);
		if (bAuthority && Record.State == ETerritoryAssaultState::Active)
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
	TrimTerminalHistory();
}

void UTerritoryCounterAttackSubsystem::AdvanceAssault(FTerritoryAssaultRecord& Assault)
{
	if (Assault.IsTerminal()) return;
	ATerritoryVolume* Territory = ResolveTerritory(Assault);
	if (!Territory) return; // World Partition: wait for authoritative actor registration.

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
			Assault.State = ETerritoryAssaultState::Evaluating;
			BroadcastChanged(Assault);
			EvaluateAssault(Assault, Territory);
		}
		break;
	case ETerritoryAssaultState::ScheduledWarning:
		NotifyRelevantPlayers(Assault, Territory);
		Assault.State = ETerritoryAssaultState::WaitingForPlayerProximity;
		BroadcastChanged(Assault);
		break;
	case ETerritoryAssaultState::WaitingForPlayerProximity:
		NotifyRelevantPlayers(Assault, Territory);
		if (UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile())
		{
			if (Territory->GetTerritoryState() == ETerritoryState::Claimed
				&& HasRelevantPlayerNearby(Assault, Territory, Profile->ActivationRadius))
			{
				ActivateAssault(Assault, Territory);
			}
		}
		break;
	case ETerritoryAssaultState::Active:
		if (Territory->GetTerritoryState() == ETerritoryState::Locked)
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Cancelled,
				ETerritoryAssaultResolution::InvalidTerritory);
			return;
		}
		if (Assault.AliveForce + Assault.PendingReserveForce <= 0)
		{
			ResolveAssault(Assault, ETerritoryAssaultState::Defeated,
				ETerritoryAssaultResolution::AllAttackersRemoved);
			return;
		}
		if (UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile())
		{
			if (Assault.PendingReserveForce > 0
				&& Assault.AliveForce < FMath::Max(1, Assault.WaveSize)
				&& HasRelevantPlayerNearby(Assault, Territory, Profile->ActivationRadius))
			{
				SpawnNextWave(Assault, Territory);
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
	UClass* ConfiguredNPCClass = ForceConfig->AttackerDefinition->NPCClassPath.LoadSynchronous();
	if (!ConfiguredNPCClass || !ConfiguredNPCClass->IsChildOf(ATerritoryAssaultCharacter::StaticClass()))
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
	Assault.State = ETerritoryAssaultState::ScheduledWarning;
	Assault.Resolution = ETerritoryAssaultResolution::None;
	BroadcastChanged(Assault);
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
	Assault.State = ETerritoryAssaultState::Active;
	Assault.ActivatedGameTime = GetCampaignGameTime();
	BroadcastChanged(Assault);
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

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const int32 GlobalAvailable = Settings
		? FMath::Max(0, Settings->MaxLiveCounterAttackNPCs - CountLiveParticipants())
		: Assault.PendingReserveForce;
	int32 ToSpawn = FMath::Min3(Assault.PendingReserveForce,
		FMath::Max(0, Assault.WaveSize - Assault.AliveForce), GlobalAvailable);
	if (ToSpawn <= 0) return;

	const int32 AlreadyDeployed = Assault.PlannedForce - Assault.PendingReserveForce;
	int32 Spawned = 0;
	TMap<FName, int32> SpawnedPerApproach;
	const int32 MaximumAttempts = ToSpawn * Assault.SelectedApproaches.Num();
	for (int32 Attempt = 0; Attempt < MaximumAttempts && Spawned < ToSpawn; ++Attempt)
	{
		const FName ApproachID = Assault.SelectedApproaches[
			(AlreadyDeployed + Attempt) % Assault.SelectedApproaches.Num()];
		FTerritoryAssaultApproach Approach;
		FTransform SpawnTransform;
		if (!ResolveApproach(Territory, ApproachID, Approach, SpawnTransform)) continue;
		if (SpawnedPerApproach.FindRef(ApproachID) >= FMath::Max(1, Approach.MaxWaveSize)) continue;

		if (SpawnParticipant(Assault, Territory, *ForceConfig, Approach, SpawnTransform))
		{
			++SpawnedPerApproach.FindOrAdd(ApproachID);
			++Spawned;
			--Assault.PendingReserveForce;
			++Assault.AliveForce;
		}
	}
	if (Spawned > 0) BroadcastChanged(Assault);
}

ATerritoryAssaultCharacter* UTerritoryCounterAttackSubsystem::SpawnParticipant(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& ForceConfig,
	const FTerritoryAssaultApproach& Approach, const FTransform& SpawnTransform)
{
	UClass* NPCClass = ForceConfig.AttackerDefinition
		? ForceConfig.AttackerDefinition->NPCClassPath.LoadSynchronous() : nullptr;
	if (!NPCClass || !NPCClass->IsChildOf(ATerritoryAssaultCharacter::StaticClass()))
	{
		UE_LOG(LogTerritory, Error,
			TEXT("Counterattack definition %s must use TerritoryAssaultCharacter; participant was not spawned"),
			*GetNameSafe(ForceConfig.AttackerDefinition));
		return nullptr;
	}

	ATerritoryAssaultCharacter* Participant = Cast<ATerritoryAssaultCharacter>(
		UGameplayStatics::BeginDeferredActorSpawnFromClass(this, NPCClass, SpawnTransform,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding, Territory));
	if (!Participant) return nullptr;

	Participant->ConfigureAssaultSpawn(ForceConfig.AttackerDefinition, Assault.AttackingFaction,
		Territory->GetTerritoryGUID(), FGuid::NewGuid(), SpawnTransform, Approach.ApproachID,
		ForceConfig.ActivityConfigurationOverride, ForceConfig.TriggerSetOverrides,
		Assault.AssaultID, Assault.TargetTerritory);
	AActor* Finished = UGameplayStatics::FinishSpawningActor(Participant, SpawnTransform);
	if (!IsValid(Finished) || Participant->IsActorBeingDestroyed()) return nullptr;

	LiveParticipants.FindOrAdd(Assault.AssaultID).Add(Participant);
	return Participant;
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

	Assault->AliveForce = FMath::Max(0, Assault->AliveForce - 1);
	if (Assault->KilledForce + Assault->WithdrawnForce < Assault->PlannedForce)
	{
		if (bKilled) ++Assault->KilledForce;
		else ++Assault->WithdrawnForce;
	}
	BroadcastChanged(*Assault);

	if (Assault->State == ETerritoryAssaultState::Active
		&& Assault->AliveForce + Assault->PendingReserveForce <= 0)
	{
		ResolveAssault(*Assault, ETerritoryAssaultState::Defeated,
			ETerritoryAssaultResolution::AllAttackersRemoved);
	}
}

void UTerritoryCounterAttackSubsystem::ResolveAssault(
	FTerritoryAssaultRecord& Assault, ETerritoryAssaultState FinalState,
	ETerritoryAssaultResolution Reason)
{
	if (Assault.IsTerminal()) return;
	const bool bWasRestoring = bRestoringState;
	bRestoringState = true;
	Assault.State = FinalState;
	Assault.Resolution = Reason;
	RetireLiveParticipants(Assault, true);
	const int32 UnaccountedForce = FMath::Max(0,
		Assault.PlannedForce - Assault.KilledForce - Assault.WithdrawnForce);
	Assault.WithdrawnForce += FMath::Min(UnaccountedForce,
		Assault.AliveForce + Assault.PendingReserveForce);
	Assault.AliveForce = 0;
	Assault.PendingReserveForce = 0;
	bRestoringState = bWasRestoring;
	BroadcastChanged(Assault);
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
			if (bDestroyActors && IsValid(Participant)) Participant->Destroy();
		}
	}
	LiveParticipants.Remove(Assault.AssaultID);
}

void UTerritoryCounterAttackSubsystem::BroadcastChanged(const FTerritoryAssaultRecord& Assault)
{
	if (!bRestoringState) OnAssaultChanged.Broadcast(Assault);
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

ATerritoryVolume* UTerritoryCounterAttackSubsystem::ResolveTerritory(
	const FTerritoryAssaultRecord& Assault) const
{
	UWorld* World = GetWorld();
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return nullptr;
	if (Assault.TargetTerritoryGUID.IsValid())
	{
		if (ATerritoryVolume* ByGuid = Registry->GetTerritoryByGUID(Assault.TargetTerritoryGUID))
		{
			return ByGuid;
		}
	}
	return Registry->GetTerritoryByTag(Assault.TargetTerritory);
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

bool UTerritoryCounterAttackSubsystem::IsRelevantPlayer(
	const APlayerController* Controller, const FTerritoryAssaultRecord& Assault,
	const UTerritoryCounterAttackProfile* Profile) const
{
	if (!Controller || !Profile) return false;
	AActor* PlayerPawn = Controller->GetPawn().Get();
	return !Profile->bNotifyDefendingFactionOnly
		|| UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			this, PlayerPawn) == Assault.DefendingFaction;
}

bool UTerritoryCounterAttackSubsystem::IsDiplomacyBlocked(
	const FTerritoryAssaultRecord& Assault, const ATerritoryVolume* Territory) const
{
	UWorld* World = GetWorld();
	const UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	return !Control || !Control->CanFactionCaptureTerritory(Territory, Assault.AttackingFaction);
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
		if (HasNavigationRoute(WorldTransform.GetLocation(), Territory->GetTerritoryBounds().GetCenter()))
		{
			Valid.Add(Approach.ApproachID);
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
	return HasNavigationRoute(OutWorldTransform.GetLocation(), Territory->GetTerritoryBounds().GetCenter());
}

bool UTerritoryCounterAttackSubsystem::HasNavigationRoute(
	const FVector& Start, const FVector& End) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav) return false;
	FNavLocation ProjectedStart;
	FNavLocation ProjectedEnd;
	if (!Nav->ProjectPointToNavigation(Start, ProjectedStart, FVector(500.f))
		|| !Nav->ProjectPointToNavigation(End, ProjectedEnd, FVector(1000.f)))
	{
		return false;
	}
	UNavigationPath* Path = Nav->FindPathToLocationSynchronously(
		World, ProjectedStart.Location, ProjectedEnd.Location);
	return Path && Path->IsValid() && !Path->IsPartial();
}

FTerritoryAssaultEvaluationInput UTerritoryCounterAttackSubsystem::BuildEvaluationInput(
	const ATerritoryVolume* Territory,
	const FTerritoryFactionAssaultConfig& ForceConfig) const
{
	FTerritoryAssaultEvaluationInput Input;
	if (!Territory) return Input;
	Input.ActiveGuards = Territory->GetSpawnedGuardCount();
	Input.DesiredGuards = Territory->GetDesiredGuardCount();
	Input.MaximumGuards = Territory->GetMaxGuardCount();
	for (const ATerritoryGuardSpawnPoint* SpawnPoint : Territory->GetGuardSpawnPoints())
	{
		if (SpawnPoint) Input.ReserveGuards += SpawnPoint->GetReserveCount();
	}
	Input.GuardQuality = Territory->GetGuardQuality();
	Input.Fortification = Territory->GetFortificationStrength();
	Input.NearbyAlliedSupport = Territory->GetNearbyAlliedSupport();
	Input.AttackingMilitaryPower = ForceConfig.MilitaryPower;
	Input.EconomyReadiness = ForceConfig.EconomyReadiness;
	Input.SupplyReadiness = ForceConfig.SupplyReadiness;
	Input.StrategicValue = Territory->GetStrategicValue();
	Input.RecentMomentum = ForceConfig.RecentMomentum;
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
			&& Pair.Value.TargetTerritory == Territory->GetTerritoryTag())
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
		ScheduleCounterAttack(Territory, OldOwner);
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
			&& (Pair.Value.TargetTerritoryGUID == Territory->GetTerritoryGUID()
				|| Pair.Value.TargetTerritory == Territory->GetTerritoryTag()))
		{
			AdvanceAssault(Pair.Value);
		}
	}
}
