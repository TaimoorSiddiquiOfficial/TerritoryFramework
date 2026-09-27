#include "Social/TerritoryConversationListenerComponent.h"
#include "Social/TerritoryConversationMeetingPoint.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "Tales/TerritoryAmbientDialogue.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Engine/World.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"

UTerritoryConversationListenerComponent::UTerritoryConversationListenerComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.25f;
}

UTalesComponent* UTerritoryConversationListenerComponent::GetPersonalTales() const
{
	const auto* PC = Cast<ANarrativePlayerController>(GetOwner());
	return PC ? PC->GetTalesComponent() : nullptr;
}

bool UTerritoryConversationListenerComponent::IsSupportedListener(UTalesComponent* Tales, const UActorComponent* Adapter)
{
	const auto* PC = Adapter ? Cast<ANarrativePlayerController>(Adapter->GetOwner()) : nullptr;
	return IsValid(PC) && IsValid(Tales) && Tales == PC->GetTalesComponent()
		&& Tales->GetOwner() == PC && Tales->GetOwningController() == PC
		&& Tales->GetWorld() == Adapter->GetWorld() && !Tales->IsPartyComponent() && !Tales->GetParty()
		&& !Tales->bIsLoading && IsValid(Tales->GetOwningPawn())
		&& !UTerritoryCinematicPresentationSubsystem::IsInCutscene(PC, nullptr, true);
}

bool UTerritoryConversationListenerComponent::Prepare(ATerritoryConversationMeetingPoint* Meeting,
	UTalesComponent* Tales, const FGuid& Session, TSubclassOf<UTerritoryAmbientDialogue> Dialogue,
	ATerritoryGuardCharacter* First, ATerritoryGuardCharacter* Second)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || SessionID.IsValid() || !Session.IsValid()
		|| !IsValid(Meeting) || !Dialogue || !IsSupportedListener(Tales, this) || Tales->GetCurrentDialogue()
		|| !IsValid(First) || !IsValid(Second) || First == Second || First->GetWorld() != GetWorld()
		|| Second->GetWorld() != GetWorld()) return false;
	SessionID = Session;
	PendingMeeting = Meeting;
	PreparedClass = Dialogue;
	FirstGuard = First;
	SecondGuard = Second;
	// The meeting owns the shorter acknowledgement deadline. This is a defensive absolute context cap.
	ExpiresAt = GetWorld()->GetTimeSeconds() + 450.;
	SetComponentTickEnabled(true);
	if (CastChecked<ANarrativePlayerController>(GetOwner())->IsLocalController())
	{
		bAcknowledged = true;
		Meeting->ListenerPrepared(Session, true);
	}
	else ClientPrepare(Session, Dialogue, First, Second);
	return true;
}

void UTerritoryConversationListenerComponent::ClientPrepare_Implementation(FGuid Session,
	TSubclassOf<UTerritoryAmbientDialogue> Dialogue, ATerritoryGuardCharacter* First, ATerritoryGuardCharacter* Second)
{
	UTalesComponent* Tales = GetPersonalTales();
	FString Failure;
	const bool bReady = !GetOwner()->HasAuthority() && !SessionID.IsValid() && Session.IsValid()
		&& IsSupportedListener(Tales, this) && !Tales->GetCurrentDialogue()
		&& IsValid(First) && IsValid(Second) && First != Second && First->GetWorld() == GetWorld()
		&& Second->GetWorld() == GetWorld() && First->IsAlive() && Second->IsAlive()
		&& FTerritoryNarrativeProAdapter::IsCharacterReady(First) && FTerritoryNarrativeProAdapter::IsCharacterReady(Second)
		&& UTerritoryAmbientDialogue::ValidateDialogueClass(Dialogue, Failure);
	if (bReady)
	{
		SessionID = Session;
		PreparedClass = Dialogue;
		FirstGuard = First;
		SecondGuard = Second;
		bAcknowledged = true;
		ExpiresAt = GetWorld()->GetTimeSeconds() + 450.;
		SetComponentTickEnabled(true);
	}
	ServerAcknowledge(Session, bReady);
}

void UTerritoryConversationListenerComponent::ServerAcknowledge_Implementation(FGuid Session, bool bReady)
{
	if (!GetOwner()->HasAuthority() || Session != SessionID || !SessionID.IsValid() || bAcknowledged) return;
	bAcknowledged = bReady;
	if (PendingMeeting.IsValid()) PendingMeeting->ListenerPrepared(Session, bReady);
	else Release(Session);
}

bool UTerritoryConversationListenerComponent::BeginPrepared(const FGuid& Session)
{
	UTalesComponent* Tales = GetPersonalTales();
	if (!GetOwner()->HasAuthority() || Session != SessionID || !bAcknowledged
		|| !IsSupportedListener(Tales, this) || Tales->GetCurrentDialogue()) return false;
	TGuardValue<bool> Starting(bStartingNativeDialogue, true);
	FDialoguePlayParams Params;
	if (!ResolvePlayParams(Tales, PreparedClass, Params)) return false;
	const bool bBegan = Tales->BeginDialogue(PreparedClass, Params);
	return bBegan && SessionID == Session && bAcknowledged && PreparedDialogue.IsValid()
		&& Tales->GetCurrentDialogue() == PreparedDialogue.Get();
}

bool UTerritoryConversationListenerComponent::BindInitializedDialogue(UTalesComponent* Tales, UTerritoryAmbientDialogue* Dialogue)
{
	FDialoguePlayParams Params;
	if (!IsValid(Dialogue) || PreparedDialogue.IsValid()
		|| !ResolvePlayParams(Tales, Dialogue->GetClass(), Params) || Dialogue->OwningComp != Tales) return false;
	PreparedDialogue = Dialogue;
	return true;
}

UDialogue* UTerritoryConversationListenerComponent::GetPreparedDialogue(const FGuid& Session) const
{
	return Session.IsValid() && SessionID == Session ? PreparedDialogue.Get() : nullptr;
}

bool UTerritoryConversationListenerComponent::ResolvePlayParams(UTalesComponent* Tales,
	const UClass* DialogueClass, FDialoguePlayParams& Params) const
{
	if (!IsSupportedListener(Tales, this) || !SessionID.IsValid() || !bAcknowledged
		|| PreparedClass.Get() != DialogueClass || !GetWorld() || GetWorld()->GetTimeSeconds() >= ExpiresAt
		|| !IsValid(FirstGuard) || !IsValid(SecondGuard) || FirstGuard == SecondGuard
		|| !FirstGuard->IsAlive() || !SecondGuard->IsAlive()
		|| (GetOwner()->HasAuthority() && !bStartingNativeDialogue)) return false;
	Params.Speakers = {FirstGuard, SecondGuard};
	Params.bOverride_bFreeMovement = true;
	Params.bFreeMovement = true;
	Params.bOverride_bStopMovement = true;
	Params.bStopMovement = false;
	Params.bOverride_bCanBeExited = true;
	Params.bCanBeExited = true;
	return true;
}

void UTerritoryConversationListenerComponent::ResetContext()
{
	SessionID.Invalidate(); PreparedClass = nullptr; FirstGuard = nullptr; SecondGuard = nullptr;
	PendingMeeting.Reset(); PreparedDialogue.Reset(); bAcknowledged = false; bStartingNativeDialogue = false; ExpiresAt = 0.;
	SetComponentTickEnabled(false);
}

void UTerritoryConversationListenerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || GetOwner()->HasAuthority() || !SessionID.IsValid()) return;
	if (!IsSupportedListener(GetPersonalTales(), this) || !IsValid(FirstGuard) || !IsValid(SecondGuard)
		|| FirstGuard->IsActorBeingDestroyed() || SecondGuard->IsActorBeingDestroyed()
		|| !FirstGuard->IsAlive() || !SecondGuard->IsAlive() || GetWorld()->GetTimeSeconds() >= ExpiresAt)
	{
		// The owning client can abandon only its matching session (for example a
		// client-only Narrative cutscene). It never chooses another listener or story.
		ServerAbandon(SessionID);
		ResetContext();
	}
}

void UTerritoryConversationListenerComponent::ServerAbandon_Implementation(FGuid Session)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !SessionID.IsValid() || Session != SessionID) return;
	if (PendingMeeting.IsValid()) PendingMeeting->CancelConversation(TEXT("Listening client became unavailable or entered a cutscene."));
	else Release(Session);
}

void UTerritoryConversationListenerComponent::Release(const FGuid& Session)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Session != SessionID || !Session.IsValid()) return;
	// This follows Native's exit message on the same owning PC channel.
	if (!CastChecked<ANarrativePlayerController>(GetOwner())->IsLocalController()) ClientRelease(Session);
	ResetContext();
}

void UTerritoryConversationListenerComponent::ClientRelease_Implementation(FGuid Session)
{
	if (Session == SessionID) ResetContext();
}

void UTerritoryConversationListenerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (PendingMeeting.IsValid()) PendingMeeting->CancelConversation(TEXT("Listener disconnected or unloaded."));
	ResetContext();
	Super::EndPlay(Reason);
}
