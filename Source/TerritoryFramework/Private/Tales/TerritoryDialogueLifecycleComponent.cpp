#include "Tales/TerritoryDialogueLifecycleComponent.h"

#include "Engine/World.h"
#include "Tales/Dialogue.h"
#include "Tales/NarrativePartyComponent.h"
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
	}
	Tales = nullptr;
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
