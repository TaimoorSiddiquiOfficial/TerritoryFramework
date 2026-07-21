#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/World.h"

void UTerritoryControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTerritory, Log, TEXT("TerritoryControlSubsystem initialized"));
}

void UTerritoryControlSubsystem::Deinitialize()
{
	TerritoryCaptureState.Empty();
	Super::Deinitialize();
}

ECaptureResult UTerritoryControlSubsystem::AttemptCapture(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction)
{
	if (!Territory || !AttackingFaction.IsValid())
	{
		return ECaptureResult::InvalidTerritory;
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
	State.CaptureProgressByFaction.FindOrAdd(AttackingFaction) = 0.f;

	FCaptureAttempt Attempt;
	Attempt.Territory = Territory;
	Attempt.AttackingFaction = AttackingFaction;
	Attempt.DefendingFaction = Territory->GetOwningFaction();
	Attempt.Result = ECaptureResult::Success;
	OnCaptureAttempted.Broadcast(Attempt);

	return ECaptureResult::Success;
}

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

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	float& Progress = State.CaptureProgressByFaction.FindOrAdd(AttackingFaction);
	Progress = FMath::Clamp(Progress + ProgressDelta, 0.f, 1.f);

	Territory->SetControlProgress(Progress);

	if (Progress >= 1.f)
	{
		CompleteCapture(Territory, AttackingFaction);
	}
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

void UTerritoryControlSubsystem::RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!Territory || !Attacker) return;
	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	State.AttackersByFaction.FindOrAdd(Faction)++;
}

void UTerritoryControlSubsystem::UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker)
{
	if (!Territory || !Attacker) return;
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	// TODO: Track which faction this attacker belongs to for proper decrement
	// For now, decrement the first faction with > 0 count
	for (auto& Pair : State->AttackersByFaction)
	{
		if (Pair.Value > 0)
		{
			Pair.Value--;
			break;
		}
	}
}

void UTerritoryControlSubsystem::Tick(float DeltaTime)
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	float ProgressRate = Settings ? Settings->CaptureProgressPerSecond : 0.1f;
	float DecayRate = Settings ? Settings->CaptureProgressDecayPerSecond : 0.05f;

	TArray<TWeakObjectPtr<ATerritoryVolume>> ToRemove;

	for (auto& Pair : TerritoryCaptureState)
	{
		ATerritoryVolume* Territory = Pair.Key.Get();
		if (!Territory)
		{
			ToRemove.Add(Pair.Key);
			continue;
		}

		EvaluateCaptureState(Territory, DeltaTime);
	}

	for (const TWeakObjectPtr<ATerritoryVolume>& WeakTerritory : ToRemove)
	{
		TerritoryCaptureState.Remove(WeakTerritory);
	}
}

void UTerritoryControlSubsystem::EvaluateCaptureState(ATerritoryVolume* Territory, float DeltaTime)
{
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	FGameplayTag BestFaction;
	float BestProgress = 0.f;

	for (auto& Pair : State->CaptureProgressByFaction)
	{
		int32 Attackers = State->AttackersByFaction.FindRef(Pair.Key);
		if (Attackers > 0)
		{
			Pair.Value = FMath::Clamp(Pair.Value + DeltaTime * 0.1f, 0.f, 1.f);
		}
		else
		{
			Pair.Value = FMath::Max(0.f, Pair.Value - DeltaTime * 0.05f);
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

	OnTerritoryControlChanged.Broadcast(Territory, OldOwner, NewOwner);
}
