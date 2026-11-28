#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Combat/TerritoryCounterAttackTypes.h"
#include "Core/TerritoryVolume.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryStoryConditions.h"
#include "Tales/TerritoryStoryEvents.h"
#include "UObject/UnrealType.h"
#include "UnrealFramework/NarrativeGameState.h"

/**
 * Authored deployment delay: a timed story beat warns the player now and holds the force until the
 * declared campaign-time window opens. These tests pin the scheduler side (the record's window and
 * the queries a story graph reads) because the timer itself runs inside AdvanceAssault.
 */
namespace TerritoryAssaultDeploymentDelayTests
{
	struct FAssaultFixture
	{
		UWorld* World = nullptr;
		UTerritoryCounterAttackSubsystem* Counter = nullptr;
		UTerritoryDiplomacySubsystem* Diplomacy = nullptr;
		ATerritoryProperty* Territory = nullptr;
		FGameplayTag Heroes;
		FGameplayTag Bandits;
		FGameplayTag TerritoryTag;

		bool Create()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) return false;
			// The admission gate requires the campaign clock, so the fixture installs the Narrative
			// Game State exactly like the other counterattack fixtures do.
			ANarrativeGameState* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
			Clock->SetRole(ROLE_Authority);
			World->SetGameState(Clock);
			Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
			Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
			Territory = World->SpawnActor<ATerritoryProperty>();
			if (!Counter || !Diplomacy || !Territory) return false;

			Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"), false);
			Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"), false);
			if (!Heroes.IsValid() || !Bandits.IsValid()) return false;

			// ATerritoryVolume keeps its identity tag protected, so the fixture writes it through
			// reflection the same way the other counterattack fixtures do.
			TerritoryTag =
				FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
			FStructProperty* TagProperty = FindFProperty<FStructProperty>(
				ATerritoryVolume::StaticClass(), TEXT("TerritoryTag"));
			if (!TagProperty) return false;
			*TagProperty->ContainerPtrToValuePtr<FGameplayTag>(Territory) = TerritoryTag;
			Territory->SetActorGUID_Implementation(FGuid::NewGuid());
			if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
			{
				Registry->RegisterTerritory(Territory);
			}
			FTerritoryOwnershipData Claimed;
			Claimed.OwningFaction = Heroes;
			Claimed.State = ETerritoryState::Claimed;
			Territory->CommitOwnershipData(Claimed);
			Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::War);

			UTerritoryCounterAttackProfile* Profile =
				NewObject<UTerritoryCounterAttackProfile>(Territory);
			UNPCDefinition* AttackerDefinition = NewObject<UNPCDefinition>(Profile);
			AttackerDefinition->CharacterID = TEXT("TF_DeploymentDelay_Character");
			AttackerDefinition->NPCID = TEXT("TF_DeploymentDelay_NPC");
			AttackerDefinition->bAllowMultipleInstances = true;
			AttackerDefinition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
			FTerritoryFactionAssaultConfig& Force = Profile->FactionForces.AddDefaulted_GetRef();
			Force.Faction = Bandits;
			Force.AttackerDefinition = AttackerDefinition;
			Force.PlannedForce = 2;
			Force.WaveSize = 2;
			Force.MilitaryPower = 100.f;
			FObjectProperty* ProfileProperty = FindFProperty<FObjectProperty>(
				ATerritoryVolume::StaticClass(), TEXT("CounterAttackProfile"));
			if (!ProfileProperty) return false;
			ProfileProperty->SetObjectPropertyValue_InContainer(Territory, Profile);
			return true;
		}

		~FAssaultFixture()
		{
			if (World) World->DestroyWorld(false);
		}

		/** Copies the newest durable record for this territory; the caller owns the storage. */
		bool TryGetLatestRecord(FTerritoryAssaultRecord& Out) const
		{
			const TArray<FTerritoryAssaultRecord> Records =
				Counter->GetAssaultsForTerritoryActor(Territory);
			if (Records.IsEmpty()) return false;
			Out = Records.Last();
			return true;
		}
	};

	static FTerritoryStoryPursuitOptions MakeOptions()
	{
		FTerritoryStoryPursuitOptions Options;
		Options.ScenarioID = TEXT("DeploymentDelay_Test");
		return Options;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultDeploymentDelaySchedule,
	"TerritoryFramework.CounterAttack.Behavior.AuthoredDeploymentDelaySetsTheWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultDeploymentDelaySchedule::RunTest(const FString& Parameters)
{
	using namespace TerritoryAssaultDeploymentDelayTests;
	FAssaultFixture Fixture;
	if (!TestTrue(TEXT("Deployment-delay fixture built"), Fixture.Create()))
	{
		return false;
	}

	FText Reason;
	const bool bTimed = Fixture.Counter->TryScheduleAssaultAdvancedWithReason(Fixture.Territory,
		Fixture.Bandits, ETerritoryAssaultLaunchMode::StoryPursuit, MakeOptions(),
		/*bStartImmediately=*/false, /*DeploymentDelaySeconds=*/90.f, Reason);
	TestTrue(*FString::Printf(TEXT("A timed story wave is admitted: %s"), *Reason.ToString()), bTimed);
	FTerritoryAssaultRecord Timed;
	if (!TestTrue(TEXT("The timed wave produced a durable record"), Fixture.TryGetLatestRecord(Timed)))
	{
		return false;
	}
	TestEqual(TEXT("A timed wave waits in its grace period first"),
		static_cast<int32>(Timed.State), static_cast<int32>(ETerritoryAssaultState::Grace));
	// The warned state is only reachable after the grace period, so the authored wait is measured
	// from the grace end. Measuring it from the capture stamp made a delay shorter than the grace a
	// silent no-op, because the window had already expired by the time the force could be warned.
	TestTrue(TEXT("The authored window opens after the grace period, not at the capture stamp"),
		Timed.DeploymentOpensGameTime > Timed.GraceEndsGameTime);
	TestEqual(TEXT("The authored window opens one delay after the grace period ends"),
		Timed.DeploymentOpensGameTime, Timed.GraceEndsGameTime + 90.0, 0.05);
	const double StampedWindow = Timed.DeploymentOpensGameTime;

	// The same authored event without a delay keeps the engine's own timing, so every existing
	// story graph and every automatic strategic schedule behaves exactly as before.
	FAssaultFixture Plain;
	if (!TestTrue(TEXT("Plain fixture built"), Plain.Create())) return false;
	FText PlainReason;
	TestTrue(TEXT("An undelayed story wave is still admitted"),
		Plain.Counter->TryScheduleAssaultAdvancedWithReason(Plain.Territory, Plain.Bandits,
			ETerritoryAssaultLaunchMode::StoryPursuit, MakeOptions(), false, /*DeploymentDelaySeconds=*/0.f,
			PlainReason));
	FTerritoryAssaultRecord Undelayed;
	if (TestTrue(TEXT("The undelayed wave produced a durable record"), Plain.TryGetLatestRecord(Undelayed)))
	{
		TestEqual(TEXT("An undelayed wave carries no authored window"), Undelayed.DeploymentOpensGameTime, 0.0);
	}

	// Immediate deployment is the instant variant of the same event: it must never carry a window.
	// The synthetic fixture has no authored route (no navmesh in a unit world), so the immediate
	// variant is refused at schedule time by the same route/force evaluation the live game runs --
	// and that refusal must still prove the authored delay was ignored rather than parked in Grace.
	FAssaultFixture Immediate;
	if (!TestTrue(TEXT("Immediate fixture built"), Immediate.Create())) return false;
	FText ImmediateReason;
	const bool bImmediateAdmitted = Immediate.Counter->TryScheduleAssaultAdvancedWithReason(
		Immediate.Territory, Immediate.Bandits,
		ETerritoryAssaultLaunchMode::StoryPursuit, MakeOptions(), /*bStartImmediately=*/true,
		/*DeploymentDelaySeconds=*/90.f, ImmediateReason);
	TestFalse(*FString::Printf(TEXT("An immediate wave without a deployable route is refused: %s"),
		*ImmediateReason.ToString()), bImmediateAdmitted);
	TestTrue(TEXT("The refusal reports the route/force evaluation outcome"),
		ImmediateReason.ToString().Contains(TEXT("route/force evaluation")));
	FTerritoryAssaultRecord Now;
	if (TestTrue(TEXT("The immediate wave produced a durable record"), Immediate.TryGetLatestRecord(Now)))
	{
		TestEqual(TEXT("Immediate deployment ignores an authored delay"),
			Now.DeploymentOpensGameTime, 0.0);
		TestTrue(TEXT("A refused immediate wave is terminal instead of waiting in grace"),
			Now.IsTerminal());
	}

	// The window is durable campaign state: a save/restore round trip must keep the same deadline.
	const TArray<FTerritoryAssaultRecord> Saved = Fixture.Counter->GetPersistentState();
	Fixture.Counter->RestorePersistentState(Saved);
	FTerritoryAssaultRecord Restored;
	if (TestTrue(TEXT("The restored wave still exists"), Fixture.TryGetLatestRecord(Restored)))
	{
		TestEqual(TEXT("Save/load keeps the authored deployment window"),
			Restored.DeploymentOpensGameTime, StampedWindow, 0.05);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultDeploymentDelayCondition,
	"TerritoryFramework.CounterAttack.Behavior.DeploymentTimingQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultDeploymentDelayCondition::RunTest(const FString& Parameters)
{
	using namespace TerritoryAssaultDeploymentDelayTests;
	FAssaultFixture Fixture;
	if (!TestTrue(TEXT("Timing-query fixture built"), Fixture.Create()))
	{
		return false;
	}
	FText Reason;
	if (!TestTrue(TEXT("A timed story wave is admitted for the query test"),
		Fixture.Counter->TryScheduleAssaultAdvancedWithReason(Fixture.Territory, Fixture.Bandits,
			ETerritoryAssaultLaunchMode::StoryPursuit, MakeOptions(), false,
			/*DeploymentDelaySeconds=*/120.f, Reason)))
	{
		return false;
	}

	UTerritoryAssaultCondition* Condition = NewObject<UTerritoryAssaultCondition>(Fixture.Territory);
	Condition->TerritoryToCheck = Fixture.TerritoryTag;
	Condition->AttackingFaction = Fixture.Bandits;
	Condition->ScenarioID = TEXT("DeploymentDelay_Test");

	Condition->Query = ETerritoryAssaultConditionQuery::DeploymentWindowOpen;
	TestFalse(TEXT("A warned force inside its authored window is not open yet"),
		Condition->CheckConditionInContext(nullptr, nullptr, nullptr));

	Condition->Query = ETerritoryAssaultConditionQuery::SecondsUntilDeployment;
	// The countdown is read against the record's own window instead of a guessed constant: this
	// fixture's influence-adjusted grace can legitimately be zero, so the authored 120 s delay is
	// the whole wait here and any absolute threshold would encode a wrong assumption.
	FTerritoryAssaultRecord TimedRecord;
	if (!TestTrue(TEXT("The query fixture carries its authored record"),
		Fixture.TryGetLatestRecord(TimedRecord)))
	{
		return false;
	}
	const float Countdown = static_cast<float>(
		TimedRecord.DeploymentOpensGameTime - Fixture.Counter->GetCampaignGameTime());
	TestTrue(TEXT("The authored window is a real wait, not an already-expired stamp"),
		Countdown > 0.f);

	Condition->Comparison = ETerritoryIntegerComparison::AtLeast;
	Condition->SecondsThreshold = Countdown - 1.f;
	TestTrue(TEXT("The authored window reads at least its own remaining seconds"),
		Condition->CheckConditionInContext(nullptr, nullptr, nullptr));
	Condition->SecondsThreshold = 86400.f;
	TestFalse(TEXT("The authored window never reads a full day remaining"),
		Condition->CheckConditionInContext(nullptr, nullptr, nullptr));
	Condition->Comparison = ETerritoryIntegerComparison::AtMost;
	Condition->SecondsThreshold = Countdown + 1.f;
	TestTrue(TEXT("The authored window reads at most its own remaining seconds"),
		Condition->CheckConditionInContext(nullptr, nullptr, nullptr));
	Condition->Comparison = ETerritoryIntegerComparison::AtLeast;
	Condition->SecondsThreshold = Countdown + 1.f;
	TestFalse(TEXT("The authored window does not read more time than it has left"),
		Condition->CheckConditionInContext(nullptr, nullptr, nullptr));

	// The undelayed record reads as already open with nothing left to wait for, so a story graph
	// written against these queries keeps working for every assault that has no authored delay.
	FAssaultFixture Plain;
	if (!TestTrue(TEXT("Plain query fixture built"), Plain.Create())) return false;
	FText PlainReason;
	TestTrue(TEXT("An undelayed wave is admitted for the query test"),
		Plain.Counter->TryScheduleAssaultAdvancedWithReason(Plain.Territory, Plain.Bandits,
			ETerritoryAssaultLaunchMode::StoryPursuit, MakeOptions(), false, 0.f, PlainReason));
	UTerritoryAssaultCondition* PlainCondition = NewObject<UTerritoryAssaultCondition>(Plain.Territory);
	PlainCondition->TerritoryToCheck = Plain.TerritoryTag;
	PlainCondition->AttackingFaction = Plain.Bandits;
	PlainCondition->ScenarioID = TEXT("DeploymentDelay_Test");
	PlainCondition->Query = ETerritoryAssaultConditionQuery::DeploymentWindowOpen;
	TestTrue(TEXT("An undelayed assault is open immediately"),
		PlainCondition->CheckConditionInContext(nullptr, nullptr, nullptr));
	PlainCondition->Query = ETerritoryAssaultConditionQuery::SecondsUntilDeployment;
	PlainCondition->Comparison = ETerritoryIntegerComparison::Equal;
	PlainCondition->SecondsThreshold = 0.f;
	TestTrue(TEXT("An undelayed assault has zero seconds remaining"),
		PlainCondition->CheckConditionInContext(nullptr, nullptr, nullptr));

	// A condition with no matching record still fails closed.
	UTerritoryAssaultCondition* Missing = NewObject<UTerritoryAssaultCondition>(Plain.Territory);
	Missing->TerritoryToCheck = Plain.TerritoryTag;
	Missing->AttackingFaction = Plain.Heroes;
	Missing->Query = ETerritoryAssaultConditionQuery::DeploymentWindowOpen;
	TestFalse(TEXT("A missing assault record never reports an open deployment window"),
		Missing->CheckConditionInContext(nullptr, nullptr, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultDeploymentDelayContract,
	"TerritoryFramework.CounterAttack.Contract.DeploymentDelayAuthoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultDeploymentDelayContract::RunTest(const FString& Parameters)
{
	// Authored event surface: designers get both story beats on one event.
	const FProperty* DelayProperty = FindFProperty<FProperty>(
		UTerritoryScheduleEnemyWaveEvent::StaticClass(), TEXT("DeploymentDelaySeconds"));
	TestNotNull(TEXT("The enemy wave event exposes an authored deployment delay"), DelayProperty);
	if (DelayProperty)
	{
		TestTrue(TEXT("The deployment delay is designer-editable"),
			DelayProperty->HasAnyPropertyFlags(CPF_Edit));
		TestEqual(TEXT("The deployment delay is a float in campaign seconds"),
			FString(DelayProperty->GetCPPType()), FString(TEXT("float")));
	}
	const FProperty* ImmediateProperty = FindFProperty<FProperty>(
		UTerritoryScheduleEnemyWaveEvent::StaticClass(), TEXT("bStartImmediately"));
	TestNotNull(TEXT("The instant variant of the same event stays available"), ImmediateProperty);

	// Durable record surface: the window persists, replicates and is readable by story logic.
	const FProperty* WindowProperty = FindFProperty<FProperty>(
		FTerritoryAssaultRecord::StaticStruct(), TEXT("DeploymentOpensGameTime"));
	TestNotNull(TEXT("The assault record stores the authored deployment window"), WindowProperty);
	if (WindowProperty)
	{
		TestTrue(TEXT("The deployment window is saved"),
			WindowProperty->HasAnyPropertyFlags(CPF_SaveGame));
	}

	// Condition surface: a story graph can ask about the timing.
	const UEnum* QueryEnum = StaticEnum<ETerritoryAssaultConditionQuery>();
	TestNotNull(TEXT("The assault condition query enum exists"), QueryEnum);
	if (QueryEnum)
	{
		TestTrue(TEXT("Story graphs can ask whether the deployment window is open"),
			QueryEnum->GetValueByNameString(TEXT("DeploymentWindowOpen")) != INDEX_NONE);
		TestTrue(TEXT("Story graphs can ask how long the countdown has left"),
			QueryEnum->GetValueByNameString(TEXT("SecondsUntilDeployment")) != INDEX_NONE);
	}
	const FProperty* ThresholdProperty = FindFProperty<FProperty>(
		UTerritoryAssaultCondition::StaticClass(), TEXT("SecondsThreshold"));
	TestNotNull(TEXT("The condition carries a seconds threshold beside the integer value"),
		ThresholdProperty);
	return true;
}

/**
 * The authored window has to actually hold the warned force: storing a deadline is not the feature.
 * This drives a real scheduled record through the same update tick the game runs, so the state
 * machine is covered instead of only the schedule-time stamp. The earlier suite never left the
 * record's grace period, which is exactly how a silently ignored delay escaped it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultDeploymentHold,
	"TerritoryFramework.CounterAttack.Regression.DeploymentWindowHoldsTheWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultDeploymentHold::RunTest(const FString& Parameters)
{
	using namespace TerritoryAssaultDeploymentDelayTests;
	FAssaultFixture Fixture;
	if (!TestTrue(TEXT("Deployment-hold fixture built"), Fixture.Create()))
	{
		return false;
	}
	FText Reason;
	// A grace far longer than the delay is the case that used to break: the window was stamped from
	// the capture, so it expired during the grace and the warned force was never actually held.
	FTerritoryStoryPursuitOptions HoldOptions = MakeOptions();
	HoldOptions.GracePeriodOverrideGameTime = 3600.f;
	if (!TestTrue(TEXT("A timed story wave is admitted for the hold test"),
		Fixture.Counter->TryScheduleAssaultAdvancedWithReason(Fixture.Territory, Fixture.Bandits,
			ETerritoryAssaultLaunchMode::StoryPursuit, HoldOptions, /*bStartImmediately=*/false,
			/*DeploymentDelaySeconds=*/90.f, Reason)))
	{
		return false;
	}
	FTerritoryAssaultRecord Record;
	if (!TestTrue(TEXT("The timed wave produced a durable record"), Fixture.TryGetLatestRecord(Record)))
	{
		return false;
	}

	// The window is anchored after the grace because the warned state is only reachable then.
	// The guarded assertion is the one the old capture-relative stamp failed.
	const double EffectiveGrace = Record.GraceEndsGameTime - Record.CapturedGameTime;
	if (EffectiveGrace > 90.0)
	{
		TestTrue(*FString::Printf(
			TEXT("A delay (90 s) shorter than the grace (%.0f s) still opens after the grace"),
			EffectiveGrace), Record.DeploymentOpensGameTime > Record.GraceEndsGameTime);
	}
	TestEqual(TEXT("The authored window opens one delay after the grace period ends"),
		Record.DeploymentOpensGameTime, Record.GraceEndsGameTime + 90.0, 0.05);
	TestTrue(TEXT("A window in the future holds the warned force"),
		UTerritoryCounterAttackSubsystem::IsDeploymentWindowHolding(Record, Record.GraceEndsGameTime));
	TestFalse(TEXT("The hold ends the moment the campaign clock reaches the window"),
		UTerritoryCounterAttackSubsystem::IsDeploymentWindowHolding(
			Record, Record.DeploymentOpensGameTime));

	// Zero stays the documented "no authored window" value for automatic and legacy schedules.
	FTerritoryAssaultRecord Undelayed;
	Undelayed.AssaultID = FGuid::NewGuid();
	Undelayed.DeploymentOpensGameTime = 0.0;
	TestFalse(TEXT("A record without an authored window is never held"),
		UTerritoryCounterAttackSubsystem::IsDeploymentWindowHolding(Undelayed, 0.0));

	// Now the state machine itself: the tick must leave a held record in its warning...
	FTerritoryAssaultRecord& Live = Fixture.Counter->Assaults.FindChecked(Record.AssaultID);
	Live.State = ETerritoryAssaultState::ScheduledWarning;
	Fixture.Counter->AdvanceAssault(Live);
	TestEqual(TEXT("A held warning stays in ScheduledWarning until its window opens"),
		static_cast<int32>(Fixture.Counter->Assaults.FindChecked(Record.AssaultID).State),
		static_cast<int32>(ETerritoryAssaultState::ScheduledWarning));

	// ...and release it as soon as nothing holds it any more.
	Fixture.Counter->Assaults.FindChecked(Record.AssaultID).DeploymentOpensGameTime = 0.0;
	FTerritoryAssaultRecord& Released = Fixture.Counter->Assaults.FindChecked(Record.AssaultID);
	Fixture.Counter->AdvanceAssault(Released);
	TestEqual(TEXT("A warning with no open window advances to the proximity policy"),
		static_cast<int32>(Fixture.Counter->Assaults.FindChecked(Record.AssaultID).State),
		static_cast<int32>(ETerritoryAssaultState::WaitingForPlayerProximity));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
