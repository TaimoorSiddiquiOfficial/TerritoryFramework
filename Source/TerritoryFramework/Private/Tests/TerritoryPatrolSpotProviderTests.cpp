// Copyright Territory Framework. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/NarrativeActorProvider_InteractableSpot.h"
#include "Components/SceneComponent.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interaction/InteractionComponent.h"
#include "Tests/TerritoryPatrolSpotTestFixtures.h"
#include <limits>

namespace
{
	/** A world the spot tests can query, torn down by the caller. */
	UWorld* MakeSpotTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		return World;
	}

	/**
	 * An actor actually placed at Location.
	 *
	 * A bare AActor has no root component, and without one the spawn transform is not applied,
	 * so every prop would silently sit at the origin and every distance-based assertion below
	 * would be measuring nothing. The root component is what makes Location real.
	 */
	AActor* SpawnPlacedActor(UWorld* World, const FVector& Location)
	{
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
		if (!Actor)
		{
			return nullptr;
		}

		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("SpotRoot"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}

	UNarrativeInteractableComponent* InteractableOn(AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<UNarrativeInteractableComponent>() : nullptr;
	}

	/**
	 * An actor placed at Location carrying an interactable component with SlotCount slots, each
	 * owned by a test-controlled behavior.
	 *
	 * SlotStatuses is sized by the component's own BeginPlay, which a spawned-but-not-begun
	 * fixture never reaches, so it is sized here. The two arrays stay the same length on purpose:
	 * UNarrativeInteractableComponent::GetAvailableSlots loops SlotStatuses.Num() and indexes
	 * InteractionSlots unguarded, so an unequal pair is undefined behaviour.
	 */
	AActor* SpawnSpotProp(UWorld* World, const FVector& Location, int32 SlotCount,
		const FGameplayTag& SlotTag = FGameplayTag())
	{
		AActor* Prop = SpawnPlacedActor(World, Location);
		if (!Prop)
		{
			return nullptr;
		}

		UNarrativeInteractableComponent* Interactable =
			NewObject<UNarrativeInteractableComponent>(Prop);
		Interactable->RegisterComponent();

		Interactable->InteractionSlots.SetNum(SlotCount);
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			Interactable->InteractionSlots[Index].SlotTag = SlotTag;
			Interactable->InteractionSlots[Index].SlotInteractBehavior =
				NewObject<UTerritoryTestSlotBehavior>(Interactable);
		}
		Interactable->SlotStatuses.SetNum(SlotCount);

		return Prop;
	}

	UTerritoryTestSlotBehavior* BehaviorOn(AActor* Prop, int32 SlotIndex)
	{
		UNarrativeInteractableComponent* Interactable = InteractableOn(Prop);
		if (!Interactable || !Interactable->InteractionSlots.IsValidIndex(SlotIndex))
		{
			return nullptr;
		}
		return Cast<UTerritoryTestSlotBehavior>(
			Interactable->InteractionSlots[SlotIndex].SlotInteractBehavior);
	}

	/** A provider aimed at Origin with the given radius, resolving on behalf of Claimer. */
	UNarrativeActorProvider_InteractableSpot* MakeProvider(UObject* Outer,
		UNarrativeInteractionComponent* Claimer, const FVector& Origin, float Radius)
	{
		UNarrativeActorProvider_InteractableSpot* Provider =
			NewObject<UNarrativeActorProvider_InteractableSpot>(Outer);
		Provider->ClaimerComponent = Claimer;
		Provider->SearchOrigin = Origin;
		Provider->SearchRadius = Radius;
		return Provider;
	}

	/**
	 * What the resolver picks, by name. Asserting on names rather than raw pointers is what
	 * makes a failure readable — a pointer comparison says only "not equal", while a name says
	 * which prop was chosen instead.
	 */
	FString ResolvedName(UNarrativeActorProvider_InteractableSpot* Provider, UWorld* World)
	{
		return GetNameSafe(Provider->ResolveSpotActor(World));
	}
}

/**
 * The spot resolver picks the prop a patrol spot refers to, and it must pick the same prop for
 * the same world every time. This drives the production resolver against real interactable
 * components, so a change to the ranking, to the availability call, or to the literal arguments
 * it passes Narrative Pro's own query fails here rather than in play.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolSpotProviderSelects,
	"TerritoryFramework.Guards.Regression.PatrolSpotProviderSelects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolSpotProviderSelects::RunTest(const FString& Parameters)
{
	UWorld* World = MakeSpotTestWorld();
	if (!TestNotNull(TEXT("Spot provider test world"), World)) return false;
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);

	AActor* Host = SpawnPlacedActor(World, FVector::ZeroVector);
	UNarrativeInteractionComponent* Claimer = NewObject<UNarrativeInteractionComponent>(Host);

	const FVector Origin = FVector::ZeroVector;
	AActor* Near = SpawnSpotProp(World, FVector(300.f, 0.f, 0.f), /*SlotCount=*/1);
	AActor* Far = SpawnSpotProp(World, FVector(900.f, 0.f, 0.f), /*SlotCount=*/1);
	if (!TestNotNull(TEXT("Near prop"), Near) || !TestNotNull(TEXT("Far prop"), Far))
	{
		World->DestroyWorld(false);
		return false;
	}

	// The fixture's premise: the props are where they were asked to be. Without this a broken
	// placement would make every leg below pass or fail for the wrong reason.
	TestEqual(TEXT("Near prop is 300cm from the origin"),
		FVector::Dist(Near->GetActorLocation(), Origin), 300.0);
	TestEqual(TEXT("Far prop is 900cm from the origin"),
		FVector::Dist(Far->GetActorLocation(), Origin), 900.0);

	// A named class is searched directly, so a bare actor with neither collision nor mesh is
	// still found. This is also what proves the class path does not depend on the overlap.
	UNarrativeActorProvider_InteractableSpot* Provider =
		MakeProvider(Host, Claimer, Origin, /*Radius=*/1500.f);
	Provider->SpotActorClass = AActor::StaticClass();

	TestEqual(TEXT("The nearest eligible prop wins"), ResolvedName(Provider, World), Near->GetName());
	TestEqual(TEXT("Resolving twice returns the same prop"),
		ResolvedName(Provider, World), Near->GetName());

	// The radius still applies on the class path: naming a class narrows the search rather
	// than widening it to the whole level.
	Provider->SearchRadius = 500.f;
	TestEqual(TEXT("A prop outside the radius is not considered even when its class is named"),
		ResolvedName(Provider, World), Near->GetName());

	Provider->SearchRadius = 250.f;
	TestNull(TEXT("A radius that excludes every prop resolves nothing"),
		Provider->ResolveSpotActor(World));

	Provider->SearchRadius = 1500.f;

	// Occupancy. Narrative Pro decides this, not Territory: the resolver only reads its answer.
	InteractableOn(Near)->SlotStatuses[0].SlotStatus = EInteractionSlotStatus::ISS_Occupied;

	BehaviorOn(Near, 0)->bStealableInTest = false;
	TestEqual(TEXT("An occupied, unstealable prop is skipped in favour of the next one"),
		ResolvedName(Provider, World), Far->GetName());

	BehaviorOn(Near, 0)->bStealableInTest = true;
	TestEqual(TEXT("An occupied but stealable prop is still eligible"),
		ResolvedName(Provider, World), Near->GetName());

	// Usability is the behavior's answer too, so a slot that refuses the claimer is not a spot.
	BehaviorOn(Near, 0)->bStealableInTest = false;
	BehaviorOn(Near, 0)->bUsableInTest = false;
	InteractableOn(Near)->SlotStatuses[0].SlotStatus = EInteractionSlotStatus::ISS_Free;
	TestEqual(TEXT("A slot that refuses the claimer is not eligible"),
		ResolvedName(Provider, World), Far->GetName());

	// Tag filter. The tag is asserted valid rather than skipped, so a missing tag fails loudly
	// instead of silently turning this leg into a no-op.
	const FGameplayTag SpotTag =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Territory.HavenReach.CastleHill")), false);
	if (!TestTrue(TEXT("The example territory tag is registered"), SpotTag.IsValid()))
	{
		World->DestroyWorld(false);
		return false;
	}

	BehaviorOn(Near, 0)->bUsableInTest = true;
	Provider->SpotTag = SpotTag;
	TestNull(TEXT("A tag no slot carries leaves every prop ineligible"),
		Provider->ResolveSpotActor(World));

	InteractableOn(Near)->InteractionSlots[0].SlotTag = SpotTag;
	TestEqual(TEXT("A prop whose slot carries the requested tag is eligible"),
		ResolvedName(Provider, World), Near->GetName());

	// Two props on the same tag: distance still decides, not discovery order.
	InteractableOn(Far)->InteractionSlots[0].SlotTag = SpotTag;
	TestEqual(TEXT("Distance outranks tag matching when both props match"),
		ResolvedName(Provider, World), Near->GetName());

	// A multi-slot prop stays a single candidate: one prop is one spot.
	AActor* Bench = SpawnSpotProp(World, FVector(100.f, 0.f, 0.f), /*SlotCount=*/3, SpotTag);
	if (!TestNotNull(TEXT("Bench prop"), Bench))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("A nearer multi-slot prop wins as one spot"),
		ResolvedName(Provider, World), Bench->GetName());

	// A prop whose slots are all occupied by someone else is skipped as a whole, which is what
	// lets two guards converging on a spot end up on two different props.
	for (int32 SlotIndex = 0; SlotIndex < 3; ++SlotIndex)
	{
		InteractableOn(Bench)->SlotStatuses[SlotIndex].SlotStatus =
			EInteractionSlotStatus::ISS_Occupied;
	}
	TestEqual(TEXT("A prop with every slot occupied and unstealable is skipped"),
		ResolvedName(Provider, World), Near->GetName());

	// One free slot is enough to make the prop a spot again.
	InteractableOn(Bench)->SlotStatuses[2].SlotStatus = EInteractionSlotStatus::ISS_Free;
	TestEqual(TEXT("One free slot makes the whole prop eligible again"),
		ResolvedName(Provider, World), Bench->GetName());

	World->DestroyWorld(false);
	return true;
}

/**
 * The failure paths. Each is a way an authored spot can look inert, and the resolver must return
 * nothing rather than fall back to a nearby prop — a silent substitution would make a
 * mis-authored spot indistinguishable from a working one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolSpotProviderRejects,
	"TerritoryFramework.Guards.Regression.PatrolSpotProviderRejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolSpotProviderRejects::RunTest(const FString& Parameters)
{
	UWorld* World = MakeSpotTestWorld();
	if (!TestNotNull(TEXT("Spot provider test world"), World)) return false;
	TGuardValue<bool> ScriptActorCallbacks(GAllowActorScriptExecutionInEditor, true);

	AActor* Host = SpawnPlacedActor(World, FVector::ZeroVector);
	UNarrativeInteractionComponent* Claimer = NewObject<UNarrativeInteractionComponent>(Host);

	const FVector Origin = FVector::ZeroVector;
	AActor* Prop = SpawnSpotProp(World, Origin, /*SlotCount=*/1);
	AActor* SlotlessProp = SpawnSpotProp(World, FVector(50.f, 0.f, 0.f), /*SlotCount=*/0);
	AActor* ProplessActor = SpawnPlacedActor(World, FVector(60.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Prop"), Prop)
		|| !TestNotNull(TEXT("Slotless prop"), SlotlessProp)
		|| !TestNotNull(TEXT("Propless actor"), ProplessActor))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestNull(TEXT("A prop with no interactable component has nothing to find"),
		InteractableOn(ProplessActor));

	// An unauthored radius is the authored way to say "no spot here", so it must not fall back
	// to any default that would let the query find something anyway.
	UNarrativeActorProvider_InteractableSpot* Provider =
		MakeProvider(Host, Claimer, Origin, /*Radius=*/0.f);
	Provider->SpotActorClass = AActor::StaticClass();
	TestNull(TEXT("A zero radius resolves nothing even with an eligible prop at the origin"),
		Provider->ResolveSpotActor(World));
	for (float InvalidRadius : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.f})
	{
		Provider->SearchRadius = InvalidRadius;
		TestNull(TEXT("Nonfinite or negative radius never reaches a world query"), Provider->ResolveSpotActor(World));
	}
	Provider->SearchRadius = 1500.f;
	Provider->SearchOrigin.X = std::numeric_limits<double>::quiet_NaN();
	TestNull(TEXT("A nonfinite origin never reaches a world query"), Provider->ResolveSpotActor(World));
	Provider->SearchOrigin = Origin;

	// The vendor availability rule is evaluated for a specific NPC; with no claimer the
	// resolver has no one to evaluate it for.
	Provider->SearchRadius = 1500.f;
	Provider->ClaimerComponent = nullptr;
	TestNull(TEXT("No claimer resolves nothing"), Provider->ResolveSpotActor(World));

	Provider->ClaimerComponent = Claimer;

	// The slotless prop sits nearer than the eligible one, so this also proves a nearer
	// ineligible prop does not mask a further eligible one.
	TestEqual(TEXT("A slotless prop nearer than the eligible one is passed over"),
		ResolvedName(Provider, World), Prop->GetName());

	// A slot with no behavior is invisible to Narrative Pro's query, so the prop is not a spot.
	InteractableOn(Prop)->InteractionSlots[0].SlotInteractBehavior = nullptr;
	TestNull(TEXT("A prop whose only slot has no behavior is not a spot"),
		Provider->ResolveSpotActor(World));

	// Give the behavior back so the remaining legs measure what they claim to.
	InteractableOn(Prop)->InteractionSlots[0].SlotInteractBehavior =
		NewObject<UTerritoryTestSlotBehavior>(InteractableOn(Prop));
	TestEqual(TEXT("Restoring the behavior makes the prop eligible again"),
		ResolvedName(Provider, World), Prop->GetName());

	// A named class the prop is not: the filter is applied, not merely recorded.
	Provider->SpotActorClass = ATerritoryGuardSpawnPoint::StaticClass();
	TestNull(TEXT("A class filter that excludes every prop resolves nothing"),
		Provider->ResolveSpotActor(World));

	// With no class named the search is a physical overlap, and a prop with no collision is
	// invisible to it. This is the documented difference between the two paths, asserted
	// rather than merely described: a collisionless spot prop must name its class.
	Provider->SpotActorClass = nullptr;
	TestNull(TEXT("Without a class filter the overlap does not see a collisionless prop"),
		Provider->ResolveSpotActor(World));

	// The same props are found again the moment a class is named, which is what makes the
	// assertion above a statement about the overlap rather than about the props.
	Provider->SpotActorClass = AActor::StaticClass();
	TestEqual(TEXT("Naming the class finds the same collisionless prop"),
		ResolvedName(Provider, World), Prop->GetName());

	// Out of range entirely. The origin has to move here, not just the radius: Prop is at the
	// search origin, so no positive radius excludes it while the search is centred there — the
	// first leg below pins down that distance really is measured from the origin.
	Provider->SearchRadius = 10.f;
	TestEqual(TEXT("A prop at the search origin is still inside a tiny radius"),
		ResolvedName(Provider, World), Prop->GetName());

	Provider->SearchOrigin = FVector(5000.f, 0.f, 0.f);
	TestNull(TEXT("Every prop outside the radius resolves nothing"),
		Provider->ResolveSpotActor(World));

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
