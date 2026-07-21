#include "TerritoryFrameworkEditor.h"
#include "DataValidation/TerritoryDataValidator.h"

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkEditorModule"

void FTerritoryFrameworkEditorModule::StartupModule()
{
	// Register data validators
	UTerritoryDataValidator::Register();
}

void FTerritoryFrameworkEditorModule::ShutdownModule()
{
	UTerritoryDataValidator::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkEditorModule, TerritoryFrameworkEditor)
