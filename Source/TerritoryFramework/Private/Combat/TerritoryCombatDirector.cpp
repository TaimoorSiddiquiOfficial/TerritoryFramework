#include "Combat/TerritoryCombatDirector.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"

void UTerritoryCombatDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTerritory, Log, TEXT("TerritoryCombatDirector initialized"));
}

void UTerritoryCombatDirector::Deinitialize()
{
	PermissionMap.Empty();
	Super::Deinitialize();
}

bool UTerritoryCombatDirector::RequestAttackPermission(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller) return false;

	if (Territory->GetTerritoryState() == ETerritoryState::Locked) return false;

	FPerTerritoryPermissions& Permissions = PermissionMap.FindOrAdd(Territory);
	CleanupInvalidControllers(Permissions);

	int32 MaxSlots = Territory->GetMaxConcurrentAttackers();
	if (Permissions.GrantedControllers.Num() >= MaxSlots)
	{
		return false;
	}

	// Check if this controller already has permission
	for (const TWeakObjectPtr<ANarrativeNPCController>& Existing : Permissions.GrantedControllers)
	{
		if (Existing.Get() == Controller) return true;
	}

	Permissions.GrantedControllers.Add(Controller);

	// NOTE: Controller cleanup handled via TWeakObjectPtr in CleanupInvalidControllers()
	// called on each RequestAttackPermission. No need for OnDestroyed binding.

	UE_LOG(LogTerritory, Verbose, TEXT("Attack permission granted in %s (%d/%d)"),
		*Territory->GetTerritoryTag().ToString(),
		Permissions.GrantedControllers.Num(), MaxSlots);

	return true;
}

void UTerritoryCombatDirector::ReleaseAttackPermission(ATerritoryVolume* Territory, ANarrativeNPCController* Controller)
{
	if (!Territory || !Controller) return;

	FPerTerritoryPermissions* Permissions = PermissionMap.Find(Territory);
	if (!Permissions) return;

	Permissions->GrantedControllers.RemoveAll(
		[Controller](const TWeakObjectPtr<ANarrativeNPCController>& Ptr)
		{
			return Ptr.Get() == Controller;
		});
}

void UTerritoryCombatDirector::ReleaseAllPermissions(ANarrativeNPCController* Controller)
{
	if (!Controller) return;

	for (auto& Pair : PermissionMap)
	{
		Pair.Value.GrantedControllers.RemoveAll(
			[Controller](const TWeakObjectPtr<ANarrativeNPCController>& Ptr)
			{
				return Ptr.Get() == Controller;
			});
	}
}

bool UTerritoryCombatDirector::HasAttackPermission(const ATerritoryVolume* Territory, const ANarrativeNPCController* Controller) const
{
	if (!Territory || !Controller) return false;

	const FPerTerritoryPermissions* Permissions = PermissionMap.Find(Territory);
	if (!Permissions) return false;

	for (const TWeakObjectPtr<ANarrativeNPCController>& Existing : Permissions->GrantedControllers)
	{
		if (Existing.Get() == Controller) return true;
	}
	return false;
}

int32 UTerritoryCombatDirector::GetGrantedPermissions(const ATerritoryVolume* Territory) const
{
	if (!Territory) return 0;
	const FPerTerritoryPermissions* Permissions = PermissionMap.Find(Territory);
	return Permissions ? Permissions->GrantedControllers.Num() : 0;
}

int32 UTerritoryCombatDirector::GetAvailableSlots(const ATerritoryVolume* Territory) const
{
	if (!Territory) return 0;
	int32 MaxSlots = Territory->GetMaxConcurrentAttackers();
	int32 Granted = GetGrantedPermissions(Territory);
	return FMath::Max(0, MaxSlots - Granted);
}

void UTerritoryCombatDirector::CleanupInvalidControllers(FPerTerritoryPermissions& Permissions)
{
	Permissions.GrantedControllers.RemoveAll([](const TWeakObjectPtr<ANarrativeNPCController>& Ptr) { return !Ptr.IsValid(); });
}
