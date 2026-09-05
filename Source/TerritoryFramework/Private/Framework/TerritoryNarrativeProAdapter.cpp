#include "Framework/TerritoryNarrativeProAdapter.h"

#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"

namespace
{
	UNarrativeAbilitySystemComponent* ResolveDirectNarrativeASC(AActor* Candidate)
	{
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed()) return nullptr;
		IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Candidate);
		return AbilityOwner
			? Cast<UNarrativeAbilitySystemComponent>(AbilityOwner->GetAbilitySystemComponent())
			: nullptr;
	}
}

APawn* FTerritoryNarrativeProAdapter::ResolvePlayerCharacter(const APlayerController* Controller)
{
	if (const ANarrativePlayerController* NarrativeController = Cast<ANarrativePlayerController>(Controller))
	{
		if (IsValid(NarrativeController->GetOwnedCharacter())) return NarrativeController->GetOwnedCharacter();
	}
	return Controller ? Controller->GetPawn() : nullptr;
}

UNarrativeAbilitySystemComponent* FTerritoryNarrativeProAdapter::ResolveAbilitySystem(
	AActor* Subject)
{
	if (AController* Controller = Cast<AController>(Subject))
	{
		// Player controllers must prefer PlayerState. While driving, Narrative keeps
		// player abilities on that ASC and deliberately does not switch the avatar to
		// the possessed vehicle, which may expose a separate vehicle ASC.
		if (UNarrativeAbilitySystemComponent* PlayerStateASC =
			ResolveDirectNarrativeASC(Controller->PlayerState))
		{
			return PlayerStateASC;
		}
		if (UNarrativeAbilitySystemComponent* ControllerASC =
			ResolveDirectNarrativeASC(Controller))
		{
			return ControllerASC;
		}
		return ResolveDirectNarrativeASC(Controller->GetPawn());
	}

	if (UNarrativeAbilitySystemComponent* Direct = ResolveDirectNarrativeASC(Subject))
	{
		return Direct;
	}

	if (APawn* Pawn = Cast<APawn>(Subject))
	{
		if (UNarrativeAbilitySystemComponent* PlayerStateASC =
			ResolveDirectNarrativeASC(Pawn->GetPlayerState()))
		{
			return PlayerStateASC;
		}
		return ResolveDirectNarrativeASC(Pawn->GetController());
	}

	return nullptr;
}

AActor* FTerritoryNarrativeProAdapter::ResolveAbilityAvatar(AActor* Subject)
{
	UNarrativeAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Subject);
	AActor* Avatar = AbilitySystem ? AbilitySystem->GetAvatarActor() : nullptr;
	return IsValid(Avatar) && !Avatar->IsActorBeingDestroyed() ? Avatar : nullptr;
}

int32 FTerritoryNarrativeProAdapter::SendGameplayEvent(AActor* Subject,
	const FGameplayTag& EventTag, FGameplayEventData Payload)
{
	if (!EventTag.IsValid()) return 0;
	UNarrativeAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Subject);
	if (!AbilitySystem) return 0;

	AActor* Avatar = ResolveAbilityAvatar(Subject);
	if (!Payload.Target || Payload.Target == Subject)
	{
		Payload.Target = Avatar ? Avatar : Subject;
	}
	return AbilitySystem->HandleGameplayEvent(EventTag, &Payload);
}

FString FTerritoryNarrativeProAdapter::MakeCanonicalFactionPairKey(
	const FGameplayTag& FactionA, const FGameplayTag& FactionB)
{
	const FString NameA = FactionA.ToString();
	const FString NameB = FactionB.ToString();
	return NameA < NameB ? NameA + TEXT("|") + NameB : NameB + TEXT("|") + NameA;
}

TArray<FTerritoryNarrativeAttitudeSnapshot> FTerritoryNarrativeProAdapter::ReadFactionAttitudes(
	const ANarrativeGameState* GameState)
{
	TMap<FString, FTerritoryNarrativeAttitudeSnapshot> ByPair;
	if (!GameState) return {};

	// FactionAllianceMap is currently the only public API that enumerates every
	// configured relationship. Keep this vendor representation dependency here.
	for (const auto& SourcePair : GameState->FactionAllianceMap)
	{
		const FGameplayTag& SourceFaction = SourcePair.Key;
		for (const auto& TargetPair : SourcePair.Value.AttitudeMap)
		{
			const FGameplayTag& TargetFaction = TargetPair.Key;
			if (!SourceFaction.IsValid() || !TargetFaction.IsValid()
				|| SourceFaction == TargetFaction)
			{
				continue;
			}

			const FString Key = MakeCanonicalFactionPairKey(SourceFaction, TargetFaction);
			FTerritoryNarrativeAttitudeSnapshot* Existing = ByPair.Find(Key);
			const ETeamAttitude::Type DirectionalAttitude = TargetPair.Value.GetValue();
			if (!Existing)
			{
				FTerritoryNarrativeAttitudeSnapshot Snapshot;
				Snapshot.FactionA = SourceFaction;
				Snapshot.FactionB = TargetFaction;
				Snapshot.Attitude = DirectionalAttitude;
				ByPair.Add(Key, Snapshot);
				continue;
			}

			// Lossless conservative merge for asymmetric Narrative data: hostility wins,
			// then friendliness, otherwise neutral.
			if (Existing->Attitude == ETeamAttitude::Hostile
				|| DirectionalAttitude == ETeamAttitude::Hostile)
			{
				Existing->Attitude = ETeamAttitude::Hostile;
			}
			else if (Existing->Attitude == ETeamAttitude::Friendly
				|| DirectionalAttitude == ETeamAttitude::Friendly)
			{
				Existing->Attitude = ETeamAttitude::Friendly;
			}
			else
			{
				Existing->Attitude = ETeamAttitude::Neutral;
			}
		}
	}

	TArray<FString> Keys;
	ByPair.GetKeys(Keys);
	Keys.Sort();
	TArray<FTerritoryNarrativeAttitudeSnapshot> Result;
	Result.Reserve(Keys.Num());
	for (const FString& Key : Keys)
	{
		Result.Add(ByPair.FindChecked(Key));
	}
	return Result;
}

void FTerritoryNarrativeProAdapter::SetSymmetricFactionAttitude(
	ANarrativeGameState* GameState, const FGameplayTag& FactionA,
	const FGameplayTag& FactionB, ETeamAttitude::Type Attitude)
{
	if (!GameState || !FactionA.IsValid() || !FactionB.IsValid() || FactionA == FactionB) return;
	GameState->SetFactionAttitude(FactionA, FactionB, Attitude);
	GameState->SetFactionAttitude(FactionB, FactionA, Attitude);
}
