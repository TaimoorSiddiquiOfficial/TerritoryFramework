#include "Tales/TerritoryOwnerHandoverEvent.h"

#include "EngineUtils.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Tales/TerritoryTalesUtilities.h"

UTerritoryOwnerHandoverEvent::UTerritoryOwnerHandoverEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRefireOnLoad = false;
}

void UTerritoryOwnerHandoverEvent::ExecuteEvent_Implementation(APawn* Target,
	APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!TerritoryTales::DoEventConditionsPass(
		this, Target, Controller, NarrativeComponent))
	{
		return;
	}

	// A defender death is allowed to have no Narrative target. This happens for
	// environment kills, Instakill, and some GAS effects which do not pass through
	// AActor::TakeDamage (so the Territory cannot recover a last damaging pawn).
	// The handover belongs to the Territory/spawner, not to the killer, therefore
	// never make world resolution depend on a player context being present.
	UWorld* World = TerritoryTales::ResolveWorld(
		this, Target, Controller, NarrativeComponent);
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	ATerritoryStoryOwnerSpawner* ResolvedSpawner = nullptr;
	if (OwnerTerritoryTag.IsValid())
	{
		for (TActorIterator<ATerritoryStoryOwnerSpawner> It(World); It; ++It)
		{
			if (It->TerritoryTag.MatchesTagExact(OwnerTerritoryTag))
			{
				ResolvedSpawner = *It;
				break;
			}
		}
	}
	if (!IsValid(ResolvedSpawner))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Territory owner handover could not resolve a spawner for %s."),
			*OwnerTerritoryTag.ToString());
		return;
	}

	if (!ResolvedSpawner->ActivateHandover(
		Target, Controller, NarrativeComponent, bBeginDialogueImmediately))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Territory owner handover failed to activate %s for %s."),
			*GetNameSafe(ResolvedSpawner), *OwnerTerritoryTag.ToString());
	}
}

FString UTerritoryOwnerHandoverEvent::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Begin owner handover: %s%s"),
		*OwnerTerritoryTag.ToString(),
		bBeginDialogueImmediately ? TEXT(" (start dialogue)") : TEXT(" (wait for interaction)"));
}
