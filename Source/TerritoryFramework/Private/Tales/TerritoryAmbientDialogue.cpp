#include "Tales/TerritoryAmbientDialogue.h"
#include "Tales/DialogueBlueprintGeneratedClass.h"
#include "Tales/DialogueSM.h"
#include "Tales/TalesComponent.h"
#include "Social/TerritoryConversationListenerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

UTerritoryAmbientDialogue::UTerritoryAmbientDialogue()
{
	NormalizeMeetingRoles();
	ApplyAmbientPresentation();
}

void UTerritoryAmbientDialogue::NormalizeMeetingRoles()
{
	Speakers.SetNum(2);
	Speakers[0].SpeakerID = FirstRoleID;
	Speakers[1].SpeakerID = SecondRoleID;
	for (FSpeakerInfo& Speaker : Speakers)
	{
		Speaker.NPCDataAsset = nullptr;
		Speaker.bIsPlayer = false;
	}
}

void UTerritoryAmbientDialogue::ApplyAmbientPresentation()
{
	bFreeMovement = true;
	bAutoStopMovement = false;
	bAutoRotateSpeakers = false;
	bAdjustPlayerTransform = false;
	bShowCinematicBars = false;
	bCanBeExited = true;
	DialogueBlendOutTime = 0.f;
	DialogueCameraShake = nullptr;
	PlayerSpeakerInfo.OwnedTags.Reset();
}

bool UTerritoryAmbientDialogue::ValidateDialogueClass(TSubclassOf<UTerritoryAmbientDialogue> Class, FString& Failure)
{
	const auto* Generated = Class ? Cast<UDialogueBlueprintGeneratedClass>(Class.Get()) : nullptr;
	const auto* Defaults = Class ? Class->GetDefaultObject<UTerritoryAmbientDialogue>() : nullptr;
	if (!Generated || !Defaults || !Generated->GetDialogueTemplate())
	{
		Failure = TEXT("Conversation requires a compiled Narrative dialogue Blueprint derived from TerritoryAmbientDialogue.");
		return false;
	}
	return Defaults->ValidateGraph(Generated->GetDialogueTemplate(), Failure);
}

bool UTerritoryAmbientDialogue::ValidateGraph(const UDialogue* Graph, FString& Failure) const
{
	if (FirstRoleID.IsNone() || SecondRoleID.IsNone() || FirstRoleID == SecondRoleID
		|| FirstRoleID == PlayerSpeakerInfo.GetSpeakerID() || SecondRoleID == PlayerSpeakerInfo.GetSpeakerID())
	{
		Failure = TEXT("Two distinct non-player role IDs are required."); return false;
	}
	if (!Graph || !Graph->RootDialogue || Graph->NPCReplies.IsEmpty() || !Graph->PlayerReplies.IsEmpty())
	{
		Failure = TEXT("Ambient conversations require a root and NPC-only replies, without player choices."); return false;
	}
	const auto ValidLine = [](const FDialogueLine& Line)
	{
		return !Line.Shot && Line.Duration != ELineDuration::LD_Never
			&& Line.Duration != ELineDuration::LD_WhenSequenceEnds
			&& (Line.Duration != ELineDuration::LD_AfterDuration
				|| FMath::IsFinite(Line.DurationSecondsOverride) && Line.DurationSecondsOverride > 0.f);
	};
	TArray<const UDialogueNode_NPC*> Nodes;
	Nodes.Add(Graph->RootDialogue);
	for (const auto* Node : Graph->NPCReplies) Nodes.AddUnique(Node);
	if (Nodes.Num() > 1024) { Failure = TEXT("Ambient dialogue exceeds the supported 1024-node bound."); return false; }
	TMap<const UDialogueNode_NPC*, uint8> Colors;
	TSet<FName> NodeIDs;
	for (const auto* Node : Nodes)
	{
		if (!Node || Node->GetID().IsNone() || NodeIDs.Contains(Node->GetID()))
			{ Failure = TEXT("Ambient node IDs must be valid and unique."); return false; }
		NodeIDs.Add(Node->GetID());
	}
	TFunction<bool(const UDialogueNode_NPC*)> Visit = [&](const UDialogueNode_NPC* Node)
	{
		if (!Nodes.Contains(Node) || Colors.FindRef(Node) == 1) return false;
		if (Colors.FindRef(Node) == 2) return true;
		Colors.Add(Node, 1);
		for (const auto* Child : Node->NPCReplies) if (!Visit(Child)) return false;
		Colors.Add(Node, 2); return true;
	};
	if (!Visit(Graph->RootDialogue) || Colors.Num() != Nodes.Num())
		{ Failure = TEXT("Ambient graph must be reachable and acyclic; Native NPC chunks cannot contain cycles."); return false; }
	bool bFirstSpeaks = false, bSecondSpeaks = false;
	for (const UDialogueNode_NPC* Node : Nodes)
	{
		if (!Node || !Node->PlayerReplies.IsEmpty() || !ValidLine(Node->Line))
		{
			Failure = TEXT("Player choices, camera shots and unbounded lines are unsupported in ambient conversations."); return false;
		}
		for (const FDialogueLine& Line : Node->AlternativeLines)
			if (!ValidLine(Line)) { Failure = TEXT("An alternate line has an unsupported shot or duration."); return false; }
		if (Node->IsRoutingNode()) continue;
		if (Node->GetSpeakerID() != FirstRoleID && Node->GetSpeakerID() != SecondRoleID)
		{
			Failure = TEXT("Every spoken line must use one of the meeting's exact role IDs."); return false;
		}
		if (!Node->DirectedAtSpeakerID.IsNone() && Node->DirectedAtSpeakerID != FirstRoleID && Node->DirectedAtSpeakerID != SecondRoleID)
		{
			Failure = TEXT("Ambient lines can address only the two guard roles."); return false;
		}
		bFirstSpeaks |= Node->GetSpeakerID() == FirstRoleID;
		bSecondSpeaks |= Node->GetSpeakerID() == SecondRoleID;
	}
	if (!bFirstSpeaks || !bSecondSpeaks)
	{
		Failure = TEXT("Both distinct guard roles need at least one spoken NPC line."); return false;
	}
	Failure.Reset(); return true;
}

bool UTerritoryAmbientDialogue::Initialize(UTalesComponent* Tales, const FDialoguePlayParams Params)
{
	FString Failure;
	if (!ValidateDialogueClass(GetClass(), Failure) || !IsValid(Tales) || !Tales->GetOwningController()) return false;
	auto* Adapter = Tales->GetOwningController()->FindComponentByClass<UTerritoryConversationListenerComponent>();
	FDialoguePlayParams ExactParams = Params;
	if (!Adapter || !Adapter->ResolvePlayParams(Tales, GetClass(), ExactParams)) return false;
	NormalizeMeetingRoles();
	ApplyAmbientPresentation();
	if (!Super::Initialize(Tales, ExactParams)) return false;
	// Native copies the graph from its compiled template; role identity and presentation
	// remain the subclass's authoring contract on both server and listening client.
	NormalizeMeetingRoles(); ApplyAmbientPresentation();
	return ValidateGraph(this, Failure) && Adapter->BindInitializedDialogue(Tales, this);
}

AActor* UTerritoryAmbientDialogue::LinkSpeakerAvatar_Implementation(const FSpeakerInfo& Speaker, int32 SpeakerIndex)
{
	return GetMeetingRoleActor(Speaker.GetSpeakerID());
}

AActor* UTerritoryAmbientDialogue::GetMeetingRoleActor(FName RoleID) const
{
	const int32 RoleIndex = RoleID == FirstRoleID ? 0 : RoleID == SecondRoleID ? 1 : INDEX_NONE;
	return PlayParams.Speakers.IsValidIndex(RoleIndex) ? PlayParams.Speakers[RoleIndex] : nullptr;
}

void UTerritoryAmbientDialogue::InitSpeakerAvatars()
{
	// Deliberately omit PlayerSpeakerInfo: the listener must not receive speaker tags,
	// OnEnterDialogue NPC behavior, a cinematic avatar, or speaker transform changes.
	SpeakerAvatars.Reset();
	for (int32 Index = 0; Index < Speakers.Num(); ++Index)
	{
		FSpeakerInfo& Speaker = Speakers[Index];
		if (AActor* Actor = LinkSpeakerAvatar_Implementation(Speaker, Index); IsValid(Actor))
		{
			SpeakerAvatars.Add(Speaker.GetSpeakerID(), Actor);
			Speaker.SpeakerAvatar = Actor;
			Speaker.SpeakerAvatarTransform = Actor->GetActorTransform();
		}
	}
}

void UTerritoryAmbientDialogue::OnEndDialogue()
{
	// Keep story context on the exact Tales/controller, but omit Native's player
	// presentation cache during speaker cleanup. Native otherwise unconditionally
	// unhides this pawn AFTER end callbacks, overwriting a new cinematic's visibility.
	// Ambient end callbacks read their listener through GetListeningPawn or Tales.
	const TWeakObjectPtr<APawn> ListenerPawn(OwningPawn);
	OwningPawn = nullptr;
	DialogueBlendOutTime = 0.f;
	Super::OnEndDialogue();
	OwningPawn = ListenerPawn.IsValid() && !ListenerPawn->IsActorBeingDestroyed() ? ListenerPawn.Get() : nullptr;
}

APawn* UTerritoryAmbientDialogue::GetListeningPawn() const
{
	return IsValid(OwningComp) ? OwningComp->GetOwningPawn() : nullptr;
}

void UTerritoryAmbientDialogue::BlendingOutFinished()
{
	// Intentional ambient contract: this dialogue never disabled player input and
	// must not enable input owned by a newer cinematic or another gameplay system.
}

void UTerritoryAmbientDialogue::PlayDialogueSequence(UNarrativeDialogueSequence* Sequence, AActor* Speaker, AActor* Listener)
{
	// Intentional ambient contract: authored/default speaker camera shots cannot
	// take the listener's camera. Voice, facial animation and NPC montage use Native playback.
}

#if WITH_EDITOR
void UTerritoryAmbientDialogue::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	NormalizeMeetingRoles();
}
#endif
