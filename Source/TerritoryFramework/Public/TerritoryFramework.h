#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"

class UWorld;

class FTerritoryFrameworkModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterVisualRemovalWorld(UWorld* World);
	FDelegateHandle VisualWorldInitializedHandle;
	FDelegateHandle VisualWorldCleanupHandle;
	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> VisualActorDestroyedHandles;
};
