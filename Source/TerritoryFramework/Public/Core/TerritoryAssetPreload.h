#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

struct FStreamableHandle;

enum class ETerritoryAssetReadiness : uint8 { Ready, Pending, Failed };

/** Transient residency owned by an existing subsystem, never gameplay/save authority.
 * Poll from the existing scheduler. No load delegate can commit stale gameplay.
 */
class TERRITORYFRAMEWORK_API FTerritoryAssetPreload
{
public:
	ETerritoryAssetReadiness Poll(const TArray<FSoftObjectPath>& Paths);
	void Reset();
	uint64 GetGeneration() const { return Generation; }

private:
	friend class FTFTerritoryAssetReadiness;
	struct FEntry
	{
		TStrongObjectPtr<UObject> Resident;
		TSharedPtr<FStreamableHandle> Handle;
	};
	TMap<FSoftObjectPath, FEntry> Entries;
	uint64 Generation = 0;
};
