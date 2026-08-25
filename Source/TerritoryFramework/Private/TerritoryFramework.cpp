#include "TerritoryFramework.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "Debug/TerritoryGameplayDebuggerCategory.h"
#include "GameplayDebugger.h"
#endif

#define LOCTEXT_NAMESPACE "FTerritoryFrameworkModule"

void FTerritoryFrameworkModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
	GameplayDebugger.RegisterCategory(TEXT("Territory"),
		IGameplayDebugger::FOnGetCategory::CreateStatic(
			&FGameplayDebuggerCategory_Territory::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebugger.NotifyCategoriesChanged();
#endif
}

void FTerritoryFrameworkModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
		GameplayDebugger.UnregisterCategory(TEXT("Territory"));
		GameplayDebugger.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTerritoryFrameworkModule, TerritoryFramework)
