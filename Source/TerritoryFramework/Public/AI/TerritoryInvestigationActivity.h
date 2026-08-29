#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "TerritoryInvestigationActivity.generated.h"

/**
 * Narrative-native, interruptible search activity. Blueprint children may add
 * dialogue or a Behaviour Tree while keeping the Territory investigation goal contract.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryInvestigationActivity : public UNPCActivity
{
	GENERATED_BODY()

public:
	UTerritoryInvestigationActivity(const FObjectInitializer& ObjectInitializer);

	bool SupportsTerritoryInvestigationGoals() const;

protected:
	virtual bool RunActivity() override;
	virtual bool EndActivity() override;
};
