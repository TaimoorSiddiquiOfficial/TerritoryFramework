#include "TerritoryFrameworkEditor.h"

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkEditorModule"

void FTerritoryFrameworkEditorModule::StartupModule()
{
	// UEditorValidator subclasses are auto-registered by the DataValidation system
	// when the module loads — no manual registration needed
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module loaded (auto-validators registered)"));
}

void FTerritoryFrameworkEditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("TerritoryFrameworkEditor module unloaded"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkEditorModule, TerritoryFrameworkEditor)
