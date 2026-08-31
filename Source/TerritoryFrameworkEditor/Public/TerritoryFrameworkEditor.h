#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTerritoryFrameworkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void EnsureVehicleRoadsForLoadedLevel();
	void NormalizeClaimedDiplomacyForLoadedLevel();
	void MigrateFactionSignatureVehiclesForLoadedLevel();
	IConsoleObject* EnsureVehicleRoadsConsoleCommand = nullptr;
	IConsoleObject* NormalizeClaimedDiplomacyConsoleCommand = nullptr;
	IConsoleObject* MigrateFactionSignatureVehiclesConsoleCommand = nullptr;
};
