#include "Tales/TerritoryNarrativePartyComponent.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Tales/NarrativeDialogueSettings.h"

void UTerritoryNarrativePartyComponent::OnRegister()
{
	Super::OnRegister();
	ReadyDialogue.Reset();
	OnDialogueBegan.AddUniqueDynamic(this, &ThisClass::HandleDialogueBegan);
	OnDialogueFinished.AddUniqueDynamic(this, &ThisClass::HandleDialogueFinished);
	OnDialogueRepliesAvailable.AddUniqueDynamic(this, &ThisClass::HandleRepliesAvailable);
	OnNPCDialogueLineStarted.AddUniqueDynamic(this, &ThisClass::HandleNPCLineStarted);
	bReplyEventsBound = true;
}

void UTerritoryNarrativePartyComponent::OnUnregister()
{
	UnbindReplyEvents();
	Super::OnUnregister();
}

void UTerritoryNarrativePartyComponent::UnbindReplyEvents()
{
	bReplyEventsBound = false;
	ReadyDialogue.Reset();
	OnDialogueBegan.RemoveDynamic(this, &ThisClass::HandleDialogueBegan);
	OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
	OnDialogueRepliesAvailable.RemoveDynamic(this, &ThisClass::HandleRepliesAvailable);
	OnNPCDialogueLineStarted.RemoveDynamic(this, &ThisClass::HandleNPCLineStarted);
}

void UTerritoryNarrativePartyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindReplyEvents();
	Super::EndPlay(EndPlayReason);
}

bool UTerritoryNarrativePartyComponent::CanMemberChooseDialogueReply(APlayerState* Member) const
{
	if (!HasAuthority() || !IsValid(Member)) return false;
	const APlayerController* Controller = Cast<APlayerController>(Member->GetOwningController());
	if (!IsValid(Controller) || !Controller->HasAuthority() || Controller->PlayerState != Member
		|| Controller->GetWorld() != GetWorld()) return false;
	for (UTalesComponent* Candidate : PartyMembers)
	{
		if (IsValid(Candidate) && Candidate->HasAuthority() && Candidate->GetParty() == this
			&& Candidate->GetOwningController() == Controller)
		{
			switch (PartyDialogueControlPolicy)
			{
			case EPartyDialogueControlPolicy::PartyLeaderControlled: return Candidate == GetPartyLeader();
			case EPartyDialogueControlPolicy::AllPlayers: return true;
			default: return false;
			}
		}
	}
	return false;
}

void UTerritoryNarrativePartyComponent::SelectDialogueOption(UDialogueNode_Player* Option, APlayerState* Selector)
{
	UDialogue* Dialogue = GetCurrentDialogue();
	if (!bReplyEventsBound || !CanMemberChooseDialogueReply(Selector) || !IsValid(Dialogue) || !Dialogue->IsInitialized()
		|| Dialogue->OwningComp != this || !Dialogue->NPCReplyChain.IsEmpty()
		|| (!bSelectingAutomaticReply && ReadyDialogue != Dialogue)
		|| !Dialogue->CanSelectDialogueOption(Option)) return;

	// Consume readiness before Native runs line events, which can re-enter Tales.
	ReadyDialogue.Reset();
	Super::SelectDialogueOption(Option, Selector);
}

void UTerritoryNarrativePartyComponent::TrySelectDialogueOption(UDialogueNode_Player* Option)
{
	// Native NPCFinishedTalking calls this on the party for authored auto replies.
	// Personal Tales still handles player intent and derives Selector from its PC.
	if (!HasAuthority() || !IsValid(Option)) return;
	UDialogue* Dialogue = GetCurrentDialogue();
	UTalesComponent* Leader = GetPartyLeader();
	APlayerController* Controller = IsValid(Leader) ? Leader->GetOwningController() : nullptr;
	if (!IsValid(Dialogue) || !IsValid(Controller)) return;
	const auto* Settings = GetDefault<UNarrativeDialogueSettings>();
	const bool bAutomatic = Option->IsAutoSelect() || Settings->bAutoSelectSingleResponse
		|| (Dialogue->AvailableResponses.Num() == 1 && Option->IsAutoSelectIfOnlyReply());
	TGuardValue<bool> AutomaticScope(bSelectingAutomaticReply, bAutomatic);
	SelectDialogueOption(Option, Controller->PlayerState);
}

void UTerritoryNarrativePartyComponent::ServerSelectDialogueOption_Implementation(const FName& OptionID)
{
	// Deliberately reject this inherited party RPC: it carries no authenticated
	// member identity. Even if a custom party actor has a network owner, intent
	// must arrive through that player's personal Tales ServerSelectDialogueOption.
	(void)OptionID;
}

void UTerritoryNarrativePartyComponent::HandleRepliesAvailable(UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& Replies)
{
	if (HasAuthority() && Dialogue && Dialogue == GetCurrentDialogue() && Dialogue->OwningComp == this
		&& Dialogue->IsInitialized() && !Replies.IsEmpty()) ReadyDialogue = Dialogue;
}

void UTerritoryNarrativePartyComponent::HandleDialogueBegan(UDialogue* Dialogue)
{
	if (Dialogue == GetCurrentDialogue()) ReadyDialogue.Reset();
}

void UTerritoryNarrativePartyComponent::HandleDialogueFinished(UDialogue* Dialogue, bool bStartingNewDialogue, EExitDialogueReason Reason)
{
	(void)bStartingNewDialogue;
	(void)Reason;
	if (ReadyDialogue == Dialogue) ReadyDialogue.Reset();
}

void UTerritoryNarrativePartyComponent::HandleNPCLineStarted(UDialogue* Dialogue, UDialogueNode_NPC* Node,
	const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
	(void)Node;
	(void)Line;
	(void)Speaker;
	if (Dialogue == GetCurrentDialogue()) ReadyDialogue.Reset();
}
