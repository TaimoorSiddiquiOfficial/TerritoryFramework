#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

namespace
{
	int32 PruneInvalidAttackers(TSet<TWeakObjectPtr<AActor>>& Attackers)
	{
		TArray<TWeakObjectPtr<AActor>> InvalidAttackers;
		for (const TWeakObjectPtr<AActor>& Attacker : Attackers)
		{
			if (!Attacker.IsValid())
			{
				InvalidAttackers.Add(Attacker);
			}
		}

		for (const TWeakObjectPtr<AActor>& Attacker : InvalidAttackers)
		{
			Attackers.Remove(Attacker);
		}
		return Attackers.Num();
	}
}

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
				Territory->SetContestingFaction(FGameplayTag());
				Territory->SetControlProgress(0.f);
				if (Territory->GetTerritoryState() == ETerritoryState::Contested)
				{
					Territory->SetTerritoryState(Territory->GetOwningFaction().IsValid()
						? ETerritoryState::Claimed
						: ETerritoryState::Unclaimed);
				}
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
	return ValidateAndBeginCapture(Territory, AttackingFaction, true);
}

ECaptureResult UTerritoryControlSubsystem::ValidateAndBeginCapture(
	ATerritoryVolume* Territory,
	const FGameplayTag& AttackingFaction,
	bool bBroadcastAttempt,
	bool bCommitContestState)
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode()) return ECaptureResult::InvalidTerritory;

	const FGameplayTag DefendingFaction = Territory ? Territory->GetOwningFaction() : FGameplayTag();
	auto FinishAttempt = [this, Territory, AttackingFaction, DefendingFaction](ECaptureResult Result)
	{
		FCaptureAttempt Attempt;
		Attempt.Territory = Territory;
		Attempt.AttackingFaction = AttackingFaction;
		Attempt.DefendingFaction = DefendingFaction;
		Attempt.Result = Result;
		Attempt.AttackersPresent = GetActiveAttackers(Territory, AttackingFaction);
		Attempt.DefendersPresent = Territory ? Territory->GetDefenderCount() : 0;
		OnCaptureAttempted.Broadcast(Attempt);
		return Result;
	};
	auto FinishValidation = [bBroadcastAttempt, &FinishAttempt](ECaptureResult Result)
	{
		return bBroadcastAttempt ? FinishAttempt(Result) : Result;
	};

	if (!Territory || !AttackingFaction.IsValid()) return FinishValidation(ECaptureResult::InvalidTerritory);
	if (Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly)
	{
		return FinishValidation(ECaptureResult::InvalidTerritory);
	}

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
		return FinishValidation(ECaptureResult::Locked);
	}

	if (DefendingFaction.IsValid() && DefendingFaction == AttackingFaction)
	{
		return FinishValidation(ECaptureResult::AlreadyOwned);
	}

	// DefendersRemain must be checked BEFORE diplomacy so a faction with active defenders
	// inside the territory gets the correct result code instead of DiplomaticallyBlocked.
	if (Territory->GetDefenderCount() > 0 && DefendingFaction.IsValid())
	{
		return FinishValidation(ECaptureResult::DefendersRemain);
	}

	if (DefendingFaction.IsValid() && !CanFactionCaptureTerritory(Territory, AttackingFaction))
	{
		return FinishValidation(ECaptureResult::DiplomaticallyBlocked);
	}

	if (bCommitContestState)
	{
		// Initiate capture only after all admission checks pass.
		if (CurrentState != ETerritoryState::Contested)
		{
			Territory->SetTerritoryState(ETerritoryState::Contested);
			if (Territory->GetTerritoryState() != ETerritoryState::Contested)
			{
				return FinishValidation(ECaptureResult::InvalidTerritory);
			}
			Territory->SetContestingFaction(AttackingFaction);
		}
		else if (!ITerritoryOwnershipInterface::Execute_GetContestingFaction(Territory).IsValid())
		{
			Territory->SetContestingFaction(AttackingFaction);
		}
	}

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	if (!State.CaptureProgressByFaction.Contains(AttackingFaction))
	{
		State.CaptureProgressByFaction.Add(AttackingFaction, 0.f);
	}

	return FinishValidation(ECaptureResult::Success);
}

bool UTerritoryControlSubsystem::CanFactionCaptureTerritory(
	const ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction) const
{
	if (!Territory || !AttackingFaction.IsValid()) return false;

	const FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (!DefendingFaction.IsValid() || DefendingFaction == AttackingFaction) return true;

	if (const UWorld* World = GetWorld())
	{
		if (const UTerritoryDiplomacySubsystem* Diplomacy =
			World->GetSubsystem<UTerritoryDiplomacySubsystem>())
		{
			switch (Diplomacy->GetDiplomacyState(AttackingFaction, DefendingFaction))
			{
			case EDiplomacyState::War:
				return true;
			case EDiplomacyState::Alliance:
			case EDiplomacyState::TradeAgreement:
			case EDiplomacyState::NonAggression:
			case EDiplomacyState::Ceasefire:
				return false;
			case EDiplomacyState::None:
			default:
				break;
			}

			// With no rich treaty, preserve Narrative's neutral policy: only
			// Friendly explicitly blocks capture; Neutral and Hostile allow it.
			if (ANarrativeGameState* NarrativeGS = Cast<ANarrativeGameState>(const_cast<UWorld*>(World)->GetGameState()))
			{
				return NarrativeGS->GetFactionAttitudeTowardsFaction(
					AttackingFaction, DefendingFaction) != ETeamAttitude::Friendly;
			}
		}
	}

	return true;
}

void UTerritoryControlSubsystem::ResetCapture(ATerritoryVolume* Territory)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory) return;
	TerritoryCaptureState.Remove(Territory);
	Territory->SetContestingFaction(FGameplayTag());
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
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !AttackingFaction.IsValid()) return;

	// Revalidate this faction on every external progress mutation.
	if (ValidateAndBeginCapture(Territory, AttackingFaction, false) != ECaptureResult::Success) return;

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	float& Progress = State.CaptureProgressByFaction.FindOrAdd(AttackingFaction);
	Progress = FMath::Clamp(Progress + ProgressDelta, 0.f, 1.f);

	Territory->SetControlProgress(Progress);

	if (Progress >= 1.f)
	{
		CompleteCapture(Territory, AttackingFaction);
	}
}

bool UTerritoryControlSubsystem::ForceCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !NewOwner.IsValid()) return false;
	if (Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly)
	{
		UE_LOG(LogTerritory, Warning, TEXT("[ForceCapture] %s is AggregateOnly; direct capture rejected"),
			*Territory->GetTerritoryTag().ToString());
		return false;
	}

	FGameplayTag OldOwner = Territory->GetOwningFaction();
	TerritoryCaptureState.Remove(Territory);
	Territory->SetContestingFaction(FGameplayTag());
	Territory->ForceSetOwningFaction(NewOwner);

	// SetOwningFaction already sets State=Claimed, but if the territory was
	// Contested before force capture, ensure the state is explicitly Claimed.
	if (Territory->GetTerritoryState() != ETerritoryState::Claimed)
	{
		Territory->ForceSetTerritoryState(ETerritoryState::Claimed);
	}
	Territory->SetControlProgress(1.f);

	// Verify final state matches requested
	if (Territory->GetOwningFaction() == NewOwner && Territory->GetTerritoryState() == ETerritoryState::Claimed)
	{
		UE_LOG(LogTerritory, Log, TEXT("[ForceCapture] %s captured by %s (was %s)"),
			*Territory->GetTerritoryTag().ToString(),
			*NewOwner.ToString(), *OldOwner.ToString());
		OnTerritoryControlChanged.Broadcast(Territory, OldOwner, NewOwner);
		return true;
	}

	UE_LOG(LogTerritory, Error, TEXT("[ForceCapture] %s failed to reach requested state (owner=%s, state=%s)"),
		*Territory->GetTerritoryTag().ToString(),
		*Territory->GetOwningFaction().ToString(),
		*UEnum::GetValueAsString(Territory->GetTerritoryState()));
	return false;
}

void UTerritoryControlSubsystem::RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !Attacker || !Faction.IsValid()) return;

	FPerTerritoryState* ExistingState = TerritoryCaptureState.Find(Territory);
	if (ExistingState)
	{
		if (TSet<TWeakObjectPtr<AActor>>* ExistingActors = ExistingState->AttackersByFaction.Find(Faction))
		{
			PruneInvalidAttackers(*ExistingActors);
			for (const TWeakObjectPtr<AActor>& Existing : *ExistingActors)
			{
				if (Existing.Get() == Attacker) return;
			}
		}
	}

	if (ValidateAndBeginCapture(Territory, Faction, false, false) != ECaptureResult::Success) return;

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	TSet<TWeakObjectPtr<AActor>>& ActorSet = State.AttackersByFaction.FindOrAdd(Faction);

	// Prune stale entries before the budget check so dead weak pointers don't
	// count against the budget.
	PruneInvalidAttackers(ActorSet);

	// Re-check attack budget right before insertion — ValidateAndBeginCapture may
	// have triggered reentrant delegate broadcasts that registered other attackers,
	// consuming the available budget (TOCTOU prevention).
	if (ActorSet.Num() >= Territory->GetMaxConcurrentAttackers())
	{
		return;
	}

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

	// Commit contested state only after the attacker has been admitted. If the
	// state transition is rejected, roll back the reservation completely.
	if (Territory->GetTerritoryState() != ETerritoryState::Contested)
	{
		Territory->SetTerritoryState(ETerritoryState::Contested);
		if (Territory->GetTerritoryState() != ETerritoryState::Contested)
		{
			ActorSet.Remove(Attacker);
			State.CaptureProgressByFaction.Remove(Faction);
			return;
		}
	}
	Territory->SetContestingFaction(Faction);
}

void UTerritoryControlSubsystem::UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !Attacker || !Faction.IsValid()) return;
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Faction);
	if (ActorSet)
	{
		ActorSet->Remove(Attacker);
		if (PruneInvalidAttackers(*ActorSet) == 0)
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
	if (!ActorSet) return 0;

	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Attacker : *ActorSet)
	{
		if (Attacker.IsValid()) ++Count;
	}
	return Count;
}

void UTerritoryControlSubsystem::RestoreCaptureState(
	ATerritoryVolume* Territory,
	const FGameplayTag& ContestingFaction,
	float ControlProgress)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Territory) return;

	// Contested capture resume policy: progress is restored but attackers are transient
	// (not saved). On next EvaluateCaptureState tick with zero attackers, progress decays.
	// This is intentional — contested saves resume as decay-only, not true "resume with participants."
	TerritoryCaptureState.Remove(Territory);
	if (Territory->GetTerritoryState() != ETerritoryState::Contested) return;
	if (!ContestingFaction.IsValid())
	{
		Territory->SetContestingFaction(FGameplayTag());
		Territory->SetControlProgress(0.f);
		Territory->SetTerritoryState(Territory->GetOwningFaction().IsValid()
			? ETerritoryState::Claimed
			: ETerritoryState::Unclaimed);
		return;
	}

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	const float ClampedProgress = FMath::Clamp(ControlProgress, 0.f, 1.f);
	State.CaptureProgressByFaction.Add(ContestingFaction, ClampedProgress);
	Territory->SetContestingFaction(ContestingFaction);
	Territory->SetControlProgress(ClampedProgress);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal — no map mutation from here during iteration
// ═══════════════════════════════════════════════════════════════════════════════

void UTerritoryControlSubsystem::EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime)
{
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;
	if (Territory->IsLocked())
	{
		DeferredCommands.Add({FDeferredCommand::Reset, Territory, FGameplayTag()});
		return;
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	const float ProgressRate = Settings ? Settings->CaptureProgressPerSecond : 0.1f;
	const float DecayRate = Settings ? Settings->CaptureProgressDecayPerSecond : 0.05f;

	// Defender check on every tick — guards that spawn/arrive mid-contest must halt progress
	const bool bDefendersPresent = Territory->GetDefenderCount() > 0 && Territory->GetOwningFaction().IsValid();

	FGameplayTag BestFaction;
	float BestProgress = 0.f;
	int32 BestAttackerCount = 0;

	for (auto& Pair : State->CaptureProgressByFaction)
	{
		TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Pair.Key);
		int32 AttackerCount = ActorSet ? PruneInvalidAttackers(*ActorSet) : 0;

		// Re-validate diplomacy: if a peace treaty was signed mid-capture, this faction
		// should not advance. Decay instead, same as if defenders were present.
		const bool bDiplomaticallyBlocked = AttackerCount > 0
			&& !CanFactionCaptureTerritory(Territory, Pair.Key);

		if (AttackerCount > 0)
		{
			// Diplomatically blocked or defenders present → halt progress (decay instead of advance)
			if (bDefendersPresent || bDiplomaticallyBlocked)
			{
				Pair.Value = FMath::Max(0.f, Pair.Value - DeltaTime * DecayRate);
			}
			else
			{
				Pair.Value = FMath::Clamp(Pair.Value + DeltaTime * ProgressRate, 0.f, 1.f);
			}
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

	// Update ContestingFaction to the leading faction (highest progress, most attackers)
	// so UI/events always show the faction most likely to capture.
	Territory->SetContestingFaction(BestFaction);

	// DEFER completion — don't mutate map during iteration (P0-01)
	if (BestProgress >= 1.f && BestFaction.IsValid())
	{
		DeferredCommands.Add({FDeferredCommand::Complete, Territory, BestFaction});
	}

	// Cleanup zero-progress factions — collect keys first, then remove after iteration
	// to avoid mutating the TMap while iterating it (safe even if reentered).
	TArray<FGameplayTag> ToRemove;
	for (const auto& Pair : State->CaptureProgressByFaction)
	{
		TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Pair.Key);
		const bool bHasValidAttackers = ActorSet && PruneInvalidAttackers(*ActorSet) > 0;
		if (Pair.Value <= 0.f && !bHasValidAttackers)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Tag : ToRemove)
	{
		State->CaptureProgressByFaction.Remove(Tag);
		State->AttackersByFaction.Remove(Tag);
	}

	// Re-fetch State pointer in case deferred commands or delegate listeners
	// invalidated the TMap value references during cleanup above.
	State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	// DEFER reset — don't mutate map during iteration
	if (State->CaptureProgressByFaction.Num() == 0)
	{
		DeferredCommands.Add({FDeferredCommand::Reset, Territory, FGameplayTag()});
	}
}

void UTerritoryControlSubsystem::CompleteCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !NewOwner.IsValid()) return;

	// Re-validate lock state — territory may have been locked via quest event
	// between progress start and completion.
	if (Territory->IsLocked())
	{
		ResetCapture(Territory);
		UE_LOG(LogTerritory, Warning, TEXT("[Capture] %s was locked before completion — capture aborted"),
			*Territory->GetTerritoryTag().ToString());
		return;
	}

	FPerTerritoryState* CaptureState = TerritoryCaptureState.Find(Territory);
	const TSet<TWeakObjectPtr<AActor>>* Attackers = CaptureState
		? CaptureState->AttackersByFaction.Find(NewOwner)
		: nullptr;
	const int32 ActiveAttackers = Attackers ? GetActiveAttackers(Territory, NewOwner) : 0;
	const float* Progress = CaptureState
		? CaptureState->CaptureProgressByFaction.Find(NewOwner)
		: nullptr;
	if (Territory->GetTerritoryState() != ETerritoryState::Contested
		|| ITerritoryOwnershipInterface::Execute_GetContestingFaction(Territory) != NewOwner
		|| ActiveAttackers <= 0
		|| !Progress || *Progress < 1.f
		|| Territory->GetDefenderCount() > 0
		|| !CanFactionCaptureTerritory(Territory, NewOwner))
	{
		ResetCapture(Territory);
		UE_LOG(LogTerritory, Warning, TEXT("[Capture] %s failed final admission validation for %s"),
			*Territory->GetTerritoryTag().ToString(), *NewOwner.ToString());
		return;
	}

	FGameplayTag OldOwner = Territory->GetOwningFaction();
	Territory->SetOwningFaction(NewOwner);
	if (Territory->GetOwningFaction() != NewOwner
		|| Territory->GetTerritoryState() != ETerritoryState::Claimed)
	{
		return;
	}
	TerritoryCaptureState.Remove(Territory);
	Territory->SetContestingFaction(FGameplayTag());

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
