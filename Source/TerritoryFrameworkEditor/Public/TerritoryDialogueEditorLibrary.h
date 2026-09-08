#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryDialogueEditorLibrary.generated.h"

class UTerritoryDialogueRecipe;
class UDialogueBlueprint;
struct FTerritoryDialogueRecipeNode;

/** Uses Native editor graph classes, compiler and runtime node/condition/event types. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDialogueEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Add one new reply through Narrative's graph schema. The row must have no outgoing replies; connect those separately. Does not save. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Dialogue")
	static bool AddDialogueReply(UDialogueBlueprint* Dialogue, FName FromID,
		const FTerritoryDialogueRecipeNode& Reply, FText& OutError);

	/** Creates a new asset only. Never overwrites an existing dialogue or modifies Narrative Pro. Save explicitly after review. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Dialogue")
	static UDialogueBlueprint* CreateDialogueFromRecipe(UTerritoryDialogueRecipe* Recipe,
		const FString& NewAssetPath, FText& OutError);

	/** Arrange an existing Native dialogue in rows, keeping its reply priority and links. Does not save. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Dialogue")
	static bool ArrangeDialogue(UDialogueBlueprint* Dialogue, FText& OutError);

	/** Connect two existing runtime node IDs through Narrative's graph schema. Does not save or create nodes. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Dialogue")
	static bool ConnectDialogueReply(UDialogueBlueprint* Dialogue, FName FromID, FName ToID, FText& OutError);

	/** Move one existing node through Narrative's schema. Position can change reply priority. Does not save. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Dialogue")
	static bool SetDialogueNodePosition(UDialogueBlueprint* Dialogue, FName NodeID, FVector2D Position, FText& OutError);
};
