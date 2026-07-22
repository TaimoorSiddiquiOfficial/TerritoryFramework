#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

void UTerritoryControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const float CaptureTickInterval = Settings ? Settings->CaptureTickInterval : 0.1f;

	if (World && World->GetNetMode() != NM_Client)
	{
		World->GetTimerManager().SetTimer(
			CaptureTickTimerHandle,
			this,
			&UTerritoryControlSubsystem::OnCaptureTick,
			CaptureTickInterval,
			true);
	}

	UE_LOG(LogTerritory, Log, TEXT("TerritoryControlSubsystem initialized (tick: %.2fs)"),
		CaptureTickInterval);
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
// Capture Timer — evaluate-then-apply (P0-01: no map mutation during iteration)
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryControlSubsystem::OnCaptureTick()
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebug = Settings && Settings->ShouldDebugCapture();
	const float DeltaTime = Settings ? Settings->CaptureTickInterval : 0.1f;

	DeferredCommands.Empty();

	// Phase 1: Evaluate all territories WITHOUT mutating the map
	TArray<TWeakObjectPtr<ATerritoryVolume>> InvalidKeys;

	for (auto& Pair : TerritoryCaptureState)
	{
		ATerritoryVolume* Territory = Pair.Key.Get();
		if (!Territory)
		{
			InvalidKeys.Add(Pair.Key);
			continue;
		}

		if (bDebug)
		{
			UE_LOG(LogTerritory, Verbose, TEXT("[CaptureTick] %s: progress=%.2f"),
				*Territory->GetTerritoryTag().ToString(), GetCaptureProgress(Territory));
		}

		EvaluateCaptureState(Territory, DeltaTime);
	}

	// Phase 2: Apply deferred commands (safe to mutate now)
	for (const FDeferredCommand& Cmd : DeferredCommands)
	{
		if (Cmd.Type == FDeferredCommand::Complete)
		{
			CompleteCapture(Cmd.Territory.Get(), Cmd.Faction);
		}
		else if (Cmd.Type == FDeferredCommand::Reset)
		{
			ATerritoryVolume* Territory = Cmd.Territory.Get();
			if (Territory)
			{
				TerritoryCaptureState.Remove(Territory);
				Territory->SetControlProgress(0.f);
			}
		}
	}

	DeferredCommands.Empty();

	// Cleanup invalid keys
	for (const TWeakObjectPtr<ATerritoryVolume>& WeakTerritory : InvalidKeys)
	{
		TerritoryCaptureState.Remove(WeakTerritory);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Capture API (Authority-Only Mutations)
// ═══════════════════════════════════════════════════════════════════════════════

ECaptureResult UTerritoryControlSubsystem::AttemptCapture(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction)
{
	if (!GetWorld()->GetAuthGameMode()) return ECaptureResult::InvalidTerritory;
	if (!Territory || !AttackingFaction.IsValid()) return ECaptureResult::InvalidTerritory;

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const bool bDebugAttempts = Settings && Settings->ShouldDebugCaptureAttempts();

	if (bDebugAttempts)
	{
		UE_LOG(LogTerritory, Log, TEXT("[CaptureAttempt] %s by %s (owner: %s, state: %d)"),
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

	// Check Narrative faction attitudes
	FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (DefendingFaction.IsValid())
	{
		ANarrativeGameState* NarrativeGS = Cast<ANarrativeGameState>(GetWorld()->GetGameState());
		if (NarrativeGS)
		{
			ETeamAttitude::Type Attitude = NarrativeGS->GetFactionAttitudeTowardsFaction(AttackingFaction, DefendingFaction);
			if (Attitude == ETeamAttitude::Friendly)
			{
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

	if (Territory->GetDefenderCount() > 0 && Territory->GetOwningFaction().IsValid())
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
		Territory->SetTerritoryState(Territory->GetOwningFaction().IsValid()
			? ETerritoryState::Claimed
			: ETerritoryState::Unclaimed);
	}
	Territory->SetControlProgress(0.f);
}

void UTerritoryControlSubsystem::AddCaptureProgress(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction, float ProgressDelta)
{
	if (!Territory || !AttackingFaction.IsValid()) return;

	// Validate through AttemptCapture rules (lock, diplomacy, defenders) before adding progress
	if (!IsCaptureInProgress(Territory))
	{
		ECaptureResult Result = AttemptCapture(Territory, AttackingFaction);
		if (Result != ECaptureResult::Success) return;
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
	TerritoryCaptureState.Remove(Territory);
	Territory->SetOwningFaction(NewOwner);
}

void UTerritoryControlSubsystem::RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!Territory || !Attacker || !Faction.IsValid()) return;

	// Ensure territory is in capture state
	if (Territory->GetTerritoryState() == ETerritoryState::Locked) return;
	if (Territory->IsOwnedByFaction(Faction)) return;

	if (Territory->GetTerritoryState() == ETerritoryState::Claimed || Territory->GetTerritoryState() == ETerritoryState::Unclaimed)
	{
		Territory->SetTerritoryState(ETerritoryState::Contested);
	}

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	TSet<TWeakObjectPtr<AActor>>& ActorSet = State.AttackersByFaction.FindOrAdd(Faction);

	// Identity-based — adding the same actor twice is a no-op
	int32 BeforeCount = ActorSet.Num();
	ActorSet.Add(Attacker);
	int32 AfterCount = ActorSet.Num();

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugCapture() && AfterCount > BeforeCount)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Attacker] %s registered for %s in %s (total: %d)"),
			*Attacker->GetName(), *Faction.ToString(),
			*Territory->GetTerritoryTag().ToString(), AfterCount);
	}

	// Seed progress if not already present
	if (!State.CaptureProgressByFaction.Contains(Faction))
	{
		State.CaptureProgressByFaction.Add(Faction, 0.f);
	}
}

void UTerritoryControlSubsystem::UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!Territory || !Attacker || !Faction.IsValid()) return;
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Faction);
	if (ActorSet)
	{
		ActorSet->Remove(Attacker);
		if (ActorSet->Num() == 0)
		{
			State->AttackersByFaction.Remove(Faction);
		}
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
	return GetActiveAttackers(Territory, Faction) < Territory->GetMaxConcurrentAttackers();
}

int32 UTerritoryControlSubsystem::GetActiveAttackers(const ATerritoryVolume* Territory, const FGameplayTag& Faction) const
{
	if (!Territory) return 0;
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return 0;
	const TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Faction);
	return ActorSet ? ActorSet->Num() : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal — no map mutation from here during iteration
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
	int32 BestAttackerCount = 0;

	for (auto& Pair : State->CaptureProgressByFaction)
	{
		const TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Pair.Key);
		int32 AttackerCount = ActorSet ? ActorSet->Num() : 0;

		if (AttackerCount > 0)
		{
			Pair.Value = FMath::Clamp(Pair.Value + DeltaTime * ProgressRate, 0.f, 1.f);
		}
		else
		{
			Pair.Value = FMath::Max(0.f, Pair.Value - DeltaTime * DecayRate);
		}

		// Deterministic winner selection: highest progress → most attackers → tag name tie-break
		bool bWins = false;
		if (Pair.Value > BestProgress)
		{
			bWins = true;
		}
		else if (Pair.Value == BestProgress && Pair.Value > 0.f)
		{
			// Tie-break: more attackers wins, then lexicographic tag for determinism
			if (AttackerCount > BestAttackerCount)
			{
				bWins = true;
			}
			else if (AttackerCount == BestAttackerCount && BestFaction.IsValid())
			{
				bWins = Pair.Key.ToString() < BestFaction.ToString();
			}
		}

		if (bWins)
		{
			BestProgress = Pair.Value;
			BestFaction = Pair.Key;
			BestAttackerCount = AttackerCount;
		}
	}

	Territory->SetControlProgress(BestProgress);

	// DEFER completion — don't mutate map during iteration (P0-01)
	if (BestProgress >= 1.f && BestFaction.IsValid())
	{
		DeferredCommands.Add({FDeferredCommand::Complete, Territory, BestFaction});
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

	// DEFER reset — don't mutate map during iteration
	if (State->CaptureProgressByFaction.Num() == 0)
	{
		DeferredCommands.Add({FDeferredCommand::Reset, Territory, FGameplayTag()});
	}
}

void UTerritoryControlSubsystem::CompleteCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner)
{
	if (!Territory || !NewOwner.IsValid()) return;

	FGameplayTag OldOwner = Territory->GetOwningFaction();
	TerritoryCaptureState.Remove(Territory);
	Territory->SetOwningFaction(NewOwner);

	UE_LOG(LogTerritory, Log, TEXT("[Capture] %s captured by %s (was %s)"),
		*Territory->GetTerritoryTag().ToString(),
		*NewOwner.ToString(),
		*OldOwner.ToString());

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->IsDebugEnabled())
	{
		const FString Msg = FString::Printf(TEXT("[Capture] %s → %s"),
			*Territory->GetTerritoryDisplayName().ToString(), *NewOwner.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, Msg);
	}

	OnTerritoryControlChanged.Broadcast(Territory, OldOwner, NewOwner);
}
