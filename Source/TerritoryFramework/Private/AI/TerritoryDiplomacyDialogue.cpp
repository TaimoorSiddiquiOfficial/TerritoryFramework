#include "AI/TerritoryDiplomacyDialogue.h"

#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Tales/Dialogue.h"
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
	if (bSameFaction && SameFactionDialogue)
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
	const FGameplayTagContainer InteractorFactions = InteractorTeam->GetFactions();
	bOutSameFaction = OwnerFactions.HasAnyExact(InteractorFactions);
	if (bOutSameFaction)
	{
		return EDiplomacyState::Alliance;
	}

	EDiplomacyState Strongest = EDiplomacyState::None;
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

UTerritoryDiplomacyDialogueProfile*
UTerritoryDiplomacyDialogueComponent::GetDialogueProfileForOwner() const
{
	const INarrativeTeamAgentInterface* OwnerTeam =
		Cast<INarrativeTeamAgentInterface>(GetOwner());
	if (OwnerTeam)
	{
		const FGameplayTagContainer OwnerFactions = OwnerTeam->GetFactions();
		for (const FTerritoryFactionDialogueProfile& Mapping : FactionDialogueProfiles)
		{
			if (Mapping.Faction.IsValid() && Mapping.DialogueProfile
				&& OwnerFactions.HasTagExact(Mapping.Faction))
			{
				return Mapping.DialogueProfile;
			}
		}
	}
	return DialogueProfile;
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
