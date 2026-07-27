#include "UI/TerritoryEconomyWidget.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UTerritoryEconomyWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
			Snapshot.Treasury = Economy->GetActorCurrency(GetOwningPlayer());
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
	return Economy ? Economy->GetActorCurrency(GetOwningPlayer()) : 0;
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
	Snapshot.Treasury = Economy->GetActorCurrency(GetOwningPlayer());
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
	if (EconomyFactionText) EconomyFactionText->SetText(FText::FromString(DisplayFaction.ToString()));
	if (EconomyTreasuryText) EconomyTreasuryText->SetText(FText::AsNumber(GetCurrentGold()));
	if (EconomyIncomeText) EconomyIncomeText->SetText(FText::AsNumber(GetCurrentIncome()));
	if (EconomyCostsText) EconomyCostsText->SetText(FText::AsNumber(GetCurrentCosts()));
	if (EconomyTerritoryCountText) EconomyTerritoryCountText->SetText(FText::AsNumber(GetTerritoryCount()));
}

UTerritoryEconomySubsystem* UTerritoryEconomyWidget::GetEconomySubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
}
