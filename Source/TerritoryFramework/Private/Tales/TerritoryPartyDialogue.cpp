#include "Tales/TerritoryPartyDialogue.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "NarrativeGameplayTags.h"
#include "Tales/NarrativePartyComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"

bool UTerritoryPartyDialogue::CanTransferPartyContext(APlayerController* NextController) const
{
	const auto* Party = Cast<UNarrativePartyComponent>(OwningComp);
	if (!Party || !Party->GetPartyMembers().ContainsByPredicate([NextController](const UTalesComponent* Member)
		{ return IsValid(Member) && Member->GetOwningController() == NextController; })) return false;
	if (GetPlayerAvatar() && GetPlayerAvatar() != OwningPawn) return false;
	return IsInitialized() && IsValid(OwningComp) && OwningComp->HasAuthority()
		&& OwningComp->IsA<UNarrativePartyComponent>() && IsValid(NextController)
		&& NextController->HasAuthority() && NextController->GetWorld() == GetWorld()
		&& IsValid(OwningController) && !OwningController->IsLocalController()
		&& !NextController->IsLocalController();
}

bool UTerritoryPartyDialogue::TransferPartyContext(APlayerController* NextController)
{
	if (!CanTransferPartyContext(NextController)) return false;
	if (NextController == OwningController) return true;
	APawn* PreviousPawn = OwningPawn;
	APlayerState* PreviousState = OwningController->PlayerState;
	AActor* PreviousAvatar = GetPlayerAvatar();
	APawn* NextPawn = NextController->GetPawn();
	const FGameplayTagContainer AvatarTags = PlayerSpeakerInfo.OwnedTags;
	const bool bMoveBars = WantsCinematicBars();
	const auto StillCurrent = [this] { return IsInitialized() && IsValid(OwningComp) && OwningComp->GetCurrentDialogue() == this; };
	// Commit caches before character/tag callbacks. These are transient Native
	// playback fields, not a new membership or quest-state authority.
	OwningController = NextController;
	OwningPawn = NextPawn;
	OldViewTarget = NextController->GetViewTarget();
	SpeakerAvatars.Remove(PlayerSpeakerInfo.GetSpeakerID());
	if (NextPawn) SpeakerAvatars.Add(PlayerSpeakerInfo.GetSpeakerID(), NextPawn);
	PlayerSpeakerInfo.SpeakerAvatar = NextPawn;
	PlayerSpeakerInfo.SpeakerAvatarTransform = NextPawn ? NextPawn->GetActorTransform() : FTransform::Identity;
	if (CurrentSpeakerAvatar == PreviousAvatar) CurrentSpeakerAvatar = NextPawn;
	if (CurrentListenerAvatar == PreviousAvatar) CurrentListenerAvatar = NextPawn;
	if (CurrentSpeaker.GetSpeakerID() == PlayerSpeakerInfo.GetSpeakerID()) CurrentSpeaker = PlayerSpeakerInfo;
	if (CurrentPartySpeakerAvatar && CurrentPartySpeakerAvatar == PreviousState)
		SetPartyCurrentSpeaker(NextController->PlayerState);
	// Native applies a separate avatar grant in addition to its per-member grant.
	// Move only that avatar grant; the party component owns departure's member grant.
	if (auto* Previous = Cast<ANarrativeCharacter>(PreviousAvatar))
	{
		if (auto* ASC = Previous->GetAbilitySystemComponent())
			ASC->RemoveLooseGameplayTags(AvatarTags, 1, EGameplayTagReplicationState::CountToOwner);
		Previous->OnEndDialogue(this);
	}
	if (!StillCurrent()) return false;
	if (auto* Next = Cast<ANarrativeCharacter>(NextPawn))
	{
		if (auto* ASC = Next->GetAbilitySystemComponent())
			ASC->AddLooseGameplayTags(AvatarTags, 1, EGameplayTagReplicationState::CountToOwner);
		Next->OnEnterDialogue(this);
	}
	if (!StillCurrent()) return false;
	if (bMoveBars)
	{
		const FGameplayTag Bars = FNarrativeGameplayTags::Get().State_Player_WantsCinematicBars;
		if (auto* Previous = Cast<ANarrativeCharacter>(PreviousPawn))
			if (auto* ASC = Previous->GetAbilitySystemComponent()) ASC->RemoveLooseGameplayTag(Bars);
		if (auto* Next = Cast<ANarrativeCharacter>(NextPawn))
			if (auto* ASC = Next->GetAbilitySystemComponent()) ASC->AddLooseGameplayTag(Bars);
	}
	return StillCurrent();
}
