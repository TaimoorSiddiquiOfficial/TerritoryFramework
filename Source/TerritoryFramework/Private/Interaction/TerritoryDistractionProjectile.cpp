#include "Interaction/TerritoryDistractionProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/TerritoryDistractionComponent.h"

ATerritoryDistractionProjectile::ATerritoryDistractionProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 10.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->SetCanEverAffectNavigation(false);
	Collision->CanCharacterStepUpOn = ECB_No;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
		TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->InitialSpeed = 1200.f;
	ProjectileMovement->MaxSpeed = 1200.f;
	ProjectileMovement->ProjectileGravityScale = 1.f;
	// The throwing ability spawns this actor deferred, writes a WORLD-space launch velocity
	// (hand socket -> crosshair aim point), and only then calls FinishSpawning. That call runs
	// UProjectileMovementComponent::InitializeComponent, which transforms the velocity by the
	// component rotation when this flag is true (the engine default). The stone was therefore
	// rotated a second time by its own launch rotation: a throw aimed along -X left along +X,
	// which players reported as "the stone does not follow the crosshair". Every throw this
	// plugin produces supplies world space, so the component must never re-interpret it.
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.35f;
	ProjectileMovement->Friction = 0.4f;

	Distraction = CreateDefaultSubobject<UTerritoryDistractionComponent>(
		TEXT("Distraction"));
}

void ATerritoryDistractionProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (Collision)
	{
		Collision->IgnoreActorWhenMoving(GetOwner(), true);
		Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	}
}

