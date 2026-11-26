#include "Cinematics/TerritoryNarrativeQTEBlueprintLibrary.h"

#include "AbilitySystemGlobals.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Core/TerritoryTypes.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "LevelSequenceDirector.h"
#include "LevelSequencePlayer.h"

ETerritoryNarrativeQTEFailureResult
UTerritoryNarrativeQTEBlueprintLibrary::ApplyNarrativeQTEFailureInstakill(
	ULevelSequenceDirector* SequenceDirector)
{
	if (!IsValid(SequenceDirector) || !IsValid(SequenceDirector->Player))
	{
		return ETerritoryNarrativeQTEFailureResult::InvalidDirector;
	}

	ANarrativeLevelSequenceActor* SequenceActor =
		SequenceDirector->GetTypedOuter<ANarrativeLevelSequenceActor>();
	if (!IsValid(SequenceActor))
	{
		SequenceActor = Cast<ANarrativeLevelSequenceActor>(SequenceDirector->Player->GetOuter());
	}
	if (!IsValid(SequenceActor))
	{
		return ETerritoryNarrativeQTEFailureResult::MissingSequenceActor;
	}

	if (!SequenceActor->HasAuthority())
	{
		return ETerritoryNarrativeQTEFailureResult::NotAuthority;
	}

	if (SequenceActor->OwnerControllers.Num() != 1
		|| !IsValid(SequenceActor->OwnerControllers[0]))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("Rejected failed QTE for %s: expected exactly one explicit owning controller, found %d."),
			*GetNameSafe(SequenceActor), SequenceActor->OwnerControllers.Num());
		return ETerritoryNarrativeQTEFailureResult::InvalidOwnerContext;
	}

	APlayerController* TargetController = SequenceActor->OwnerControllers[0];
	UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetController));
	if (!IsValid(NarrativeASC))
	{
		return ETerritoryNarrativeQTEFailureResult::MissingAbilitySystem;
	}

	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.State.Invulnerable"), false);
	const bool bSequenceAuthoredInvulnerability = InvulnerableTag.IsValid()
		&& SequenceActor->NarrativeSequenceParams.TagsToApplyWhilstBound.HasTagExact(InvulnerableTag)
		&& NarrativeASC->GetGameplayTagCount(InvulnerableTag) > 0;

	if (bSequenceAuthoredInvulnerability)
	{
		NarrativeASC->RemoveLooseGameplayTag(
			InvulnerableTag, 1, EGameplayTagReplicationState::CountToOwner);
	}

	NarrativeASC->Instakill();

	if (bSequenceAuthoredInvulnerability)
	{
		// Restore the sequence-owned count. ANarrativeLevelSequenceActor::OnStop will remove it once,
		// preserving any independent invulnerability count owned by another gameplay system.
		NarrativeASC->AddLooseGameplayTag(
			InvulnerableTag, 1, EGameplayTagReplicationState::CountToOwner);
	}

	return ETerritoryNarrativeQTEFailureResult::Applied;
}
