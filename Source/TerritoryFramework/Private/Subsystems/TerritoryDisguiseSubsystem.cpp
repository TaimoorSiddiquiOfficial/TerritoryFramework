#include "Subsystems/TerritoryDisguiseSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryStealthTags.h"
#include "Core/TerritoryVolume.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace
{
	FGameplayTag GetChangeEventTag(const UTerritoryDisguiseProfile& Profile,
		ETerritoryDisguiseChange Change)
	{
		switch (Change)
		{
		case ETerritoryDisguiseChange::Activated: return Profile.ActivatedEventTag;
		case ETerritoryDisguiseChange::Removed: return Profile.RemovedEventTag;
		case ETerritoryDisguiseChange::Compromised: return Profile.CompromisedEventTag;
		case ETerritoryDisguiseChange::Restored: return Profile.RestoredEventTag;
		case ETerritoryDisguiseChange::IdentityCheckPassed:
			return Profile.IdentityCheckPassedEventTag;
		case ETerritoryDisguiseChange::IdentityCheckFailed:
			return Profile.IdentityCheckFailedEventTag;
		default: return FGameplayTag();
		}
	}
}

bool UTerritoryDisguiseSubsystem::ActivateDisguise(AActor* Target,
	UTerritoryDisguiseProfile* Profile, UObject* SourceObject)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Target)
		|| !IsValid(Profile) || !Profile->PerceivedFaction.IsValid())
	{
		return false;
	}
	if (ActiveDisguises.Contains(Target))
	{
		RemoveDisguise(Target, nullptr);
	}
	FDisguiseRuntime& Runtime = ActiveDisguises.Add(Target);
	Runtime.Profile = Profile;
	Runtime.SourceObject = SourceObject;
	RefreshStateTags(Target, Runtime);
	EmitChange(Target, Runtime, ETerritoryDisguiseChange::Activated,
		Profile->PerceivedFaction, nullptr);
	return true;
}

bool UTerritoryDisguiseSubsystem::RemoveDisguise(AActor* Target, UObject* SourceObject)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Target) return false;
	FDisguiseRuntime* Runtime = ActiveDisguises.Find(Target);
	if (!Runtime || SourceObject && Runtime->SourceObject.IsValid()
		&& Runtime->SourceObject.Get() != SourceObject)
	{
		return false;
	}
	RemoveStateTags(*Runtime);
	EmitChange(Target, *Runtime, ETerritoryDisguiseChange::Removed,
		Runtime->Profile ? Runtime->Profile->PerceivedFaction : FGameplayTag(), nullptr);
	ActiveDisguises.Remove(Target);
	return true;
}

bool UTerritoryDisguiseSubsystem::CompromiseDisguise(AActor* Target,
	FGameplayTag ObserverFaction, ATerritoryVolume* Territory)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Target) return false;
	FDisguiseRuntime* Runtime = ActiveDisguises.Find(Target);
	if (!Runtime || !Runtime->Profile
		|| IsCompromisedForFaction(*Runtime, ObserverFaction))
	{
		return false;
	}
	if (ObserverFaction.IsValid()) Runtime->CompromisedFactions.Add(ObserverFaction);
	else Runtime->bCompromisedForEveryone = true;
	RefreshStateTags(Target, *Runtime);
	EmitChange(Target, *Runtime, ETerritoryDisguiseChange::Compromised,
		ObserverFaction, Territory);
	return true;
}

bool UTerritoryDisguiseSubsystem::RestoreDisguise(AActor* Target,
	FGameplayTag ObserverFaction)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Target) return false;
	FDisguiseRuntime* Runtime = ActiveDisguises.Find(Target);
	if (!Runtime || !Runtime->Profile) return false;
	bool bChanged = false;
	if (ObserverFaction.IsValid())
	{
		bChanged = Runtime->CompromisedFactions.Remove(ObserverFaction) > 0;
	}
	else
	{
		bChanged = Runtime->bCompromisedForEveryone
			|| !Runtime->CompromisedFactions.IsEmpty();
		Runtime->bCompromisedForEveryone = false;
		Runtime->CompromisedFactions.Empty();
	}
	if (!bChanged) return false;
	RefreshStateTags(Target, *Runtime);
	EmitChange(Target, *Runtime, ETerritoryDisguiseChange::Restored,
		ObserverFaction, nullptr);
	return true;
}

bool UTerritoryDisguiseSubsystem::GetDisguiseSnapshot(const AActor* Target,
	FTerritoryDisguiseSnapshot& OutSnapshot) const
{
	OutSnapshot = FTerritoryDisguiseSnapshot();
	const FDisguiseRuntime* Runtime = Target ? ActiveDisguises.Find(Target) : nullptr;
	if (!Runtime || !Runtime->Profile) return false;
	OutSnapshot = MakeSnapshot(Target, *Runtime);
	return true;
}

bool UTerritoryDisguiseSubsystem::IsDisguiseAccepted(const AActor* Target,
	const ATerritoryVolume* Territory, FGameplayTag ObserverFaction,
	FText& OutReason) const
{
	OutReason = FText::GetEmpty();
	const FDisguiseRuntime* Runtime = Target ? ActiveDisguises.Find(Target) : nullptr;
	const UTerritoryStealthProfile* Stealth = Territory
		? Territory->GetActiveStealthProfile() : nullptr;
	if (!Runtime || !Runtime->Profile)
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseMissing", "No disguise is active.");
		return false;
	}
	if (!Territory || !Stealth)
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseNotAllowed",
			"This Territory does not accept disguises.");
		return false;
	}
	if (!ObserverFaction.IsValid()) ObserverFaction = Territory->GetOwningFaction();
	return EvaluateDisguisePolicy(Runtime->Profile, Stealth,
		Territory->GetOwningFaction(), ObserverFaction,
		IsCompromisedForFaction(*Runtime, ObserverFaction), OutReason);
}

bool UTerritoryDisguiseSubsystem::EvaluateDisguisePolicy(
	const UTerritoryDisguiseProfile* Disguise,
	const UTerritoryStealthProfile* TerritoryPolicy,
	FGameplayTag OwningFaction, FGameplayTag ObserverFaction,
	bool bCompromised, FText& OutReason)
{
	OutReason = FText::GetEmpty();
	if (!Disguise || !Disguise->PerceivedFaction.IsValid())
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseMissingPolicy",
			"No valid disguise identity is active.");
		return false;
	}
	if (!TerritoryPolicy || !TerritoryPolicy->bAllowFactionDisguises)
	{
		OutReason = NSLOCTEXT("Territory", "DisguisePolicyDisabled",
			"This Territory does not accept disguises.");
		return false;
	}
	if (!ObserverFaction.IsValid()
		|| Disguise->PerceivedFaction != ObserverFaction)
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseWrongFaction",
			"The uniform belongs to a different faction.");
		return false;
	}
	if (TerritoryPolicy->bRequireOwningFactionDisguise
		&& Disguise->PerceivedFaction != OwningFaction)
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseWrongOwner",
			"The uniform does not match the faction controlling this Territory.");
		return false;
	}
	if (bCompromised)
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseBurned",
			"This faction has already identified the wearer.");
		return false;
	}
	if (Disguise->Quality < TerritoryPolicy->MinimumDisguiseQuality)
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseQualityLow",
			"The uniform is not convincing enough for this security level.");
		return false;
	}
	if (!Disguise->ClearanceTags.HasAllExact(
		TerritoryPolicy->RequiredDisguiseClearanceTags))
	{
		OutReason = NSLOCTEXT("Territory", "DisguiseClearanceMissing",
			"The uniform does not have the required clearance.");
		return false;
	}
	OutReason = NSLOCTEXT("Territory", "DisguiseAccepted",
		"The observer accepts the disguised identity.");
	return true;
}

bool UTerritoryDisguiseSubsystem::PerformIdentityCheck(AActor* Target,
	ATerritoryVolume* Territory, FGameplayTag ObserverFaction, FText& OutReason)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !Target) return false;
	if (!ObserverFaction.IsValid() && Territory)
	{
		ObserverFaction = Territory->GetOwningFaction();
	}
	FDisguiseRuntime* Runtime = ActiveDisguises.Find(Target);
	if (!Runtime || !Runtime->Profile)
	{
		FTerritoryDisguiseSnapshot EmptySnapshot;
		OnDisguiseChanged.Broadcast(Target,
			ETerritoryDisguiseChange::IdentityCheckFailed, ObserverFaction,
			Territory, EmptySnapshot);
		FGameplayEventData Payload;
		Payload.EventTag = TerritoryStealthTags::DisguiseCheckFailedEvent.GetTag();
		Payload.Instigator = Territory;
		Payload.Target = Target;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target,
			TerritoryStealthTags::DisguiseCheckFailedEvent.GetTag(), Payload);
		OutReason = NSLOCTEXT("Territory", "IdentityCheckNoDisguise",
			"The player is not wearing a disguise.");
		return false;
	}
	const bool bPassed = IsDisguiseAccepted(
		Target, Territory, ObserverFaction, OutReason);
	EmitChange(Target, *Runtime, bPassed
		? ETerritoryDisguiseChange::IdentityCheckPassed
		: ETerritoryDisguiseChange::IdentityCheckFailed,
		ObserverFaction, Territory);
	const UTerritoryStealthProfile* Stealth = Territory
		? Territory->GetActiveStealthProfile() : nullptr;
	if (!bPassed && Stealth && Stealth->bCompromiseDisguiseOnFailedIdentityCheck)
	{
		CompromiseDisguise(Target, ObserverFaction, Territory);
	}
	return bPassed;
}

FGameplayTagContainer UTerritoryDisguiseSubsystem::ResolvePerceivedFactions(
	const AActor* Target, const ATerritoryVolume* Territory,
	FGameplayTag ObserverFaction) const
{
	FText Reason;
	if (Target && IsDisguiseAccepted(Target, Territory, ObserverFaction, Reason))
	{
		if (const FDisguiseRuntime* Runtime = ActiveDisguises.Find(Target))
		{
			FGameplayTagContainer Result;
			Result.AddTag(Runtime->Profile->PerceivedFaction);
			return Result;
		}
	}
	if (const INarrativeTeamAgentInterface* Team =
		Cast<const INarrativeTeamAgentInterface>(Target))
	{
		return Team->GetFactions();
	}
	return FGameplayTagContainer();
}

bool UTerritoryDisguiseSubsystem::ProcessStealthEvidence(AActor* Target,
	ATerritoryVolume* Territory, AActor* Observer,
	ETerritoryStealthEvidence Evidence, bool bConfirmedIdentity)
{
	FDisguiseRuntime* Runtime = Target ? ActiveDisguises.Find(Target) : nullptr;
	if (!Runtime || !Runtime->Profile || !Territory) return false;
	const FGameplayTag ObserverFaction = ResolveObserverFaction(Territory, Observer);
	FText Reason;
	const bool bAccepted = IsDisguiseAccepted(
		Target, Territory, ObserverFaction, Reason);
	// A convincing uniform does not make the player optically invisible. The guard
	// sees them and accepts the identity, including at point-blank range. Explicit
	// checkpoint/reveal events remain able to test or burn the cover.
	if (Evidence == ETerritoryStealthEvidence::Sight && bAccepted)
	{
		return true;
	}

	bool bBurn = bConfirmedIdentity;
	switch (Evidence)
	{
	case ETerritoryStealthEvidence::FireSeen:
		bBurn |= Runtime->Profile->bCompromiseWhenFiringWhileSeen;
		break;
	case ETerritoryStealthEvidence::Damage:
		bBurn |= Runtime->Profile->bCompromiseWhenDealingDamage;
		break;
	case ETerritoryStealthEvidence::DefenderKilledSeen:
		bBurn |= Runtime->Profile->bCompromiseWhenDefenderKillIsSeen;
		break;
	case ETerritoryStealthEvidence::Scripted:
		bBurn |= Runtime->Profile->bCompromiseFromScriptedReveal;
		break;
	default:
		break;
	}
	if (bBurn)
	{
		CompromiseDisguise(Target, ObserverFaction, Territory);
	}
	return false;
}

FGameplayTag UTerritoryDisguiseSubsystem::ResolveObserverFaction(
	const ATerritoryVolume* Territory, const AActor* Observer) const
{
	if (const INarrativeTeamAgentInterface* Team =
		Cast<const INarrativeTeamAgentInterface>(Observer))
	{
		for (const FGameplayTag& Faction : Team->GetFactions())
		{
			if (Faction.IsValid()) return Faction;
		}
	}
	return Territory ? Territory->GetOwningFaction() : FGameplayTag();
}

bool UTerritoryDisguiseSubsystem::IsCompromisedForFaction(
	const FDisguiseRuntime& Runtime, FGameplayTag ObserverFaction) const
{
	return Runtime.bCompromisedForEveryone
		|| ObserverFaction.IsValid() && Runtime.CompromisedFactions.Contains(ObserverFaction);
}

FTerritoryDisguiseSnapshot UTerritoryDisguiseSubsystem::MakeSnapshot(
	const AActor* Target, const FDisguiseRuntime& Runtime) const
{
	FTerritoryDisguiseSnapshot Snapshot;
	Snapshot.bActive = Runtime.Profile != nullptr;
	Snapshot.bCompromisedForEveryone = Runtime.bCompromisedForEveryone;
	Snapshot.Profile = Runtime.Profile;
	if (Runtime.Profile)
	{
		Snapshot.PerceivedFaction = Runtime.Profile->PerceivedFaction;
		Snapshot.Quality = Runtime.Profile->Quality;
		Snapshot.ClearanceTags = Runtime.Profile->ClearanceTags;
	}
	Snapshot.TrueFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(
		this, const_cast<AActor*>(Target));
	Snapshot.CompromisedForFactions.Reserve(Runtime.CompromisedFactions.Num());
	for (const FGameplayTag& CompromisedFaction : Runtime.CompromisedFactions)
	{
		Snapshot.CompromisedForFactions.Add(CompromisedFaction);
	}
	return Snapshot;
}

void UTerritoryDisguiseSubsystem::RefreshStateTags(AActor* Target,
	FDisguiseRuntime& Runtime)
{
	UNarrativeAbilitySystemComponent* ASC = Target
		? Cast<UNarrativeAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target)) : nullptr;
	if (!ASC) return;
	if (!Runtime.ActiveTagHandle.IsValid())
	{
		FGameplayTagContainer Tags(TerritoryStealthTags::DisguiseActive);
		Runtime.ActiveTagHandle = ASC->AddDynamicTagsGameplayEffect(Tags);
	}
	const bool bAnyCompromised = Runtime.bCompromisedForEveryone
		|| !Runtime.CompromisedFactions.IsEmpty();
	if (bAnyCompromised && !Runtime.CompromisedTagHandle.IsValid())
	{
		FGameplayTagContainer Tags(TerritoryStealthTags::DisguiseCompromised);
		Runtime.CompromisedTagHandle = ASC->AddDynamicTagsGameplayEffect(Tags);
	}
	else if (!bAnyCompromised && Runtime.CompromisedTagHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(Runtime.CompromisedTagHandle);
		Runtime.CompromisedTagHandle.Invalidate();
	}
}

void UTerritoryDisguiseSubsystem::RemoveStateTags(FDisguiseRuntime& Runtime)
{
	if (UAbilitySystemComponent* ASC = Runtime.ActiveTagHandle.IsValid()
		? Runtime.ActiveTagHandle.GetOwningAbilitySystemComponent() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(Runtime.ActiveTagHandle);
	}
	if (UAbilitySystemComponent* ASC = Runtime.CompromisedTagHandle.IsValid()
		? Runtime.CompromisedTagHandle.GetOwningAbilitySystemComponent() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(Runtime.CompromisedTagHandle);
	}
	Runtime.ActiveTagHandle.Invalidate();
	Runtime.CompromisedTagHandle.Invalidate();
}

void UTerritoryDisguiseSubsystem::EmitChange(AActor* Target,
	FDisguiseRuntime& Runtime, ETerritoryDisguiseChange Change,
	FGameplayTag ObserverFaction, ATerritoryVolume* Territory)
{
	if (!Target || !Runtime.Profile) return;
	const FTerritoryDisguiseSnapshot Snapshot = MakeSnapshot(Target, Runtime);
	FTerritoryDisguiseSnapshot EventSnapshot = Snapshot;
	if (Change == ETerritoryDisguiseChange::Removed)
	{
		EventSnapshot.bActive = false;
	}
	OnDisguiseChanged.Broadcast(Target, Change, ObserverFaction, Territory, EventSnapshot);
	const FGameplayTag EventTag = GetChangeEventTag(*Runtime.Profile, Change);
	if (EventTag.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = Territory;
		Payload.Target = Target;
		Payload.OptionalObject = Runtime.Profile;
		Payload.EventMagnitude = Runtime.Profile->Quality;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Target, EventTag, Payload);
	}
}
