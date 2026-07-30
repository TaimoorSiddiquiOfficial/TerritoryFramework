#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "TerritoryAssaultActivity.generated.h"

/** Narrative activity that moves toward a Territory goal and can be interrupted by combat. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryAssaultActivity : public UNPCActivity
{
	GENERATED_BODY()

public:
	UTerritoryAssaultActivity(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Territory|Assault", meta=(ClampMin="10.0"))
	float AcceptanceRadius = 150.f;

	/** Explicit contract query used by validation/tests without exposing Narrative internals. */
	bool SupportsAssaultGoals() const;

protected:
	virtual bool RunActivity() override;
	virtual bool EndActivity() override;
};
