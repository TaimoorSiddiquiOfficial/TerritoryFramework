#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativePartyComponent.h"
#include "TerritoryNarrativePartyComponent.generated.h"

UENUM(BlueprintType)
enum class ETerritoryPartyOwnerDeparturePolicy : uint8
{
	ContinueWhenSupported UMETA(DisplayName="Continue with compatible dialogue adapter"),
	EndConversation UMETA(DisplayName="End the conversation safely")
};

/** Native shared Tales component with server checks for who may choose a reply.
 * Uses Narrative's Party Dialogue Control Policy, members, leader and dialogue.
 * Use Territory Narrative Party, or this component on a custom replicated party actor.
 */
UCLASS(ClassGroup=(Territory), meta=(BlueprintSpawnableComponent), DisplayName="Territory Narrative Party Component")
class TERRITORYFRAMEWORK_API UTerritoryNarrativePartyComponent : public UNarrativePartyComponent
{
	GENERATED_BODY()
public:
	/** Only applies when the dialogue's cached owning controller leaves. The default
	 * continues through Territory Party Dialogue's compatible adapter; otherwise
	 * Native ends before Leave callbacks. Departure during a spoken player reply
	 * ends for everyone: Native has no client speaker/camera handover RPC. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Parties")
	ETerritoryPartyOwnerDeparturePolicy OwnerDeparturePolicy = ETerritoryPartyOwnerDeparturePolicy::ContinueWhenSupported;
	/** Keep Native's local viewing player. A server with only remote members uses the Native leader. */
	virtual APlayerController* GetOwningController() const override;
	/** Join through Native only after the old party confirms departure. Requires a valid
	 * server controller and PlayerState in this world. A repeated join returns false.
	 * Active conversations reject new members; Native has no playback catch-up.
	 * Story callbacks may change membership again; success means this party still owns it.
	 */
	virtual bool AddPartyMember(UTalesComponent* Member) override;
	/** Release the departing member's alias and party tag grant before Leave Party.
	 * Owner departure uses OwnerDeparturePolicy. The final departure ends it through Native first.
	 */
	virtual bool RemovePartyMember(UTalesComponent* Member) override;
	/** Start a Native conversation for the current members. An empty party returns false. */
	virtual bool BeginDialogue(TSubclassOf<UDialogue> Dialogue, const FDialoguePlayParams PlayParams = FDialoguePlayParams()) override;
	virtual bool SetCurrentDialogue(TSubclassOf<UDialogue> Dialogue, const FDialoguePlayParams PlayParams = FDialoguePlayParams()) override;
	virtual void ExitDialogue(EExitDialogueReason Reason) override;

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
	// Records only Native's one party-member grant, never its separate avatar grant.
	// Connection/ASC references and the active dialogue are transient, not campaign data.
	TWeakObjectPtr<UDialogue> SpeakerTagDialogue;
	FGameplayTagContainer PartySpeakerTags;
	TMap<TWeakObjectPtr<APlayerState>, TWeakObjectPtr<class UAbilitySystemComponent>> PartySpeakerGrants;
	int32 DialogueMutationDepth = 0;
	bool bAllowNativeInitialSet = false;
	TWeakObjectPtr<UDialogue> DeferredExitDialogue;
	TOptional<EExitDialogueReason> DeferredExitReason;
	struct FPendingLogout
	{
		TWeakObjectPtr<UTalesComponent> Member;
		TWeakObjectPtr<APlayerState> State;
	};
	TArray<FPendingLogout> PendingLogouts;
	TSet<TWeakObjectPtr<AController>> ExitingControllers;
	FDelegateHandle LogoutHandle;
	bool bFlushingDeferredOperations = false;
	void HandleGameModeLogout(class AGameModeBase* GameMode, AController* Exiting);
	void UnbindLogout();
	void RecordNativePartySpeakerGrants();
	void PrepareNativeSpeakerCleanup();
	void FlushDeferredDialogueExit();
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
