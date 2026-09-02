#include "Combat/TerritoryCombatDirector.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "AI/NarrativeNPCController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "GameFramework/GameUserSettings.h"

void UTerritoryCombatDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugCombat())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Combat] director initialized (assault budget manager)"));
	}
}

void UTerritoryCombatDirector::Deinitialize()
{
	TArray<TWeakObjectPtr<ANarrativeNPCController>> Controllers = BoundControllers.Array();
	for (const TWeakObjectPtr<ANarrativeNPCController>& Controller : Controllers)
	{
		UnbindControllerDeath(Controller.Get());
	}
	SlotMap.Empty();
	BoundControllers.Empty();
	BoundControllerASCs.Empty();
	Super::Deinitialize();
}

bool UTerritoryCombatDirector::RequestAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return false;
	if (!IsEligibleAssaultController(Territory, Controller)) return false;

	if (!Territory->IsAvailableForGameplay()) return false;

	// Periodically clean stale territory keys (destroyed territories) to prevent
	// SlotMap from accumulating dead entries over time.
	CleanupStaleTerritoryKeys();

	FPerTerritorySlots& Slots = SlotMap.FindOrAdd(Territory);
	CleanupInvalidControllers(Slots);

	// Check if this controller already has a slot
	for (const TWeakObjectPtr<ANarrativeNPCController>& Existing : Slots.GrantedControllers)
	{
		if (Existing.Get() == Controller) return true;
	}

	const int32 MaxSlots = GetEffectiveMaxConcurrentAttackers(Territory);
	if (MaxSlots <= 0) return false;
	if (Slots.GrantedControllers.Num() >= MaxSlots)
	{
		return false;
	}

	Slots.GrantedControllers.Add(Controller);

	// Bind to the controller's death so slots are released even if the BT never
	// reaches BTTask_ReleaseTerritoryPermission (e.g. NPC killed mid-assault).
	BindControllerDeath(Controller);

	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugCombat())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Combat] Assault slot granted in %s (%d/%d)"),
			*Territory->GetTerritoryTag().ToString(),
			Slots.GrantedControllers.Num(), MaxSlots);
	}

	return true;
}

void UTerritoryCombatDirector::ReleaseAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;

	FPerTerritorySlots* Slots = SlotMap.Find(Territory);
	if (!Slots) return;

	Slots->GrantedControllers.RemoveAll(
		[Controller](const TWeakObjectPtr<ANarrativeNPCController>& Ptr)
		{
			return Ptr.Get() == Controller;
		});

	if (!ControllerHasAnySlot(Controller))
	{
		UnbindControllerDeath(Controller);
	}
}

void UTerritoryCombatDirector::ReleaseAllSlots(ANarrativeNPCController* Controller)
{
	if (!Controller || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;

	// Unbind delegate before processing all territories
	UnbindControllerDeath(Controller);

	for (auto& Pair : SlotMap)
	{
		Pair.Value.GrantedControllers.RemoveAll(
			[Controller](const TWeakObjectPtr<ANarrativeNPCController>& Ptr)
			{
				return Ptr.Get() == Controller;
			});
	}
}

bool UTerritoryCombatDirector::HasAssaultSlot(const ATerritoryVolume* Territory, const ANarrativeNPCController* Controller) const
{
	if (!Territory || !Controller) return false;

	const FPerTerritorySlots* Slots = SlotMap.Find(Territory);
	if (!Slots) return false;

	for (const TWeakObjectPtr<ANarrativeNPCController>& Existing : Slots->GrantedControllers)
	{
		if (Existing.Get() == Controller) return true;
	}
	return false;
}

int32 UTerritoryCombatDirector::GetGrantedSlots(const ATerritoryVolume* Territory) const
{
	if (!Territory) return 0;
	const FPerTerritorySlots* Slots = SlotMap.Find(Territory);
	if (!Slots) return 0;

	// Count only valid (alive) controllers — dead weak pointers should not consume budget
	int32 Count = 0;
	for (const TWeakObjectPtr<ANarrativeNPCController>& Ptr : Slots->GrantedControllers)
	{
		if (Ptr.IsValid()) ++Count;
	}
	return Count;
}

int32 UTerritoryCombatDirector::GetAvailableSlots(const ATerritoryVolume* Territory) const
{
	if (!Territory) return 0;
	const int32 MaxSlots = GetEffectiveMaxConcurrentAttackers(Territory);
	int32 Granted = GetGrantedSlots(Territory);
	return FMath::Max(0, MaxSlots - Granted);
}

int32 UTerritoryCombatDirector::GetEffectiveMaxConcurrentAttackers(
	const ATerritoryVolume* Territory) const
{
	if (!Territory) return 0;
	int32 EffectiveLimit = FMath::Max(0, Territory->GetMaxConcurrentAttackers());
	const UTerritoryCounterAttackProfile* Profile = Territory->GetCounterAttackProfile();
	if (!Profile || !Profile->bCapConcurrentAttackersToNarrativeDifficulty)
	{
		return EffectiveLimit;
	}

	const UNarrativeCombatDeveloperSettings* CombatSettings =
		GetDefault<UNarrativeCombatDeveloperSettings>();
	if (!CombatSettings) return EffectiveLimit;
	ENarrativeGameplayDifficulty Difficulty = ENarrativeGameplayDifficulty::Easy;
	if (UNarrativeGameUserSettings* UserSettings = Cast<UNarrativeGameUserSettings>(
		UGameUserSettings::GetGameUserSettings()))
	{
		Difficulty = UserSettings->GetGameplayDifficulty();
	}
	const int32 NarrativeLimit =
		CombatSettings->GetAttackTokensForDifficulty(Difficulty);
	return NarrativeLimit == TNumericLimits<int32>::Max()
		? EffectiveLimit : FMath::Min(EffectiveLimit, FMath::Max(0, NarrativeLimit));
}

bool UTerritoryCombatDirector::IsEligibleAssaultController(
	const ATerritoryVolume* Territory, const ANarrativeNPCController* Controller) const
{
	if (!Territory || !Controller) return false;
	const APawn* Pawn = Controller->GetPawn();
	const UTerritoryAssaultParticipantComponent* Participant = Pawn
		? Pawn->FindComponentByClass<UTerritoryAssaultParticipantComponent>() : nullptr;
	return Participant && Participant->IsConfigured()
		&& Participant->MatchesTargetTerritory(Territory);
}

bool UTerritoryCombatDirector::RequiresStrategicAssaultSlot(const APawn* Pawn)
{
	return Pawn && Pawn->FindComponentByClass<UTerritoryAssaultParticipantComponent>() != nullptr;
}

void UTerritoryCombatDirector::CleanupInvalidControllers(FPerTerritorySlots& Slots)
{
	Slots.GrantedControllers.RemoveAll([](const TWeakObjectPtr<ANarrativeNPCController>& Ptr) { return !Ptr.IsValid(); });
}

void UTerritoryCombatDirector::CleanupStaleTerritoryKeys()
{
	TArray<TWeakObjectPtr<ATerritoryVolume>> StaleKeys;
	for (const auto& Pair : SlotMap)
	{
		if (!Pair.Key.IsValid())
		{
			StaleKeys.Add(Pair.Key);
		}
	}
	for (const TWeakObjectPtr<ATerritoryVolume>& Key : StaleKeys)
	{
		SlotMap.Remove(Key);
	}

	// A streamed-out Territory invalidates its weak slot key. Release death
	// delegates for controllers that no longer hold a slot anywhere.
	const TArray<TWeakObjectPtr<ANarrativeNPCController>> Controllers = BoundControllers.Array();
	for (const TWeakObjectPtr<ANarrativeNPCController>& Controller : Controllers)
	{
		if (!Controller.IsValid())
		{
			BoundControllerASCs.Remove(Controller);
			BoundControllers.Remove(Controller);
		}
		else if (!ControllerHasAnySlot(Controller.Get()))
		{
			UnbindControllerDeath(Controller.Get());
		}
	}
}

void UTerritoryCombatDirector::BindControllerDeath(ANarrativeNPCController* Controller)
{
	if (!Controller || BoundControllers.Contains(Controller)) return;

	if (UNarrativeAbilitySystemComponent* ASC = ResolveControllerASC(Controller))
	{
		ASC->OnDeathStateChanged.AddUniqueDynamic(
			this, &UTerritoryCombatDirector::OnAssaultControllerDied);
		BoundControllers.Add(Controller);
		BoundControllerASCs.Add(Controller, ASC);
	}
}

void UTerritoryCombatDirector::UnbindControllerDeath(ANarrativeNPCController* Controller)
{
	if (!Controller || !BoundControllers.Contains(Controller)) return;

	if (const TWeakObjectPtr<UNarrativeAbilitySystemComponent>* FoundASC = BoundControllerASCs.Find(Controller))
	{
		if (UNarrativeAbilitySystemComponent* ASC = FoundASC->Get())
		{
			ASC->OnDeathStateChanged.RemoveDynamic(
				this, &UTerritoryCombatDirector::OnAssaultControllerDied);
		}
		else
		{
			UE_LOG(LogTerritory, Warning, TEXT("UnbindControllerDeath: ASC already destroyed for %s, delegate may be dangling"),
				*GetNameSafe(Controller));
		}
	}
	BoundControllerASCs.Remove(Controller);
	BoundControllers.Remove(Controller);
}

void UTerritoryCombatDirector::OnAssaultControllerDied(AActor* KilledActor,
	UNarrativeAbilitySystemComponent* KilledASC, const bool bIsDead)
{
	if (!bIsDead) return;
	if (!KilledActor) return;

	// Find the controller that owns this ASC — it could be the pawn's controller
	APawn* Pawn = Cast<APawn>(KilledActor);
	ANarrativeNPCController* DeadController = Pawn ? Cast<ANarrativeNPCController>(Pawn->GetController()) : nullptr;
	if (!DeadController)
	{
		DeadController = Cast<ANarrativeNPCController>(KilledActor);
	}
	if (!DeadController && KilledASC)
	{
		for (const auto& Pair : BoundControllerASCs)
		{
			if (Pair.Value.Get() == KilledASC)
			{
				DeadController = Pair.Key.Get();
				break;
			}
		}
	}
	if (!DeadController) return;

	// Release all assault slots held by the dead controller
	ReleaseAllSlots(DeadController);
	BoundControllers.Remove(DeadController);

	if (const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
		Settings && Settings->ShouldDebugCombat())
	{
		UE_LOG(LogTerritory, Log, TEXT("[Combat] released assault slots for dead controller %s"),
			*DeadController->GetName());
	}
}

bool UTerritoryCombatDirector::ControllerHasAnySlot(const ANarrativeNPCController* Controller) const
{
	if (!Controller) return false;
	for (const auto& Pair : SlotMap)
	{
		for (const TWeakObjectPtr<ANarrativeNPCController>& Granted : Pair.Value.GrantedControllers)
		{
			if (Granted.Get() == Controller)
			{
				return true;
			}
		}
	}
	return false;
}

UNarrativeAbilitySystemComponent* UTerritoryCombatDirector::ResolveControllerASC(ANarrativeNPCController* Controller) const
{
	return FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Controller);
}
