#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AssetRegistry/AssetData.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/DataValidation.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUnloadedHierarchyReconciliation,
	"TerritoryFramework.WorldPartition.Regression.UnloadedAncestorsFollowChildSnapshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUnloadedHierarchyReconciliation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Hierarchy world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	const auto Tag = [](const TCHAR* Value) { return FGameplayTag::RequestGameplayTag(Value); };
	const FGameplayTag Heroes = Tag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
	auto* City = NewObject<UTerritoryCityDefinition>();
	auto* District = NewObject<UTerritoryDistrictDefinition>();
	auto* First = NewObject<UTerritoryPlaceDefinition>();
	auto* Second = NewObject<UTerritoryPlaceDefinition>();
	City->TerritoryTag = Tag(TEXT("Territory.HavenReach"));
	District->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare"));
	First->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Second->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Warehouse"));
	for (UTerritoryDefinition* Definition : TArray<UTerritoryDefinition*>{City, District, First, Second})
	{
		Definition->StableTerritoryGUID = FGuid::NewGuid();
		Definition->InitialState = ETerritoryInitialState::Unclaimed;
		Definition->InitialAvailability = ETerritoryAvailability::Unlocked;
		Definition->InitialGuardCount = 0;
	}
	District->Places = {First, Second};
	City->Districts = {District};
	City->RefreshHierarchyLinks();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->CampaignCities = {City};
	State->RefreshStrategicDirectory();
	auto Publish = [&](UTerritoryPlaceDefinition* Definition, FGameplayTag Owner,
		ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked)
	{
		FReplicatedCaptureSummary Row = State->GetCaptureSummary(Definition->TerritoryTag);
		Row.CurrentOwner = Owner;
		Row.State = Owner.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Row.Availability = Availability;
		State->SetCaptureSummary(Row);
	};
	Publish(First, Heroes);
	TestEqual(TEXT("One child cannot secure an absent District"), State->GetCaptureSummary(District->TerritoryTag).State, ETerritoryState::Contested);
	Publish(Second, Heroes);
	TestEqual(TEXT("Never-loaded District follows complete ownership"), State->GetCaptureSummary(District->TerritoryTag).CurrentOwner, Heroes);
	TestEqual(TEXT("Never-loaded City follows its reduced District"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	TestEqual(TEXT("Strategic staging sees the new secure District"), State->GetClaimedDistrictCountForFaction(Heroes), 1);
	State->ExportPersistentState();
	Publish(Second, Bandits);
	TestFalse(TEXT("Mixed ownership clears the City owner"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner.IsValid());
	TestEqual(TEXT("Mixed ownership removes staging eligibility"), State->GetClaimedDistrictCountForFaction(Heroes), 0);
	State->ImportPersistentState();
	TestEqual(TEXT("Save/load rebuilds unloaded ancestors from saved children"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	Publish(Second, Heroes, ETerritoryAvailability::Locked);
	TestEqual(TEXT("Locked child cannot secure the District"), State->GetCaptureSummary(District->TerritoryTag).State, ETerritoryState::Contested);
	Publish(Second, Heroes);
	State->ReplicatedCaptureSummaries.RemoveAll([Second](const auto& Row) { return Row.TerritoryTag == Second->TerritoryTag; });
	Publish(First, Heroes);
	// Preserved, not cleared: a missing row is an unknown, and the district keeps the last owner
	// it was reconciled to until a complete reduction replaces it. The dedicated contract test
	// below carries the tenure and eligibility half of this.
	TestEqual(TEXT("Missing child snapshot preserves the last verified owner"),
		State->GetCaptureSummary(District->TerritoryTag).CurrentOwner, Heroes);
	State->RegisterDefinitionHierarchy(City);
	Publish(Second, Heroes);
	District->Places.Add(First);
	State->RegisterDefinitionHierarchy(City);
	// A duplicate authored slot is refused rather than silently skipped, so it cannot secure the
	// parent - but refusing is a deferral, so the parent keeps its owner instead of losing it.
	TestEqual(TEXT("Duplicate authored slot defers without clearing the parent"),
		State->GetCaptureSummary(District->TerritoryTag).CurrentOwner, Heroes);
	District->Places.Pop();
	State->RegisterDefinitionHierarchy(City);
	TestEqual(TEXT("Corrected topology recovers"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	FReplicatedCaptureSummary WrongIdentity = State->GetCaptureSummary(Second->TerritoryTag);
	WrongIdentity.TerritoryGUID = FGuid::NewGuid();
	State->SetCaptureSummary(WrongIdentity);
	TestEqual(TEXT("Reused tag with wrong GUID is an unknown, so it defers rather than clearing"),
		State->GetCaptureSummary(District->TerritoryTag).CurrentOwner, Heroes);
	State->RegisterDefinitionHierarchy(City);
	Publish(Second, Heroes);
	State->SetRole(ROLE_SimulatedProxy);
	Publish(Second, Bandits);
	TestEqual(TEXT("Client cannot mutate snapshot ownership"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	State->SetRole(ROLE_Authority);

	auto* LoadedDistrict = World->SpawnActor<ATerritoryDistrict>();
	District->ApplyToTerritory(LoadedDistrict);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(LoadedDistrict);
	State->PublishTerritorySummary(LoadedDistrict);
	Publish(First, Bandits);
	TestEqual(TEXT("Loaded District reduces its absent children through Volume"), LoadedDistrict->GetTerritoryState(), ETerritoryState::Contested);
	Publish(First, Heroes);
	TestEqual(TEXT("Loaded District secures when absent children agree"), LoadedDistrict->GetOwningFaction(), Heroes);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->UnregisterTerritory(LoadedDistrict);
	LoadedDistrict->Destroy();
	auto* LoadedCity = World->SpawnActor<ATerritoryCity>();
	City->ApplyToTerritory(LoadedCity);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(LoadedCity);
	auto* LoadedPlace = World->SpawnActor<ATerritoryProperty>();
	First->ApplyToTerritory(LoadedPlace);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(LoadedPlace);
	LoadedPlace->ForceSetOwningFaction(Heroes);
	TestEqual(TEXT("Loaded City reads the absent District after a loaded Place captures"), LoadedCity->GetOwningFaction(), Heroes);
	LoadedPlace->ForceSetOwningFaction(Bandits);
	TestFalse(TEXT("Loaded City loses control through absent District immediately"), LoadedCity->GetOwningFaction().IsValid());
	TestEqual(TEXT("Mixed-lifetime City snapshot matches its actor"), State->GetCaptureSummary(City->TerritoryTag).State, LoadedCity->GetTerritoryState());
	State->ExportPersistentState();
	LoadedPlace->ForceSetOwningFaction(Heroes);
	TestEqual(TEXT("Control recovers without reloading the City"), LoadedCity->GetOwningFaction(), Heroes);
	State->ImportPersistentState();
	TestEqual(TEXT("Directory import does not overwrite loaded actor ownership"), LoadedCity->GetOwningFaction(), Heroes);
	State->PublishTerritorySummary(LoadedPlace);
	TestEqual(TEXT("Restored actor publication reconciles all ancestor snapshots"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	return true;
}

/**
 * The completeness contract on the durable side, as five transitions.
 *
 * A reduction is a *result* only when every authored child resolved to an exact identity. Anything
 * less - a child with no row, a child whose own subtree is unresolved, a child slot that is empty or
 * declared twice - is a default view standing in for a value nobody has. Committing it clears a
 * restored owner and, worse, writes a tenure to history that never ended: the second half is what
 * made the mistake permanent, because history is saved and nothing later can tell a fabricated
 * tenure from a real one.
 *
 * The three "records no tenure" assertions are the evidence for the fix. Against the
 * pre-contract reducer the first of them fails twice over: the owner is cleared *and* the tenure is
 * appended.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFHierarchyCompletenessContract,
	"TerritoryFramework.WorldPartition.Regression.IncompleteReductionPreservesVerifiedState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFHierarchyCompletenessContract::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Completeness world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	const auto Tag = [](const TCHAR* Value) { return FGameplayTag::RequestGameplayTag(Value); };
	const FGameplayTag Heroes = Tag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
	auto* City = NewObject<UTerritoryCityDefinition>();
	auto* District = NewObject<UTerritoryDistrictDefinition>();
	auto* First = NewObject<UTerritoryPlaceDefinition>();
	auto* Second = NewObject<UTerritoryPlaceDefinition>();
	City->TerritoryTag = Tag(TEXT("Territory.HavenReach"));
	District->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare"));
	First->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Second->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Warehouse"));
	for (UTerritoryDefinition* Definition : TArray<UTerritoryDefinition*>{City, District, First, Second})
	{
		Definition->StableTerritoryGUID = FGuid::NewGuid();
		Definition->InitialState = ETerritoryInitialState::Unclaimed;
		Definition->InitialAvailability = ETerritoryAvailability::Unlocked;
		Definition->InitialGuardCount = 0;
	}
	District->Places = {First, Second};
	City->Districts = {District};
	City->RefreshHierarchyLinks();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->CampaignCities = {City};
	State->RefreshStrategicDirectory();
	auto Publish = [&](UTerritoryPlaceDefinition* Definition, FGameplayTag Owner,
		ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked)
	{
		FReplicatedCaptureSummary Row = State->GetCaptureSummary(Definition->TerritoryTag);
		Row.CurrentOwner = Owner;
		Row.State = Owner.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Row.Availability = Availability;
		State->SetCaptureSummary(Row);
	};
	const auto OwnerOf = [State](const UTerritoryDefinition* Definition)
	{
		return State->GetCaptureSummary(Definition->TerritoryTag).CurrentOwner;
	};
	const auto TenureCount = [State](const UTerritoryDefinition* Definition)
	{
		return State->GetCaptureSummary(Definition->TerritoryTag).FormerOwningFactions.Num();
	};
	const auto TenureOf = [State](const UTerritoryDefinition* Definition)
	{
		return State->GetCaptureSummary(Definition->TerritoryTag).FormerOwningFactions;
	};

	Publish(First, Heroes);
	Publish(Second, Heroes);
	TestEqual(TEXT("Premise: a complete hierarchy secures the City"), OwnerOf(City), Heroes);
	TestEqual(TEXT("Premise: a complete hierarchy stages its District"),
		State->GetClaimedDistrictCountForFaction(Heroes), 1);
	const int32 CityTenuresBefore = TenureCount(City);
	const int32 DistrictTenuresBefore = TenureCount(District);
	const FReplicatedCaptureSummary FirstRow = State->GetCaptureSummary(First->TerritoryTag);
	const FReplicatedCaptureSummary SecondRow = State->GetCaptureSummary(Second->TerritoryTag);

	// ─── Case 1: a missing child is an unknown, not a loss ───
	State->ReplicatedCaptureSummaries.RemoveAll([Second](const auto& Row)
	{
		return Row.TerritoryTag == Second->TerritoryTag;
	});
	// Re-publishing a child that did resolve is what re-enters the ancestors, exactly as the
	// streamed-out sibling's removal would in gameplay.
	Publish(First, Heroes);
	TestFalse(TEXT("Case 1: a missing child makes the District reduction incomplete"),
		State->IsHierarchyReductionComplete(District->TerritoryTag));
	TestEqual(TEXT("Case 1: a missing child preserves the District's last verified owner"),
		OwnerOf(District), Heroes);
	TestEqual(TEXT("Case 1: a missing child preserves the District's last verified state"),
		State->GetCaptureSummary(District->TerritoryTag).State, ETerritoryState::Claimed);
	TestEqual(TEXT("Case 1: a missing child preserves the City's last verified owner"),
		OwnerOf(City), Heroes);
	TestEqual(TEXT("Case 1: a missing child records no tenure on the District"),
		TenureCount(District), DistrictTenuresBefore);
	TestEqual(TEXT("Case 1: a missing child records no tenure on the City"),
		TenureCount(City), CityTenuresBefore);
	TestEqual(TEXT("Case 1: a retained owner is not staging eligibility"),
		State->GetClaimedDistrictCountForFaction(Heroes), 0);

	// ─── Case 2: incompleteness propagates upward through an unresolved grandchild ───
	// Restore the missing child so the hierarchy is complete again, then remove the *other* Place.
	// The City's own slot still resolves to an exact District row, so nothing at the City's own
	// level is missing: only the recursion into the District's Places can deny it. That recursion
	// is the whole content of this case.
	State->SetCaptureSummary(SecondRow);
	TestTrue(TEXT("Case 2 premise: restoring the child restores completeness"),
		State->IsHierarchyReductionComplete(City->TerritoryTag));
	State->ReplicatedCaptureSummaries.RemoveAll([First](const auto& Row)
	{
		return Row.TerritoryTag == First->TerritoryTag;
	});
	Publish(Second, Heroes);
	TestFalse(TEXT("Case 2: a missing grandchild denies the District reduction"),
		State->IsHierarchyReductionComplete(District->TerritoryTag));
	TestFalse(TEXT("Case 2: a missing grandchild denies the City reduction"),
		State->IsHierarchyReductionComplete(City->TerritoryTag));
	TestEqual(TEXT("Case 2: the District's own row still resolves, so only the recursion denies it"),
		State->GetCaptureSummary(District->TerritoryTag).TerritoryTag, District->TerritoryTag);
	TestEqual(TEXT("Case 2: the District authors two Places, which is what the authored walk requires"),
		State->GetCaptureSummary(District->TerritoryTag).TotalChildren, 2);
	TestEqual(TEXT("Case 2: a missing grandchild preserves the City's last verified owner"),
		OwnerOf(City), Heroes);
	TestEqual(TEXT("Case 2: a missing grandchild preserves the District's last verified owner"),
		OwnerOf(District), Heroes);
	TestEqual(TEXT("Case 2: a missing grandchild records no tenure on the City"),
		TenureCount(City), CityTenuresBefore);
	TestEqual(TEXT("Case 2: a retained owner is still not staging eligibility"),
		State->GetClaimedDistrictCountForFaction(Heroes), 0);

	// ─── Case 5 (taken here, while the hierarchy is still incomplete): a save/load round trip ───
	State->ExportPersistentState();
	State->ImportPersistentState();
	TestFalse(TEXT("Case 5: the incomplete window survives a save/load"),
		State->IsHierarchyReductionComplete(City->TerritoryTag));
	TestEqual(TEXT("Case 5: the preserved City owner survives a save/load"), OwnerOf(City), Heroes);
	TestEqual(TEXT("Case 5: the preserved District owner survives a save/load"),
		OwnerOf(District), Heroes);
	TestEqual(TEXT("Case 5: the preserved District state survives a save/load"),
		State->GetCaptureSummary(District->TerritoryTag).State, ETerritoryState::Claimed);
	TestEqual(TEXT("Case 5: no tenure is fabricated by the round trip"),
		TenureCount(City), CityTenuresBefore);
	TestEqual(TEXT("Case 5: an incomplete hierarchy still stages nothing"),
		State->GetClaimedDistrictCountForFaction(Heroes), 0);

	// ─── Case 3: the child comes back with the same owner ───
	// Restored as the very rows that left, which is what a streamed-in Place re-publishes: the
	// point is that no intermediate state is invented on the way back. Second is already restored
	// by case 2, so this is First arriving.
	State->SetCaptureSummary(FirstRow);
	TestTrue(TEXT("Case 3: restoring the child restores completeness"),
		State->IsHierarchyReductionComplete(City->TerritoryTag));
	TestEqual(TEXT("Case 3: the City keeps the owner it never lost"), OwnerOf(City), Heroes);
	TestEqual(TEXT("Case 3: the District keeps the owner it never lost"), OwnerOf(District), Heroes);
	TestEqual(TEXT("Case 3: restoring the same owner records no tenure"),
		TenureCount(District), DistrictTenuresBefore);
	TestEqual(TEXT("Case 3: restoring the same owner records no tenure on the City"),
		TenureCount(City), CityTenuresBefore);
	TestEqual(TEXT("Case 3: a complete hierarchy stages its District again"),
		State->GetClaimedDistrictCountForFaction(Heroes), 1);

	// ─── Case 4: a genuinely Unclaimed child IS a loss, recorded exactly once ───
	FReplicatedCaptureSummary Unclaimed = State->GetCaptureSummary(Second->TerritoryTag);
	Unclaimed.CurrentOwner = FGameplayTag();
	Unclaimed.State = ETerritoryState::Unclaimed;
	State->SetCaptureSummary(Unclaimed);
	TestFalse(TEXT("Case 4: a complete hierarchy with an unclaimed child is a real loss"),
		OwnerOf(District).IsValid());
	TestFalse(TEXT("Case 4: the City loses control with it"), OwnerOf(City).IsValid());
	TestTrue(TEXT("Case 4: the real loss records the tenure"),
		TenureOf(District).HasTagExact(Heroes));
	TestEqual(TEXT("Case 4: the real loss records exactly one tenure on the District"),
		TenureCount(District), DistrictTenuresBefore + 1);
	TestEqual(TEXT("Case 4: the City's loss is recorded too, and only from the real transition"),
		TenureCount(City), CityTenuresBefore + 1);
	TestTrue(TEXT("Case 4: the City's tenure names the faction it genuinely lost to"),
		TenureOf(City).HasTagExact(Heroes));
	State->SetCaptureSummary(Unclaimed);
	TestEqual(TEXT("Case 4: re-reducing the same inputs does not add a second tenure"),
		TenureCount(District), DistrictTenuresBefore + 1);
	TestEqual(TEXT("Case 4: re-reducing adds no second City tenure either"),
		TenureCount(City), CityTenuresBefore + 1);
	TestEqual(TEXT("Case 4: an unowned District stages nothing"),
		State->GetClaimedDistrictCountForFaction(Heroes), 0);

	// ─── Premise control: the same fixture can still record a loss and a recovery ───
	Publish(Second, Heroes);
	Publish(First, Heroes);
	TestEqual(TEXT("Premise: the fixture recovers ownership"), OwnerOf(City), Heroes);
	TestEqual(TEXT("Premise: the fixture stages again"),
		State->GetClaimedDistrictCountForFaction(Heroes), 1);
	Publish(First, Bandits);
	TestFalse(TEXT("Premise: mixed ownership is still a loss"), OwnerOf(City).IsValid());
	TestTrue(TEXT("Premise: the mixed-ownership loss is recorded"),
		TenureOf(City).HasTagExact(Heroes));
	return true;
}

/**
 * The completeness query and the durable reducer are one rule, so a reduction the reducer refused can
 * never be read as verified control.
 *
 * IsHierarchyReductionComplete counted rows whose parent and level matched and compared that count to
 * the row's saved TotalChildren, while ReconcileUnloadedHierarchy classified each *authored* child by
 * exact tag, GUID, parent and level. The two therefore disagreed in precisely the cases where it
 * matters: a child row carrying the right tag and the wrong identity, an obsolete row standing in for
 * a missing authored child, and a TotalChildren saved before the authored child list changed. In all
 * three the reducer deferred and kept its last verified owner, while the query - the gate
 * GetClaimedDistrictCountForFaction applies before a District may stage an assault - reported
 * verified control.
 *
 * Every case drives the same three assertions: the query refuses, the retained owner is the one the
 * reducer kept, and the eligibility consumer agrees with both. Against the cardinality rule the three
 * "the query refuses it" assertions read complete and the three eligibility assertions read 1.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFHierarchyCompletenessIdentity,
	"TerritoryFramework.WorldPartition.Regression.CompletenessIsIdentityNotCardinality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFHierarchyCompletenessIdentity::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Identity world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	const auto Tag = [](const TCHAR* Value) { return FGameplayTag::RequestGameplayTag(Value); };
	const FGameplayTag Heroes = Tag(TEXT("Narrative.Factions.Heroes"));
	auto* City = NewObject<UTerritoryCityDefinition>();
	auto* District = NewObject<UTerritoryDistrictDefinition>();
	auto* First = NewObject<UTerritoryPlaceDefinition>();
	auto* Second = NewObject<UTerritoryPlaceDefinition>();
	City->TerritoryTag = Tag(TEXT("Territory.HavenReach"));
	District->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare"));
	First->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Second->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Warehouse"));
	for (UTerritoryDefinition* Definition : TArray<UTerritoryDefinition*>{City, District, First, Second})
	{
		Definition->StableTerritoryGUID = FGuid::NewGuid();
		Definition->InitialState = ETerritoryInitialState::Unclaimed;
		Definition->InitialAvailability = ETerritoryAvailability::Unlocked;
		Definition->InitialGuardCount = 0;
	}
	District->Places = {First, Second};
	City->Districts = {District};
	City->RefreshHierarchyLinks();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->CampaignCities = {City};
	State->RefreshStrategicDirectory();
	auto Publish = [&](UTerritoryPlaceDefinition* Definition, FGameplayTag Owner)
	{
		FReplicatedCaptureSummary Row = State->GetCaptureSummary(Definition->TerritoryTag);
		Row.CurrentOwner = Owner;
		Row.State = Owner.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Row.Availability = ETerritoryAvailability::Unlocked;
		State->SetCaptureSummary(Row);
	};
	const auto OwnerOf = [State](const UTerritoryDefinition* Definition)
	{
		return State->GetCaptureSummary(Definition->TerritoryTag).CurrentOwner;
	};
	const auto DropRow = [State](const UTerritoryDefinition* Definition)
	{
		State->ReplicatedCaptureSummaries.RemoveAll([Definition](const FReplicatedCaptureSummary& Row)
		{
			return Row.TerritoryTag == Definition->TerritoryTag;
		});
	};

	Publish(First, Heroes);
	Publish(Second, Heroes);
	TestEqual(TEXT("Premise: a complete hierarchy secures the District"), OwnerOf(District), Heroes);
	TestEqual(TEXT("Premise: a complete hierarchy secures the City"), OwnerOf(City), Heroes);
	TestEqual(TEXT("Premise: a complete hierarchy stages its District"),
		State->GetClaimedDistrictCountForFaction(Heroes), 1);
	// Captured while the reduction is known complete, so a later case can restore the exact rows an
	// earlier one disturbed without re-deriving them.
	const FReplicatedCaptureSummary DistrictRow = State->GetCaptureSummary(District->TerritoryTag);
	const FReplicatedCaptureSummary SecondRow = State->GetCaptureSummary(Second->TerritoryTag);

	// ─── Case 1: the right tag carrying the wrong identity is an unknown, not control ───
	// The reducer refuses this row, because its GUID is not the authored child's. The query must
	// refuse it too: a reused tag is how a recycled or corrupted row presents, and the identity test
	// is the only thing that can tell it from the real child.
	{
		FReplicatedCaptureSummary Impostor = State->GetCaptureSummary(Second->TerritoryTag);
		Impostor.TerritoryGUID = FGuid::NewGuid();
		State->SetCaptureSummary(Impostor);
		TestFalse(TEXT("Case 1: the query refuses a child carrying the wrong GUID"),
			State->IsHierarchyReductionComplete(District->TerritoryTag));
		TestEqual(TEXT("Case 1: the retained owner is the one the reducer kept"),
			OwnerOf(District), Heroes);
		TestEqual(TEXT("Case 1: a row the reducer refused is not staging eligibility"),
			State->GetClaimedDistrictCountForFaction(Heroes), 0);
		State->SetCaptureSummary(SecondRow);
		TestTrue(TEXT("Case 1: the exact identity restores completeness"),
			State->IsHierarchyReductionComplete(District->TerritoryTag));
		TestEqual(TEXT("Case 1: and restores staging eligibility"),
			State->GetClaimedDistrictCountForFaction(Heroes), 1);
	}

	// ─── Case 2: an obsolete row cannot stand in for a missing authored child ───
	// The Farm belongs to a District this City does not author, so it is what content leaves behind
	// when a Place is reparented or retired: a row with a valid tag, a valid GUID, this parent and
	// this level, and no authored child to match. The authored walk never looks at it; the count rule
	// read it as the missing Place and approved the reduction on the strength of it.
	{
		DropRow(Second);
		FReplicatedCaptureSummary Orphan;
		Orphan.TerritoryTag = Tag(TEXT("Territory.HavenReach.CastleHill.Farm"));
		Orphan.TerritoryGUID = FGuid::NewGuid();
		Orphan.ParentTerritoryTag = District->TerritoryTag;
		Orphan.HierarchyLevel = ETerritoryHierarchyLevel::Place;
		Orphan.State = ETerritoryState::Unclaimed;
		Orphan.Availability = ETerritoryAvailability::Unlocked;
		Orphan.bDefinitionBacked = true;
		State->SetCaptureSummary(Orphan);
		TestEqual(TEXT("Case 2: the two authored Places are the threshold the count compared against"),
			State->GetCaptureSummary(District->TerritoryTag).TotalChildren, 2);
		TestFalse(TEXT("Case 2: an obsolete row does not resolve the missing authored child"),
			State->IsHierarchyReductionComplete(District->TerritoryTag));
		TestEqual(TEXT("Case 2: the retained owner is the one the reducer kept"),
			OwnerOf(District), Heroes);
		TestEqual(TEXT("Case 2: an obsolete row grants no staging eligibility"),
			State->GetClaimedDistrictCountForFaction(Heroes), 0);
		State->ReplicatedCaptureSummaries.RemoveAll([&](const FReplicatedCaptureSummary& Row)
		{
			return Row.TerritoryTag == Orphan.TerritoryTag;
		});
		State->SetCaptureSummary(SecondRow);
		TestTrue(TEXT("Case 2: restoring the authored child restores completeness"),
			State->IsHierarchyReductionComplete(District->TerritoryTag));
	}

	// ─── Case 3: a count saved before the authored child list changed cannot approve it ───
	// Registration refreshes TotalChildren, but a row restored by an in-place import keeps whatever
	// the build that wrote it recorded, and nothing re-validates it against the authored list. One
	// authored Place standing against a count of one reads complete - with the other authored Place
	// gone.
	{
		DropRow(Second);
		FReplicatedCaptureSummary Stale = DistrictRow;
		Stale.TotalChildren = 1;
		State->SetCaptureSummary(Stale);
		TestEqual(TEXT("Case 3: the saved count is the narrower one this case is about"),
			State->GetCaptureSummary(District->TerritoryTag).TotalChildren, 1);
		TestFalse(TEXT("Case 3: a saved count cannot approve a reduction the authored list denies"),
			State->IsHierarchyReductionComplete(District->TerritoryTag));
		TestEqual(TEXT("Case 3: the retained owner is the one the reducer kept"),
			OwnerOf(District), Heroes);
		TestEqual(TEXT("Case 3: a stale count grants no staging eligibility"),
			State->GetClaimedDistrictCountForFaction(Heroes), 0);
	}

	// ─── The refusals above are deferrals, not permanent stalls ───
	// Same reason the topology-defect test gives: a parent that can never reconcile would be worse
	// than one that commits a phantom result. Restoring the authored rows is all it takes, which is
	// what makes refusing the right answer rather than merely the safe one.
	State->SetCaptureSummary(DistrictRow);
	State->SetCaptureSummary(SecondRow);
	TestTrue(TEXT("The authored child list restores completeness"),
		State->IsHierarchyReductionComplete(District->TerritoryTag));
	TestTrue(TEXT("and the City above it"), State->IsHierarchyReductionComplete(City->TerritoryTag));
	TestEqual(TEXT("The fixture returns to exactly what it staged before the cases"),
		State->GetClaimedDistrictCountForFaction(Heroes), 1);
	TestEqual(TEXT("and holds the owner it never lost"), OwnerOf(District), Heroes);
	return true;
}

/**
 * The topology defects both reducers now refuse are reported, so refusing is a deferral and not a
 * permanent silent stall.
 *
 * The old duplicate-slot exemption was justified by exactly that risk: a parent that can never
 * reconcile is worse than one that commits a phantom result. The exemption was still wrong - the
 * phantom result is the same defect the unknown case is refused for - so the fix has to answer the
 * objection rather than ignore it. This is the answer: the asset fails validation, in the shape the
 * duplicate-floor check already established, and the project's editor validation gate already
 * treats an error as a failed run.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFHierarchyChildTopologyValidation,
	"TerritoryFramework.Hierarchy.Validation.ReportsInconsistentChildSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFHierarchyChildTopologyValidation::RunTest(const FString& Parameters)
{
	const TArray<FAssetData> NoAssociatedAssets;
	const auto HasIssue = [](const FDataValidationContext& Context, const FString& Fragment)
	{
		return Context.GetIssues().ContainsByPredicate(
			[&Fragment](const FDataValidationContext::FIssue& Issue)
			{
				return Issue.Message.ToString().Contains(Fragment);
			});
	};
	const auto Tag = [](const TCHAR* Value) { return FGameplayTag::RequestGameplayTag(Value); };
	const FGameplayTag CityTag = Tag(TEXT("Territory.HavenReach"));
	const FGameplayTag DistrictTag = Tag(TEXT("Territory.HavenReach.MarketSquare"));
	const FGameplayTag PlaceTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag OtherTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Warehouse"));

	// Healthy content must validate clean, or the gate would fail on every shipped asset.
	{
		auto* District = NewObject<UTerritoryDistrictDefinition>();
		auto* OtherDistrict = NewObject<UTerritoryDistrictDefinition>();
		District->TerritoryTag = DistrictTag;
		OtherDistrict->TerritoryTag = OtherTag;
		auto* City = NewObject<UTerritoryCityDefinition>();
		City->TerritoryTag = CityTag;
		City->Districts = {District, OtherDistrict};
		FDataValidationContext Context(false, EDataValidationUsecase::Script, NoAssociatedAssets);
		City->IsDataValid(Context);
		TestEqual(TEXT("A City with two distinct child tags reports no errors"),
			static_cast<int32>(Context.GetNumErrors()), 0);
	}
	{
		auto* First = NewObject<UTerritoryPlaceDefinition>();
		auto* Second = NewObject<UTerritoryPlaceDefinition>();
		First->TerritoryTag = PlaceTag;
		Second->TerritoryTag = OtherTag;
		auto* District = NewObject<UTerritoryDistrictDefinition>();
		District->TerritoryTag = DistrictTag;
		District->Places = {First, Second};
		FDataValidationContext Context(false, EDataValidationUsecase::Script, NoAssociatedAssets);
		District->IsDataValid(Context);
		TestEqual(TEXT("A District with two distinct child tags reports no errors"),
			static_cast<int32>(Context.GetNumErrors()), 0);
	}

	// The defect the reducers refuse with InconsistentChildCount.
	{
		auto* District = NewObject<UTerritoryDistrictDefinition>();
		District->TerritoryTag = DistrictTag;
		auto* City = NewObject<UTerritoryCityDefinition>();
		City->TerritoryTag = CityTag;
		City->Districts = {District, District};
		FDataValidationContext Context(false, EDataValidationUsecase::Script, NoAssociatedAssets);
		City->IsDataValid(Context);
		TestEqual(TEXT("A duplicated District tag is an error"),
			static_cast<int32>(Context.GetNumErrors()), 1);
		TestTrue(TEXT("The duplicated District error names the tag"),
			HasIssue(Context, TEXT("declared more than once")));
		TestTrue(TEXT("The duplicated District error names the tag's text"),
			HasIssue(Context, DistrictTag.ToString()));
	}
	{
		auto* Place = NewObject<UTerritoryPlaceDefinition>();
		Place->TerritoryTag = PlaceTag;
		auto* District = NewObject<UTerritoryDistrictDefinition>();
		District->TerritoryTag = DistrictTag;
		District->Places = {Place, Place};
		FDataValidationContext Context(false, EDataValidationUsecase::Script, NoAssociatedAssets);
		District->IsDataValid(Context);
		TestTrue(TEXT("A duplicated Place tag is an error"),
			HasIssue(Context, TEXT("declared more than once")));
	}

	// An empty slot cannot resolve to an identity either, so it is refused the same way.
	{
		auto* City = NewObject<UTerritoryCityDefinition>();
		City->TerritoryTag = CityTag;
		City->Districts = {nullptr};
		FDataValidationContext Context(false, EDataValidationUsecase::Script, NoAssociatedAssets);
		City->IsDataValid(Context);
		TestEqual(TEXT("A null child slot is an error"),
			static_cast<int32>(Context.GetNumErrors()), 1);
		TestTrue(TEXT("The null child error explains the empty slot"),
			HasIssue(Context, TEXT("empty child slot")));
	}

	// An untagged child is the third shape, and a validation warning would be worse than useless
	// here: the project's gate counts warnings as a failed run, and a deferred parent silently
	// stops reconciling.
	{
		auto* Place = NewObject<UTerritoryPlaceDefinition>();
		auto* District = NewObject<UTerritoryDistrictDefinition>();
		District->TerritoryTag = DistrictTag;
		District->Places = {Place};
		FDataValidationContext Context(false, EDataValidationUsecase::Script, NoAssociatedAssets);
		District->IsDataValid(Context);
		TestEqual(TEXT("An untagged child is an error"),
			static_cast<int32>(Context.GetNumErrors()), 1);
		TestTrue(TEXT("The untagged child error asks for a tag"),
			HasIssue(Context, TEXT("has no Territory tag")));
	}
	return true;
}
#endif
