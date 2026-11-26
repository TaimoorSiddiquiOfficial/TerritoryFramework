#include "Vehicles/TerritorySpawnExecutorVehicle.h"

#include "Spawn/TerritorySpawnAnchor.h"
#include "Spawn/TerritorySpawnTypes.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Vehicles/NarrativeVehicleBase.h"

#include "ChaosVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GAS/NarrativeAttributeSetBase.h"

namespace TerritorySpawnExecutorVehiclePrivate
{
	/** Scalar key under which a vehicle's condition is carried in the anchor's durable record. */
	static const FName HealthFractionKey(TEXT("HealthFraction"));

	/** "This car never told us its condition", which is not the same fact as "it is at zero health". */
	constexpr float UnknownHealthFraction = -1.f;
}

UTerritorySpawnExecutorVehicle::UTerritorySpawnExecutorVehicle()
{
	// THE CONSTRUCTOR DELIBERATELY SETS NOTHING ABOUT PLACEMENT, PHYSICS OR REPLICATION.
	//
	// A framework vehicle is spawned at the authored transform and left alone. Everything a previous
	// version did here — snapping to the ground, hiding until the suspension settled, forcing physics on,
	// applying a parking brake, overriding the net update rate — made a spawned car behave differently
	// from one dragged out of the Content Browser, and the reported symptom was exactly that: the
	// framework's ambulance could not be driven the way the level's own ambulance could.
	//
	// The base class's own snap stays OFF, which for a vehicle is the correct default: a designer places a
	// car by eye at the height it looks right, and a bay is authored the same way.
}

bool UTerritorySpawnExecutorVehicle::SupportsActorClass_Implementation(TSubclassOf<AActor> ActorClass) const
{
	if (!Super::SupportsActorClass_Implementation(ActorClass))
	{
		return false;
	}

	// Vehicles only. The executor writes a health fraction through a Narrative attribute set, and pointing
	// it at a crate would either crash on the cast or, worse, silently write a scalar nobody reads.
	return ActorClass->IsChildOf(ANarrativeVehicleBase::StaticClass());
}

AActor* UTerritorySpawnExecutorVehicle::CreateSpawned_Implementation(ATerritorySpawnAnchor* Anchor,
	TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
	AActor* const Created = Super::CreateSpawned_Implementation(Anchor, ActorClass, Transform);
	if (!Created)
	{
		return nullptr;
	}

	if (ANarrativeVehicleBase* const Vehicle = Cast<ANarrativeVehicleBase>(Created))
	{
		Vehicle->ForceNetUpdate();
	}

	return Created;
}

void UTerritorySpawnExecutorVehicle::ConfigureSpawnParameters(FActorSpawnParameters& Params) const
{
	Super::ConfigureSpawnParameters(Params);

	// RF_Transient is load-bearing, not hygiene.
	//
	// UNarrativeSaveSubsystem treats a net-startup actor with a valid GUID as part of the level. Its
	// RemoveSingleActor path marks that record bDestroyed, and from then on every load reaches
	// LoadActorFromRecord and destroys the actor before this framework can adopt it. A framework car is
	// created at runtime by an anchor that already holds its durable record, so it must never be visible
	// to the save system as a level actor at all.
	Params.ObjectFlags |= RF_Transient;
}

void UTerritorySpawnExecutorVehicle::ConfigureSpawnedActor(AActor* Actor) const
{
	Super::ConfigureSpawnedActor(Actor);

	ANarrativeVehicleBase* const Vehicle = Cast<ANarrativeVehicleBase>(Actor);
	if (!Vehicle)
	{
		return;
	}

	// The single line that makes the anchor the only authority over this car.
	//
	// ANarrativeVehicleBase::GetActorGUID_Implementation returns VehicleSaveGUID whenever the vehicle is
	// unoccupied. The vendor ships it zero (NarrativeVehicleBase.cpp has CreateSaveGuid commented out),
	// so on today's assets this is a no-op. It is kept because it is the one line that stops a future
	// author from ticking a box in a Blueprint and silently gaining a second durable record for one car.
	Vehicle->SetVehicleSaveGuid(FGuid());

	// NOTHING ELSE. No net rate, no brake, no physics, no possession watch.
	//
	// A previous version set the net update rate to 2 Hz here and then restored it on possession. That is
	// a bandwididth optimisation for a parked car, and it is the vehicle's own business: the Blueprint's
	// authored NetUpdateFrequency is what a hand-placed car uses, so it is what a spawned one must use too.
}

void UTerritorySpawnExecutorVehicle::RetireSpawned_Implementation(ATerritorySpawnAnchor* Anchor,
	AActor* Actor, ESpawnRetireMode Mode)
{
	// The record already captured the car's condition via CaptureCheckpoint, so a plain destroy loses
	// nothing the framework promised to remember.
	Super::RetireSpawned_Implementation(Anchor, Actor, Mode);
}

void UTerritorySpawnExecutorVehicle::CaptureCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor,
	AActor* Actor, FTerritorySpawnRecord& InOutRecord)
{
	const ANarrativeVehicleBase* const Vehicle = Cast<ANarrativeVehicleBase>(Actor);
	if (!Vehicle)
	{
		return;
	}

	// A fraction rather than an absolute: MaxHealth can differ between the variant that was saved and the
	// variant being spawned, and "20% left" survives that while "183 health" does not.
	const float MaxHealth = Vehicle->GetMaxHealth();
	if (MaxHealth <= 0.f)
	{
		// Nothing trustworthy to record. Leaving the previous value in place would let a car with an
		// unusable attribute set keep an old fraction and come back exactly as damaged as it was, which is
		// a silent repair by another route.
		InOutRecord.SetScalar(TerritorySpawnExecutorVehiclePrivate::HealthFractionKey,
			TerritorySpawnExecutorVehiclePrivate::UnknownHealthFraction);
		return;
	}

	const float Fraction = FMath::Clamp(Vehicle->GetHealth() / MaxHealth, 0.f, 1.f);
	InOutRecord.SetScalar(TerritorySpawnExecutorVehiclePrivate::HealthFractionKey, Fraction);
}

void UTerritorySpawnExecutorVehicle::ApplyCheckpoint_Implementation(ATerritorySpawnAnchor* Anchor,
	AActor* Actor, const FTerritorySpawnRecord& Record, bool bRestoreCondition)
{
	// The policy decides whether this car comes back as it was left or serviced and fresh. A policy that
	// does not ask for the condition gets a working vehicle, which is the right answer for an ambulance
	// depot: a second call after a wreck must yield a drivable ambulance, not the wreck again.
	if (!bRestoreCondition)
	{
		return;
	}

	ANarrativeVehicleBase* const Vehicle = Cast<ANarrativeVehicleBase>(Actor);
	if (!Vehicle)
	{
		return;
	}

	float SavedFraction = TerritorySpawnExecutorVehiclePrivate::UnknownHealthFraction;
	if (!Record.GetScalar(TerritorySpawnExecutorVehiclePrivate::HealthFractionKey, SavedFraction))
	{
		return;
	}

	// Negative means "unknown" — a car that was destroyed before it ever reported its condition. Applying
	// it would clamp to zero and spawn a permanently wrecked vehicle, so it is skipped instead.
	if (SavedFraction < 0.f)
	{
		return;
	}

	const float MaxHealth = Vehicle->GetMaxHealth();
	UAbilitySystemComponent* const ASC = Vehicle->GetAbilitySystemComponent();

	if (!ASC || MaxHealth <= 0.f
		|| !ASC->HasAttributeSetForAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()))
	{
		// An unusable Native ASC must not silently turn a nearly-wrecked car into a fresh one. That would be
		// a free repair the economy never authorised. This mirrors the assault subsystem's existing choice.
		UE_LOG(LogTerritory, Error,
			TEXT("[Spawn] %s cannot accept a saved health fraction (ASC %s, MaxHealth %.1f). Destroying it "
				 "rather than returning a fresh car the player never paid to repair."),
			*Vehicle->GetName(), ASC ? TEXT("present") : TEXT("missing"), MaxHealth);

		if (bDestroyIfHealthUnusable)
		{
			Vehicle->Destroy();
		}
		return;
	}

	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), MaxHealth * SavedFraction);
}

bool UTerritorySpawnExecutorVehicle::IsClaimedByExternalOwner_Implementation(AActor* Actor) const
{
	// The project already answers this question correctly, including mounted passengers whose pawn is
	// momentarily unpossessed during a Native mount. Re-deriving it here is how the two answers drift apart
	// and a player ends up teleported into the road when a car they are sitting in is despawned.
	return UTerritoryCounterAttackSubsystem::HasPlayerVehicleOccupant(Cast<ANarrativeVehicleBase>(Actor));
}
