#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryCollectibleTags.h"
#include "Core/TerritoryCollectibleTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"
#include "Subsystems/TerritoryCollectibleSubsystem.h"

/**
 * Behavioural tests for the collectible findings authority.
 *
 * These are world-level tests, not reflection tests: every assertion reads state back
 * from the subsystem after a mutation the same way a pickup producer would drive it.
 * The subsystem is a World Subsystem, so each case owns its world.
 *
 * The collectible tags are declared here rather than taken from project content: a
 * collectible is authored content, and a test that depended on a designer's tag would
 * start failing the day that collectible is renamed.
 */
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestCollectibleSword, "Territory.Collectible.Test.BronzeSword");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestCollectiblePlate, "Territory.Collectible.Test.HistoryPlate");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestCollectibleLedgerPage, "Territory.Collectible.Test.LedgerPage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestCollectibleRecording, "Territory.Collectible.Test.Recording");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestCollectibleMap, "Territory.Collectible.Test.CityMap");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TestCategoryDocuments, "Territory.CollectibleCategory.Test.Documents");

namespace TestCollectibles
{
	/**
	 * A native tag symbol (UE_DEFINE_GAMEPLAY_TAG_STATIC) is not an FGameplayTag, and the
	 * registry calls take the tag. These read the registered value once, after the macro
	 * above has registered it.
	 */
	const FGameplayTag Sword = TAG_TestCollectibleSword.GetTag();
	const FGameplayTag Plate = TAG_TestCollectiblePlate.GetTag();
	const FGameplayTag LedgerPage = TAG_TestCollectibleLedgerPage.GetTag();
	const FGameplayTag Recording = TAG_TestCollectibleRecording.GetTag();
	const FGameplayTag CityMap = TAG_TestCollectibleMap.GetTag();
	const FGameplayTag DocumentsCategory = TAG_TestCategoryDocuments.GetTag();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectibleValidation,
	"TerritoryFramework.Collectibles.Behavior.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectibleValidation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Collectible validation world exists"), World))
	{
		return false;
	}

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	if (!TestNotNull(TEXT("The collectible subsystem is created with its world"), Collectibles))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag Market = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	const FGuid PlayerId(21, 22, 23, 24);

	// 1. A finding has to name a collectible.
	FTerritoryCollectibleMutationResult Untagged = Collectibles->RecordCollectibleFound(
		FGameplayTag(), FGameplayTag(), FGuid(), FGuid());
	TestFalse(TEXT("A finding without a tag is rejected"), Untagged.bSuccess);
	TestFalse(TEXT("The rejection explains itself"), Untagged.FailureReason.IsEmpty());

	// 2. A category is not a collectible, so the sibling family is refused. This is the
	// whole reason the two roots are separate instead of nested.
	FTerritoryCollectibleMutationResult WrongFamily = Collectibles->RecordCollectibleFound(
		TestCollectibles::DocumentsCategory, FGameplayTag(), FGuid(), FGuid());
	TestFalse(TEXT("A category tag is not a collectible"), WrongFamily.bSuccess);
	TestTrue(TEXT("The rejection names the collectible family"),
		WrongFamily.FailureReason.ToString().Contains(TEXT("Territory.Collectible")));

	// 3. A rejected finding leaves no row and does not advance the counter, so a lifetime
	// "recorded" number cannot be inflated by producers that retry without a tag.
	TestEqual(TEXT("A rejected finding leaves no row behind"), Collectibles->GetFindingCount(), 0);
	TestEqual(TEXT("A rejected finding does not advance the recorded counter"),
		Collectibles->GetTotalRecordedFindingCount(), 0);

	// 4. The happy path.
	FTerritoryCollectibleMutationResult Found = Collectibles->RecordCollectibleFound(
		TestCollectibles::Sword, Market, PlayerId,
		UTerritoryCollectibleSubsystem::MakeCollectibleContextId(TEXT("Sword")));
	TestTrue(TEXT("A well-formed finding is accepted"), Found.bSuccess);
	TestTrue(TEXT("The committed finding carries the authored tag"),
		Found.Finding.CollectibleTag == TestCollectibles::Sword);
	TestTrue(TEXT("The committed finding remembers where it was found"),
		Found.Finding.SourceTerritory == Market);
	TestTrue(TEXT("The committed finding remembers who picked it up"),
		Found.Finding.DiscoveredByPlayerId == PlayerId);
	TestTrue(TEXT("The context id is the caller's deterministic recipe, not a fresh one"),
		Found.Finding.ContextId == UTerritoryCollectibleSubsystem::MakeCollectibleContextId(TEXT("Sword")));
	TestTrue(TEXT("A collectible that was found reports as found"),
		Collectibles->HasCollectibleBeenFound(TestCollectibles::Sword));
	TestEqual(TEXT("One finding was retained"), Collectibles->GetFindingCount(), 1);
	TestEqual(TEXT("One finding was recorded"), Collectibles->GetTotalRecordedFindingCount(), 1);

	FTerritoryCollectibleFinding ReadBack;
	TestTrue(TEXT("The finding can be read by tag"),
		Collectibles->GetCollectibleFinding(TestCollectibles::Sword, ReadBack));
	TestTrue(TEXT("The read-back carries the same tag"), ReadBack.CollectibleTag == TestCollectibles::Sword);

	// 5. A collectible is found once per campaign. Identity is the tag, so a producer that
	// retries (a reloaded overlap, a replayed quest event) records nothing new.
	FTerritoryCollectibleMutationResult Repeat = Collectibles->RecordCollectibleFound(
		TestCollectibles::Sword, FGameplayTag(), FGuid(),
		UTerritoryCollectibleSubsystem::MakeCollectibleContextId(TEXT("SwordAgain")));
	TestFalse(TEXT("A repeat is not reported as a fresh success"), Repeat.bSuccess);
	TestTrue(TEXT("A repeat is reported as already collected"), Repeat.bAlreadyCollected);
	TestTrue(TEXT("A repeat returns the original finding"),
		Repeat.Finding.ContextId == Found.Finding.ContextId);
	TestEqual(TEXT("A repeat does not add a row"), Collectibles->GetFindingCount(), 1);
	TestEqual(TEXT("A repeat does not advance the recorded counter"),
		Collectibles->GetTotalRecordedFindingCount(), 1);

	// 6. A tag that was never found reports nothing rather than a default row.
	FTerritoryCollectibleFinding Missing;
	TestFalse(TEXT("An unfound collectible has no finding"),
		Collectibles->GetCollectibleFinding(TestCollectibles::Plate, Missing));
	TestFalse(TEXT("An unfound collectible reports as unfound"),
		Collectibles->HasCollectibleBeenFound(TestCollectibles::Plate));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectibleRetention,
	"TerritoryFramework.Collectibles.Behavior.RetentionIsBoundedAndCounted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectibleRetention::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Collectible retention world exists"), World))
	{
		return false;
	}

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	if (!TestNotNull(TEXT("The collectible subsystem is created with its world"), Collectibles))
	{
		World->DestroyWorld(false);
		return false;
	}

	// A window small enough to evict inside one test. The limit is a designer setting,
	// and this case drives the same field the settings panel writes.
	Collectibles->MaxCollectibleFindings = 2;

	const TArray<FGameplayTag> Recorded = {
		TestCollectibles::Sword,
		TestCollectibles::Plate,
		TestCollectibles::LedgerPage,
		TestCollectibles::Recording,
		TestCollectibles::CityMap
	};

	for (const FGameplayTag& Tag : Recorded)
	{
		const FTerritoryCollectibleMutationResult Result = Collectibles->RecordCollectibleFound(
			Tag, FGameplayTag(), FGuid(),
			UTerritoryCollectibleSubsystem::MakeCollectibleContextId(Tag.ToString()));
		if (!Result.bSuccess)
		{
			AddError(FString::Printf(TEXT("Recording %s failed: %s"),
				*Tag.ToString(), *Result.FailureReason.ToString()));
		}
	}

	TestEqual(TEXT("The retained window stops at the configured limit"),
		Collectibles->GetFindingCount(), 2);
	TestEqual(TEXT("Eviction is counted rather than hidden"),
		Collectibles->GetEvictedFindingCount(), Recorded.Num() - 2);
	TestEqual(TEXT("The lifetime counter still reports every recorded finding"),
		Collectibles->GetTotalRecordedFindingCount(), Recorded.Num());

	// The newest findings survive: the Dossier shows the recent window, and a trimmed row
	// is reported through the eviction count instead of silently looking empty.
	const TArray<FTerritoryCollectibleFinding> Retained = Collectibles->GetCollectibleFindings();
	TestTrue(TEXT("The retained window starts at the fourth finding"),
		Retained[0].CollectibleTag == Recorded[3]);
	TestTrue(TEXT("The retained window ends at the newest finding"),
		Retained[1].CollectibleTag == Recorded[4]);
	TestFalse(TEXT("A trimmed finding no longer reports as found"),
		Collectibles->HasCollectibleBeenFound(TestCollectibles::Sword));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectibleProjection,
	"TerritoryFramework.Collectibles.Projection.WorldState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectibleProjection::RunTest(const FString& Parameters)
{
	// Same fixture as the evidence projection case: a registered world context, actors
	// initialized for play, then BeginPlay. The WorldState subscribes from its own
	// BeginPlay, so a world that never begins is not the world this projection runs in.
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Collectible projection world exists"), World))
	{
		GEngine->DestroyWorldContext(World);
		return false;
	}
	WorldContext.SetCurrentWorld(World);

	auto Teardown = [World]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryWorldState* State = World->SpawnActor<ATerritoryWorldState>(
		ATerritoryWorldState::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("The collectible subsystem is created with its world"), Collectibles)
		|| !TestNotNull(TEXT("A WorldState snapshot exists"), State))
	{
		Teardown();
		return false;
	}

	State->SetRole(ROLE_Authority);
	State->WorldStateGUID = FGuid(31, 32, 33, 34);

	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	State->DispatchBeginPlay();

	TestTrue(TEXT("The snapshot actor is on the authority"), State->HasAuthority());
	TestNotNull(TEXT("The live handler is reflectively callable"),
		State->FindFunction(TEXT("OnCollectibleFoundLive")));

	// The binding that ships: ATerritoryWorldState::BeginPlay -> SubscribeToLiveUpdates.
	TestTrue(TEXT("The WorldState bound the collectible delegate in BeginPlay"),
		Collectibles->OnCollectibleFound.Contains(State, TEXT("OnCollectibleFoundLive")));
	TestTrue(TEXT("The authority read model is ready before anything was found"),
		State->IsCollectibleReadModelReady());

	// A committed finding reaches the replicated read model.
	const FTerritoryCollectibleMutationResult Found = Collectibles->RecordCollectibleFound(
		TestCollectibles::Plate, FGameplayTag(), FGuid(),
		UTerritoryCollectibleSubsystem::MakeCollectibleContextId(TEXT("Plate")));
	TestTrue(TEXT("The projection case records a finding"), Found.bSuccess);

	const TArray<FTerritoryCollectibleFinding> Published = State->GetCollectibleFindings();
	TestEqual(TEXT("The live push published the new finding"), Published.Num(), 1);
	if (Published.Num() == 1)
	{
		TestTrue(TEXT("The published row is the committed row"),
			Published[0].CollectibleTag == TestCollectibles::Plate);
	}
	TestTrue(TEXT("The read model stays ready after a live finding"),
		State->IsCollectibleReadModelReady());

	// A rejected finding reaches nothing: no row, no counter, no published window change.
	const FTerritoryCollectibleMutationResult Rejected = Collectibles->RecordCollectibleFound(
		TestCollectibles::DocumentsCategory, FGameplayTag(), FGuid(), FGuid());
	TestFalse(TEXT("A category is still refused after a live finding"), Rejected.bSuccess);
	TestEqual(TEXT("A refused finding does not grow the published window"),
		State->GetCollectibleFindings().Num(), 1);

	Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectiblePersistence,
	"TerritoryFramework.Collectibles.Persistence.SaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectiblePersistence::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Collectible persistence world exists"), World))
	{
		return false;
	}

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	if (!TestNotNull(TEXT("The collectible subsystem is created with its world"), Collectibles))
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag Market = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	Collectibles->RecordCollectibleFound(TestCollectibles::Recording, Market, FGuid(41, 42, 43, 44),
		UTerritoryCollectibleSubsystem::MakeCollectibleContextId(TEXT("Recording")));
	// A row whose tag is no longer authored: the campaign found it before the content was
	// renamed, and it must survive the round trip instead of vanishing from the record.
	FTerritoryCollectibleFinding Retired;
	Retired.CollectibleTag = TestCollectibles::CityMap;
	Retired.ContextId = UTerritoryCollectibleSubsystem::MakeCollectibleContextId(TEXT("RetiredMap"));
	Retired.GameTimeSeconds = 1234.5;
	Collectibles->RecordCollectibleFinding(Retired);

	const TArray<FTerritoryCollectibleFinding> Saved = Collectibles->GetPersistentState();
	TestEqual(TEXT("Both findings are in the save boundary"), Saved.Num(), 2);

	// Restore into a fresh subsystem, the way a load does.
	UWorld* LoadedWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("The loaded world exists"), LoadedWorld))
	{
		World->DestroyWorld(false);
		return false;
	}
	UTerritoryCollectibleSubsystem* Loaded = LoadedWorld->GetSubsystem<UTerritoryCollectibleSubsystem>();
	if (!TestNotNull(TEXT("The loaded subsystem exists"), Loaded))
	{
		World->DestroyWorld(false);
		LoadedWorld->DestroyWorld(false);
		return false;
	}

	Loaded->RestorePersistentState(Saved);
	TestEqual(TEXT("The restored window carries every saved finding"), Loaded->GetFindingCount(), 2);
	TestTrue(TEXT("A restored finding reports as found"),
		Loaded->HasCollectibleBeenFound(TestCollectibles::Recording));
	TestTrue(TEXT("A finding whose tag is no longer authored still reports as found"),
		Loaded->HasCollectibleBeenFound(TestCollectibles::CityMap));
	TestTrue(TEXT("Restoring marks the read model known, so an empty list can be trusted"),
		Loaded->IsCollectibleLedgerReady());

	FTerritoryCollectibleFinding RestoredMap;
	TestTrue(TEXT("The retired row is readable by tag"),
		Loaded->GetCollectibleFinding(TestCollectibles::CityMap, RestoredMap));
	TestEqual(TEXT("The restored row keeps the original campaign time"),
		RestoredMap.GameTimeSeconds, 1234.5);

	World->DestroyWorld(false);
	LoadedWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCollectibleAuthority,
	"TerritoryFramework.Collectibles.Authority.ClientCannotRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCollectibleAuthority::RunTest(const FString& Parameters)
{
	// Same fixture as the evidence authority case: a registered world context, then a
	// BeginPlay, because the engine derives the net mode from the travel URL while the
	// world begins. Setting NextURL alone leaves the world standalone.
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Collectible authority world exists"), World))
	{
		GEngine->DestroyWorldContext(World);
		return false;
	}
	WorldContext.SetCurrentWorld(World);

	auto Teardown = [World]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	// TravelUrl "host:port" -> the engine derives NM_Client (Engine/Private/World.cpp,
	// InternalGetNetMode).
	World->NextURL = TEXT("127.0.0.1:7777");

	UTerritoryCollectibleSubsystem* Collectibles = World->GetSubsystem<UTerritoryCollectibleSubsystem>();
	if (!TestNotNull(TEXT("The client still creates the subsystem"), Collectibles))
	{
		Teardown();
		return false;
	}

	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	TestEqual(TEXT("The fixture world is a client"),
		static_cast<int32>(World->GetNetMode()), static_cast<int32>(NM_Client));

	const FTerritoryCollectibleMutationResult Result = Collectibles->RecordCollectibleFound(
		TestCollectibles::Sword, FGameplayTag(), FGuid(), FGuid());
	TestFalse(TEXT("A client cannot record a finding"), Result.bSuccess);
	TestTrue(TEXT("The client refusal explains itself"),
		Result.FailureReason.ToString().Contains(TEXT("server")));
	TestEqual(TEXT("A refused client write leaves no row"), Collectibles->GetFindingCount(), 0);
	TestFalse(TEXT("A client without a published window reports the read model as unknown"),
		Collectibles->IsCollectibleLedgerReady());

	Teardown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
