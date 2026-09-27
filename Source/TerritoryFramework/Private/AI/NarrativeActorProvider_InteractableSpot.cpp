// Copyright Territory Framework. All Rights Reserved.

#include "AI/NarrativeActorProvider_InteractableSpot.h"

#include "Core/TerritoryTypes.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/InteractableComponent.h"
#include "Interaction/InteractionComponent.h"

namespace
{
	/** A candidate prop and how far it sits from the search origin. */
	struct FTerritorySpotCandidate
	{
		AActor* Actor = nullptr;
		double DistSq = 0.0;
	};
}

UNarrativeActorProvider_InteractableSpot::UNarrativeActorProvider_InteractableSpot()
{
	// SearchRadius stays 0 on purpose: the base default must mean "not configured", never
	// "some sensible radius", so an unauthored spot cannot silently claim a prop.
}

AActor* UNarrativeActorProvider_InteractableSpot::ResolveSpotActor(const UObject* WorldContextObject)
{
	// An unauthored radius disables the query. Substituting a default here would make every
	// mis-authored spot look like it worked, on whatever prop happened to be closest.
	if (!FMath::IsFinite(SearchRadius) || SearchRadius <= 0.f || SearchOrigin.ContainsNaN())
	{
		return nullptr;
	}

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (!World)
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("%s: no world available from the context object, so the spot could not be resolved."),
			*GetName());
		return nullptr;
	}

	if (!IsValid(ClaimerComponent))
	{
		UE_LOG(LogTerritory, Warning,
			TEXT("%s: no claimer set, so Narrative Pro's seat availability rule cannot be evaluated "
				 "for anyone and the spot is left unresolved."),
			*GetName());
		return nullptr;
	}

	// Gather every candidate first and order them afterwards. The overlap array's order is not
	// stable between runs, so ranking at the end is what makes a spot resolve to the same prop
	// every time rather than to whichever actor the physics query happened to report first.
	TArray<AActor*> Candidates;
	const double RadiusSq = static_cast<double>(SearchRadius) * SearchRadius;

	if (SpotActorClass)
	{
		// A named class needs no collision, so an authored spot prop is still found when the
		// prop has none, and the designer's explicit class choice is honoured exactly.
		for (TActorIterator<AActor> It(World, SpotActorClass.Get()); It; ++It)
		{
			Candidates.Add(*It);
		}
	}
	else
	{
		TArray<FOverlapResult> Overlaps;

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

		FCollisionQueryParams QueryParams(TEXT("TerritorySpotQuery"), /*bTraceComplex=*/false);

		World->OverlapMultiByObjectType(Overlaps, SearchOrigin, FQuat::Identity, ObjectParams,
			FCollisionShape::MakeSphere(SearchRadius), QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (AActor* OverlapActor = Overlap.GetActor())
			{
				// One prop can overlap through several components; keep it once.
				Candidates.AddUnique(OverlapActor);
			}
		}
	}

	TArray<FTerritorySpotCandidate> Ranked;
	Ranked.Reserve(Candidates.Num());
	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(Candidate->GetActorLocation(), SearchOrigin);
		if (!FMath::IsFinite(DistSq) || DistSq > RadiusSq)
		{
			// The class path still honours the radius: naming a class narrows the search, it
			// does not widen it to the whole level.
			continue;
		}

		Ranked.Add({Candidate, DistSq});
	}

	// Nearest first, then by actor name. Distance alone is not a total order — two props can sit
	// equidistant from a node — and the tie-break must be stable rather than arbitrary, so it is
	// the actor's own name and never a runtime hash.
	Ranked.Sort([](const FTerritorySpotCandidate& A, const FTerritorySpotCandidate& B)
	{
		if (A.DistSq != B.DistSq)
		{
			return A.DistSq < B.DistSq;
		}
		return A.Actor->GetName() < B.Actor->GetName();
	});

	TArray<int32> EligibleSlots;
	for (const FTerritorySpotCandidate& Candidate : Ranked)
	{
		// A previous candidate's Blueprint slot behavior can remove another actor.
		if (!IsValid(Candidate.Actor)) continue;
		UNarrativeInteractableComponent* Interactable =
			Candidate.Actor->FindComponentByClass<UNarrativeInteractableComponent>();
		if (GetEligibleSlots(Interactable, EligibleSlots))
		{
			return Candidate.Actor;
		}
	}

	return nullptr;
}

bool UNarrativeActorProvider_InteractableSpot::GetEligibleSlots(
	UNarrativeInteractableComponent* Interactable, TArray<int32>& OutSlots)
{
	OutSlots.Reset();

	if (!IsValid(Interactable) || !IsValid(ClaimerComponent))
	{
		return false;
	}

	// Narrative Pro's own availability rule, called with the same literals the vendor's NPC
	// paths pass: two NPCs must not double-target one free slot, but an occupied-yet-stealable
	// slot is still a candidate. Occupancy alone therefore does not protect a seated guard.
	OutSlots = Interactable->GetAvailableSlots(ClaimerComponent,
		/*bIncludeTargeted=*/false, /*bIncludeStealable=*/true);
	if (!IsValid(Interactable) || !IsValid(ClaimerComponent))
	{
		OutSlots.Reset();
		return false;
	}

	if (OutSlots.IsEmpty())
	{
		return false;
	}

	if (SpotTag.IsValid())
	{
		// Read-only: the slot config is authored data and is never mutated here.
		OutSlots.RemoveAll([Interactable, this](int32 SlotIndex)
		{
			return Interactable->GetConfigAtSlot(SlotIndex).SlotTag != SpotTag;
		});

		if (OutSlots.IsEmpty())
		{
			return false;
		}
	}

	return true;
}

AActor* UNarrativeActorProvider_InteractableSpot::ProvideActor_Implementation(const UObject* WorldContextObject)
{
	AActor* Spot = ResolveSpotActor(WorldContextObject);

	if (!Spot)
	{
		// Verbose, not Warning: the vendor calls this from the goal's own graph and re-evaluates
		// it on rescore, so a warning here would repeat every RescoreInterval. The delivery path
		// logs the loud, once-per-node warning instead — see TerritoryGuardCharacter.
		UE_LOG(LogTerritory, Verbose,
			TEXT("%s: no eligible interactable within %.0fcm of %s."),
			*GetName(), SearchRadius, *SearchOrigin.ToCompactString());
	}

	return Spot;
}

FText UNarrativeActorProvider_InteractableSpot::GetDescription() const
{
	if (!FMath::IsFinite(SearchRadius) || SearchRadius <= 0.f)
	{
		return NSLOCTEXT("NarrativeActorProvider_InteractableSpot", "Unconfigured",
			"Interactable spot (no search radius set)");
	}

	if (SpotActorClass)
	{
		return FText::Format(
			NSLOCTEXT("NarrativeActorProvider_InteractableSpot", "TypedSpot",
				"Find {0} within {1}cm"),
			FText::FromString(SpotActorClass->GetName()),
			FText::AsNumber(SearchRadius));
	}

	return FText::Format(
		NSLOCTEXT("NarrativeActorProvider_InteractableSpot", "AnySpot",
			"Find any interactable within {0}cm"),
		FText::AsNumber(SearchRadius));
}
