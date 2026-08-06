#include "UI/TerritoryUIBlueprintLibrary.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

namespace
{
	AActor* ResolveViewerActor(APlayerController* Viewer)
	{
		return Viewer && Viewer->GetPawn() ? static_cast<AActor*>(Viewer->GetPawn()) : Viewer;
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

	FText GetCaptureResultText(ECaptureResult Result, const ATerritoryDistrict* District)
	{
		switch (Result)
		{
		case ECaptureResult::Success:
			return NSLOCTEXT("TerritoryOperations", "CaptureAvailable", "Available for physical capture.");
		case ECaptureResult::AlreadyOwned:
			return NSLOCTEXT("TerritoryOperations", "AlreadyOwned", "Owned by your faction.");
		case ECaptureResult::Locked:
			return District && !District->GetLockReason().IsEmpty()
				? District->GetLockReason()
				: NSLOCTEXT("TerritoryOperations", "Locked", "Locked by territory policy or Narrative conditions.");
		case ECaptureResult::DefendersRemain:
			return NSLOCTEXT("TerritoryOperations", "DefendersRemain", "Defenders must be defeated before capture can begin.");
		case ECaptureResult::DiplomaticallyBlocked:
			return NSLOCTEXT("TerritoryOperations", "DiplomacyBlocked", "A treaty or friendly Narrative attitude blocks an assault.");
		case ECaptureResult::InvalidTerritory:
		default:
			return District && District->GetControlMode() == ETerritoryControlMode::AggregateOnly
				? NSLOCTEXT("TerritoryOperations", "AggregateOnly", "Control is derived from child territories.")
				: NSLOCTEXT("TerritoryOperations", "InvalidCaptureContext", "No valid faction capture context is available.");
		}
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
	if (!PlayerController || !PlayerController->IsLocalController() || !WidgetClass || !LayerTag.IsValid())
	{
		return nullptr;
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
	OutView.bOwnedByViewer = OutView.ViewerFaction.IsValid() && OutView.OwnerFaction == OutView.ViewerFaction;

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
		for (ATerritoryVolume* Property : District->GetProperties())
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
	OutView.bAvailable = OutView.bRegistered && OutView.bUnlocked
		&& (OutView.bManageable || OutView.bAvailableForCapture || OutView.TerritoryState == ETerritoryState::Unclaimed);

	OutView.GarrisonTargets = GetDistrictGarrisonOperationsViews(WorldContextObject, District, Viewer);
	const TArray<ATerritoryVolume*> Properties = District->GetProperties();
	OutView.TotalProperties = Properties.Num();
	for (const ATerritoryVolume* Property : Properties)
	{
		if (Property && Property->GetOwningFaction() == OutView.OwnerFaction)
		{
			++OutView.OwnedProperties;
		}
	}
	OutView.ActiveGuards = 0;
	OutView.DesiredGuards = 0;
	OutView.MaximumGuards = 0;
	OutView.ReserveGuards = 0;
	OutView.PeriodicIncome = 0;
	OutView.GuardUpkeep = 0;
	OutView.bCanAddGuard = false;
	OutView.bCanRemoveGuard = false;
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
	OutView.bFinancialRisk = OutView.NetIncome < 0 || OutView.ActiveGuards < OutView.DesiredGuards;

	if (const UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
	{
		TArray<FTerritoryAssaultRecord> Assaults =
			Counterattacks->GetAssaultsForTerritory(OutView.DistrictTag);
		for (const ATerritoryVolume* Property : Properties)
		{
			if (Property && Property->GetOwningFaction() == OutView.OwnerFaction)
			{
				Assaults.Append(Counterattacks->GetAssaultsForTerritory(Property->GetTerritoryTag()));
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
			for (const ATerritoryVolume* Property : Properties)
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
		auto Priority = [](const FTerritoryDistrictOperationsView& View)
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
		};
		const int32 APriority = Priority(A);
		const int32 BPriority = Priority(B);
		if (APriority != BPriority) return APriority > BPriority;
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
	case ETerritoryOperationsFilter::All:
	default: return View.bRegistered;
	}
}

bool UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(
	const FTerritoryDistrictOperationsView& View)
{
	return View.bRegistered
		&& View.bUnlocked
		&& View.bAvailable
		&& !View.bOwnedByViewer;
}

bool UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(
	const FTerritoryDistrictOperationsView& View)
{
	return View.bRegistered
		&& View.bOwnedByViewer
		&& View.TerritoryState != ETerritoryState::Unclaimed;
}

int32 UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(
	const FTerritoryDistrictOperationsView& View)
{
	uint32 Hash = GetTypeHash(View.DistrictTag);
	Hash = HashCombineFast(Hash, GetTypeHash(View.OwnerFaction));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ViewerFaction));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ContestingFaction));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.TerritoryState)));
	Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(View.CaptureEligibility)));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bRegistered));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bUnlocked));
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
	Hash = HashCombineFast(Hash, GetTypeHash(View.OwnedProperties));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ManageableGarrisonTargets));
	Hash = HashCombineFast(Hash, GetTypeHash(View.UnguardedGarrisonTargets));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanAddGuard));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanRemoveGuard));
	for (const FTerritoryGarrisonOperationsView& Garrison : View.GarrisonTargets)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.TerritoryTag));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.ActiveGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.DesiredGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.MaximumGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.ReserveGuards));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.PendingDeployments));
		Hash = HashCombineFast(Hash, GetTypeHash(Garrison.NetIncome));
	}
	Hash = HashCombineFast(Hash, GetTypeHash(View.LockReason.ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(View.AvailabilityReason.ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(View.ThreatEvaluationReason.ToString()));
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
	return View;
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
