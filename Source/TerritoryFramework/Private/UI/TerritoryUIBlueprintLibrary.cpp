#include "UI/TerritoryUIBlueprintLibrary.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryCommandTags.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Navigation/TerritoryMapMarker.h"
#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Items/NarrativeItem.h"
#include "NarrativeGameplayTags.h"

namespace
{
	AActor* ResolveViewerActor(APlayerController* Viewer)
	{
		return Viewer && Viewer->GetPawn() ? static_cast<AActor*>(Viewer->GetPawn()) : Viewer;
	}

	void GatherVisibleWaypointLeaves(
		APlayerController* Viewer, ATerritoryVolume* Territory,
		UTerritoryRegistrySubsystem* Registry, TSet<FGameplayTag>& Visited,
		TArray<ATerritoryVolume*>& OutLeaves)
	{
		if (!Territory || !Registry
			|| !UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(Viewer, Territory))
		{
			return;
		}

		const FGameplayTag TerritoryTag = Territory->GetTerritoryTag();
		if (!TerritoryTag.IsValid() || Visited.Contains(TerritoryTag))
		{
			return;
		}
		Visited.Add(TerritoryTag);

		if (Territory->IsA<ATerritoryProperty>())
		{
			OutLeaves.Add(Territory);
			return;
		}

		if (Territory->IsA<ATerritoryDistrict>() || Territory->IsA<ATerritoryCity>())
		{
			TArray<ATerritoryVolume*> Children = Registry->GetChildTerritories(TerritoryTag);
			Children.Sort([](const ATerritoryVolume& A, const ATerritoryVolume& B)
			{
				const FString ATag = A.GetTerritoryTag().ToString();
				const FString BTag = B.GetTerritoryTag().ToString();
				return ATag == BTag ? A.GetPathName() < B.GetPathName() : ATag < BTag;
			});
			for (ATerritoryVolume* Child : Children)
			{
				GatherVisibleWaypointLeaves(Viewer, Child, Registry, Visited, OutLeaves);
			}
			return;
		}

		// Backward-compatible support for projects using a standalone TerritoryVolume.
		OutLeaves.Add(Territory);
	}

	int32 GetWaypointPriority(const ATerritoryVolume* Territory,
		const FGameplayTag& ViewerFaction)
	{
		if (!Territory) return MAX_int32;
		if (Territory->GetTerritoryState() == ETerritoryState::Contested) return 0;
		if (!ViewerFaction.IsValid()
			|| Territory->GetOwningFaction() != ViewerFaction) return 1;
		return 2;
	}

	FTerritoryCommandCapabilityView BuildCommandCapabilityView(
		const UObject* WorldContextObject, const FGameplayTag& Faction,
		const FGameplayTag& Capability, const FText& DisplayName,
		const FText& Description)
	{
		FTerritoryCommandCapabilityView View;
		View.Capability = Capability;
		View.DisplayName = DisplayName;
		View.Description = Description;
		View.bConfigured = UTerritoryBlueprintLibrary::IsCommandCapabilityUsed(
			WorldContextObject, Capability);
		FText FailureReason;
		View.bGranted = UTerritoryBlueprintLibrary::CanFactionUseCommandCapability(
			WorldContextObject, Faction, Capability, FailureReason);
		for (const ATerritoryVolume* Source :
			UTerritoryBlueprintLibrary::GetFactionCommandCapabilitySources(
				WorldContextObject, Faction, Capability))
		{
			if (IsValid(Source))
			{
				View.ActiveSourceNames.Add(Source->GetTerritoryDisplayName());
			}
		}
		if (!View.bConfigured)
		{
			View.AvailabilityReason = NSLOCTEXT("TerritoryOperations", "LegacyCapabilityAvailable",
				"Available. No strategic source requirement is configured for this project.");
		}
		else if (View.bGranted)
		{
			View.AvailabilityReason = FText::Format(
				NSLOCTEXT("TerritoryOperations", "CapabilityGrantedBy",
					"Available while your faction holds: {0}"),
				FText::FromString(FString::JoinBy(View.ActiveSourceNames, TEXT(", "),
					[](const FText& Name) { return Name.ToString(); })));
		}
		else
		{
			View.AvailabilityReason = FailureReason;
		}
		return View;
	}

	const ATerritoryWorldState* FindTerritoryWorldState(const UWorld* World)
	{
		if (!World) return nullptr;
		for (TActorIterator<ATerritoryWorldState> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	int32 GetStoredResourceQuantity(const FTerritoryFactionResourceSnapshot& Snapshot,
		TSubclassOf<UNarrativeItem> ItemClass)
	{
		for (const FTerritoryResourceAmount& Resource : Snapshot.Resources)
		{
			if (Resource.ItemClass == ItemClass) return Resource.Quantity;
		}
		return 0;
	}

	FTerritoryResourceOperationsView& FindOrAddResourceView(
		TArray<FTerritoryResourceOperationsView>& Resources,
		TSubclassOf<UNarrativeItem> ItemClass,
		const FTerritoryFactionResourceSnapshot& Snapshot)
	{
		for (FTerritoryResourceOperationsView& Resource : Resources)
		{
			if (Resource.ItemClass == ItemClass) return Resource;
		}
		FTerritoryResourceOperationsView& Resource = Resources.AddDefaulted_GetRef();
		Resource.ItemClass = ItemClass;
		Resource.StoredQuantity = GetStoredResourceQuantity(Snapshot, ItemClass);
		if (const UNarrativeItem* CDO = ItemClass ? GetDefault<UNarrativeItem>(ItemClass) : nullptr)
		{
			Resource.DisplayName = CDO->DisplayName;
			Resource.Thumbnail = CDO->Thumbnail;
		}
		return Resource;
	}

	FTerritoryProductionSiteOperationsView BuildProductionSiteView(
		const FTerritoryProductionSiteRecord& Record,
		const FTerritoryFactionResourceSnapshot& Snapshot)
	{
		FTerritoryProductionSiteOperationsView View;
		View.TerritoryTag = Record.TerritoryTag;
		View.ParentTerritoryTag = Record.ParentTerritoryTag;
		View.DisplayName = Record.DisplayName;
		View.OwnerFaction = Record.OwnerFaction;
		View.ActiveRuleTag = Record.LastRuleTag;
		View.Status = Record.LastStatus;
		View.StatusReason = Record.StatusReason;
		View.LastEvaluatedCycle = Record.LastEvaluatedCycle;
		View.RuleStates = Record.RuleStates;
		View.bHasProductionProfile = !Record.ProductionProfile.IsNull();
		View.bProducing = Record.LastStatus == ETerritoryProductionStatus::Produced
			|| Record.LastStatus == ETerritoryProductionStatus::Ready
			|| Record.LastStatus == ETerritoryProductionStatus::AlreadyProcessed;
		View.bBlocked = Record.LastStatus == ETerritoryProductionStatus::MissingInput
			|| Record.LastStatus == ETerritoryProductionStatus::StorageUnavailable
			|| Record.LastStatus == ETerritoryProductionStatus::StorageFull
			|| Record.LastStatus == ETerritoryProductionStatus::InvalidProfile;

		const UTerritoryProductionProfile* Profile = Record.ProductionProfile.LoadSynchronous();
		if (Profile)
		{
			for (const FTerritoryProductionRule& Rule : Profile->Rules)
			{
				if (!Rule.bEnabled) continue;
				for (const FTerritoryResourceRate& Rate : Rule.Inputs)
				{
					int32 Quantity = 0;
					if (!UTerritoryProductionProfile::CalculateScaledQuantity(
						Rate, Record.UpgradeLevel, 1, Quantity)) continue;
					FTerritoryResourceOperationsView& Resource = FindOrAddResourceView(
						View.Resources, Rate.ItemClass, Snapshot);
					Resource.InputPerCycle += Quantity;
				}
				for (const FTerritoryResourceRate& Rate : Rule.Outputs)
				{
					int32 Quantity = 0;
					if (!UTerritoryProductionProfile::CalculateScaledQuantity(
						Rate, Record.UpgradeLevel, 1, Quantity)) continue;
					FTerritoryResourceOperationsView& Resource = FindOrAddResourceView(
						View.Resources, Rate.ItemClass, Snapshot);
					Resource.OutputPerCycle += Quantity;
				}
			}
		}
		for (FTerritoryResourceOperationsView& Resource : View.Resources)
		{
			Resource.NetPerCycle = Resource.OutputPerCycle - Resource.InputPerCycle;
			Resource.bSufficientForNextCycle = Resource.StoredQuantity >= Resource.InputPerCycle;
		}
		View.Resources.Sort([](const FTerritoryResourceOperationsView& A,
			const FTerritoryResourceOperationsView& B)
		{
			return A.DisplayName.ToString() < B.DisplayName.ToString();
		});
		return View;
	}

	void MergeResourceFlows(const TArray<FTerritoryProductionSiteOperationsView>& Sites,
		const FTerritoryFactionResourceSnapshot& Snapshot,
		TArray<FTerritoryResourceOperationsView>& OutResources)
	{
		for (const FTerritoryProductionSiteOperationsView& Site : Sites)
		{
			for (const FTerritoryResourceOperationsView& SiteResource : Site.Resources)
			{
				FTerritoryResourceOperationsView& Aggregate = FindOrAddResourceView(
					OutResources, SiteResource.ItemClass, Snapshot);
				Aggregate.InputPerCycle += SiteResource.InputPerCycle;
				Aggregate.OutputPerCycle += SiteResource.OutputPerCycle;
			}
		}
		for (FTerritoryResourceOperationsView& Resource : OutResources)
		{
			Resource.NetPerCycle = Resource.OutputPerCycle - Resource.InputPerCycle;
			Resource.bSufficientForNextCycle = Resource.StoredQuantity >= Resource.InputPerCycle;
		}
		OutResources.Sort([](const FTerritoryResourceOperationsView& A,
			const FTerritoryResourceOperationsView& B)
		{
			return A.DisplayName.ToString() < B.DisplayName.ToString();
		});
	}

	int32 GetAssaultPriority(ETerritoryAssaultState State)
	{
		switch (State)
		{
		case ETerritoryAssaultState::Active: return 500;
		case ETerritoryAssaultState::WaitingForPlayerProximity: return 400;
		case ETerritoryAssaultState::ScheduledWarning: return 300;
		case ETerritoryAssaultState::Evaluating: return 200;
		case ETerritoryAssaultState::Grace: return 100;
		default: return 0;
		}
	}

	FText GetCaptureResultText(ECaptureResult Result, const ATerritoryVolume* Territory)
	{
		switch (Result)
		{
		case ECaptureResult::Success:
			return NSLOCTEXT("TerritoryOperations", "CaptureAvailable", "Available for physical capture.");
		case ECaptureResult::AlreadyOwned:
			return NSLOCTEXT("TerritoryOperations", "AlreadyOwned", "Owned by your faction.");
		case ECaptureResult::Locked:
			return Territory && !Territory->GetLockReason().IsEmpty()
				? Territory->GetLockReason()
				: NSLOCTEXT("TerritoryOperations", "Locked", "Locked by territory policy or Narrative conditions.");
		case ECaptureResult::DefendersRemain:
			return NSLOCTEXT("TerritoryOperations", "DefendersRemain", "Defenders must be defeated before capture can begin.");
		case ECaptureResult::DiplomaticallyBlocked:
			return NSLOCTEXT("TerritoryOperations", "DiplomacyBlocked", "A treaty or friendly Narrative attitude blocks an assault.");
		case ECaptureResult::InvalidTerritory:
		default:
			return Territory && Territory->GetControlMode() == ETerritoryControlMode::AggregateOnly
				? NSLOCTEXT("TerritoryOperations", "AggregateOnly", "Control is derived from child territories.")
				: NSLOCTEXT("TerritoryOperations", "InvalidCaptureContext", "No valid faction capture context is available.");
		}
	}

	int32 GetDistrictOperationsPriority(const FTerritoryDistrictOperationsView& View)
	{
		int32 Value = static_cast<int32>(View.ThreatLevel) * 10000;
		Value += View.bUnderAttack ? 9000 : 0;
		Value += View.bAttackScheduled ? 8000 : 0;
		Value += View.bThreatPreviewAvailable
			? 5000 + FMath::RoundToInt(View.LaunchProbability * 1000.f) : 0;
		Value += View.bOwnedByViewer && View.bUnguarded ? 4000 : 0;
		Value += View.bFinancialRisk ? 3000 : 0;
		Value += View.bManageable ? 2000 : 0;
		Value += View.bAvailable ? 1000 : 0;
		Value -= View.bUnlocked ? 0 : 1000;
		return Value;
	}

	FText BuildThreatSummary(const FTerritoryDistrictOperationsView& View)
	{
		if (View.bUnderAttack)
		{
			return FText::Format(
				NSLOCTEXT("TerritoryOperations", "ActiveThreat", "UNDER ATTACK — {0} alive, {1} reserve"),
				FText::AsNumber(View.AliveAttackers),
				FText::AsNumber(View.PendingReserveAttackers));
		}
		if (View.bAttackScheduled)
		{
			return FText::Format(
				NSLOCTEXT("TerritoryOperations", "ScheduledThreat", "{0} — planned force {1}"),
				UTerritoryUIBlueprintLibrary::GetAssaultStateText(View.AssaultState),
				FText::AsNumber(View.PlannedAttackers));
		}
		if (View.bCaptureInProgress)
		{
			return NSLOCTEXT("TerritoryOperations", "CaptureThreat", "Territory control is being contested.");
		}
		if (View.bThreatPreviewAvailable)
		{
			return FText::Format(
				NSLOCTEXT("TerritoryOperations", "ProjectedThreat",
					"PROJECTED — {0} can attack {1}: launch {2}, estimated success {3}, defence {4}, power ratio {5}"),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.AttackingFaction),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ThreatTargetTerritory),
				FText::AsPercent(FMath::Clamp(View.LaunchProbability, 0.f, 1.f)),
				FText::AsPercent(FMath::Clamp(View.EstimatedSuccessProbability, 0.f, 1.f)),
				FText::AsNumber(View.DistrictDefencePower),
				FText::AsNumber(View.PowerRatio));
		}
		if (!View.ThreatEvaluationReason.IsEmpty())
		{
			return View.ThreatEvaluationReason;
		}
		return NSLOCTEXT("TerritoryOperations", "NoThreat", "No active threat.");
	}
}

UTerritoryActivatableWidget* UTerritoryUIBlueprintLibrary::OpenTerritoryMenu(
	APlayerController* PlayerController,
	TSubclassOf<UTerritoryActivatableWidget> WidgetClass,
	FGameplayTag LayerTag)
{
	if (!PlayerController || !PlayerController->IsLocalController() || !WidgetClass)
	{
		return nullptr;
	}

	// "Open Territory Menu" should work out of the box for community users.
	// An explicit tag still supports custom layers; an empty tag uses Narrative's
	// standard Menu layer and therefore participates in normal HUD suppression.
	if (!LayerTag.IsValid())
	{
		LayerTag = FNarrativeGameplayTags::Get().UI_Layer_Menu;
	}
	const ANarrativePlayerController* NarrativeController = Cast<ANarrativePlayerController>(PlayerController);
	UNarrativeGameplayHUD* HUD = NarrativeController
		? NarrativeController->GetNarrativeGameplayHUD()
		: nullptr;
	UCommonActivatableWidgetContainerBase* Layer = HUD ? HUD->GetLayerContainer(LayerTag) : nullptr;
	if (!Layer)
	{
		return nullptr;
	}
	if (UTerritoryActivatableWidget* Active = Cast<UTerritoryActivatableWidget>(Layer->GetActiveWidget()))
	{
		if (Active->GetClass() == WidgetClass)
		{
			return Active;
		}
	}
	return Layer->AddWidget<UTerritoryActivatableWidget>(WidgetClass);
}

ATerritoryVolume* UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
	APlayerController* PlayerController, ATerritoryVolume* Territory)
{
	UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
	UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!PlayerController || !Territory || !Registry
		|| !IsTerritoryVisibleToPlayer(PlayerController, Territory))
	{
		return nullptr;
	}

	TSet<FGameplayTag> Visited;
	TArray<ATerritoryVolume*> Candidates;
	GatherVisibleWaypointLeaves(
		PlayerController, Territory, Registry, Visited, Candidates);
	if (Candidates.IsEmpty()) return nullptr;

	const AActor* ViewerActor = ResolveViewerActor(PlayerController);
	const FVector ViewerLocation = ViewerActor
		? ViewerActor->GetActorLocation() : FVector::ZeroVector;
	const FGameplayTag ViewerFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
		PlayerController, const_cast<AActor*>(ViewerActor));
	Candidates.Sort([ViewerFaction, ViewerLocation](
		const ATerritoryVolume& A, const ATerritoryVolume& B)
	{
		const int32 APriority = GetWaypointPriority(&A, ViewerFaction);
		const int32 BPriority = GetWaypointPriority(&B, ViewerFaction);
		if (APriority != BPriority) return APriority < BPriority;
		const double ADistance = FVector::DistSquared(A.GetActorLocation(), ViewerLocation);
		const double BDistance = FVector::DistSquared(B.GetActorLocation(), ViewerLocation);
		if (!FMath::IsNearlyEqual(ADistance, BDistance)) return ADistance < BDistance;
		const FString ATag = A.GetTerritoryTag().ToString();
		const FString BTag = B.GetTerritoryTag().ToString();
		return ATag == BTag ? A.GetPathName() < B.GetPathName() : ATag < BTag;
	});
	return Candidates[0];
}

bool UTerritoryUIBlueprintLibrary::SetTerritoryWaypoint(
	APlayerController* PlayerController, ATerritoryVolume* Territory)
{
	if (!PlayerController || !PlayerController->IsLocalController()) return false;
	ATerritoryVolume* WaypointTarget = ResolveTerritoryWaypointTarget(
		PlayerController, Territory);
	if (!WaypointTarget) return false;
	UTerritoryNavigationMarkerComponent* Component = WaypointTarget->GetMapMarkerComponent();
	UTerritoryMapMarker* Marker = Component ? Component->GetTerritoryMapMarker() : nullptr;
	if (!Marker) return false;
	ClearTerritoryWaypoint(PlayerController);
	Marker->SetTracked(true);
	return Marker->IsTracked();
}

void UTerritoryUIBlueprintLibrary::ClearTerritoryWaypoint(
	APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController()) return;
	for (ATerritoryVolume* Territory :
		UTerritoryBlueprintLibrary::GetAllTerritories(PlayerController))
	{
		if (!Territory) continue;
		if (UTerritoryNavigationMarkerComponent* Component =
			Territory->GetMapMarkerComponent())
		{
			if (UTerritoryMapMarker* Marker = Component->GetTerritoryMapMarker())
			{
				Marker->SetTracked(false);
			}
		}
	}
}

ATerritoryVolume* UTerritoryUIBlueprintLibrary::GetTrackedTerritory(
	APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController()) return nullptr;
	for (ATerritoryVolume* Territory :
		UTerritoryBlueprintLibrary::GetAllTerritories(PlayerController))
	{
		if (!Territory) continue;
		const UTerritoryNavigationMarkerComponent* Component =
			Territory->GetMapMarkerComponent();
		const UTerritoryMapMarker* Marker = Component
			? Component->GetTerritoryMapMarker() : nullptr;
		if (Marker && Marker->IsTracked()) return Territory;
	}
	return nullptr;
}

bool UTerritoryUIBlueprintLibrary::IsTerritoryWaypointTracked(
	APlayerController* PlayerController, ATerritoryVolume* Territory)
{
	ATerritoryVolume* Tracked = GetTrackedTerritory(PlayerController);
	if (!Territory || !Tracked
		|| !IsTerritoryVisibleToPlayer(PlayerController, Territory))
	{
		return false;
	}
	if (Tracked == Territory) return true;

	UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Registry) return false;

	const FGameplayTag RequestedTag = Territory->GetTerritoryTag();
	TSet<FGameplayTag> Visited;
	FGameplayTag ParentTag = Tracked->GetParentTerritoryTag();
	while (ParentTag.IsValid() && !Visited.Contains(ParentTag))
	{
		if (ParentTag == RequestedTag) return true;
		Visited.Add(ParentTag);
		const ATerritoryVolume* Parent = Registry->GetTerritoryByTag(ParentTag);
		if (!Parent) return false;
		ParentTag = Parent->GetParentTerritoryTag();
	}
	return false;
}

ATerritoryDistrict* UTerritoryUIBlueprintLibrary::GetDistrictAtPlayerLocation(
	const UObject* WorldContextObject, APlayerController* PlayerController)
{
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Pawn || !Registry)
	{
		return nullptr;
	}

	ATerritoryVolume* Territory = Registry->GetTerritoryAtLocation(
		Pawn->GetActorLocation());
	if (ATerritoryDistrict* District = Cast<ATerritoryDistrict>(Territory))
	{
		return District;
	}
	if (const ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		return Property->GetOwningDistrict();
	}
	return nullptr;
}

bool UTerritoryUIBlueprintLibrary::IsTerritoryVisibleToPlayer(
	const UObject* WorldContextObject, ATerritoryVolume* Territory)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UTerritoryRegistrySubsystem* Registry = World
		? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
	if (!Territory || !Registry
		|| Registry->GetTerritoryByTag(Territory->GetTerritoryTag()) != Territory)
	{
		return false;
	}

	TSet<FGameplayTag> Visited;
	ATerritoryVolume* Current = Territory;
	while (Current)
	{
		if (Current->GetTerritoryState() == ETerritoryState::Locked)
		{
			return false;
		}

		const FGameplayTag CurrentTag = Current->GetTerritoryTag();
		if (!CurrentTag.IsValid() || Visited.Contains(CurrentTag))
		{
			return false;
		}
		Visited.Add(CurrentTag);

		const FGameplayTag ParentTag = Current->GetParentTerritoryTag();
		if (!ParentTag.IsValid())
		{
			return true;
		}

		// Fail closed when a World Partition parent is not loaded. This prevents a
		// child Place from revealing the existence of an unloaded story-locked City
		// or District and keeps the player list structurally coherent.
		Current = Registry->GetTerritoryByTag(ParentTag);
		if (!Current)
		{
			return false;
		}
	}
	return false;
}

bool UTerritoryUIBlueprintLibrary::BuildHierarchyOperationsView(
	const UObject* WorldContextObject, ATerritoryVolume* Territory,
	APlayerController* Viewer, FTerritoryHierarchyOperationsView& OutView)
{
	OutView = FTerritoryHierarchyOperationsView();
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || !Territory)
	{
		return false;
	}

	OutView.Territory = Territory;
	OutView.TerritoryTag = Territory->GetTerritoryTag();
	OutView.ParentTerritoryTag = Territory->GetParentTerritoryTag();
	OutView.DisplayName = Territory->GetTerritoryDisplayName();
	OutView.TerritoryState = Territory->GetTerritoryState();
	OutView.OwnerFaction = Territory->GetOwningFaction();
	OutView.ViewerFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
		WorldContextObject, ResolveViewerActor(Viewer));
	OutView.HierarchyLevel = Territory->IsA<ATerritoryCity>()
		? ETerritoryHierarchyLevel::City
		: Territory->IsA<ATerritoryDistrict>()
			? ETerritoryHierarchyLevel::District
			: ETerritoryHierarchyLevel::Place;
	if (const UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		OutView.bRegistered = Registry->GetTerritoryByTag(OutView.TerritoryTag) == Territory;
	}
	OutView.bVisibleToPlayer = IsTerritoryVisibleToPlayer(WorldContextObject, Territory);
	OutView.bOwnedByViewer = OutView.ViewerFaction.IsValid()
		&& OutView.OwnerFaction == OutView.ViewerFaction;
	if (const UTerritoryControlSubsystem* Control =
		World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		OutView.CaptureEligibility = Control->GetCaptureEligibility(
			Territory, OutView.ViewerFaction);
	}
	OutView.bAvailableForCapture = OutView.bVisibleToPlayer
		&& OutView.CaptureEligibility == ECaptureResult::Success;
	OutView.AvailabilityReason = GetCaptureResultText(
		OutView.CaptureEligibility, Territory);

	FTerritoryGarrisonOperationsView Garrison;
	if (BuildGarrisonOperationsView(WorldContextObject, Territory, Viewer, Garrison))
	{
		OutView.ActiveGuards = Garrison.ActiveGuards;
		OutView.DesiredGuards = Garrison.DesiredGuards;
		OutView.MaximumGuards = Garrison.MaximumGuards;
		OutView.PeriodicIncome = Garrison.PeriodicIncome;
		OutView.GuardUpkeep = Garrison.GuardUpkeep;
		OutView.NetIncome = Garrison.NetIncome;
	}
	if (const ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		OutView.bHasProductionProfile = Property->GetProductionProfile() != nullptr;
	}
	return OutView.bRegistered;
}

TArray<FTerritoryHierarchyOperationsView>
UTerritoryUIBlueprintLibrary::GetDistrictHierarchyOperationsViews(
	const UObject* WorldContextObject, ATerritoryDistrict* District,
	APlayerController* Viewer)
{
	TArray<FTerritoryHierarchyOperationsView> Result;
	if (!District)
	{
		return Result;
	}

	if (ATerritoryCity* City = District->GetOwningCity())
	{
		FTerritoryHierarchyOperationsView CityView;
		if (BuildHierarchyOperationsView(WorldContextObject, City, Viewer, CityView)
			&& CityView.bVisibleToPlayer)
		{
			Result.Add(MoveTemp(CityView));
		}
	}

	FTerritoryHierarchyOperationsView DistrictView;
	if (!BuildHierarchyOperationsView(WorldContextObject, District, Viewer, DistrictView)
		|| !DistrictView.bVisibleToPlayer)
	{
		return TArray<FTerritoryHierarchyOperationsView>();
	}
	Result.Add(MoveTemp(DistrictView));

	TArray<FTerritoryHierarchyOperationsView> Places;
	for (ATerritoryVolume* Property : District->GetProperties())
	{
		FTerritoryHierarchyOperationsView PlaceView;
		if (BuildHierarchyOperationsView(WorldContextObject, Property, Viewer, PlaceView)
			&& PlaceView.bVisibleToPlayer)
		{
			Places.Add(MoveTemp(PlaceView));
		}
	}
	Places.Sort([](const FTerritoryHierarchyOperationsView& A,
		const FTerritoryHierarchyOperationsView& B)
	{
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	Result.Append(MoveTemp(Places));
	return Result;
}

bool UTerritoryUIBlueprintLibrary::BuildGarrisonOperationsView(
	const UObject* WorldContextObject, ATerritoryVolume* Territory,
	APlayerController* Viewer, FTerritoryGarrisonOperationsView& OutView)
{
	OutView = FTerritoryGarrisonOperationsView();
	if (!WorldContextObject || !Territory || !WorldContextObject->GetWorld()) return false;

	UWorld* World = WorldContextObject->GetWorld();
	AActor* ViewerActor = ResolveViewerActor(Viewer);
	OutView.Territory = Territory;
	OutView.TerritoryTag = Territory->GetTerritoryTag();
	OutView.DisplayName = Territory->GetTerritoryDisplayName();
	OutView.bDistrictGarrison = Territory->IsA<ATerritoryDistrict>();
	const FGameplayTag ViewerFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
		WorldContextObject, ViewerActor);
	OutView.bOwnedByViewer = ViewerFaction.IsValid()
		&& Territory->GetOwningFaction() == ViewerFaction;
	const UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	OutView.bManageable = ViewerActor && OutView.bOwnedByViewer
		&& Territory->GetTerritoryState() == ETerritoryState::Claimed
		&& Registry && Registry->GetTerritoryByTag(OutView.TerritoryTag) == Territory;

	const FTerritoryGarrisonSnapshot Snapshot = Territory->GetGarrisonSnapshot();
	OutView.ActiveGuards = Snapshot.ActiveGuards;
	OutView.DesiredGuards = Territory->GetDesiredGuardCount();
	OutView.MaximumGuards = Territory->GetMaxGuardCount();
	OutView.ReserveGuards = Snapshot.ReserveGuards;
	OutView.PendingDeployments = Snapshot.PendingDeployments;
	OutView.RecruitmentCostPerGuard = Territory->GetGuardRecruitmentCost(1);
	OutView.UpkeepPerGuard = FMath::Max(0, Territory->GetGuardCost());
	if (const ATerritoryProperty* Property = Cast<ATerritoryProperty>(Territory))
	{
		OutView.PeriodicIncome = Property->GetEffectiveIncome();
	}
	OutView.GuardUpkeep = static_cast<int64>(OutView.UpkeepPerGuard)
		* FMath::Max(0, OutView.DesiredGuards);
	OutView.NetIncome = OutView.PeriodicIncome - OutView.GuardUpkeep;

	if (OutView.bManageable && OutView.DesiredGuards < OutView.MaximumGuards)
	{
		int32 Cost = 0;
		OutView.bCanIncreaseTarget = Territory->CanSetDesiredGuardCount(ViewerActor,
			OutView.DesiredGuards + 1, OutView.IncreaseFailureReason, Cost);
	}
	else if (OutView.DesiredGuards >= OutView.MaximumGuards)
	{
		OutView.IncreaseFailureReason = NSLOCTEXT("TerritoryOperations", "GarrisonAtCapacity",
			"Garrison target is at maximum capacity.");
	}
	if (OutView.bManageable && OutView.DesiredGuards > 0)
	{
		int32 Cost = 0;
		OutView.bCanDecreaseTarget = Territory->CanSetDesiredGuardCount(ViewerActor,
			OutView.DesiredGuards - 1, OutView.DecreaseFailureReason, Cost);
	}
	else if (OutView.DesiredGuards <= 0)
	{
		OutView.DecreaseFailureReason = NSLOCTEXT("TerritoryOperations", "GarrisonAlreadyEmpty",
			"Garrison staffing target is already zero.");
	}
	if (ViewerActor)
	{
		OutView.bCanSendReinforcements = Territory->CanSendReinforcements(
			ViewerActor, 1, OutView.ReinforcementFailureReason);
	}
	return true;
}

TArray<FTerritoryGarrisonOperationsView> UTerritoryUIBlueprintLibrary::GetDistrictGarrisonOperationsViews(
	const UObject* WorldContextObject, ATerritoryDistrict* District, APlayerController* Viewer)
{
	TArray<FTerritoryGarrisonOperationsView> Result;
	if (!District) return Result;
	FTerritoryGarrisonOperationsView DistrictView;
	if (BuildGarrisonOperationsView(WorldContextObject, District, Viewer, DistrictView))
	{
		Result.Add(MoveTemp(DistrictView));
	}
	for (ATerritoryVolume* Property : District->GetProperties())
	{
		FTerritoryGarrisonOperationsView PropertyView;
		if (BuildGarrisonOperationsView(WorldContextObject, Property, Viewer, PropertyView))
		{
			Result.Add(MoveTemp(PropertyView));
		}
	}
	Result.Sort([](const FTerritoryGarrisonOperationsView& A,
		const FTerritoryGarrisonOperationsView& B)
	{
		if (A.bDistrictGarrison != B.bDistrictGarrison) return A.bDistrictGarrison;
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	return Result;
}

bool UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
	const UObject* WorldContextObject,
	ATerritoryDistrict* District,
	APlayerController* Viewer,
	FTerritoryDistrictOperationsView& OutView)
{
	OutView = FTerritoryDistrictOperationsView();
	if (!WorldContextObject || !District)
	{
		OutView.AvailabilityReason = NSLOCTEXT("TerritoryOperations", "DistrictUnavailable", "District is not loaded or registered.");
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		OutView.AvailabilityReason = NSLOCTEXT("TerritoryOperations", "WorldUnavailable", "World context is unavailable.");
		return false;
	}

	AActor* ViewerActor = ResolveViewerActor(Viewer);
	OutView.District = District;
	OutView.DistrictTag = District->GetTerritoryTag();
	OutView.DisplayName = District->GetTerritoryDisplayName();
	OutView.City = District->GetOwningCity();
	if (OutView.City)
	{
		OutView.CityTag = OutView.City->GetTerritoryTag();
		OutView.CityDisplayName = OutView.City->GetTerritoryDisplayName();
	}
	OutView.OwnerFaction = District->GetOwningFaction();
	OutView.ViewerFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(WorldContextObject, ViewerActor);
	OutView.ContestingFaction = District->GetOwnershipData().ContestingFaction;
	OutView.TerritoryState = District->GetTerritoryState();
	OutView.LockReason = District->GetLockReason();

	if (const UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
	{
		OutView.bRegistered = Registry->GetTerritoryByTag(OutView.DistrictTag) == District;
	}
	OutView.bUnlocked = OutView.TerritoryState != ETerritoryState::Locked;
	OutView.bHierarchyVisible = IsTerritoryVisibleToPlayer(WorldContextObject, District);
	OutView.bOwnedByViewer = OutView.ViewerFaction.IsValid() && OutView.OwnerFaction == OutView.ViewerFaction;
	TArray<ATerritoryVolume*> AllProperties = District->GetProperties();
	AllProperties.RemoveAll([](const ATerritoryVolume* Property)
	{
		return !IsValid(Property);
	});
	OutView.Hierarchy = GetDistrictHierarchyOperationsViews(WorldContextObject, District, Viewer);
	TArray<ATerritoryVolume*> VisibleProperties;
	for (const FTerritoryHierarchyOperationsView& Item : OutView.Hierarchy)
	{
		if (Item.HierarchyLevel == ETerritoryHierarchyLevel::Place && Item.Territory)
		{
			OutView.VisiblePlaces.Add(Item);
			VisibleProperties.Add(Item.Territory);
			OutView.ContestableProperties += Item.bAvailableForCapture ? 1 : 0;
		}
	}
	OutView.TotalProperties = AllProperties.Num();
	OutView.KnownProperties = OutView.VisiblePlaces.Num();
	OutView.HiddenProperties = FMath::Max(0,
		OutView.TotalProperties - OutView.KnownProperties);
	OutView.bAllPlacesDiscovered = OutView.TotalProperties > 0
		&& OutView.HiddenProperties == 0;
	if (OutView.ViewerFaction.IsValid())
	{
		for (const ATerritoryVolume* Property : VisibleProperties)
		{
			OutView.OwnedProperties += Property
				&& Property->GetOwningFaction() == OutView.ViewerFaction ? 1 : 0;
		}
	}

	OutView.CaptureEligibility = ECaptureResult::InvalidTerritory;
	if (const UTerritoryControlSubsystem* Control = World->GetSubsystem<UTerritoryControlSubsystem>())
	{
		OutView.CaptureEligibility = Control->GetCaptureEligibility(District, OutView.ViewerFaction);
		OutView.bCaptureInProgress = Control->IsCaptureInProgress(District)
			|| OutView.TerritoryState == ETerritoryState::Contested;
		OutView.CaptureProgress = District->GetControlProgress();
		OutView.bAttackerCountKnown = World->GetNetMode() != NM_Client;
		if (OutView.bAttackerCountKnown && OutView.ContestingFaction.IsValid())
		{
			OutView.ActiveAttackers = Control->GetActiveAttackers(District, OutView.ContestingFaction);
		}

		// District is an aggregate authority, so surface child Property pressure instead of
		// making an active leaf assault disappear from the command center.
		float HighestChildProgress = OutView.CaptureProgress;
		for (ATerritoryVolume* Property : VisibleProperties)
		{
			if (!Property || !Control->IsCaptureInProgress(Property)
				&& Property->GetTerritoryState() != ETerritoryState::Contested)
			{
				continue;
			}
			OutView.bCaptureInProgress = true;
			const float ChildProgress = Property->GetControlProgress();
			if (ChildProgress >= HighestChildProgress)
			{
				HighestChildProgress = ChildProgress;
				OutView.ContestingFaction = Property->GetOwnershipData().ContestingFaction;
			}
			if (OutView.bAttackerCountKnown
				&& Property->GetOwnershipData().ContestingFaction.IsValid())
			{
				OutView.ActiveAttackers += Control->GetActiveAttackers(
					Property, Property->GetOwnershipData().ContestingFaction);
			}
		}
		OutView.CaptureProgress = HighestChildProgress;
	}
	OutView.bAvailableForCapture = OutView.CaptureEligibility == ECaptureResult::Success;
	OutView.AvailabilityReason = GetCaptureResultText(OutView.CaptureEligibility, District);

	OutView.bManageable = OutView.bRegistered
		&& OutView.bHierarchyVisible
		&& OutView.bOwnedByViewer
		&& OutView.TerritoryState == ETerritoryState::Claimed
		&& ViewerActor != nullptr;
	if (!ViewerActor)
	{
		OutView.ManagementFailureReason = NSLOCTEXT("TerritoryOperations", "NoViewer", "No local player faction is available.");
	}
	else if (!OutView.bOwnedByViewer)
	{
		OutView.ManagementFailureReason = NSLOCTEXT("TerritoryOperations", "NotOwner", "Only the owning faction can manage this district.");
	}
	else if (OutView.TerritoryState != ETerritoryState::Claimed)
	{
		OutView.ManagementFailureReason = NSLOCTEXT("TerritoryOperations", "NotClaimed", "The district must be securely claimed before it can be managed.");
	}
	else if (!OutView.bRegistered)
	{
		OutView.ManagementFailureReason = NSLOCTEXT("TerritoryOperations", "NotRegistered", "The district is not currently registered.");
	}
	OutView.bAvailable = OutView.bRegistered && OutView.bHierarchyVisible
		&& (OutView.bManageable || OutView.bAvailableForCapture || OutView.TerritoryState == ETerritoryState::Unclaimed);

	OutView.GarrisonTargets = GetDistrictGarrisonOperationsViews(WorldContextObject, District, Viewer);
	OutView.GarrisonTargets.RemoveAll([&VisibleProperties, District](
		const FTerritoryGarrisonOperationsView& Garrison)
	{
		return Garrison.Territory && Garrison.Territory != District
			&& !VisibleProperties.Contains(Garrison.Territory);
	});
	OutView.ActiveGuards = 0;
	OutView.DesiredGuards = 0;
	OutView.MaximumGuards = 0;
	OutView.ReserveGuards = 0;
	OutView.PeriodicIncome = 0;
	OutView.GuardUpkeep = 0;
	OutView.bCanAddGuard = false;
	OutView.bCanRemoveGuard = false;
	OutView.bCanSendReinforcements = false;
	float WeightedGuardQuality = 0.f;
	float GuardQualityWeight = 0.f;
	OutView.GuardQuality = 0.f;
	OutView.Fortification = 0.f;
	OutView.AlliedSupport = 0.f;
	OutView.StrategicValue = 0.f;
	for (const FTerritoryGarrisonOperationsView& Garrison : OutView.GarrisonTargets)
	{
		if (!Garrison.Territory || Garrison.Territory->GetOwningFaction() != OutView.OwnerFaction) continue;
		OutView.ActiveGuards += Garrison.ActiveGuards;
		OutView.DesiredGuards += Garrison.DesiredGuards;
		OutView.MaximumGuards += Garrison.MaximumGuards;
		OutView.ReserveGuards += Garrison.ReserveGuards;
		OutView.PeriodicIncome += Garrison.PeriodicIncome;
		OutView.GuardUpkeep += Garrison.GuardUpkeep;
		OutView.bCanAddGuard = OutView.bCanAddGuard || Garrison.bCanIncreaseTarget;
		OutView.bCanRemoveGuard = OutView.bCanRemoveGuard || Garrison.bCanDecreaseTarget;
		OutView.bCanSendReinforcements = OutView.bCanSendReinforcements
			|| Garrison.bCanSendReinforcements;
		if (OutView.ReinforcementFailureReason.IsEmpty()
			&& !Garrison.ReinforcementFailureReason.IsEmpty())
		{
			OutView.ReinforcementFailureReason = Garrison.ReinforcementFailureReason;
		}
		OutView.ManageableGarrisonTargets += Garrison.bManageable ? 1 : 0;
		OutView.UnguardedGarrisonTargets +=
			Garrison.MaximumGuards > 0 && Garrison.ActiveGuards <= 0 ? 1 : 0;
		const float QualityWeight = static_cast<float>(FMath::Max(0, Garrison.ActiveGuards))
			+ 0.5f * static_cast<float>(FMath::Max(0, Garrison.ReserveGuards));
		WeightedGuardQuality += FMath::Max(0.f, Garrison.Territory->GetGuardQuality())
			* QualityWeight;
		GuardQualityWeight += QualityWeight;
		OutView.Fortification += FMath::Max(0.f, Garrison.Territory->GetFortificationStrength());
		OutView.AlliedSupport += FMath::Max(0.f, Garrison.Territory->GetNearbyAlliedSupport());
		OutView.StrategicValue += FMath::Max(0.f, Garrison.Territory->GetStrategicValue());
	}
	OutView.GuardQuality = GuardQualityWeight > KINDA_SMALL_NUMBER
		? WeightedGuardQuality / GuardQualityWeight : FMath::Max(0.f, District->GetGuardQuality());
	OutView.bUnguarded = OutView.ActiveGuards <= 0;
	OutView.bReserveCountKnown = true;

	OutView.GuardPurchaseCost = District->GetGuardRecruitmentCost(1);
	OutView.NetIncome = OutView.PeriodicIncome - OutView.GuardUpkeep;
	if (const UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		OutView.AvailableFunds = Economy->GetActorCurrency(ViewerActor);
	}
	if (!OutView.bManageable)
	{
		OutView.AddGuardFailureReason = OutView.ManagementFailureReason;
		OutView.RemoveGuardFailureReason = OutView.ManagementFailureReason;
	}
	OutView.CommandCapabilities.Add(BuildCommandCapabilityView(
		WorldContextObject, OutView.ViewerFaction,
		TerritoryCommandTags::GuardStaffing,
		NSLOCTEXT("TerritoryOperations", "GuardStaffingCapability", "Garrison Staffing"),
		NSLOCTEXT("TerritoryOperations", "GuardStaffingCapabilityDescription",
			"Raise persistent guard targets and pay recruitment plus upkeep.")));
	OutView.CommandCapabilities.Add(BuildCommandCapabilityView(
		WorldContextObject, OutView.ViewerFaction,
		TerritoryCommandTags::Reinforcements,
		NSLOCTEXT("TerritoryOperations", "ReinforcementCapability", "Emergency Reinforcements"),
		NSLOCTEXT("TerritoryOperations", "ReinforcementCapabilityDescription",
			"Deploy an existing reserve into an empty assigned guard post without recruiting a new guard.")));
	OutView.bFinancialRisk = OutView.NetIncome < 0 || OutView.ActiveGuards < OutView.DesiredGuards;

	TArray<FTerritoryProductionSiteRecord> ProductionRecords;
	FTerritoryFactionResourceSnapshot ResourceSnapshot;
	if (World->GetNetMode() != NM_Client)
	{
		if (const UTerritoryEconomySubsystem* Economy =
			World->GetSubsystem<UTerritoryEconomySubsystem>())
		{
			ProductionRecords = Economy->GetAllProductionSites();
			ResourceSnapshot = Economy->GetFactionResourceSnapshot(OutView.OwnerFaction);
		}
	}
	else if (const ATerritoryWorldState* WorldState = FindTerritoryWorldState(World))
	{
		ProductionRecords = WorldState->GetProductionSites();
		ResourceSnapshot = WorldState->GetFactionResourceSnapshot(OutView.OwnerFaction);
	}
	for (const FTerritoryProductionSiteRecord& Record : ProductionRecords)
	{
		if (Record.ParentTerritoryTag != OutView.DistrictTag) continue;
		const bool bVisiblePlace = VisibleProperties.ContainsByPredicate(
			[&Record](const ATerritoryVolume* Property)
			{
				return Property && Property->GetTerritoryTag() == Record.TerritoryTag;
			});
		if (!bVisiblePlace) continue;
		FTerritoryProductionSiteOperationsView SiteView =
			BuildProductionSiteView(Record, ResourceSnapshot);
		OutView.ProducingSiteCount += SiteView.bProducing ? 1 : 0;
		OutView.BlockedProductionSiteCount += SiteView.bBlocked ? 1 : 0;
		OutView.ProductionSites.Add(MoveTemp(SiteView));
	}
	OutView.ProductionSites.Sort([](const FTerritoryProductionSiteOperationsView& A,
		const FTerritoryProductionSiteOperationsView& B)
	{
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	MergeResourceFlows(OutView.ProductionSites, ResourceSnapshot, OutView.ResourceFlows);

	if (const UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
	{
		TArray<FTerritoryAssaultRecord> Assaults =
			Counterattacks->GetAssaultsForTerritoryActor(District);
		for (const ATerritoryVolume* Property : VisibleProperties)
		{
			if (Property && Property->GetOwningFaction() == OutView.OwnerFaction)
			{
				Assaults.Append(Counterattacks->GetAssaultsForTerritoryActor(Property));
			}
		}
		const FTerritoryAssaultRecord* Preferred = nullptr;
		for (const FTerritoryAssaultRecord& Assault : Assaults)
		{
			if (Assault.IsTerminal())
			{
				continue;
			}
			++OutView.NonTerminalAssaultCount;
			if (!Preferred || GetAssaultPriority(Assault.State) > GetAssaultPriority(Preferred->State))
			{
				Preferred = &Assault;
			}
		}

		if (Preferred)
		{
			OutView.AssaultID = Preferred->AssaultID;
			OutView.ThreatTargetTerritory = Preferred->TargetTerritory;
			OutView.AssaultState = Preferred->State;
			OutView.AssaultResolution = Preferred->Resolution;
			OutView.AttackingFaction = Preferred->AttackingFaction;
			OutView.PlannedAttackers = Preferred->PlannedForce;
			OutView.AliveAttackers = Preferred->AliveForce;
			OutView.PendingReserveAttackers = Preferred->PendingReserveForce;
			OutView.KilledAttackers = Preferred->KilledForce;
			OutView.WithdrawnAttackers = Preferred->WithdrawnForce;
			OutView.LaunchProbability = Preferred->EvaluationResult.LaunchProbability;
			OutView.EstimatedSuccessProbability = Preferred->EvaluationResult.EstimatedSuccessProbability;
			OutView.AttackPriority = Preferred->EvaluationResult.AttackPriority;
			OutView.DistrictDefencePower = Preferred->EvaluationResult.DistrictDefencePower;
			OutView.PowerRatio = Preferred->EvaluationResult.PowerRatio;
			OutView.SelectedApproaches = Preferred->SelectedApproaches;
			OutView.bUnderAttack = Preferred->State == ETerritoryAssaultState::Active;
			OutView.bAttackScheduled = !OutView.bUnderAttack;
			if (!OutView.bAttackerCountKnown && OutView.bUnderAttack)
			{
				OutView.ActiveAttackers = Preferred->AliveForce;
				OutView.bAttackerCountKnown = true;
			}
		}
		else if (OutView.OwnerFaction.IsValid())
		{
			// No durable assault exists yet: show the same diplomacy-first strongest-candidate
			// calculation used by automatic scheduling. This is planning data only.
			TArray<const ATerritoryVolume*> PreviewTargets;
			PreviewTargets.Add(District);
			for (const ATerritoryVolume* Property : VisibleProperties)
			{
				if (Property && Property->GetOwningFaction() == OutView.OwnerFaction)
				{
					PreviewTargets.Add(Property);
				}
			}
			FText LastReason;
			for (const ATerritoryVolume* Target : PreviewTargets)
			{
				FGameplayTag CandidateFaction;
				FTerritoryAssaultEvaluationInput CandidateInput;
				FTerritoryAssaultEvaluationResult CandidateResult;
				FText CandidateReason;
				if (!Counterattacks->GetBestEligibleAttackerPreview(Target, FGameplayTag(),
					CandidateFaction, CandidateInput, CandidateResult, CandidateReason))
				{
					if (!CandidateReason.IsEmpty()) LastReason = CandidateReason;
					continue;
				}
				const bool bBetter = !OutView.bThreatPreviewAvailable
					|| CandidateResult.AttackPriority > OutView.AttackPriority + KINDA_SMALL_NUMBER
					|| (FMath::IsNearlyEqual(CandidateResult.AttackPriority, OutView.AttackPriority)
						&& Target->GetTerritoryTag().ToString()
							< OutView.ThreatTargetTerritory.ToString());
				if (bBetter)
				{
					OutView.bThreatPreviewAvailable = true;
					OutView.ThreatTargetTerritory = Target->GetTerritoryTag();
					OutView.AttackingFaction = CandidateFaction;
					OutView.LaunchProbability = CandidateResult.LaunchProbability;
					OutView.EstimatedSuccessProbability = CandidateResult.EstimatedSuccessProbability;
					OutView.AttackPriority = CandidateResult.AttackPriority;
					OutView.DistrictDefencePower = CandidateResult.DistrictDefencePower;
					OutView.PowerRatio = CandidateResult.PowerRatio;
					OutView.ThreatEvaluationReason = CandidateReason;
				}
			}
			if (!OutView.bThreatPreviewAvailable)
			{
				OutView.ThreatEvaluationReason = LastReason;
			}
		}
	}

	OutView.bUnderAttack = OutView.bUnderAttack || OutView.TerritoryState == ETerritoryState::Contested;
	if (OutView.bUnderAttack)
	{
		OutView.ThreatLevel = ETerritoryThreatLevel::Critical;
	}
	else if (OutView.AssaultID.IsValid()
		&& (OutView.AssaultState == ETerritoryAssaultState::ScheduledWarning
			|| OutView.AssaultState == ETerritoryAssaultState::WaitingForPlayerProximity))
	{
		OutView.ThreatLevel = ETerritoryThreatLevel::Warning;
	}
	else if (OutView.AssaultID.IsValid())
	{
		OutView.ThreatLevel = ETerritoryThreatLevel::Watch;
	}
	else if (OutView.bThreatPreviewAvailable)
	{
		OutView.ThreatLevel = OutView.LaunchProbability >= 0.75f
			? ETerritoryThreatLevel::Warning : ETerritoryThreatLevel::Watch;
	}
	OutView.ThreatSummary = BuildThreatSummary(OutView);

	if (const UTerritoryDiplomacySubsystem* Diplomacy =
		World->GetSubsystem<UTerritoryDiplomacySubsystem>())
	{
		OutView.OwnerReputation = OutView.OwnerFaction.IsValid()
			? Diplomacy->GetReputation(OutView.OwnerFaction) : 0;
		if (OutView.ViewerFaction.IsValid() && OutView.OwnerFaction.IsValid()
			&& OutView.ViewerFaction != OutView.OwnerFaction)
		{
			OutView.ViewerOwnerDiplomacy = Diplomacy->GetDiplomacyState(
				OutView.ViewerFaction, OutView.OwnerFaction);
			OutView.bViewerAtWarWithOwner = Diplomacy->IsAtWar(
				OutView.ViewerFaction, OutView.OwnerFaction);
			OutView.bViewerAlliedWithOwner = Diplomacy->IsAllied(
				OutView.ViewerFaction, OutView.OwnerFaction);
			OutView.bViewerTradesWithOwner = Diplomacy->HasTradeAgreement(
				OutView.ViewerFaction, OutView.OwnerFaction);
			OutView.DiplomacySummary = FText::Format(
				NSLOCTEXT("TerritoryOperations", "DiplomacySummary",
					"{0} with {1}  |  Reputation {2}"),
				GetDiplomacyStateText(OutView.ViewerOwnerDiplomacy),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(OutView.OwnerFaction),
				FText::AsNumber(OutView.OwnerReputation));
		}
		else if (OutView.ViewerFaction.IsValid()
			&& OutView.ViewerFaction == OutView.OwnerFaction)
		{
			OutView.bViewerAlliedWithOwner = true;
			OutView.DiplomacySummary = NSLOCTEXT(
				"TerritoryOperations", "OwnFactionDiplomacy", "Your faction controls this District.");
		}
	}
	if (OutView.DiplomacySummary.IsEmpty())
	{
		OutView.DiplomacySummary = NSLOCTEXT(
			"TerritoryOperations", "NoDiplomacyContext", "No faction relationship is available.");
	}
	return true;
}

TArray<FTerritoryDistrictOperationsView> UTerritoryUIBlueprintLibrary::GetDistrictOperationsViews(
	const UObject* WorldContextObject,
	APlayerController* Viewer,
	ETerritoryOperationsFilter Filter)
{
	TArray<FTerritoryDistrictOperationsView> Result;
	for (ATerritoryDistrict* District : UTerritoryBlueprintLibrary::GetAllDistricts(WorldContextObject))
	{
		FTerritoryDistrictOperationsView View;
		if (BuildDistrictOperationsView(WorldContextObject, District, Viewer, View)
			&& DoesDistrictMatchFilter(View, Filter))
		{
			Result.Add(MoveTemp(View));
		}
	}
	Result.Sort([](const FTerritoryDistrictOperationsView& A, const FTerritoryDistrictOperationsView& B)
	{
		const int32 APriority = GetDistrictOperationsPriority(A);
		const int32 BPriority = GetDistrictOperationsPriority(B);
		if (APriority != BPriority) return APriority > BPriority;
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	return Result;
}

TArray<FTerritoryDistrictOperationsView>
UTerritoryUIBlueprintLibrary::GetPlayerVisibleDistrictOperationsViews(
	const UObject* WorldContextObject, APlayerController* Viewer,
	ETerritoryOperationsFilter Filter)
{
	TArray<FTerritoryDistrictOperationsView> Result;
	for (FTerritoryDistrictOperationsView& View : GetDistrictOperationsViews(
		WorldContextObject, Viewer, ETerritoryOperationsFilter::All))
	{
		if (View.bHierarchyVisible && DoesDistrictMatchFilter(View, Filter))
		{
			Result.Add(MoveTemp(View));
		}
	}
	Result.Sort([](const FTerritoryDistrictOperationsView& A,
		const FTerritoryDistrictOperationsView& B)
	{
		const FString ACity = A.CityDisplayName.IsEmpty()
			? FString(TEXT("~")) : A.CityDisplayName.ToString();
		const FString BCity = B.CityDisplayName.IsEmpty()
			? FString(TEXT("~")) : B.CityDisplayName.ToString();
		if (ACity != BCity)
		{
			return ACity < BCity;
		}
		const int32 APriority = GetDistrictOperationsPriority(A);
		const int32 BPriority = GetDistrictOperationsPriority(B);
		if (APriority != BPriority)
		{
			return APriority > BPriority;
		}
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	return Result;
}

bool UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(
	const FTerritoryDistrictOperationsView& View,
	ETerritoryOperationsFilter Filter)
{
	switch (Filter)
	{
	case ETerritoryOperationsFilter::Unlocked: return View.bRegistered && View.bUnlocked;
	case ETerritoryOperationsFilter::Available: return View.bAvailable;
	case ETerritoryOperationsFilter::Owned: return View.bOwnedByViewer;
	case ETerritoryOperationsFilter::Manageable: return View.bManageable;
	case ETerritoryOperationsFilter::UnderAttack:
		return View.bUnderAttack || View.bAttackScheduled || View.bThreatPreviewAvailable;
	case ETerritoryOperationsFilter::Contested: return View.bCaptureInProgress;
	case ETerritoryOperationsFilter::Locked: return !View.bUnlocked;
	case ETerritoryOperationsFilter::FinancialRisk: return View.bFinancialRisk;
	case ETerritoryOperationsFilter::Producing: return View.ProducingSiteCount > 0;
	case ETerritoryOperationsFilter::ProductionBlocked: return View.BlockedProductionSiteCount > 0;
	case ETerritoryOperationsFilter::MissingInputs:
		for (const FTerritoryProductionSiteOperationsView& Site : View.ProductionSites)
		{
			if (Site.Status == ETerritoryProductionStatus::MissingInput) return true;
		}
		return false;
	case ETerritoryOperationsFilter::StorageFull:
		for (const FTerritoryProductionSiteOperationsView& Site : View.ProductionSites)
		{
			if (Site.Status == ETerritoryProductionStatus::StorageFull) return true;
		}
		return false;
	case ETerritoryOperationsFilter::All:
	default: return View.bRegistered;
	}
}

bool UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(
	const FTerritoryDistrictOperationsView& View, const FString& SearchText)
{
	FString Query = SearchText;
	Query.TrimStartAndEndInline();
	if (Query.IsEmpty())
	{
		return true;
	}

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	const FString StateName = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(View.TerritoryState)).ToString()
		: FString();
	TArray<FString> SearchFields = {
		View.DisplayName.ToString(),
		View.DistrictTag.ToString(),
		View.CityDisplayName.ToString(),
		View.CityTag.ToString(),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.OwnerFaction).ToString(),
		View.OwnerFaction.ToString(),
		StateName,
		View.AvailabilityReason.ToString(),
		View.ManagementFailureReason.ToString(),
		View.LockReason.ToString(),
		View.ThreatSummary.ToString(),
		View.ThreatEvaluationReason.ToString(),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.AttackingFaction).ToString(),
		View.AttackingFaction.ToString(),
		View.ThreatTargetTerritory.ToString(),
		View.DiplomacySummary.ToString()
	};
	for (const FTerritoryHierarchyOperationsView& Place : View.VisiblePlaces)
	{
		SearchFields.Add(Place.DisplayName.ToString());
		SearchFields.Add(Place.TerritoryTag.ToString());
	}
	for (const FTerritoryGarrisonOperationsView& Garrison : View.GarrisonTargets)
	{
		SearchFields.Add(Garrison.DisplayName.ToString());
		SearchFields.Add(Garrison.TerritoryTag.ToString());
	}
	for (const FTerritoryProductionSiteOperationsView& Site : View.ProductionSites)
	{
		SearchFields.Add(Site.DisplayName.ToString());
		SearchFields.Add(Site.TerritoryTag.ToString());
		SearchFields.Add(Site.ActiveRuleTag.ToString());
		SearchFields.Add(Site.StatusReason.ToString());
		SearchFields.Add(UTerritoryUIBlueprintLibrary::GetProductionStatusText(Site.Status).ToString());
		for (const FTerritoryResourceOperationsView& Resource : Site.Resources)
		{
			SearchFields.Add(Resource.DisplayName.ToString());
			if (Resource.ItemClass) SearchFields.Add(Resource.ItemClass->GetPathName());
		}
	}

	TArray<FString> Tokens;
	Query.ParseIntoArrayWS(Tokens);
	for (const FString& Token : Tokens)
	{
		bool bTokenMatched = false;
		for (const FString& Field : SearchFields)
		{
			if (Field.Contains(Token, ESearchCase::IgnoreCase))
			{
				bTokenMatched = true;
				break;
			}
		}
		if (!bTokenMatched)
		{
			return false;
		}
	}
	return true;
}

bool UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(
	const FTerritoryDistrictOperationsView& View)
{
	return View.bRegistered
		&& View.bHierarchyVisible
		&& View.bUnlocked
		&& !View.bOwnedByViewer;
}

bool UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(
	const FTerritoryDistrictOperationsView& View)
{
	return View.bRegistered
		&& View.bHierarchyVisible
		&& View.bOwnedByViewer
		&& View.TerritoryState != ETerritoryState::Unclaimed;
}

int32 UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(
	const FTerritoryDistrictOperationsView& View)
{
	uint32 Hash = GetTypeHash(View.DistrictTag);
	Hash = HashCombineFast(Hash, GetTypeHash(View.CityTag));
	Hash = HashCombineFast(Hash, GetTypeHash(View.OwnerFaction));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ViewerFaction));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ContestingFaction));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.TerritoryState)));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.CaptureEligibility)));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bRegistered));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bUnlocked));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bHierarchyVisible));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bAvailable));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bManageable));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bUnderAttack));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bAttackScheduled));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bThreatPreviewAvailable));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bUnguarded));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bFinancialRisk));
	Hash = HashCombineFast(Hash, GetTypeHash(FMath::RoundToInt(View.CaptureProgress * 1000.f)));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ActiveAttackers));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ActiveGuards));
	Hash = HashCombineFast(Hash, GetTypeHash(View.DesiredGuards));
	Hash = HashCombineFast(Hash, GetTypeHash(View.MaximumGuards));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ReserveGuards));
	Hash = HashCombineFast(Hash, GetTypeHash(View.AvailableFunds));
	Hash = HashCombineFast(Hash, GetTypeHash(View.NetIncome));
	Hash = HashCombineFast(Hash, GetTypeHash(View.GuardPurchaseCost));
	Hash = HashCombineFast(Hash, GetTypeHash(View.AssaultID));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.AssaultState)));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.ThreatLevel)));
	Hash = HashCombineFast(Hash, GetTypeHash(View.AliveAttackers));
	Hash = HashCombineFast(Hash, GetTypeHash(View.PendingReserveAttackers));
	Hash = HashCombineFast(Hash, GetTypeHash(View.KilledAttackers));
	Hash = HashCombineFast(Hash, GetTypeHash(View.WithdrawnAttackers));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ThreatTargetTerritory));
	Hash = HashCombineFast(Hash, GetTypeHash(FMath::RoundToInt(View.LaunchProbability * 1000.f)));
	Hash = HashCombineFast(Hash, GetTypeHash(FMath::RoundToInt(View.EstimatedSuccessProbability * 1000.f)));
	Hash = HashCombineFast(Hash, GetTypeHash(FMath::RoundToInt(View.AttackPriority * 10.f)));
	Hash = HashCombineFast(Hash, GetTypeHash(View.TotalProperties));
	Hash = HashCombineFast(Hash, GetTypeHash(View.KnownProperties));
	Hash = HashCombineFast(Hash, GetTypeHash(View.HiddenProperties));
	Hash = HashCombineFast(Hash, GetTypeHash(View.OwnedProperties));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ContestableProperties));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bAllPlacesDiscovered));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ManageableGarrisonTargets));
	Hash = HashCombineFast(Hash, GetTypeHash(View.UnguardedGarrisonTargets));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanAddGuard));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanRemoveGuard));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanSendReinforcements));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ProducingSiteCount));
	Hash = HashCombineFast(Hash, GetTypeHash(View.BlockedProductionSiteCount));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.ViewerOwnerDiplomacy)));
	Hash = HashCombineFast(Hash, GetTypeHash(View.OwnerReputation));
	for (const FTerritoryHierarchyOperationsView& Place : View.VisiblePlaces)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Place.TerritoryTag));
		Hash = HashCombineFast(Hash, GetTypeHash(Place.OwnerFaction));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Place.TerritoryState)));
		Hash = HashCombineFast(Hash, GetTypeHash(Place.ActiveGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Place.DesiredGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Place.NetIncome));
	}
	for (const FTerritoryGarrisonOperationsView& Garrison : View.GarrisonTargets)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.TerritoryTag));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.ActiveGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.DesiredGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.MaximumGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.ReserveGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.PendingDeployments));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.NetIncome));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.bCanSendReinforcements));
	}
	for (const FTerritoryCommandCapabilityView& Capability : View.CommandCapabilities)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Capability.Capability));
		Hash = HashCombineFast(Hash, GetTypeHash(Capability.bConfigured));
		Hash = HashCombineFast(Hash, GetTypeHash(Capability.bGranted));
		for (const FText& SourceName : Capability.ActiveSourceNames)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(SourceName.ToString()));
		}
	}
	for (const FTerritoryProductionSiteOperationsView& Site : View.ProductionSites)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Site.TerritoryTag));
		Hash = HashCombineFast(Hash, GetTypeHash(Site.ActiveRuleTag));
		Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Site.Status)));
		Hash = HashCombineFast(Hash, GetTypeHash(Site.LastEvaluatedCycle));
		for (const FTerritoryResourceOperationsView& Resource : Site.Resources)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Resource.ItemClass));
			Hash = HashCombineFast(Hash, GetTypeHash(Resource.StoredQuantity));
			Hash = HashCombineFast(Hash, GetTypeHash(Resource.NetPerCycle));
		}
	}
	Hash = HashCombineFast(Hash, GetTypeHash(View.LockReason.ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(View.AvailabilityReason.ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ThreatEvaluationReason.ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(View.DiplomacySummary.ToString()));
	return static_cast<int32>(Hash);
}

FTerritoryEconomyOperationsView UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(
	const UObject* WorldContextObject,
	APlayerController* Viewer,
	FGameplayTag Faction,
	int32 MaxRecentTransactions)
{
	FTerritoryEconomyOperationsView View;
	View.Faction = Faction;
	if (!WorldContextObject || !WorldContextObject->GetWorld())
	{
		return View;
	}

	if (!View.Faction.IsValid())
	{
		View.Faction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			WorldContextObject, ResolveViewerActor(Viewer));
	}
	UWorld* World = WorldContextObject->GetWorld();
	const UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	View.AvailableFunds = Economy ? Economy->GetActorCurrency(ResolveViewerActor(Viewer)) : 0;
	if (World->GetNetMode() != NM_Client && Economy)
	{
		const FTerritoryTreasury Snapshot = Economy->GetFactionEconomy(View.Faction);
		View.IncomePerTick = Snapshot.IncomePerTick;
		View.CostsPerTick = Snapshot.CostsPerTick;
		View.NetPerTick = View.IncomePerTick - View.CostsPerTick;
		View.TerritoryCount = Snapshot.TerritoryCount;
		View.bDeficit = View.NetPerTick < 0;
		View.RecentTransactions = Economy->GetTransactionHistory(
			View.Faction, FMath::Max(0, MaxRecentTransactions));
		for (const FTerritoryTransaction& Transaction : View.RecentTransactions)
		{
			if (Transaction.Amount >= 0)
			{
				View.RecentCredits += Transaction.Amount;
			}
			else
			{
				View.RecentDebits += -static_cast<int64>(Transaction.Amount);
			}
		}
	}
	else
	{
		const ATerritoryWorldState* WorldState = nullptr;
		for (TActorIterator<ATerritoryWorldState> It(World); It; ++It)
		{
			WorldState = *It;
			break;
		}
		if (WorldState)
		{
			const FTerritoryTreasury Snapshot = WorldState->GetFactionTreasury(View.Faction);
			View.IncomePerTick = Snapshot.IncomePerTick;
			View.CostsPerTick = Snapshot.CostsPerTick;
			View.NetPerTick = View.IncomePerTick - View.CostsPerTick;
			View.TerritoryCount = Snapshot.TerritoryCount;
			View.bDeficit = View.NetPerTick < 0;
			for (const FReplicatedTransaction& Replicated :
				WorldState->GetTransactionHistory(View.Faction, FMath::Max(0, MaxRecentTransactions)))
			{
				FTerritoryTransaction Transaction;
				Transaction.TransactionID = Replicated.TransactionID;
				Transaction.Faction = Replicated.Faction;
				Transaction.Type = Replicated.Type;
				Transaction.Amount = Replicated.Amount;
				Transaction.BalanceAfter = Replicated.BalanceAfter;
				Transaction.GameTime = Replicated.GameTime;
				Transaction.Reason = Replicated.Reason;
				Transaction.SourceTerritory = Replicated.SourceTerritory;
				View.RecentTransactions.Add(MoveTemp(Transaction));
				if (Replicated.Amount >= 0) View.RecentCredits += Replicated.Amount;
				else View.RecentDebits += -static_cast<int64>(Replicated.Amount);
			}
		}
	}

	TArray<FTerritoryProductionSiteRecord> ProductionRecords;
	FTerritoryFactionResourceSnapshot ResourceSnapshot;
	if (World->GetNetMode() != NM_Client && Economy)
	{
		ProductionRecords = Economy->GetProductionSitesForFaction(View.Faction);
		ResourceSnapshot = Economy->GetFactionResourceSnapshot(View.Faction);
	}
	else if (const ATerritoryWorldState* WorldState = FindTerritoryWorldState(World))
	{
		ProductionRecords = WorldState->GetProductionSitesForFaction(View.Faction);
		ResourceSnapshot = WorldState->GetFactionResourceSnapshot(View.Faction);
	}
	View.bResourceStorageAvailable = ResourceSnapshot.bStorageAvailable;
	for (const FTerritoryProductionSiteRecord& Record : ProductionRecords)
	{
		FTerritoryProductionSiteOperationsView SiteView =
			BuildProductionSiteView(Record, ResourceSnapshot);
		View.ProducingSiteCount += SiteView.bProducing ? 1 : 0;
		View.BlockedProductionSiteCount += SiteView.bBlocked ? 1 : 0;
		View.ProductionSites.Add(MoveTemp(SiteView));
	}
	View.ProductionSites.Sort([](const FTerritoryProductionSiteOperationsView& A,
		const FTerritoryProductionSiteOperationsView& B)
	{
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	for (const FTerritoryResourceAmount& Resource : ResourceSnapshot.Resources)
	{
		FindOrAddResourceView(View.ResourceStockpile, Resource.ItemClass, ResourceSnapshot);
	}
	MergeResourceFlows(View.ProductionSites, ResourceSnapshot, View.ResourceStockpile);
	return View;
}

bool UTerritoryUIBlueprintLibrary::BuildProductionSiteOperationsView(
	const UObject* WorldContextObject,
	FGameplayTag TerritoryTag,
	FTerritoryProductionSiteOperationsView& OutView)
{
	OutView = FTerritoryProductionSiteOperationsView();
	if (!WorldContextObject || !TerritoryTag.IsValid()) return false;

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World) return false;

	FTerritoryProductionSiteRecord Record;
	FTerritoryFactionResourceSnapshot ResourceSnapshot;
	bool bFound = false;
	if (World->GetNetMode() != NM_Client)
	{
		if (const UTerritoryEconomySubsystem* Economy =
			World->GetSubsystem<UTerritoryEconomySubsystem>())
		{
			Record = Economy->GetProductionSite(TerritoryTag);
			bFound = Record.TerritoryTag == TerritoryTag;
			if (bFound)
			{
				ResourceSnapshot = Economy->GetFactionResourceSnapshot(Record.OwnerFaction);
			}
		}
	}
	else if (const ATerritoryWorldState* WorldState = FindTerritoryWorldState(World))
	{
		for (const FTerritoryProductionSiteRecord& Candidate : WorldState->GetProductionSites())
		{
			if (Candidate.TerritoryTag == TerritoryTag)
			{
				Record = Candidate;
				ResourceSnapshot = WorldState->GetFactionResourceSnapshot(Record.OwnerFaction);
				bFound = true;
				break;
			}
		}
	}

	if (!bFound) return false;
	OutView = BuildProductionSiteView(Record, ResourceSnapshot);
	return true;
}

FText UTerritoryUIBlueprintLibrary::GetProductionStatusText(
	ETerritoryProductionStatus Status)
{
	switch (Status)
	{
	case ETerritoryProductionStatus::Ready:
		return NSLOCTEXT("TerritoryOperations", "ProductionReady", "Ready");
	case ETerritoryProductionStatus::Produced:
		return NSLOCTEXT("TerritoryOperations", "ProductionProduced", "Producing");
	case ETerritoryProductionStatus::MissingInput:
		return NSLOCTEXT("TerritoryOperations", "ProductionMissingInput", "Missing input");
	case ETerritoryProductionStatus::StorageUnavailable:
		return NSLOCTEXT("TerritoryOperations", "ProductionNoStorage", "Storage unavailable");
	case ETerritoryProductionStatus::StorageFull:
		return NSLOCTEXT("TerritoryOperations", "ProductionStorageFull", "Storage full");
	case ETerritoryProductionStatus::Inactive:
		return NSLOCTEXT("TerritoryOperations", "ProductionInactive", "Inactive");
	case ETerritoryProductionStatus::InvalidProfile:
		return NSLOCTEXT("TerritoryOperations", "ProductionInvalid", "Invalid profile");
	case ETerritoryProductionStatus::AlreadyProcessed:
		return NSLOCTEXT("TerritoryOperations", "ProductionWaiting", "Next cycle");
	case ETerritoryProductionStatus::AuthorityRejected:
		return NSLOCTEXT("TerritoryOperations", "ProductionRejected", "Request rejected");
	case ETerritoryProductionStatus::NeverEvaluated:
	default:
		return NSLOCTEXT("TerritoryOperations", "ProductionNotEvaluated", "Not evaluated");
	}
}

FText UTerritoryUIBlueprintLibrary::GetThreatLevelText(ETerritoryThreatLevel ThreatLevel)
{
	switch (ThreatLevel)
	{
	case ETerritoryThreatLevel::Critical: return NSLOCTEXT("TerritoryOperations", "ThreatCritical", "CRITICAL");
	case ETerritoryThreatLevel::Warning: return NSLOCTEXT("TerritoryOperations", "ThreatWarning", "WARNING");
	case ETerritoryThreatLevel::Watch: return NSLOCTEXT("TerritoryOperations", "ThreatWatch", "WATCH");
	case ETerritoryThreatLevel::None:
	default: return NSLOCTEXT("TerritoryOperations", "ThreatNone", "SECURE");
	}
}

FText UTerritoryUIBlueprintLibrary::GetAssaultStateText(ETerritoryAssaultState AssaultState)
{
	const UEnum* Enum = StaticEnum<ETerritoryAssaultState>();
	return Enum
		? Enum->GetDisplayNameTextByValue(static_cast<int64>(AssaultState))
		: FText::GetEmpty();
}

FText UTerritoryUIBlueprintLibrary::GetDiplomacyStateText(EDiplomacyState DiplomacyState)
{
	const UEnum* Enum = StaticEnum<EDiplomacyState>();
	return Enum
		? Enum->GetDisplayNameTextByValue(static_cast<int64>(DiplomacyState))
		: NSLOCTEXT("TerritoryOperations", "UnknownDiplomacyState", "Unknown relationship");
}
