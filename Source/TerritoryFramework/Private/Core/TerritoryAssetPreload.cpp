#include "Core/TerritoryAssetPreload.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

ETerritoryAssetReadiness FTerritoryAssetPreload::Poll(const TArray<FSoftObjectPath>& Paths)
{
	check(IsInGameThread());
	ETerritoryAssetReadiness Result = ETerritoryAssetReadiness::Ready;
	for (const FSoftObjectPath& Path : Paths)
	{
		if (Path.IsNull()) continue;
		FEntry* Entry = Entries.Find(Path);
		if (!Entry)
		{
			Entry = &Entries.Add(Path);
			Entry->Resident.Reset(Path.ResolveObject());
			if (!Entry->Resident.IsValid())
			{
				Entry->Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Path);
			}
		}
		if (Entry->Resident.IsValid()) continue;
		if (!Entry->Handle || Entry->Handle->WasCanceled()) return ETerritoryAssetReadiness::Failed;
		if (!Entry->Handle->HasLoadCompleted())
		{
			Result = ETerritoryAssetReadiness::Pending;
			continue;
		}
		Entry->Resident.Reset(Path.ResolveObject());
		if (!Entry->Resident.IsValid()) return ETerritoryAssetReadiness::Failed;
	}
	return Result;
}

void FTerritoryAssetPreload::Reset()
{
	check(IsInGameThread());
	++Generation;
	for (auto& Pair : Entries)
	{
		if (Pair.Value.Handle) Pair.Value.Handle->CancelHandle();
	}
	Entries.Empty();
}
