#include "Tales/TerritoryNarrativePartyComponent.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/ScopeExit.h"
#include "Tales/NarrativeDialogueSettings.h"
#include "UnrealFramework/NarrativeParty.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "AbilitySystemComponent.h"
#include "Tales/TerritoryPartyDialogue.h"

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
	if (!HasAuthority() || DialogueMutationDepth || PartyMembers.IsEmpty()) return false;
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
	if (DialogueMutationDepth || bFlushingDeferredOperations) return;
	TGuardValue<bool> Flushing(bFlushingDeferredOperations, true);
	while (!PendingLogouts.IsEmpty() || DeferredExitReason.IsSet())
	{
		if (!PendingLogouts.IsEmpty())
		{
			// Logout can run inside Native's member iteration. Keep arrays stable until
			// it returns, then remove captured identities even if their actors died.
			const auto Departures = MoveTemp(PendingLogouts);
			PendingLogouts.Reset();
			TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
			PartyMembers.RemoveAll([](UTalesComponent* Member) { return !IsValid(Member); });
			PartyMemberStates.RemoveAll([](APlayerState* State) { return !IsValid(State); });
			RefreshActorMembership();
			PrepareNativeSpeakerCleanup();
			// A nested teardown cannot safely transfer context partway through Native
			// initialization/tag/finish callbacks. End after the outer operation.
			Super::ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
			for (const FPendingLogout& Departure : Departures)
			{
				if (UTalesComponent* Member = Departure.Member.Get())
				{
					if (Member->GetParty() == this && IsValid(Member->GetOwningController()))
					{
						RefreshActorMembership(nullptr, Member);
						Super::RemovePartyMember(Member);
					}
				}
				// Native Remove requires a live controller and its original PlayerState.
				// Destroyed objects cannot receive Leave; retire their captured entries.
				PartyMembers.RemoveAll([&](UTalesComponent* Member)
					{ return TWeakObjectPtr<UTalesComponent>(Member) == Departure.Member; });
				PartyMemberStates.RemoveAll([&](APlayerState* State)
					{ return TWeakObjectPtr<APlayerState>(State) == Departure.State; });
			}
			RefreshActorMembership();
		}
		if (DeferredExitReason.IsSet())
		{
			UDialogue* Requested = DeferredExitDialogue.Get();
			const EExitDialogueReason Reason = DeferredExitReason.GetValue();
			DeferredExitReason.Reset();
			DeferredExitDialogue.Reset();
			// A finish callback for an old dialogue must not close its replacement.
			if (IsValid(Requested) && Requested == GetCurrentDialogue() && Requested->IsInitialized()) ExitDialogue(Reason);
		}
	}
}

void UTerritoryNarrativePartyComponent::HandleGameModeLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!HasAuthority() || !IsValid(GameMode) || GameMode->GetWorld() != GetWorld()
		|| !IsValid(Exiting) || Exiting->GetWorld() != GetWorld()) return;
	for (UTalesComponent* Member : GetPartyMembers())
	{
		if (!IsValid(Member) || Member->GetParty() != this || Member->GetOwningController() != Exiting) continue;
		ExitingControllers.Add(Exiting);
		ON_SCOPE_EXIT { ExitingControllers.Remove(Exiting); };
		const TWeakObjectPtr<APlayerState> DepartingState = Exiting->PlayerState.Get();
		if (!DialogueMutationDepth && RemovePartyMember(Member)) continue;
		if (!PartyMembers.Contains(Member)) continue;
		// Personal Tales EndPlay otherwise deinitializes the group's shared object.
		// Do not mutate membership or broadcast Leave inside Native's iteration.
		// Native can restore this alias while finishing Begin; its normal Exit below
		// clears it after that operation, before another gameplay frame can run.
		PendingLogouts.Add({Member, DepartingState});
		if (IsValid(Member))
			if (UDialogue* Alias = Member->GetCurrentDialogue(); IsValid(Alias) && Alias->OwningComp == this)
				Member->CurrentDialogue = nullptr;
		FlushDeferredDialogueExit();
	}
}

bool UTerritoryNarrativePartyComponent::CanAddMember(const UTalesComponent* Member) const
{
	if (DialogueMutationDepth || !HasAuthority() || !IsValid(Member) || !Member->HasAuthority()
		|| Member->IsA<UNarrativePartyComponent>() || Member->GetWorld() != GetWorld()
		|| Member->GetParty() == this || PartyMembers.Contains(Member)) return false;
	// Native does not synchronize an in-progress conversation to a newly joined
	// member. Reject before releasing their old party or applying unbalanced tags.
	if (IsValid(GetCurrentDialogue()) && GetCurrentDialogue()->IsInitialized()) return false;
	const APlayerController* Controller = Member->GetOwningController();
	const APlayerState* State = IsValid(Controller) ? Controller->PlayerState.Get() : nullptr;
	if (!IsValid(Controller) || Controller->IsActorBeingDestroyed() || ExitingControllers.Contains(Controller)
		|| !Controller->HasAuthority() || Controller->GetWorld() != GetWorld()
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
	ON_SCOPE_EXIT { FlushDeferredDialogueExit(); };
	UDialogue* Current = GetCurrentDialogue();
	// Native sends the selected party speaker only to client copies. A server-only
	// context transfer cannot repair a camera already following a departing speaker.
	const bool bPlayerLineActive = IsValid(Current) && Current->GetCurrentNode()
		&& Current->GetCurrentNode()->IsA<UDialogueNode_Player>();
	const bool bOwnerLeaving = IsValid(Current) && Current->OwningController == Controller;
	APlayerController* NextController = nullptr;
	if (bOwnerLeaving)
	{
		for (UTalesComponent* Remaining : PartyMembers)
		{
			if (!IsValid(Remaining) || Remaining == Member) continue;
			APlayerController* Candidate = Remaining->GetOwningController();
			if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed() || ExitingControllers.Contains(Candidate)) continue;
			if (!NextController) NextController = Candidate;
			if (GetNetMode() != NM_DedicatedServer && Candidate->IsLocalController())
			{
				NextController = Candidate;
				break;
			}
		}
	}
	auto* Adapter = Cast<UTerritoryPartyDialogue>(Current);
	const bool bTransferContext = bOwnerLeaving && !bPlayerLineActive
		&& OwnerDeparturePolicy == ETerritoryPartyOwnerDeparturePolicy::ContinueWhenSupported
		&& Adapter && Adapter->CanTransferPartyContext(NextController);

	// Native shares the authority's UDialogue with each personal Tales component.
	// Its removal clears membership but leaves that alias. A personal BeginDialogue
	// would then deinitialize the party's live object; TryExit/Skip can also reach it.
	// Clear before Super broadcasts Leave Party, since story callbacks can start a
	// personal dialogue synchronously. Remaining members keep their shared session.
	bool bRemoved = false;
	{
		TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
		if ((PartyMembers.Num() == 1 || bPlayerLineActive || (bOwnerLeaving && !bTransferContext)) && GetCurrentDialogue())
		{
			// Native must still see the departing member when it balances speaker grants
			// and sends its reliable client exit. Finish before Leave callbacks can
			// start a personal conversation. Unsupported transfers end for the group.
			PrepareNativeSpeakerCleanup();
			Super::ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
			if (!IsValid(Member) || Member->GetParty() != this || !PartyMembers.Contains(Member)) return false;
		}
		// A Native finish callback may have started a separate personal dialogue.
		// Only detach an alias that still belongs to this party.
		UDialogue* Alias = Member->GetCurrentDialogue();
		const bool bSharedAlias = IsValid(Alias) && Alias->OwningComp == this;
		if (bSharedAlias) Member->CurrentDialogue = nullptr;
		if (bTransferContext && Adapter == GetCurrentDialogue() && !Adapter->TransferPartyContext(NextController))
		{
			PrepareNativeSpeakerCleanup();
			Super::ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
		}
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
	if (HasAuthority() && !LogoutHandle.IsValid())
		LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &ThisClass::HandleGameModeLogout);
}

void UTerritoryNarrativePartyComponent::OnUnregister()
{
	UnbindReplyEvents();
	UnbindLogout();
	Super::OnUnregister();
}

void UTerritoryNarrativePartyComponent::UnbindLogout()
{
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
	LogoutHandle.Reset();
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
	UnbindLogout();
	TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
	PrepareNativeSpeakerCleanup();
	Super::EndPlay(EndPlayReason);
	DeferredExitDialogue.Reset();
	DeferredExitReason.Reset();
	PendingLogouts.Reset();
	ExitingControllers.Reset();
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
	{
		TGuardValue<int32> Mutation(DialogueMutationDepth, DialogueMutationDepth + 1);
		Super::SelectDialogueOption(Option, Selector);
	}
	FlushDeferredDialogueExit();
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
