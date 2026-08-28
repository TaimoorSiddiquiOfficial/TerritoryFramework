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
 * Selects where a state-config diplomacy event gets one side of its faction pair.
 * Dynamic sources make one reusable Place work after any number of story ownership
 * changes instead of keeping a serialized Heroes/Bandits assumption forever.
 */
UENUM(BlueprintType)
enum class ETerritoryDiplomacyFactionSource : uint8
{
	ExplicitTag UMETA(DisplayName="Explicit Faction Tag",
		ToolTip="Use the Faction A/B tag below. Best for global quest events."),
	CurrentOwningFaction UMETA(DisplayName="Current Owning Faction",
		ToolTip="Use the containing Territory's owner after this transition. Example: the faction that just captured a Place."),
	PreviousOwningFaction UMETA(DisplayName="Previous Owning Faction",
		ToolTip="Use the owner immediately before this transition. Only valid while a state event is firing."),
	ContestingFaction UMETA(DisplayName="Contesting Faction",
		ToolTip="Use the faction currently contesting the containing Territory."),
	TransitionRequestingFaction UMETA(DisplayName="Transition Requesting Faction",
		ToolTip="Use the faction carried by the explicit capture, quest, or mutation context.")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event|Faction Resolution",
		meta=(ToolTip="Where Faction A comes from. For a Contested state row, Current Owning Faction means the defending faction."))
	ETerritoryDiplomacyFactionSource FactionASource =
		ETerritoryDiplomacyFactionSource::ExplicitTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Explicit first Narrative faction and migration fallback. Example: Narrative.Factions.Heroes."))
	FGameplayTag FactionA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event|Faction Resolution",
		meta=(ToolTip="Where Faction B comes from. For a Contested state row, Contesting Faction means the attacker."))
	ETerritoryDiplomacyFactionSource FactionBSource =
		ETerritoryDiplomacyFactionSource::ExplicitTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event",
		meta=(Categories="Narrative.Factions",
			ToolTip="Explicit second Narrative faction and migration fallback. Example: Narrative.Factions.Bandits."))
	FGameplayTag FactionB;

	/**
	 * Keeps old assets safe when a dynamic context is unavailable. For example, a
	 * fresh level can start Claimed without having a previous transition owner.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event|Faction Resolution",
		meta=(ToolTip="If a selected dynamic source is empty, use its Explicit Faction Tag. Recommended during migration and for initial-state policy."))
	bool bFallbackToExplicitFactionWhenContextMissing = true;

	/**
	 * Safety filter requested for owner-based state config. Global quest events are
	 * unaffected because they are not contained by a Territory actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event|Faction Resolution",
		meta=(ToolTip="When this event is inside a Territory state config, require the post-transition owning faction to be one side of the resolved pair. This prevents an old hardcoded Heroes/Bandits row from changing diplomacy after a third faction owns the Place."))
	bool bRequireContainingTerritoryOwner = true;

	/**
	 * A captured Place must not declare global peace while another Place is still
	 * actively contested by the same factions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Event|Conflict Safety",
		meta=(EditCondition="NewState != EDiplomacyState::War", EditConditionHides,
			ToolTip="Before applying a peace-like relationship, check loaded and World Partition territory summaries. Skip the change if another Place is still contested by this pair."))
	bool bPreserveOtherActiveTerritoryWars = true;

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

	/**
	 * Resolves the pair that would be applied right now. This is useful in a
	 * Blueprint debug panel and lets automated tests verify A->B->C ownership stories.
	 */
	UFUNCTION(BlueprintPure, Category="Territory|Diplomacy",
		meta=(DisplayName="Resolve Territory Diplomacy Faction Pair"))
	bool ResolveFactionPair(FGameplayTag& OutFactionA, FGameplayTag& OutFactionB) const;

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
