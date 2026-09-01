#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTerritoryFrameworkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterTerritoryAssetTypes();
	void UnregisterTerritoryAssetTypes();
	void EnsureVehicleRoadsForLoadedLevel();
	void NormalizeClaimedDiplomacyForLoadedLevel();
	void MigrateFactionSignatureVehiclesForLoadedLevel();
	TArray<TSharedPtr<class IAssetTypeActions>> RegisteredAssetTypeActions;
	uint32 TerritoryAssetCategory = 0;
	IConsoleObject* EnsureVehicleRoadsConsoleCommand = nullptr;
	IConsoleObject* NormalizeClaimedDiplomacyConsoleCommand = nullptr;
	IConsoleObject* MigrateFactionSignatureVehiclesConsoleCommand = nullptr;
};
