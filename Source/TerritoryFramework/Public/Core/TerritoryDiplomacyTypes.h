#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryDiplomacyTypes.generated.h"

UENUM(BlueprintType)
enum class EDiplomacyState : uint8
{
	None,
	Alliance,
	TradeAgreement,
	NonAggression,
	War,
	Ceasefire
};

UENUM(BlueprintType)
enum class EDiplomacyEventType : uint8
{
	DeclaredWar,
	DeclaredPeace,
	FormedAlliance,
	BrokeAlliance,
	SignedTradeAgreement,
	ExpiredTreaty,
	BrokeCeasefire,
	SignedNonAggression
};

USTRUCT(BlueprintType)
struct FTreatyRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag FactionA;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag FactionB;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy")
	EDiplomacyState State = EDiplomacyState::None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy")
	float SignedGameTime = 0.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy")
	float ExpiryGameTime = -1.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy")
	bool bPermanent = true;

	bool IsValid() const { return FactionA.IsValid() && FactionB.IsValid(); }

	bool IsExpired(float CurrentGameTime) const
	{
		return !bPermanent && ExpiryGameTime > 0.f && CurrentGameTime >= ExpiryGameTime;
	}

	FGuid GetCanonicalKey() const
	{
		const uint32 HashA = GetTypeHash(FactionA);
		const uint32 HashB = GetTypeHash(FactionB);
		const uint32 MinHash = FMath::Min(HashA, HashB);
		const uint32 MaxHash = FMath::Max(HashA, HashB);
		return FGuid(MinHash, MaxHash, 0, 0);
	}
};

USTRUCT(BlueprintType)
struct FDiplomacyEvent
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy")
	EDiplomacyEventType EventType = EDiplomacyEventType::DeclaredWar;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag FactionA;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag FactionB;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy")
	float GameTime = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnDiplomacyStateChanged,
	FGameplayTag, FactionA,
	FGameplayTag, FactionB,
	EDiplomacyState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDiplomacyEvent,
	const FDiplomacyEvent&, Event);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnReputationChanged,
	FGameplayTag, Faction,
	int32, NewReputation);
