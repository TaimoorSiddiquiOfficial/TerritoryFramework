#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TerritoryDiplomacyTypes.generated.h"

UENUM(BlueprintType)
enum class EDiplomacyState : uint8
{
	None UMETA(DisplayName="Neutral / No Treaty", ToolTip="No rich treaty exists. Narrative attitude is Neutral. Example: two factions ignore each other."),
	Alliance UMETA(DisplayName="Alliance", ToolTip="Friendly relationship that blocks capture and counterattacks. Example: Heroes and Rebels fight the Regime together."),
	TradeAgreement UMETA(DisplayName="Trade Agreement", ToolTip="Friendly trade relationship with optional expiry. Use Territory metadata for trade rules and Narrative attitude for AI friendliness."),
	NonAggression UMETA(DisplayName="Non-Aggression Pact", ToolTip="The factions agree not to attack. Territory capture and scheduled assaults are blocked."),
	War UMETA(DisplayName="War", ToolTip="Hostile relationship. Physical capture and counterattacks are allowed when all other rules pass."),
	Ceasefire UMETA(DisplayName="Ceasefire", ToolTip="Temporary peace after hostility. New assaults are blocked and waiting assaults are cancelled.")
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
		meta = (Categories = "Narrative.Factions", ToolTip="First Narrative faction tag. Pair order does not change the treaty."))
	FGameplayTag FactionA;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta = (Categories = "Narrative.Factions", ToolTip="Second Narrative faction tag. Example: Narrative.Factions.Bandits."))
	FGameplayTag FactionB;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta=(ToolTip="Rich Territory treaty state. Narrative GameState remains the AI attitude authority."))
	EDiplomacyState State = EDiplomacyState::None;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta=(ToolTip="Narrative accumulated game time when the treaty began."))
	float SignedGameTime = 0.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta=(ToolTip="Narrative accumulated game time when a timed treaty ends. -1 means no scheduled expiry."))
	float ExpiryGameTime = -1.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Diplomacy",
		meta=(ToolTip="True means the treaty does not expire automatically. Story events may still change or break it."))
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
