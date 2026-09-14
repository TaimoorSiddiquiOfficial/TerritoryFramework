#include "TerritoryPartyReplyProbe.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "Core/TerritoryPropertyTags.h"
#include "UnrealFramework/NarrativePlayerState.h"

FGameplayTag GetTerritoryPartySpeakerTestTag() { return TerritoryPropertyTags::ArmsShopRole; }

UAbilitySystemComponent* ATerritoryPartySpeakerAvatarProbe::GetAbilitySystemComponent() const { return PlayerASC.Get(); }

bool UTerritoryPartySpeakerTestDialogue::Initialize(UTalesComponent* Component, const FDialoguePlayParams Params)
{
	if (!Super::Initialize(Component, Params)) return false;
	PlayerSpeakerInfo.OwnedTags.AddTag(GetTerritoryPartySpeakerTestTag());
	bShowCinematicBars = false;
	bAdjustPlayerTransform = false;
	DialogueBlendOutTime = 0.f;
	return true;
}

void InstallTerritoryPartyMemberProbe(ANarrativePlayerController* Controller, UTalesComponent* Member)
{
	const auto* Property = FindFProperty<FObjectPropertyBase>(ANarrativePlayerController::StaticClass(), TEXT("TalesComponent"));
	check(Property);
	Property->SetObjectPropertyValue_InContainer(Controller, Member);
}

bool UTerritoryPartyReplyTestDialogue::Initialize(UTalesComponent* Component, const FDialoguePlayParams Params)
{
	if (!Component || HasAnyFlags(RF_ClassDefaultObject)) return false;
	OwningComp = Component;
	OwningController = Component->GetOwningController();
	OwningPawn = Component->GetOwningPawn();
	bFreeMovement = true;
	bAutoStopMovement = false;
	PlayParams = Params;
	if (Params.Priority >= 0) Priority = Params.Priority;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* NPC = NewObject<UDialogueNode_NPC>(this);
		NPC->SetID(FName(*FString::Printf(TEXT("NPC%d"), Index)));
		NPC->Line.Text = FText::FromString(TEXT("Wait until this line has finished."));
		NPC->Line.Duration = ELineDuration::LD_Never;
		NPC->OwningDialogue = this;
		NPC->OwningComponent = Component;
		NPCReplies.Add(NPC);
		auto* Reply = NewObject<UDialogueNode_Player>(this);
		Reply->SetID(FName(*FString::Printf(TEXT("Reply%d"), Index)));
		Reply->Line.Text = FText::FromString(Index ? TEXT("Second choice.") : TEXT("First choice."));
		Reply->Line.Duration = ELineDuration::LD_Never;
		Reply->OwningDialogue = this;
		Reply->OwningComponent = Component;
		PlayerReplies.Add(Reply);
	}
	RootDialogue = NPCReplies[0];
	RootDialogue->NPCReplies.Add(NPCReplies[1]);
	NPCReplies[1]->PlayerReplies = PlayerReplies;
	return !Component->HasAuthority() || GenerateDialogueChunk(RootDialogue);
}

bool ATerritoryPartyReplyTestDriver::ChooseReply(UTalesComponent* Member, FName OptionID)
{
	UDialogue* Dialogue = IsValid(Member) ? Member->GetCurrentDialogue() : nullptr;
	UDialogueNode_Player* Option = Dialogue ? Dialogue->GetPlayerReplyByID(OptionID) : nullptr;
	if (!Option) return false;
	Member->TrySelectDialogueOption(Option);
	return true; // Dispatch only; the recorder checks authoritative acceptance separately.
}

bool ATerritoryPartyReplyTestDriver::SkipLine(UTalesComponent* Member)
{
	return IsValid(Member) && Member->TrySkipCurrentDialogueLine();
}

bool ATerritoryPartyReplyTestDriver::JoinParty(UTalesComponent* Member, UNarrativePartyComponent* Party)
{
	return IsValid(Party) && Party->AddPartyMember(Member);
}

bool ATerritoryPartyReplyTestDriver::AddExternalSpeakerTag(APlayerState* State)
{
	auto* NativeState = Cast<ANarrativePlayerState>(State);
	if (!IsValid(NativeState) || !NativeState->HasAuthority()) return false;
	if (UAbilitySystemComponent* ASC = NativeState->GetAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(GetTerritoryPartySpeakerTestTag(), 1, EGameplayTagReplicationState::CountToOwner);
		return true;
	}
	return false;
}

int32 ATerritoryPartyReplyTestDriver::GetSpeakerTagCount(APlayerState* State) const
{
	const auto* NativeState = Cast<ANarrativePlayerState>(State);
	const UAbilitySystemComponent* ASC = IsValid(NativeState) ? NativeState->GetAbilitySystemComponent() : nullptr;
	return ASC ? ASC->GetTagCount(GetTerritoryPartySpeakerTestTag()) : INDEX_NONE;
}
