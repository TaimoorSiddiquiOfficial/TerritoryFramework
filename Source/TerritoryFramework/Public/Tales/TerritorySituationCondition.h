#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/DataAsset.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryStoryConditions.h"
#include "TerritorySituationCondition.generated.h"

UENUM(BlueprintType)
enum class ETerritorySituationScope : uint8
{
	District UMETA(DisplayName="Place's District"),
	City UMETA(DisplayName="Place's City"),
	Place UMETA(DisplayName="This Place Only")
};

/** A disposable read of existing authorities, never a second saved campaign state. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySituationReport
{
	GENERATED_BODY()
	/** Whether the required Territory and requesting-faction context could be resolved. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bContextKnown = false;
	/** Whether the target Territory is currently available for this situation query. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bTargetAvailable = false;
	/** Whether saved ownership history shows that the requesting faction owned this Place before. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bPreviouslyOwned = false;
	/** Whether the requesting faction previously owned this Place and no longer owns it. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bRetakeNeeded = false;
	/** Narrative faction resolved from the explicit caller or story context. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag RequestingFaction;
	/** Narrative faction that currently owns the target Territory. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag CurrentOwner;
	/** District or City tag defining the area used by this situation report. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag ScopeTerritory;
	/** Whether the required Place ownership rows are available for this area query. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bHoldingsKnown = false;
	/** Places counted in the selected area after availability rules are applied. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") int32 AvailablePlaces = 0;
	/** Available Places owned by the requesting faction in the selected area. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") int32 FactionPlaces = 0;
	/** Available Places owned by the checked faction, including Places currently contested by another faction. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") int32 FactionOwnedPlaces = 0;
	/** Percentage of available Places owned by the requesting faction, from 0 to 100. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") float FactionSharePercent = 0.f;
	/** Strict majority of available Places; ties/fragmented control have no dominant faction. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag DominantFaction;
	/** Whether the requesting faction's relationship with the current owner is known. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bOwnerRelationshipKnown = false;
	/** Diplomacy relationship between the requesting faction and the current owner. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") EDiplomacyState OwnerRelationship = EDiplomacyState::None;
	/** Whether diplomacy with the area's dominant faction is known and meaningful. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bDominantRelationshipKnown = false;
	/** Diplomacy relationship with the faction holding a strict majority of the area's Places. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") EDiplomacyState DominantRelationship = EDiplomacyState::None;
	/** Uses the scheduler's target-owner defence front. Unknown while relevant defenders are unloaded or on clients. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bDefencePowerKnown = false;
	/** Estimated defence strength for the target owner's District. Check whether this value is known before using it. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") float DistrictDefencePower = 0.f;
	/** Explains why the requested context or action is unavailable. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") FText UnavailableReason;
};

/** Share one Place/faction binding across dialogue nodes, events and quest conditions. */
UCLASS(BlueprintType, meta=(DisplayName="Territory Situation Profile"))
class TERRITORYFRAMEWORK_API UTerritorySituationProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(Categories="Territory",
		ToolTip="One independent Place. Its authored parents supply District and City context. Duplicate this profile to reuse a dialogue pattern for another Place."))
	FGameplayTag Territory;
	/** Faction checked by default. Narrative Target uses the pawn passed to the condition (normally the player's Tales pawn), not the speaking NPC or the Place's contesting faction. Controller Pawn uses the requesting controller. Explicit Faction uses the tag below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation")
	ETerritoryCaptureFactionSource FactionSource = ETerritoryCaptureFactionSource::NarrativeTargetFaction;
	/** Exact Narrative faction used when Faction Source is Explicit Faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(Categories="Narrative.Factions",
		EditCondition="FactionSource == ETerritoryCaptureFactionSource::ExplicitFaction", EditConditionHides))
	FGameplayTag ExplicitFaction;

	/** Read ownership history, diplomacy, Place share and known defence power for the supplied player context. Does not start combat or capture. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue", meta=(DisplayName="Inspect Territory Situation"))
	FTerritorySituationReport InspectSituation(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* Tales, ETerritorySituationScope Scope, FGameplayTag FactionOverride = FGameplayTag()) const;

	FGameplayTag ResolveRequestingFaction(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* Tales) const;

	/** Strict, deterministic hierarchy read shared by runtime and malformed-directory tests. */
	static bool ReadPlaceHoldings(TConstArrayView<FReplicatedCaptureSummary> Rows,
		FGameplayTag ScopeTag, FGameplayTag Faction, FTerritorySituationReport& Report,
		TArray<FReplicatedCaptureSummary>* OutPlaces = nullptr);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UENUM(BlueprintType)
enum class ETerritorySituationQuery : uint8
{
	ContextKnown,
	TargetAvailable,
	PreviouslyOwned UMETA(DisplayName="Faction Previously Owned Place"),
	RetakeNeeded UMETA(DisplayName="Faction Lost Place And Does Not Own It Now"),
	AlreadyOwned UMETA(DisplayName="Faction Owns Place Now"),
	HoldingsKnown,
	FactionPlaceCount UMETA(DisplayName="Faction's Secure Place Count"),
	FactionPlaceShare UMETA(DisplayName="Faction Share Of Available Places (%)"),
	FactionDominant UMETA(DisplayName="Faction Holds Strict Majority Of Places"),
	NoDominantFaction UMETA(DisplayName="Known Area Has No Dominant Faction"),
	RelationshipWithOwner,
	RelationshipWithDominant,
	DefencePowerKnown,
	DistrictDefencePower UMETA(DisplayName="Target Owner's District Defence Power"),
	FactionOwnedPlaceCount UMETA(DisplayName="Faction's Owned Place Count (Including Contested)",
		ToolTip="Counts Claimed and Contested Places still owned by this faction. Use this for owner reinforcements before handover. Secure Place Count excludes contested holdings.")
};

/** Native Narrative branch condition: combine several on a node for AND; alternative nodes provide OR. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, meta=(DisplayName="Territory Situation Condition"))
class TERRITORYFRAMEWORK_API UTerritorySituationCondition : public UNarrativeCondition
{
	GENERATED_BODY()
public:
	UTerritorySituationCondition();
	/** Reusable Place and faction context shared by related planning, retake and handover conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation")
	TObjectPtr<UTerritorySituationProfile> Profile;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(Categories="Narrative.Factions",
		ToolTip="Optional exact faction for this condition only. Empty uses the profile's Faction Source. Example: Narrative.Factions.Bandits checks Bandit holdings without changing the player's capture faction."))
	FGameplayTag FactionOverride;
	/** Select the live Territory fact this condition compares; related fields become relevant for that query. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation")
	ETerritorySituationQuery Query = ETerritorySituationQuery::RetakeNeeded;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(
		ToolTip="Used for holdings, share and dominant-faction queries. Defence power always uses the target owner's District front, as the assault scheduler does."))
	ETerritorySituationScope Scope = ETerritorySituationScope::District;
	/** Numeric comparison used between the live value and the authored threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(EditCondition="Query == ETerritorySituationQuery::FactionPlaceCount || Query == ETerritorySituationQuery::FactionOwnedPlaceCount || Query == ETerritorySituationQuery::FactionPlaceShare || Query == ETerritorySituationQuery::DistrictDefencePower", EditConditionHides))
	ETerritoryFloatComparison Comparison = ETerritoryFloatComparison::AtLeast;
	/** Nonnegative threshold for the selected count, percentage or defence-power query. Shares use 0 to 100 percent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(ClampMin="0.0", EditCondition="Query == ETerritorySituationQuery::FactionPlaceCount || Query == ETerritorySituationQuery::FactionOwnedPlaceCount || Query == ETerritorySituationQuery::FactionPlaceShare || Query == ETerritorySituationQuery::DistrictDefencePower", EditConditionHides))
	float Value = 1.f;
	/** Diplomacy state required by the selected relationship query. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(EditCondition="Query == ETerritorySituationQuery::RelationshipWithOwner || Query == ETerritorySituationQuery::RelationshipWithDominant", EditConditionHides))
	EDiplomacyState Relationship = EDiplomacyState::War;

	bool MatchesReport(const FTerritorySituationReport& Report) const;
protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
