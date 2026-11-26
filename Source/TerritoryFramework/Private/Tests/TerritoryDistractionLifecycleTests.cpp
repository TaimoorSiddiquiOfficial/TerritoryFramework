#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/TerritoryDistractionAbility.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryStealthTags.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/TerritoryDistractionComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Items/InventoryComponent.h"
#include "Items/EquippableItem.h"
#include "Items/NarrativeItem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionLifecycle,
	"TerritoryFramework.Stealth.Regression.DistractionCancellationAndInventoryRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionLifecycle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Distraction test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardCharacter* Character = World->SpawnActor<ATerritoryGuardCharacter>();
	UNarrativeAbilitySystemComponent* ASC = Character->GetNarrativeAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	UNarrativeInventoryComponent* Inventory = Character->GetInventoryComponent();
	const FItemAddResult Added = Inventory->TryAddItemFromClass(UNarrativeItem::StaticClass(), 1);
	if (!TestEqual(TEXT("Native item was added"), Added.Stacks.Num(), 1))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UNarrativeItem* Item = Added.Stacks[0];
	// Exercise the documented unequipped-consumable option with a native item.
	TGuardValue<bool> EquippedOption(GetMutableDefault<UTerritoryDistractionAbility>()->bRequireEquippedNarrativeItemSource, false);
	FGameplayAbilitySpecHandle Handle = Character->AddAbility(UTerritoryDistractionAbility::StaticClass(), Item);
	UTerritoryDistractionAbility* Ability = CastChecked<UTerritoryDistractionAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	// Native class construction uses its C++ defaults for the per-actor instance;
	// configure both the CDO eligibility check and this transient test instance.
	Ability->bRequireEquippedNarrativeItemSource = false;
	int32 ThrownEvents = 0;
	TWeakObjectPtr<ATerritoryDistractionProjectile> LastProjectile;
	const FDelegateHandle ThrownListener = ASC->GenericGameplayEventCallbacks.FindOrAdd(TerritoryStealthTags::DistractionThrownEvent).AddLambda(
		[&](const FGameplayEventData* Event)
		{
			++ThrownEvents;
			LastProjectile = Cast<ATerritoryDistractionProjectile>(const_cast<AActor*>(Event->Target.Get()));
		});
	TestTrue(TEXT("A real source item in the avatar inventory is ready"), Ability->IsThrowableSourceItemReady(Item));

	// CommitAbility calls external GAS listeners before returning true.
	int32 CommitCallbacks = 0;
	const FDelegateHandle CancelCommit = ASC->AbilityCommittedCallbacks.AddLambda(
		[&](UGameplayAbility*) { ++CommitCallbacks; ASC->CancelAbilityHandle(Handle); });
	ASC->TryActivateAbility(Handle);
	TestEqual(TEXT("Cancellation probe reached a real GAS commit"), CommitCallbacks, 1);
	TestEqual(TEXT("Cancellation during commit produces no throw event"), ThrownEvents, 0);
	TestEqual(TEXT("Cancellation during commit consumes no item"), Item->GetQuantity(), 1);
	TestFalse(TEXT("Cancelled ability has ended"), Ability->IsActive());
	ASC->AbilityCommittedCallbacks.Remove(CancelCommit);

	// World spawn callbacks can cancel while SpawnActorDeferred is on the stack.
	TWeakObjectPtr<AActor> RejectedProjectile;
	FDelegateHandle SpawnListener = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
		[&](AActor* Actor)
		{
			if (Actor->IsA<ATerritoryDistractionProjectile>())
			{
				RejectedProjectile = Actor;
				ASC->CancelAbilityHandle(Handle);
			}
		}));
	ASC->TryActivateAbility(Handle);
	World->RemoveOnActorSpawnedHandler(SpawnListener);
	TestTrue(TEXT("Spawn cancellation probe saw the deferred projectile"), RejectedProjectile.IsExplicitlyNull() == false);
	TestTrue(TEXT("Cancelled deferred projectile is removed"), !RejectedProjectile.IsValid() || RejectedProjectile->IsActorBeingDestroyed());
	TestEqual(TEXT("Cancelled spawn consumes no item"), Item->GetQuantity(), 1);
	TestEqual(TEXT("Cancelled spawn publishes no success"), ThrownEvents, 0);

	SpawnListener = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
		[](AActor* Actor) { if (Actor->IsA<ATerritoryDistractionProjectile>()) Actor->Destroy(); }));
	ASC->TryActivateAbility(Handle);
	World->RemoveOnActorSpawnedHandler(SpawnListener);
	TestEqual(TEXT("A destroyed spawn consumes no item"), Item->GetQuantity(), 1);
	TestEqual(TEXT("A destroyed spawn publishes no success"), ThrownEvents, 0);

	// Native Load rebuilds Items, but old live objects can retain OwningInventory.
	Inventory->PrepareForSave_Implementation();
	Inventory->Load_Implementation();
	UNarrativeItem* RestoredItem = Inventory->GetItems()[0];
	TestTrue(TEXT("Native restore creates a new source object"), RestoredItem != Item);
	TestEqual(TEXT("Native restore preserves the unspent unit"), RestoredItem->GetQuantity(), 1);
	TestFalse(TEXT("The old source is rejected after load"), Ability->IsThrowableSourceItemReady(Item));
	TestFalse(TEXT("A stale ability spec cannot consume the restored inventory"), ASC->TryActivateAbility(Handle));
	ASC->ClearAbility(Handle);
	Handle = Character->AddAbility(UTerritoryDistractionAbility::StaticClass(), RestoredItem);
	Ability = CastChecked<UTerritoryDistractionAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	Ability->bRequireEquippedNarrativeItemSource = false;

	ATerritoryGuardCharacter* OtherCharacter = World->SpawnActor<ATerritoryGuardCharacter>();
	UNarrativeAbilitySystemComponent* OtherASC = OtherCharacter->GetNarrativeAbilitySystemComponent();
	OtherASC->InitAbilityActorInfo(OtherCharacter, OtherCharacter);
	FGameplayAbilitySpecHandle ForeignHandle = OtherCharacter->AddAbility(UTerritoryDistractionAbility::StaticClass(), RestoredItem);
	CastChecked<UTerritoryDistractionAbility>(OtherASC->FindAbilitySpecFromHandle(ForeignHandle)->GetPrimaryInstance())->bRequireEquippedNarrativeItemSource = false;
	TestFalse(TEXT("Another avatar cannot throw this character's item"), OtherASC->TryActivateAbility(ForeignHandle));
	OtherASC->ClearAbility(ForeignHandle);

	Character->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client cannot execute this server-only throw locally"), ASC->TryActivateAbility(Handle, false));
	TestEqual(TEXT("A rejected client attempt consumes nothing"), RestoredItem->GetQuantity(), 1);
	Character->SetRole(ROLE_Authority);

	bool bReentered = false;
	const FDelegateHandle ReenterCommit = ASC->AbilityCommittedCallbacks.AddLambda(
		[&](UGameplayAbility*)
		{
			if (!bReentered)
			{
				bReentered = true;
				ASC->CancelAbilityHandle(Handle);
				ASC->TryActivateAbility(Handle);
			}
		});
	TestTrue(TEXT("Restored source can activate through Native GAS"), ASC->TryActivateAbility(Handle));
	ASC->AbilityCommittedCallbacks.Remove(ReenterCommit);
	TestTrue(TEXT("Commit listener exercised cancel and reactivation"), bReentered);
	TestEqual(TEXT("Successful throw publishes exactly one event"), ThrownEvents, 1);
	TestEqual(TEXT("Native inventory consumed the last unit"), Inventory->GetItems().Num(), 0);
	TestTrue(TEXT("Projectile survives its completed ability"), LastProjectile.IsValid() && !LastProjectile->IsActorBeingDestroyed());
	TestFalse(TEXT("Successful instant throw ends its ability"), Ability->IsActive());
	TestFalse(TEXT("Depleted source cannot throw again"), ASC->TryActivateAbility(Handle));
	Inventory->PrepareForSave_Implementation();
	Inventory->Load_Implementation();
	TestEqual(TEXT("Saving after the throw does not restore the consumed unit"), Inventory->GetItems().Num(), 0);

	if (LastProjectile.IsValid())
	{
		int32 ImpactEvents = 0;
		bool bNestedImpact = true;
		const FDelegateHandle ImpactListener = ASC->GenericGameplayEventCallbacks.FindOrAdd(TerritoryStealthTags::DistractionImpactEvent).AddLambda(
			[&](const FGameplayEventData*)
			{
				++ImpactEvents;
				// Bound the probe so a regression fails without recursing indefinitely.
				if (ImpactEvents == 1) bNestedImpact = LastProjectile->Distraction->ReportDistractionAtLocation(FVector(100.f));
			});
		TestTrue(TEXT("First impact publishes through Native GAS"), LastProjectile->Distraction->ReportDistractionAtLocation(FVector(100.f)));
		TestFalse(TEXT("An impact listener cannot republish the same impact"), bNestedImpact);
		TestEqual(TEXT("Impact event is delivered once"), ImpactEvents, 1);
		TestFalse(TEXT("Later collision cannot repeat impact"), LastProjectile->Distraction->ReportDistractionAtLocation(FVector(100.f)));
		ASC->GenericGameplayEventCallbacks.FindChecked(TerritoryStealthTags::DistractionImpactEvent).Remove(ImpactListener);
		LastProjectile->Destroy();
	}
	ASC->GenericGameplayEventCallbacks.FindChecked(TerritoryStealthTags::DistractionThrownEvent).Remove(ThrownListener);
	ASC->ClearAllAbilities();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionEquippedAsset,
	"TerritoryFramework.Stealth.Regression.DistractionEquippedBlueprintLastUnit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionEquippedAsset::RunTest(const FString& Parameters)
{
	UClass* RockClass = LoadClass<UEquippableItem>(nullptr,
		TEXT("/TerritoryFramework/Stealth/Equippable_Throwable_Rock.Equippable_Throwable_Rock_C"));
	if (!TestNotNull(TEXT("Shipped Narrative equippable Blueprint loads"), RockClass)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Equipped throw world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardCharacter* Character = World->SpawnActor<ATerritoryGuardCharacter>();
	// This fixture does not run a full NPC spawn. Supply Native's definition read
	// model without starting asynchronous appearance/AI loading in a unit world.
	FObjectPropertyBase* Definition = FindFProperty<FObjectPropertyBase>(ANarrativeNPCCharacter::StaticClass(), TEXT("NPCDefinition"));
	if (!TestNotNull(TEXT("Native definition field exists"), Definition))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	Definition->SetObjectPropertyValue_InContainer(Character, NewObject<UNPCDefinition>(Character));
	UNarrativeAbilitySystemComponent* ASC = Character->GetNarrativeAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Territory.Property.Benefit.WeaponUpgrades")));
	UNarrativeInventoryComponent* Inventory = Character->GetInventoryComponent();
	const FItemAddResult Added = Inventory->TryAddItemFromClass(RockClass, 1);
	if (!TestEqual(TEXT("Shipped rock is added"), Added.Stacks.Num(), 1))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UEquippableItem* Rock = CastChecked<UEquippableItem>(Added.Stacks[0]);
	TestTrue(TEXT("Native auto-equips the shipped rock"), Rock->IsEquipped());
	FGameplayAbilitySpecHandle Handle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.SourceObject == Rock && Spec.Ability->IsA<UTerritoryDistractionAbility>()) Handle = Spec.Handle;
	}
	if (!TestTrue(TEXT("Native equipment granted the authored ability with item source"), Handle.IsValid()))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	int32 ThrownEvents = 0;
	TWeakObjectPtr<ATerritoryDistractionProjectile> Projectile;
	const FDelegateHandle Listener = ASC->GenericGameplayEventCallbacks.FindOrAdd(TerritoryStealthTags::DistractionThrownEvent).AddLambda(
		[&](const FGameplayEventData* Event)
		{
			++ThrownEvents;
			Projectile = Cast<ATerritoryDistractionProjectile>(const_cast<AActor*>(Event->Target.Get()));
		});
	TestTrue(TEXT("Authored throw activates"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("Last equipped unit produces one successful throw"), ThrownEvents, 1);
	TestEqual(TEXT("Last unit is removed through Native inventory"), Inventory->GetItems().Num(), 0);
	TestFalse(TEXT("Native removal unequips the rock"), Rock->IsEquipped());
	TestNull(TEXT("Native equipment revokes its ability spec"), ASC->FindAbilitySpecFromHandle(Handle));
	TestTrue(TEXT("Paid projectile survives Native ability revocation"), Projectile.IsValid() && !Projectile->IsActorBeingDestroyed());
	if (Projectile.IsValid()) Projectile->Destroy();
	ASC->GenericGameplayEventCallbacks.FindChecked(TerritoryStealthTags::DistractionThrownEvent).Remove(Listener);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

/**
 * The launch transform used to start on the camera view vector. In a third-person setup that is
 * metres behind the character, so the stone appeared near the camera and a held item caught the aim
 * trace, producing backwards throws. The contract now pinned here: the spawn point stays at the
 * thrower, and the direction never points backwards.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionLaunchOrigin,
	"TerritoryFramework.Stealth.Regression.DistractionLaunchOriginAndDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionLaunchOrigin::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Distraction aim test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);

	ATerritoryGuardCharacter* Thrower = World->SpawnActor<ATerritoryGuardCharacter>(
		ATerritoryGuardCharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Thrower"), Thrower))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}

	// A blocking wall one metre ahead: the aim trace hits it, and the throw must still leave the
	// hand forward rather than flipping backwards over the shoulder.
	AActor* Wall = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(120.f, 0.f, 100.f), FRotator::ZeroRotator);
	if (Wall)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
		Box->InitBoxExtent(FVector(5.f, 200.f, 200.f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetRootComponent(Box);
		Box->RegisterComponent();
		Wall->SetActorLocation(FVector(120.f, 0.f, 100.f));
	}

	UNarrativeAbilitySystemComponent* ASC = Thrower->GetNarrativeAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Thrower, Thrower);
	const FGameplayAbilitySpecHandle AbilityHandle =
		Thrower->AddAbility(UTerritoryDistractionAbility::StaticClass());
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
	UTerritoryDistractionAbility* Ability = Spec
		? Cast<UTerritoryDistractionAbility>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Granted ability instance"), Ability))
	{
		if (Wall) Wall->Destroy();
		Thrower->Destroy();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	FTransform Launch = FTransform::Identity;
	const bool bResolved = Ability->GetDistractionLaunchTransform(Launch);
	TestTrue(TEXT("Launch transform resolves without a controller"), bResolved);

	const FVector FromThrower = Launch.GetLocation() - Thrower->GetActorLocation();
	TestTrue(TEXT("Spawn point stays at the thrower, not on the camera"), FromThrower.Size() < 200.f);
	TestTrue(TEXT("Launch direction never points backwards"),
		Launch.GetRotation().Vector().GetSafeNormal().X > 0.f);

	if (Wall) Wall->Destroy();
	Thrower->Destroy();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

/**
 * The stone left along the crosshair but gravity dropped it 0.9 m at 5 m and 3.4 m at 10 m,
 * which players read as "the throw does not follow the crosshair". The launch must now bend up
 * just enough that the authored speed and gravity land on the aim point, and it must degrade
 * to "no compensation" only when the target is genuinely unreachable.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionAimCompensation,
	"TerritoryFramework.Stealth.Regression.DistractionAimCompensation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionAimCompensation::RunTest(const FString& Parameters)
{
	// Authored values: ATerritoryDistractionProjectile InitialSpeed 1200, gravity scale 1.0,
	// world gravity 980 cm/s^2.
	const float Speed = 1200.f;
	const float Gravity = 980.f;
	const FVector Start(0.f, 0.f, 100.f);

	// 1) Reachable target: the ballistic arc must actually land on it.
	for (const float Distance : {500.f, 1000.f})
	{
		const FVector AimPoint = Start + FVector(Distance, 0.f, 0.f);
		FVector Direction = FVector::ZeroVector;
		const bool bSolved = UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
			Start, AimPoint, Speed, Gravity, 55.f, Direction);
		TestTrue(FString::Printf(TEXT("%.0f cm target resolves a launch arc"), Distance), bSolved);
		if (!bSolved) continue;

		TestTrue(FString::Printf(TEXT("%.0f cm arc points forward"), Distance), Direction.X > 0.f);
		TestTrue(FString::Printf(TEXT("%.0f cm arc is not a mortar lob"), Distance),
			Direction.Z < 1.f);

		// Integrate the parabola and confirm the flight passes through the aim point.
		const FVector Velocity = Direction * Speed;
		const float HorizontalSpeed = Velocity.Size2D();
		const float FlightTime = HorizontalSpeed > KINDA_SMALL_NUMBER
			? (AimPoint - Start).Size2D() / HorizontalSpeed : 0.f;
		const FVector Landed = Start + Velocity * FlightTime
			+ FVector(0.f, 0.f, -0.5f * Gravity * FlightTime * FlightTime);
		TestTrue(FString::Printf(TEXT("%.0f cm arc passes through the aim point"), Distance),
			FVector::Dist(Landed, AimPoint) < 1.f);

		// Straight-line aim would have undershot this much; the compensation must beat it.
		const FVector Straight = (AimPoint - Start).GetSafeNormal();
		const FVector StraightVelocity = Straight * Speed;
		const FVector StraightLanded = Start + StraightVelocity * FlightTime
			+ FVector(0.f, 0.f, -0.5f * Gravity * FlightTime * FlightTime);
		TestTrue(FString::Printf(TEXT("%.0f cm compensation beats straight aim"), Distance),
			FVector::Dist(Landed, AimPoint) < FVector::Dist(StraightLanded, AimPoint));
	}

	// 2) Unreachable target: no arc exists, so the caller keeps its straight aim direction.
	{
		FVector Direction = FVector::ZeroVector;
		const FVector TooFar = Start + FVector(100000.f, 0.f, 0.f);
		TestFalse(TEXT("Unreachable target reports no arc"),
			UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
				Start, TooFar, Speed, Gravity, 55.f, Direction));
	}

	// 3) Disabled physics must not fabricate a launch direction.
	{
		FVector Direction = FVector::ZeroVector;
		const FVector AimPoint = Start + FVector(500.f, 0.f, 0.f);
		TestFalse(TEXT("Zero gravity reports no arc"),
			UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
				Start, AimPoint, Speed, 0.f, 55.f, Direction));
		TestFalse(TEXT("Zero speed reports no arc"),
			UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
				Start, AimPoint, 0.f, Gravity, 55.f, Direction));
	}

	// 4) A target directly overhead has no horizontal arc to aim along.
	{
		FVector Direction = FVector::ZeroVector;
		TestFalse(TEXT("A target directly overhead reports no arc"),
			UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
				Start, Start + FVector(0.f, 0.f, 300.f), Speed, Gravity, 55.f, Direction));
	}

	// 5) The pitch cap is honoured, so a shallow cap must not overshoot the limit.
	{
		FVector Direction = FVector::ZeroVector;
		const FVector AimPoint = Start + FVector(900.f, 0.f, 0.f);
		const bool bSolved = UTerritoryDistractionAbility::ComputeAimCompensatedDirection(
			Start, AimPoint, Speed, Gravity, 5.f, Direction);
		TestTrue(TEXT("A capped arc still resolves"), bSolved);
		TestTrue(TEXT("Capped elevation stays under the 5 degree limit"),
			Direction.Rotation().Pitch <= 5.01f);
	}

	return true;
}

/**
 * A ServerOnly ability runs on the server's copy of a remote client, where the camera is only
 * refreshed by ServerUpdateCamera. Until the first camera update arrives GetPlayerViewPoint
 * returns the view-target actor rotation, which is yaw only, so the throw silently lost pitch.
 * This pins the decision the launch code relies on.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionRemoteClientAim,
	"TerritoryFramework.Stealth.Regression.DistractionRemoteClientReplicatedAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionRemoteClientAim::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Remote controller without a camera cache uses the replicated aim"),
		UTerritoryDistractionAbility::ShouldUseReplicatedClientAim(false, false));
	TestFalse(TEXT("Remote controller with a camera cache keeps the synced camera"),
		UTerritoryDistractionAbility::ShouldUseReplicatedClientAim(false, true));
	TestFalse(TEXT("A local controller always keeps its own camera"),
		UTerritoryDistractionAbility::ShouldUseReplicatedClientAim(true, false));
	TestFalse(TEXT("A local controller with a camera keeps its own camera"),
		UTerritoryDistractionAbility::ShouldUseReplicatedClientAim(true, true));
	return true;
}

/**
 * The projectile is spawned deferred, given a WORLD-space launch velocity, and only then finished.
 * FinishSpawning runs UProjectileMovementComponent::InitializeComponent, which transforms Velocity
 * by the component rotation whenever bInitialVelocityInLocalSpace is true (the engine default).
 * A throw aimed along -X therefore left along +X, which players reported as "the stone does not
 * follow the crosshair". This pins the world-space contract on the CDO and proves a finished
 * actor keeps the direction it was given.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionWorldSpaceLaunch,
	"TerritoryFramework.Stealth.Regression.DistractionWorldSpaceLaunchVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionWorldSpaceLaunch::RunTest(const FString& Parameters)
{
	// 1) The shipped defaults must opt out of the local-space conversion.
	const ATerritoryDistractionProjectile* Defaults =
		GetDefault<ATerritoryDistractionProjectile>();
	if (!TestNotNull(TEXT("Distraction projectile CDO"), Defaults)) return false;
	if (!TestNotNull(TEXT("Distraction projectile movement"), Defaults->ProjectileMovement.Get()))
	{
		return false;
	}
	TestFalse(TEXT("Distraction projectile default velocity is authored in world space"),
		Defaults->ProjectileMovement->bInitialVelocityInLocalSpace);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World space launch test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);

	// 2) A finished projectile keeps the world direction it was given, for a yawed throw and a
	//    pitched throw. Before the fix the yawed case reversed and the pitched case flattened.
	const TArray<FRotator> LaunchRotations = {
		FRotator(0.f, 180.f, 0.f),
		FRotator(18.f, 90.f, 0.f),
		FRotator(-12.f, -135.f, 0.f)
	};
	for (const FRotator& LaunchRotation : LaunchRotations)
	{
		const FVector SpawnLocation(0.f, 0.f, 100.f);
		const float LaunchSpeed = 1200.f;
		const FVector ExpectedDirection = LaunchRotation.Vector().GetSafeNormal();

		ATerritoryDistractionProjectile* Projectile =
			World->SpawnActorDeferred<ATerritoryDistractionProjectile>(
				ATerritoryDistractionProjectile::StaticClass(),
				FTransform(LaunchRotation, SpawnLocation), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!TestNotNull(*FString::Printf(TEXT("Deferred projectile %s"),
			*LaunchRotation.ToString()), Projectile))
		{
			continue;
		}
		if (!TestNotNull(*FString::Printf(TEXT("Projectile movement %s"),
			*LaunchRotation.ToString()), Projectile->ProjectileMovement.Get()))
		{
			Projectile->Destroy();
			continue;
		}

		// This mirrors UTerritoryDistractionAbility::PerformThrow.
		Projectile->ProjectileMovement->bInitialVelocityInLocalSpace = false;
		Projectile->ProjectileMovement->Velocity = ExpectedDirection * LaunchSpeed;
		Projectile->FinishSpawning(FTransform(LaunchRotation, SpawnLocation));

		const FVector FinishedVelocity = Projectile->ProjectileMovement->Velocity;
		TestTrue(*FString::Printf(TEXT("%s keeps its world-space speed after spawning"),
			*LaunchRotation.ToString()),
			FMath::IsNearlyEqual(FinishedVelocity.Size(), LaunchSpeed, 1.f));
		TestTrue(*FString::Printf(TEXT("%s keeps its world-space direction after spawning"),
			*LaunchRotation.ToString()),
			FVector::DotProduct(FinishedVelocity.GetSafeNormal(), ExpectedDirection) > 0.999f);

		Projectile->Destroy();
	}

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

/**
 * The screen centre is only the aim point while the project hides the cursor. A visible cursor
 * means the player is pointing at a pixel, and the throw must use the camera ray through it.
 * The pointer state of a remote client is not replicated, so that decision stays local-only.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDistractionCursorAim,
	"TerritoryFramework.Stealth.Regression.DistractionCursorAimDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDistractionCursorAim::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A local player with a visible cursor aims at the pointer"),
		UTerritoryDistractionAbility::ShouldAimWithMouseCursor(true, true, true));
	TestFalse(TEXT("A hidden cursor keeps the crosshair ray"),
		UTerritoryDistractionAbility::ShouldAimWithMouseCursor(true, false, true));
	TestFalse(TEXT("No pointer position keeps the crosshair ray"),
		UTerritoryDistractionAbility::ShouldAimWithMouseCursor(true, true, false));
	TestFalse(TEXT("A remote client without a visible cursor never deprojects"),
		UTerritoryDistractionAbility::ShouldAimWithMouseCursor(false, true, true));
	TestFalse(TEXT("A remote client never deprojects"),
		UTerritoryDistractionAbility::ShouldAimWithMouseCursor(false, false, false));
	return true;
}

#endif
