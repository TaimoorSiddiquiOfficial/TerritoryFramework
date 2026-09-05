#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"

class ANarrativeGameState;
class AActor;
class APawn;
class APlayerController;
class UNarrativeAbilitySystemComponent;
struct FGameplayEventData;

/** One unordered Narrative faction-pair attitude, normalized for Territory use. */
struct TERRITORYFRAMEWORK_API FTerritoryNarrativeAttitudeSnapshot
{
	FGameplayTag FactionA;
	FGameplayTag FactionB;
	ETeamAttitude::Type Attitude = ETeamAttitude::Neutral;
};

/**
 * Narrow, Territory-owned compatibility seam around Narrative Pro context, ability and faction APIs.
 * Vendor representation access is intentionally isolated here so monthly Narrative
 * upgrades cannot leak implementation coupling throughout TerritoryFramework.
 */
class TERRITORYFRAMEWORK_API FTerritoryNarrativeProAdapter final
{
public:
	/** Narrative retains the player's character while a vehicle is possessed.
	 * Use it for identity, inventory and management checks; use the current pawn
	 * explicitly for vehicle position. Plain player controllers retain pawn fallback.
	 */
	static APawn* ResolvePlayerCharacter(const APlayerController* Controller);

	/**
	 * Resolve Narrative Pro's authoritative Ability System from a gameplay subject.
	 *
	 * Narrative players keep the ASC on PlayerState while NPCs keep it on the
	 * character. Callers may also hold a Controller during AI and possession
	 * callbacks, so a Controller deliberately prefers PlayerState, then its own
	 * Narrative ASC, then its controlled Pawn. Direct Pawn/Actor subjects retain
	 * their own Narrative ASC and Narrative's owner/avatar relationship is unchanged.
	 */
	static UNarrativeAbilitySystemComponent* ResolveAbilitySystem(AActor* Subject);

	/** Return the live Narrative ASC avatar, or nullptr when no Narrative ASC exists. */
	static AActor* ResolveAbilityAvatar(AActor* Subject);

	/**
	 * Deliver a Gameplay Event directly through the resolved Narrative ASC.
	 * This is safe for Controller inputs, which cannot receive
	 * SendGameplayEventToActor even when their pawn owns a valid ASC.
	 */
	static int32 SendGameplayEvent(AActor* Subject, const FGameplayTag& EventTag,
		FGameplayEventData Payload);

	static TArray<FTerritoryNarrativeAttitudeSnapshot> ReadFactionAttitudes(
		const ANarrativeGameState* GameState);

	static void SetSymmetricFactionAttitude(ANarrativeGameState* GameState,
		const FGameplayTag& FactionA, const FGameplayTag& FactionB,
		ETeamAttitude::Type Attitude);

	static FString MakeCanonicalFactionPairKey(
		const FGameplayTag& FactionA, const FGameplayTag& FactionB);
};
