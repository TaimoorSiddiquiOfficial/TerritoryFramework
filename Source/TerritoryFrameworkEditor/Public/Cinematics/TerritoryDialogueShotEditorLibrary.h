#pragma once

#include "Cinematics/TerritoryDialogueShot.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryDialogueShotEditorLibrary.generated.h"

class UDialogueBlueprint;
class ULevelSequence;

/** Result of creating and optionally applying the reusable Territory dialogue shot pack. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORKEDITOR_API FTerritoryDialogueShotPackBuildReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="True only when every requested AAA dialogue sequence was created or repaired and all required checks passed."))
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="True when the selected Narrative Dialogue Blueprint compiled with Territory default, speaker, and reply-selection shots."))
	bool bDialogueConfigured = false;

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="Long package folder containing the reusable Level Sequences. Recommended: /Game/TerritoryFramework/Cinematics/Dialogue/Shots."))
	FString DestinationContentPath;

	/** Sequence assets and ShotRoles use matching indices. */
	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="Generated Level Sequences. Every sequence contains a Cinecam spawnable, an explicit Spawn track, a full-length Camera Cut, and a static authored transform."))
	TArray<TObjectPtr<ULevelSequence>> SequenceAssets;

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="Shot purpose matching Sequence Assets by array index: establishing, two-shot, over-shoulder, medium close-up, close-up, reaction, and insert."))
	TArray<ETerritoryDialogueShotRole> ShotRoles;

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="Artist guidance matching Sequence Assets and Shot Roles by array index. Each entry explains the strongest editorial use for that shot."))
	TArray<FText> ShotGuidance;

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="Blocking authoring problems. Fix every entry before using the pack in a dialogue."))
	TArray<FText> Errors;

	UPROPERTY(BlueprintReadOnly, Category="Territory Dialogue Shots",
		meta=(ToolTip="Non-blocking recommendations that should be reviewed by the cinematic designer."))
	TArray<FText> Warnings;
};

/**
 * Editor authoring bridge for Narrative Pro dialogue cinematics. It creates a
 * project-owned set of static, tagged CineCamera sequences and installs inline
 * UTerritoryDialogueShot defaults on Narrative Dialogue Blueprints.
 */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryDialogueShotEditorLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Create a stable seven-shot Level Sequence pack, reusing existing assets in the folder. */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Dialogue|Editor",
		meta=(DisplayName="Create Territory AAA Dialogue Shot Pack",
			ToolTip="Creates or repairs seven 24 fps Narrative-ready Level Sequences. Each has a Cinecam spawnable, explicit full-range Spawn track, Camera Cut, role-specific lens, focus, aperture, and composition. Recommended destination: /Game/TerritoryFramework/Cinematics/Dialogue/Shots."))
	static FTerritoryDialogueShotPackBuildReport CreateStudioDialogueShotPack(
		UPARAM(meta=(ToolTip="Unreal content folder for project-owned shot assets. Use /Game, never the Narrative Pro vendor plugin."))
		const FString& DestinationContentPath);

	/** Apply a generated shot pack to a Narrative Dialogue Blueprint without touching line-level shots. */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Dialogue|Editor",
		meta=(DisplayName="Apply Territory AAA Shot Pack To Dialogue",
			ToolTip="Assigns safe master, speaker, and reply-selection defaults to a Narrative Dialogue Blueprint, enables cinematic bars and movement control, and preserves every authored line-level shot."))
	static FTerritoryDialogueShotPackBuildReport ApplyStudioDialogueShotPack(
		UPARAM(meta=(ToolTip="Project-owned Narrative Dialogue Blueprint to configure."))
		UDialogueBlueprint* DialogueBlueprint,
		UPARAM(meta=(ToolTip="Successful result returned by Create Territory AAA Dialogue Shot Pack."))
		const FTerritoryDialogueShotPackBuildReport& ShotPack);

	/** Convenience operation used by content authors and automation. */
	UFUNCTION(BlueprintCallable, Category="Territory|Narrative Dialogue|Editor",
		meta=(DisplayName="Create And Apply Territory AAA Dialogue Shot Pack",
			ToolTip="One-click authoring path: creates or repairs the complete camera pack and applies the recommended defaults to the selected project-owned Narrative Dialogue Blueprint."))
	static FTerritoryDialogueShotPackBuildReport CreateAndApplyStudioDialogueShotPack(
		UPARAM(meta=(ToolTip="Project-owned Narrative Dialogue Blueprint. Vendor assets are reference-only."))
		UDialogueBlueprint* DialogueBlueprint,
		UPARAM(meta=(ToolTip="Recommended: /Game/TerritoryFramework/Cinematics/Dialogue/Shots."))
		const FString& DestinationContentPath);
};
