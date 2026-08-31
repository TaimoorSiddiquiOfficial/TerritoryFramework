#include "Tales/TerritoryStealthConditions.h"

#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryTalesUtilities.h"

namespace
{
	ATerritoryVolume* ResolveStealthTerritory(const UObject* Context, UWorld* World,
		const FGameplayTag& TerritoryTag, const APawn* Target)
	{
		if (ATerritoryVolume* Containing = Context
			? Context->GetTypedOuter<ATerritoryVolume>() : nullptr)
		{
			if (!TerritoryTag.IsValid() || Containing->GetTerritoryTag() == TerritoryTag)
			{
				return Containing;
			}
		}
		UTerritoryRegistrySubsystem* Registry = World
			? World->GetSubsystem<UTerritoryRegistrySubsystem>() : nullptr;
		if (!Registry) return nullptr;
		if (TerritoryTag.IsValid()) return Registry->GetTerritoryByTag(TerritoryTag);
		return Target ? Registry->GetTerritoryAtLocation(Target->GetActorLocation()) : nullptr;
	}
}

UTerritoryStealthPolicyCondition::UTerritoryStealthPolicyCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
}

bool UTerritoryStealthPolicyCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	ATerritoryVolume* Territory = ResolveStealthTerritory(
		this, World, TerritoryToCheck, Target);
	const UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	return Control && Territory
		&& Control->IsStealthInfiltrationEnabled(Territory) == bRequireEnabled;
}

FString UTerritoryStealthPolicyCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Territory stealth infiltration is %s"),
		bRequireEnabled ? TEXT("enabled") : TEXT("disabled"));
}

UTerritoryExposureCondition::UTerritoryExposureCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
}

bool UTerritoryExposureCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	ATerritoryVolume* Territory = ResolveStealthTerritory(
		this, World, TerritoryToCheck, Target);
	const UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	if (!Control || !Territory || !Target) return false;
	if (RequiredExposure == ETerritoryExposureRequirement::ExposedOrStealthDisabled
		&& !Control->IsStealthInfiltrationEnabled(Territory))
	{
		return true;
	}
	FTerritoryInfiltrationSnapshot Snapshot;
	if (!Control->GetInfiltrationSnapshot(Territory, Target, Snapshot)) return false;
	switch (RequiredExposure)
	{
	case ETerritoryExposureRequirement::Undetected:
		return Snapshot.ExposureState == ETerritoryExposureState::Undetected;
	case ETerritoryExposureRequirement::Suspicious:
		return Snapshot.ExposureState == ETerritoryExposureState::Suspicious;
	case ETerritoryExposureRequirement::Exposed:
	case ETerritoryExposureRequirement::ExposedOrStealthDisabled:
		return Snapshot.ExposureState == ETerritoryExposureState::Exposed;
	default:
		return false;
	}
}

FString UTerritoryExposureCondition::GetGraphDisplayText_Implementation()
{
	const UEnum* Enum = StaticEnum<ETerritoryExposureRequirement>();
	return FString::Printf(TEXT("Territory target is %s"), Enum
		? *Enum->GetDisplayNameTextByValue(static_cast<int64>(RequiredExposure)).ToString()
		: TEXT("in the required exposure state"));
}

UTerritoryStealthEvidenceCondition::UTerritoryStealthEvidenceCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
}

bool UTerritoryStealthEvidenceCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	ATerritoryVolume* Territory = ResolveStealthTerritory(
		this, World, TerritoryToCheck, Target);
	const UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	FTerritoryInfiltrationSnapshot Snapshot;
	if (!Control || !Territory || !Target
		|| !Control->GetInfiltrationSnapshot(Territory, Target, Snapshot)
		|| Snapshot.LastEvidence != RequiredEvidence)
	{
		return false;
	}
	return MaximumEvidenceAge <= 0.f || Snapshot.LastEvidenceWorldTime >= 0.f
		&& World->GetTimeSeconds() - Snapshot.LastEvidenceWorldTime <= MaximumEvidenceAge;
}

FString UTerritoryStealthEvidenceCondition::GetGraphDisplayText_Implementation()
{
	const UEnum* Enum = StaticEnum<ETerritoryStealthEvidence>();
	return FString::Printf(TEXT("Last Territory stealth evidence is %s"), Enum
		? *Enum->GetDisplayNameTextByValue(static_cast<int64>(RequiredEvidence)).ToString()
		: TEXT("the required evidence"));
}

UTerritorySuspicionCondition::UTerritorySuspicionCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
}

bool UTerritorySuspicionCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	ATerritoryVolume* Territory = ResolveStealthTerritory(
		this, World, TerritoryToCheck, Target);
	const UTerritoryControlSubsystem* Control = World
		? World->GetSubsystem<UTerritoryControlSubsystem>() : nullptr;
	FTerritoryInfiltrationSnapshot Snapshot;
	return Control && Territory && Target
		&& Control->GetInfiltrationSnapshot(Territory, Target, Snapshot)
		&& Snapshot.Suspicion * 100.f >= MinimumSuspicionPercent;
}

FString UTerritorySuspicionCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Territory suspicion is at least %.0f%%"),
		MinimumSuspicionPercent);
}

UTerritoryDisguiseCondition::UTerritoryDisguiseCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
}

bool UTerritoryDisguiseCondition::CheckCondition_Implementation(
	APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	const UTerritoryDisguiseSubsystem* Disguises = World
		? World->GetSubsystem<UTerritoryDisguiseSubsystem>() : nullptr;
	FTerritoryDisguiseSnapshot Snapshot;
	if (!Disguises || !Target
		|| !Disguises->GetDisguiseSnapshot(Target, Snapshot))
	{
		return false;
	}
	switch (Requirement)
	{
	case ETerritoryDisguiseRequirement::Active:
		return Snapshot.bActive;
	case ETerritoryDisguiseRequirement::PerceivedAsFaction:
		return Faction.IsValid() && Snapshot.PerceivedFaction == Faction;
	case ETerritoryDisguiseRequirement::TrueFaction:
		return Faction.IsValid() && Snapshot.TrueFaction == Faction;
	case ETerritoryDisguiseRequirement::CompromisedForFaction:
		return Snapshot.bCompromisedForEveryone
			|| Faction.IsValid() && Snapshot.CompromisedForFactions.Contains(Faction);
	case ETerritoryDisguiseRequirement::AcceptedByTerritory:
	{
		ATerritoryVolume* Territory = ResolveStealthTerritory(
			this, World, TerritoryToCheck, Target);
		FText Reason;
		return Territory && Disguises->IsDisguiseAccepted(
			Target, Territory, Faction, Reason);
	}
	default:
		return false;
	}
}

FString UTerritoryDisguiseCondition::GetGraphDisplayText_Implementation()
{
	const UEnum* Enum = StaticEnum<ETerritoryDisguiseRequirement>();
	return FString::Printf(TEXT("Territory disguise: %s%s"), Enum
		? *Enum->GetDisplayNameTextByValue(static_cast<int64>(Requirement)).ToString()
		: TEXT("required state"),
		Faction.IsValid() ? *FString::Printf(TEXT(" (%s)"), *Faction.ToString()) : TEXT(""));
}
