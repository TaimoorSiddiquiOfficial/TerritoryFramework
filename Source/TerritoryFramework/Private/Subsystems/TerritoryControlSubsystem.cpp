#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTerritoryControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Only start capture tick timer on the server — capture state is server-authoritative
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Client)
	{
		World->GetTimerManager().SetTimer(
			CaptureTickTimerHandle,
			this,
			&UTerritoryControlSubsystem::OnCaptureTick,
			0.1f,
			true);
	}

	UE_LOG(LogTerritory, Log, TEXT("TerritoryControlSubsystem initialized (capture tick: 0.1s, server-only: %s)"),
		World && World->GetNetMode() != NM_Client ? TEXT("true") : TEXT("false"));
}

void UTerritoryControlSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CaptureTickTimerHandle);
	}
	TerritoryCaptureState.Empty();
	Super::Deinitialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Capture Timer Callback
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryControlSubsystem::OnCaptureTick()
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugCapture();
	const float DeltaTime = 0.1f;

	TArray<TWeakObjectPtr<ATerritoryVolume>> ToRemove;

	for (auto& Pair : TerritoryCaptureState)
	{
		ATerritoryVolume* Territory = Pair.Key.Get();
		if (!Territory)
		{
			ToRemove.Add(Pair.Key);
			continue;
		}

		if (bDebug)
		{
			float Progress = GetCaptureProgress(Territory);
			FGameplayTag Contesting = GetContestingFaction(Territory);
			UE_LOG(LogTerritory, Verbose, TEXT("[CaptureTick] %s: progress=%.2f, contesting=%s"),
				*Territory->GetTerritoryTag().ToString(), Progress, *Contesting.ToString());
		}

		EvaluateCaptureState(Territory, DeltaTime);
	}

	for (const TWeakObjectPtr<ATerritoryVolume>& WeakTerritory : ToRemove)
	{
		TerritoryCaptureState.Remove(WeakTerritory);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Capture API (Authority-Only Mutations)
// ═══════════════════════════════════════════════════════════════════════════════

ECaptureResult UTerritoryControlSubsystem::AttemptCapture(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction)
{
	if (!Territory || !AttackingFaction.IsValid())
	{
		return ECaptureResult::InvalidTerritory;
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugAttempts = Settings && Settings->ShouldDebugCaptureAttempts();

	if (bDebugAttempts)
	{
		UE_LOG(LogTerritory, Log, TEXT("[CaptureAttempt] %s by %s (current owner: %s, state: %d)"),
			*Territory->GetTerritoryTag().ToString(),
			*AttackingFaction.ToString(),
			*Territory->GetOwningFaction().ToString(),
			static_cast<int32>(Territory->GetTerritoryState()));
	}

	ETerritoryState CurrentState = Territory->GetTerritoryState();

	if (CurrentState == ETerritoryState::Locked)
	{
		FCaptureAttempt Attempt;
		Attempt.Territory = Territory;
		Attempt.AttackingFaction = AttackingFaction;
		Attempt.Result = ECaptureResult::Locked;
		OnCaptureAttempted.Broadcast(Attempt);
		return ECaptureResult::Locked;
	}

	if (Territory->IsOwnedByFaction(AttackingFaction))
	{
		return ECaptureResult::AlreadyOwned;
	}

	// ─── P1.2: Check Narrative faction attitudes before allowing capture ───
	FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (DefendingFaction.IsValid())
	{
		ANarrativeGameState* NarrativeGS = Cast<ANarrativeGameState>(GetWorld()->GetGameState());
		if (NarrativeGS)
		{
			ETeamAttitude::Type Attitude = NarrativeGS->GetFactionAttitudeTowardsFaction(AttackingFaction, DefendingFaction);
			if (Attitude == ETeamAttitude::Friendly)
			{
				UE_LOG(LogTerritory, Warning,
					TEXT("Capture denied: %s is Friendly with %s (territory %s)"),
					*AttackingFaction.ToString(), *DefendingFaction.ToString(),
					*Territory->GetTerritoryTag().ToString());

				FCaptureAttempt Attempt;
				Attempt.Territory = Territory;
				Attempt.AttackingFaction = AttackingFaction;
				Attempt.DefendingFaction = DefendingFaction;
				Attempt.Result = ECaptureResult::DiplomaticallyBlocked;
				OnCaptureAttempted.Broadcast(Attempt);
				return ECaptureResult::DiplomaticallyBlocked;
			}
		}
	}

	if (Territory->GetDefenderCount() > 0 && Territory->IsOwnedByFaction(Territory->GetOwningFaction()))
	{
		FCaptureAttempt Attempt;
		Attempt.Territory = Territory;
		Attempt.AttackingFaction = AttackingFaction;
		Attempt.DefendingFaction = Territory->GetOwningFaction();
		Attempt.DefendersPresent = Territory->GetDefenderCount();
		Attempt.Result = ECaptureResult::DefendersRemain;
		OnCaptureAttempted.Broadcast(Attempt);
		return ECaptureResult::DefendersRemain;
	}

	// Initiate capture
	if (CurrentState != ETerritoryState::Contested)
	{
		Territory->SetTerritoryState(ETerritoryState::Contested);
	}

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	// Only reset to 0 if not already in progress (prevents repeated reset on re-call)
	if (!State.CaptureProgressByFaction.Contains(AttackingFaction))
	{
		State.CaptureProgressByFaction.Add(AttackingFaction, 0.f);
	}

	FCaptureAttempt Attempt;
	Attempt.Territory = Territory;
	Attempt.AttackingFaction = AttackingFaction;
	Attempt.DefendingFaction = Territory->GetOwningFaction();
	Attempt.Result = ECaptureResult::Success;
	OnCaptureAttempted.Broadcast(Attempt);

	return ECaptureResult::Success;
}

void UTerritoryControlSubsystem::ResetCapture(ATerritoryVolume* Territory)
{
	if (!Territory) return;
	TerritoryCaptureState.Remove(Territory);
	if (Territory->GetTerritoryState() == ETerritoryState::Contested)
	{
		if (Territory->GetOwningFaction().IsValid())
		{
			Territory->SetTerritoryState(ETerritoryState::Claimed);
		}
		else
		{
			Territory->SetTerritoryState(ETerritoryState::Unclaimed);
		}
	}
	Territory->SetControlProgress(0.f);
}

void UTerritoryControlSubsystem::AddCaptureProgress(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction, float ProgressDelta)
{
	if (!Territory || !AttackingFaction.IsValid()) return;

	// Ensure capture is initiated
	if (Territory->GetTerritoryState() != ETerritoryState::Contested)
	{
		Territory->SetTerritoryState(ETerritoryState::Contested);
	}

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	float& Progress = State.CaptureProgressByFaction.FindOrAdd(AttackingFaction);
	Progress = FMath::Clamp(Progress + ProgressDelta, 0.f, 1.f);

	Territory->SetControlProgress(Progress);

	if (Progress >= 1.f)
	{
		CompleteCapture(Territory, AttackingFaction);
	}
}

void UTerritoryControlSubsystem::ForceCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner)
{
	if (!Territory || !NewOwner.IsValid()) return;

	// Clear any in-progress capture state
	TerritoryCaptureState.Remove(Territory);

	// Direct ownership transfer through the actor's authoritative setter
	Territory->SetOwningFaction(NewOwner);

	UE_LOG(LogTerritory, Log, TEXT("ForceCapture: %s → %s"),
		*Territory->GetTerritoryTag().ToString(), *NewOwner.ToString());
}

void UTerritoryControlSubsystem::RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!Territory || !Attacker || !Faction.IsValid()) return;
	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	State.AttackersByFaction.FindOrAdd(Faction)++;
}

void UTerritoryControlSubsystem::UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!Territory || !Attacker || !Faction.IsValid()) return;
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	int32* Count = State->AttackersByFaction.Find(Faction);
	if (Count && *Count > 0)
	{
		(*Count)--;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Query API (Read-Only)
// ═══════════════════════════════════════════════════════════════════════════════

bool UTerritoryControlSubsystem::IsCaptureInProgress(const ATerritoryVolume* Territory) const
{
	if (!Territory) return false;
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	return State && State->CaptureProgressByFaction.Num() > 0;
}

float UTerritoryControlSubsystem::GetCaptureProgress(const ATerritoryVolume* Territory) const
{
	if (!Territory) return 0.f;
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return 0.f;

	float MaxProgress = 0.f;
	for (const auto& Pair : State->CaptureProgressByFaction)
	{
		MaxProgress = FMath::Max(MaxProgress, Pair.Value);
	}
	return MaxProgress;
}

FGameplayTag UTerritoryControlSubsystem::GetContestingFaction(const ATerritoryVolume* Territory) const
{
	if (!Territory) return FGameplayTag();
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return FGameplayTag();

	float MaxProgress = 0.f;
	FGameplayTag LeadingFaction;
	for (const auto& Pair : State->CaptureProgressByFaction)
	{
		if (Pair.Value > MaxProgress)
		{
			MaxProgress = Pair.Value;
			LeadingFaction = Pair.Key;
		}
	}
	return LeadingFaction;
}

bool UTerritoryControlSubsystem::HasAttackBudget(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const
{
	if (!Territory) return false;
	int32 MaxAttackers = Territory->GetMaxConcurrentAttackers();
	int32 CurrentAttackers = GetActiveAttackers(Territory, Faction);
	return CurrentAttackers < MaxAttackers;
}

int32 UTerritoryControlSubsystem::GetActiveAttackers(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const
{
	if (!Territory) return 0;
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return 0;
	const int32* Count = State->AttackersByFaction.Find(Faction);
	return Count ? *Count : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryControlSubsystem::EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime)
{
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const float ProgressRate = Settings ? Settings->CaptureProgressPerSecond : 0.1f;
	const float DecayRate = Settings ? Settings->CaptureProgressDecayPerSecond : 0.05f;

	FGameplayTag BestFaction;
	float BestProgress = 0.f;

	for (auto& Pair : State->CaptureProgressByFaction)
	{
		int32 Attackers = State->AttackersByFaction.FindRef(Pair.Key);
		if (Attackers > 0)
		{
			Pair.Value = FMath::Clamp(Pair.Value + DeltaTime * ProgressRate, 0.f, 1.f);
		}
		else
		{
			Pair.Value = FMath::Max(0.f, Pair.Value - DeltaTime * DecayRate);
		}

		if (Pair.Value > BestProgress)
		{
			BestProgress = Pair.Value;
			BestFaction = Pair.Key;
		}
	}

	Territory->SetControlProgress(BestProgress);

	if (BestProgress >= 1.f && BestFaction.IsValid())
	{
		CompleteCapture(Territory, BestFaction);
	}

	// Cleanup zero-progress factions
	TArray<FGameplayTag> ToRemove;
	for (const auto& Pair : State->CaptureProgressByFaction)
	{
		if (Pair.Value <= 0.f)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Tag : ToRemove)
	{
		State->CaptureProgressByFaction.Remove(Tag);
		State->AttackersByFaction.Remove(Tag);
	}

	if (State->CaptureProgressByFaction.Num() == 0)
	{
		ResetCapture(Territory);
	}
}

void UTerritoryControlSubsystem::CompleteCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner)
{
	if (!Territory || !NewOwner.IsValid()) return;

	FGameplayTag OldOwner = Territory->GetOwningFaction();
	TerritoryCaptureState.Remove(Territory);
	Territory->SetOwningFaction(NewOwner);

	UE_LOG(LogTerritory, Log, TEXT("Territory %s captured by %s (was %s)"),
		*Territory->GetTerritoryTag().ToString(),
		*NewOwner.ToString(),
		*OldOwner.ToString());

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugOwnership())
	{
		UE_LOG(LogTerritory, Log, TEXT("[CaptureComplete] %s: %s → %s"),
			*Territory->GetTerritoryTag().ToString(),
			*OldOwner.ToString(), *NewOwner.ToString());
	}

	OnTerritoryControlChanged.Broadcast(Territory, OldOwner, NewOwner);
}
