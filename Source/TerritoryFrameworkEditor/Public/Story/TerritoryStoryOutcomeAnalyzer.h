#pragma once

#include "CoreMinimal.h"

class UTerritoryDefinition;

/** How confidently the editor can describe one authored outcome without a live world. */
enum class ETerritoryStoryOutcomeCertainty : uint8
{
	Configured,
	RuntimeConditional,
	ChanceBased,
	CustomBlueprint,
	Warning
};

/** One compact, read-only story branch generated from a Territory Definition. */
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryStoryOutcomeScenario
{
	FString Category;
	FString Title;
	FString When;
	FString OnlyIf;
	FString Then;
	FString IfNot;
	FString AlsoAffects;
	FString Source;
	ETerritoryStoryOutcomeCertainty Certainty =
		ETerritoryStoryOutcomeCertainty::Configured;
};

/** Complete temporary report. It is never serialized back into the Definition. */
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryStoryOutcomeReport
{
	FString DefinitionName;
	FString DefinitionType;
	FString Headline;
	TArray<FTerritoryStoryOutcomeScenario> Scenarios;
	TArray<FString> ValidationErrors;
	TArray<FString> ValidationWarnings;

	int32 Count(ETerritoryStoryOutcomeCertainty Certainty) const;
	FString BuildPlainText() const;
};

/**
 * Pure authoring-time explanation of a Territory Definition.
 *
 * The analyzer never checks live Narrative conditions, executes Narrative events,
 * rolls assault probability, mutates ownership, or stores its result on the asset.
 */
class TERRITORYFRAMEWORKEDITOR_API FTerritoryStoryOutcomeAnalyzer
{
public:
	static FTerritoryStoryOutcomeReport Analyze(
		const UTerritoryDefinition* Definition,
		bool bIncludeFullValidation = true);

	static FString CertaintyLabel(ETerritoryStoryOutcomeCertainty Certainty);
};
