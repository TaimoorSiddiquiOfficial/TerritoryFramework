#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryStealthProfile.h"
#include "GameplayTagContainer.h"
#include "Tales/NarrativeCondition.h"
#include "TerritoryStealthConditions.generated.h"

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
	meta=(DisplayName="Territory Stealth Policy Condition"))
class TERRITORYFRAMEWORK_API UTerritoryStealthPolicyCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryStealthPolicyCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(Categories="Territory", ToolTip="Optional exact Territory. Empty uses the containing state-config Territory or the Territory containing the explicit target."))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition")
	bool bRequireEnabled = true;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

/** Checks the server-owned awareness state for the explicit Narrative target. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew,
	meta=(DisplayName="Territory Exposure Condition"))
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
	meta=(DisplayName="Territory Stealth Evidence Condition"))
class TERRITORYFRAMEWORK_API UTerritoryStealthEvidenceCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritoryStealthEvidenceCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(Categories="Territory"))
	FGameplayTag TerritoryToCheck;

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
	meta=(DisplayName="Territory Suspicion Condition"))
class TERRITORYFRAMEWORK_API UTerritorySuspicionCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	UTerritorySuspicionCondition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition", meta=(Categories="Territory"))
	FGameplayTag TerritoryToCheck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory Condition",
		meta=(ClampMin="0.0", ClampMax="100.0", Units="Percent"))
	float MinimumSuspicionPercent = 50.f;

protected:
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller,
		class UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
