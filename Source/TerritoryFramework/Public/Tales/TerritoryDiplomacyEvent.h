#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryDiplomacyTypes.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryDiplomacyEvent.generated.h"

UENUM(BlueprintType)
enum class ETerritoryReputationOperation : uint8
{
	Add UMETA(DisplayName="Add To Current Reputation", ToolTip="Adds the value. Use a negative value to reduce reputation."),
	Set UMETA(DisplayName="Set Exact Reputation", ToolTip="Replaces the current value with the authored value.")
};

/**
 * Changes the rich Territory treaty and synchronizes Narrative Pro's AI attitude.
 *
 * Easy example: after a betrayal quest, set Heroes and Regime to War. Regime NPCs
 * become hostile and any peace-blocked Territory assaults may be evaluated again.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Set Territory Diplomacy"))
class TERRITORYFRAMEWORK_API UTerritorySetDiplomacyEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritorySetDiplomacyEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="First Narrative faction. Example: Narrative.Factions.Heroes."))
	FGameplayTag FactionA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Second Narrative faction. Example: Narrative.Factions.Bandits."))
	FGameplayTag FactionB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="New rich relationship. Territory also writes the matching Friendly, Neutral, or Hostile attitude to Narrative GameState."))
	EDiplomacyState NewState = EDiplomacyState::War;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(EditCondition="NewState == EDiplomacyState::TradeAgreement", EditConditionHides,
			Units="s", ToolTip="Narrative game-time duration for a Trade Agreement. Zero or a negative value means permanent."))
	float TradeDurationGameTime = -1.f;

	/**
	 * Reconcile this idempotent diplomacy rule when a fresh world starts with the
	 * containing Territory already in this state. This does not run after loading a
	 * save and does not cause neighboring XP, wave, or reward events to execute.
	 *
	 * Easy example: a Place starts Claimed by Bandits and its Claimed row sets
	 * Bandits-Heroes to Neutral. Narrative AI is neutral before guards perceive Heroes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Recommended for state policy. On a fresh world only, apply this diplomacy event when the Territory starts already in the configured state. Other entry events are not fired."))
	bool bApplyWhenStateStartsActive = true;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/**
 * Changes the campaign's saved reputation value for one Narrative faction.
 *
 * Easy example: add -20 to Bandits after the player attacks a Bandit convoy.
 * This changes reputation metadata; treaty/AI attitude changes remain a separate event.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Modify Territory Faction Reputation"))
class TERRITORYFRAMEWORK_API UTerritoryModifyReputationEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	UTerritoryModifyReputationEvent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Narrative faction whose saved campaign reputation changes."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Add changes the current value. Set replaces it."))
	ETerritoryReputationOperation Operation = ETerritoryReputationOperation::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(ToolTip="Value to add or set. Example: -20 reduces reputation by twenty."))
	int32 Value = 0;

protected:
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
