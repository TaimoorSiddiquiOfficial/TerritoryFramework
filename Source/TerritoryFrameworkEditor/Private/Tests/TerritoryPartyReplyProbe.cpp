#include "TerritoryPartyReplyProbe.h"

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
