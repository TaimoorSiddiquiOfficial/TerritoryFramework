#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryDialogueEditorLibrary.generated.h"

class UTerritoryDialogueRecipe;
class UDialogueBlueprint;

/** Uses Native editor graph classes, compiler and runtime node/condition/event types. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDialogueEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Creates a new asset only. Never overwrites an existing dialogue or modifies Narrative Pro. Save explicitly after review. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Dialogue")
	static UDialogueBlueprint* CreateDialogueFromRecipe(UTerritoryDialogueRecipe* Recipe,
		const FString& NewAssetPath, FText& OutError);
};
