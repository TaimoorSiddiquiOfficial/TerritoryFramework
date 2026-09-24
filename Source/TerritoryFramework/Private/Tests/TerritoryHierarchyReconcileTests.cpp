// Coverage for ATerritoryCity::ReconcileDerivedControl — the aggregate reduction that
// turns District results into City ownership.
//
// Before this file no test instantiated a City/District hierarchy that reconciled, so the
// reduction ran only in gameplay. Three properties are pinned:
//
//  1. A City that already holds an owner must not commit — or record a tenure for — a
//     derivation taken while one of its authored Districts is still unknown. An unknown
//     child reduces to a default Unclaimed view, and committing that fabricates a city
//     loss that never happened and writes it to the save.
//  2. The suppression must be narrow: a *complete* hierarchy whose Districts are
//     unclaimed is a real loss, and must still commit.
//  3. A fresh campaign City cannot hold an authored owner at all. An aggregate's
//     political state is reduced from its children, so authoring an owner on a City is
//     inert (TerritoryVolume.cpp:245-255).
//
// Property 1 has exactly one reachable entry: a save load. CommitOwnershipData refuses a
// direct aggregate political change unless the commit is derived (:1739-1744), so the only
// writer of an aggregate City's political state is Actor->Serialize(Ar) with ArIsSaveGame
// (:798, and ":816 Narrative's Serialize(Ar) just restored OwnershipData from the save").
// The window is a City whose save restored an owner and whose authored District has no
// saved record and has not registered yet — the migration case AGENTS.md §7 covers, where
// a campaign predates a District being authored into the City.
//
// Scope note: the City's loss/capture delegates are dynamic multicast delegates, which
// cannot take a lambda and have no UObject listener in this suite. They are therefore not
// observed directly. What is asserted instead is the commit they announce — the public
// read model (GetOwningFaction / GetTerritoryState / IsFullyCaptured) and the durable
// history the commit writes. A broadcast that did not correspond to one of those would be
// a different defect, and this file would not catch it.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Test-only seam onto the two members no public route reaches. */
class FTFHierarchyTestAccess
{
public:
	/** Writes OwnershipData the way a load does, and selects the BeginPlay branch a load
	 *  selects. Faithful to TerritoryVolume.cpp:816; not a stand-in for a different state. */
	static void SeedLoadedOwnership(ATerritoryCity& City, const FGameplayTag& Owner,
		ETerritoryState State, bool bLoadedFromSave)
	{
		City.OwnershipData.OwningFaction =
			State == ETerritoryState::Claimed ? Owner : FGameplayTag();
		City.OwnershipData.State = State;
		City.OwnershipData.ControlProgress = State == ETerritoryState::Claimed ? 1.f : 0.f;
		City.bLoadedFromSave = bLoadedFromSave;
	}

	static void Reconcile(ATerritoryCity& City)
	{
		City.ReconcileDerivedControl();
	}
};

namespace
{
	/** The three tags these fixtures share with the example content's tag set. */
	struct FTFHierarchyTags
	{
		FGameplayTag City;
		FGameplayTag District;
		FGameplayTag Bandits;

		static FTFHierarchyTags Resolve()
		{
			const auto Tag = [](const TCHAR* Value)
			{
				return FGameplayTag::RequestGameplayTag(Value, false);
			};
			FTFHierarchyTags Result;
			Result.City = Tag(TEXT("Territory.HavenReach"));
			Result.District = Tag(TEXT("Territory.HavenReach.MarketSquare"));
			Result.Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
			return Result;
		}

		bool IsValid() const
		{
			return City.IsValid() && District.IsValid() && Bandits.IsValid();
		}
	};

	/** City with exactly one authored District and no Places — the smallest hierarchy whose
	 *  child can be independently absent. */
	struct FTFHierarchyFixture
	{
		UTerritoryCityDefinition* CityDefinition = nullptr;
		UTerritoryDistrictDefinition* DistrictDefinition = nullptr;
	};

	FTFHierarchyFixture MakeHierarchyFixture(const FTFHierarchyTags& Tags,
		const FGameplayTag& CityInitialOwner, ETerritoryInitialState CityInitialState)
	{
		FTFHierarchyFixture Fixture;
		Fixture.CityDefinition = NewObject<UTerritoryCityDefinition>();
		Fixture.DistrictDefinition = NewObject<UTerritoryDistrictDefinition>();
		Fixture.CityDefinition->TerritoryTag = Tags.City;
		Fixture.CityDefinition->StableTerritoryGUID = FGuid::NewGuid();
		Fixture.CityDefinition->TerritoryActorClass = ATerritoryCity::StaticClass();
		Fixture.CityDefinition->InitialOwningFaction = CityInitialOwner;
		Fixture.CityDefinition->InitialState = CityInitialState;
		Fixture.DistrictDefinition->TerritoryTag = Tags.District;
		Fixture.DistrictDefinition->StableTerritoryGUID = FGuid::NewGuid();
		Fixture.DistrictDefinition->TerritoryActorClass = ATerritoryDistrict::StaticClass();
		Fixture.CityDefinition->Districts.Add(Fixture.DistrictDefinition);
		Fixture.CityDefinition->RefreshHierarchyLinks();
		return Fixture;
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// 1. An incomplete hierarchy is not a state, and must not be committed
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFHierarchyIncompleteReconcile,
	"TerritoryFramework.Hierarchy.Regression.IncompleteHierarchyIsNotCommitted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFHierarchyIncompleteReconcile::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Hierarchy reconcile world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

	const FTFHierarchyTags Tags = FTFHierarchyTags::Resolve();
	if (!TestTrue(TEXT("Fixture tags resolve from the example tag set"), Tags.IsValid()))
	{
		return false;
	}

	FTFHierarchyFixture Fixture = MakeHierarchyFixture(Tags, Tags.Bandits,
		ETerritoryInitialState::Claimed);
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryCity* City = World->SpawnActor<ATerritoryCity>(
		ATerritoryCity::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("City actor"), City)) return false;
	if (!TestTrue(TEXT("City reads its hierarchy Definition"),
		Fixture.CityDefinition->ApplyToTerritory(City)))
	{
		return false;
	}

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!TestNotNull(TEXT("Registry"), Registry)) return false;
	TestEqual(TEXT("City registers"), Registry->RegisterTerritory(City),
		ETerritoryRegistrationResult::Success);

	// The ownership a save load hands an aggregate City. This is the state under test.
	FTFHierarchyTestAccess::SeedLoadedOwnership(*City, Tags.Bandits,
		ETerritoryState::Claimed, false);
	TestEqual(TEXT("Premise: the City holds an owner before reconciling"),
		City->GetOwningFaction(), Tags.Bandits);
	TestEqual(TEXT("Premise: the City is Claimed before reconciling"),
		City->GetTerritoryState(), ETerritoryState::Claimed);

	// ─── Phase 1: the District is neither registered nor summarised ───
	TestEqual(TEXT("The City authors exactly one District slot"), City->GetDistrictCount(), 1);
	TestTrue(TEXT("No District is loaded yet"), City->GetDistricts().IsEmpty());

	FTFHierarchyTestAccess::Reconcile(*City);

	TestEqual(TEXT("An unresolved District does not fabricate a City loss"),
		City->GetOwningFaction(), Tags.Bandits);
	TestEqual(TEXT("An unresolved District does not un-Claim the City"),
		City->GetTerritoryState(), ETerritoryState::Claimed);
	TestFalse(TEXT("An unresolved District does not record a tenure the City never lost"),
		City->GetOwnershipData().FormerOwningFactions.HasTagExact(Tags.Bandits));
	// IsFullyCaptured() is a live recomputation over *loaded* Districts
	// (TerritoryHierarchy.cpp:374-378), so it cannot agree with the committed state while a
	// District is unknown. That divergence is by design — the live query fails closed — and
	// this test governs the committed, saved, broadcast state instead. Pinned here so a
	// future change to the live query has to confront the difference.
	TestFalse(TEXT("The live child recomputation stays conservative about an unknown District"),
		City->IsFullyCaptured());

	// ─── Phase 2: premise control — same call, complete hierarchy, unclaimed child ───
	// One input changed. If the reduction were not running, this phase would report nothing
	// either, and phase 1 would prove nothing.
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("District actor"), District)) return false;
	if (!TestTrue(TEXT("District reads its hierarchy Definition"),
		Fixture.DistrictDefinition->ApplyToTerritory(District)))
	{
		return false;
	}
	TestEqual(TEXT("District registers"), Registry->RegisterTerritory(District),
		ETerritoryRegistrationResult::Success);
	TestEqual(TEXT("The District is now loaded"), City->GetDistricts().Num(), 1);

	FTFHierarchyTestAccess::Reconcile(*City);

	TestFalse(TEXT("A complete hierarchy with an unclaimed District IS a City loss"),
		City->GetOwningFaction().IsValid());
	TestEqual(TEXT("The City is Unclaimed once its only District is"),
		City->GetTerritoryState(), ETerritoryState::Unclaimed);
	TestTrue(TEXT("A real loss DOES record the tenure in history"),
		City->GetOwnershipData().FormerOwningFactions.HasTagExact(Tags.Bandits));

	// ─── Phase 3: recovery — the same District comes back to the same faction ───
	// Through the derived path, not a direct aggregate write: the guard at
	// TerritoryVolume.cpp:1739 rejects a direct aggregate mutation even for a District.
	District->SetDerivedControl(Tags.Bandits, ETerritoryState::Claimed);
	TestEqual(TEXT("Premise: the District now reports the owning faction"),
		District->GetOwningFaction(), Tags.Bandits);

	FTFHierarchyTestAccess::Reconcile(*City);

	TestEqual(TEXT("A complete, single-owner hierarchy secures the City"),
		City->GetOwningFaction(), Tags.Bandits);
	TestEqual(TEXT("The City is Claimed again"), City->GetTerritoryState(),
		ETerritoryState::Claimed);
	TestTrue(TEXT("Securing the City reports it fully captured"),
		City->IsFullyCaptured());
	TestEqual(TEXT("The capturing faction is the District's owner"),
		City->GetCapturingFaction(), Tags.Bandits);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 2. A fresh campaign City cannot hold an authored owner
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCityFreshStartHoldsNoAuthoredOwner,
	"TerritoryFramework.Hierarchy.Regression.FreshStartCityHoldsNoAuthoredOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCityFreshStartHoldsNoAuthoredOwner::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Fresh start world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

	const FTFHierarchyTags Tags = FTFHierarchyTags::Resolve();
	if (!TestTrue(TEXT("Fixture tags resolve"), Tags.IsValid())) return false;

	// Authored to start Claimed by Bandits — the configuration that would make a fresh-start
	// ordering hazard reachable, if a fresh campaign honoured it.
	FTFHierarchyFixture Fixture = MakeHierarchyFixture(Tags, Tags.Bandits,
		ETerritoryInitialState::Claimed);
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryCity* City = World->SpawnActor<ATerritoryCity>(
		ATerritoryCity::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("City actor"), City)) return false;
	if (!TestTrue(TEXT("City reads its hierarchy Definition"),
		Fixture.CityDefinition->ApplyToTerritory(City)))
	{
		return false;
	}
	TestEqual(TEXT("Premise: the Definition DID author an owning faction"),
		Fixture.CityDefinition->InitialOwningFaction, Tags.Bandits);
	TestEqual(TEXT("Premise: the Definition DID author an initial Claimed state"),
		Fixture.CityDefinition->InitialState, ETerritoryInitialState::Claimed);

	City->DispatchBeginPlay();

	AddInfo(FString::Printf(
		TEXT("Fresh-start City after begin play with zero loaded Districts: restored=%d owner=%s state=%d"),
		City->WasRestoredFromCampaignSave() ? 1 : 0,
		City->GetOwningFaction().IsValid() ? *City->GetOwningFaction().ToString() : TEXT("<none>"),
		static_cast<int32>(City->GetTerritoryState())));

	TestFalse(TEXT("Premise: this is the fresh-campaign branch, not the loaded branch"),
		City->WasRestoredFromCampaignSave());
	TestFalse(TEXT("A fresh campaign City does not start owned, so no loss can be fabricated"),
		City->GetOwningFaction().IsValid());
	TestEqual(TEXT("A fresh campaign City starts Unclaimed"),
		City->GetTerritoryState(), ETerritoryState::Unclaimed);
	TestFalse(TEXT("A fresh campaign City with no loaded District records no tenure"),
		City->GetOwnershipData().FormerOwningFactions.HasTagExact(Tags.Bandits));
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 3. A save-restored City must not lose to a District it has no record of
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFSaveLoadedCityUnknownDistrict,
	"TerritoryFramework.Hierarchy.Regression.SaveLoadedCityKeepsOwnerWithoutItsDistrict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSaveLoadedCityUnknownDistrict::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Save-load world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

	const FTFHierarchyTags Tags = FTFHierarchyTags::Resolve();
	if (!TestTrue(TEXT("Fixture tags resolve"), Tags.IsValid())) return false;

	FTFHierarchyFixture Fixture = MakeHierarchyFixture(Tags, Tags.Bandits,
		ETerritoryInitialState::Claimed);
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryCity* City = World->SpawnActor<ATerritoryCity>(
		ATerritoryCity::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("City actor"), City)) return false;
	if (!TestTrue(TEXT("City reads its hierarchy Definition"),
		Fixture.CityDefinition->ApplyToTerritory(City)))
	{
		return false;
	}

	// The campaign was saved before this District was authored into the City, so the save
	// holds the City's owner and nothing at all for the District.
	FTFHierarchyTestAccess::SeedLoadedOwnership(*City, Tags.Bandits,
		ETerritoryState::Claimed, true);

	City->DispatchBeginPlay();

	AddInfo(FString::Printf(
		TEXT("Save-restored City after begin play with zero loaded Districts: restored=%d owner=%s state=%d history=%d"),
		City->WasRestoredFromCampaignSave() ? 1 : 0,
		City->GetOwningFaction().IsValid() ? *City->GetOwningFaction().ToString() : TEXT("<none>"),
		static_cast<int32>(City->GetTerritoryState()),
		City->GetOwnershipData().FormerOwningFactions.Num()));

	TestTrue(TEXT("Premise: the City took the restored-from-save branch"),
		City->WasRestoredFromCampaignSave());
	TestEqual(TEXT("A restored City keeps its owner while its District is unknown"),
		City->GetOwningFaction(), Tags.Bandits);
	TestEqual(TEXT("A restored City stays Claimed while its District is unknown"),
		City->GetTerritoryState(), ETerritoryState::Claimed);
	TestFalse(TEXT("A restored City does not record a tenure it never lost"),
		City->GetOwnershipData().FormerOwningFactions.HasTagExact(Tags.Bandits));
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 4. A duplicated child slot defers; it does not clear a restored owner
// ═══════════════════════════════════════════════════════════════════════════════

// Property 1 above covers a child that is *unknown*. This covers the other incomplete
// cause: the same authored slot declared twice. It used to be exempt, on the grounds that a
// permanent deferral was worse than a phantom result — but a phantom result is exactly what
// the unknown case is refused for, and the exemption meant a defective asset cleared a
// restored City owner and wrote a tenure for it, silently and forever. The defect is now an
// error from UTerritoryDefinition::IsDataValid, so refusing it defers until the asset is
// fixed rather than deferring forever.
//
// The phase-2 premise control is what makes phase 1 mean anything: one input changes (the
// duplicate is removed) and the identical District is then a real loss. Without it, phase 1
// would pass on a reducer that simply never ran.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFHierarchyDuplicateChildSlot,
	"TerritoryFramework.Hierarchy.Regression.DuplicatedChildSlotDefersInsteadOfClearing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFHierarchyDuplicateChildSlot::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Duplicate slot world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };

	const FTFHierarchyTags Tags = FTFHierarchyTags::Resolve();
	if (!TestTrue(TEXT("Fixture tags resolve"), Tags.IsValid())) return false;

	FTFHierarchyFixture Fixture = MakeHierarchyFixture(Tags, Tags.Bandits,
		ETerritoryInitialState::Claimed);
	// The defect under test: one District object occupying two authored slots, so the reduction
	// cannot tell the two slots apart.
	Fixture.CityDefinition->Districts.Add(Fixture.DistrictDefinition);
	Fixture.CityDefinition->RefreshHierarchyLinks();

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryCity* City = World->SpawnActor<ATerritoryCity>(
		ATerritoryCity::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("City actor"), City)) return false;
	if (!TestTrue(TEXT("City reads its hierarchy Definition"),
		Fixture.CityDefinition->ApplyToTerritory(City)))
	{
		return false;
	}

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!TestNotNull(TEXT("Registry"), Registry)) return false;
	TestEqual(TEXT("City registers"), Registry->RegisterTerritory(City),
		ETerritoryRegistrationResult::Success);

	FTFHierarchyTestAccess::SeedLoadedOwnership(*City, Tags.Bandits,
		ETerritoryState::Claimed, false);
	TestEqual(TEXT("Premise: the City holds an owner before reconciling"),
		City->GetOwningFaction(), Tags.Bandits);

	// Loaded and genuinely unclaimed, so with a sound topology this IS a loss of the City.
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("District actor"), District)) return false;
	if (!TestTrue(TEXT("District reads its hierarchy Definition"),
		Fixture.DistrictDefinition->ApplyToTerritory(District)))
	{
		return false;
	}
	TestEqual(TEXT("District registers"), Registry->RegisterTerritory(District),
		ETerritoryRegistrationResult::Success);
	TestFalse(TEXT("Premise: the loaded District is unclaimed"),
		District->GetOwningFaction().IsValid());
	TestEqual(TEXT("Premise: the City authors two slots for one District tag"),
		City->GetDistrictCount(), 2);
	TestEqual(TEXT("Premise: the loaded District resolves through the child query"),
		City->GetDistricts().Num(), 1);

	// ─── Phase 1: the duplicated slot is refused, so nothing is committed ───
	FTFHierarchyTestAccess::Reconcile(*City);

	TestEqual(TEXT("A duplicated child slot does not clear a restored owner"),
		City->GetOwningFaction(), Tags.Bandits);
	TestEqual(TEXT("A duplicated child slot does not un-Claim the City"),
		City->GetTerritoryState(), ETerritoryState::Claimed);
	TestFalse(TEXT("A duplicated child slot records no tenure the City never lost"),
		City->GetOwnershipData().FormerOwningFactions.HasTagExact(Tags.Bandits));

	// ─── Phase 2: premise control — the duplicate is the only input that changes ───
	Fixture.CityDefinition->Districts.Pop();
	Fixture.CityDefinition->RefreshHierarchyLinks();
	TestEqual(TEXT("Premise: the City now authors one slot"), City->GetDistrictCount(), 1);

	FTFHierarchyTestAccess::Reconcile(*City);

	TestFalse(TEXT("With a sound topology the same unclaimed District IS a City loss"),
		City->GetOwningFaction().IsValid());
	TestEqual(TEXT("The City is Unclaimed once its only District is"),
		City->GetTerritoryState(), ETerritoryState::Unclaimed);
	TestTrue(TEXT("The real loss DOES record the tenure in history"),
		City->GetOwnershipData().FormerOwningFactions.HasTagExact(Tags.Bandits));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
