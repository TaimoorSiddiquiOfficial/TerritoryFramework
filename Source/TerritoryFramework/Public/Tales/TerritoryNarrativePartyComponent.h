#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativePartyComponent.h"
#include "Tales/TerritoryReplyTimeoutProfile.h"
#include "TerritoryNarrativePartyComponent.generated.h"

UENUM(BlueprintType)
enum class ETerritoryPartyOwnerDeparturePolicy : uint8
{
	ContinueWhenSupported UMETA(DisplayName="Continue with compatible dialogue adapter"),
	EndConversation UMETA(DisplayName="End the conversation safely")
};

/** Fired on the server when a reply countdown starts, so the HUD can draw it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerritoryReplyTimeoutStarted,
	float, Seconds, FName, SilentReplyID);

/** Fired when a reply countdown stops. True when it stopped by selecting the silent reply. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryReplyTimeoutEnded,
	bool, bSelectedSilentReply);

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
  virtual ANarrativePlayerController* GetOwningController() const override;
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

	/** Server-owned reply countdown policy. An empty profile, or zero seconds, keeps Native's behaviour
	 * of waiting for a real answer. Set it on the party component (or its Blueprint default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Territory|Dialogue|Reply Timeout",
		meta=(ToolTip="Optional reply countdown policy. With no profile, or zero seconds, the reply UI waits indefinitely like Native."))
	TObjectPtr<UTerritoryReplyTimeoutProfile> ReplyTimeoutProfile;

	/** True while the server is waiting out a reply countdown for the current dialogue. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout",
		meta=(ToolTip="True while a reply countdown is running on the server for the current dialogue."))
	bool IsReplyTimeoutActive() const;

	/** Seconds the player has left to answer, or zero when no countdown is running. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout",
		meta=(ToolTip="Seconds left before the silent reply is selected, or zero when no countdown is running."))
	float GetReplyTimeRemaining() const;

	/** Length of the countdown that is running, or zero when none is. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout",
		meta=(ToolTip="Total length of the running countdown in seconds, or zero when none is running."))
	float GetReplyTimeoutDuration() const;

	/** Node id the running countdown will select when it expires. */
	UFUNCTION(BlueprintPure, Category="Territory|Dialogue|Reply Timeout",
		meta=(ToolTip="Node id of the silence reply the running countdown will select. Empty when none is running."))
	FName GetPendingSilentReplyID() const;

	/** Server-only: resolve the running countdown immediately, exactly as the timer would.
	 * Handy for QA keys, skip inputs and automation; production relies on the timer. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Territory|Dialogue|Reply Timeout",
		meta=(ToolTip="Resolve the running reply countdown now, exactly as the timer would. Returns true when the silence reply was selected."))
	bool ResolveReplyCountdownNow();

	/** Fired on the server when a countdown starts. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Dialogue|Reply Timeout")
	FOnTerritoryReplyTimeoutStarted OnReplyTimeoutStarted;

	/** Fired when a countdown stops, with the reason it stopped. */
	UPROPERTY(BlueprintAssignable, Category="Territory|Dialogue|Reply Timeout")
	FOnTerritoryReplyTimeoutEnded OnReplyTimeoutEnded;

private:
	// Server-only reply countdown. Transient: never campaign state, never replicated.
	void StartReplyTimeout(UDialogue* Dialogue, class UTalesComponent* Owner,
		const TArray<UDialogueNode_Player*>& Replies);
	void CancelReplyTimeout(bool bSelectedSilentReply);
	void HandleReplyTimeoutExpired();
	bool CanResolveReplyTimeoutToSilence(const FName& SilentReplyID) const;
	/** Also listen to sibling Tales components on the same actor, so the countdown covers a plain
	 * Native dialogue that runs on the owner's own component instead of this party adapter. */
	void BindSiblingReplySources();
	FTimerHandle ReplyTimeoutHandle;
	TWeakObjectPtr<UDialogue> TimeoutDialogue;
	/** Component that owns the dialogue the countdown belongs to; the silence reply is selected there. */
	TWeakObjectPtr<UTalesComponent> TimeoutOwnerComponent;
	FName PendingSilentReplyID;
	float ReplyTimeoutSeconds = 0.f;

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
