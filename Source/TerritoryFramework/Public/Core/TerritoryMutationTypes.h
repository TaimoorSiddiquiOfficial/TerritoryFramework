// Copyright TerritoryFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Core/TerritoryTypes.h"
#include "TerritoryMutationTypes.generated.h"

class ATerritoryVolume;

/**
 * Result of an atomic territory mutation (AGENTS.md 5.3).
 * Replaces loosely coupled setter sequences with a single transaction.
 */
UENUM(BlueprintType)
enum class ETerritoryMutationResult : uint8
{
	Success,
	Rejected_Authority,
	Rejected_NullTerritory,
	Rejected_AggregateOnly,
	Rejected_InvalidFaction,
	Rejected_DiplomacyBlocked,
	Rejected_Locked,
	Rejected_ConditionsFailed,
	Rejected_StateUnchanged,
	Failed_FinalStateMismatch
};

/**
 * Request for an atomic territory mutation.
 * All fields are validated before any state changes are applied.
 */
USTRUCT(BlueprintType)
struct FTerritoryMutationRequest
{
	GENERATED_BODY()

	/** The territory to mutate. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation")
	TObjectPtr<ATerritoryVolume> Territory = nullptr;

	/** The faction that should own the territory after mutation. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag NewOwner;

	/** Desired state after mutation. Default = Claimed. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation")
	ETerritoryState DesiredState = ETerritoryState::Claimed;

	/** @deprecated Has no effect — ApplyTerritoryMutation always clears capture state on success. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation",
		meta = (DeprecatedProperty, DeprecationMessage = "Has no effect. Capture state is always cleared on successful mutation."))
	bool bClearCaptureState = true;

	/** Whether to bypass Narrative state entry conditions. Default = false (conditions checked). */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation")
	bool bBypassConditions = false;

	/** Whether to bypass diplomacy checks. Default = false. Set true for ForceCapture. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation")
	bool bBypassDiplomacy = false;

	/** Whether to bypass a locked Territory. Default = false. Set true only for explicit force capture. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation")
	bool bBypassLock = false;

	/** Optional transition context for Narrative conditions/events. */
	UPROPERTY(BlueprintReadWrite, Category = "Territory|Mutation")
	FTerritoryTransitionContext TransitionContext;
};

/**
 * Response from an atomic territory mutation.
 * Contains old/new state for audit trail and Blueprint feedback.
 */
USTRUCT(BlueprintType)
struct FTerritoryMutationResponse
{
	GENERATED_BODY()

	/** Whether the mutation succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation")
	ETerritoryMutationResult Result = ETerritoryMutationResult::Rejected_NullTerritory;

	/** The territory that was mutated (null on rejection). */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation")
	TObjectPtr<ATerritoryVolume> Territory = nullptr;

	/** Owner before the mutation. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag OldOwner;

	/** Owner after the mutation. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation",
		meta = (Categories = "Narrative.Factions"))
	FGameplayTag NewOwner;

	/** State before the mutation. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation")
	ETerritoryState OldState = ETerritoryState::Unclaimed;

	/** State after the mutation. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation")
	ETerritoryState NewState = ETerritoryState::Unclaimed;

	/** Human-readable explanation of what happened or why it was rejected. */
	UPROPERTY(BlueprintReadOnly, Category = "Territory|Mutation")
	FText Explanation;
};
