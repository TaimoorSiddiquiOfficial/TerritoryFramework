#include "Cinematics/TerritoryCutsceneTeardown.h"

#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Core/TerritoryTypes.h"
#include "Engine/World.h"
#include "LevelSequencePlayer.h"

UTerritoryCutsceneTeardownComponent::UTerritoryCutsceneTeardownComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The teardown itself is never replicated: destroying the sequence actor is what reaches clients,
	// through the actor destruction that already replicates. A replicated copy of this component
	// would only add a client that is able to destroy an actor the server owns.
	SetIsReplicatedByDefault(false);
}

UTerritoryCutsceneTeardownComponent* UTerritoryCutsceneTeardownComponent::ScheduleAfterSequence(
	ANarrativeLevelSequenceActor* SequenceActor, float GraceSeconds)
{
	if (!IsValid(SequenceActor) || !SequenceActor->HasAuthority()) return nullptr;

	ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
	if (!Player) return nullptr;

	// Idempotent per actor. One cutscene must not be able to arm two teardowns, and a second call is
	// the expected shape when an event is authored on more than one of a floor's event arrays.
	TInlineComponentArray<UTerritoryCutsceneTeardownComponent*> Components(SequenceActor);
	for (auto* Existing : Components)
	{
		if (IsValid(Existing)) return Existing;
	}

	auto* Component = NewObject<UTerritoryCutsceneTeardownComponent>(
		SequenceActor, NAME_None, RF_Transient);
	Component->SequenceActor = SequenceActor;
	Component->SequencePlayer = Player;
	Component->GraceSeconds = FMath::Max(0.f, GraceSeconds);
	SequenceActor->AddInstanceComponent(Component);
	Component->RegisterComponent();

	Player->OnStop.AddUniqueDynamic(Component,
		&UTerritoryCutsceneTeardownComponent::HandleSequenceStopped);
	Player->OnFinished.AddUniqueDynamic(Component,
		&UTerritoryCutsceneTeardownComponent::HandleSequenceStopped);

	UE_LOG(LogTerritory, Log,
		TEXT("[CutsceneTeardown] %s will be destroyed %.2fs after its sequence stops"),
		*GetNameSafe(SequenceActor), Component->GraceSeconds);
	return Component;
}

void UTerritoryCutsceneTeardownComponent::HandleSequenceStopped()
{
	// A natural finish reaches StopInternal through FinishPlaybackInternal, which broadcasts OnStop
	// and then OnFinished, so both bindings fire for one ending. Arming once is what keeps that from
	// starting two teardowns.
	if (bTeardownArmed) return;

	// A paused player has not stopped. Narrative's dialogue shots hold their final frame while a line
	// is still displayed and the engine broadcasts OnFinished after Pause(), so destroying the actor
	// here would cut a live shot short. StopInternal assigns Status = Stopped before it broadcasts
	// OnStop (MovieSceneSequencePlayer.cpp:466-529), so this guard only ever rejects the OnFinished
	// path and can never reject a real stop.
	if (const ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		if (Player->IsPaused()) return;
	}

	bTeardownArmed = true;

	ANarrativeLevelSequenceActor* Subject = SequenceActor;
	if (!IsValid(Subject)) return;

	if (GraceSeconds > 0.f)
	{
		// A grace delay rather than an immediate destroy, because this destruction replicates: a
		// client whose own copy is a fraction of a second behind must not have its cutscene cut off
		// mid-shot. SetLifeSpan is authority-gated, which is the same gate this component is created
		// behind, so the two cannot disagree about who owns the actor.
		Subject->SetLifeSpan(GraceSeconds);
		UE_LOG(LogTerritory, Log,
			TEXT("[CutsceneTeardown] %s stopped; destroying in %.2fs"),
			*GetNameSafe(Subject), GraceSeconds);
		return;
	}

	// Never SetLifeSpan(0) here. AActor::SetLifeSpan takes its clear-the-timer branch for any value
	// <= 0 (Actor.cpp:6584-6602) and would leave the actor alive with no timer at all, which is the
	// silent leak this component exists to prevent. An immediate teardown destroys directly.
	UE_LOG(LogTerritory, Log, TEXT("[CutsceneTeardown] %s stopped; destroying now"),
		*GetNameSafe(Subject));
	Subject->Destroy();
}

void UTerritoryCutsceneTeardownComponent::UnbindFromPlayer()
{
	if (ULevelSequencePlayer* Player = SequencePlayer.Get())
	{
		Player->OnStop.RemoveAll(this);
		Player->OnFinished.RemoveAll(this);
	}
	SequencePlayer.Reset();
}

void UTerritoryCutsceneTeardownComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	UnbindFromPlayer();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UTerritoryCutsceneTeardownComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UnbindFromPlayer();
	Super::EndPlay(Reason);
}
