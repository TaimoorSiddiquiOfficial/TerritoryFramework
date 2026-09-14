#include "Tales/TerritoryNarrativePartyComponent.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Tales/NarrativeDialogueSettings.h"
#include "UnrealFramework/NarrativeParty.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "AbilitySystemComponent.h"

void UTerritoryNarrativePartyComponent::RecordNativePartySpeakerGrants()
{
	UDialogue* Dialogue = GetCurrentDialogue();
	if (!HasAuthority() || !IsValid(Dialogue) || !Dialogue->IsInitialized() || Dialogue->OwningComp != this) return;
	SpeakerTagDialogue = Dialogue;
	PartySpeakerTags = Dialogue->PlayerSpeakerInfo.OwnedTags;
	PartySpeakerGrants.Reset();
	// Membership is stable for Native's whole Begin call. Its Play/OnBegin has
	// applied this exact container once to each Native PlayerState ASC by now.
	for (APlayerState* State : PartyMemberStates)
	{
		if (ANarrativePlayerState* NativeState = Cast<ANarrativePlayerState>(State))
		{
			if (UAbilitySystemComponent* ASC = NativeState->GetAbilitySystemComponent())
			{
				PartySpeakerGrants.Add(State, ASC);
			}
		}
	}
}

void UTerritoryNarrativePartyComponent::PrepareNativeSpeakerCleanup()
{
	UDialogue* Tracked = SpeakerTagDialogue.Get();
	// Native normally removes grants for current members. Only a departed entry
	// needs our cleanup; clear the record before tag callbacks can re-enter Tales.
	auto Grants = MoveTemp(PartySpeakerGrants);
	const FGameplayTagContainer Tags = PartySpeakerTags;
	SpeakerTagDialogue.Reset();
	PartySpeakerTags.Reset();
	if (!HasAuthority() || !IsValid(Tracked) || !Tracked->IsInitialized()) return;
	for (const auto& Grant : Grants)
	{
		if (!PartyMemberStates.Contains(Grant.Key.Get()))
		{
			if (UAbilitySystemComponent* ASC = Grant.Value.Get())
			{
				ASC->RemoveLooseGameplayTags(Tags, 1, EGameplayTagReplicationState::CountToOwner);
			}
		}
	}
}

bool UTerritoryNarrativePartyComponent::BeginDialogue(TSubclassOf<UDialogue> Dialogue, const FDialoguePlayParams PlayParams)
{
	if (!HasAuthority() || DialogueMutationDepth) return false;
	bool bStarted = false;
	{
		TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
		TGuardValue<bool> InitialSet(bAllowNativeInitialSet, true);
		bStarted = Super::BeginDialogue(Dialogue, PlayParams);
		if (bStarted) RecordNativePartySpeakerGrants();
	}
	FlushDeferredDialogueExit();
	return bStarted;
}

bool UTerritoryNarrativePartyComponent::SetCurrentDialogue(TSubclassOf<UDialogue> Dialogue, const FDialoguePlayParams PlayParams)
{
	if (!HasAuthority()) return Super::SetCurrentDialogue(Dialogue, PlayParams);
	if (DialogueMutationDepth && !bAllowNativeInitialSet) return false;
	bAllowNativeInitialSet = false;
	bool bSet = false;
	{
		TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
		bSet = Super::SetCurrentDialogue(Dialogue, PlayParams);
		// Null/priority rejection keeps the old session and its exact grant record.
		// On an accepted replacement Native has already ended the old dialogue.
		if (!SpeakerTagDialogue.IsValid() || SpeakerTagDialogue != GetCurrentDialogue() || !SpeakerTagDialogue->IsInitialized())
		{
			PrepareNativeSpeakerCleanup();
		}
	}
	FlushDeferredDialogueExit();
	return bSet;
}

void UTerritoryNarrativePartyComponent::ExitDialogue(EExitDialogueReason Reason)
{
	if (!HasAuthority()) return;
	if (DialogueMutationDepth)
	{
		DeferredExitDialogue = GetCurrentDialogue();
		DeferredExitReason = Reason;
		return;
	}
	{
		TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
		PrepareNativeSpeakerCleanup();
		Super::ExitDialogue(Reason);
	}
	FlushDeferredDialogueExit();
}

void UTerritoryNarrativePartyComponent::FlushDeferredDialogueExit()
{
	if (DialogueMutationDepth || !DeferredExitReason.IsSet()) return;
	UDialogue* Requested = DeferredExitDialogue.Get();
	const EExitDialogueReason Reason = DeferredExitReason.GetValue();
	DeferredExitReason.Reset();
	DeferredExitDialogue.Reset();
	// A finish callback for an old dialogue must not close its replacement.
	if (IsValid(Requested) && Requested == GetCurrentDialogue() && Requested->IsInitialized()) ExitDialogue(Reason);
}

bool UTerritoryNarrativePartyComponent::CanAddMember(const UTalesComponent* Member) const
{
	if (DialogueMutationDepth || !HasAuthority() || !IsValid(Member) || !Member->HasAuthority()
		|| Member->IsA<UNarrativePartyComponent>() || Member->GetWorld() != GetWorld()
		|| Member->GetParty() == this || PartyMembers.Contains(Member)) return false;
	// Native does not synchronize an in-progress conversation to a newly joined
	// member. Reject before releasing their old party or applying unbalanced tags.
	if (SpeakerTagDialogue == GetCurrentDialogue() && SpeakerTagDialogue.IsValid()
		&& SpeakerTagDialogue->IsInitialized() && !PartySpeakerTags.IsEmpty()) return false;
	const APlayerController* Controller = Member->GetOwningController();
	const APlayerState* State = IsValid(Controller) ? Controller->PlayerState.Get() : nullptr;
	if (!IsValid(Controller) || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()
		|| !IsValid(State) || !State->HasAuthority() || State->GetWorld() != GetWorld()
		|| State->GetOwningController() != Controller || PartyMemberStates.Contains(State)) return false;
	if (const ANarrativePlayerController* NarrativeController = Cast<ANarrativePlayerController>(Controller))
	{
		// Narrative routes client intent through this exact personal Tales slot.
		// A second component must not register the same player in another party.
		if (NarrativeController->GetTalesComponent() != Member) return false;
	}
	// Native's party actor exposes Narrative PlayerStates to clients. A custom actor
	// with just the component may use another PlayerState class, as Native supports.
	if (GetOwner()->IsA<ANarrativeParty>() && !State->IsA<ANarrativePlayerState>()) return false;
	for (const UTalesComponent* Existing : PartyMembers)
	{
		if (IsValid(Existing) && Existing->GetOwningController() == Controller) return false;
	}
	return true;
}

namespace
{

void RefreshNativeActorMembership(UNarrativePartyComponent* Party, UTalesComponent* Joining = nullptr, UTalesComponent* Leaving = nullptr)
{
	ANarrativeParty* Actor = IsValid(Party) ? Cast<ANarrativeParty>(Party->GetOwner()) : nullptr;
	if (!IsValid(Actor) || !Party->HasAuthority() || Actor->PartyTalesComponent != Party) return;
	Actor->PartyMembers.Reset();
	Actor->PartyMemberControllers.Reset();
	const auto AddReadModel = [Actor](UTalesComponent* Member)
	{
		APlayerController* Controller = IsValid(Member) ? Member->GetOwningController() : nullptr;
		ANarrativePlayerState* State = IsValid(Controller) ? Cast<ANarrativePlayerState>(Controller->PlayerState) : nullptr;
		if (IsValid(State))
		{
			Actor->PartyMembers.AddUnique(State);
			Actor->PartyMemberControllers.Add(Controller);
		}
	};
	for (UTalesComponent* Member : Party->GetPartyMembers())
	{
		if (Member != Leaving) AddReadModel(Member);
	}
	if (Joining) AddReadModel(Joining);
	Actor->ForceNetUpdate();
}

}

void UTerritoryNarrativePartyComponent::RefreshActorMembership(UTalesComponent* Joining, UTalesComponent* Leaving)
{
	RefreshNativeActorMembership(this, Joining, Leaving);
}

bool UTerritoryNarrativePartyComponent::AddPartyMember(UTalesComponent* Member)
{
	if (!CanAddMember(Member)) return false;
	if (UNarrativePartyComponent* Previous = Member->GetParty())
	{
		// Native Add ignores Remove's result and can register a player twice. Use
		// its virtual removal hook first, then validate again after story callbacks.
		if (!IsValid(Previous) || !Previous->HasAuthority() || Previous->GetWorld() != GetWorld()
			|| !Previous->GetPartyMembers().Contains(Member)) return false;
		const bool bRemoved = Previous->RemovePartyMember(Member);
		// Also repair Native's stock actor wrapper, which a component transfer bypasses.
		RefreshNativeActorMembership(Previous);
		if (!bRemoved || !IsValid(Previous) || Previous->GetPartyMembers().Contains(Member)
			|| !IsValid(Member) || Member->GetParty() || !CanAddMember(Member)) return false;
	}
	// Native updates PartyComponent/arrays before broadcasting Joined. Prepare its
	// actor read model for that same result so callback queries see consistent data.
	RefreshActorMembership(Member);
	const bool bAdded = Super::AddPartyMember(Member);
	RefreshActorMembership();
	APlayerController* Controller = IsValid(Member) ? Member->GetOwningController() : nullptr;
	return bAdded && IsValid(Controller) && IsValid(Controller->PlayerState) && Member->GetParty() == this
		&& PartyMembers.Contains(Member) && PartyMemberStates.Contains(Controller->PlayerState);
}

bool UTerritoryNarrativePartyComponent::RemovePartyMember(UTalesComponent* Member)
{
	if (DialogueMutationDepth || !HasAuthority() || !IsValid(Member) || !Member->HasAuthority()
		|| Member->GetParty() != this || !PartyMembers.Contains(Member)
		|| Member->GetWorld() != GetWorld()) return false;
	APlayerController* Controller = Member->GetOwningController();
	if (!IsValid(Controller) || !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()) return false;

	// Native shares the authority's UDialogue with each personal Tales component.
	// Its removal clears membership but leaves that alias. A personal BeginDialogue
	// would then deinitialize the party's live object; TryExit/Skip can also reach it.
	// Clear before Super broadcasts Leave Party, since story callbacks can start a
	// personal dialogue synchronously. Never deinitialize or send a group exit here.
	UDialogue* Alias = Member->GetCurrentDialogue();
	const bool bSharedAlias = IsValid(Alias) && Alias->OwningComp == this;
	bool bRemoved = false;
	{
		TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
		if (bSharedAlias) Member->CurrentDialogue = nullptr;
		const TWeakObjectPtr<APlayerState> State = Controller->PlayerState.Get();
		TWeakObjectPtr<UAbilitySystemComponent> ReleasedASC;
		const FGameplayTagContainer ReleasedTags = PartySpeakerTags;
		UDialogue* Tracked = SpeakerTagDialogue.Get();
		if (IsValid(Tracked) && Tracked->IsInitialized() && Tracked == GetCurrentDialogue())
		{
			if (PartySpeakerGrants.RemoveAndCopyValue(State, ReleasedASC))
			{
				if (UAbilitySystemComponent* ASC = ReleasedASC.Get())
				{
					ASC->RemoveLooseGameplayTags(ReleasedTags, 1, EGameplayTagReplicationState::CountToOwner);
				}
			}
		}
		RefreshActorMembership(nullptr, Member);
		bRemoved = Super::RemovePartyMember(Member);
		RefreshActorMembership();
		if (!bRemoved && IsValid(Member) && Member->GetParty() == this && ReleasedASC.IsValid()
			&& Tracked == GetCurrentDialogue() && IsValid(Tracked) && Tracked->IsInitialized())
		{
			PartySpeakerGrants.Add(State, ReleasedASC);
			ReleasedASC->AddLooseGameplayTags(ReleasedTags, 1, EGameplayTagReplicationState::CountToOwner);
		}
		if (!bRemoved && bSharedAlias && IsValid(Member) && Member->GetParty() == this && !Member->GetCurrentDialogue()
			&& IsValid(Alias) && Alias == GetCurrentDialogue() && Alias->IsInitialized())
		{
			Member->CurrentDialogue = Alias;
		}
		}
	FlushDeferredDialogueExit();
	return bRemoved && IsValid(Member) && Member->GetParty() != this && !PartyMembers.Contains(Member);
}

APlayerController* UTerritoryNarrativePartyComponent::GetOwningController() const
{
	// Native's dedicated-server path already uses its leader. Keep Native's
	// local viewer on clients and listen hosts; only fill the remote-only gap.
	if (APlayerController* Controller = Super::GetOwningController()) return Controller;
	if (!HasAuthority()) return nullptr;
	UTalesComponent* Leader = GetPartyLeader();
	if (!IsValid(Leader) || Leader->GetParty() != this || !Leader->HasAuthority()) return nullptr;
	APlayerController* Controller = Leader->GetOwningController();
	return IsValid(Controller) && Controller->HasAuthority() && Controller->GetWorld() == GetWorld()
		? Controller : nullptr;
}

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
	TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
	PrepareNativeSpeakerCleanup();
	Super::EndPlay(EndPlayReason);
	DeferredExitDialogue.Reset();
	DeferredExitReason.Reset();
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
