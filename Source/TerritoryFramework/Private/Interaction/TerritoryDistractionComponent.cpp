#include "Interaction/TerritoryDistractionComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Core/TerritoryStealthTags.h"
#include "Perception/AISense_Hearing.h"

UTerritoryDistractionComponent::UTerritoryDistractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTerritoryDistractionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bReportOnFirstHit && GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->OnActorHit.AddUniqueDynamic(
			this, &UTerritoryDistractionComponent::HandleOwnerHit);
	}
}

void UTerritoryDistractionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnActorHit.RemoveDynamic(
			this, &UTerritoryDistractionComponent::HandleOwnerHit);
	}
	Super::EndPlay(EndPlayReason);
}

void UTerritoryDistractionComponent::HandleOwnerHit(AActor*, AActor*,
	FVector, const FHitResult& Hit)
{
	if (bHasReported) return;
	const FVector ImpactLocation(Hit.ImpactPoint);
	ReportDistractionAtLocation(ImpactLocation.IsNearlyZero()
		? GetOwner()->GetActorLocation() : ImpactLocation);
}

bool UTerritoryDistractionComponent::ReportDistractionAtLocation(
	const FVector& WorldLocation)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority() || bHasReported) return false;
	AActor* Source = Owner->GetInstigator();
	if (!Source) Source = Owner;
	UAISense_Hearing::ReportNoiseEvent(World, WorldLocation,
		FMath::Max(0.f, Loudness), Source, FMath::Max(0.f, MaximumRange),
		TerritoryStealthTags::DistractionThrowable.GetTag().GetTagName());

	// Publish the same impact into Narrative GAS. Quests, abilities, Gameplay
	// Effects, and analytics may listen without polling AI Perception.
	FGameplayEventData Payload;
	Payload.EventTag = TerritoryStealthTags::DistractionImpactEvent;
	Payload.Instigator = Source;
	Payload.Target = Owner;
	Payload.OptionalObject = Owner;
	Payload.EventMagnitude = FMath::Max(0.f, Loudness);
	if (Source != Owner)
	{
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Source, Payload.EventTag, Payload);
	}
	bHasReported = true;
	return true;
}
