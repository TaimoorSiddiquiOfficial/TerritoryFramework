#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Tales/Dialogue.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryDialogueRecipe.generated.h"

/** Authoring row only. The generated Narrative dialogue executes these conditions/events. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryDialogueRecipeNode
{
	GENERATED_BODY()
	/** Unique node identifier used by reply links and the dialogue root reference. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") FName ID;
	/** Create a player response node instead of an NPC dialogue node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") bool bPlayer = false;
	/** Configured Narrative NPC ID, or an ambient meeting role ID; identifies who speaks this line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue", meta=(EditCondition="!bPlayer", EditConditionHides)) FName SpeakerID;
	/** Dialogue line displayed or spoken by this node. The routing root may be silent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue", meta=(MultiLine="true")) FText Text;
	/** Smaller coordinates receive higher Native branch priority. Keep both axes ordered for either editor wiring mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") FVector2D Position = FVector2D::ZeroVector;
	/** Narrative conditions that must all pass before this node is eligible. Alternative reply nodes provide OR branches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category="Dialogue") TArray<TObjectPtr<UNarrativeCondition>> Conditions;
	/** Narrative events attached to this generated dialogue node and executed by the normal dialogue flow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category="Dialogue") TArray<TObjectPtr<UNarrativeEvent>> Events;
	/** Native AND within a node, OR through alternative replies. Empty means end the conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") TArray<FName> Replies;
};

/** Reusable authoring recipe. Build once in the editor, then edit/play an ordinary Narrative Dialogue Blueprint. */
UCLASS(BlueprintType, meta=(DisplayName="Territory Conditional Dialogue Recipe"))
class TERRITORYFRAMEWORK_API UTerritoryDialogueRecipe : public UDataAsset
{
	GENERATED_BODY()
public:
	UTerritoryDialogueRecipe();
	/** UDialogue preserves ordinary NPC/player recipes. TerritoryAmbientDialogue creates two-role ambient meetings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue", meta=(AllowAbstract="false"))
	TSubclassOf<UDialogue> ParentDialogueClass;
	/** First recruited actor's role. Used only when Parent Dialogue Class derives from TerritoryAmbientDialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue|Ambient Roles") FName FirstRoleID = TEXT("FirstGuard");
	/** Second recruited actor's distinct role. Neither meeting role may identify the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue|Ambient Roles") FName SecondRoleID = TEXT("SecondGuard");
	/** One NPC routing root. Give it empty Text for a silent entry point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") FName RootID;
	/** Ordinary dialogues only: Narrative NPC Definitions with distinct NPC IDs. Ambient meetings use role IDs instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") TArray<TObjectPtr<UNPCDefinition>> Speakers;
	/** Dialogue nodes and reply links used to generate an editable Native Dialogue Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue") TArray<FTerritoryDialogueRecipeNode> Nodes;

	/** Deterministic structural validation; no conditions/events are executed. */
	bool ValidateRecipe(FText& OutError) const;
	/** The selected parent determines the speaker contract; no separate mode can drift from it. */
	UFUNCTION(BlueprintPure, Category="Dialogue") bool UsesAmbientRoles() const;
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
