#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TerritoryNarrativeSequencerEditorLibrary.generated.h"

class ULevelSequence;

/** Result of normalizing Narrative Pro custom-track metadata on a project-owned sequence. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryNarrativeQTETrackRepairReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory|Cinematics|Editor")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Cinematics|Editor")
	int32 QTETracksFound = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Cinematics|Editor")
	int32 InvalidSupportedSectionOverridesRemoved = 0;

	UPROPERTY(BlueprintReadOnly, Category="Territory|Cinematics|Editor")
	TArray<FText> Errors;
};

/** Editor-only repair helpers for project-owned sequences which reuse Narrative Pro tracks. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryNarrativeSequencerEditorLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Restore every Narrative QTE track in Sequence to the vendor track class's section contract.
	 *
	 * BP_QTETrack uses DefaultSectionType=BP_QTESection and intentionally leaves
	 * SupportedSections empty. A copied track can retain unrelated Input/UI section classes as an
	 * instance override, preventing its QTE track instance from presenting the CommonUI prompt.
	 * This operation removes only that invalid project-asset override; it never changes Narrative
	 * Pro content or authored QTE sections/options.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Territory|Cinematics|Editor",
		meta=(DisplayName="Normalize Narrative QTE Track Configuration"))
	static FTerritoryNarrativeQTETrackRepairReport NormalizeNarrativeQTETrackConfiguration(
		ULevelSequence* Sequence);
};
