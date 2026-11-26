#include "AI/TerritoryDialogueStateLibrary.h"

#include "AI/TerritoryDiplomacyDialogue.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"

bool UTerritoryDialogueStateLibrary::IsActorWithinRange(const AActor* Actor, const AActor* OtherActor,
	float Radius)
{
	if (!IsValid(Actor) || !IsValid(OtherActor))
	{
		return false;
	}

	return Actor->GetDistanceTo(OtherActor) <= Radius;
}

bool UTerritoryDialogueStateLibrary::IsActorRelationship(const UObject* WorldContextObject, AActor* Actor,
	AActor* OtherActor, EDiplomacyState Expected)
{
	if (!IsValid(Actor) || !IsValid(OtherActor))
	{
		return false;
	}

	// Prefer the NPC's own resolver: it handles multi-faction pairs, disguise perception and war-wins,
	// so the state machine can never disagree with the interaction dialogue selection.
	if (const UTerritoryDiplomacyDialogueComponent* Resolver =
		Actor->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>())
	{
		if (const APawn* OtherPawn = Cast<APawn>(OtherActor))
		{
			bool bSameFaction = false;
			return Resolver->ResolveRelationshipForInteractor(OtherPawn, bSameFaction) == Expected;
		}
	}

	// Fallback for actors without the component (for example two guards compared with each other).
	const UWorld* World = WorldContextObject
		? WorldContextObject->GetWorld()
		: Actor->GetWorld();
	const UTerritoryDiplomacySubsystem* Diplomacy = World
		? World->GetSubsystem<UTerritoryDiplomacySubsystem>()
		: nullptr;
	if (!Diplomacy)
	{
		return false;
	}

	const FGameplayTag FactionA = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(WorldContextObject, Actor);
	const FGameplayTag FactionB = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(WorldContextObject, OtherActor);
	if (!FactionA.IsValid() || !FactionB.IsValid())
	{
		return false;
	}

	// A shared exact faction counts as friendly unless the subsystem reports something stronger.
	if (FactionA == FactionB)
	{
		return Expected == EDiplomacyState::Alliance;
	}

	return Diplomacy->GetDiplomacyState(FactionA, FactionB) == Expected;
}

bool UTerritoryDialogueStateLibrary::IsActorInDialogue(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (UTalesComponent* Tales = Actor->FindComponentByClass<UTalesComponent>())
	{
		return Tales->IsInDialogue();
	}

	return false;
}

bool UTerritoryDialogueStateLibrary::IsActorHealthBelow(const AActor* Actor, float Percent)
{
	const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Actor);
	if (!Character)
	{
		return false;
	}

	const float MaxHealth = Character->GetMaxHealth();
	if (MaxHealth <= 0.f)
	{
		return false;
	}

	return Character->GetHealth() <= MaxHealth * FMath::Clamp(Percent, 0.f, 1.f);
}
