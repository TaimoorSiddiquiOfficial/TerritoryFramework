#pragma once

#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "TerritoryConversationActivity.generated.h"

/** Uses Native activity selection and movement; reaching the point holds until the meeting releases its goal. */
UCLASS(BlueprintType, Blueprintable, EditInlineNew)
class TERRITORYFRAMEWORK_API UTerritoryConversationActivity : public UNPCActivity
{
	GENERATED_BODY()
public:
	UTerritoryConversationActivity(const FObjectInitializer& Initializer);
protected:
	virtual bool RunActivity() override;
	virtual bool EndActivity() override;
};
