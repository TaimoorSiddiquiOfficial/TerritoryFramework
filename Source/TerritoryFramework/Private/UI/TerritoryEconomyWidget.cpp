#include "UI/TerritoryEconomyWidget.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/TerritoryUIBlueprintLibrary.h"

void UTerritoryEconomyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!DisplayFaction.IsValid())
	{
		APlayerController* PlayerController = GetOwningPlayer();
		DisplayFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
			this, PlayerController && PlayerController->GetPawn()
				? static_cast<AActor*>(PlayerController->GetPawn())
				: PlayerController);
	}
	BindDelegates();
	RefreshEconomyDisplay();
}

void UTerritoryEconomyWidget::NativeDestruct()
{
	UnbindDelegates();
	Super::NativeDestruct();
}

void UTerritoryEconomyWidget::SetDisplayFaction(const FGameplayTag& Faction)
{
	DisplayFaction = Faction;

	// Immediately update with current data
	if (DisplayFaction.IsValid())
	{
		UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
		if (Economy)
		{
			FTerritoryEconomySnapshot Snapshot;
			APlayerController* PlayerController = GetOwningPlayer();
			Snapshot.Treasury = Economy->GetActorCurrency(
				PlayerController && PlayerController->GetPawn()
					? static_cast<AActor*>(PlayerController->GetPawn())
					: PlayerController);
			Snapshot.TotalIncome = Economy->GetIncome(DisplayFaction);
			Snapshot.TotalCosts = Economy->GetCosts(DisplayFaction);
			Snapshot.TerritoryCount = Economy->GetFactionEconomy(DisplayFaction).TerritoryCount;
			OnEconomyUpdated(DisplayFaction, Snapshot);
			RefreshEconomyDisplay();
		}
	}
}

FGameplayTag UTerritoryEconomyWidget::GetDisplayFaction() const
{
	return DisplayFaction;
}

int32 UTerritoryEconomyWidget::GetCurrentGold() const
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	APlayerController* PlayerController = GetOwningPlayer();
	AActor* AccountActor = PlayerController && PlayerController->GetPawn()
		? static_cast<AActor*>(PlayerController->GetPawn())
		: PlayerController;
	return Economy ? Economy->GetActorCurrency(AccountActor) : 0;
}

int32 UTerritoryEconomyWidget::GetCurrentIncome() const
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	return Economy ? Economy->GetIncome(DisplayFaction) : 0;
}

int32 UTerritoryEconomyWidget::GetCurrentCosts() const
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	return Economy ? Economy->GetCosts(DisplayFaction) : 0;
}

int32 UTerritoryEconomyWidget::GetTerritoryCount() const
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	return Economy ? Economy->GetFactionEconomy(DisplayFaction).TerritoryCount : 0;
}

int64 UTerritoryEconomyWidget::GetNetIncome() const
{
	return static_cast<int64>(GetCurrentIncome()) - GetCurrentCosts();
}

bool UTerritoryEconomyWidget::IsOperatingAtDeficit() const
{
	return GetNetIncome() < 0;
}

FTerritoryEconomyOperationsView UTerritoryEconomyWidget::GetEconomyOperationsView(
	int32 MaxRecentTransactions) const
{
	return UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(
		this, GetOwningPlayer(), DisplayFaction, MaxRecentTransactions);
}

void UTerritoryEconomyWidget::BindDelegates()
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	if (Economy)
	{
		Economy->OnEconomyTickFired.AddDynamic(this, &UTerritoryEconomyWidget::HandleEconomyTick);
		Economy->OnTransactionRecorded.AddDynamic(this, &UTerritoryEconomyWidget::HandleTransactionRecorded);
	}

	// Client polling fallback — if delegates are missed (late join, network desync),
	// periodically refresh from the subsystem's current state.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ClientPollTimerHandle,
			this,
			&UTerritoryEconomyWidget::ClientPollRefresh,
			ClientPollInterval,
			true);
	}
}

void UTerritoryEconomyWidget::UnbindDelegates()
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	if (Economy)
	{
		Economy->OnEconomyTickFired.RemoveDynamic(this, &UTerritoryEconomyWidget::HandleEconomyTick);
		Economy->OnTransactionRecorded.RemoveDynamic(this, &UTerritoryEconomyWidget::HandleTransactionRecorded);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClientPollTimerHandle);
	}
}

void UTerritoryEconomyWidget::ClientPollRefresh()
{
	if (!DisplayFaction.IsValid()) return;
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	if (!Economy) return;

	FTerritoryEconomySnapshot Snapshot;
	APlayerController* PlayerController = GetOwningPlayer();
	Snapshot.Treasury = Economy->GetActorCurrency(
		PlayerController && PlayerController->GetPawn()
			? static_cast<AActor*>(PlayerController->GetPawn())
			: PlayerController);
	Snapshot.TotalIncome = Economy->GetIncome(DisplayFaction);
	Snapshot.TotalCosts = Economy->GetCosts(DisplayFaction);
	Snapshot.TerritoryCount = Economy->GetFactionEconomy(DisplayFaction).TerritoryCount;
	OnEconomyUpdated(DisplayFaction, Snapshot);
	RefreshEconomyDisplay();
}

void UTerritoryEconomyWidget::HandleEconomyTick(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot)
{
	if (Faction == DisplayFaction)
	{
		OnEconomyUpdated(Faction, Snapshot);
		RefreshEconomyDisplay();
	}
}

void UTerritoryEconomyWidget::HandleTransactionRecorded(const FTerritoryTransaction& Transaction)
{
	if (Transaction.Faction == DisplayFaction)
	{
		OnTransactionRecorded(Transaction);
		RefreshEconomyDisplay();
	}
}

void UTerritoryEconomyWidget::RefreshEconomyDisplay()
{
	const FTerritoryEconomyOperationsView View = GetEconomyOperationsView(10);
	if (EconomyFactionText) EconomyFactionText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.Faction));
	if (EconomyTreasuryText) EconomyTreasuryText->SetText(FText::AsNumber(View.AvailableFunds));
	if (EconomyIncomeText) EconomyIncomeText->SetText(FText::AsNumber(View.IncomePerTick));
	if (EconomyCostsText) EconomyCostsText->SetText(FText::AsNumber(View.CostsPerTick));
	if (EconomyTerritoryCountText) EconomyTerritoryCountText->SetText(FText::AsNumber(View.TerritoryCount));
	if (EconomyNetText)
	{
		EconomyNetText->SetText(FText::Format(
			NSLOCTEXT("TerritoryEconomy", "NetPerCycle", "Net per cycle: {0}"),
			FText::AsNumber(View.NetPerTick)));
	}
	if (EconomyHealthText)
	{
		EconomyHealthText->SetText(View.bDeficit
			? NSLOCTEXT("TerritoryEconomy", "Deficit", "DEFICIT — reduce upkeep or increase income")
			: NSLOCTEXT("TerritoryEconomy", "Sustainable", "SUSTAINABLE"));
	}
	if (EconomyRecentActivityText)
	{
		EconomyRecentActivityText->SetText(FText::Format(
			NSLOCTEXT("TerritoryEconomy", "RecentActivity", "Recent credits {0}  |  Recent debits {1}"),
			FText::AsNumber(View.RecentCredits), FText::AsNumber(View.RecentDebits)));
	}
}

UTerritoryEconomySubsystem* UTerritoryEconomyWidget::GetEconomySubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
}
