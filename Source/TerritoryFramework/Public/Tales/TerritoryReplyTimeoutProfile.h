#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TerritoryReplyTimeoutProfile.generated.h"

class UDialogue;

/** Per-dialogue override of the reply countdown. Zero seconds disables the timer for that dialogue. */
USTRUCT(BlueprintType)
struct TERRITORYFRAMEWORK_API FTerritoryReplyTimeoutRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Dialogue this rule applies to. Leave empty to keep the rule unused."))
	TSoftObjectPtr<UDialogue> Dialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Seconds the player has to answer in this dialogue. Zero disables the countdown here.",
			ClampMin="0.0", UIMin="0.0", UIMax="60.0"))
	float Seconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Node id of the silent reply in this dialogue. Leave empty to use the profile default."))
	FName SilentReplyID;
};

/**
 * Server-owned reply countdown policy. When a dialogue publishes its replies, the party component starts
 * one timer on the server; if the player has not answered by then, the authored silence reply is selected
 * through Native's normal server selection path so authority, party rules and Narrative events stay intact.
 * Empty profile, or zero seconds, keeps the original behaviour: the reply UI waits indefinitely.
 */
UCLASS(BlueprintType)
class TERRITORYFRAMEWORK_API UTerritoryReplyTimeoutProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Master switch for the countdown. Disable to keep every dialogue waiting for a real answer."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Seconds the player has to answer before the silent reply is selected. Zero disables the countdown.",
			ClampMin="0.0", UIMin="0.0", UIMax="60.0"))
	float DefaultSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Node id of the authored silence option. It must exist among the replies the dialogue offers, otherwise no timer starts."))
	FName SilentReplyID = TEXT("SayNothing");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reply Timeout",
		meta=(ToolTip="Optional per-dialogue rules. A matching rule replaces the default seconds and silence id."))
	TArray<FTerritoryReplyTimeoutRule> DialogueOverrides;

	/** Seconds the player gets in this dialogue. Zero means no countdown. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout")
	float ResolveSeconds(const UDialogue* Dialogue) const;

	/** Node id the countdown selects when it expires. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout")
	FName ResolveSilentReplyID(const UDialogue* Dialogue) const;

	/** True when this dialogue should run a countdown at all. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout")
	bool IsEnabledForDialogue(const UDialogue* Dialogue) const;
};
