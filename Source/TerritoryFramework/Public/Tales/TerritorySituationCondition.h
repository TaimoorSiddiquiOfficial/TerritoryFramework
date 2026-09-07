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
	City UMETA(DisplayName="Place's City")
};

/** A disposable read of existing authorities, never a second saved campaign state. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritorySituationReport
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bContextKnown = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bTargetAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bPreviouslyOwned = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bRetakeNeeded = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag RequestingFaction;
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag CurrentOwner;
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag ScopeTerritory;
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bHoldingsKnown = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") int32 AvailablePlaces = 0;
	UPROPERTY(BlueprintReadOnly, Category="Situation") int32 FactionPlaces = 0;
	UPROPERTY(BlueprintReadOnly, Category="Situation") float FactionSharePercent = 0.f;
	/** Strict majority of available Places; ties/fragmented control have no dominant faction. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") FGameplayTag DominantFaction;
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bOwnerRelationshipKnown = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") EDiplomacyState OwnerRelationship = EDiplomacyState::None;
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bDominantRelationshipKnown = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") EDiplomacyState DominantRelationship = EDiplomacyState::None;
	/** Uses the scheduler's target-owner defence front. Unknown while relevant defenders are unloaded or on clients. */
	UPROPERTY(BlueprintReadOnly, Category="Situation") bool bDefencePowerKnown = false;
	UPROPERTY(BlueprintReadOnly, Category="Situation") float DistrictDefencePower = 0.f;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation")
	ETerritoryCaptureFactionSource FactionSource = ETerritoryCaptureFactionSource::NarrativeTargetFaction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(Categories="Narrative.Factions",
		EditCondition="FactionSource == ETerritoryCaptureFactionSource::ExplicitFaction", EditConditionHides))
	FGameplayTag ExplicitFaction;

	UFUNCTION(BlueprintPure, Category="Territory|Dialogue", meta=(DisplayName="Inspect Territory Situation"))
	FTerritorySituationReport InspectSituation(APawn* NarrativeTarget, APlayerController* Controller,
		UTalesComponent* Tales, ETerritorySituationScope Scope) const;

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
	FactionPlaceCount,
	FactionPlaceShare UMETA(DisplayName="Faction Share Of Available Places (%)"),
	FactionDominant UMETA(DisplayName="Faction Holds Strict Majority Of Places"),
	NoDominantFaction UMETA(DisplayName="Known Area Has No Dominant Faction"),
	RelationshipWithOwner,
	RelationshipWithDominant,
	DefencePowerKnown,
	DistrictDefencePower UMETA(DisplayName="Target Owner's District Defence Power")
};

/** Native Narrative branch condition: combine several on a node for AND; alternative nodes provide OR. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, meta=(DisplayName="Territory Situation Condition"))
class TERRITORYFRAMEWORK_API UTerritorySituationCondition : public UNarrativeCondition
{
	GENERATED_BODY()
public:
	UTerritorySituationCondition();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation")
	TObjectPtr<UTerritorySituationProfile> Profile;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation")
	ETerritorySituationQuery Query = ETerritorySituationQuery::RetakeNeeded;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(
		ToolTip="Used for holdings, share and dominant-faction queries. Defence power always uses the target owner's District front, as the assault scheduler does."))
	ETerritorySituationScope Scope = ETerritorySituationScope::District;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(EditCondition="Query == ETerritorySituationQuery::FactionPlaceCount || Query == ETerritorySituationQuery::FactionPlaceShare || Query == ETerritorySituationQuery::DistrictDefencePower", EditConditionHides))
	ETerritoryFloatComparison Comparison = ETerritoryFloatComparison::AtLeast;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(ClampMin="0.0", EditCondition="Query == ETerritorySituationQuery::FactionPlaceCount || Query == ETerritorySituationQuery::FactionPlaceShare || Query == ETerritorySituationQuery::DistrictDefencePower", EditConditionHides))
	float Value = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Situation", meta=(EditCondition="Query == ETerritorySituationQuery::RelationshipWithOwner || Query == ETerritorySituationQuery::RelationshipWithDominant", EditConditionHides))
	EDiplomacyState Relationship = EDiplomacyState::War;

	bool MatchesReport(const FTerritorySituationReport& Report) const;
protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
