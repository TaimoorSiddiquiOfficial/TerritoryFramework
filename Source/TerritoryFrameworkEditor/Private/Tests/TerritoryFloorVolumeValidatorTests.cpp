// Tests for CheckFloorVolumes, the level-scoped check over the floor regions that give a floor row
// its geometry.
//
// A floor row is an integer grouping key with no volume, bounds, anchor or transform, so a Place can
// declare floors that separate nothing and no other check can see it. CheckFloorVolumes is the only
// place that reports it, which is why every test here builds a real level and calls ValidateLevel
// rather than calling the check directly: the wiring into ValidateLevel is part of the contract.
//
// Nothing asserts the validator's boolean result. One of the three rules writes an error and two
// write warnings, and Scripts/Territory/editor_validation.py already treats a single warning as a
// failure, so the message is what is under test.
//
// The two deliberate narrowings are pinned by their own tests rather than left to the doc comment:
// a post-less floor row is silent (PostlessFloorIsNotReported), and so is a same-floor overlap
// (TwoRegionsOfOneFloorAreNotReported). Both would otherwise be the two easiest false positives to
// introduce while "improving" the check.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryFloorVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace TerritoryFloorVolumeValidatorTests
{
FGameplayTag BlacksmithTag()
{
	return FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
}

/** A second real Place tag. Two Places are what makes the "scoped to its own Place" rule testable. */
FGameplayTag WarehouseTag()
{
	return FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Warehouse"), false);
}

/**
 * A level holding two Places, each with its authored Definition, plus the authoring helpers the
 * rules are stated in terms of: a floor row, a guard-post row, and a floor region.
 *
 * Regions are spawned with World->SpawnActor so they land in the level's actor array, which is the
 * array GetActorsForValidation reads. They are never registered with the registry: the check reads
 * the authored PlaceDefinition / FloorIndex / FloorBounds off the actors, so it stays a pure
 * authoring question and does not need the runtime registry to be populated.
 */
struct FFloorFixture
{
	UWorld* World = nullptr;
	ATerritoryProperty* Blacksmith = nullptr;
	UTerritoryPlaceDefinition* BlacksmithDefinition = nullptr;
	ATerritoryProperty* Warehouse = nullptr;
	UTerritoryPlaceDefinition* WarehouseDefinition = nullptr;

	FFloorFixture()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
	}

	~FFloorFixture()
	{
		if (World)
		{
			World->DestroyWorld(false);
			World = nullptr;
		}
	}

	UTerritoryPlaceDefinition* MakePlace(const FGameplayTag& Tag, const FString& DisplayName,
		ATerritoryProperty*& OutActor)
	{
		if (!World) return nullptr;

		OutActor = World->SpawnActor<ATerritoryProperty>(
			ATerritoryProperty::StaticClass(), FTransform::Identity);
		UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
		Definition->TerritoryTag = Tag;
		Definition->DisplayName = FText::FromString(DisplayName);
		Definition->StableTerritoryGUID = FGuid::NewGuid();
		Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();

		// The Place reads its own tag off the actor, so the Definition has to be applied for the
		// level's actor and its Definition to be the same Place.
		if (!OutActor || !Definition->ApplyToTerritory(OutActor))
		{
			return nullptr;
		}
#if WITH_EDITOR
		// Named as the authored Place would be, so a finding that names it reads the way it does on
		// a real level. Not marked dirty: this is a throwaway world with no package on disk.
		OutActor->SetActorLabel(DisplayName, /*bMarkDirty=*/false);
#endif
		return Definition;
	}

	bool Build()
	{
		if (!World) return false;
		BlacksmithDefinition = MakePlace(BlacksmithTag(), TEXT("Blacksmith"), Blacksmith);
		WarehouseDefinition = MakePlace(WarehouseTag(), TEXT("Warehouse"), Warehouse);
		return BlacksmithDefinition && WarehouseDefinition;
	}

	/** Author a floor row, so a region's index names something real. */
	void AuthorFloor(UTerritoryPlaceDefinition* Definition, int32 FloorIndex,
		int32 DesiredGuards = 0)
	{
		if (!Definition) return;
		FTerritoryFloorTemplate Floor;
		Floor.FloorIndex = FloorIndex;
		Floor.DisplayName = FText::FromString(FString::Printf(TEXT("Floor %d"), FloorIndex));
		Floor.DesiredGuards = DesiredGuards;
		Definition->Floors.Add(Floor);
	}

	/**
	 * Author a guard-post row on a floor. The row alone is what "this floor staffs guards" means:
	 * GetFloorGuardPostCount counts rows, so no post actor is needed and none is spawned.
	 */
	void AuthorPostRow(UTerritoryPlaceDefinition* Definition, const TCHAR* PostID, int32 FloorIndex)
	{
		if (!Definition) return;
		FTerritoryGuardPostTemplate Row;
		Row.GuardPostID = FName(PostID);
		Row.FloorIndex = FloorIndex;
		Definition->GuardPosts.Add(Row);
	}

	/** One floor region. Extent defaults to the actor's own constructor default. */
	ATerritoryFloorVolume* SpawnRegion(UTerritoryPlaceDefinition* Definition, int32 FloorIndex,
		const FVector& Center, const FVector& Extent = FVector(800.f, 800.f, 200.f), float Yaw = 0.f)
	{
		if (!World) return nullptr;

		ATerritoryFloorVolume* Volume = World->SpawnActor<ATerritoryFloorVolume>(
			ATerritoryFloorVolume::StaticClass(),
			FTransform(FRotator(0.f, Yaw, 0.f), Center));
		if (!Volume) return nullptr;

		Volume->PlaceDefinition = Definition;
		Volume->FloorIndex = FloorIndex;
		// Baked here rather than left to the editor path, so a region spawned into a Game world never
		// depends on the construction-time bake, which is deliberately editor-world-only.
		Volume->FloorVolumeGUID = FGuid::NewGuid();
#if WITH_EDITOR
		// A real level's regions have author-set labels. Spawned actors get a name from a global
		// counter instead, which varies with spawn order, and two of the findings name the region -
		// so without a label here the determinism test would compare counter values rather than
		// findings. Derived from the Place's authored name and the floor the region claims, both of
		// which are properties of the level rather than of the order it was built in.
		const FString PlaceLabel = (Definition && !Definition->DisplayName.IsEmpty())
			? Definition->DisplayName.ToString() : FString(TEXT("Unbound"));
		Volume->SetActorLabel(
			FString::Printf(TEXT("Region_%s_Floor%d"), *PlaceLabel, FloorIndex),
			/*bMarkDirty=*/false);
#endif
		Volume->FloorBounds->SetBoxExtent(Extent);
		return Volume;
	}

	/**
	 * Author a guard-post row and spawn the post actor that stands over it, which the
	 * post-versus-region rule needs and AuthorPostRow deliberately does not provide: that rule is
	 * the only one here that reads where a post physically STANDS, and a row has no location.
	 *
	 * SetDefinitionBinding writes the serialized binding a loaded map already carries, which is what
	 * ResolveAuthoredPostFloor reads the row through - it never reads the post's own FloorIndex,
	 * because that is transient and its zero default is indistinguishable from an authored ground
	 * floor. OwnerTerritoryTag is how the post resolves to its Place here: the typed
	 * GuardSpawnPoints array is authoritative but protected, and the tag path is the one a streamed
	 * post uses.
	 */
	ATerritoryGuardSpawnPoint* SpawnBoundPost(UTerritoryPlaceDefinition* Definition,
		const FGameplayTag& OwnerTag, const TCHAR* PostID, int32 FloorIndex, const FVector& Location)
	{
		if (!World || !Definition) return nullptr;

		AuthorPostRow(Definition, PostID, FloorIndex);

		ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(
			ATerritoryGuardSpawnPoint::StaticClass(), FTransform(Location));
		if (!Post) return nullptr;

		Post->SetDefinitionBinding(Definition, FName(PostID));
		Post->OwnerTerritoryTag = OwnerTag;
#if WITH_EDITOR
		// Named from the authored post id rather than the spawn counter, so the finding that names
		// the post is stable across the order the level was built in.
		Post->SetActorLabel(FString::Printf(TEXT("Post_%s"), PostID), /*bMarkDirty=*/false);
#endif
		return Post;
	}
};

void ValidateFloorFixture(const FFloorFixture& Fixture, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	OutErrors.Reset();
	OutWarnings.Reset();
	if (Fixture.World)
	{
		UTerritoryDataValidator::ValidateLevel(Fixture.World->PersistentLevel, OutErrors, OutWarnings);
	}
}

bool HasMessage(const TArray<FString>& Messages, const FString& Fragment)
{
	return Messages.ContainsByPredicate([&Fragment](const FString& Message)
	{
		return Message.Contains(Fragment);
	});
}

int32 CountMessages(const TArray<FString>& Messages, const FString& Fragment)
{
	int32 Count = 0;
	for (const FString& Message : Messages)
	{
		Count += Message.Contains(Fragment) ? 1 : 0;
	}
	return Count;
}

/** "floor 1 staffs 1 guard post(s)" - floor-numbered, so a neighbouring floor cannot satisfy it. */
FString UnseparatedFloorFragment(int32 FloorIndex)
{
	return FString::Printf(TEXT("floor %d staffs"), FloorIndex);
}

const TCHAR* const UnseparatedFloorSuffix =
	TEXT("but no floor volume in this level claims it");
const TCHAR* const UndeclaredFloorFragment =
	TEXT("which authors no such floor row");
const TCHAR* const NoPlaceDefinitionFragment =
	TEXT("names no Place Definition");
const TCHAR* const OverlapFragment =
	TEXT("overlap; a point in the overlap resolves to the smaller region");

/**
 * The post-versus-region finding. Both floors are in the message for the same reason
 * UnseparatedFloorFragment numbers them: a neighbouring floor's finding must not be able to satisfy
 * an assertion about this one.
 */
const TCHAR* const FloorMismatchFragment = TEXT("but stands inside floor");

/** "guard post 'Post_UpperPost'" - one post's findings, told apart from its neighbour's. */
FString PostFragment(const TCHAR* PostID)
{
	return FString::Printf(TEXT("guard post 'Post_%s'"), PostID);
}

/**
 * Only the messages CheckFloorVolumes itself writes.
 *
 * ValidateLevel's overall ordering is not stable, and never was: several checks that run before this
 * one walk the level's actor array, so a level built in a different order shifts their messages
 * relative to each other. That is a pre-existing property of the aggregate report, not of this
 * check, and a test asserting the whole report is order-independent would be asserting something
 * untrue. What this check owns is that its own findings are a pure function of authored state, and
 * that is what the determinism test compares.
 */
bool IsFloorVolumeFinding(const FString& Message)
{
	return Message.Contains(UnseparatedFloorSuffix)
		|| Message.Contains(OverlapFragment)
		|| Message.Contains(UndeclaredFloorFragment)
		|| Message.Contains(NoPlaceDefinitionFragment)
		|| Message.Contains(FloorMismatchFragment);
}

void KeepFloorVolumeFindings(const TArray<FString>& In, TArray<FString>& Out)
{
	for (const FString& Message : In)
	{
		if (IsFloorVolumeFinding(Message)) Out.Add(Message);
	}
}
} // namespace TerritoryFloorVolumeValidatorTests

using namespace TerritoryFloorVolumeValidatorTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorFloorWithGuardsAndNoRegion,
	"TerritoryFramework.Editor.DataValidation.FloorWithGuardsAndNoRegionIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorFloorWithGuardsAndNoRegion::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	// Floor 0 is separated by a region; floor 1 staffs a guard post and is not.
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("UpperPost"), 1);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("The staffed floor with no region is reported once"),
		CountMessages(Warnings, UnseparatedFloorFragment(1)), 1);
	TestTrue(TEXT("The finding says what is missing"),
		HasMessage(Warnings, UnseparatedFloorSuffix));
	TestEqual(TEXT("The separated floor is not reported"),
		CountMessages(Warnings, UnseparatedFloorFragment(0)), 0);

	// Positive control: giving floor 1 a region is the whole fix, and it must clear the finding.
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 1000.f));
	ValidateFloorFixture(Fixture, Errors, Warnings);
	TestEqual(TEXT("Authoring the missing region clears the finding"),
		CountMessages(Warnings, UnseparatedFloorFragment(1)), 0);
	TestEqual(TEXT("No unseparated-floor finding remains at all"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorPostlessFloorStaysSilent,
	"TerritoryFramework.Editor.DataValidation.PostlessFloorIsNotReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorPostlessFloorStaysSilent::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	// Floor 2 is a story-only row: it drives a cleared event and holds no guard post, so no guard
	// can stand on it and it cannot be separated from anything.
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 2, /*DesiredGuards=*/3);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("UpperPost"), 1);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("The post-less floor is silent even though it asks for defenders"),
		CountMessages(Warnings, UnseparatedFloorFragment(2)), 0);
	TestEqual(TEXT("The staffed floor with no region is still reported"),
		CountMessages(Warnings, UnseparatedFloorFragment(1)), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorRegionNamingUndeclaredFloor,
	"TerritoryFramework.Editor.DataValidation.FloorVolumeNamingAnUndeclaredFloorIsAnError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorRegionNamingUndeclaredFloor::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);

	ATerritoryFloorVolume* Region =
		Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 7, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Region exists"), Region)) return false;

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("A region claiming a floor row that does not exist is an error"),
		CountMessages(Errors, UndeclaredFloorFragment), 1);
	TestEqual(TEXT("The dead region does not count as separating floor 0"),
		CountMessages(Warnings, UnseparatedFloorFragment(0)), 1);

	// Positive control: naming the declared row turns the dead region into a real one.
	Region->FloorIndex = 0;
	ValidateFloorFixture(Fixture, Errors, Warnings);
	TestEqual(TEXT("Naming a declared floor clears the error"),
		CountMessages(Errors, UndeclaredFloorFragment), 0);
	TestEqual(TEXT("And the floor is then separated"),
		CountMessages(Warnings, UnseparatedFloorFragment(0)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorRegionWithoutPlace,
	"TerritoryFramework.Editor.DataValidation.FloorVolumeWithoutAPlaceDefinitionIsAnError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorRegionWithoutPlace::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);

	ATerritoryFloorVolume* Region = Fixture.SpawnRegion(nullptr, 0, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Region exists"), Region)) return false;

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("A region bound to no Place is an error"),
		CountMessages(Errors, NoPlaceDefinitionFragment), 1);

	// Positive control: binding it is the fix.
	Region->PlaceDefinition = Fixture.BlacksmithDefinition;
	ValidateFloorFixture(Fixture, Errors, Warnings);
	TestEqual(TEXT("Binding a Place clears the error"),
		CountMessages(Errors, NoPlaceDefinitionFragment), 0);
	TestEqual(TEXT("And the region now separates its floor"),
		CountMessages(Warnings, UnseparatedFloorFragment(0)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorDifferentFloorOverlap,
	"TerritoryFramework.Editor.DataValidation.FloorVolumesOnDifferentFloorsOverOneSpaceAreReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorDifferentFloorOverlap::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("UpperPost"), 1);

	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 40.f));

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("Two floors claiming the same space are reported once"),
		CountMessages(Warnings, OverlapFragment), 1);
	TestEqual(TEXT("Both floors count as separated, so the overlap is the only finding"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 0);

	// Positive control: the finding is about the overlap, not about having two regions.
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector(9000.f, 0.f, 0.f));
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(9000.f, 0.f, 1000.f));
	ValidateFloorFixture(Fixture, Errors, Warnings);
	TestEqual(TEXT("The disjoint pair is not reported, so the finding is still the one overlap"),
		CountMessages(Warnings, OverlapFragment), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorSameFloorOverlapSilent,
	"TerritoryFramework.Editor.DataValidation.TwoRegionsOfOneFloorAreNotReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorSameFloorOverlapSilent::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("UpperPost"), 1);

	// Two regions of floor 0 over the same space. Both resolve to 0, so no point in the overlap
	// has two different answers and the resolver has nothing to choose between. That is redundant
	// authoring, not ambiguity, and warning about it would be noise.
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector(100.f, 100.f, 0.f));

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("Two regions of the same floor are not reported as ambiguous"),
		CountMessages(Warnings, OverlapFragment), 0);
	TestEqual(TEXT("The unseparated upper floor is still reported"),
		CountMessages(Warnings, UnseparatedFloorFragment(1)), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorRotatedNeighbourSilent,
	"TerritoryFramework.Editor.DataValidation.RotatedNeighbourSharingOnlyABoundCornerIsNotReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorRotatedNeighbourSilent::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("UpperPost"), 1);

	// Floor 0 is axis-aligned and occupies [-400,400]^2. Floor 1 is the same box yawed 45 degrees
	// at (900,900), so its extent-driven bound reaches back to 334.3 on each axis and the two
	// axis-aligned bounds DO intersect - while its nearest vertex to floor 0 sits on x+y=1234.3,
	// against floor 0's farthest corner at x+y=800. The bounds overlap; the regions do not.
	const FVector Extent(400.f, 400.f, 100.f);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector, Extent);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(900.f, 900.f, 0.f), Extent,
		/*Yaw=*/45.f);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("A rotated neighbour whose bound alone overlaps is not reported"),
		CountMessages(Warnings, OverlapFragment), 0);
	TestEqual(TEXT("Neither floor is reported as unseparated"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorSeparatedFloorsSilent,
	"TerritoryFramework.Editor.DataValidation.CorrectlySeparatedFloorsAreNotReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorSeparatedFloorsSilent::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	// The authoring the feature wants: a row, a post and a region per floor, stacked and disjoint.
	// This is the positive control for the check as a whole - if it ever grows a rule that fires on
	// correct authoring, this test is what fails.
	for (int32 FloorIndex = 0; FloorIndex < 3; ++FloorIndex)
	{
		Fixture.AuthorFloor(Fixture.BlacksmithDefinition, FloorIndex);
		Fixture.AuthorPostRow(Fixture.BlacksmithDefinition,
			*FString::Printf(TEXT("Post_%d"), FloorIndex), FloorIndex);
		Fixture.SpawnRegion(Fixture.BlacksmithDefinition, FloorIndex,
			FVector(0.f, 0.f, FloorIndex * 500.f));
	}

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("No unseparated floor is reported"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 0);
	TestEqual(TEXT("No overlap is reported"), CountMessages(Warnings, OverlapFragment), 0);
	TestEqual(TEXT("No region error is reported"), CountMessages(Errors, NoPlaceDefinitionFragment), 0);
	TestEqual(TEXT("No undeclared-floor error is reported"),
		CountMessages(Errors, UndeclaredFloorFragment), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorRegionScopedToItsPlace,
	"TerritoryFramework.Editor.DataValidation.ARegionDoesNotSeparateAPlaceItIsNotBoundTo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorRegionScopedToItsPlace::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	// The Blacksmith staffs floor 0 and has no region at all. The Warehouse has a floor 0 region
	// standing in the Blacksmith's space. A region answers for the Place it is bound to, so exactly
	// one unseparated-floor finding survives - and it is the Blacksmith's, even though a region of
	// the right floor index covers that spot. If regions were keyed by location instead of by the
	// Place they name, this count would be zero.
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("BlacksmithPost"), 0);
	Fixture.AuthorFloor(Fixture.WarehouseDefinition, 0);
	Fixture.AuthorPostRow(Fixture.WarehouseDefinition, TEXT("WarehousePost"), 0);
	Fixture.SpawnRegion(Fixture.WarehouseDefinition, 0, FVector::ZeroVector);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("Only the Place with no region of its own is reported"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 1);
	TestEqual(TEXT("And it is reported on floor 0"),
		CountMessages(Warnings, UnseparatedFloorFragment(0)), 1);

	// Positive control: the same region, bound to the Blacksmith, clears it - so the finding above
	// is about which Place the region names, not about the region or its position.
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
	ValidateFloorFixture(Fixture, Errors, Warnings);
	TestEqual(TEXT("Binding an equivalent region to this Place clears the finding"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorFloorFindingsAreDeterministic,
	"TerritoryFramework.Editor.DataValidation.FloorFindingsDoNotDependOnSpawnOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorFloorFindingsAreDeterministic::RunTest(const FString& Parameters)
{
	// The check groups regions in a TMap, whose iteration order is not stable, and its output is a
	// release gate. Two levels holding the same authored state but created in opposite orders must
	// therefore produce the same findings in the same order. Only this check's own messages are
	// compared - see IsFloorVolumeFinding for why the aggregate report is not order-stable.
	const auto BuildInOrder = [](bool bForward, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
	{
		FFloorFixture Fixture;
		if (!Fixture.Build()) return false;

		Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
		Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
		// Floor 2 staffs a post and gets no region, so an unseparated-floor finding is present too
		// and both kinds of finding have to come out in a stable order.
		Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 2);
		Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("GroundPost"), 0);
		Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("UpperPost"), 1);
		Fixture.AuthorPostRow(Fixture.BlacksmithDefinition, TEXT("RoofPost"), 2);
		Fixture.AuthorFloor(Fixture.WarehouseDefinition, 0);
		Fixture.AuthorPostRow(Fixture.WarehouseDefinition, TEXT("WarehousePost"), 0);

		// The Warehouse's region is the one that separates its floor.
		const FVector WarehouseRegion(4000.f, 0.f, 0.f);
		if (bForward)
		{
			Fixture.SpawnRegion(Fixture.WarehouseDefinition, 0, WarehouseRegion);
			Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
			Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 40.f));
		}
		else
		{
			Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 40.f));
			Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
			Fixture.SpawnRegion(Fixture.WarehouseDefinition, 0, WarehouseRegion);
		}

		TArray<FString> AllErrors;
		TArray<FString> AllWarnings;
		ValidateFloorFixture(Fixture, AllErrors, AllWarnings);
		KeepFloorVolumeFindings(AllErrors, OutErrors);
		KeepFloorVolumeFindings(AllWarnings, OutWarnings);
		return true;
	};

	TArray<FString> ForwardErrors;
	TArray<FString> ForwardWarnings;
	TArray<FString> ReverseErrors;
	TArray<FString> ReverseWarnings;
	if (!TestTrue(TEXT("Forward fixture is built"), BuildInOrder(true, ForwardErrors, ForwardWarnings)))
		return false;
	if (!TestTrue(TEXT("Reverse fixture is built"), BuildInOrder(false, ReverseErrors, ReverseWarnings)))
		return false;

	TestTrue(TEXT("The forward fixture produced an overlap finding"),
		HasMessage(ForwardWarnings, OverlapFragment));
	TestTrue(TEXT("The forward fixture produced an unseparated-floor finding"),
		HasMessage(ForwardWarnings, UnseparatedFloorSuffix));
	TestEqual(TEXT("Warnings are identical regardless of spawn order"),
		FString::Join(ForwardWarnings, TEXT("\n")), FString::Join(ReverseWarnings, TEXT("\n")));
	TestEqual(TEXT("Errors are identical regardless of spawn order"),
		FString::Join(ForwardErrors, TEXT("\n")), FString::Join(ReverseErrors, TEXT("\n")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorPostStandingOnAnotherFloor,
	"TerritoryFramework.Editor.DataValidation.PostStandingOnAnotherFloorIsReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorPostStandingOnAnotherFloor::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	// Two floors, each with a region and each with a post. The regions do not overlap in Z, so
	// nothing here can be satisfied by the overlap rule instead.
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 1000.f));

	Fixture.SpawnBoundPost(Fixture.BlacksmithDefinition, BlacksmithTag(),
		TEXT("GroundPost"), 0, FVector::ZeroVector);

	// Authored on floor 1, standing inside floor 0's region. The resolver reads position before the
	// row, so this guard is treated as floor 0's defender whatever its row says.
	ATerritoryGuardSpawnPoint* const Mismatched = Fixture.SpawnBoundPost(
		Fixture.BlacksmithDefinition, BlacksmithTag(), TEXT("UpperPost"), 1, FVector(0.f, 0.f, 100.f));
	if (!TestNotNull(TEXT("The mismatched post exists"), Mismatched)) return false;

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("The disagreement between a post's row and its position is reported once"),
		CountMessages(Warnings, FloorMismatchFragment), 1);
	TestTrue(TEXT("The finding names the post"), HasMessage(Warnings, PostFragment(TEXT("UpperPost"))));
	TestTrue(TEXT("The finding names both floors, not just one"),
		HasMessage(Warnings, TEXT("authored on floor 1 but stands inside floor 0's floor volume")));
	TestEqual(TEXT("The post whose row and position agree is not reported"),
		CountMessages(Warnings, PostFragment(TEXT("GroundPost"))), 0);
	TestEqual(TEXT("Both floors have regions, so neither is unseparated"),
		CountMessages(Warnings, UnseparatedFloorSuffix), 0);
	TestEqual(TEXT("This rule warns rather than errors"), Errors.Num(), 0);

	// Positive control: moving the post into the region its own row names is the whole fix, so the
	// finding must clear. Without this leg the first assertion could hold for the wrong reason.
	Mismatched->SetActorLocation(FVector(0.f, 0.f, 1000.f));
	ValidateFloorFixture(Fixture, Errors, Warnings);
	TestEqual(TEXT("Placing the post where its row says it is clears the finding"),
		CountMessages(Warnings, FloorMismatchFragment), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorPostOutsideEveryRegionStaysSilent,
	"TerritoryFramework.Editor.DataValidation.PostOutsideEveryRegionIsNotReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorPostOutsideEveryRegionStaysSilent::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 1000.f));

	// One genuine mismatch, so a silent fixture cannot pass this test by producing nothing at all.
	Fixture.SpawnBoundPost(Fixture.BlacksmithDefinition, BlacksmithTag(),
		TEXT("MismatchedPost"), 1, FVector(0.f, 0.f, 100.f));

	// Well above every region. This is not a defect: with no region containing the post the
	// engagement gate falls back to the post's own row, so the defender is separated exactly as
	// authored. It is also the state every guard that has patrolled off its region is in, which is
	// why the fallback exists - warning here would flag correct authoring.
	Fixture.SpawnBoundPost(Fixture.BlacksmithDefinition, BlacksmithTag(),
		TEXT("OffRegionPost"), 1, FVector(0.f, 0.f, 5000.f));

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("Only the real disagreement is reported"),
		CountMessages(Warnings, FloorMismatchFragment), 1);
	TestTrue(TEXT("And it is the mismatched post that is named"),
		HasMessage(Warnings, PostFragment(TEXT("MismatchedPost"))));
	TestEqual(TEXT("A post outside every region is left alone, because its row still answers"),
		CountMessages(Warnings, PostFragment(TEXT("OffRegionPost"))), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFValidatorMismatchDoesNotDoubleReportUndeclaredFloor,
	"TerritoryFramework.Editor.DataValidation.PostMismatchDoesNotDoubleReportAnUndeclaredFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFValidatorMismatchDoesNotDoubleReportUndeclaredFloor::RunTest(const FString& Parameters)
{
	FFloorFixture Fixture;
	if (!TestTrue(TEXT("Fixture level with two Places is built"), Fixture.Build())) return false;

	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 0);
	Fixture.AuthorFloor(Fixture.BlacksmithDefinition, 1);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 0, FVector::ZeroVector);
	Fixture.SpawnRegion(Fixture.BlacksmithDefinition, 1, FVector(0.f, 0.f, 1000.f));

	Fixture.SpawnBoundPost(Fixture.BlacksmithDefinition, BlacksmithTag(),
		TEXT("DeclaredPost"), 1, FVector(0.f, 0.f, 100.f));

	// Its row names floor 7, which this Place does not declare. That is already an error in the
	// Definition's own IsDataValid, so this check must skip it rather than report the same mistake
	// from a second place - and the post above keeps a silent fixture from passing this test.
	Fixture.SpawnBoundPost(Fixture.BlacksmithDefinition, BlacksmithTag(),
		TEXT("StrayPost"), 7, FVector::ZeroVector);

	TArray<FString> Errors;
	TArray<FString> Warnings;
	ValidateFloorFixture(Fixture, Errors, Warnings);

	TestEqual(TEXT("Only the declared-floor disagreement is reported"),
		CountMessages(Warnings, FloorMismatchFragment), 1);
	TestTrue(TEXT("And it is the declared post that is named"),
		HasMessage(Warnings, PostFragment(TEXT("DeclaredPost"))));
	TestEqual(TEXT("The undeclared-floor post is left to IsDataValid, not reported twice"),
		CountMessages(Warnings, PostFragment(TEXT("StrayPost"))), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
