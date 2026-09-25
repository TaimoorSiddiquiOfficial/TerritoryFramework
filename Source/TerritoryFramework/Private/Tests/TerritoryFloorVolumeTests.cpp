#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryFloorVolume.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

/**
 * Floor membership is geometry, and geometry is the one thing a floor row never had.
 *
 * FTerritoryFloorTemplate carries FloorIndex, DisplayName, DesiredGuards and FloorClearedEvents -
 * no volume, bounds, anchor or transform. So a floor row is an integer grouping key, and before
 * ATerritoryFloorVolume existed nothing could answer "is this actor on that floor". These tests hold
 * the resolution contract that the engagement gate depends on:
 *
 *   - INDEX_NONE means "no separation is declared here", never an error. A Place with floor rows and
 *     no authored volume must behave exactly as it did before this feature existed, which is why the
 *     inert case is the first thing tested rather than an afterthought.
 *   - The answer is a pure function of authored geometry plus a total tie-break order. It must not
 *     depend on registration order, because World Partition stream-in order is not stable.
 *   - A volume never answers for a Place it is not bound to, and never for a location it does not
 *     contain - including a location directly above it, which is the whole point of a floor.
 */
namespace TerritoryFloorVolumeTest
{
	FGameplayTag PlaceTag()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	}

	FGameplayTag SiblingPlaceTag()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Territory.HavenReach.MarketSquare.Warehouse"), false);
	}

	struct FVolumeFixture
	{
		UWorld* World = nullptr;
		ATerritoryProperty* Place = nullptr;
		UTerritoryPlaceDefinition* Definition = nullptr;
		UTerritoryRegistrySubsystem* Registry = nullptr;

		bool IsValid() const { return World && Place && Definition && Registry; }

		void TearDown()
		{
			if (World) World->DestroyWorld(false);
			World = nullptr;
			Place = nullptr;
			Definition = nullptr;
			Registry = nullptr;
		}

		/** Author a floor row so a volume's index names something real. */
		void AuthorFloor(int32 FloorIndex, int32 DesiredGuards = 0)
		{
			FTerritoryFloorTemplate Floor;
			Floor.FloorIndex = FloorIndex;
			Floor.DisplayName = FText::FromString(
				FString::Printf(TEXT("Floor %d"), FloorIndex));
			Floor.DesiredGuards = DesiredGuards;
			Definition->Floors.Add(Floor);
		}

		/** Create, bind and register one floor region. Returns the actor regardless of admission. */
		ATerritoryFloorVolume* AddVolume(int32 FloorIndex, const FVector& Center,
			const FVector& Extent, const FGuid& GUID)
		{
			if (!IsValid()) return nullptr;
			ATerritoryFloorVolume* Volume =
				NewObject<ATerritoryFloorVolume>(World->PersistentLevel);
			if (!Volume) return nullptr;
			Volume->PlaceDefinition = Definition;
			Volume->FloorIndex = FloorIndex;
			Volume->FloorVolumeGUID = GUID;
			Volume->SetActorLocation(Center);
			Volume->FloorBounds->SetBoxExtent(Extent);
			Volume->ApplyFloorVolumeDefinition();
			Registry->RegisterFloorVolume(Volume);
			return Volume;
		}
	};

	bool BuildFixture(FVolumeFixture& Out, FAutomationTestBase& Test)
	{
		Out.World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Test.TestNotNull(TEXT("Floor volume world exists"), Out.World)) return false;

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		Out.Place = Out.World->SpawnActor<ATerritoryProperty>(
			ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Test.TestNotNull(TEXT("Floor volume Place exists"), Out.Place)) return false;

		Out.Definition = NewObject<UTerritoryPlaceDefinition>();
		Out.Definition->TerritoryTag = PlaceTag();
		Out.Definition->DisplayName = FText::FromString(TEXT("Blacksmith"));
		Out.Definition->StableTerritoryGUID = FGuid::NewGuid();
		Out.Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		if (!Test.TestTrue(TEXT("Place tag exists"), PlaceTag().IsValid())) return false;
		if (!Test.TestTrue(TEXT("Sibling place tag exists"), SiblingPlaceTag().IsValid())) return false;

		// The Place reads its tag straight off the actor, so the definition must be applied.
		if (!Test.TestTrue(TEXT("Place definition applied"),
			Out.Definition->ApplyToTerritory(Out.Place))) return false;

		Out.Registry = Out.World->GetSubsystem<UTerritoryRegistrySubsystem>();
		return Test.TestNotNull(TEXT("Floor volume registry exists"), Out.Registry);
	}

	/** Two GUIDs whose ToString() order is known, so the tie-break expectation is unambiguous. */
	const FGuid LowerGUID(0x00000000, 0x00000000, 0x00000000, 0x000000AA);
	const FGuid HigherGUID(0x00000000, 0x00000000, 0x00000000, 0x000000BB);
}

/**
 * A Place that authors floors but binds no volume must resolve to INDEX_NONE everywhere.
 * This is the inert default the whole phase ships behind: if it were anything else, adding
 * this feature would have changed gameplay for every existing level.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeInertByDefault,
	"TerritoryFramework.Floors.ResolveInertWithoutAuthoredVolumes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeInertByDefault::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	FVolumeFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	Fixture.AuthorFloor(0);
	Fixture.AuthorFloor(2);

	TestEqual(TEXT("A Place with floor rows and no volume resolves nothing at its own centre"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), INDEX_NONE);
	TestEqual(TEXT("...and nothing well above it"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(0, 0, 900)), INDEX_NONE);
	TestFalse(TEXT("No authored floor volumes are reported"),
		Fixture.Registry->HasAuthoredFloorVolumes(Fixture.Place));

	Fixture.TearDown();
	return true;
}

/**
 * The core claim of the feature: a location inside a floor's region resolves to that floor, and a
 * location outside it - including one directly above it - does not. The vertical case is the whole
 * point, because ATerritoryVolume::ContainsPoint spans every floor and cannot answer it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeResolvesInsideNotAbove,
	"TerritoryFramework.Floors.ResolveInsideVolumeButNotAboveIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeResolvesInsideNotAbove::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	FVolumeFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	Fixture.AuthorFloor(0);
	Fixture.AuthorFloor(2);
	Fixture.AddVolume(0, FVector::ZeroVector, FVector(1000, 1000, 200),
		LowerGUID);

	TestEqual(TEXT("The volume's own centre is floor 0"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), 0);
	TestEqual(TEXT("A point inside the authored box is floor 0"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(900, -900, 150)), 0);
	TestEqual(TEXT("A point above the box is unresolved, so floor 0 does not own the floor above it"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(0, 0, 400)), INDEX_NONE);
	TestEqual(TEXT("A point below the box is unresolved"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(0, 0, -400)), INDEX_NONE);
	TestEqual(TEXT("A point outside in XY is unresolved"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(2000, 0, 0)), INDEX_NONE);

	Fixture.TearDown();
	return true;
}

/**
 * Registration order must not decide the answer. Two overlapping regions claiming different floors
 * both contain the point; the smaller region is the more specific claim and wins either way round.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeOverlapIsOrderIndependent,
	"TerritoryFramework.Floors.ResolveOverlappingVolumesIndependentlyOfRegistrationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeOverlapIsOrderIndependent::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;

	const FVector Center = FVector::ZeroVector;
	const FVector BroadExtent(1000, 1000, 200);
	const FVector SmallExtent(300, 300, 200);

	// Smaller region registered first.
	int32 BroadFirstAnswer = INDEX_NONE;
	{
		FVolumeFixture Fixture;
		if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
		Fixture.AuthorFloor(0);
		Fixture.AuthorFloor(2);
		Fixture.AddVolume(0, Center, SmallExtent, LowerGUID);
		Fixture.AddVolume(2, Center, BroadExtent, HigherGUID);
		BroadFirstAnswer = Fixture.Registry->GetFloorAtLocation(Fixture.Place, Center);
		Fixture.TearDown();
	}

	// Same two regions, registration order reversed.
	int32 BroadLastAnswer = INDEX_NONE;
	{
		FVolumeFixture Fixture;
		if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
		Fixture.AuthorFloor(0);
		Fixture.AuthorFloor(2);
		Fixture.AddVolume(2, Center, BroadExtent, HigherGUID);
		Fixture.AddVolume(0, Center, SmallExtent, LowerGUID);
		BroadLastAnswer = Fixture.Registry->GetFloorAtLocation(Fixture.Place, Center);
		Fixture.TearDown();
	}

	TestEqual(TEXT("The smaller, more specific region wins"), BroadFirstAnswer, 0);
	TestEqual(TEXT("The answer does not depend on which region registered first"),
		BroadLastAnswer, BroadFirstAnswer);
	return true;
}

/**
 * When two regions of identical size both contain the point, the answer must still be total.
 * The volume GUID breaks the tie, so the result is the same whichever order they arrive in.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeExactTieIsTotal,
	"TerritoryFramework.Floors.ResolveIdenticalOverlapByStableIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeExactTieIsTotal::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	const FVector Center = FVector::ZeroVector;
	const FVector Extent(500, 500, 200);

	int32 LowerFirstAnswer = INDEX_NONE;
	int32 LowerLastAnswer = INDEX_NONE;

	{
		FVolumeFixture Fixture;
		if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
		Fixture.AuthorFloor(0);
		Fixture.AuthorFloor(2);
		Fixture.AddVolume(0, Center, Extent, LowerGUID);
		Fixture.AddVolume(2, Center, Extent, HigherGUID);
		LowerFirstAnswer = Fixture.Registry->GetFloorAtLocation(Fixture.Place, Center);
		Fixture.TearDown();
	}
	{
		FVolumeFixture Fixture;
		if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }
		Fixture.AuthorFloor(0);
		Fixture.AuthorFloor(2);
		Fixture.AddVolume(2, Center, Extent, HigherGUID);
		Fixture.AddVolume(0, Center, Extent, LowerGUID);
		LowerLastAnswer = Fixture.Registry->GetFloorAtLocation(Fixture.Place, Center);
		Fixture.TearDown();
	}

	TestTrue(TEXT("The two tie-break GUIDs order the way the test assumes"),
		LowerGUID.ToString()
			< HigherGUID.ToString());
	TestEqual(TEXT("The lower GUID wins an exact tie"), LowerFirstAnswer, 0);
	TestEqual(TEXT("...and wins it in either registration order"),
		LowerLastAnswer, LowerFirstAnswer);
	return true;
}

/**
 * A floor region answers only for the Place it is bound to. Without this, a volume covering the
 * same space would silently govern a neighbouring Place's guards - and every aggregate volume in
 * the level would be a candidate answer.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeDoesNotAnswerForOtherPlaces,
	"TerritoryFramework.Floors.ResolveIsScopedToTheBoundPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeDoesNotAnswerForOtherPlaces::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	FVolumeFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	Fixture.AuthorFloor(0);
	Fixture.AddVolume(0, FVector::ZeroVector, FVector(1000, 1000, 200),
		LowerGUID);

	// A sibling Place occupying the same space, with its own identity and no volumes at all.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Sibling = Fixture.World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Sibling Place exists"), Sibling))
	{
		Fixture.TearDown();
		return false;
	}
	UTerritoryPlaceDefinition* SiblingDefinition = NewObject<UTerritoryPlaceDefinition>();
	SiblingDefinition->TerritoryTag = SiblingPlaceTag();
	SiblingDefinition->StableTerritoryGUID = FGuid::NewGuid();
	SiblingDefinition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	if (!TestTrue(TEXT("Sibling definition applied"),
		SiblingDefinition->ApplyToTerritory(Sibling)))
	{
		Fixture.TearDown();
		return false;
	}

	TestEqual(TEXT("The bound Place resolves its own floor"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), 0);
	TestEqual(TEXT("A Place with no authored volume resolves nothing, even in the same space"),
		Fixture.Registry->GetFloorAtLocation(Sibling, FVector::ZeroVector), INDEX_NONE);
	TestEqual(TEXT("The volume is not reported for the sibling Place"),
		Fixture.Registry->GetFloorVolumesForPlace(Sibling).Num(), 0);

	Fixture.TearDown();
	return true;
}

/**
 * Containment honours rotation. A rotated region is not the same region as an axis-aligned box of
 * the same extent, and a floor author rotating a stairwell volume expects it to follow.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeRespectsRotation,
	"TerritoryFramework.Floors.ResolveHonoursRotatedRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeRespectsRotation::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	FVolumeFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	Fixture.AuthorFloor(0);
	// A long thin corridor along X.
	ATerritoryFloorVolume* Volume = Fixture.AddVolume(0, FVector::ZeroVector,
		FVector(1000, 100, 200), LowerGUID);
	if (!TestNotNull(TEXT("Corridor volume exists"), Volume))
	{
		Fixture.TearDown();
		return false;
	}

	// A point out along Y at 900 is outside the unrotated corridor...
	TestEqual(TEXT("A point off the corridor's long axis is outside it"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(0, 900, 0)), INDEX_NONE);

	// ...and inside it once the corridor is rotated a quarter turn about Z.
	Volume->SetActorRotation(FRotator(0, 90, 0));
	TestEqual(TEXT("The same point is inside the rotated corridor"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector(0, 900, 0)), 0);

	Fixture.TearDown();
	return true;
}

/**
 * Admission rejects the two states that would make resolution ambiguous or meaningless, and the
 * rejection must actually remove the volume from the index rather than merely log.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeAdmissionRejectsAmbiguity,
	"TerritoryFramework.Floors.RegistrationRejectsAmbiguousOrUnusableVolumes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeAdmissionRejectsAmbiguity::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	FVolumeFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	// Rejection is loud by design: each refused volume logs an Error naming the exact reason. The
	// automation framework counts an unexpected Error as a failure, so declaring them here both
	// suppresses that and asserts the diagnostic actually fires rather than failing silently.
	AddExpectedError(TEXT("floor index -1 is negative"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("is already claimed by"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("it claims no Place tag"), EAutomationExpectedErrorFlags::Contains, 1);

	Fixture.AuthorFloor(0);
	const FVector Center = FVector::ZeroVector;
	const FVector Extent(500, 500, 200);

	// A negative index would be indistinguishable from "unresolved".
	ATerritoryFloorVolume* Negative = Fixture.AddVolume(-1, Center, Extent,
		LowerGUID);
	TestEqual(TEXT("A negative floor index is not admitted"),
		Fixture.Registry->GetFloorVolumesForPlace(Fixture.Place).Num(), 0);
	TestEqual(TEXT("...and it cannot answer, so the location stays unresolved"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, Center), INDEX_NONE);

	// A duplicated stable identity would make the tie-break non-total.
	ATerritoryFloorVolume* First = Fixture.AddVolume(0, Center, Extent,
		LowerGUID);
	ATerritoryFloorVolume* Duplicate = Fixture.AddVolume(0, Center, Extent,
		LowerGUID);
	TestNotNull(TEXT("Both duplicate-identity volumes were constructed"), Duplicate);
	TestEqual(TEXT("Only one of the two duplicate-identity volumes is admitted"),
		Fixture.Registry->GetFloorVolumesForPlace(Fixture.Place).Num(), 1);
	TestTrue(TEXT("The admitted volume is the first one"), First != nullptr);

	// An unbound volume has no Place to answer for at all.
	ATerritoryFloorVolume* Unbound =
		NewObject<ATerritoryFloorVolume>(Fixture.World->PersistentLevel);
	Unbound->FloorVolumeGUID = FGuid::NewGuid();
	Unbound->SetActorLocation(FVector(5000, 5000, 0));
	TestFalse(TEXT("A volume with no Place is not admitted"),
		Fixture.Registry->RegisterFloorVolume(Unbound));
	TestEqual(TEXT("...and it changes nothing"),
		Fixture.Registry->GetFloorVolumesForPlace(Fixture.Place).Num(), 1);
	TestNotNull(TEXT("The unused actors are still alive for the assertion above"), Negative);

	Fixture.TearDown();
	return true;
}

/**
 * World Partition streams actors out. A streamed-out region must resolve to INDEX_NONE rather than
 * leaving a readable stale entry behind, and re-streaming it in must restore the same answer.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeStreamOutAndBack,
	"TerritoryFramework.Floors.StreamOutResolvesUnresolvedAndStreamsBackClean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeStreamOutAndBack::RunTest(const FString& Parameters)
{
	using namespace TerritoryFloorVolumeTest;
	FVolumeFixture Fixture;
	if (!BuildFixture(Fixture, *this)) { Fixture.TearDown(); return false; }

	Fixture.AuthorFloor(0);
	ATerritoryFloorVolume* Volume = Fixture.AddVolume(0, FVector::ZeroVector,
		FVector(500, 500, 200), LowerGUID);
	TestEqual(TEXT("The region resolves before stream-out"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), 0);

	// Stream out: this is the call EndPlay makes.
	Fixture.Registry->UnregisterFloorVolume(Volume);
	TestEqual(TEXT("A streamed-out region resolves to unresolved"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), INDEX_NONE);
	TestFalse(TEXT("...and no longer counts as an authored region"),
		Fixture.Registry->HasAuthoredFloorVolumes(Fixture.Place));

	// Stream back in. The identity is editor-baked, so it survives the round trip unchanged.
	TestTrue(TEXT("The same region is re-admitted"),
		Fixture.Registry->RegisterFloorVolume(Volume));
	TestEqual(TEXT("The streamed-back region resolves to the same floor"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), 0);

	// A destroyed actor must not be readable through the index.
	Fixture.Registry->UnregisterFloorVolume(Volume);
	Volume->MarkAsGarbage();
	TestEqual(TEXT("A garbage region cannot answer"),
		Fixture.Registry->GetFloorAtLocation(Fixture.Place, FVector::ZeroVector), INDEX_NONE);

	Fixture.TearDown();
	return true;
}

/**
 * The resolution queries are read models: side-effect free, callable from a non-authoritative world,
 * and exposed to Blueprint as pure nodes. AGENTS.md requires a Blueprint contract test for any
 * exposed API, and requires BlueprintPure specifically for queries with no side effects.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryFloorVolumeBlueprintContract,
	"TerritoryFramework.Floors.VolumeResolutionBlueprintContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryFloorVolumeBlueprintContract::RunTest(const FString& Parameters)
{
	const UClass* RegistryClass = UTerritoryRegistrySubsystem::StaticClass();
	const auto IsBlueprintPure = [RegistryClass](const TCHAR* FunctionName)
	{
		const UFunction* Function = RegistryClass->FindFunctionByName(FName(FunctionName));
		return Function && Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	};

	TestTrue(TEXT("Get Floor At Location is a pure query"),
		IsBlueprintPure(TEXT("GetFloorAtLocation")));
	TestTrue(TEXT("Get Floor Volumes For Place is a pure query"),
		IsBlueprintPure(TEXT("GetFloorVolumesForPlace")));
	TestTrue(TEXT("Has Authored Floor Volumes is a pure query"),
		IsBlueprintPure(TEXT("HasAuthoredFloorVolumes")));

	const UClass* VolumeClass = ATerritoryFloorVolume::StaticClass();
	TestNotNull(TEXT("Contains Point is exposed to Blueprint"),
		VolumeClass->FindFunctionByName(FName(TEXT("ContainsPoint"))));

	const FProperty* PlaceProp = VolumeClass->FindPropertyByName(FName(TEXT("PlaceDefinition")));
	const FProperty* FloorProp = VolumeClass->FindPropertyByName(FName(TEXT("FloorIndex")));
	TestTrue(TEXT("A floor volume's Place is authorable"),
		PlaceProp && PlaceProp->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(TEXT("A floor volume's floor index is authorable"),
		FloorProp && FloorProp->HasAnyPropertyFlags(CPF_Edit));

	// The binding type is the Place subclass, so a City or District cannot be bound here at all.
	TestTrue(TEXT("The Place binding is typed to the Place definition"),
		PlaceProp && CastField<FObjectProperty>(PlaceProp)
			&& CastField<FObjectProperty>(PlaceProp)->PropertyClass
				== UTerritoryPlaceDefinition::StaticClass());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
