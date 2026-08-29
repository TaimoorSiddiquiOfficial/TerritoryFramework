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

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth", meta=(Categories="Territory"))
	FGameplayTag TerritoryTag;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	ETerritoryStealthEvidence Evidence = ETerritoryStealthEvidence::None;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector InvestigationLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	FVector EstimatedSourceDirection = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Territory|Stealth")
	TWeakObjectPtr<AActor> SuspectedSource;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth")
	bool bIdentityConfirmed = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Stealth", meta=(ClampMin="10.0", Units="cm"))
	float AcceptanceRadius = 150.f;

	void Refresh(const FGameplayTag& InTerritoryTag,
		ETerritoryStealthEvidence InEvidence, const FVector& InLocation,
		const FVector& InEstimatedDirection, AActor* InSuspectedSource,
		bool bInIdentityConfirmed, float InLifetime, float InAcceptanceRadius);

	virtual FString GetDebugString_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};
