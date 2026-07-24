#include "UI/TerritoryDebugWidget.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void UTerritoryDebugWidget::SetDebugEnabled(bool bEnabled)
{
	bDebugEnabled = bEnabled;
}

void UTerritoryDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bDebugEnabled)
	{
		return;
	}

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	if (!Settings || !Settings->IsDebugEnabled())
	{
		return;
	}

	// Throttle rebuilds — avoid per-frame string allocation pressure.
	TimeSinceLastUpdate += InDeltaTime;
	if (TimeSinceLastUpdate < UpdateInterval)
	{
		return;
	}
	TimeSinceLastUpdate = 0.f;

	CacheSubsystems();
	FText DebugText = BuildDebugString();
	OnUpdateDebugText(DebugText);
}

void UTerritoryDebugWidget::CacheSubsystems() const
{
	if (bSubsystemsCached) return;
	const UWorld* World = GetWorld();
	if (!World) return;
	CachedRegistry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	CachedControl = World->GetSubsystem<UTerritoryControlSubsystem>();
	CachedEconomy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	CachedDiplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	bSubsystemsCached = true;
}

FText UTerritoryDebugWidget::BuildDebugString() const
{
	FString Result = TEXT("=== TERRITORY DEBUG ===\n");

	Result += BuildTerritorySummary().ToString();
	Result += BuildEconomySummary().ToString();
	Result += BuildDiplomacySummary().ToString();
	Result += BuildCaptureSummary().ToString();

	return FText::FromString(Result);
}

FText UTerritoryDebugWidget::BuildTerritorySummary() const
{
	if (!CachedRegistry) return FText::GetEmpty();
	const UTerritoryRegistrySubsystem* Registry = CachedRegistry;

	FString Result = TEXT("--- Territories ---\n");
	Result += FString::Printf(TEXT("Total: %d\n"), Registry->GetTerritoryCount());

	TArray<ATerritoryVolume*> All = Registry->GetAllTerritories();
	for (const ATerritoryVolume* Terr : All)
	{
		if (!Terr) continue;

		FString Name = Terr->GetTerritoryDisplayName().ToString();
		if (Name.IsEmpty()) Name = Terr->GetTerritoryTag().ToString();

		FString StateStr;
		switch (Terr->GetTerritoryState())
		{
		case ETerritoryState::Unclaimed: StateStr = TEXT("Unclaimed"); break;
		case ETerritoryState::Claimed: StateStr = TEXT("Claimed"); break;
		case ETerritoryState::Contested: StateStr = TEXT("Contested"); break;
		case ETerritoryState::Locked: StateStr = TEXT("Locked"); break;
		default: StateStr = TEXT("Unknown"); break;
		}

		Result += FString::Printf(TEXT("  %s [%s] Owner=%s State=%s Guards=%d Income=%d\n"),
			*Name,
			*Terr->GetTerritoryTag().ToString(),
			*Terr->GetOwningFaction().ToString(),
			*StateStr,
			Terr->GetDefenderCount(),
			Terr->GetPeriodicIncome());
	}

	return FText::FromString(Result);
}

FText UTerritoryDebugWidget::BuildEconomySummary() const
{
	if (!CachedEconomy) return FText::GetEmpty();
	const UTerritoryEconomySubsystem* Economy = CachedEconomy;

	FString Result = TEXT("--- Economy ---\n");

	TArray<FGameplayTag> Factions = Economy->GetAllFactionsWithTreasury();
	for (const FGameplayTag& Faction : Factions)
	{
		FTerritoryTreasury Treasury = Economy->GetFactionEconomy(Faction);
		int32 Aggregate = Economy->GetTreasury(Faction);
		Result += FString::Printf(TEXT("  %s: Wealth=%d Income=%d Costs=%d Territories=%d\n"),
			*Faction.ToString(),
			Aggregate,
			Treasury.IncomePerTick,
			Treasury.CostsPerTick,
			Treasury.TerritoryCount);
	}

	return FText::FromString(Result);
}

FText UTerritoryDebugWidget::BuildDiplomacySummary() const
{
	if (!CachedDiplomacy) return FText::GetEmpty();
	const UTerritoryDiplomacySubsystem* Diplomacy = CachedDiplomacy;

	FString Result = TEXT("--- Diplomacy ---\n");

	TArray<FTreatyRecord> Treaties = Diplomacy->GetAllTreaties();
	for (const FTreatyRecord& Treaty : Treaties)
	{
		FString StateStr;
		switch (Treaty.State)
		{
		case EDiplomacyState::Alliance: StateStr = TEXT("Alliance"); break;
		case EDiplomacyState::TradeAgreement: StateStr = TEXT("Trade"); break;
		case EDiplomacyState::NonAggression: StateStr = TEXT("NonAggr"); break;
		case EDiplomacyState::War: StateStr = TEXT("WAR"); break;
		case EDiplomacyState::Ceasefire: StateStr = TEXT("Ceasefire"); break;
		default: StateStr = TEXT("None"); break;
		}

		Result += FString::Printf(TEXT("  %s ↔ %s: %s\n"),
			*Treaty.FactionA.ToString(),
			*Treaty.FactionB.ToString(),
			*StateStr);
	}

	if (Treaties.Num() == 0)
	{
		Result += TEXT("  (no active treaties)\n");
	}

	return FText::FromString(Result);
}

FText UTerritoryDebugWidget::BuildCaptureSummary() const
{
	if (!CachedControl) return FText::GetEmpty();
	const UTerritoryControlSubsystem* Control = CachedControl;

	FString Result = TEXT("--- Capture ---\n");

	TArray<ATerritoryVolume*> AllTerritories;
	if (CachedRegistry)
	{
		AllTerritories = CachedRegistry->GetAllTerritories();
	}

	bool bAnyContested = false;
	for (const ATerritoryVolume* Terr : AllTerritories)
	{
		if (!Terr) continue;

		if (Control->IsCaptureInProgress(Terr))
		{
			bAnyContested = true;
			float Progress = Control->GetCaptureProgress(Terr);
			FGameplayTag Contesting = Control->GetContestingFaction(Terr);

			Result += FString::Printf(TEXT("  %s: CONTESTED by %s (%.0f%%)\n"),
				*Terr->GetTerritoryTag().ToString(),
				*Contesting.ToString(),
				Progress * 100.f);
		}
	}

	if (!bAnyContested)
	{
		Result += TEXT("  (no active captures)\n");
	}

	return FText::FromString(Result);
}
