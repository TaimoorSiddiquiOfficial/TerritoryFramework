#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryFloorVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Combat/TerritoryFloorCombatPolicy.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "TerritoryDefinitionEditorLibrary.h"
#include "UObject/StrongObjectPtr.h"

namespace TerritoryFloorAuthoringTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		UTerritoryPlaceDefinition* Definition = nullptr;
		ATerritoryProperty* Place = nullptr;
		ATerritoryGuardSpawnPoint* Post = nullptr;
		ATerritoryFloorVolume* Region = nullptr;

		FFixture()
		{
			if (!World) return;
			Definition = NewObject<UTerritoryPlaceDefinition>(GetTransientPackage(), NAME_None, RF_Transactional);
			Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
			Definition->StableTerritoryGUID = FGuid::NewGuid();
			Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
			Place = World->SpawnActor<ATerritoryProperty>();
			Definition->ApplyToTerritory(Place);
			FTerritoryFloorTemplate& Ground = Definition->Floors.AddDefaulted_GetRef();
			Ground.DisplayName = FText::FromString(TEXT("Keep my authored ground name"));
			Ground.CombatPolicy = NewObject<UTerritoryFloorCombatPolicy>();
			FTerritoryGuardPostTemplate& Row = Definition->GuardPosts.AddDefaulted_GetRef();
			Row.GuardPostID = TEXT("UpperPost");
			Row.StableGuardPostGUID = FGuid::NewGuid();
			Post = World->SpawnActor<ATerritoryGuardSpawnPoint>(ATerritoryGuardSpawnPoint::StaticClass(), FTransform(FVector(0, 0, 120)));
			Post->SetDefinitionBinding(Definition, Row.GuardPostID);
			Post->ApplyTerritoryDefinition();
			Region = World->SpawnActor<ATerritoryFloorVolume>(ATerritoryFloorVolume::StaticClass(), FTransform(FVector(0, 0, 120)));
			Region->FloorBounds->SetBoxExtent(FVector(100, 100, 50));
			Region->FloorIndex = 1;
		}
		~FFixture() { if (World) World->DestroyWorld(false); }
		bool IsReady() const { return Definition && Place && Post && Region; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFloorDetectionCanonicalRows,
	"TerritoryFramework.Editor.Authoring.FloorDetection.CanonicalRowsAndPreservedSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFloorDetectionCanonicalRows::RunTest(const FString& Parameters)
{
	TerritoryFloorAuthoringTests::FFixture F;
	if (!TestTrue(TEXT("Floor authoring fixture"), F.IsReady())) return false;
	const FTransform PlaceTransform = F.Place->GetActorTransform();
	const FTransform PostTransform = F.Post->GetActorTransform();
	const FGuid PostGUID = F.Post->GetActorGUID_Implementation();
	const FGuid RegionGUID = F.Region->FloorVolumeGUID;
	const UTerritoryFloorCombatPolicy* Policy = F.Definition->Floors[0].CombatPolicy;
	FTerritoryDefinitionSyncReport Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, false);
	TestTrue(TEXT("Normal setup can infer the one containing Place"), Report.bSucceeded);
	TestEqual(TEXT("Region is bound to that exact Definition"), F.Region->GetPlaceDefinition(), F.Definition);
	TestEqual(TEXT("Normal setup preserves the canonical legacy assignment"), F.Definition->GuardPosts[0].FloorIndex, 0);
	TestTrue(TEXT("Conflict is visible instead of silently moving a legacy assignment"), Report.Warnings.ContainsByPredicate([](const FString& Text) { return Text.Contains(TEXT("geometry detects 1")); }));
	TestNotNull(TEXT("Detected floor receives its missing policy row"), F.Definition->FindFloor(1));
	TestEqual(TEXT("Existing floor label survives discovery"), F.Definition->Floors[0].DisplayName.ToString(), FString(TEXT("Keep my authored ground name")));
	TestTrue(TEXT("Existing combat policy survives discovery"), F.Definition->Floors[0].CombatPolicy == Policy);
	Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
	TestTrue(TEXT("Explicit geometry sync succeeds"), Report.bSucceeded);
	TestEqual(TEXT("Only canonical post row is reassigned"), F.Definition->GuardPosts[0].FloorIndex, 1);
	TestEqual(TEXT("Derived post cache follows its one authority"), F.Post->GetFloorIndex(), 1);
	TestEqual(TEXT("Report identifies the one assignment"), Report.UpdatedFloorAssignments, 1);
	TestTrue(TEXT("Place transform is not reauthored"), F.Place->GetActorTransform().Equals(PlaceTransform));
	TestTrue(TEXT("Post transform is not reauthored"), F.Post->GetActorTransform().Equals(PostTransform));
	TestEqual(TEXT("Post durable identity stays stable"), F.Post->GetActorGUID_Implementation(), PostGUID);
	TestEqual(TEXT("Region identity stays stable"), F.Region->FloorVolumeGUID, RegionGUID);
	Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
	TestTrue(TEXT("Repeated synchronization is idempotent"), Report.bSucceeded);
	TestEqual(TEXT("No repeated asset updates"), Report.UpdatedDefinitions, 0);
	TestEqual(TEXT("No repeated actor updates"), Report.UpdatedActors, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFloorDetectionAmbiguityAtomic,
	"TerritoryFramework.Editor.Authoring.FloorDetection.AmbiguityIsNonMutating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFloorDetectionAmbiguityAtomic::RunTest(const FString& Parameters)
{
	TerritoryFloorAuthoringTests::FFixture F;
	if (!TestTrue(TEXT("Floor authoring fixture"), F.IsReady())) return false;
	ATerritoryProperty* Other = F.World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* OtherDefinition = NewObject<UTerritoryPlaceDefinition>();
	OtherDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	OtherDefinition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	OtherDefinition->ApplyToTerritory(Other);
	FTerritoryDefinitionSyncReport Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
	TestFalse(TEXT("Overlapping Places cannot claim an unbound floor automatically"), Report.bSucceeded);
	TestTrue(TEXT("Ambiguity diagnostic reports both candidates"), Report.Errors.ContainsByPredicate([](const FString& Text) { return Text.Contains(TEXT("2 fully containing Places")); }));
	TestNull(TEXT("No partial binding after failed preflight"), F.Region->GetPlaceDefinition());
	TestEqual(TEXT("No partial rows after failed preflight"), F.Definition->Floors.Num(), 1);
	TestEqual(TEXT("No partial post assignment after failed preflight"), F.Definition->GuardPosts[0].FloorIndex, 0);
	F.Region->PlaceDefinition = F.Definition;
	Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
	TestTrue(TEXT("Explicit owner resolves automatic ownership ambiguity"), Report.bSucceeded);
	TestTrue(TEXT("Explicit owner is preserved"), F.Region->GetPlaceDefinition() == F.Definition);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFloorDetectionQuotaAndGeometry,
	"TerritoryFramework.Editor.Authoring.FloorDetection.PreservesQuotaAndRejectsPartialGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFloorDetectionQuotaAndGeometry::RunTest(const FString& Parameters)
{
	TerritoryFloorAuthoringTests::FFixture F;
	if (!TestTrue(TEXT("Floor authoring fixture"), F.IsReady())) return false;
	F.Definition->Floors[0].DesiredGuards = 1;
	FTerritoryDefinitionSyncReport Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
	TestFalse(TEXT("Detection cannot make an existing quota impossible"), Report.bSucceeded);
	TestEqual(TEXT("Rejected proposal keeps floor assignment"), F.Definition->GuardPosts[0].FloorIndex, 0);
	TestNull(TEXT("Rejected proposal does not partially bind a region"), F.Region->PlaceDefinition.Get());
	F.Definition->Floors[0].DesiredGuards = 0;
	F.Region->FloorBounds->SetBoxExtent(FVector(600, 100, 50));
	TestFalse(TEXT("A contained center cannot hide a floor box extending outside Place bounds"), F.Region->IsFullyContainedBy(F.Place));
	Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, nullptr, true);
	TestFalse(TEXT("Whole-world detection reports uncontained boxes"), Report.bSucceeded);
	TestNull(TEXT("Uncontained region remains unbound"), F.Region->PlaceDefinition.Get());
	F.Region->FloorBounds->SetBoxExtent(FVector(100, 100, 50));
	F.Region->SetActorRotation(FRotator(0, 45, 0));
	TestTrue(TEXT("Rotated floor boxes use transformed corners"), F.Region->IsFullyContainedBy(F.Place));
	F.Region->SetActorScale3D(FVector(10, 1, 1));
	TestFalse(TEXT("Scaled floor boxes cannot be mistaken for their unscaled extent"), F.Region->IsFullyContainedBy(F.Place));
	F.Region->SetActorScale3D(FVector::OneVector);
	F.Region->PlaceDefinition = F.Definition;
	ATerritoryFloorVolume* Conflict = F.World->SpawnActor<ATerritoryFloorVolume>(ATerritoryFloorVolume::StaticClass(), F.Region->GetActorTransform());
	Conflict->FloorBounds->SetBoxExtent(FVector(100, 100, 50));
	Conflict->PlaceDefinition = F.Definition;
	Conflict->FloorIndex = 2;
	Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
	TestFalse(TEXT("A post inside two different floors requires explicit geometry repair"), Report.bSucceeded);
	TestEqual(TEXT("Overlapping floor conflict cannot add partial policy rows"), F.Definition->Floors.Num(), 1);
	TestEqual(TEXT("Overlapping floor conflict cannot change post grouping"), F.Definition->GuardPosts[0].FloorIndex, 0);
	UWorld* GameWorld = UWorld::CreateWorld(EWorldType::Game, false);
	Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(GameWorld, nullptr, true);
	TestFalse(TEXT("Editor authoring can never mutate a gameplay world"), Report.bSucceeded);
	if (GameWorld) GameWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFloorDetectionUndoRedo,
	"TerritoryFramework.Editor.Authoring.FloorDetection.IsolatedUndoRedo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFloorDetectionUndoRedo::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Editor is available for the transaction regression"), GEditor)) return false;
	if (GEditor->IsTransactionActive() || GIsTransacting || GUndo)
	{
		AddError(TEXT("Floor undo regression requires an idle transaction system; existing user transactions were not touched."));
		return false;
	}
	const TStrongObjectPtr<UTransactor> OriginalBuffer(GEditor->Trans.Get());
	const int32 OriginalQueueLength = OriginalBuffer.IsValid() ? OriginalBuffer->GetQueueLength() : 0;
	const int32 OriginalUndoCount = OriginalBuffer.IsValid() ? OriginalBuffer->GetUndoCount() : 0;
	const TStrongObjectPtr<UTransBuffer> FixtureBuffer(NewObject<UTransBuffer>());
	FixtureBuffer->Initialize(1024 * 1024);
	{
		// UndoTransaction forwards to Trans. This isolated buffer never contains the user's
		// map or redo history, and deliberately has no editor-wide UI undo delegates.
		TGuardValue<TObjectPtr<UTransactor>> IsolatedBuffer(GEditor->Trans, TObjectPtr<UTransactor>(FixtureBuffer.Get()));
		TerritoryFloorAuthoringTests::FFixture F;
		if (!TestTrue(TEXT("Transient floor authoring fixture"), F.IsReady())) return false;
		const FTransform PostTransform = F.Post->GetActorTransform();
		const FGuid PostGUID = F.Post->GetActorGUID_Implementation();
		const FGuid FloorGUID = F.Region->FloorVolumeGUID;
		TestEqual(TEXT("Fixture begins with an empty isolated history"), FixtureBuffer->GetQueueLength(), 0);
		const FTerritoryDefinitionSyncReport Report = UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(F.World, F.Definition, true);
		if (!TestTrue(TEXT("Floor synchronization succeeds before undo"), Report.bSucceeded)) return false;
		TestTrue(TEXT("Detected region binds to the fixture Definition"), F.Region->GetPlaceDefinition() == F.Definition);
		TestEqual(TEXT("Detected floor adds the missing policy row"), F.Definition->Floors.Num(), 2);
		TestEqual(TEXT("Canonical post row moves to the detected floor"), F.Definition->GuardPosts[0].FloorIndex, 1);
		TestEqual(TEXT("Derived post cache follows the canonical row"), F.Post->GetFloorIndex(), 1);
		if (!TestEqual(TEXT("Synchronization owns exactly one undo transaction"), FixtureBuffer->GetQueueLength(), 1)) return false;
		TestTrue(TEXT("Definition changes are recorded"), FixtureBuffer->IsObjectInTransactionBuffer(F.Definition));
		TestTrue(TEXT("Floor binding changes are recorded"), FixtureBuffer->IsObjectInTransactionBuffer(F.Region));
		TestTrue(TEXT("Derived post changes are recorded"), FixtureBuffer->IsObjectInTransactionBuffer(F.Post));
		if (!TestTrue(TEXT("Editor undoes only the isolated synchronization"), GEditor->UndoTransaction())) return false;
		TestNull(TEXT("Undo restores the unbound floor"), F.Region->GetPlaceDefinition());
		TestEqual(TEXT("Undo removes only the newly detected policy row"), F.Definition->Floors.Num(), 1);
		TestEqual(TEXT("Undo restores the authored floor name"), F.Definition->Floors[0].DisplayName.ToString(), FString(TEXT("Keep my authored ground name")));
		TestEqual(TEXT("Undo restores the canonical post assignment"), F.Definition->GuardPosts[0].FloorIndex, 0);
		TestEqual(TEXT("Undo restores the post's derived floor cache"), F.Post->GetFloorIndex(), 0);
		TestTrue(TEXT("Undo preserves the post transform"), F.Post->GetActorTransform().Equals(PostTransform));
		TestEqual(TEXT("Undo preserves the stable post identity"), F.Post->GetActorGUID_Implementation(), PostGUID);
		TestEqual(TEXT("Undo preserves the stable floor identity"), F.Region->FloorVolumeGUID, FloorGUID);
		if (!TestTrue(TEXT("Editor can redo the same isolated synchronization"), GEditor->RedoTransaction())) return false;
		TestTrue(TEXT("Redo reapplies the exact Definition binding"), F.Region->GetPlaceDefinition() == F.Definition);
		TestEqual(TEXT("Redo restores the generated policy row"), F.Definition->Floors.Num(), 2);
		TestEqual(TEXT("Redo restores the canonical assignment"), F.Definition->GuardPosts[0].FloorIndex, 1);
		TestEqual(TEXT("Redo restores the derived post cache"), F.Post->GetFloorIndex(), 1);
		TestTrue(TEXT("Redo preserves the post transform"), F.Post->GetActorTransform().Equals(PostTransform));
		FixtureBuffer->Reset(FText::FromString(TEXT("Transient floor test finished")));
	}
	TestTrue(TEXT("The user's original transaction buffer is restored"), GEditor->Trans == OriginalBuffer.Get());
	if (OriginalBuffer.IsValid())
	{
		TestEqual(TEXT("User undo history length is unchanged"), OriginalBuffer->GetQueueLength(), OriginalQueueLength);
		TestEqual(TEXT("User redo position is unchanged"), OriginalBuffer->GetUndoCount(), OriginalUndoCount);
	}
	return true;
}

#endif
