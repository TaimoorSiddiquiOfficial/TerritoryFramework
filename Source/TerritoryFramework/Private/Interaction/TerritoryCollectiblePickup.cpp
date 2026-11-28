#include "Interaction/TerritoryCollectiblePickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TerritoryCollectibleDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Subsystems/TerritoryCollectibleSubsystem.h"

ATerritoryCollectiblePickup::ATerritoryCollectiblePickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(120.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PickupSphere);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
}

void ATerritoryCollectiblePickup::BeginPlay()
{
	Super::BeginPlay();

	// Only the server decides whether the campaign already holds this collectible; a client
	// would be reading a projection it may not have received yet.
	if (HasAuthority())
	{
		RefreshCollectedState();
	}

	if (bCollectOnOverlap)
	{
		PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ATerritoryCollectiblePickup::HandlePickupOverlap);
	}
}

FGameplayTag ATerritoryCollectiblePickup::GetCollectibleTag() const
{
	if (CollectibleTag.IsValid())
	{
		return CollectibleTag;
	}

	return Definition ? Definition->CollectibleTag : FGameplayTag();
}

bool ATerritoryCollectiblePickup::CanBeCollectedBy(const AActor* Collector) const
{
	// A possessable pawn is the only thing that can reach a Dossier entry; a stray overlap from
	// a projectile or a dropped prop must not record a finding.
	return Collector != nullptr && Collector->IsA<APawn>();
}

bool ATerritoryCollectiblePickup::IsCollected() const
{
	const UWorld* World = GetWorld();
	const UTerritoryCollectibleSubsystem* Collectibles =
		World ? World->GetSubsystem<UTerritoryCollectibleSubsystem>() : nullptr;
	const FGameplayTag Tag = GetCollectibleTag();

	return Collectibles != nullptr && Tag.IsValid() && Collectibles->HasCollectibleBeenFound(Tag);
}

FTerritoryCollectibleMutationResult ATerritoryCollectiblePickup::Collect(AActor* Collector)
{
	FTerritoryCollectibleMutationResult Result;

	if (!HasAuthority())
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectiblePickup", "CollectNoAuthority",
			"Only the server records a collectible; this call had no authority.");
		return Result;
	}

	const FGameplayTag Tag = GetCollectibleTag();
	if (!Tag.IsValid())
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectiblePickup", "CollectNoTag",
			"This pickup has no collectible tag; set Collectible Tag or assign a Definition.");
		return Result;
	}

	if (bRecordedThisInstance)
	{
		// One actor instance records one finding per session; a second overlap in the same frame
		// is not a second discovery.
		Result.FailureReason = NSLOCTEXT("TerritoryCollectiblePickup", "CollectAlreadyRecorded",
			"This pickup already recorded its finding in this session.");
		return Result;
	}

	if (!CanBeCollectedBy(Collector))
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectiblePickup", "CollectNotAPawn",
			"Only a pawn collects a collectible.");
		return Result;
	}

	UWorld* World = GetWorld();
	UTerritoryCollectibleSubsystem* Collectibles =
		World ? World->GetSubsystem<UTerritoryCollectibleSubsystem>() : nullptr;
	if (Collectibles == nullptr)
	{
		Result.FailureReason = NSLOCTEXT("TerritoryCollectiblePickup", "CollectNoSubsystem",
			"This world has no collectible authority, so nothing can be recorded.");
		return Result;
	}

	if (!Collectibles->IsCollectibleLedgerReady())
	{
		// Writing now would publish a finding window the save has not hydrated yet.
		Result.FailureReason = NSLOCTEXT("TerritoryCollectiblePickup", "CollectNotReady",
			"The collectible window is not loaded yet; try the pickup again once the save has finished restoring.");
		return Result;
	}

	// The key is the placed actor, so a retried overlap on the same pickup cannot double-record.
	const FGuid ContextId = UTerritoryCollectibleSubsystem::MakeCollectibleContextId(
		FString::Printf(TEXT("Pickup|%s"), *GetPathName()));

	// Campaign-wide finding: the pickup does not invent a per-player identity it was never given.
	const FGuid DiscoveredBy = FGuid();

	Result = Collectibles->RecordCollectibleFound(Tag, SourceTerritory, DiscoveredBy, ContextId);
	if (!Result.bSuccess && !Result.bAlreadyCollected)
	{
		return Result;
	}

	bRecordedThisInstance = true;

	if (Result.bAlreadyCollected)
	{
		// The campaign already held this collectible: nothing was written, and a retrying
		// caller must not read that as a failure. The end state holds, so the pickup reports
		// success, but it does not broadcast a second discovery or destroy this instance.
		Result.bSuccess = true;
		RefreshCollectedState();
		return Result;
	}

	OnCollected.Broadcast(Collector, Result.Finding);
	RefreshCollectedState();

	if (bDestroyWhenCollected)
	{
		Destroy();
	}

	return Result;
}

void ATerritoryCollectiblePickup::RefreshCollectedState()
{
	const bool bCollected = IsCollected();

	if (PickupMesh)
	{
		PickupMesh->SetVisibility(!bCollected, /*bPropagateToChildren=*/true);
	}

	if (PickupSphere)
	{
		// A collected pickup stops taking overlaps, but stays in the level so a designer's
		// Blueprint hooks (audio, VFX, quest step) still have an actor to talk to.
		PickupSphere->SetGenerateOverlapEvents(!bCollected);
	}
}

void ATerritoryCollectiblePickup::HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bCollectOnOverlap || bRecordedThisInstance)
	{
		return;
	}

	Collect(OtherActor);
}
