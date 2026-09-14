#pragma once

#include "Tales/Dialogue.h"
#include "Tales/TalesComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "TerritoryPartyReplyProbe.generated.h"

/** Editor-only fixture: holds both NPC lines and the selected reply until explicitly skipped. */
UCLASS()
class UTerritoryPartyReplyTestDialogue : public UDialogue
{
	GENERATED_BODY()
public:
	virtual bool Initialize(UTalesComponent* Component, const FDialoguePlayParams Params) override;
};

/** Observes Native member RPC dispatch without replaying a client line on the server fixture. */
UCLASS()
class UTerritoryPartyReplyMemberProbe : public UTalesComponent
{
	GENERATED_BODY()
public:
	int32 ReplyDispatches = 0;
	TWeakObjectPtr<APlayerState> LastSelector;
	virtual void ClientSelectDialogueOption_Implementation(const FName& OptionID, APlayerState* Selector) override
	{
		++ReplyDispatches;
		LastSelector = Selector;
	}
};

/** Native PIE dispatch avoids re-entering network gameplay from an editor Python stack. */
UCLASS(NotBlueprintable, Transient)
class ATerritoryPartyReplyTestDriver : public AActor
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Test")
	bool ChooseReply(UTalesComponent* Member, FName OptionID);
	UFUNCTION(BlueprintCallable, Category="Test")
	bool SkipLine(UTalesComponent* Member);
};
