#include "Debug/TerritoryDebugger.h"
#include "Debug/TerritoryGameplayDebuggerCategory.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace TerritoryDebugPrivate
{
	FString EnumName(const UEnum* Enum, int64 Value)
	{
		return Enum ? Enum->GetDisplayNameTextByValue(Value).ToString() : TEXT("Unknown");
	}

	FString RuntimeSource(const UWorld* World, const ATerritoryVolume* Territory)
	{
		if (!World || !Territory) return TEXT("Unknown");
		if (World->GetNetMode() == NM_Client) return TEXT("Server replication");
		return Territory->WasRestoredFromCampaignSave()
			? TEXT("Campaign save") : TEXT("Definition seed (new campaign)");
	}

	FString BuildHierarchyPath(const UWorld* World, const ATerritoryVolume* Territory)
	{
		if (!World || !Territory) return TEXT("None");
		const UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>();
		if (!Registry) return Territory->GetTerritoryTag().ToString();

		TArray<FString> Segments;
		TSet<FGameplayTag> Visited;
		const ATerritoryVolume* Current = Territory;
		for (int32 Depth = 0; Current && Depth < 16; ++Depth)
		{
			const FGameplayTag Tag = Current->GetTerritoryTag();
			if (!Tag.IsValid() || Visited.Contains(Tag))
			{
				Segments.Insert(TEXT("[invalid or cyclic parent]"), 0);
				break;
			}
			Visited.Add(Tag);
			Segments.Insert(FString::Printf(TEXT("%s(%s)"), *Tag.ToString(),
				Current->IsLocked() ? TEXT("Locked") : TEXT("Unlocked")), 0);
			const FGameplayTag ParentTag = Current->GetParentTerritoryTag();
			Current = ParentTag.IsValid()
				? Registry->GetTerritoryByTag(ParentTag) : nullptr;
			if (ParentTag.IsValid() && !Current)
			{
				Segments.Insert(FString::Printf(TEXT("[missing %s]"), *ParentTag.ToString()), 0);
				break;
			}
		}
		return FString::Join(Segments, TEXT(" -> "));
	}

	FString BuildTerritorySummary(UWorld* World, ATerritoryVolume* Territory)
	{
		if (!World || !Territory) return TEXT("Territory: unavailable");

		const ETerritoryAvailability RuntimeAvailability =
			Territory->GetTerritoryAvailability();
		const ETerritoryState RuntimeState = Territory->GetTerritoryState();
		const ETerritoryAvailability InitialAvailability =
			Territory->ResolveInitialTerritoryAvailability();
		const ETerritoryState InitialState = Territory->ResolveInitialTerritoryState();
		const FString RuntimeAvailabilityName = EnumName(
			StaticEnum<ETerritoryAvailability>(), static_cast<int64>(RuntimeAvailability));
		const FString RuntimeStateName = EnumName(
			StaticEnum<ETerritoryState>(), static_cast<int64>(RuntimeState));
		const FString InitialAvailabilityName = EnumName(
			StaticEnum<ETerritoryAvailability>(), static_cast<int64>(InitialAvailability));
		const FString InitialStateName = EnumName(
			StaticEnum<ETerritoryState>(), static_cast<int64>(InitialState));
		const FString DisplayStatus =
			UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
				RuntimeAvailability, RuntimeState).ToString();
		const UTerritoryDefinition* Definition = Territory->GetTerritoryDefinition();

		FString Summary = FString::Printf(
			TEXT("Territory: %s\nName: %s\nActor: %s\nDefinition: %s\n")
			TEXT("Runtime source: %s\n")
			TEXT("Runtime: Availability=%s | Political State=%s | UI Status=%s\n")
			TEXT("New-campaign seed: Availability=%s | Political State=%s\n")
			TEXT("Effective hierarchy availability: %s\nHierarchy: %s\n")
			TEXT("Owner: %s | Contesting: %s | Progress: %.3f\n")
			TEXT("Guards: %d/%d desired | Capacity: %d | Registered defenders: %d"),
			*Territory->GetTerritoryTag().ToString(),
			*Territory->GetTerritoryDisplayName().ToString(),
			*Territory->GetPathName(),
			Definition ? *Definition->GetPathName() : TEXT("None"),
			*RuntimeSource(World, Territory),
			*RuntimeAvailabilityName, *RuntimeStateName, *DisplayStatus,
			*InitialAvailabilityName, *InitialStateName,
			Territory->IsAvailableForGameplay() ? TEXT("Available") : TEXT("Blocked"),
			*BuildHierarchyPath(World, Territory),
			*Territory->GetOwningFaction().ToString(),
			*Territory->GetOwnershipData().ContestingFaction.ToString(),
			Territory->GetControlProgress(), Territory->GetDefenderCount(),
			Territory->GetDesiredGuardCount(), Territory->GetMaxGuardCount(),
			Territory->GetRegisteredDefenders().Num());

		if (!Territory->GetLockReason().IsEmpty())
		{
			Summary += FString::Printf(TEXT("\nLock reason: %s"),
				*Territory->GetLockReason().ToString());
		}
		if (RuntimeAvailability != InitialAvailability || RuntimeState != InitialState)
		{
			Summary += Territory->WasRestoredFromCampaignSave()
				? TEXT("\nNOTE: Runtime differs from the Definition because the campaign save is authoritative. Initial values only seed a new campaign.")
				: TEXT("\nNOTE: Runtime differs from the initial seed because gameplay changed this Territory after initialization.");
		}
		if (RuntimeAvailability == ETerritoryAvailability::Locked
			&& RuntimeState == ETerritoryState::Contested)
		{
			Summary += TEXT("\nNOTE: Political State may remain Contested while availability is Locked, but player UI and capture rules must present Locked.");
		}

		if (UTerritoryCounterAttackSubsystem* Counterattacks =
			World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
		{
			const TArray<FTerritoryAssaultRecord> Assaults =
				Counterattacks->GetAssaultsForTerritoryActor(Territory);
			Summary += FString::Printf(TEXT("\nAssault records: %d"), Assaults.Num());
			for (const FTerritoryAssaultRecord& Assault : Assaults)
			{
				Summary += FString::Printf(TEXT("\n%s | %s"),
					*Assault.AssaultID.ToString(),
					*Counterattacks->GetAssaultDebugString(Assault.AssaultID));
			}
		}
		return Summary;
	}
}

FText UTerritoryDebugger::BuildTerritoryDebugSummary(
	const UObject* WorldContextObject, const AActor* DebugActor)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return FText::FromString(TEXT("Territory: no valid world"));

	ATerritoryVolume* Territory = const_cast<ATerritoryVolume*>(
		Cast<ATerritoryVolume>(DebugActor));
	if (!Territory && DebugActor)
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Territory = Registry->GetTerritoryAtLocation(DebugActor->GetActorLocation());
		}
	}
	if (!Territory) return FText::FromString(TEXT("Territory: no Territory at debug actor"));

	return FText::FromString(TerritoryDebugPrivate::BuildTerritorySummary(
		World, Territory));
}

FText UTerritoryDebugger::BuildTerritoryDebugSummaryByTag(
	const UObject* WorldContextObject, FGameplayTag TerritoryTag)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return FText::FromString(TEXT("Territory: no valid world"));
	const UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	ATerritoryVolume* Territory = Registry
		? Registry->GetTerritoryByTag(TerritoryTag) : nullptr;
	if (!Territory)
	{
		return FText::FromString(FString::Printf(
			TEXT("Territory: tag '%s' is not loaded or registered"),
			*TerritoryTag.ToString()));
	}
	return FText::FromString(TerritoryDebugPrivate::BuildTerritorySummary(
		World, Territory));
}

FText UTerritoryDebugger::BuildTerritorySystemDebugReport(
	const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return FText::FromString(TEXT("Territory: no valid world"));
	const UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry) return FText::FromString(TEXT("Territory: registry unavailable"));

	TArray<ATerritoryVolume*> Territories = Registry->GetAllTerritories();
	Territories.Sort([](const ATerritoryVolume& A, const ATerritoryVolume& B)
	{
		return A.GetTerritoryTag().ToString() < B.GetTerritoryTag().ToString();
	});

	FString Report = BuildDebugSettingsSummary().ToString();
	Report += FString::Printf(TEXT("\n\n=== Loaded Territory Runtime (%d) ==="),
		Territories.Num());
	for (const ATerritoryVolume* Territory : Territories)
	{
		if (!Territory) continue;
		const ETerritoryAvailability Availability =
			Territory->GetTerritoryAvailability();
		const ETerritoryState State = Territory->GetTerritoryState();
		const bool bDiffersFromSeed =
			Availability != Territory->ResolveInitialTerritoryAvailability()
			|| State != Territory->ResolveInitialTerritoryState();
		Report += FString::Printf(
			TEXT("\n%s | %s/%s | HUD=%s | Source=%s%s"),
			*Territory->GetTerritoryTag().ToString(),
			*TerritoryDebugPrivate::EnumName(StaticEnum<ETerritoryAvailability>(),
				static_cast<int64>(Availability)),
			*TerritoryDebugPrivate::EnumName(StaticEnum<ETerritoryState>(),
				static_cast<int64>(State)),
			*UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
				Availability, State).ToString(),
			*TerritoryDebugPrivate::RuntimeSource(World, Territory),
			bDiffersFromSeed ? TEXT(" | differs from initial seed") : TEXT(""));
	}
	return FText::FromString(Report);
}

FText UTerritoryDebugger::BuildDebugSettingsSummary()
{
	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	if (!Settings) return FText::FromString(TEXT("Territory debug settings unavailable"));

	TArray<FString> Enabled;
	auto Add = [&Enabled](bool bEnabled, const TCHAR* Name)
	{
		if (bEnabled) Enabled.Add(Name);
	};
	Add(Settings->ShouldDebugAvailability(), TEXT("Availability + hierarchy"));
	Add(Settings->ShouldDebugRegistry(), TEXT("Registry"));
	Add(Settings->ShouldDebugCapture(), TEXT("Capture progression"));
	Add(Settings->ShouldDebugCaptureAttempts(), TEXT("Capture attempts"));
	Add(Settings->ShouldDebugOwnership(), TEXT("Ownership"));
	Add(Settings->ShouldDebugStateTransitions(), TEXT("State transitions"));
	Add(Settings->ShouldDebugEconomy(), TEXT("Economy"));
	Add(Settings->ShouldDebugTransactions(), TEXT("Transactions"));
	Add(Settings->ShouldDebugProduction(), TEXT("Production"));
	Add(Settings->ShouldDebugGuards(), TEXT("Guard spawning"));
	Add(Settings->ShouldDebugGuardDeaths(), TEXT("Guard deaths"));
	Add(Settings->ShouldDebugDiplomacy(), TEXT("Diplomacy"));
	Add(Settings->ShouldDebugAttitudes(), TEXT("Faction attitudes"));
	Add(Settings->ShouldDebugCounterAttacks(), TEXT("Counterattacks"));
	Add(Settings->ShouldDebugStealth(), TEXT("Stealth"));
	Add(Settings->ShouldDebugSaveLoad(), TEXT("Save/load"));
	Add(Settings->ShouldDebugWorldState(), TEXT("WorldState"));
	Add(Settings->ShouldDebugSpatial(), TEXT("Spatial index"));
	Add(Settings->ShouldDebugMarkers(), TEXT("Map markers"));
	Add(Settings->ShouldDebugUI(), TEXT("UI"));
	Add(Settings->ShouldDebugInteraction(), TEXT("Interaction"));
	Add(Settings->ShouldDebugTales(), TEXT("Narrative/Tales"));
	Add(Settings->ShouldDebugBT(), TEXT("Behavior trees"));
	Add(Settings->ShouldDebugCombat(), TEXT("Combat director"));

	TArray<FString> Visuals;
	auto AddVisual = [&Visuals](bool bEnabled, const TCHAR* Name)
	{
		if (bEnabled) Visuals.Add(Name);
	};
	AddVisual(Settings->IsDebugEnabled() && Settings->bDrawTerritoryBounds,
		TEXT("Territory bounds"));
	AddVisual(Settings->IsDebugEnabled() && Settings->bDrawOwnershipOverlay,
		TEXT("Ownership overlay"));
	AddVisual(Settings->IsDebugEnabled() && Settings->bDrawCaptureProgress,
		TEXT("Capture progress"));
	AddVisual(Settings->IsDebugEnabled() && Settings->bDrawGuardSpawnPoints,
		TEXT("Guard posts + patrols"));
	AddVisual(Settings->IsDebugEnabled() && Settings->bDrawSpatialGrid,
		TEXT("Spatial grid"));

	return FText::FromString(FString::Printf(
		TEXT("=== Territory Debug Settings ===\nMaster gate: %s | Verbosity: %d\nEnabled log categories: %s\nEnabled visual overlays: %s\n")
		TEXT("Always on (outside the debug gate): validation failures, authority/security rejections, invalid assets, duplicate identity, save corruption, and other Error/Warning diagnostics. These stay visible because hiding them would make broken content silent.\n")
		TEXT("Explicitly requested tools also bypass the gate: Print Territory Debug, the Territory Gameplay Debugger category, Build Territory Debug Report functions, and a Debug Widget enabled by Set Debug Enabled."),
		Settings->IsDebugEnabled() ? TEXT("ON") : TEXT("OFF"),
		Settings->DebugVerbosityLevel,
		Enabled.IsEmpty() ? TEXT("None") : *FString::Join(Enabled, TEXT(", ")),
		Visuals.IsEmpty() ? TEXT("None") : *FString::Join(Visuals, TEXT(", "))));
}

#if WITH_GAMEPLAY_DEBUGGER
FGameplayDebuggerCategory_Territory::FGameplayDebuggerCategory_Territory()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

void FGameplayDebuggerCategory_Territory::CollectData(
	APlayerController* OwnerPC, AActor* DebugActor)
{
	DataPack.Summary = UTerritoryDebugger::BuildTerritoryDebugSummary(
		OwnerPC ? static_cast<const UObject*>(OwnerPC) : DebugActor, DebugActor).ToString();
}

void FGameplayDebuggerCategory_Territory::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	(void)OwnerPC;
	TArray<FString> Lines;
	DataPack.Summary.ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		CanvasContext.Print(Line);
	}
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Territory::MakeInstance()
{
	return MakeShared<FGameplayDebuggerCategory_Territory>();
}
#endif
