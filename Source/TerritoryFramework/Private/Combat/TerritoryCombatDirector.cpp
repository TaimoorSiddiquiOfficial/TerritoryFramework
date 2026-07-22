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
	Super::Deinitialize();
}

bool UTerritoryCombatDirector::RequestAssaultSlot(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller) return false;

	if (Territory->GetTerritoryState() == ETerritoryState::Locked) return false;

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
	return Slots ? Slots->GrantedControllers.Num() : 0;
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
