#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativePartyComponent.h"
#include "TerritoryNarrativePartyComponent.generated.h"

/** Native shared Tales component with server checks for who may choose a reply.
 * Uses Narrative's Party Dialogue Control Policy, members, leader and dialogue.
 * Use Territory Narrative Party, or this component on a custom replicated party actor.
 */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent), DisplayName="Territory Narrative Party Component")
class TERRITORYFRAMEWORK_API UTerritoryNarrativePartyComponent : public UNarrativePartyComponent
{
	GENERATED_BODY()
public:
	/** Keep Native's local viewing player. A server with only remote members uses the Native leader. */
	virtual APlayerController* GetOwningController() const override;
	/** Join through Native only after the old party confirms departure. Requires a valid
	 * server controller and PlayerState in this world. A repeated join returns false.
	 * Story callbacks may change membership again; success means this party still owns it.
	 */
	virtual bool AddPartyMember(UTalesComponent* Member) override;
	/** Clear only the departing member's shared-dialogue alias before Native publishes Leave Party.
	 * The remaining members keep the same Native dialogue. Camera/tag and leader transfer are separate concerns.
	 */
	virtual bool RemovePartyMember(UTalesComponent* Member) override;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TrySelectDialogueOption(UDialogueNode_Player* Option) override;
	virtual void SelectDialogueOption(UDialogueNode_Player* Option, APlayerState* Selector = nullptr) override;
	virtual void ServerSelectDialogueOption_Implementation(const FName& OptionID) override;

	/** Server query: can this current party member choose replies under Narrative's policy?
	 * Does not check whether a dialogue is ready. Clients use Narrative's replicated member states for UI.
	 */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category="Territory|Dialogue")
	bool CanMemberChooseDialogueReply(APlayerState* Member) const;

private:
	bool CanAddMember(const UTalesComponent* Member) const;
	// Native actor fields are a read model of the component, including inside its callbacks.
	void RefreshActorMembership(UTalesComponent* Joining = nullptr, UTalesComponent* Leaving = nullptr);
	// Derived from Native replies-available/start/finish events; never saved or replicated.
	TWeakObjectPtr<UDialogue> ReadyDialogue;
	bool bSelectingAutomaticReply = false;
	bool bReplyEventsBound = false;
	void UnbindReplyEvents();
	UFUNCTION()
	void HandleRepliesAvailable(UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& Replies);
	UFUNCTION()
	void HandleDialogueBegan(UDialogue* Dialogue);
	UFUNCTION()
	void HandleDialogueFinished(UDialogue* Dialogue, bool bStartingNewDialogue, EExitDialogueReason Reason);
	UFUNCTION()
	void HandleNPCLineStarted(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker);
};
