#include "AI/TerritoryDiplomacyDialogue.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Tales/Dialogue.h"
#include "Tales/TaggedDialogueSet.h"
#include "AI/NPCDefinition.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace
{
	int32 GetRelationshipPriority(EDiplomacyState State)
	{
		switch (State)
		{
		case EDiplomacyState::War: return 6;
		case EDiplomacyState::Alliance: return 5;
		case EDiplomacyState::TradeAgreement: return 4;
		case EDiplomacyState::NonAggression: return 3;
		case EDiplomacyState::Ceasefire: return 2;
		case EDiplomacyState::None:
		default: return 1;
		}
	}
}

TSubclassOf<UDialogue> UTerritoryDiplomacyDialogueProfile::GetDialogueForRelationship(
	EDiplomacyState Relationship, bool bSameFaction,
	TSubclassOf<UDialogue> DefaultDialogue) const
{
	if (bSameFaction && Relationship != EDiplomacyState::War && SameFactionDialogue)
	{
		return SameFactionDialogue;
	}

	TSubclassOf<UDialogue> Selected;
	switch (Relationship)
	{
	case EDiplomacyState::War: Selected = WarDialogue; break;
	case EDiplomacyState::Alliance: Selected = AllianceDialogue; break;
	case EDiplomacyState::TradeAgreement: Selected = TradeAgreementDialogue; break;
	case EDiplomacyState::NonAggression: Selected = NonAggressionDialogue; break;
	case EDiplomacyState::Ceasefire: Selected = CeasefireDialogue; break;
	case EDiplomacyState::None:
	default: Selected = NeutralDialogue; break;
	}
	return Selected ? Selected : DefaultDialogue;
}

UTerritoryDiplomacyDialogueComponent::UTerritoryDiplomacyDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UTerritoryDiplomacyDialogueComponent::SetDialogueProfiles(
	UTerritoryDiplomacyDialogueProfile* InFallbackProfile,
	const TArray<FTerritoryFactionDialogueProfile>& InFactionProfiles)
{
	DialogueProfile = InFallbackProfile;
	FactionDialogueProfiles = InFactionProfiles;
}

EDiplomacyState UTerritoryDiplomacyDialogueComponent::ResolveRelationshipForInteractor(
	const APawn* Interactor, bool& bOutSameFaction) const
{
	bOutSameFaction = false;
	const AActor* OwnerActor = GetOwner();
	const INarrativeTeamAgentInterface* OwnerTeam =
		Cast<INarrativeTeamAgentInterface>(OwnerActor);
	const INarrativeTeamAgentInterface* InteractorTeam =
		Cast<INarrativeTeamAgentInterface>(Interactor);
	const UWorld* World = GetWorld();
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>() : nullptr;
	if (!OwnerTeam || !InteractorTeam || !Diplomacy)
	{
		return EDiplomacyState::None;
	}

	const FGameplayTagContainer OwnerFactions = OwnerTeam->GetFactions();
	const ATerritoryGuardCharacter* TerritoryGuard =
		Cast<ATerritoryGuardCharacter>(OwnerActor);
	const ATerritoryVolume* Territory = TerritoryGuard
		? TerritoryGuard->GetOwningTerritory() : nullptr;
	const FGameplayTag OwnerPrimaryFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, GetOwner());
	const UTerritoryDisguiseSubsystem* Disguises = World
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	const FGameplayTagContainer InteractorFactions = Disguises
		? Disguises->ResolvePerceivedFactions(
			Interactor, Territory, OwnerPrimaryFaction)
		: InteractorTeam->GetFactions();
	bOutSameFaction = OwnerFactions.HasAnyExact(InteractorFactions);
	EDiplomacyState Strongest = bOutSameFaction ? EDiplomacyState::Alliance : EDiplomacyState::None;
	for (const FGameplayTag& OwnerFaction : OwnerFactions)
	{
		for (const FGameplayTag& InteractorFaction : InteractorFactions)
		{
			if (!OwnerFaction.IsValid() || !InteractorFaction.IsValid()
				|| OwnerFaction == InteractorFaction)
			{
				continue;
			}
			const EDiplomacyState Candidate = Diplomacy->GetDiplomacyState(
				OwnerFaction, InteractorFaction);
			if (GetRelationshipPriority(Candidate) > GetRelationshipPriority(Strongest))
			{
				Strongest = Candidate;
			}
		}
	}
	// A shared membership must not bypass the hostile pair or select SameFactionDialogue.
	if (Strongest == EDiplomacyState::War) bOutSameFaction = false;
	return Strongest;
}

TSubclassOf<UDialogue> UTerritoryDiplomacyDialogueComponent::ResolveDialogueForInteractor(
	const APawn* Interactor, TSubclassOf<UDialogue> DefaultDialogue) const
{
	const UTerritoryDiplomacyDialogueProfile* ResolvedProfile =
		GetDialogueProfileForOwner();
	if (!ResolvedProfile)
	{
		return DefaultDialogue;
	}
	bool bSameFaction = false;
	const EDiplomacyState Relationship = ResolveRelationshipForInteractor(
		Interactor, bSameFaction);
	return ResolvedProfile->GetDialogueForRelationship(
		Relationship, bSameFaction, DefaultDialogue);
}

ETerritoryDialogueState UTerritoryDiplomacyDialogueComponent::ApplyDialogueStateFromInteractor(
	const APawn* Interactor)
{
	bool bSameFaction = false;
	const EDiplomacyState Relationship = ResolveRelationshipForInteractor(Interactor, bSameFaction);
	// One mapping table owns the relationship -> state decision; the tag layer only mirrors it.
	const FGameplayTag Tag = TerritoryDialogueTags::GetDialogueStateTag(Relationship, bSameFaction);
	const ETerritoryDialogueState NewState = TerritoryDialogueTags::GetDialogueStateForTag(Tag);

	ApplyDialogueState(NewState);
	return NewState;
}

UAbilitySystemComponent* UTerritoryDiplomacyDialogueComponent::ResolveOwnerAbilitySystem() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	if (IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(OwnerActor))
	{
		return AbilityInterface->GetAbilitySystemComponent();
	}

	return nullptr;
}

bool UTerritoryDiplomacyDialogueComponent::ApplyDialogueState(ETerritoryDialogueState NewState)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		// Client-side reads are welcome; state changes are not.
		return false;
	}

	UAbilitySystemComponent* ASC = ResolveOwnerAbilitySystem();
	if (!ASC)
	{
		return false;
	}

	const FGameplayTag NewTag = TerritoryDialogueTags::GetDialogueStateTagForState(NewState);
	if (!NewTag.IsValid())
	{
		return false;
	}

	if (AppliedStateTag.IsValid() && AppliedStateTag != NewTag)
	{
		ASC->RemoveLooseGameplayTag(AppliedStateTag, 1);
	}

	ASC->AddLooseGameplayTag(NewTag, 1);
	AppliedStateTag = NewTag;
	DialogueState = NewState;
	return true;
}

void UTerritoryDiplomacyDialogueComponent::ClearDialogueState()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (AppliedStateTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ResolveOwnerAbilitySystem())
		{
			ASC->RemoveLooseGameplayTag(AppliedStateTag, 1);
		}
		AppliedStateTag = FGameplayTag();
	}

	DialogueState = ETerritoryDialogueState::Neutral;
}

bool UTerritoryDiplomacyDialogueComponent::RequestTaggedDialogue(FGameplayTag Tag,
	AActor* DialogueInstigator)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !Tag.IsValid())
	{
		return false;
	}

	ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(OwnerActor);
	if (!NPC)
	{
		return false;
	}

	// Single authority for a Territory tagged line. Narrative's own PlayTaggedDialogue path ends in
	// ExecutePlayTaggedDialogue, a BlueprintImplementableEvent that nothing implements in this
	// project, so that route only stamps LastPlayTime and is never actually heard. This component
	// owns the whole chain for guard, assault and story NPCs: resolve the row, apply the NPC's own
	// required/blocked gate, then play it through the listener's Tales component - the same call
	// Narrative's own NPCInteractable makes. The row is read from the definition's set asset because
	// ANarrativeNPCCharacter::TaggedDialogues is protected, and the character copies its rows from
	// exactly that asset at spawn.
	UNPCDefinition* Definition = NPC->GetNPCDefinition();
	UTaggedDialogueSet* Set = Definition ? Definition->TaggedDialogueSet.LoadSynchronous() : nullptr;
	const FTaggedDialogue* Row = Set
		? Set->TaggedDialogues.FindByPredicate(
			[&Tag](const FTaggedDialogue& Candidate) { return Candidate.Tag == Tag; })
		: nullptr;
	if (!Row || Row->Dialogue.IsNull())
	{
		return false;
	}

	if (const UNarrativeAbilitySystemComponent* ASC = NPC->GetNarrativeAbilitySystemComponent())
	{
		if (!ASC->HasAllMatchingGameplayTags(Row->RequiredTags)
			|| ASC->HasAnyMatchingGameplayTags(Row->BlockedTags))
		{
			return false;
		}
	}

	AActor* Listener = DialogueInstigator ? DialogueInstigator : OwnerActor;
	if (const APawn* ListenerPawn = Cast<APawn>(Listener))
	{
		if (Row->MaxDistance > 0.f
			&& FVector::DistSquared(ListenerPawn->GetActorLocation(), OwnerActor->GetActorLocation())
				> FMath::Square(Row->MaxDistance))
		{
			return false;
		}
	}

	// Narrative's own bookkeeping: per-instance cooldown, LastPlayTime, and vendor callers that
	// expect the row to have been consulted. This is also what fires a Territory NPC Blueprint's
	// ExecutePlayTaggedDialogue override when one exists.
	NPC->PlayTaggedDialogue(Tag, Listener);

	// A Blueprint hook may have played the line from that event already - never start a second
	// dialogue on the same listener.
	if (UTalesComponent* ListenerTales =
		UNarrativeFunctionLibrary::GetTalesComponentFromTarget(Listener);
		ListenerTales && ListenerTales->GetCurrentDialogue())
	{
		return true;
	}

	// No hook implementation (or it did nothing): play it natively. One playback rule for both routes.
	return UTerritoryBlueprintLibrary::PlayTaggedDialogueLine(*Row, NPC, Listener);
}

void UTerritoryDiplomacyDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearDialogueState();
	Super::EndPlay(EndPlayReason);
}

UTerritoryDiplomacyDialogueProfile*
UTerritoryDiplomacyDialogueComponent::GetDialogueProfileForOwner() const
{
	// Guard behavior is authored on the replicated Territory Definition. The
	// server's transient cache is not populated on clients, and can predate a
	// streamed owner or a definition replacement. Resolve that authority at use.
	const ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(GetOwner());
	const ATerritoryVolume* Territory = Guard ? Guard->GetOwningTerritory() : nullptr;
	const UTerritoryDefinition* Definition = Territory ? Territory->GetTerritoryDefinition() : nullptr;
	const auto& Profiles = Definition ? Definition->GuardBehavior.FactionDialogueProfiles : FactionDialogueProfiles;
	UTerritoryDiplomacyDialogueProfile* Fallback = Definition ? Definition->GuardBehavior.DialogueProfile.Get() : DialogueProfile.Get();
	const INarrativeTeamAgentInterface* OwnerTeam =
		Cast<INarrativeTeamAgentInterface>(GetOwner());
	if (OwnerTeam)
	{
		const FGameplayTagContainer OwnerFactions = OwnerTeam->GetFactions();
		for (const FTerritoryFactionDialogueProfile& Mapping : Profiles)
		{
			if (Mapping.Faction.IsValid() && Mapping.DialogueProfile
				&& OwnerFactions.HasTagExact(Mapping.Faction))
			{
				return Mapping.DialogueProfile;
			}
		}
	}
	return Fallback;
}

UTerritoryDiplomacyInteractable::UTerritoryDiplomacyInteractable(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UTerritoryDiplomacyInteractable::Interact(APawn* Interactor,
	UNarrativeInteractionComponent* InteractionComp)
{
	const TSubclassOf<UDialogue> OriginalDialogue = Dialogue;
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const UTerritoryDiplomacyDialogueComponent* Resolver =
			OwnerActor->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>())
		{
			Dialogue = Resolver->ResolveDialogueForInteractor(Interactor, OriginalDialogue);
		}
	}
	const bool bResult = Super::Interact(Interactor, InteractionComp);
	Dialogue = OriginalDialogue;
	return bResult;
}

bool UTerritoryDiplomacyInteractable::CanInteract_Implementation(APawn* Interactor,
	UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText)
{
	const TSubclassOf<UDialogue> OriginalDialogue = Dialogue;
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const UTerritoryDiplomacyDialogueComponent* Resolver =
			OwnerActor->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>())
		{
			Dialogue = Resolver->ResolveDialogueForInteractor(Interactor, OriginalDialogue);
		}
	}
	const bool bResult = Super::CanInteract_Implementation(
		Interactor, InteractionComp, OutErrorText);
	Dialogue = OriginalDialogue;
	return bResult;
}
