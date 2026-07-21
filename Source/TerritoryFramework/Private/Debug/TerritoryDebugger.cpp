#include "Debug/TerritoryDebugger.h"
#include "Core/TerritoryTypes.h"

// GameplayDebugger API varies significantly between UE versions.
// Registration is stubbed out. Enable and adapt when targeting a specific UE version.
// To activate: include the correct GameplayDebugger headers for your UE version
// and implement RegisterCategory/UnregisterCategory using the version-specific API.

void UTerritoryDebugger::RegisterCategory()
{
	UE_LOG(LogTerritory, Log, TEXT("TerritoryDebugger: RegisterCategory (stub — adapt to your UE version's GameplayDebugger API)"));
}

void UTerritoryDebugger::UnregisterCategory()
{
	UE_LOG(LogTerritory, Log, TEXT("TerritoryDebugger: UnregisterCategory (stub)"));
}
