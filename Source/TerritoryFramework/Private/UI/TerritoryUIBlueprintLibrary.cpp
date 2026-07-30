#include "UI/TerritoryUIBlueprintLibrary.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
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

	OutView.ActiveGuards = District->GetSpawnedGuardCount();
	OutView.DesiredGuards = District->GetDesiredGuardCount();
	OutView.MaximumGuards = District->GetMaxGuardCount();
	OutView.GuardQuality = District->GetGuardQuality();
	OutView.Fortification = District->GetFortificationStrength();
	OutView.AlliedSupport = District->GetNearbyAlliedSupport();
	OutView.StrategicValue = District->GetStrategicValue();
	OutView.bReserveCountKnown = World->GetNetMode() != NM_Client;
	if (OutView.bReserveCountKnown)
	{
		for (const ATerritoryGuardSpawnPoint* SpawnPoint : District->GetGuardSpawnPoints())
		{
			OutView.ReserveGuards += SpawnPoint ? SpawnPoint->GetReserveCount() : 0;
		}
	}

	OutView.GuardPurchaseCost = District->GetGuardPurchaseCost(1);
	OutView.PeriodicIncome = District->GetEffectiveIncome();
	OutView.GuardUpkeep = static_cast<int64>(FMath::Max(0, District->GetGuardCost()))
		* FMath::Max(0, OutView.DesiredGuards);
	OutView.NetIncome = OutView.PeriodicIncome - OutView.GuardUpkeep;
	if (const UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		OutView.AvailableFunds = Economy->GetActorCurrency(ViewerActor);
	}
	if (OutView.bManageable)
	{
		OutView.bCanAddGuard = District->CanPurchaseGuards(ViewerActor, 1, OutView.AddGuardFailureReason);
		OutView.bCanRemoveGuard = District->CanRemoveGuards(ViewerActor, 1, OutView.RemoveGuardFailureReason);
	}
	else
	{
		OutView.AddGuardFailureReason = OutView.ManagementFailureReason;
		OutView.RemoveGuardFailureReason = OutView.ManagementFailureReason;
	}
	OutView.bFinancialRisk = OutView.NetIncome < 0 || OutView.ActiveGuards < OutView.DesiredGuards;

	if (const UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
	{
		const TArray<FTerritoryAssaultRecord> Assaults = Counterattacks->GetAssaultsForTerritory(OutView.DistrictTag);
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
			OutView.SelectedApproaches = Preferred->SelectedApproaches;
			OutView.bUnderAttack = Preferred->State == ETerritoryAssaultState::Active;
			OutView.bAttackScheduled = !OutView.bUnderAttack;
			if (!OutView.bAttackerCountKnown && OutView.bUnderAttack)
			{
				OutView.ActiveAttackers = Preferred->AliveForce;
				OutView.bAttackerCountKnown = true;
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
		if (A.ThreatLevel != B.ThreatLevel)
		{
			return static_cast<uint8>(A.ThreatLevel) > static_cast<uint8>(B.ThreatLevel);
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
	case ETerritoryOperationsFilter::UnderAttack: return View.bUnderAttack || View.bAttackScheduled;
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
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanAddGuard));
	Hash = HashCombineFast(Hash, GetTypeHash(View.bCanRemoveGuard));
	Hash = HashCombineFast(Hash, GetTypeHash(View.LockReason.ToString()));
	Hash = HashCombineFast(Hash, GetTypeHash(View.AvailabilityReason.ToString()));
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
	if (const UTerritoryEconomySubsystem* Economy =
		WorldContextObject->GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>())
	{
		View.AvailableFunds = Economy->GetActorCurrency(ResolveViewerActor(Viewer));
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
