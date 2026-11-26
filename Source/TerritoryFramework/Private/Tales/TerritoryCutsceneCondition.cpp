#include "Tales/TerritoryCutsceneCondition.h"

#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "GameFramework/Pawn.h"
#include "LevelSequence.h"
#include "UnrealFramework/NarrativePlayerController.h"

UTerritoryCutsceneCondition::UTerritoryCutsceneCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
}

bool UTerritoryCutsceneCondition::CheckConditionInContext(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	// A supplied target is authoritative; never silently check another player's
	// cutscene when this pawn is unpossessed or belongs to an NPC.
	const APlayerController* ContextController = Target
		? Cast<APlayerController>(Target->GetController()) : Controller;
	if (!IsValid(Cast<ANarrativePlayerController>(ContextController))) return false;
	// A non-null unloaded asset cannot be playing. Preserve a meaningful false
	// query without loading cinematic content just to evaluate a condition.
	const bool bPlaying = (Sequence.IsNull() || Sequence.IsValid())
		&& UTerritoryCinematicPresentationSubsystem::IsInCutscene(
			ContextController, Sequence.Get(), bIncludePaused);
	return bPlaying == bIsInCutscene;
}

FString UTerritoryCutsceneCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Player %s in cutscene%s%s"),
		bIsInCutscene ? TEXT("is") : TEXT("is not"),
		Sequence.IsNull() ? TEXT("") : *FString::Printf(TEXT(" %s"), *Sequence.GetAssetName()),
		bIncludePaused ? TEXT(" (including paused)") : TEXT(" (playing)"));
}
