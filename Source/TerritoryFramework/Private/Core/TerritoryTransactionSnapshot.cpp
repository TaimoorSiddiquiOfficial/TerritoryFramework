#include "Core/TerritoryTransactionSnapshot.h"
#include "Core/TerritoryWorldState.h"
#include "Misc/Compression.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	bool SerializeText(FArchive& Ar, FString& Text)
	{
		uint32 Length = 0;
		if (Ar.IsSaving())
		{
			FTCHARToUTF8 Encoded(*Text, Text.Len());
			Length = Encoded.Length();
			if (Length > FTerritoryTransactionSnapshot::MaximumDecodedBytes) return false;
			Ar << Length;
			Ar.Serialize(const_cast<ANSICHAR*>(Encoded.Get()), Length);
		}
		else
		{
			Ar << Length;
			if (Ar.IsError() || Length > FTerritoryTransactionSnapshot::MaximumDecodedBytes
				|| Length > Ar.TotalSize() - Ar.Tell()) return false;
			TArray<ANSICHAR> Encoded;
			Encoded.SetNumUninitialized(Length);
			Ar.Serialize(Encoded.GetData(), Length);
			if (Length == 0) Text.Reset();
			else { FUTF8ToTCHAR Decoded(Encoded.GetData(), Length); Text = FString(Decoded.Length(), Decoded.Get()); }
		}
		return !Ar.IsError();
	}

	bool SerializeTag(FArchive& Ar, FGameplayTag& Tag)
	{
		FString Name = Ar.IsSaving() && Tag.IsValid() ? Tag.ToString() : FString();
		if (!SerializeText(Ar, Name)) return false;
		if (Ar.IsLoading())
		{
			if (Name.Len() >= NAME_SIZE) return false;
			Tag = Name.IsEmpty() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(FName(*Name), false);
			if (!Name.IsEmpty() && !Tag.IsValid()) return false;
		}
		return true;
	}

	bool SerializeRow(FArchive& Ar, FReplicatedTransaction& Row)
	{
		Ar << Row.TransactionID;
		uint8 Type = static_cast<uint8>(Row.Type);
		Ar << Type << Row.Amount << Row.BalanceAfter << Row.GameTime;
		if (Ar.IsError() || !FMath::IsFinite(Row.GameTime)
			|| StaticEnum<ETerritoryTransactionType>()->GetIndexByValue(Type) == INDEX_NONE) return false;
		Row.Type = static_cast<ETerritoryTransactionType>(Type);
		return SerializeTag(Ar, Row.Faction) && SerializeTag(Ar, Row.SourceTerritory) && SerializeText(Ar, Row.Reason);
	}
}

bool FTerritoryTransactionSnapshot::Build(TConstArrayView<FReplicatedTransaction> Rows)
{
	bAvailable = false;
	DecodedBytes = 0;
	Payload.Reset();
	if (Rows.Num() > MaximumRows) return false;
	TArray<uint8> Raw;
	FMemoryWriter Writer(Raw);
	int32 Count = Rows.Num();
	Writer << Count;
	for (const FReplicatedTransaction& Source : Rows)
	{
		FReplicatedTransaction Row = Source;
		if (!SerializeRow(Writer, Row) || Raw.Num() > MaximumDecodedBytes) return false;
	}
	int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
	TArray<uint8> Compressed;
	Compressed.SetNumUninitialized(CompressedSize);
	if (!FCompression::CompressMemory(NAME_Zlib, Compressed.GetData(), CompressedSize, Raw.GetData(), Raw.Num())
		|| CompressedSize > MaximumWireBytes) return false;
	Compressed.SetNum(CompressedSize);
	Payload = MoveTemp(Compressed);
	DecodedBytes = Raw.Num();
	bAvailable = true;
	return true;
}

bool FTerritoryTransactionSnapshot::Decode(TArray<FReplicatedTransaction>& OutRows) const
{
	// Decode into a temporary array: malformed or incomplete data never partially replaces a cache.
	if (!bAvailable || DecodedBytes < sizeof(int32) || DecodedBytes > MaximumDecodedBytes
		|| Payload.IsEmpty() || Payload.Num() > MaximumWireBytes) return false;
	TArray<uint8> Raw;
	Raw.SetNumUninitialized(DecodedBytes);
	if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), Raw.Num(), Payload.GetData(), Payload.Num())) return false;
	FMemoryReader Reader(Raw);
	int32 Count = 0;
	Reader << Count;
	if (Reader.IsError() || Count < 0 || Count > MaximumRows) return false;
	TArray<FReplicatedTransaction> Decoded;
	Decoded.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FReplicatedTransaction Row;
		if (!SerializeRow(Reader, Row)) return false;
		Decoded.Add(MoveTemp(Row));
	}
	if (Reader.IsError() || Reader.Tell() != Reader.TotalSize()) return false;
	OutRows = MoveTemp(Decoded);
	return true;
}

bool FTerritoryTransactionSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 Version = 1;
	Ar << Version;
	uint8 Available = bAvailable ? 1 : 0;
	Ar.SerializeBits(&Available, 1);
	if (Ar.IsLoading()) { bAvailable = Available != 0; Payload.Reset(); DecodedBytes = 0; }
	if (Version != 1) { Ar.SetError(); bOutSuccess = false; return false; }
	if (!Available) { bOutSuccess = !Ar.IsError(); return true; }
	uint32 Size = Payload.Num();
	Ar.SerializeIntPacked(DecodedBytes);
	Ar.SerializeIntPacked(Size);
	if (Ar.IsError() || Size == 0 || Size > MaximumWireBytes
		|| DecodedBytes < sizeof(int32) || DecodedBytes > MaximumDecodedBytes)
	{
		Ar.SetError(); bOutSuccess = false; return false;
	}
	if (Ar.IsLoading()) Payload.SetNumUninitialized(Size);
	Ar.Serialize(Payload.GetData(), Size);
	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}
