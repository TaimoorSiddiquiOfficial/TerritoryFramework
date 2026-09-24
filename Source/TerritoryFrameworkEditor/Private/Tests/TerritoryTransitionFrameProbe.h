#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "Tales/NarrativeEvent.h"

/**
 * Read and arrange seam for the deferred-ancestor transition frame.
 *
 * `ATerritoryWorldState` and `ATerritoryVolume` declare this type a friend. The frame
 * depth, the deferred queue and the authored runtime state configs are private because
 * nothing in production may read or mutate them that way - the whole point of the fix is
 * that one path owns the ancestor commit - so the assertions that prove the frame opens,
 * closes and drains need an explicit test seam rather than a public accessor that would
 * invite a second reader.
 */
struct FTFTransitionFrameProbe
{
	/** Nested commit depth. -1 means no world state was reachable at all. */
	static int32 Depth(const ATerritoryWorldState* State)
	{
		return State ? State->TransitionFrameDepth : -1;
	}

	/** Entries still awaiting a deferred commit. */
	static int32 Queued(const ATerritoryWorldState* State)
	{
		return State ? State->DeferredLoadedAncestorReconciles.Num() : -1;
	}

	/** `Parent<-Child,Parent<-Child` - the queue shape, for failure messages. */
	static FString QueuedTags(const ATerritoryWorldState* State)
	{
		if (!State) return TEXT("<no world state>");
		TArray<FString> Parts;
		for (const TPair<FGameplayTag, FGameplayTag>& Entry
			: State->DeferredLoadedAncestorReconciles)
		{
			Parts.Add(FString::Printf(TEXT("%s<-%s"),
				*Entry.Key.ToString(), *Entry.Value.ToString()));
		}
		return Parts.IsEmpty() ? TEXT("<empty>") : FString::Join(Parts, TEXT(","));
	}

	/** True while a drain loop owns the queue, so a nested exit must not start a second one. */
	static bool bDraining(const ATerritoryWorldState* State)
	{
		return State ? State->bDrainingDeferredReconciles : false;
	}

	/**
	 * Register a live defender the way a guard post does. `SpawnedGuards` is private, and the
	 * frame tests need a garrison that changes during the deferred window.
	 */
	static bool AddGuard(ATerritoryVolume* Volume, ATerritoryGuardCharacter* Guard)
	{
		if (!Volume || !Guard) return false;
		Volume->SpawnedGuards.Add(Guard);
		Volume->RegisterDefender(Guard);
		Volume->RefreshGarrisonSnapshot();
		return true;
	}

	/** Author one lifecycle event on the volume's runtime state configs. */
	static bool InstallStateEvent(ATerritoryVolume* Volume, ETerritoryState State,
		bool bEntering, UNarrativeEvent* Event)
	{
		if (!Volume || !Event) return false;
		FTerritoryStateConfig& Config = Volume->RuntimeStateConfigs.FindOrAdd(State);
		(bEntering ? Config.EntryEvents : Config.ExitEvents) = {Event};
		return true;
	}

	static void ClearStateEvents(ATerritoryVolume* Volume, ETerritoryState State)
	{
		if (Volume) Volume->RuntimeStateConfigs.Remove(State);
	}
};
