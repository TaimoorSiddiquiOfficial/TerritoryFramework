#include "TerritoryAuditEventProbe.h"
#include "AssetCompilingManager.h"
#include "Editor.h"

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
