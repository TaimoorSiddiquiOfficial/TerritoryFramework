#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Core/TerritoryStealthProfile.h"
#include "GameplayTagContainer.h"
#include "TerritoryInvestigationGoal.generated.h"

/** Transient Narrative goal for a guard searching a sound, impact, distraction, or corpse. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryInvestigationGoal : public UNPCGoalItem
{
	GENERATED_BODY()

public:
	UTerritoryInvestigationGoal(const FObjectInitializer& ObjectInitializer);

	/** Stable Territory GameplayTag used by lookups, Narrative conditions and hierarchy references. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth", meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

	/** Kind of clue reported to the Territory stealth system. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryStealthEvidence Evidence = ETerritoryStealthEvidence::None;

	/** World-space location guards are being asked to investigate. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector InvestigationLocation = FVector::ZeroVector;

	/** Estimated direction toward the source of the reported evidence. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector EstimatedSourceDirection = FVector::ZeroVector;

	/** Actor suspected of causing the evidence; may be empty for an anonymous distraction. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory|Stealth")
	TWeakObjectPtr<AActor> SuspectedSource;

	/** Whether defenders have confirmed who caused the evidence. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	bool bIdentityConfirmed = false;

	/** Distance in centimetres at which movement considers the destination reached. */
	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth", meta=(ClampMin="10.0", Units="cm"))
	float AcceptanceRadius = 150.f;

	void Refresh(const FGameplayTag& InTerritoryTag,
		ETerritoryStealthEvidence InEvidence, const FVector& InLocation,
		const FVector& InEstimatedDirection, AActor* InSuspectedSource,
		bool bInIdentityConfirmed, float InLifetime, float InAcceptanceRadius);

	virtual FString GetDebugString_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};
