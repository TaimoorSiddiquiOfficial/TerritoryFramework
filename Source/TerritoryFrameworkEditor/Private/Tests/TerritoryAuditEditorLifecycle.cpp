#include "TerritoryAuditEventProbe.h"
#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "TimerManager.h"

bool UTerritoryAuditEventProbe::RequestLateJoinForPIE()
{
	if (!GEditor || !GEditor->PlayWorld || GEditor->bIsSimulatingInEditor) return false;
	GEditor->RequestLateJoin();
	return true;
}

void UTerritoryAuditEventProbe::FinishAssetCompilationForAudit()
{
	FAssetCompilingManager::Get().FinishAllCompilation();
}

bool UTerritoryAuditEventProbe::ScheduleProductionRecipeForPIE(AActor* Account,
	FGameplayTag Faction, const FTerritoryProductionRule& Recipe, int32 BatchCount,
	FGameplayTag SourceTerritory)
{
	UWorld* World = IsValid(Account) ? Account->GetWorld() : nullptr;
	if (!World || World->WorldType != EWorldType::PIE || !Account->HasAuthority()
		|| World->GetNetMode() == NM_Client || BatchCount <= 0) return false;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Account,
		[Account, Faction, Recipe, BatchCount, SourceTerritory]()
		{
			if (UTerritoryEconomySubsystem* Economy = Account->GetWorld()->GetSubsystem<UTerritoryEconomySubsystem>())
			{
				FTerritoryProductionResult Result;
				Economy->ExecuteResourceRecipe(Account, Faction, Recipe, 1, BatchCount, SourceTerritory, Result);
			}
		}));
	return true;
}
