#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryDiplomacyTypes.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"

/**
 * AGENTS.md §5.2: "Never map Narrative Friendly to a Territory state that writes back as
 * Neutral."
 *
 * AttitudeToDiplomacyState is the public Narrative → Territory direction and is covered
 * directly below. DiplomacyStateToAttitude, the write-back the clause is actually about, is
 * private (TerritoryDiplomacySubsystem.h:192 sits under the private section at :186), so the
 * second test reaches it the way gameplay does — through LoadFromGameState and SyncToGameState.
 *
 * The suite previously covered neither: TerritoryFrameworkTests.cpp:2661-2675 is a comment plus
 * three assertions on ETeamAttitude's engine numbering, which cannot fail for a wrong mapping.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyAttitudeToStateMapping,
	"TerritoryFramework.Diplomacy.AttitudeMapping.FriendlyNeverWritesBackAsNeutral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyAttitudeToStateMapping::RunTest(const FString& Parameters)
{
	// AttitudeToDiplomacyState is a const pure switch over its argument and never reads world
	// state, so no world fixture is needed and none is invented here.
	UTerritoryDiplomacySubsystem* Diplomacy = NewObject<UTerritoryDiplomacySubsystem>();
	if (!TestNotNull(TEXT("Diplomacy subsystem is constructible for the mapping bridge"), Diplomacy))
	{
		return false;
	}

	// The named states, pinned explicitly. Friendly resolving to Anything-but-Alliance is the
	// failure the clause names, because only Alliance is the state that reads back as Friendly.
	TestEqual(TEXT("Friendly resolves to Alliance, not to a treaty-free default"),
		Diplomacy->AttitudeToDiplomacyState(ETeamAttitude::Friendly), EDiplomacyState::Alliance);
	TestEqual(TEXT("Hostile resolves to War"),
		Diplomacy->AttitudeToDiplomacyState(ETeamAttitude::Hostile), EDiplomacyState::War);
	TestEqual(TEXT("Neutral resolves to no rich treaty"),
		Diplomacy->AttitudeToDiplomacyState(ETeamAttitude::Neutral), EDiplomacyState::None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyFriendlyRoundTripPreservesRichTreaty,
	"TerritoryFramework.Diplomacy.AttitudeMapping.FriendlyRoundTripPreservesRichTreaty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyFriendlyRoundTripPreservesRichTreaty::RunTest(const FString& Parameters)
{
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Police = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Police"), false);
	if (!TestTrue(TEXT("Test faction tags resolve"),
		Bandits.IsValid() && Heroes.IsValid() && Police.IsValid()))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Diplomacy round-trip world exists"), World))
	{
		return false;
	}
	WorldContext.SetCurrentWorld(World);

	auto Teardown = [World]()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	// NewObject rather than SpawnActor: SpawnActor fires the vendor save subsystem's
	// OnActorSpawned handler, which asserts for actors implementing the stable-actor interface
	// without a native GetActorGUID. The GameState only has to exist as the world's GameState
	// for the adapter to read it.
	ANarrativeGameState* GameState = NewObject<ANarrativeGameState>(World->PersistentLevel);
	if (!TestNotNull(TEXT("Narrative GameState fixture exists"), GameState))
	{
		Teardown();
		return false;
	}

	// Symmetric pairs. Symmetry matters: RefreshFromGameState only preserves existing rich
	// metadata when the pair agrees or the record is Narrative-observed, so an asymmetric
	// fixture would test a different branch.
	FFactionAttitudeData BanditView;
	BanditView.AttitudeMap.Add(Heroes, ETeamAttitude::Friendly);
	GameState->FactionAllianceMap.Add(Bandits, BanditView);
	FFactionAttitudeData HeroView;
	HeroView.AttitudeMap.Add(Bandits, ETeamAttitude::Friendly);
	HeroView.AttitudeMap.Add(Police, ETeamAttitude::Hostile);
	GameState->FactionAllianceMap.Add(Heroes, HeroView);
	FFactionAttitudeData PoliceView;
	PoliceView.AttitudeMap.Add(Heroes, ETeamAttitude::Hostile);
	GameState->FactionAllianceMap.Add(Police, PoliceView);
	World->SetGameState(GameState);

	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!TestNotNull(TEXT("Diplomacy subsystem exists"), Diplomacy))
	{
		Teardown();
		return false;
	}

	// Territory authors the richer friendly state that Narrative cannot express.
	Diplomacy->SignTradeAgreement(Heroes, Bandits);
	if (!TestEqual(TEXT("Fixture authored a rich trade agreement"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::TradeAgreement))
	{
		Teardown();
		return false;
	}

	// ── Narrative → Territory, over an existing rich treaty ──
	// RefreshFromGameState preserves an existing treaty when its write-back attitude already
	// matches Narrative's. The comparison runs through the private DiplomacyStateToAttitude, so
	// a TradeAgreement that no longer reads back as Friendly fails the comparison and the record
	// is reconciled away from the rich state — silently discarding the trade terms. The treaty
	// is not merely presented wrong; the metadata is gone.
	//
	// Narrative re-asserts the friendship here the way a story event does: independently of the
	// treaty, and through Narrative's own authority. Without this step the assertion is
	// unfalsifiable, because SignTradeAgreement writes Narrative through the very write-back
	// under test and the two sides therefore always agree at this point.
	Diplomacy->SetNarrativeAttitude(Heroes, Bandits, ETeamAttitude::Friendly);
	Diplomacy->LoadFromGameState();

	// Premise control. Every assertion below is vacuous if RefreshFromGameState did not run —
	// an early return leaves the rich treaty untouched and the preservation assertion would pass
	// while covering nothing. The hostile control pair proves the function read Narrative's map
	// and reconciled it: it can only reach War by having observed the authored hostility.
	if (!TestEqual(TEXT("Premise: the attitude re-read observed Narrative's map"),
		Diplomacy->GetDiplomacyState(Heroes, Police), EDiplomacyState::War))
	{
		Teardown();
		return false;
	}

	TestEqual(TEXT("A narrative attitude re-read does not degrade a rich friendly treaty"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::TradeAgreement);

	// ── Territory → Narrative, the direction the clause names ──
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::Alliance);
	if (!TestEqual(TEXT("Fixture authored an alliance"),
		Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::Alliance))
	{
		Teardown();
		return false;
	}

	Diplomacy->SyncToGameState();

	// Read Narrative's own storage rather than the adapter that performs the write, so the
	// assertion is independent of the bridge under test. Find is used deliberately: the
	// attitude enum's zero value is Friendly, so FindRef would report a missing pair as the
	// passing answer.
	const FFactionAttitudeData* NarrativeView = GameState->FactionAllianceMap.Find(Heroes);
	if (TestNotNull(TEXT("Narrative still holds the observing faction's attitude map"), NarrativeView))
	{
		const TEnumAsByte<ETeamAttitude::Type>* WrittenBack = NarrativeView->AttitudeMap.Find(Bandits);
		if (TestNotNull(TEXT("Narrative holds a directional attitude for the pair"), WrittenBack))
		{
			// The AGENTS.md §5.2 assertion. An Alliance that writes back as Neutral would strip
			// the faction of its friendliness in Narrative's own AI attitude map, which is the
			// authority for guard and settlement behaviour.
			TestEqual(TEXT("An alliance reaches Narrative AI as Friendly, not as Neutral"),
				WrittenBack->GetValue(), ETeamAttitude::Friendly);
		}
	}

	Teardown();
	return true;
}

#endif
