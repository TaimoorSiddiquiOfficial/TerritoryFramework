#include "UI/TerritoryEconomyWidget.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Engine/World.h"

void UTerritoryEconomyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindDelegates();
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
			Snapshot.Treasury = Economy->GetTreasury(DisplayFaction);
			Snapshot.TotalIncome = Economy->GetIncome(DisplayFaction);
			Snapshot.TotalCosts = Economy->GetCosts(DisplayFaction);
			Snapshot.TerritoryCount = Economy->GetFactionEconomy(DisplayFaction).TerritoryCount;
			OnEconomyUpdated(DisplayFaction, Snapshot);
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
	return Economy ? Economy->GetTreasury(DisplayFaction) : 0;
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
}

void UTerritoryEconomyWidget::UnbindDelegates()
{
	UTerritoryEconomySubsystem* Economy = GetEconomySubsystem();
	if (Economy)
	{
		Economy->OnEconomyTickFired.RemoveDynamic(this, &UTerritoryEconomyWidget::HandleEconomyTick);
		Economy->OnTransactionRecorded.RemoveDynamic(this, &UTerritoryEconomyWidget::HandleTransactionRecorded);
	}
}

void UTerritoryEconomyWidget::HandleEconomyTick(FGameplayTag Faction, FTerritoryEconomySnapshot Snapshot)
{
	if (Faction == DisplayFaction)
	{
		OnEconomyUpdated(Faction, Snapshot);
	}
}

void UTerritoryEconomyWidget::HandleTransactionRecorded(const FTerritoryTransaction& Transaction)
{
	if (Transaction.Faction == DisplayFaction)
	{
		OnTransactionRecorded(Transaction);
	}
}

UTerritoryEconomySubsystem* UTerritoryEconomyWidget::GetEconomySubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTerritoryEconomySubsystem>() : nullptr;
}
