#include "Tales/TerritoryDialogueLifecycleComponent.h"

#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryEvidenceTags.h"
#include "Engine/World.h"
#include "NarrativeStableActor.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "Tales/Dialogue.h"
#include "Tales/NarrativePartyComponent.h"
#include "Tales/Quest.h"
#include "UnrealFramework/NarrativePlayerController.h"

UTerritoryDialogueLifecycleComponent::UTerritoryDialogueLifecycleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UTerritoryDialogueLifecycleComponent* UTerritoryDialogueLifecycleComponent::FindOrCreate(
	ANarrativePlayerController* Controller)
{
	if (!IsValid(Controller) || !Controller->HasAuthority()) return nullptr;
	return FindOrCreateForTales(Controller->GetTalesComponent());
}

UTerritoryDialogueLifecycleComponent* UTerritoryDialogueLifecycleComponent::FindOrCreateForTales(
	UTalesComponent* Source)
{
	if (!IsValid(Source) || !Source->HasAuthority() || !IsValid(Source->GetOwner())) return nullptr;
	AActor* Owner = Source->GetOwner();
	TInlineComponentArray<UTerritoryDialogueLifecycleComponent*> Observers(Owner);
	for (auto* Existing : Observers)
	{
		if (Existing && Existing->SourceTales == Source) return Existing;
	}
	auto* Component = NewObject<UTerritoryDialogueLifecycleComponent>(Owner,
		MakeUniqueObjectName(Owner, StaticClass(), TEXT("TerritoryDialogueLifecycle")), RF_Transient);
	Component->SourceTales = Source;
	Owner->AddInstanceComponent(Component);
	Component->RegisterComponent();
	return Component;
}

void UTerritoryDialogueLifecycleComponent::OnRegister()
{
	Super::OnRegister();
	Unbind();
	UTalesComponent* Source = SourceTales.Get();
	if (Source && Source->GetOwner() == GetOwner() && Source->HasAuthority())
	{
		Tales = Source;
	}
	if (Tales)
	{
		Tales->OnDialogueBegan.AddUniqueDynamic(this, &ThisClass::HandleDialogueBegan);
		Tales->OnDialogueFinished.AddUniqueDynamic(this, &ThisClass::HandleDialogueFinished);
		Tales->OnJoinedParty.AddUniqueDynamic(this, &ThisClass::HandleJoinedParty);
		// The owning character's own Tales component is the one authority that knows a quest
		// reached its success state, so the evidence binding hangs off the same observer.
		Tales->OnQuestSucceeded.AddUniqueDynamic(this, &ThisClass::HandleQuestSucceeded);
		HandleJoinedParty(Tales->GetParty(), nullptr);
	}
}

void UTerritoryDialogueLifecycleComponent::OnUnregister()
{
	Unbind();
	Super::OnUnregister();
}

void UTerritoryDialogueLifecycleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();
	Super::EndPlay(EndPlayReason);
}

void UTerritoryDialogueLifecycleComponent::Unbind()
{
	CancelReconciliation();
	if (Tales)
	{
		Tales->OnDialogueBegan.RemoveDynamic(this, &ThisClass::HandleDialogueBegan);
		Tales->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
		Tales->OnJoinedParty.RemoveDynamic(this, &ThisClass::HandleJoinedParty);
		Tales->OnQuestSucceeded.RemoveDynamic(this, &ThisClass::HandleQuestSucceeded);
	}
	Tales = nullptr;
}

UTerritoryDialogueLifecycleComponent::FTalesFactAttribution
UTerritoryDialogueLifecycleComponent::ResolveFactAttribution() const
{
	FTalesFactAttribution Attribution;
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return Attribution;
	Attribution.InstigatorFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, Owner);
	if (Owner->GetClass()->ImplementsInterface(UNarrativeStableActor::StaticClass()))
	{
		Attribution.SubjectId = INarrativeStableActor::Execute_GetActorGUID(Owner);
	}
	return Attribution;
}

void UTerritoryDialogueLifecycleComponent::RecordDialogueCompletedEvidence(const UDialogue& Dialogue)
{
	UWorld* World = GetWorld();
	UTerritoryEvidenceLedgerSubsystem* Ledger = World
		? World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>() : nullptr;
	if (!Ledger || !Ledger->IsEvidenceLedgerReady()) return;

	const FTalesFactAttribution Attribution = ResolveFactAttribution();
	FTerritoryEvidenceRecord Record;
	Record.EvidenceTag = TerritoryEvidenceTags::DialogueCompleted.GetTag();
	Record.SubjectId = Attribution.SubjectId;
	Record.InstigatorFaction = Attribution.InstigatorFaction;
	Record.SourceTerritory = FGameplayTag();
	Record.ContextId = UTerritoryEvidenceLedgerSubsystem::MakeEvidenceContextId(
		FString::Printf(TEXT("DialogueCompleted|%s|%s|%lld"), *Dialogue.GetName(),
			IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("None"),
			static_cast<int64>(UTerritoryEvidenceLedgerSubsystem::ResolveCampaignGameTime(this)
				* 1000.0)));
	Record.Weight = 1;
	Record.GameTimeSeconds = UTerritoryEvidenceLedgerSubsystem::ResolveCampaignGameTime(this);

	const FTerritoryEvidenceMutationResult Result = Ledger->RecordEvidenceRecord(Record);
	if (!Result.bSuccess && !Result.bAlreadyRecorded)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Evidence] Completed dialogue %s was not recorded: %s"),
			*Dialogue.GetName(), *Result.FailureReason.ToString());
	}
}

void UTerritoryDialogueLifecycleComponent::RecordQuestCompletedEvidence(const UQuest& Quest)
{
	UWorld* World = GetWorld();
	UTerritoryEvidenceLedgerSubsystem* Ledger = World
		? World->GetSubsystem<UTerritoryEvidenceLedgerSubsystem>() : nullptr;
	if (!Ledger || !Ledger->IsEvidenceLedgerReady()) return;

	const FTalesFactAttribution Attribution = ResolveFactAttribution();
	FTerritoryEvidenceRecord Record;
	Record.EvidenceTag = TerritoryEvidenceTags::QuestCompleted.GetTag();
	Record.SubjectId = Attribution.SubjectId;
	Record.InstigatorFaction = Attribution.InstigatorFaction;
	Record.SourceTerritory = FGameplayTag();
	Record.ContextId = UTerritoryEvidenceLedgerSubsystem::MakeEvidenceContextId(
		FString::Printf(TEXT("QuestCompleted|%s|%s|%lld"), *Quest.GetName(),
			IsValid(GetOwner()) ? *GetOwner()->GetName() : TEXT("None"),
			static_cast<int64>(UTerritoryEvidenceLedgerSubsystem::ResolveCampaignGameTime(this)
				* 1000.0)));
	Record.Weight = 1;
	Record.GameTimeSeconds = UTerritoryEvidenceLedgerSubsystem::ResolveCampaignGameTime(this);

	const FTerritoryEvidenceMutationResult Result = Ledger->RecordEvidenceRecord(Record);
	if (!Result.bSuccess && !Result.bAlreadyRecorded)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("[Evidence] Succeeded quest %s was not recorded: %s"),
			*Quest.GetName(), *Result.FailureReason.ToString());
	}
}

void UTerritoryDialogueLifecycleComponent::HandleQuestSucceeded(
	const UQuest* Quest, const FText& QuestSucceededMessage)
{
	(void)QuestSucceededMessage;
	if (!Tales || !Tales->HasAuthority() || !Quest) return;
	RecordQuestCompletedEvidence(*Quest);
}

void UTerritoryDialogueLifecycleComponent::HandleJoinedParty(
	UNarrativePartyComponent* NewParty, UNarrativePartyComponent* LeftParty)
{
	(void)LeftParty;
	if (Tales && NewParty && Tales->HasAuthority() && Tales->GetParty() == NewParty)
	{
		// Native owns membership. All members converge on one observer attached
		// to the party owner, so a failed replacement sends one group exit.
		FindOrCreateForTales(NewParty);
	}
}

void UTerritoryDialogueLifecycleComponent::CancelReconciliation()
{
	if (UWorld* World = TimerWorld.Get()) World->GetTimerManager().ClearTimer(ReconcileTimer);
	ReconcileTimer.Invalidate();
	TimerWorld.Reset();
}

void UTerritoryDialogueLifecycleComponent::HandleDialogueBegan(UDialogue* Dialogue)
{
	if (Dialogue && Tales && Dialogue == Tales->GetCurrentDialogue()) CancelReconciliation();
}

void UTerritoryDialogueLifecycleComponent::HandleDialogueFinished(
	UDialogue* Dialogue, bool bStartingNewDialogue, EExitDialogueReason Reason)
{
	if (!Tales || !Tales->HasAuthority() || !Dialogue
		|| Dialogue->OwningComp != Tales || Dialogue != Tales->GetCurrentDialogue()) return;
	CancelReconciliation();
	// A dialogue that ran out of lines is the one that completed. Walking away (PlayerExited),
	// drifting out of range (TooFarAway), being replaced (NewDialogueStarted) and being cut off
	// by a cinematic (StoppedByCinematic) are all exits that are not a completion, so a dialogue
	// ended that way leaves no fact behind.
	if (!bStartingNewDialogue && Reason == EExitDialogueReason::EDR_NoLines)
	{
		RecordDialogueCompletedEvidence(*Dialogue);
	}
	if (!bStartingNewDialogue || Reason != EExitDialogueReason::EDR_NewDialogueStarted) return;
	if (UWorld* World = Tales->GetWorld())
	{
		// SetCurrentDialogue publishes Finished before trying the new instance.
		// Wait until that call returns; a successful Began cancels this check.
		TimerWorld = World;
		ReconcileTimer = World->GetTimerManager().SetTimerForNextTick(
			this, &ThisClass::ReconcileReplacement);
	}
}

void UTerritoryDialogueLifecycleComponent::ReconcileReplacement()
{
	ReconcileTimer.Invalidate();
	TimerWorld.Reset();
	if (IsValid(Tales) && Tales->HasAuthority() && !Tales->GetCurrentDialogue())
	{
		// Native BeginDialogue only sends ClientBeginDialogue on success. Its
		// virtual ExitDialogue still sends client exits when CurrentDialogue is
		// null. The party override also clears member aliases and routes its own
		// group messages. Never create another RPC or session authority here.
		Tales->ExitDialogue(EExitDialogueReason::EDR_NewDialogueStarted);
	}
}
