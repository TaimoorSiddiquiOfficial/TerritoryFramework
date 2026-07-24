#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "AI/NarrativeNPCController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"

void UTerritoryCombatDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTerritory, Log, TEXT("TerritoryCombatDirector initialized (assault budget manager)"));
}

void UTerritoryCombatDirector::Deinitialize()
{
	SlotMap.Empty();
	BoundControllers.Empty();
	Super::Deinitialize();
}

bool UTerritoryCombatDirector::RequestAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller) return false;

	if (Territory->GetTerritoryState() == ETerritoryState::Locked) return false;

	// Periodically clean stale territory keys (destroyed territories) to prevent
	// SlotMap from accumulating dead entries over time.
	CleanupStaleTerritoryKeys();

	FPerTerritorySlots& Slots = SlotMap.FindOrAdd(Territory);
	CleanupInvalidControllers(Slots);

	int32 MaxSlots = Territory->GetMaxConcurrentAttackers();
	if (Slots.GrantedControllers.Num() >= MaxSlots)
	{
		return false;
	}

	// Check if this controller already has a slot
	for (const TWeakObjectPtr<ANarrativeNPCController>& Existing : Slots.GrantedControllers)
	{
		if (Existing.Get() == Controller) return true;
	}

	Slots.GrantedControllers.Add(Controller);

	// Bind to the controller's death so slots are released even if the BT never
	// reaches BTTask_ReleaseTerritoryPermission (e.g. NPC killed mid-assault).
	BindControllerDeath(Controller);

	UE_LOG(LogTerritory, Verbose, TEXT("Assault slot granted in %s (%d/%d)"),
		*Territory->GetTerritoryTag().ToString(),
		Slots.GrantedControllers.Num(), MaxSlots);

	return true;
}

void UTerritoryCombatDirector::ReleaseAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller) return;

	FPerTerritorySlots* Slots = SlotMap.Find(Territory);
	if (!Slots) return;

	Slots->GrantedControllers.RemoveAll(
		[Controller](const TWeakObjectPtr<ANarrativeNPCController>& Ptr)
		{
			return Ptr.Get() == Controller;
		});

	BoundControllers.Remove(Controller);
}

void UTerritoryCombatDirector::ReleaseAllSlots(ANarrativeNPCController* Controller)
{
	if (!Controller) return;

	for (auto& Pair : SlotMap)
	{
		Pair.Value.GrantedControllers.RemoveAll(
			[Controller](const TWeakObjectPtr<ANarrativeNPCController>& Ptr)
			{
				return Ptr.Get() == Controller;
			});
	}

	BoundControllers.Remove(Controller);
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
	int32 MaxSlots = Territory->GetMaxConcurrentAttackers();
	int32 Granted = GetGrantedSlots(Territory);
	return FMath::Max(0, MaxSlots - Granted);
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
}

void UTerritoryCombatDirector::BindControllerDeath(ANarrativeNPCController* Controller)
{
	if (!Controller || BoundControllers.Contains(Controller)) return;

	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Controller))
	{
		if (UNarrativeAbilitySystemComponent* ASC =
			Cast<UNarrativeAbilitySystemComponent>(ASCInterface->GetAbilitySystemComponent()))
		{
			ASC->OnDied.AddUniqueDynamic(this, &UTerritoryCombatDirector::OnAssaultControllerDied);
			BoundControllers.Add(Controller);
		}
	}
}

void UTerritoryCombatDirector::OnAssaultControllerDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC)
{
	if (!KilledActor) return;

	// Find the controller that owns this ASC — it could be the pawn's controller
	APawn* Pawn = Cast<APawn>(KilledActor);
	ANarrativeNPCController* DeadController = Pawn ? Cast<ANarrativeNPCController>(Pawn->GetController()) : nullptr;
	if (!DeadController)
	{
		// Try direct cast — the killed actor might be the controller itself
		DeadController = Cast<ANarrativeNPCController>(KilledActor);
	}
	if (!DeadController) return;

	// Release all assault slots held by the dead controller
	ReleaseAllSlots(DeadController);
	BoundControllers.Remove(DeadController);

	UE_LOG(LogTerritory, Verbose, TEXT("CombatDirector: released assault slots for dead controller %s"),
		*DeadController->GetName());
}
