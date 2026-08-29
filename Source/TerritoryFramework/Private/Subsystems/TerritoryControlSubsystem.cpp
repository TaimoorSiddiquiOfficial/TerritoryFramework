#include "Subsystems/TerritoryControlSubsystem.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryMutationTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryStealthProfile.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "NavigationSystem.h"

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
	TArray<TWeakObjectPtr<ATerritoryVolume>> Territories;
	TerritoryCaptureState.GetKeys(Territories);
	for (const TWeakObjectPtr<ATerritoryVolume>& Territory : Territories)
	{
		ReleaseTerritoryAttackers(Territory.Get());
	}
	for (const auto& Pair : BoundAttackerASCs)
	{
		if (UNarrativeAbilitySystemComponent* ASC = Pair.Value.Get())
		{
			ASC->OnDeathStateChanged.RemoveDynamic(
				this, &UTerritoryControlSubsystem::OnRegisteredAttackerDied);
		}
	}
	TerritoryCaptureState.Empty();
	TerritoryInfiltrationState.Empty();
	StealthInfiltrationOverrides.Empty();
	AttackerRegistrationCounts.Empty();
	BoundAttackerASCs.Empty();
	Super::Deinitialize();
}

UNarrativeAbilitySystemComponent* UTerritoryControlSubsystem::ResolveAttackerASC(AActor* Attacker) const
{
	if (!Attacker) return nullptr;
	if (IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Attacker))
	{
		return Cast<UNarrativeAbilitySystemComponent>(AbilityOwner->GetAbilitySystemComponent());
	}
	if (const AController* Controller = Cast<AController>(Attacker))
	{
		if (IAbilitySystemInterface* PawnAbilityOwner = Cast<IAbilitySystemInterface>(Controller->GetPawn()))
		{
			return Cast<UNarrativeAbilitySystemComponent>(PawnAbilityOwner->GetAbilitySystemComponent());
		}
	}
	return nullptr;
}

void UTerritoryControlSubsystem::AddAttackerRegistration(AActor* Attacker)
{
	if (!Attacker) return;
	TWeakObjectPtr<AActor> WeakAttacker(Attacker);
	int32& RegistrationCount = AttackerRegistrationCounts.FindOrAdd(WeakAttacker);
	++RegistrationCount;
	if (RegistrationCount != 1) return;

	if (UNarrativeAbilitySystemComponent* ASC = ResolveAttackerASC(Attacker))
	{
		ASC->OnDeathStateChanged.AddUniqueDynamic(
			this, &UTerritoryControlSubsystem::OnRegisteredAttackerDied);
		BoundAttackerASCs.Add(WeakAttacker, ASC);
	}
}

void UTerritoryControlSubsystem::ReleaseAttackerRegistration(const TWeakObjectPtr<AActor>& Attacker)
{
	int32* RegistrationCount = AttackerRegistrationCounts.Find(Attacker);
	if (!RegistrationCount) return;
	--(*RegistrationCount);
	if (*RegistrationCount > 0) return;

	if (const TWeakObjectPtr<UNarrativeAbilitySystemComponent>* FoundASC = BoundAttackerASCs.Find(Attacker))
	{
		if (UNarrativeAbilitySystemComponent* ASC = FoundASC->Get())
		{
			ASC->OnDeathStateChanged.RemoveDynamic(
				this, &UTerritoryControlSubsystem::OnRegisteredAttackerDied);
		}
	}
	BoundAttackerASCs.Remove(Attacker);
	AttackerRegistrationCounts.Remove(Attacker);
}

int32 UTerritoryControlSubsystem::PruneInvalidAttackers(TSet<TWeakObjectPtr<AActor>>& Attackers)
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
		ReleaseAttackerRegistration(Attacker);
	}
	return Attackers.Num();
}

int32 UTerritoryControlSubsystem::GetActiveCapturePressure(
	const ATerritoryVolume* Territory, const FGameplayTag& Faction) const
{
	if (!Territory) return 0;
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	const TSet<TWeakObjectPtr<AActor>>* Attackers = State
		? State->AttackersByFaction.Find(Faction) : nullptr;
	if (!Attackers) return 0;
	const TSet<TWeakObjectPtr<AActor>>* NonCapturing =
		State->NonCapturingAttackersByFaction.Find(Faction);

	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Attacker : *Attackers)
	{
		AActor* Actor = Attacker.Get();
		if (!Actor || (NonCapturing && NonCapturing->Contains(Attacker))) continue;
		if (const UNarrativeAbilitySystemComponent* ASC = ResolveAttackerASC(Actor))
		{
			if (ASC->IsDead()) continue;
		}
		++Count;
	}
	return Count;
}

void UTerritoryControlSubsystem::ReleaseTerritoryAttackers(ATerritoryVolume* Territory)
{
	if (!Territory) return;
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;
	for (auto& Pair : State->AttackersByFaction)
	{
		for (const TWeakObjectPtr<AActor>& Attacker : Pair.Value)
		{
			ReleaseAttackerRegistration(Attacker);
		}
	}
	TerritoryCaptureState.Remove(Territory);
}

void UTerritoryControlSubsystem::RemoveAttackerFromAllCaptures(AActor* Attacker)
{
	if (!Attacker) return;
	TWeakObjectPtr<AActor> WeakAttacker(Attacker);
	int32 RemovedRegistrations = 0;
	for (auto& TerritoryPair : TerritoryCaptureState)
	{
		TArray<FGameplayTag> EmptyFactions;
		for (auto& FactionPair : TerritoryPair.Value.AttackersByFaction)
		{
			if (FactionPair.Value.Remove(WeakAttacker) > 0)
			{
				++RemovedRegistrations;
			}
			if (FactionPair.Value.IsEmpty())
			{
				EmptyFactions.Add(FactionPair.Key);
			}
		}
		for (const FGameplayTag& Faction : EmptyFactions)
		{
			TerritoryPair.Value.AttackersByFaction.Remove(Faction);
			TerritoryPair.Value.NonCapturingAttackersByFaction.Remove(Faction);
		}
		for (auto& NonCapturingPair : TerritoryPair.Value.NonCapturingAttackersByFaction)
		{
			NonCapturingPair.Value.Remove(WeakAttacker);
		}
	}

	for (int32 Index = 0; Index < RemovedRegistrations; ++Index)
	{
		ReleaseAttackerRegistration(WeakAttacker);
	}
}

void UTerritoryControlSubsystem::OnRegisteredAttackerDied(
	AActor* KilledActor,
	UNarrativeAbilitySystemComponent* KilledASC,
	const bool bIsDead)
{
	if (!bIsDead) return;
	AActor* RegisteredAttacker = KilledActor;
	if (!AttackerRegistrationCounts.Contains(RegisteredAttacker) && KilledASC)
	{
		for (const auto& Pair : BoundAttackerASCs)
		{
			if (Pair.Value.Get() == KilledASC)
			{
				RegisteredAttacker = Pair.Key.Get();
				break;
			}
		}
	}
	if (!RegisteredAttacker) return;

	RemoveAttackerFromAllCaptures(RegisteredAttacker);
	RemoveInfiltratorFromAllTerritories(RegisteredAttacker);
	UE_LOG(LogTerritory, Verbose, TEXT("[Capture] Removed dead attacker %s from all capture participation"),
		*GetNameSafe(RegisteredAttacker));
}

FTerritoryTransitionContext UTerritoryControlSubsystem::BuildTransitionContext(
	AActor* Attacker,
	const FGameplayTag& Faction) const
{
	FTerritoryTransitionContext Context;
	Context.Instigator = Attacker;
	Context.RequestingFaction = Faction;

	if (APawn* Pawn = Cast<APawn>(Attacker))
	{
		Context.TargetPawn = Pawn;
		Context.PlayerController = Cast<APlayerController>(Pawn->GetController());
	}
	else if (AController* Controller = Cast<AController>(Attacker))
	{
		Context.TargetPawn = Controller->GetPawn();
		Context.PlayerController = Cast<APlayerController>(Controller);
	}
	else if (Attacker)
	{
		Context.TargetPawn = Cast<APawn>(Attacker->GetOwner());
		Context.PlayerController = Context.TargetPawn
			? Cast<APlayerController>(Context.TargetPawn->GetController())
			: nullptr;
	}

	AActor* TalesTarget = Context.PlayerController
		? static_cast<AActor*>(Context.PlayerController.Get())
		: static_cast<AActor*>(Context.TargetPawn.Get());
	Context.TalesComponent = UNarrativeFunctionLibrary::GetTalesComponentFromTarget(TalesTarget);
	return Context;
}

FTerritoryTransitionContext UTerritoryControlSubsystem::ResolveCaptureContext(
	const ATerritoryVolume* Territory,
	const FGameplayTag& Faction) const
{
	if (!Territory) return FTerritoryTransitionContext();
	const FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	const TSet<TWeakObjectPtr<AActor>>* Attackers = State
		? State->AttackersByFaction.Find(Faction)
		: nullptr;
	if (!Attackers) return FTerritoryTransitionContext();

	TArray<AActor*> ValidAttackers;
	for (const TWeakObjectPtr<AActor>& Attacker : *Attackers)
	{
		if (AActor* Actor = Attacker.Get()) ValidAttackers.Add(Actor);
	}
	ValidAttackers.Sort([](const AActor& A, const AActor& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	return ValidAttackers.IsEmpty()
		? FTerritoryTransitionContext()
		: BuildTransitionContext(ValidAttackers[0], Faction);
}

FTerritoryTransitionContext UTerritoryControlSubsystem::ResolveFactionPlayerContext(
	const FGameplayTag& Faction) const
{
	FTerritoryTransitionContext EmptyContext;
	EmptyContext.RequestingFaction = Faction;
	if (!Faction.IsValid() || !GetWorld()) return EmptyContext;

	auto HasExactFaction = [&Faction](const AActor* Actor)
	{
		const INarrativeTeamAgentInterface* TeamAgent =
			Cast<INarrativeTeamAgentInterface>(Actor);
		return TeamAgent && TeamAgent->GetFactions().HasTagExact(Faction);
	};

	TArray<APlayerController*> MatchingControllers;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController) continue;
		if (HasExactFaction(PlayerController)
			|| HasExactFaction(PlayerController->GetPawn())
			|| HasExactFaction(PlayerController->GetPlayerState<APlayerState>()))
		{
			MatchingControllers.Add(PlayerController);
		}
	}

	// Stable selection matters when several players share the same Narrative faction.
	MatchingControllers.Sort([](const APlayerController& A, const APlayerController& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	return MatchingControllers.IsEmpty()
		? EmptyContext
		: BuildTransitionContext(MatchingControllers[0], Faction);
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
	EvaluateInfiltrationState(DeltaTime);

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
				ResetCapture(Territory);
			}
		}
	}

	DeferredCommands.Empty();

	// Cleanup invalid keys
	for (const TWeakObjectPtr<ATerritoryVolume>& WeakTerritory : InvalidKeys)
	{
		if (FPerTerritoryState* State = TerritoryCaptureState.Find(WeakTerritory))
		{
			for (auto& FactionPair : State->AttackersByFaction)
			{
				for (const TWeakObjectPtr<AActor>& Attacker : FactionPair.Value)
				{
					ReleaseAttackerRegistration(Attacker);
				}
			}
		}
		TerritoryCaptureState.Remove(WeakTerritory);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Capture API (Authority-Only Mutations)
// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════
// P1-08: Pure validation — no side effects
// ═══════════════════════════════════════════════════════════════════════════════
ECaptureResult UTerritoryControlSubsystem::ValidateCaptureAttempt(
	const ATerritoryVolume* Territory,
	const FGameplayTag& AttackingFaction) const
{
	if (!Territory || !AttackingFaction.IsValid()) return ECaptureResult::InvalidTerritory;
	if (Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly)
	{
		return ECaptureResult::InvalidTerritory;
	}
	if (Territory->GetTerritoryState() == ETerritoryState::Locked)
	{
		return ECaptureResult::Locked;
	}

	const FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (DefendingFaction.IsValid() && DefendingFaction == AttackingFaction)
	{
		return ECaptureResult::AlreadyOwned;
	}

	// Capture completion is stricter than beginning a contest. Living defenders
	// block ownership/progress, but must not block the conflict which makes those
	// defenders react to the intruding faction.
	if (Territory->GetDefenderCount() > 0)
	{
		return ECaptureResult::DefendersRemain;
	}
	if (DefendingFaction.IsValid() && !CanFactionCaptureTerritory(Territory, AttackingFaction))
	{
		return ECaptureResult::DiplomaticallyBlocked;
	}

	return ECaptureResult::Success;
}

bool UTerritoryControlSubsystem::IsStealthInfiltrationEnabled(
	const ATerritoryVolume* Territory) const
{
	if (!Territory) return false;
	const UTerritoryStealthProfile* Profile = Territory->GetActiveStealthProfile();
	if (!Profile) return false;
	if (const bool* Override = StealthInfiltrationOverrides.Find(Territory))
	{
		return *Override;
	}
	return Profile->bAllowStealthInfiltration;
}

bool UTerritoryControlSubsystem::RegisterInfiltrator(ATerritoryVolume* Territory,
	AActor* Target, const FGameplayTag& Faction)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !Target
		|| !Faction.IsValid() || !IsStealthInfiltrationEnabled(Territory))
	{
		return false;
	}
	if (const UNarrativeAbilitySystemComponent* ASC = ResolveAttackerASC(Target))
	{
		if (ASC->IsDead()) return false;
	}

	TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>& PerTarget =
		TerritoryInfiltrationState.FindOrAdd(Territory);
	const TWeakObjectPtr<AActor> TargetKey(Target);
	FInfiltrationRuntime* Existing = PerTarget.Find(TargetKey);
	if (!Existing)
	{
		FInfiltrationRuntime& Added = PerTarget.Add(TargetKey);
		Added.Faction = Faction;
		Added.Snapshot.bInsideTerritory = true;
		AddAttackerRegistration(Target);
	}
	else
	{
		Existing->Faction = Faction;
		Existing->Snapshot.bInsideTerritory = true;
	}
	return true;
}

void UTerritoryControlSubsystem::UnregisterInfiltrator(ATerritoryVolume* Territory,
	AActor* Target)
{
	if (!Territory || !Target) return;
	TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>* PerTarget =
		TerritoryInfiltrationState.Find(Territory);
	if (!PerTarget) return;
	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (PerTarget->Remove(TargetKey) > 0)
	{
		ReleaseAttackerRegistration(TargetKey);
	}
	if (PerTarget->IsEmpty())
	{
		TerritoryInfiltrationState.Remove(Territory);
	}
}

void UTerritoryControlSubsystem::RemoveInfiltratorFromAllTerritories(AActor* Target)
{
	if (!Target) return;
	const TWeakObjectPtr<AActor> TargetKey(Target);
	int32 RemovedCount = 0;
	TArray<TWeakObjectPtr<ATerritoryVolume>> EmptyTerritories;
	for (auto& Pair : TerritoryInfiltrationState)
	{
		if (Pair.Value.Remove(TargetKey) > 0) ++RemovedCount;
		if (Pair.Value.IsEmpty()) EmptyTerritories.Add(Pair.Key);
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& Territory : EmptyTerritories)
	{
		TerritoryInfiltrationState.Remove(Territory);
	}
	for (int32 Index = 0; Index < RemovedCount; ++Index)
	{
		ReleaseAttackerRegistration(TargetKey);
	}
}

bool UTerritoryControlSubsystem::GetInfiltrationSnapshot(
	const ATerritoryVolume* Territory, const AActor* Target,
	FTerritoryInfiltrationSnapshot& OutSnapshot) const
{
	OutSnapshot = FTerritoryInfiltrationSnapshot();
	if (!Territory || !Target) return false;
	const TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>* PerTarget =
		TerritoryInfiltrationState.Find(Territory);
	const FInfiltrationRuntime* Runtime = PerTarget ? PerTarget->Find(Target) : nullptr;
	if (!Runtime) return false;
	OutSnapshot = Runtime->Snapshot;
	OutSnapshot.ConfirmingObserverCount = Runtime->CurrentSightObservers.Num();
	return true;
}

bool UTerritoryControlSubsystem::IsInfiltratorExposed(
	const ATerritoryVolume* Territory, const AActor* Target) const
{
	FTerritoryInfiltrationSnapshot Snapshot;
	return GetInfiltrationSnapshot(Territory, Target, Snapshot)
		&& Snapshot.ExposureState == ETerritoryExposureState::Exposed;
}

bool UTerritoryControlSubsystem::IsTargetCurrentlySeen(
	const ATerritoryVolume* Territory, const AActor* Target) const
{
	if (!Territory || !Target) return false;
	const TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>* PerTarget =
		TerritoryInfiltrationState.Find(Territory);
	const FInfiltrationRuntime* Runtime = PerTarget ? PerTarget->Find(Target) : nullptr;
	return Runtime && !Runtime->CurrentSightObservers.IsEmpty();
}

void UTerritoryControlSubsystem::ForgetStealthObserver(
	ATerritoryVolume* Territory, AActor* Observer)
{
	if (!Territory || !Observer) return;
	if (TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>* PerTarget =
		TerritoryInfiltrationState.Find(Territory))
	{
		for (auto& Pair : *PerTarget)
		{
			Pair.Value.CurrentSightObservers.Remove(Observer);
			Pair.Value.Snapshot.ConfirmingObserverCount =
				Pair.Value.CurrentSightObservers.Num();
		}
	}
}

void UTerritoryControlSubsystem::SetStealthInfiltrationOverride(
	ATerritoryVolume* Territory, bool bEnabled, bool bClearOverride)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Territory) return;
	if (bClearOverride)
	{
		StealthInfiltrationOverrides.Remove(Territory);
	}
	else
	{
		StealthInfiltrationOverrides.Add(Territory, bEnabled);
	}
}

bool UTerritoryControlSubsystem::ClearInfiltratorExposure(
	ATerritoryVolume* Territory, AActor* Target, bool bResetSuspicion)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Territory || !Target)
	{
		return false;
	}
	TMap<TWeakObjectPtr<AActor>, FInfiltrationRuntime>* PerTarget =
		TerritoryInfiltrationState.Find(Territory);
	FInfiltrationRuntime* Runtime = PerTarget ? PerTarget->Find(Target) : nullptr;
	if (!Runtime) return false;
	const ETerritoryExposureState OldState = Runtime->Snapshot.ExposureState;
	Runtime->CurrentSightObservers.Empty();
	Runtime->Snapshot.ExposureState = bResetSuspicion
		? ETerritoryExposureState::Undetected
		: (Runtime->Snapshot.Suspicion > 0.f
			? ETerritoryExposureState::Suspicious : ETerritoryExposureState::Undetected);
	if (bResetSuspicion) Runtime->Snapshot.Suspicion = 0.f;
	if (OldState != Runtime->Snapshot.ExposureState)
	{
		OnExposureChanged.Broadcast(Territory, Target, OldState,
			Runtime->Snapshot.ExposureState);
	}
	return true;
}

bool UTerritoryControlSubsystem::ReportStealthEvidence(ATerritoryVolume* Territory,
	AActor* Target, AActor* Observer, ETerritoryStealthEvidence Evidence,
	float Strength, const FVector& EvidenceLocation,
	const FVector& EstimatedSourceDirection, bool bConfirmedIdentity)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !Target)
	{
		return false;
	}
	const UTerritoryStealthProfile* Profile = Territory->GetActiveStealthProfile();
	if (!Profile || !IsStealthInfiltrationEnabled(Territory)) return false;

	FGameplayTag Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Target);
	if (!RegisterInfiltrator(Territory, Target, Faction)) return false;
	FInfiltrationRuntime& Runtime = TerritoryInfiltrationState.FindChecked(Territory)
		.FindChecked(Target);
	const ETerritoryExposureState OldState = Runtime.Snapshot.ExposureState;
	const float ClampedStrength = FMath::Clamp(Strength, 0.f, 1.f);
	const float Now = GetWorld()->GetTimeSeconds();

	if (Evidence == ETerritoryStealthEvidence::Sight)
	{
		if (Observer)
		{
			if (ClampedStrength >= Profile->MinimumSightEvidence)
			{
				Runtime.CurrentSightObservers.Add(Observer);
			}
			else
			{
				Runtime.CurrentSightObservers.Remove(Observer);
			}
		}
		if (ClampedStrength >= Profile->MinimumSightEvidence)
		{
			Runtime.Snapshot.Suspicion = FMath::Clamp(
				Runtime.Snapshot.Suspicion + ClampedStrength
					* Profile->SightSuspicionGainPerSecond * 0.25f, 0.f, 1.f);
		}
	}
	else
	{
		float EvidenceSuspicion = ClampedStrength;
		switch (Evidence)
		{
		case ETerritoryStealthEvidence::Gunshot:
			EvidenceSuspicion = Profile->GunshotSuspicion;
			break;
		case ETerritoryStealthEvidence::BulletImpact:
			EvidenceSuspicion = Profile->BulletImpactSuspicion;
			break;
		case ETerritoryStealthEvidence::Corpse:
			EvidenceSuspicion = Profile->CorpseSuspicion;
			break;
		case ETerritoryStealthEvidence::ThrowableDistraction:
			EvidenceSuspicion = Profile->ThrowableDistractionSuspicion;
			break;
		default:
			break;
		}
		Runtime.Snapshot.Suspicion = FMath::Clamp(
			Runtime.Snapshot.Suspicion + EvidenceSuspicion, 0.f, 1.f);
	}

	const bool bForcedExposure = bConfirmedIdentity
		|| (Evidence == ETerritoryStealthEvidence::Sight
			&& ClampedStrength >= Profile->ImmediateSightExposureThreshold)
		|| (Evidence == ETerritoryStealthEvidence::FireSeen
			&& Profile->bFireWhileSeenExposes)
		|| (Evidence == ETerritoryStealthEvidence::Damage
			&& Profile->bDamageImmediatelyExposes)
		|| (Evidence == ETerritoryStealthEvidence::DefenderKilledSeen
			&& Profile->bSeenDefenderKillExposes);
	if (bForcedExposure || Runtime.Snapshot.Suspicion >= 1.f)
	{
		Runtime.Snapshot.ExposureState = ETerritoryExposureState::Exposed;
	}
	else if (Runtime.Snapshot.Suspicion > 0.f)
	{
		Runtime.Snapshot.ExposureState = ETerritoryExposureState::Suspicious;
	}

	if (ClampedStrength > 0.f || Evidence != ETerritoryStealthEvidence::Sight)
	{
		Runtime.Snapshot.LastEvidence = Evidence;
		Runtime.Snapshot.LastEvidenceLocation = EvidenceLocation;
		Runtime.Snapshot.EstimatedSourceDirection =
			EstimatedSourceDirection.GetSafeNormal();
		Runtime.Snapshot.LastEvidenceWorldTime = Now;
	}
	Runtime.Snapshot.ConfirmingObserverCount = Runtime.CurrentSightObservers.Num();
	OnStealthEvidenceReported.Broadcast(Territory, Target, Evidence, Runtime.Snapshot);

	if (Runtime.Snapshot.ExposureState != ETerritoryExposureState::Exposed
		&& (Evidence != ETerritoryStealthEvidence::Sight
			|| OldState == ETerritoryExposureState::Undetected
				&& Runtime.Snapshot.ExposureState == ETerritoryExposureState::Suspicious))
	{
		AssignClosestInvestigators(Territory, Target, Evidence, EvidenceLocation,
			EstimatedSourceDirection, false, *Profile);
	}

	if (OldState != Runtime.Snapshot.ExposureState)
	{
		OnExposureChanged.Broadcast(Territory, Target, OldState,
			Runtime.Snapshot.ExposureState);
	}

	if (OldState != ETerritoryExposureState::Exposed
		&& Runtime.Snapshot.ExposureState == ETerritoryExposureState::Exposed)
	{
		// Narrative Pro's built-in stealth is supplied by the active Crouch ability
		// (Abilities.Crouch) plus StealthRating. A Gameplay Event alone only helps an
		// ability that explicitly listens for it, so also cancel configured stealth
		// abilities on the authoritative ASC. Dedicated project abilities should use
		// Territory.Ability.Stealth or add their own tag to the profile.
		if (UNarrativeAbilitySystemComponent* ASC = ResolveAttackerASC(Target))
		{
			if (Profile->bCancelActiveStealthAbilitiesOnExposure
				&& !Profile->StealthAbilityTagsToCancel.IsEmpty())
			{
				ASC->CancelAbilities(&Profile->StealthAbilityTagsToCancel);
			}

			// Narrative's GA_Crouch is an instant toggle: after activation the ability
			// is no longer active, but its infinite GE_CrouchStealth remains. Canceling
			// the ability therefore cannot end stealth. Remove only explicitly authored
			// temporary effects so permanent skill/equipment bonuses remain intact.
			if (Profile->bRemoveActiveStealthEffectsOnExposure)
			{
				for (const TSubclassOf<UGameplayEffect> EffectClass :
					Profile->StealthGameplayEffectsToRemove)
				{
					if (EffectClass)
					{
						ASC->RemoveActiveGameplayEffectBySourceEffect(
							EffectClass, nullptr, INDEX_NONE);
					}
				}
			}
		}

		if (Profile->bSendBreakStealthGameplayEvent
			&& Profile->BreakStealthGameplayEventTag.IsValid())
		{
			FGameplayEventData Payload;
			Payload.EventTag = Profile->BreakStealthGameplayEventTag;
			Payload.Instigator = Observer;
			Payload.Target = Target;
			Payload.EventMagnitude = Runtime.Snapshot.Suspicion;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
				Target, Profile->BreakStealthGameplayEventTag, Payload);
		}

		if (Profile->EscalationScope != ETerritoryStealthEscalationScope::LocalAlarm)
		{
			TryRegisterContester(Territory, Target, Runtime.Faction);
		}
	}
	return true;
}

void UTerritoryControlSubsystem::EvaluateInfiltrationState(float DeltaTime)
{
	TArray<TWeakObjectPtr<ATerritoryVolume>> InvalidTerritories;
	for (auto& TerritoryPair : TerritoryInfiltrationState)
	{
		ATerritoryVolume* Territory = TerritoryPair.Key.Get();
		if (!Territory)
		{
			InvalidTerritories.Add(TerritoryPair.Key);
			continue;
		}
		const UTerritoryStealthProfile* Profile = Territory->GetActiveStealthProfile();
		TArray<TWeakObjectPtr<AActor>> InvalidTargets;
		for (auto& TargetPair : TerritoryPair.Value)
		{
			AActor* Target = TargetPair.Key.Get();
			if (!Target)
			{
				InvalidTargets.Add(TargetPair.Key);
				continue;
			}
			FInfiltrationRuntime& Runtime = TargetPair.Value;
			for (auto It = Runtime.CurrentSightObservers.CreateIterator(); It; ++It)
			{
				if (!It->IsValid()) It.RemoveCurrent();
			}
			if (Profile && Runtime.Snapshot.ExposureState == ETerritoryExposureState::Suspicious
				&& Runtime.CurrentSightObservers.IsEmpty())
			{
				Runtime.Snapshot.Suspicion = FMath::Max(0.f,
					Runtime.Snapshot.Suspicion - Profile->SuspicionDecayPerSecond * DeltaTime);
				if (Runtime.Snapshot.Suspicion <= 0.f)
				{
					const ETerritoryExposureState OldState = Runtime.Snapshot.ExposureState;
					Runtime.Snapshot.ExposureState = ETerritoryExposureState::Undetected;
					OnExposureChanged.Broadcast(Territory, Target, OldState,
						Runtime.Snapshot.ExposureState);
				}
			}
			Runtime.Snapshot.ConfirmingObserverCount = Runtime.CurrentSightObservers.Num();
		}
		for (const TWeakObjectPtr<AActor>& Target : InvalidTargets)
		{
			TerritoryPair.Value.Remove(Target);
			ReleaseAttackerRegistration(Target);
		}
		if (TerritoryPair.Value.IsEmpty()) InvalidTerritories.Add(TerritoryPair.Key);
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& Territory : InvalidTerritories)
	{
		TerritoryInfiltrationState.Remove(Territory);
		StealthInfiltrationOverrides.Remove(Territory);
	}
}

void UTerritoryControlSubsystem::AssignClosestInvestigators(
	ATerritoryVolume* Territory, AActor* SuspectedSource,
	ETerritoryStealthEvidence Evidence, const FVector& Location,
	const FVector& EstimatedSourceDirection, bool bIdentityConfirmed,
	const UTerritoryStealthProfile& Profile)
{
	if (!Territory || Profile.MaximumInvestigators <= 0) return;
	struct FCandidate
	{
		ATerritoryGuardCharacter* Guard = nullptr;
		double Cost = TNumericLimits<double>::Max();
	};
	TArray<FCandidate> Candidates;
	for (AActor* Defender : Territory->GetRegisteredDefenders())
	{
		ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(Defender);
		if (!Guard || !IsValid(Guard) || Guard->IsActorBeingDestroyed()) continue;
		const double DirectDistance = FVector::Distance(Guard->GetActorLocation(), Location);
		if (DirectDistance > Profile.InvestigationRadius) continue;
		double PathLength = DirectDistance;
		double QueriedLength = 0.0;
		if (UNavigationSystemV1::GetPathLength(this, Guard->GetActorLocation(), Location,
			QueriedLength) == ENavigationQueryResult::Success)
		{
			PathLength = QueriedLength;
		}
		Candidates.Add({Guard, PathLength});
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.Cost, B.Cost)) return A.Cost < B.Cost;
		return GetPathNameSafe(A.Guard) < GetPathNameSafe(B.Guard);
	});
	const int32 Count = FMath::Min(Profile.MaximumInvestigators, Candidates.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Candidates[Index].Guard->RequestTerritoryInvestigation(
			Evidence, Location, EstimatedSourceDirection, SuspectedSource,
			bIdentityConfirmed, Profile);
	}
}

ECaptureResult UTerritoryControlSubsystem::ValidateContestAttempt(
	const ATerritoryVolume* Territory,
	const FGameplayTag& AttackingFaction) const
{
	if (!Territory || !AttackingFaction.IsValid()) return ECaptureResult::InvalidTerritory;
	if (Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly)
	{
		return ECaptureResult::InvalidTerritory;
	}

	const ETerritoryState CurrentState = Territory->GetTerritoryState();
	if (CurrentState == ETerritoryState::Locked)
	{
		return ECaptureResult::Locked;
	}

	const FGameplayTag DefendingFaction = Territory->GetOwningFaction();
	if (DefendingFaction.IsValid() && DefendingFaction == AttackingFaction)
	{
		return ECaptureResult::AlreadyOwned;
	}

	if (DefendingFaction.IsValid() && !CanFactionCaptureTerritory(Territory, AttackingFaction))
	{
		return ECaptureResult::DiplomaticallyBlocked;
	}

	return ECaptureResult::Success;
}

ECaptureResult UTerritoryControlSubsystem::GetCaptureEligibility(
	const ATerritoryVolume* Territory,
	const FGameplayTag& AttackingFaction) const
{
	return ValidateCaptureAttempt(Territory, AttackingFaction);
}

ECaptureResult UTerritoryControlSubsystem::GetContestEligibility(
	const ATerritoryVolume* Territory,
	const FGameplayTag& AttackingFaction) const
{
	return ValidateContestAttempt(Territory, AttackingFaction);
}

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

	// P1-08: Delegate to pure validation
	const ECaptureResult ValidateResult = ValidateCaptureAttempt(Territory, AttackingFaction);
	if (ValidateResult != ECaptureResult::Success)
	{
		return FinishValidation(ValidateResult);
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

	const ETerritoryState CurrentState = Territory->GetTerritoryState();

	if (bCommitContestState)
	{
		// P1-06: Only mutate territory state and create capture entries when committing
		// Initiate capture only after all admission checks pass.
		const FGameplayTag CurrentContestingFaction =
			Territory->GetOwnershipData().ContestingFaction;
		if (CurrentState != ETerritoryState::Contested || !CurrentContestingFaction.IsValid())
		{
			const FGameplayTag CommittedFaction = CurrentContestingFaction.IsValid()
				? CurrentContestingFaction : AttackingFaction;
			const FPerTerritoryState* ExistingCaptureState =
				TerritoryCaptureState.Find(Territory);
			const float* ExistingProgress = ExistingCaptureState
				? ExistingCaptureState->CaptureProgressByFaction.Find(CommittedFaction)
				: nullptr;
			const float ContestProgress = ExistingProgress ? *ExistingProgress : 0.f;
			if (!CommitCaptureReadModel(Territory, ETerritoryState::Contested,
				CommittedFaction, ContestProgress,
				ResolveCaptureContext(Territory, AttackingFaction)))
			{
				return FinishValidation(ECaptureResult::InvalidTerritory);
			}
		}

		FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
		if (!State.CaptureProgressByFaction.Contains(AttackingFaction))
		{
			State.CaptureProgressByFaction.Add(AttackingFaction, 0.f);
		}
	}
	// P1-06: When bCommitContestState=false (validation-only from RegisterAttacker),
	// do NOT create capture state entries or mutate territory state.

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

bool UTerritoryControlSubsystem::CommitCaptureReadModel(
	ATerritoryVolume* Territory, ETerritoryState NewState,
	const FGameplayTag& ContestingFaction, float ControlProgress,
	const FTerritoryTransitionContext& TransitionContext) const
{
	if (!Territory || !Territory->HasAuthority()) return false;

	if (Territory->GetTerritoryState() != NewState)
	{
		FText ConditionFailure;
		if (!Territory->CheckStateConditions(NewState, ConditionFailure, TransitionContext))
		{
			UE_LOG(LogTerritory, Warning,
				TEXT("[Capture] %s atomic state transition to %d rejected: %s"),
				*Territory->GetTerritoryTag().ToString(), static_cast<int32>(NewState),
				*ConditionFailure.ToString());
			return false;
		}
	}

	FTerritoryOwnershipData Candidate = Territory->GetOwnershipData();
	Candidate.State = NewState;
	Candidate.ContestingFaction = ContestingFaction;
	Candidate.ControlProgress = FMath::Clamp(ControlProgress, 0.f, 1.f);

	const FTerritoryOwnershipData BeforeCommit = Territory->GetOwnershipData();
	const bool bAlreadyCommitted = BeforeCommit.State == Candidate.State
		&& BeforeCommit.ContestingFaction == Candidate.ContestingFaction
		&& FMath::IsNearlyEqual(BeforeCommit.ControlProgress, Candidate.ControlProgress);
	if (bAlreadyCommitted) return true;

	if (!Territory->CommitOwnershipData(Candidate, TransitionContext)) return false;
	const FTerritoryOwnershipData Committed = Territory->GetOwnershipData();
	return Committed.State == Candidate.State
		&& Committed.ContestingFaction == Candidate.ContestingFaction
		&& FMath::IsNearlyEqual(Committed.ControlProgress, Candidate.ControlProgress);
}

void UTerritoryControlSubsystem::ResetCapture(ATerritoryVolume* Territory)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory) return;
	const FGameplayTag DepartingFaction =
		Territory->GetOwnershipData().ContestingFaction;
	FTerritoryTransitionContext TransitionContext =
		ResolveCaptureContext(Territory, DepartingFaction);
	// Preserve the exact faction that caused this conflict even when its pawn died
	// or streamed before the reset. Claimed-state diplomacy can then end the same
	// War pair instead of falling back to a serialized Heroes/Bandits assumption.
	TransitionContext.RequestingFaction = DepartingFaction;
	ReleaseTerritoryAttackers(Territory);
	const ETerritoryState RecoveredState = Territory->GetOwningFaction().IsValid()
		? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
	CommitCaptureReadModel(Territory, RecoveredState, FGameplayTag(), 0.f,
		TransitionContext);
}

void UTerritoryControlSubsystem::ClearCaptureTrackingOnly(ATerritoryVolume* Territory)
{
	// P0-01: Remove only the internal tracking map — do NOT call SetContestingFaction,
	// SetTerritoryState, or SetControlProgress. Those fields were already committed
	// atomically by CommitOwnershipData and must not be overwritten.
	if (Territory)
	{
		ReleaseTerritoryAttackers(Territory);
	}
}

void UTerritoryControlSubsystem::AddCaptureProgress(ATerritoryVolume* Territory, const FGameplayTag& AttackingFaction, float ProgressDelta)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !AttackingFaction.IsValid()) return;

	// Revalidate this faction on every external progress mutation.
	if (ValidateAndBeginCapture(Territory, AttackingFaction, false) != ECaptureResult::Success) return;

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	float& Progress = State.CaptureProgressByFaction.FindOrAdd(AttackingFaction);
	Progress = FMath::Clamp(Progress + ProgressDelta, 0.f, 1.f);

	CommitCaptureReadModel(Territory, ETerritoryState::Contested,
		Territory->GetOwnershipData().ContestingFaction, Progress,
		ResolveCaptureContext(Territory, AttackingFaction));

	if (Progress >= 1.f)
	{
		CompleteCapture(Territory, AttackingFaction);
	}
}

bool UTerritoryControlSubsystem::ForceCapture(ATerritoryVolume* Territory, const FGameplayTag& NewOwner)
{
	// Preserve the legacy Blueprint node while recovering explicit player context from
	// Narrative's faction authority. This is deterministic and never relies on a first PC.
	return ForceCaptureWithContext(Territory, NewOwner, ResolveFactionPlayerContext(NewOwner));
}

bool UTerritoryControlSubsystem::ForceCaptureWithContext(ATerritoryVolume* Territory,
	const FGameplayTag& NewOwner, const FTerritoryTransitionContext& TransitionContext)
{
	// P1-06: Route through ApplyTerritoryMutation with all bypass flags set.
	// This ensures single authoritative path, proper event ordering, and structured response.
	FTerritoryMutationRequest Request;
	Request.Territory = Territory;
	Request.NewOwner = NewOwner;
	Request.DesiredState = ETerritoryState::Claimed;
	Request.bBypassConditions = true;
	Request.bBypassDiplomacy = true;
	Request.bBypassLock = true;
	Request.bBypassDefenders = true;
	Request.TransitionContext = TransitionContext;
	Request.TransitionContext.RequestingFaction = NewOwner;

	const FTerritoryMutationResponse Response = ApplyTerritoryMutation(Request);

	if (Response.Result == ETerritoryMutationResult::Success)
	{
		UE_LOG(LogTerritory, Log, TEXT("[ForceCapture] %s captured by %s (was %s)"),
			*Territory->GetTerritoryTag().ToString(),
			*NewOwner.ToString(), *Response.OldOwner.ToString());
		return true;
	}

	// P1-06: Log structured failure instead of silently returning false
	UE_LOG(LogTerritory, Warning, TEXT("[ForceCapture] %s rejected: %s (result=%d)"),
		Territory ? *Territory->GetTerritoryTag().ToString() : TEXT("<null>"),
		*Response.Explanation.ToString(),
		static_cast<int32>(Response.Result));
	return false;
}

FTerritoryMutationResponse UTerritoryControlSubsystem::ApplyTerritoryMutation(const FTerritoryMutationRequest& Request)
{
	FTerritoryMutationResponse Response;
	Response.Territory = Request.Territory;

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 1: Validate authority and target
	// ═══════════════════════════════════════════════════════════════════════════
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		Response.Result = ETerritoryMutationResult::Rejected_Authority;
		Response.Explanation = FText::FromString(TEXT("No authority — server-only mutation"));
		return Response;
	}

	ATerritoryVolume* Territory = Request.Territory;
	if (!Territory)
	{
		Response.Result = ETerritoryMutationResult::Rejected_NullTerritory;
		Response.Explanation = FText::FromString(TEXT("Null territory"));
		return Response;
	}

	if (Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly)
	{
		Response.Result = ETerritoryMutationResult::Rejected_AggregateOnly;
		Response.Explanation = FText::FromString(TEXT("AggregateOnly territory cannot be directly mutated"));
		return Response;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 2: Validate locks
	// ═══════════════════════════════════════════════════════════════════════════
	if (Territory->IsLocked() && Request.DesiredState != ETerritoryState::Locked
		&& !Request.bBypassLock)
	{
		Response.Result = ETerritoryMutationResult::Rejected_Locked;
		Response.Explanation = FText::FromString(TEXT("Territory is locked"));
		return Response;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 3: Validate diplomacy — attacker faction vs current owner
	// ═══════════════════════════════════════════════════════════════════════════
	Response.OldOwner = Territory->GetOwningFaction();
	Response.OldState = Territory->GetTerritoryState();

	// Every ownership-changing capture route, including Narrative events and the
	// public Set Owning Faction node, must respect the physical fight. Only an
	// explicitly forced mutation may bypass living registered defenders.
	if (Request.DesiredState == ETerritoryState::Claimed
		&& Request.NewOwner.IsValid()
		&& Request.NewOwner != Response.OldOwner
		&& Territory->GetDefenderCount() > 0
		&& !Request.bBypassDefenders)
	{
		Response.Result = ETerritoryMutationResult::Rejected_DefendersRemain;
		Response.Explanation = FText::Format(
			NSLOCTEXT("Territory", "DefendersRemain",
				"{0} still has {1} living defender(s); defeat them before capture"),
			FText::FromString(Territory->GetTerritoryTag().ToString()),
			FText::AsNumber(Territory->GetDefenderCount()));
		return Response;
	}

	if (Request.NewOwner.IsValid() && Response.OldOwner.IsValid() && Request.NewOwner != Response.OldOwner)
	{
		// P1-06: Skip diplomacy check when bypass is requested (ForceCapture)
		if (!Request.bBypassDiplomacy && !CanFactionCaptureTerritory(Territory, Request.NewOwner))
		{
			Response.Result = ETerritoryMutationResult::Rejected_DiplomacyBlocked;
			Response.Explanation = FText::Format(
				NSLOCTEXT("Territory", "DiploBlocked", "Diplomacy blocks {0} from capturing {1}"),
				FText::FromString(Request.NewOwner.ToString()),
				FText::FromString(Territory->GetTerritoryTag().ToString()));
			return Response;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 4: Validate invariant — owner/state/progress consistency
	// P0-01: Restrict to terminal states only. Contested state is managed by
	// AttemptCapture/RegisterAttacker/EvaluateCaptureState, not by this API.
	// ═══════════════════════════════════════════════════════════════════════════
	if (Request.DesiredState == ETerritoryState::Contested)
	{
		Response.Result = ETerritoryMutationResult::Rejected_InvalidFaction;
		Response.Explanation = FText::FromString(TEXT("Contested state cannot be set via ApplyTerritoryMutation — use AttemptCapture/RegisterAttacker"));
		return Response;
	}
	if (Request.DesiredState == ETerritoryState::Unclaimed && Request.NewOwner.IsValid())
	{
		Response.Result = ETerritoryMutationResult::Rejected_InvalidFaction;
		Response.Explanation = FText::FromString(TEXT("Unclaimed state requires invalid NewOwner"));
		return Response;
	}
	if (Request.DesiredState == ETerritoryState::Claimed && !Request.NewOwner.IsValid())
	{
		Response.Result = ETerritoryMutationResult::Rejected_InvalidFaction;
		Response.Explanation = FText::FromString(TEXT("Claimed state requires valid NewOwner"));
		return Response;
	}

	// P0-02: Validate Narrative conditions through the transition context
	if (!Request.bBypassConditions)
	{
		FText ConditionFailure;
		if (!Territory->CheckStateConditions(Request.DesiredState, ConditionFailure, Request.TransitionContext))
		{
			Response.Result = ETerritoryMutationResult::Rejected_ConditionsFailed;
			Response.Explanation = ConditionFailure.IsEmpty()
				? FText::FromString(TEXT("State entry conditions not met"))
				: ConditionFailure;
			return Response;
		}
	}

	// P1-04: No-op detection is now handled by CommitOwnershipData's full-field comparison.
	// Removed the owner/state-only early return that missed contesting faction, progress, and guard count changes.

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 6: Build candidate FTerritoryOwnershipData
	// ═══════════════════════════════════════════════════════════════════════════
	FTerritoryOwnershipData Candidate = Territory->GetOwnershipData();
	Candidate.OwningFaction = Request.NewOwner;
	Candidate.State = Request.DesiredState;

	// P0-04: Terminal-state normalization is mandatory — always enforce invariants
	// regardless of bClearCaptureState. Terminal states have strict contracts:
	//   Claimed:   valid owner, no contesting faction, progress 1.0
	//   Unclaimed: no owner, no contesting faction, progress 0.0
	//   Locked:    no contesting faction, progress 0.0
	Candidate.ContestingFaction = FGameplayTag();
	switch (Request.DesiredState)
	{
	case ETerritoryState::Claimed:
		Candidate.ControlProgress = 1.f;
		break;
	case ETerritoryState::Unclaimed:
	case ETerritoryState::Locked:
		Candidate.ControlProgress = 0.f;
		break;
	default:
		break;
	}

	// P1-01/P1-07: Only reset guard count when owner actually changes.
	// Same-owner mutations preserve purchased garrison targets.
	const bool bOwnerChanged = (Response.OldOwner != Request.NewOwner);
	if (bOwnerChanged && Request.NewOwner.IsValid() && Request.DesiredState == ETerritoryState::Claimed)
	{
		Candidate.DesiredGuardCount = Territory->GetPostCaptureGuardCountForOwner(
			Request.TransitionContext, Request.NewOwner);
	}
	else if (bOwnerChanged && !Request.NewOwner.IsValid())
	{
		Candidate.DesiredGuardCount = 0;
	}
	// else: same owner — preserve existing DesiredGuardCount

	// Clear lock reason unless transitioning to Locked
	if (Request.DesiredState != ETerritoryState::Locked)
	{
		Candidate.LockReason = FText();
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 7: Atomic commit — one struct write, one event bundle
	// P1-05: Do NOT clear capture state before commit — clear only after success
	// ═══════════════════════════════════════════════════════════════════════════
	const bool bApplied = Territory->CommitOwnershipData(Candidate, Request.TransitionContext);
	if (!bApplied)
	{
		Response.Result = ETerritoryMutationResult::Rejected_StateUnchanged;
		Response.NewOwner = Response.OldOwner;
		Response.NewState = Response.OldState;
		Response.Explanation = FText::FromString(TEXT("CommitOwnershipData returned false — no change applied"));
		return Response;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 8: Verify final state
	// P1-02: Guard reconciliation already happened inside CommitOwnershipData.
	// Do NOT call DespawnGuards/SpawnGuards again here.
	// ═══════════════════════════════════════════════════════════════════════════
	Response.NewOwner = Territory->GetOwningFaction();
	Response.NewState = Territory->GetTerritoryState();

	if (Response.NewOwner != Request.NewOwner || Response.NewState != Request.DesiredState)
	{
		Response.Result = ETerritoryMutationResult::Failed_FinalStateMismatch;
		Response.Explanation = FText::Format(
			NSLOCTEXT("Territory", "MutationMismatch", "Final state mismatch: owner={0} state={1}"),
			FText::FromString(Response.NewOwner.ToString()),
			FText::AsNumber(static_cast<int32>(Response.NewState)));
		return Response;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Step 9: Success — clear capture state and broadcast subsystem delegate
	// P1-05: Only clear capture state AFTER commit succeeded (post-commit cleanup)
	// ═══════════════════════════════════════════════════════════════════════════
	ReleaseTerritoryAttackers(Territory);

	Response.Result = ETerritoryMutationResult::Success;
	Response.Explanation = FText::Format(
		NSLOCTEXT("Territory", "MutationSuccess", "{0}: {1} → {2}"),
		FText::FromString(Territory->GetTerritoryTag().ToString()),
		FText::FromString(Response.OldOwner.ToString()),
		FText::FromString(Response.NewOwner.ToString()));

	UE_LOG(LogTerritory, Log, TEXT("[Mutation] %s"), *Response.Explanation.ToString());
	OnTerritoryControlChanged.Broadcast(Territory, Response.OldOwner, Response.NewOwner);

	return Response;
}

void UTerritoryControlSubsystem::RegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	TryRegisterAttacker(Territory, Attacker, Faction);
}

bool UTerritoryControlSubsystem::TryRegisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	return TryRegisterAttackerInternal(Territory, Attacker, Faction, true);
}

bool UTerritoryControlSubsystem::TryRegisterContester(ATerritoryVolume* Territory,
	AActor* Attacker, const FGameplayTag& Faction)
{
	return TryRegisterAttackerInternal(Territory, Attacker, Faction, false);
}

bool UTerritoryControlSubsystem::TryRegisterAttackerInternal(
	ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction,
	const bool bContributesCaptureProgress)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !Attacker || !Faction.IsValid()) return false;
	if (const UNarrativeAbilitySystemComponent* ASC = ResolveAttackerASC(Attacker))
	{
		if (ASC->IsDead()) return false;
	}

	FPerTerritoryState* ExistingState = TerritoryCaptureState.Find(Territory);
	if (ExistingState)
	{
		if (TSet<TWeakObjectPtr<AActor>>* ExistingActors = ExistingState->AttackersByFaction.Find(Faction))
		{
			PruneInvalidAttackers(*ExistingActors);
			for (const TWeakObjectPtr<AActor>& Existing : *ExistingActors)
			{
				if (Existing.Get() == Attacker)
				{
					// A real capture-pressure registration promotes a prior story-only
					// contest. A contest-only request never demotes an existing attacker.
					if (bContributesCaptureProgress)
					{
						if (TSet<TWeakObjectPtr<AActor>>* NonCapturing =
							ExistingState->NonCapturingAttackersByFaction.Find(Faction))
						{
							NonCapturing->Remove(Attacker);
							if (NonCapturing->IsEmpty())
							{
								ExistingState->NonCapturingAttackersByFaction.Remove(Faction);
							}
						}
					}
					return true;
				}
			}
		}
	}

	// Conflict and completion use separate criteria. A living garrison is allowed
	// to be contested (that is what wakes its guards), while Locked/owned/allied
	// Places still reject the participant.
	if (ValidateContestAttempt(Territory, Faction) != ECaptureResult::Success) return false;

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	TSet<TWeakObjectPtr<AActor>>& ActorSet = State.AttackersByFaction.FindOrAdd(Faction);

	// Prune stale entries before the budget check so dead weak pointers don't
	// count against the budget.
	PruneInvalidAttackers(ActorSet);

	// Re-check attack budget right before insertion — ValidateAndBeginCapture may
	// have triggered reentrant delegate broadcasts that registered other attackers,
	// consuming the available budget (TOCTOU prevention).
	// Use GetActiveAttackers for consistency with HasAttackBudget — dead-ASC
	// actors in the set should not consume budget.
	if (GetActiveAttackers(Territory, Faction) >= Territory->GetMaxConcurrentAttackers())
	{
		return false;
	}

	// Identity-based — adding the same actor twice is a no-op
	int32 BeforeCount = ActorSet.Num();
	ActorSet.Add(Attacker);
	int32 AfterCount = ActorSet.Num();
	if (AfterCount > BeforeCount)
	{
		AddAttackerRegistration(Attacker);
		if (!bContributesCaptureProgress)
		{
			State.NonCapturingAttackersByFaction.FindOrAdd(Faction).Add(Attacker);
		}
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (Settings && Settings->ShouldDebugCapture() && AfterCount > BeforeCount)
	{
		UE_LOG(LogTerritory, Log, TEXT("[Attacker] %s registered for %s in %s (total: %d)"),
			*Attacker->GetName(), *Faction.ToString(),
			*Territory->GetTerritoryTag().ToString(), AfterCount);
	}

	// Seed progress if not already present
	const bool bSeededProgress = !State.CaptureProgressByFaction.Contains(Faction);
	if (bSeededProgress)
	{
		State.CaptureProgressByFaction.Add(Faction, 0.f);
	}

	// Commit the complete contested read model only after the attacker has been
	// admitted. State-change listeners must never observe Contested with an empty
	// contesting faction. If the transition is rejected, roll back completely.
	const FGameplayTag ExistingContestingFaction =
		Territory->GetOwnershipData().ContestingFaction;
	const FGameplayTag CommittedFaction = ExistingContestingFaction.IsValid()
		? ExistingContestingFaction : Faction;
	const float ContestProgress = State.CaptureProgressByFaction.FindRef(CommittedFaction);
	if (!CommitCaptureReadModel(Territory, ETerritoryState::Contested,
		CommittedFaction, ContestProgress,
		BuildTransitionContext(Attacker, Faction)))
	{
		ActorSet.Remove(Attacker);
		if (TSet<TWeakObjectPtr<AActor>>* NonCapturing =
			State.NonCapturingAttackersByFaction.Find(Faction))
		{
			NonCapturing->Remove(Attacker);
			if (NonCapturing->IsEmpty())
			{
				State.NonCapturingAttackersByFaction.Remove(Faction);
			}
		}
		ReleaseAttackerRegistration(Attacker);
		// Only remove the progress entry if we seeded it — pre-existing progress
		// (from AddCaptureProgress or a prior attacker) must survive a failed
		// registration so the capture can continue with remaining attackers.
		if (bSeededProgress)
		{
			State.CaptureProgressByFaction.Remove(Faction);
		}
		return false;
	}
	return ActorSet.Contains(Attacker);
}

void UTerritoryControlSubsystem::UnregisterAttacker(ATerritoryVolume* Territory, AActor* Attacker, const FGameplayTag& Faction)
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Territory || !Attacker || !Faction.IsValid()) return;
	FPerTerritoryState* State = TerritoryCaptureState.Find(Territory);
	if (!State) return;

	TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Faction);
	if (ActorSet)
	{
		if (ActorSet->Remove(Attacker) > 0)
		{
			ReleaseAttackerRegistration(Attacker);
		}
		if (TSet<TWeakObjectPtr<AActor>>* NonCapturing =
			State->NonCapturingAttackersByFaction.Find(Faction))
		{
			NonCapturing->Remove(Attacker);
			if (NonCapturing->IsEmpty())
			{
				State->NonCapturingAttackersByFaction.Remove(Faction);
			}
		}
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
	// Zero-progress story confrontations still have a real contesting faction.
	// The replicated Territory read model is the authority for that state.
	if (Territory->GetTerritoryState() == ETerritoryState::Contested
		&& Territory->GetOwnershipData().ContestingFaction.IsValid())
	{
		return Territory->GetOwnershipData().ContestingFaction;
	}
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
		AActor* Actor = Attacker.Get();
		if (!Actor) continue;
		const UNarrativeAbilitySystemComponent* ASC = ResolveAttackerASC(Actor);
		if (!ASC || !ASC->IsDead()) ++Count;
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
	ReleaseTerritoryAttackers(Territory);
	if (Territory->GetTerritoryState() != ETerritoryState::Contested) return;
	if (!ContestingFaction.IsValid())
	{
		CommitCaptureReadModel(Territory,
			Territory->GetOwningFaction().IsValid()
				? ETerritoryState::Claimed : ETerritoryState::Unclaimed,
			FGameplayTag(), 0.f);
		return;
	}

	// P2-N05: Validate diplomacy before restoring — if peace was signed between save and load,
	// don't resume capture for an allied faction
	if (!CanFactionCaptureTerritory(Territory, ContestingFaction))
	{
		UE_LOG(LogTerritory, Warning, TEXT("[RestoreCaptureState] %s: diplomacy blocks %s — skipping restore"),
			*Territory->GetTerritoryTag().ToString(), *ContestingFaction.ToString());
		CommitCaptureReadModel(Territory,
			Territory->GetOwningFaction().IsValid()
				? ETerritoryState::Claimed : ETerritoryState::Unclaimed,
			FGameplayTag(), 0.f);
		return;
	}

	FPerTerritoryState& State = TerritoryCaptureState.FindOrAdd(Territory);
	const float ClampedProgress = FMath::Clamp(ControlProgress, 0.f, 1.f);
	State.CaptureProgressByFaction.Add(ContestingFaction, ClampedProgress);
	CommitCaptureReadModel(Territory, ETerritoryState::Contested,
		ContestingFaction, ClampedProgress);
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
	const bool bDefendersPresent = Territory->GetDefenderCount() > 0;

	FGameplayTag BestFaction;
	float BestProgress = 0.f;
	int32 BestAttackerCount = 0;

	for (auto& Pair : State->CaptureProgressByFaction)
	{
		TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Pair.Key);
		int32 AttackerCount = ActorSet ? PruneInvalidAttackers(*ActorSet) : 0;
		TSet<TWeakObjectPtr<AActor>>* NonCapturingSet =
			State->NonCapturingAttackersByFaction.Find(Pair.Key);
		if (NonCapturingSet)
		{
			for (auto It = NonCapturingSet->CreateIterator(); It; ++It)
			{
				if (!It->IsValid() || !ActorSet || !ActorSet->Contains(*It))
				{
					It.RemoveCurrent();
				}
			}
		}
		const int32 CapturePressureCount = GetActiveCapturePressure(Territory, Pair.Key);

		// Re-validate diplomacy: if a peace treaty was signed mid-capture, this faction
		// should not advance. Decay instead, same as if defenders were present.
		const bool bDiplomaticallyBlocked = AttackerCount > 0
			&& !CanFactionCaptureTerritory(Territory, Pair.Key);

		if (AttackerCount > 0)
		{
			// Diplomatically blocked, defenders present, or a story-only contester:
			// keep the Place Contested but do not grant automatic ownership pressure.
			if (bDefendersPresent || bDiplomaticallyBlocked || CapturePressureCount <= 0)
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
		if (!BestFaction.IsValid() && AttackerCount > 0)
		{
			// A zero-progress story confrontation still has a real contesting faction.
			bWins = true;
		}
		else if (Pair.Value > BestProgress)
		{
			bWins = true;
		}
		else if (Pair.Value == BestProgress && AttackerCount > 0)
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

	// Progress and its leading faction form one replicated read model. Commit them
	// together so UI, assault scheduling, and save listeners never see a mixed frame.
	CommitCaptureReadModel(Territory, ETerritoryState::Contested,
		BestFaction, BestProgress);

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
		// P2-N04: Already pruned in first loop above — just check remaining count
		TSet<TWeakObjectPtr<AActor>>* ActorSet = State->AttackersByFaction.Find(Pair.Key);
		const bool bHasValidAttackers = ActorSet && ActorSet->Num() > 0;
		if (Pair.Value <= 0.f && !bHasValidAttackers)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Tag : ToRemove)
	{
		State->CaptureProgressByFaction.Remove(Tag);
		State->AttackersByFaction.Remove(Tag);
		State->NonCapturingAttackersByFaction.Remove(Tag);
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
	const int32 ActiveCapturePressure = GetActiveCapturePressure(Territory, NewOwner);
	const float* Progress = CaptureState
		? CaptureState->CaptureProgressByFaction.Find(NewOwner)
		: nullptr;
	if (Territory->GetTerritoryState() != ETerritoryState::Contested
		|| Territory->GetOwnershipData().ContestingFaction != NewOwner
		|| ActiveAttackers <= 0
		|| ActiveCapturePressure <= 0
		|| !Progress || *Progress < 1.f
		|| Territory->GetDefenderCount() > 0
		|| !CanFactionCaptureTerritory(Territory, NewOwner))
	{
		ResetCapture(Territory);
		UE_LOG(LogTerritory, Warning, TEXT("[Capture] %s failed final admission validation for %s"),
			*Territory->GetTerritoryTag().ToString(), *NewOwner.ToString());
		return;
	}

	const FGameplayTag OldOwner = Territory->GetOwningFaction();
	FTerritoryMutationRequest Request;
	Request.Territory = Territory;
	Request.NewOwner = NewOwner;
	Request.DesiredState = ETerritoryState::Claimed;
	Request.TransitionContext = ResolveCaptureContext(Territory, NewOwner);
	const FTerritoryMutationResponse Response = ApplyTerritoryMutation(Request);
	if (Response.Result != ETerritoryMutationResult::Success)
	{
		ResetCapture(Territory);
		UE_LOG(LogTerritory, Warning, TEXT("[Capture] %s: atomic completion rejected for %s — %s"),
			*Territory->GetTerritoryTag().ToString(), *NewOwner.ToString(),
			*Response.Explanation.ToString());
		return;
	}

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

}
