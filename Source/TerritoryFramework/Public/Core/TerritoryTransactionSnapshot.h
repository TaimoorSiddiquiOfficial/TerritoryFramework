#pragma once

#include "CoreMinimal.h"
#include "TerritoryTransactionSnapshot.generated.h"

struct FReplicatedTransaction;

/** Bounded, lossless transport only. Native inventory and the existing saved ledger remain authoritative. */
USTRUCT()
struct TERRITORYFRAMEWORK_API FTerritoryTransactionSnapshot
{
	GENERATED_BODY()
	static constexpr int32 MaximumWireBytes = 32 * 1024;
	static constexpr int32 MaximumDecodedBytes = 1024 * 1024;
	static constexpr int32 MaximumRows = 4096;

	UPROPERTY() bool bAvailable = false;
	UPROPERTY() uint32 DecodedBytes = 0;
	UPROPERTY() TArray<uint8> Payload;

	bool Build(TConstArrayView<FReplicatedTransaction> Rows);
	bool Decode(TArray<FReplicatedTransaction>& OutRows) const;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FTerritoryTransactionSnapshot>
	: TStructOpsTypeTraitsBase2<FTerritoryTransactionSnapshot>
{
	enum { WithNetSerializer = true };
};
