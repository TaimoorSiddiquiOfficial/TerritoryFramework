#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryStealthProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryStealthConditions.generated.h"

UENUM(BlueprintType)
enum class ETerritoryDisguiseRequirement : uint8
{
	Active UMETA(DisplayName="Any Disguise Is Active"),
	PerceivedAsFaction UMETA(DisplayName="Perceived As Faction"),
	TrueFaction UMETA(DisplayName="True Faction Is"),
	CompromisedForFaction UMETA(DisplayName="Compromised For Faction"),
	AcceptedByTerritory UMETA(DisplayName="Accepted By Territory Security")
};

UENUM(BlueprintType)
enum class ETerritoryExposureRequirement : uint8
{
	Undetected UMETA(DisplayName="Undetected"),
	Suspicious UMETA(DisplayName="Suspicious / Investigating"),
	Exposed UMETA(DisplayName="Exposed"),
	ExposedOrStealthDisabled UMETA(DisplayName="Exposed Or Stealth Is Disabled",
		ToolTip="Recommended migration option for a Contested diplomacy event. Legacy Territories without a stealth profile still pass.")
};

/** Checks whether the active Data Asset or quest override permits infiltration. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Stealth Policy Condition",
		ToolTip="Server story check for the active stealth profile and its runtime override. Empty territory uses the containing state-config place, then the target location. This is not a client-side security read model."))
class TERRITORYFRAMEWORK_API UTerritoryStealthPolicyCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryStealthPolicyCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Optional exact Territory. Empty uses the containing state-config Territory or the Territory containing the explicit target."))
	FGameplayTag TerritoryToCheck;

	/** Require infiltration to be enabled; disable this option to test for infiltration being disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	bool bRequireEnabled = true;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks the server-owned awareness state for the explicit Narrative target. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Exposure Condition",
		ToolTip="Read server awareness for the exact Narrative target. Undetected, Suspicious and Exposed are separate states. Exposed Or Stealth Disabled also supports places without stealth. Missing infiltration records fail."))
class TERRITORYFRAMEWORK_API UTerritoryExposureCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryExposureCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Optional exact Territory. Empty uses the containing state-config Territory or target location."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ToolTip="Easy example: add Exposed Or Stealth Disabled to the Contested Set Diplomacy event. Walking inside stays peaceful; confirmed sight starts the existing War event."))
	ETerritoryExposureRequirement RequiredExposure =
		ETerritoryExposureRequirement::ExposedOrStealthDisabled;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks the latest sight, shot, damage, corpse, distraction, or scripted evidence. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Stealth Evidence Condition",
		ToolTip="Read the latest server evidence for the exact Narrative target. Choose sight, shot, damage or another kind and an age limit. Empty territory uses the containing place or target location. This does not search all past evidence."))
class TERRITORYFRAMEWORK_API UTerritoryStealthEvidenceCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryStealthEvidenceCondition();

	/** Stable tag of the Territory evaluated by this Narrative condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(Categories="Territory"))
	FGameplayTag TerritoryToCheck;

	/** Kind of recorded stealth evidence required for this condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryStealthEvidence RequiredEvidence = ETerritoryStealthEvidence::Sight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", Units="s", ToolTip="Zero accepts evidence of any age. Example: 5 accepts only a gunshot heard during the last five seconds."))
	float MaximumEvidenceAge = 5.f;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Compares the explicit target's current suspicion meter. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Suspicion Condition",
		ToolTip="Check the exact target suspicion on the server. Fifty means fifty percent. Empty territory uses the containing place or target location. Missing infiltration records fail."))
class TERRITORYFRAMEWORK_API UTerritorySuspicionCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritorySuspicionCondition();

	/** Stable tag of the Territory evaluated by this Narrative condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(Categories="Territory"))
	FGameplayTag TerritoryToCheck;

	/** Minimum current suspicion required, on a scale from 0 to 100 percent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MinimumSuspicionPercent = 50.f;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Dialogue/event condition for double-agent and uniform-based mission branches. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Disguise Condition",
		ToolTip="Check the target disguise or real faction through the existing disguise system. Disguise changes perceived identity, not political ownership. Security acceptance requires a loaded place and the server security rules."))
class TERRITORYFRAMEWORK_API UTerritoryDisguiseCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryDisguiseCondition();

	/** Choose which active, perceived, true-faction, compromised or security-acceptance disguise state to test. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	ETerritoryDisguiseRequirement Requirement =
		ETerritoryDisguiseRequirement::Active;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Narrative.Factions",
			ToolTip="Faction used by Perceived, True, or Compromised checks."))
	FGameplayTag Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory",
			ToolTip="Optional exact Territory for security acceptance. Empty uses the Place containing the player."))
	FGameplayTag TerritoryToCheck;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
