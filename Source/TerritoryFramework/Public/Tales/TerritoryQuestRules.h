#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryQuestRules.generated.h"

class UQuest;
class UTalesComponent;

/** Narrative Pro quest state inspected by Territory conditions and counter rules. */
UENUM(BlueprintType)
enum class ETerritoryQuestStateRequirement : uint8
{
	NotStarted UMETA(DisplayName="Not Started"),
	InProgress UMETA(DisplayName="In Progress"),
	Succeeded UMETA(DisplayName="Succeeded"),
	Failed UMETA(DisplayName="Failed"),
	Finished UMETA(DisplayName="Finished (Success or Failure)"),
	StartedOrFinished UMETA(DisplayName="Started at Any Time")
};

/** Small adapter over Narrative Pro's saved and replicated Tales quest state. */
UCLASS()
class TERRITORYFRAMEWORK_API UTerritoryQuestRulesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Territory|Tales|Quest",
		meta=(DisplayName="Does Narrative Quest State Match"))
	static bool DoesQuestStateMatch(const UTalesComponent* TalesComponent,
		TSubclassOf<UQuest> QuestClass, ETerritoryQuestStateRequirement RequiredState);

	/** Pure truth-table helper used by runtime and native regression tests. */
	static bool DoesQuestStateMatchValues(ETerritoryQuestStateRequirement RequiredState,
		bool bStartedOrFinished, bool bInProgress, bool bSucceeded,
		bool bFailed, bool bFinished);
};
