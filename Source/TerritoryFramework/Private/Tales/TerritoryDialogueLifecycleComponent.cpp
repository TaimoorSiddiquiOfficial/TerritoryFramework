#include "Tales/TerritoryDialogueLifecycleComponent.h"

#include "Engine/World.h"
#include "Tales/Dialogue.h"
#include "UnrealFramework/NarrativePlayerController.h"

UTerritoryDialogueLifecycleComponent::UTerritoryDialogueLifecycleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UTerritoryDialogueLifecycleComponent* UTerritoryDialogueLifecycleComponent::FindOrCreate(
	ANarrativePlayerController* Controller)
{
	if (!IsValid(Controller) || !Controller->HasAuthority()) return nullptr;
	if (auto* Existing = Controller->FindComponentByClass<UTerritoryDialogueLifecycleComponent>())
	{
		return Existing;
	}
	auto* Component = NewObject<UTerritoryDialogueLifecycleComponent>(Controller,
		TEXT("TerritoryDialogueLifecycle"), RF_Transient);
	Controller->AddInstanceComponent(Component);
	Component->RegisterComponent();
	return Component;
}

void UTerritoryDialogueLifecycleComponent::OnRegister()
{
	Super::OnRegister();
	Unbind();
	if (const auto* Controller = Cast<ANarrativePlayerController>(GetOwner()))
	{
		if (Controller->HasAuthority()) Tales = Controller->GetTalesComponent();
	}
	if (Tales)
	{
		Tales->OnDialogueBegan.AddUniqueDynamic(this, &ThisClass::HandleDialogueBegan);
		Tales->OnDialogueFinished.AddUniqueDynamic(this, &ThisClass::HandleDialogueFinished);
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
	}
	Tales = nullptr;
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
		// existing ExitDialogue still sends the reliable client exit when the
		// server's CurrentDialogue is null. Never create another RPC/session here.
		Tales->ExitDialogue(EExitDialogueReason::EDR_NewDialogueStarted);
	}
}
