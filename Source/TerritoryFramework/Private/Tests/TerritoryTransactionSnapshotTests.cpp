#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TerritoryTransactionSnapshot.h"
#include "Core/TerritoryWorldState.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransactionWireBudget,
	"TerritoryFramework.Replication.TransactionHistory.WireBudgetAndRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransactionWireBudget::RunTest(const FString& Parameters)
{
	TArray<FReplicatedTransaction> Rows;
	for (int32 Index = 0; Index < 500; ++Index)
	{
		FReplicatedTransaction& Row = Rows.AddDefaulted_GetRef();
		Row.TransactionID = FGuid::NewGuid();
		Row.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		Row.Type = ETerritoryTransactionType::ManualCredit;
		Row.Amount = Index % 2 ? -Index : Index;
		Row.BalanceAfter = 5000 + Index;
		Row.GameTime = 12345.123456789 + Index;
		Row.Reason = FString::Printf(TEXT("Verified territory transaction %d: currency / Unicode \u0631\u0642\u0645"), Index);
	}
	FTerritoryTransactionSnapshot Snapshot;
	TestTrue(TEXT("500 full rows fit the bounded wire snapshot"), Snapshot.Build(Rows));
	FBitWriter Wire(0, true);
	bool Success = false;
	Snapshot.NetSerialize(Wire, nullptr, Success);
	TestTrue(TEXT("A valid snapshot serializes"), Success);
	TestTrue(TEXT("History leaves room for other WorldState properties in a 64 KB bunch"), Wire.GetNumBytes() < 33 * 1024);
	FTerritoryTransactionSnapshot Received;
	FBitReader Reader(Wire.GetData(), Wire.GetNumBits());
	Received.NetSerialize(Reader, nullptr, Success);
	TArray<FReplicatedTransaction> Decoded;
	TestTrue(TEXT("Received bytes decode"), Success && Received.Decode(Decoded));
	TestEqual(TEXT("All 500 records survive"), Decoded.Num(), Rows.Num());
	if (Decoded.Num() == Rows.Num())
	{
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const auto& A = Rows[Index]; const auto& B = Decoded[Index];
			if (A.TransactionID != B.TransactionID || A.Faction != B.Faction || A.Type != B.Type
				|| A.Amount != B.Amount || A.BalanceAfter != B.BalanceAfter || A.GameTime != B.GameTime
				|| A.Reason != B.Reason || A.SourceTerritory != B.SourceTerritory)
			{
				AddError(FString::Printf(TEXT("Lossy record %d"), Index)); break;
			}
		}
	}
	TestTrue(TEXT("An empty authoritative ledger is a valid ready snapshot"), Snapshot.Build({}) && Snapshot.Decode(Decoded));
	TestEqual(TEXT("Empty replaces the old history"), Decoded.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTransactionWireFailure,
	"TerritoryFramework.Replication.TransactionHistory.RejectAndRecover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTransactionWireFailure::RunTest(const FString& Parameters)
{
	FTerritoryTransactionSnapshot Snapshot;
	TArray<FReplicatedTransaction> Rows;
	Rows.SetNum(1);
	Rows[0].Reason = FString::ChrN(FTerritoryTransactionSnapshot::MaximumDecodedBytes + 1, TEXT('x'));
	TestFalse(TEXT("Oversized input is rejected before network replication"), Snapshot.Build(Rows));
	TestFalse(TEXT("Unavailable is explicit, not a ready empty history"), Snapshot.bAvailable);
	TestTrue(TEXT("Unavailable payload is discarded"), Snapshot.Payload.IsEmpty());
	FBitWriter Wire(0, true);
	bool Success = false;
	Snapshot.NetSerialize(Wire, nullptr, Success);
	TestTrue(TEXT("Unavailable marker can replicate without poisoning the actor bunch"), Success && Wire.GetNumBytes() <= 2);
	Rows.SetNum(500);
	for (auto& Row : Rows)
	{
		Row.Reason.Reset();
		for (int32 Part = 0; Part < 16; ++Part) Row.Reason += FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	TestFalse(TEXT("High-entropy text cannot exceed the actual wire budget"), Snapshot.Build(Rows));
	TestTrue(TEXT("A failed build leaves no oversized wire payload"), Snapshot.Payload.IsEmpty());
	Rows.SetNum(1);
	Rows[0].Reason = TEXT("Recovered");
	TestTrue(TEXT("The next supported snapshot recovers"), Snapshot.Build(Rows));
	TArray<FReplicatedTransaction> Decoded;
	TestTrue(TEXT("Recovery decodes normally"), Snapshot.Decode(Decoded));
	Snapshot.DecodedBytes = FTerritoryTransactionSnapshot::MaximumDecodedBytes + 1;
	TestFalse(TEXT("Untrusted decompression size is bounded"), Snapshot.Decode(Decoded));
	TestEqual(TEXT("A rejected decode cannot partially replace the prior cache"), Decoded.Num(), 1);
	Snapshot.DecodedBytes = 0;
	TestFalse(TEXT("Truncated metadata is rejected"), Snapshot.Decode(Decoded));
	return true;
}
#endif
