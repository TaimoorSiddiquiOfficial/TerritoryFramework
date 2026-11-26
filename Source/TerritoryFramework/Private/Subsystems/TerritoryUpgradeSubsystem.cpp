#include "Subsystems/TerritoryUpgradeSubsystem.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryUpgradeDefinition.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TerritoryUpgradeBroker.h"
#include "Skills/TerritoryUpgradePerk.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "TimerManager.h"

bool UTerritoryUpgradeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTerritoryUpgradeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UTerritoryUpgradeSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaintenanceTimer);
	}
	Brokers.Reset();
	Super::Deinitialize();
}

void UTerritoryUpgradeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	if (InWorld.GetNetMode() != NM_Client)
	{
		EnsureBrokers();
		InWorld.GetTimerManager().SetTimer(MaintenanceTimer, this,
			&UTerritoryUpgradeSubsystem::TickMaintenance, 1.f, true);
	}
}

int32 UTerritoryUpgradeSubsystem::GetFactionTier(FGameplayTag Faction) const
{
	return Faction.IsValid() ? FactionTiers.FindRef(Faction) : 0;
}

bool UTerritoryUpgradeSubsystem::HasFactionTier(FGameplayTag Faction, int32 MinimumTier) const
{
	return MinimumTier <= 0 || GetFactionTier(Faction) >= MinimumTier;
}

bool UTerritoryUpgradeSubsystem::SetFactionTier(FGameplayTag Faction, int32 Tier)
{
	if (!Faction.IsValid() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	const bool bChanged = SetFactionTierInternal(Faction, FMath::Clamp(Tier, 0, 99));
	if (bChanged)
	{
		EvaluateAIBalance();
	}
	return bChanged;
}

bool UTerritoryUpgradeSubsystem::SetFactionTierInternal(FGameplayTag Faction, int32 Tier)
{
	if (!Faction.IsValid())
	{
		return false;
	}
	int32& Current = FactionTiers.FindOrAdd(Faction);
	if (Current == Tier)
	{
		return false;
	}
	Current = Tier;
	PublishFactionTierToWorldState(Faction, Tier);
	OnFactionTierChanged.Broadcast(Faction, Tier);
	return true;
}

void UTerritoryUpgradeSubsystem::PublishFactionTierToWorldState(FGameplayTag Faction, int32 Tier)
{
	if (ATerritoryWorldState* WorldState = ATerritoryWorldState::FindTerritoryWorldState(this))
	{
		WorldState->SetFactionUpgradeTier(Faction, Tier);
	}
}

bool UTerritoryUpgradeSubsystem::TryUpgradePlace(ATerritoryProperty* Place, AActor* Requester)
{
	return IsValid(Place) && Place->TryUpgrade(Requester);
}

bool UTerritoryUpgradeSubsystem::ApplyScriptedUpgrade(ATerritoryProperty* Place, int32 TargetLevel,
	FGameplayTag RequestingFaction, bool bAllowForeignOwnership, bool bFree, AActor* Requester,
	FString& OutFailure)
{
	OutFailure.Reset();
	if (!IsValid(Place))
	{
		OutFailure = TEXT("The target Place is invalid.");
		return false;
	}
	return Place->ApplyScriptedUpgrade(TargetLevel, RequestingFaction, bAllowForeignOwnership, bFree,
		Requester, OutFailure);
}

bool UTerritoryUpgradeSubsystem::RequestBuyPerk(APlayerController* Controller,
	TSubclassOf<UTerritoryUpgradePerk> PerkClass)
{
	if (!Controller || !PerkClass)
	{
		return false;
	}
	ATerritoryUpgradeBroker* Broker = GetUpgradeBroker(Controller);
	if (!Broker)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[UpgradePerk] No upgrade broker is available yet for %s."), *Controller->GetName());
		return false;
	}
	if (Controller->IsLocalController())
	{
		Broker->ServerBuyPerk(PerkClass);
		return true;
	}
	return false;
}

ATerritoryUpgradeBroker* UTerritoryUpgradeSubsystem::GetUpgradeBroker(APlayerController* Controller) const
{
	if (!Controller || !GetWorld())
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<ATerritoryUpgradeBroker>* Found = Brokers.Find(Controller))
	{
		if (ATerritoryUpgradeBroker* Broker = Found->Get())
		{
			return Broker;
		}
	}
	for (TActorIterator<ATerritoryUpgradeBroker> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == Controller)
		{
			const_cast<UTerritoryUpgradeSubsystem*>(this)->Brokers.Add(
				Controller, *It);
			return *It;
		}
	}
	return nullptr;
}

void UTerritoryUpgradeSubsystem::ImportFactionTiers(const TArray<FReplicatedFactionUpgradeTier>& Tiers)
{
	FactionTiers.Reset();
	for (const FReplicatedFactionUpgradeTier& Entry : Tiers)
	{
		if (Entry.Faction.IsValid())
		{
			FactionTiers.Add(Entry.Faction, FMath::Max(0, Entry.Tier));
		}
	}
	for (const TPair<FGameplayTag, int32>& Pair : FactionTiers)
	{
		OnFactionTierChanged.Broadcast(Pair.Key, Pair.Value);
	}
}

TArray<FReplicatedFactionUpgradeTier> UTerritoryUpgradeSubsystem::GetAllFactionTiers() const
{
	TArray<FReplicatedFactionUpgradeTier> Result;
	Result.Reserve(FactionTiers.Num());
	for (const TPair<FGameplayTag, int32>& Pair : FactionTiers)
	{
		FReplicatedFactionUpgradeTier Entry;
		Entry.Faction = Pair.Key;
		Entry.Tier = Pair.Value;
		Result.Add(Entry);
	}
	Result.Sort([](const FReplicatedFactionUpgradeTier& A, const FReplicatedFactionUpgradeTier& B)
	{
		return A.Faction.ToString() < B.Faction.ToString();
	});
	return Result;
}

void UTerritoryUpgradeSubsystem::NotifyPlaceCaptured(ATerritoryProperty* Place)
{
	if (IsValid(Place))
	{
		EvaluateAIBalance();
	}
}

UTerritoryUpgradeBalanceProfile* UTerritoryUpgradeSubsystem::ResolveBalanceProfile() const
{
	if (const ATerritoryWorldState* WorldState = ATerritoryWorldState::FindTerritoryWorldState(this))
	{
		return WorldState->GetUpgradeBalanceProfile();
	}
	return nullptr;
}

void UTerritoryUpgradeSubsystem::EvaluateAIBalance()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || bEvaluatingBalance)
	{
		return;
	}
	UTerritoryUpgradeBalanceProfile* Profile = ResolveBalanceProfile();
	if (!Profile || !Profile->bAutoUpgradeAIFactions)
	{
		return;
	}
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	TSet<FGameplayTag> PlayerFactions;
	int32 PlayerTier = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		const FGameplayTag Faction = Pawn
			? UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Pawn) : FGameplayTag();
		if (Faction.IsValid())
		{
			PlayerFactions.Add(Faction);
			PlayerTier = FMath::Max(PlayerTier, GetFactionTier(Faction));
		}
	}
	if (PlayerFactions.IsEmpty())
	{
		return;
	}

	const int32 TargetTier = FMath::Clamp(PlayerTier + Profile->TierOffset, 0, Profile->MaxAutoTier);
	if (TargetTier <= 0)
	{
		return;
	}

	TGuardValue<bool> EvaluationGuard(bEvaluatingBalance, true);

	TMap<FGameplayTag, TArray<ATerritoryProperty*>> PlacesByFaction;
	for (ATerritoryVolume* Territory : Registry->GetAllTerritories())
	{
		ATerritoryProperty* Place = Cast<ATerritoryProperty>(Territory);
		if (!Place)
		{
			continue;
		}
		const FGameplayTag Faction = Place->GetOwningFaction();
		if (Faction.IsValid())
		{
			PlacesByFaction.FindOrAdd(Faction).Add(Place);
		}
	}

	for (TPair<FGameplayTag, TArray<ATerritoryProperty*>>& Pair : PlacesByFaction)
	{
		if (PlayerFactions.Contains(Pair.Key))
		{
			continue;
		}
		SetFactionTierInternal(Pair.Key, FMath::Max(GetFactionTier(Pair.Key), TargetTier));
		if (!Profile->bUpgradeAIPlaces)
		{
			continue;
		}
		Pair.Value.Sort([](const ATerritoryProperty& A, const ATerritoryProperty& B)
		{
			return A.GetTerritoryTag().ToString() < B.GetTerritoryTag().ToString();
		});
		for (ATerritoryProperty* Place : Pair.Value)
		{
			while (IsValid(Place) && Place->CanUpgrade()
				&& Place->GetUpgradeLevel() < TargetTier && Place->GetUpgradeLevel() < Place->MaxUpgradeLevel)
			{
				FString Failure;
				const int32 NextLevel = Place->GetUpgradeLevel() + 1;
				if (!Place->ApplyScriptedUpgrade(NextLevel, Pair.Key, false, true, nullptr, Failure))
				{
					break;
				}
			}
		}
	}
}

void UTerritoryUpgradeSubsystem::TickMaintenance()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	EnsureBrokers();

	BalanceAccumulator += 1.f;
	if (const UTerritoryUpgradeBalanceProfile* Profile = ResolveBalanceProfile())
	{
		if (Profile->bAutoUpgradeAIFactions
			&& BalanceAccumulator >= FMath::Max(1.f, Profile->EvaluationInterval))
		{
			BalanceAccumulator = 0.f;
			EvaluateAIBalance();
		}
	}
}

void UTerritoryUpgradeSubsystem::EnsureBrokers()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// A placed or auto-created controller without a pawn cannot own a purchase. Drop its broker
	// and retry on the maintenance tick once it possesses a pawn.
	for (auto It = Brokers.CreateIterator(); It; ++It)
	{
		APlayerController* Controller = It->Key.Get();
		ATerritoryUpgradeBroker* Broker = It->Value.Get();
		if (!Controller || !Controller->GetPawn() || !IsValid(Broker))
		{
			if (IsValid(Broker))
			{
				Broker->Destroy();
			}
			It.RemoveCurrent();
		}
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsPlayerController() || !Controller->GetPawn())
		{
			continue;
		}
		if (ATerritoryUpgradeBroker* Existing = GetUpgradeBroker(Controller))
		{
			if (IsValid(Existing))
			{
				continue;
			}
		}
		FActorSpawnParameters Params;
		Params.Owner = Controller;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ATerritoryUpgradeBroker* Broker = World->SpawnActor<ATerritoryUpgradeBroker>(
			ATerritoryUpgradeBroker::StaticClass(), FTransform::Identity, Params))
		{
			Brokers.Add(Controller, Broker);
		}
	}
}
