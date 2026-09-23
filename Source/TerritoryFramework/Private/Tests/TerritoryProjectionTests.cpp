#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

namespace TerritoryOwnershipProjection
{
	/**
	 * One durable ownership field and the replicated-summary field that must carry it.
	 *
	 * The pairs are spelled out rather than inferred because the mapping is semantic, not
	 * structural: `OwningFaction` is published as `CurrentOwner`. Reflection can find a field
	 * by name but cannot discover a rename, so the rename is declared here once and the test
	 * proves the declaration by comparing the two values, not by trusting the table.
	 */
	struct FProjectionRule
	{
		const TCHAR* DurableField;
		const TCHAR* SummaryField;
	};

	/** A durable field the directory deliberately does not carry, with the reason it does not. */
	struct FExclusionRule
	{
		const TCHAR* DurableField;
		const TCHAR* Reason;
	};

	/**
	 * The contract. Every reflected field of FTerritoryOwnershipData must appear in exactly one
	 * of these two tables; anything in neither is a test failure, so adding a durable field
	 * without deciding whether the directory carries it breaks the build instead of silently
	 * dropping out of the read model.
	 *
	 * CapturedBy and CapturedFor were precisely that omission. They have been written and saved
	 * since the betrayal beat was designed, but they were never published, so no client could
	 * answer "which Places did I win for the faction that betrayed me?" for a Place that World
	 * Partition had unloaded — the one case the summary exists to serve. Their sibling
	 * FormerOwningFactions, which is history of the same kind, was carried all along, which is
	 * what makes the omission look like an oversight rather than a decision.
	 */
	const FProjectionRule GProjected[] =
	{
		{ TEXT("OwningFaction"),        TEXT("CurrentOwner") },
		{ TEXT("FormerOwningFactions"), TEXT("FormerOwningFactions") },
		{ TEXT("CapturedBy"),           TEXT("CapturedBy") },
		{ TEXT("CapturedFor"),          TEXT("CapturedFor") },
		{ TEXT("State"),                TEXT("State") },
		{ TEXT("Availability"),         TEXT("Availability") },
		{ TEXT("ControlProgress"),      TEXT("ControlProgress") },
		{ TEXT("ContestingFaction"),    TEXT("ContestingFaction") },
	};

	const FExclusionRule GNotProjected[] =
	{
		{ TEXT("DefenderCount"),
			TEXT("Live garrison strength, recounted per query; not directory identity.") },
		{ TEXT("MaxConcurrentAttackers"),
			TEXT("Assault planning limit, read by the authority that schedules the attack.") },
		{ TEXT("PeriodicIncome"),
			TEXT("Economy tuning evaluated against the treasury, not a property of the row.") },
		{ TEXT("GuardCost"),
			TEXT("Economy tuning, as above.") },
		{ TEXT("GuardRecruitmentCost"),
			TEXT("Economy tuning, as above.") },
		{ TEXT("DesiredGuardCount"),
			TEXT("Garrison target for the owner's own accounting, not a directory fact.") },
		{ TEXT("LockReason"),
			TEXT("Author-facing FText; it already replicates inside OwnershipData on the volume.") },
	};

	/** Render a reflected value as text, so a mismatch prints both sides rather than just IDs. */
	FString ReadField(const FProperty* Property, const void* Container)
	{
		FString Text;
		Property->ExportText_InContainer(0, Text, Container, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	}

	/** Look up a reflected field by its C++ name; null when no such field is declared. */
	const FProperty* FindField(const UStruct* Struct, const TCHAR* Name)
	{
		return Struct->FindPropertyByName(FName(Name));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryOwnershipProjectionCoverage,
	"TerritoryFramework.WorldState.Regression.EveryDurableOwnershipFieldIsProjected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryOwnershipProjectionCoverage::RunTest(const FString& Parameters)
{
	using namespace TerritoryOwnershipProjection;

	const UScriptStruct* Durable = FTerritoryOwnershipData::StaticStruct();
	const UScriptStruct* Summary = FReplicatedCaptureSummary::StaticStruct();

	// ── 1. Every durable field has a declared projection decision ──
	// A field in neither table is the failure mode this test exists for, so it is counted and
	// reported per field rather than asserted as a bare total.
	int32 Undeclared = 0;
	for (TFieldIterator<FProperty> It(Durable); It; ++It)
	{
		const FString Name = It->GetName();
		bool bDeclared = false;
		for (const FProjectionRule& Rule : GProjected)
		{
			if (Name == Rule.DurableField) { bDeclared = true; break; }
		}
		if (!bDeclared)
		{
			for (const FExclusionRule& Rule : GNotProjected)
			{
				if (Name == Rule.DurableField) { bDeclared = true; break; }
			}
		}
		if (!bDeclared)
		{
			++Undeclared;
			AddError(FString::Printf(
				TEXT("Projection coverage: %s is neither published in FReplicatedCaptureSummary nor "
					 "listed as a documented exclusion. Decide which it is, and record why."), *Name));
		}
	}
	TestEqual(TEXT("Every durable ownership field has a declared projection decision"), Undeclared, 0);

	// ── 2. The declared names still exist, in both directions ──
	// Without this the tables rot into a lie: a renamed field would keep its entry and the
	// sweep above would report full coverage while nothing was actually compared.
	for (const FProjectionRule& Rule : GProjected)
	{
		TestTrue(FString::Printf(TEXT("%s is still a durable ownership field"), Rule.DurableField),
			FindField(Durable, Rule.DurableField) != nullptr);
		TestTrue(FString::Printf(TEXT("%s is still a published summary field"), Rule.SummaryField),
			FindField(Summary, Rule.SummaryField) != nullptr);
	}
	for (const FExclusionRule& Rule : GNotProjected)
	{
		TestTrue(FString::Printf(TEXT("%s is still a durable ownership field (exclusion: %s)"),
			Rule.DurableField, Rule.Reason),
			FindField(Durable, Rule.DurableField) != nullptr);
	}

	// ── 3. The directory carries the values the authority actually committed ──
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Projection test world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	// AGameModeBase rather than the project's Narrative game mode: the vendor save subsystem
	// asserts on a game state that does not implement GetActorGUID. Same reason the Dialogue
	// fixture uses it.
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);

	ATerritoryWorldState* State = World->SpawnActor<ATerritoryWorldState>();
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	const auto Teardown = [&]()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	if (!TestNotNull(TEXT("Projection fixture world state"), State)
		|| !TestNotNull(TEXT("Projection fixture territory"), Place))
	{
		Teardown();
		return false;
	}

	const FGameplayTag PlaceTag =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	const FGameplayTag Bandits =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Police =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Police"), false);
	if (!TestTrue(TEXT("Projection territory and faction fixtures resolve"),
		PlaceTag.IsValid() && Bandits.IsValid() && Heroes.IsValid() && Police.IsValid()))
	{
		Teardown();
		return false;
	}

	// The volume owns its own tag; there is no setter, so the fixture writes it the same way
	// the Dialogue fixture does.
	FStructProperty* TagProperty =
		FindFProperty<FStructProperty>(Place->GetClass(), TEXT("TerritoryTag"));
	if (!TestNotNull(TEXT("The territory exposes its own tag to the directory"), TagProperty))
	{
		Teardown();
		return false;
	}
	*TagProperty->ContainerPtrToValuePtr<FGameplayTag>(Place) = PlaceTag;

	// First tenure: Bandits claim the Place outright, so CapturedBy == CapturedFor. This is the
	// degenerate case the existing save test already pins.
	FTerritoryOwnershipData First = Place->GetOwnershipData();
	First.OwningFaction = Bandits;
	First.State = ETerritoryState::Claimed;
	First.ControlProgress = 1.f;
	TestTrue(TEXT("The Bandit tenure commits"), Place->CommitOwnershipData(First));

	// The discriminating case the provenance fields exist for: Heroes take the Place on the
	// Police's behalf, so CapturedBy (Heroes) and CapturedFor (Police) differ. A projection
	// that carried only the owner would publish this as a solo capture and lose the beat.
	FTerritoryOwnershipData Handover = Place->GetOwnershipData();
	Handover.OwningFaction = Police;
	Handover.State = ETerritoryState::Claimed;
	Handover.ControlProgress = 1.f;
	FTerritoryTransitionContext Context;
	Context.RequestingFaction = Heroes;
	TestTrue(TEXT("The handover commits"), Place->CommitOwnershipData(Handover, Context));

	// Prove the fixture before interpreting it: a probe that silently ran the degenerate case
	// would pass against the old code and prove nothing.
	const FTerritoryOwnershipData Committed = Place->GetOwnershipData();
	TestEqual(TEXT("Provenance records who physically took the Place"), Committed.CapturedBy, Heroes);
	TestEqual(TEXT("Provenance records who it was taken for"), Committed.CapturedFor, Police);
	TestNotEqual(TEXT("The probe distinguishes a handover from a solo capture"),
		Committed.CapturedBy, Committed.CapturedFor);

	const FReplicatedCaptureSummary Published = State->GetCaptureSummary(PlaceTag);

	int32 Uncomparable = 0;
	for (const FProjectionRule& Rule : GProjected)
	{
		const FProperty* Source = FindField(Durable, Rule.DurableField);
		const FProperty* Target = FindField(Summary, Rule.SummaryField);
		if (!Source || !Target)
		{
			++Uncomparable;
			AddError(FString::Printf(
				TEXT("Projection coverage: cannot compare %s -> %s (%s is not declared on %s)."),
				Rule.DurableField, Rule.SummaryField,
				Source ? Rule.SummaryField : Rule.DurableField,
				Source ? TEXT("FReplicatedCaptureSummary") : TEXT("FTerritoryOwnershipData")));
			continue;
		}
		TestEqual(FString::Printf(TEXT("The directory publishes %s as %s"),
			Rule.DurableField, Rule.SummaryField),
			ReadField(Target, &Published), ReadField(Source, &Committed));
	}
	TestEqual(TEXT("Every projected field was comparable by reflection"), Uncomparable, 0);

	Teardown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
