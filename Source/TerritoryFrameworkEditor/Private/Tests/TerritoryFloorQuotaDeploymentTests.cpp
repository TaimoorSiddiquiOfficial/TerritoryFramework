// Tests for the per-floor staffing claim inside ATerritoryVolume::SpawnGuardsToCount.
//
// A floor row that authors a DesiredGuards count reserves that many guards out of the Territory's
// staffing target before the flat Priority order is consulted (ATerritoryGuardSpawnPoint::
// PlanFloorClaims). This is the only test that deploys real Narrative guards, because the claim is
// spent by the fill itself: asserting the plan alone would leave the fill free to ignore it, and
// asserting the snapshot alone would not show a single defender standing on the claimed floor.
//
// The fixture deploys through the production path on purpose - ApplyToTerritory, then
// SetDefinitionBinding + ApplyTerritoryDefinition per post - so the floor each post reports is the
// one its authored row names, not a value the test wrote onto the actor.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/NPCDefinition.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

namespace TerritoryFloorQuotaDeploymentTests
{
/** The class the fixture deploys. Native, so its CDO satisfies ValidateNarrativeSpawnDefinition. */
TSubclassOf<ATerritoryGuardCharacter> FixtureGuardClass()
{
	return ATerritoryGuardCharacter::StaticClass();
}

/** The real authored Place tag the sibling editor fixtures use, so the tag path is the map's. */
FGameplayTag FixturePlaceTag()
{
	return FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
}

UNPCDefinition* MakeGuardDefinition()
{
	UNPCDefinition* Definition = NewObject<UNPCDefinition>();
	Definition->CharacterID = TEXT("FloorQuotaDeploymentGuardCharacter");
	Definition->NPCID = TEXT("FloorQuotaDeploymentGuardNPC");
	Definition->NPCClassPath = FixtureGuardClass();
	Definition->bAllowMultipleInstances = true;
	return Definition;
}

/**
 * One Place standing in a Game world with a world context, two authored floors and two posts.
 *
 * Floor 0 is the better-placed post and authors no quota; floor 1 is the worse-placed post and
 * authors one guard. So the flat deployment order wants the ground post and the floor claim wants
 * the upper post, and which one actually holds the defender is the whole question.
 */
struct FQuotaFixture
{
	UWorld* World = nullptr;
	ATerritoryProperty* Place = nullptr;
	UTerritoryPlaceDefinition* Definition = nullptr;
	ATerritoryGuardSpawnPoint* GroundPost = nullptr;
	ATerritoryGuardSpawnPoint* UpperPost = nullptr;

	FQuotaFixture()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	}

	~FQuotaFixture()
	{
		Destroy();
	}

	void Destroy()
	{
		if (Place && IsValid(Place))
		{
			Place->DespawnGuards();
		}
		for (ATerritoryGuardSpawnPoint* Post : {GroundPost, UpperPost})
		{
			if (IsValid(Post))
			{
				Post->Destroy();
			}
		}
		GroundPost = nullptr;
		UpperPost = nullptr;
		if (World)
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			World = nullptr;
		}
		Place = nullptr;
		Definition = nullptr;
	}

	/** Author a row, then spawn and bind a post through the two calls the Definition path uses. */
	ATerritoryGuardSpawnPoint* SpawnBoundPost(
		const TCHAR* PostID, int32 FloorIndex, int32 Priority, const FVector& Location)
	{
		FTerritoryGuardPostTemplate Row;
		Row.GuardPostID = FName(PostID);
		Row.StableGuardPostGUID = FGuid::NewGuid();
		Row.FloorIndex = FloorIndex;
		Row.Priority = Priority;
		Row.ReserveSlots = 1;
		Row.RelativeTransform = FTransform(Location);
		Definition->GuardPosts.Add(Row);

		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(
			ATerritoryGuardSpawnPoint::StaticClass(), FTransform(Location));
		if (!Post)
		{
			return nullptr;
		}
		Post->SetActorGUID_Implementation(FGuid::NewGuid());
		Post->SetDefinitionBinding(Definition, FName(PostID));
		// Bound and applied, but not yet owned: BindToTerritory is private and is called from the
		// test class, which is the friend - the fixture struct is not.
		return Post->ApplyTerritoryDefinition() ? Post : nullptr;
	}

	/** Author the whole Place and its two posts the way the content pipeline does. */
	bool Build()
	{
		if (!World)
		{
			return false;
		}
		Place = World->SpawnActor<ATerritoryProperty>(
			ATerritoryProperty::StaticClass(), FTransform::Identity);
		if (!Place)
		{
			return false;
		}

		Definition = NewObject<UTerritoryPlaceDefinition>();
		Definition->TerritoryTag = FixturePlaceTag();
		Definition->DisplayName = FText::FromString(TEXT("Floor Quota Place"));
		Definition->StableTerritoryGUID = FGuid::NewGuid();
		Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		Definition->DefaultGuardDefinition = MakeGuardDefinition();
		Definition->InitialGuardCount = 1;

		FTerritoryFloorTemplate GroundFloor;
		GroundFloor.FloorIndex = 0;
		GroundFloor.DisplayName = FText::FromString(TEXT("Ground"));
		GroundFloor.DesiredGuards = 0; // Claims nothing: takes what the flat order gives it.
		FTerritoryFloorTemplate UpperFloor;
		UpperFloor.FloorIndex = 1;
		UpperFloor.DisplayName = FText::FromString(TEXT("Upper"));
		UpperFloor.DesiredGuards = 1; // Claims the single guard outright.
		Definition->Floors = {GroundFloor, UpperFloor};

		if (!Definition->ApplyToTerritory(Place))
		{
			return false;
		}

		// GetOwningTerritory() resolves the post's authored tag through the registry and reports
		// null while the tag names no loaded territory, so the owner must be registered first.
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			if (Registry->RegisterTerritory(Place) != ETerritoryRegistrationResult::Success)
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
			TEXT("Narrative.Factions.Heroes"), false);
		Place->SetRole(ROLE_Authority);

		// Claim the Place through the public atomic commit rather than writing the protected
		// ownership struct, so the fixture holds the same state a captured Place holds. Conditions
		// are bypassed because the fixture is the authority that decided this capture happened.
		FTerritoryOwnershipData Ownership;
		Ownership.OwningFaction = Heroes;
		Ownership.State = ETerritoryState::Claimed;
		Ownership.DesiredGuardCount = 1;
		if (!Place->CommitOwnershipData(Ownership, FTerritoryTransitionContext(), true))
		{
			UE_LOG(LogTemp, Error, TEXT("Floor quota fixture: ownership commit was refused"));
			return false;
		}

		GroundPost = SpawnBoundPost(TEXT("Post_Ground"), 0, 100, FVector::ZeroVector);
		UpperPost = SpawnBoundPost(TEXT("Post_Upper"), 1, 50, FVector(600.f, 0.f, 1000.f));
		return GroundPost != nullptr && UpperPost != nullptr;
	}

	/** Re-author one floor's quota. The runtime reads Floors off the bound Definition directly. */
	void SetFloorQuota(int32 FloorIndex, int32 Quota)
	{
		for (FTerritoryFloorTemplate& Floor : Definition->Floors)
		{
			if (Floor.FloorIndex == FloorIndex)
			{
				Floor.DesiredGuards = Quota;
			}
		}
	}
};
}

/**
 * A claiming floor is staffed ahead of the flat deployment order, and nothing else changes.
 *
 * The second leg is the control that makes the first one mean something: with floor 1's quota
 * removed the same fixture, the same priorities and the same target put the defender back on the
 * ground post. Without it a green first leg could be explained by the fixture happening to sort the
 * upper post first, which would prove nothing about the claim.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFloorQuotaDeployment,
	"TerritoryFramework.Guards.Floors.ClaimedFloorIsStaffedAheadOfTheFlatOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFloorQuotaDeployment::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorQuotaDeploymentTests;
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);

	FQuotaFixture Fixture;
	if (!TestTrue(TEXT("Floor quota fixture builds"), Fixture.Build()))
	{
		return false;
	}
	ATerritoryProperty* Place = Fixture.Place;
	ATerritoryGuardSpawnPoint* GroundPost = Fixture.GroundPost;
	ATerritoryGuardSpawnPoint* UpperPost = Fixture.UpperPost;

	// The binding BeginPlay makes for each authored post: ownership is resolved from the Place
	// reference and recorded on the post. TrySpawnSingleGuard refuses a post that does not report
	// this Place as its owner, and GetOwningTerritory resolves the post's authored tag through the
	// registry, which the fixture registered.
	GroundPost->BindToTerritory(Place);
	UpperPost->BindToTerritory(Place);

	// Preconditions, so a fixture that never reaches the fill is not mistaken for a claim failure.
	TestTrue(TEXT("Precondition: the Place is claimed by a valid faction"),
		Place->GetOwnershipData().State == ETerritoryState::Claimed
		&& Place->GetOwnershipData().OwningFaction.IsValid());
	TestTrue(TEXT("Precondition: the ground post is bound to the Place"),
		GroundPost->GetOwningTerritory() == static_cast<ATerritoryVolume*>(Place));
	TestTrue(TEXT("Precondition: the upper post is bound to the Place"),
		UpperPost->GetOwningTerritory() == static_cast<ATerritoryVolume*>(Place));
	TestEqual(TEXT("Precondition: the row put the ground post on floor 0"),
		GroundPost->GetFloorIndex(), 0);
	TestEqual(TEXT("Precondition: the row put the upper post on floor 1"),
		UpperPost->GetFloorIndex(), 1);
	TestEqual(TEXT("Precondition: both posts are legal deployment capacity"),
		Place->GetMaxGuardCount(), 2);
	TestTrue(TEXT("Precondition: the flat deployment order prefers the ground post"),
		GroundPost->Priority > UpperPost->Priority);
	TestEqual(TEXT("Precondition: the Place's authored staffing target is one guard"),
		Place->GetConfiguredGuardCount(), 1);
	TestEqual(TEXT("Precondition: no guard is deployed yet"), Place->GetSpawnedGuardCount(), 0);

	// Leg 1 - the claim. Floor 1 authors one guard, floor 0 authors none.
	Place->SpawnGuardsToCount(1);
	TestEqual(TEXT("The target deploys exactly one guard"), Place->GetSpawnedGuardCount(), 1);
	TestEqual(TEXT("The claimed floor holds the guard even though the flat order prefers floor 0"),
		UpperPost->GetActiveGuardCount(), 1);
	TestEqual(TEXT("The unclaimed better-placed floor is passed over"),
		GroundPost->GetActiveGuardCount(), 0);

	// Leg 2 - the control. Same fixture, same priorities, same target, no quota on either floor.
	Place->DespawnGuards();
	TestEqual(TEXT("Control setup: despawn leaves no guard on the upper post"),
		UpperPost->GetActiveGuardCount(), 0);
	TestEqual(TEXT("Control setup: despawn leaves no guard on the ground post"),
		GroundPost->GetActiveGuardCount(), 0);

	Fixture.SetFloorQuota(1, 0);
	Place->SpawnGuardsToCount(1);
	TestEqual(TEXT("Control: the target still deploys exactly one guard"),
		Place->GetSpawnedGuardCount(), 1);
	TestEqual(TEXT("Control: with no authored quota the flat order decides, so floor 0 is staffed"),
		GroundPost->GetActiveGuardCount(), 1);
	TestEqual(TEXT("Control: and the upper floor is empty, as it was before this feature"),
		UpperPost->GetActiveGuardCount(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
