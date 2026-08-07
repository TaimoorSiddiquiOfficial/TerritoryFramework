#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/TerritoryAssaultActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

namespace TerritoryCounterAttackTests
{
	static FTerritoryAssaultEvaluationInput MakeBaselineInput()
	{
		FTerritoryAssaultEvaluationInput Input;
		Input.ActiveGuards = 3;
		Input.DesiredGuards = 6;
		Input.MaximumGuards = 10;
		Input.ReserveGuards = 2;
		Input.GuardQuality = 4.f;
		Input.Fortification = 20.f;
		Input.NearbyAlliedSupport = 10.f;
		Input.AttackingMilitaryPower = 120.f;
		Input.EconomyReadiness = 0.8f;
		Input.SupplyReadiness = 0.7f;
		Input.StrategicValue = 2.f;
		Input.RecentMomentum = 0.2f;
		return Input;
	}

	static bool HasFunctionFlag(const UClass* Class, FName Name, EFunctionFlags Flag)
	{
		const UFunction* Function = Class ? Class->FindFunctionByName(Name) : nullptr;
		return Function && Function->HasAnyFunctionFlags(Flag);
	}

	static bool HasPropertyFlag(const UStruct* Struct, FName Name, EPropertyFlags Flag)
	{
		const FProperty* Property = Struct ? Struct->FindPropertyByName(Name) : nullptr;
		return Property && Property->HasAnyPropertyFlags(Flag);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackDeterminism,
	"TerritoryFramework.CounterAttack.Behavior.DeterministicEvaluation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackDeterminism::RunTest(const FString& Parameters)
{
	const UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	const FTerritoryAssaultEvaluationInput Input = TerritoryCounterAttackTests::MakeBaselineInput();
	const FTerritoryAssaultEvaluationResult First =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Input, Profile);
	const FTerritoryAssaultEvaluationResult Second =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Input, Profile);

	TestEqual(TEXT("Defence power is deterministic"), First.DistrictDefencePower, Second.DistrictDefencePower);
	TestEqual(TEXT("Power ratio is deterministic"), First.PowerRatio, Second.PowerRatio);
	TestEqual(TEXT("Priority is deterministic"), First.AttackPriority, Second.AttackPriority);
	TestEqual(TEXT("Launch probability is deterministic"), First.LaunchProbability, Second.LaunchProbability);
	TestEqual(TEXT("Estimated success is deterministic"), First.EstimatedSuccessProbability, Second.EstimatedSuccessProbability);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackDefenceMonotonicity,
	"TerritoryFramework.CounterAttack.Behavior.DefenceMonotonicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackDefenceMonotonicity::RunTest(const FString& Parameters)
{
	const UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	FTerritoryAssaultEvaluationInput Input = TerritoryCounterAttackTests::MakeBaselineInput();
	Input.DesiredGuards = 10;
	Input.MaximumGuards = 10;

	float PreviousPriority = TNumericLimits<float>::Max();
	float PreviousLaunch = TNumericLimits<float>::Max();
	for (int32 Guards = 0; Guards <= Input.MaximumGuards; ++Guards)
	{
		Input.ActiveGuards = Guards;
		const FTerritoryAssaultEvaluationResult Result =
			UTerritoryCounterAttackSubsystem::CalculateEvaluation(Input, Profile);
		TestTrue(FString::Printf(TEXT("%d guards do not increase priority"), Guards),
			Result.AttackPriority <= PreviousPriority + KINDA_SMALL_NUMBER);
		TestTrue(FString::Printf(TEXT("%d guards do not increase launch probability"), Guards),
			Result.LaunchProbability <= PreviousLaunch + KINDA_SMALL_NUMBER);
		PreviousPriority = Result.AttackPriority;
		PreviousLaunch = Result.LaunchProbability;
	}

	auto TestDefenceDimension = [this, Profile](const TCHAR* Label, auto Mutate)
	{
		FTerritoryAssaultEvaluationInput Low = TerritoryCounterAttackTests::MakeBaselineInput();
		FTerritoryAssaultEvaluationInput High = Low;
		Mutate(Low, High);
		const FTerritoryAssaultEvaluationResult LowResult =
			UTerritoryCounterAttackSubsystem::CalculateEvaluation(Low, Profile);
		const FTerritoryAssaultEvaluationResult HighResult =
			UTerritoryCounterAttackSubsystem::CalculateEvaluation(High, Profile);
		TestTrue(FString::Printf(TEXT("More %s does not increase launch probability"), Label),
			HighResult.LaunchProbability <= LowResult.LaunchProbability + KINDA_SMALL_NUMBER);
		TestTrue(FString::Printf(TEXT("More %s does not increase priority"), Label),
			HighResult.AttackPriority <= LowResult.AttackPriority + KINDA_SMALL_NUMBER);
	};
	TestDefenceDimension(TEXT("reserve guards"), [](auto& Low, auto& High)
	{
		Low.ReserveGuards = 0; High.ReserveGuards = 8;
	});
	TestDefenceDimension(TEXT("guard quality"), [](auto& Low, auto& High)
	{
		Low.GuardQuality = 1.f; High.GuardQuality = 8.f;
	});
	TestDefenceDimension(TEXT("fortification"), [](auto& Low, auto& High)
	{
		Low.Fortification = 0.f; High.Fortification = 100.f;
	});
	TestDefenceDimension(TEXT("allied support"), [](auto& Low, auto& High)
	{
		Low.NearbyAlliedSupport = 0.f; High.NearbyAlliedSupport = 100.f;
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackUnguardedGuarantee,
	"TerritoryFramework.CounterAttack.Regression.UnguardedCascadeLaunchesAfterDiplomacyAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackUnguardedGuarantee::RunTest(const FString& Parameters)
{
	UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	Profile->UnguardedLaunchProbability = 1.f;
	FTerritoryAssaultEvaluationInput Empty = TerritoryCounterAttackTests::MakeBaselineInput();
	Empty.ActiveGuards = 0;
	Empty.DesiredGuards = 0;
	Empty.ReserveGuards = 0;
	Empty.Fortification = 0.f;
	Empty.NearbyAlliedSupport = 0.f;
	const FTerritoryAssaultEvaluationResult EmptyResult =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Empty, Profile);
	TestEqual(TEXT("No active guards uses the explicit unguarded launch policy"),
		EmptyResult.LaunchProbability, 1.f);
	TestTrue(TEXT("Estimated success remains bounded planning data and is not used as an ownership roll"),
		EmptyResult.EstimatedSuccessProbability >= 0.f
		&& EmptyResult.EstimatedSuccessProbability <= 1.f);

	FTerritoryAssaultEvaluationInput Defended = Empty;
	Defended.ActiveGuards = 1;
	const FTerritoryAssaultEvaluationResult DefendedResult =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Defended, Profile);
	TestTrue(TEXT("Adding the first valid guard cannot increase launch probability"),
		DefendedResult.LaunchProbability <= EmptyResult.LaunchProbability);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackPhysicalSpawnContract,
	"TerritoryFramework.CounterAttack.Regression.PhysicalSpawnUsesNarrativeController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackPhysicalSpawnContract::RunTest(const FString& Parameters)
{
	const ATerritoryAssaultCharacter* CDO =
		GetDefault<ATerritoryAssaultCharacter>();
	TestNotNull(TEXT("Native physical assault pawn has a CDO"), CDO);
	if (!CDO) return false;

	TestTrue(TEXT("Physical assault pawn selects a Narrative NPC controller"),
		CDO->AIControllerClass
		&& CDO->AIControllerClass->IsChildOf(ANarrativeNPCController::StaticClass()));
	TestTrue(TEXT("Physical assault pawn auto-possesses dynamically spawned instances"),
		CDO->AutoPossessAI == EAutoPossessAI::Spawned
		|| CDO->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned);
	TestEqual(TEXT("Assault capsule initializes from a valid named collision profile"),
		CDO->GetCapsuleComponent()->GetCollisionProfileName(),
		UCollisionProfile::Pawn_ProfileName);
	TestEqual(TEXT("Assault mesh initializes from the engine CharacterMesh profile"),
		CDO->GetMesh()->GetCollisionProfileName(), FName(TEXT("CharacterMesh")));
	const UFunction* ReadinessFunction = ATerritoryAssaultCharacter::StaticClass()->
		FindFunctionByName(GET_FUNCTION_NAME_CHECKED(ATerritoryAssaultCharacter, IsNarrativeSpawnReady));
	TestNotNull(TEXT("Narrative readiness is exposed as a Blueprint/MCP diagnostic"),
		ReadinessFunction);
	if (ReadinessFunction)
	{
		TestTrue(TEXT("Narrative readiness diagnostic is side-effect free"),
			ReadinessFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));
	}
	const UFunction* RagdollFunction = ATerritoryAssaultCharacter::StaticClass()->
		FindFunctionByName(GET_FUNCTION_NAME_CHECKED(
			ATerritoryAssaultCharacter, HasValidDeathRagdollSetup));
	TestNotNull(TEXT("Death ragdoll readiness is exposed as a Blueprint/MCP diagnostic"),
		RagdollFunction);
	if (RagdollFunction)
	{
		TestTrue(TEXT("Death ragdoll diagnostic is side-effect free"),
			RagdollFunction->HasAnyFunctionFlags(FUNC_BlueprintPure));
	}

	const ATerritoryGuardCharacter* GuardCDO = GetDefault<ATerritoryGuardCharacter>();
	TestNotNull(TEXT("Native Territory guard has a CDO"), GuardCDO);
	if (GuardCDO)
	{
		TestTrue(TEXT("Native Territory guard selects a Narrative NPC controller"),
			GuardCDO->AIControllerClass
			&& GuardCDO->AIControllerClass->IsChildOf(ANarrativeNPCController::StaticClass()));
		TestTrue(TEXT("Native Territory guard auto-possesses dynamically spawned instances"),
			GuardCDO->AutoPossessAI == EAutoPossessAI::Spawned
			|| GuardCDO->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned);
		TestEqual(TEXT("Guard capsule initializes from a valid named collision profile"),
			GuardCDO->GetCapsuleComponent()->GetCollisionProfileName(),
			UCollisionProfile::Pawn_ProfileName);
		TestEqual(TEXT("Guard mesh initializes from the engine CharacterMesh profile"),
			GuardCDO->GetMesh()->GetCollisionProfileName(), FName(TEXT("CharacterMesh")));
	}

	FText FailureReason;
	TestTrue(TEXT("The native assault class satisfies runtime/editor admission"),
		ATerritoryAssaultCharacter::ValidateNarrativeSpawnClass(
			ATerritoryAssaultCharacter::StaticClass(), FailureReason));
	TestTrue(TEXT("Successful admission has no failure reason"),
		FailureReason.IsEmpty());
	TestFalse(TEXT("A plain Narrative NPC cannot bypass the Territory assault contract"),
		ATerritoryAssaultCharacter::ValidateNarrativeSpawnClass(
			ANarrativeNPCCharacter::StaticClass(), FailureReason));
	TestFalse(TEXT("Rejected admission returns an actionable reason"),
		FailureReason.IsEmpty());

	UNPCDefinition* Definition = NewObject<UNPCDefinition>();
	Definition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	Definition->bAllowMultipleInstances = false;
	TestTrue(TEXT("A single finite attacker may use a unique Narrative definition"),
		ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
			Definition, 1, FailureReason));
	TestFalse(TEXT("A multi-pawn force cannot use a single-instance Narrative definition"),
		ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
			Definition, 2, FailureReason));
	TestFalse(TEXT("Multiple-instance rejection returns an actionable reason"),
		FailureReason.IsEmpty());
	Definition->bAllowMultipleInstances = true;
	TestTrue(TEXT("A reusable Narrative definition admits a finite multi-pawn force"),
		ATerritoryAssaultCharacter::ValidateNarrativeSpawnDefinition(
			Definition, 6, FailureReason));

	UNPCDefinition* GuardDefinition = NewObject<UNPCDefinition>();
	GuardDefinition->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
	GuardDefinition->bAllowMultipleInstances = false;
	TestTrue(TEXT("A one-slot garrison may use a unique Narrative definition"),
		ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
			GuardDefinition, 1, FailureReason));
	TestFalse(TEXT("A multi-slot garrison rejects a single-instance Narrative definition"),
		ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
			GuardDefinition, 2, FailureReason));
	GuardDefinition->bAllowMultipleInstances = true;
	TestTrue(TEXT("A reusable Narrative guard definition admits a multi-slot garrison"),
		ATerritoryGuardCharacter::ValidateNarrativeSpawnDefinition(
			GuardDefinition, 7, FailureReason));

	TestEqual(TEXT("Unstaffed target receives no hidden defensive reserves"),
		UTerritoryCounterAttackSubsystem::CalculateEffectiveReserveGuards(7, 0), 0);
	TestEqual(TEXT("One authorized slot can use at most one reserve entitlement"),
		UTerritoryCounterAttackSubsystem::CalculateEffectiveReserveGuards(7, 1), 1);
	TestEqual(TEXT("Negative saved/config values are bounded"),
		UTerritoryCounterAttackSubsystem::CalculateEffectiveReserveGuards(-2, -4), 0);

	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	TestTrue(TEXT("Pre-activation assault phases accept a securely claimed target"),
		UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
			ETerritoryAssaultState::WaitingForPlayerProximity,
			ETerritoryState::Claimed, FGameplayTag(), Bandits));
	TestTrue(TEXT("An active physical assault keeps its own matching contested target"),
		UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
			ETerritoryAssaultState::Active,
			ETerritoryState::Contested, Bandits, Bandits));
	TestFalse(TEXT("A warning cannot generate or inherit capture pressure"),
		UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
			ETerritoryAssaultState::ScheduledWarning,
			ETerritoryState::Contested, Bandits, Bandits));
	TestFalse(TEXT("A third faction contest invalidates the active assault"),
		UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
			ETerritoryAssaultState::Active,
			ETerritoryState::Contested, Heroes, Bandits));
	TestFalse(TEXT("Locked targets remain invalid during physical activation"),
		UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
			ETerritoryAssaultState::Active,
			ETerritoryState::Locked, FGameplayTag(), Bandits));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackPowerMonotonicity,
	"TerritoryFramework.CounterAttack.Behavior.AttackerPowerMonotonicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackPowerMonotonicity::RunTest(const FString& Parameters)
{
	const UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	FTerritoryAssaultEvaluationInput Input = TerritoryCounterAttackTests::MakeBaselineInput();
	float PreviousLaunch = -1.f;
	float PreviousSuccess = -1.f;
	for (int32 Power = 0; Power <= 500; Power += 10)
	{
		Input.AttackingMilitaryPower = static_cast<float>(Power);
		const FTerritoryAssaultEvaluationResult Result =
			UTerritoryCounterAttackSubsystem::CalculateEvaluation(Input, Profile);
		TestTrue(FString::Printf(TEXT("Power %d does not reduce launch probability"), Power),
			Result.LaunchProbability + KINDA_SMALL_NUMBER >= PreviousLaunch);
		TestTrue(FString::Printf(TEXT("Power %d does not reduce estimated success"), Power),
			Result.EstimatedSuccessProbability + KINDA_SMALL_NUMBER >= PreviousSuccess);
		PreviousLaunch = Result.LaunchProbability;
		PreviousSuccess = Result.EstimatedSuccessProbability;
	}

	UTerritoryCounterAttackProfile* AggressiveProfile = NewObject<UTerritoryCounterAttackProfile>();
	AggressiveProfile->BaseLaunchProbability = 0.9f;
	const FTerritoryAssaultEvaluationResult Base =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Input, Profile);
	const FTerritoryAssaultEvaluationResult Aggressive =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Input, AggressiveProfile);
	TestTrue(TEXT("Launch policy can change independently"),
		Aggressive.LaunchProbability >= Base.LaunchProbability);
	TestEqual(TEXT("Estimated success is not an ownership/launch policy roll"),
		Aggressive.EstimatedSuccessProbability, Base.EstimatedSuccessProbability);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackSaveRestore,
	"TerritoryFramework.CounterAttack.SaveLoad.ActiveForceReconstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackSaveRestore::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Test world created"), World);
	if (!World) return false;
	UTerritoryCounterAttackSubsystem* Subsystem =
		NewObject<UTerritoryCounterAttackSubsystem>(World);
	TestNotNull(TEXT("Counterattack subsystem created"), Subsystem);

	FTerritoryAssaultRecord Saved;
	Saved.AssaultID = FGuid::NewGuid();
	Saved.TargetTerritoryGUID = FGuid::NewGuid();
	Saved.TargetTerritory = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	Saved.State = ETerritoryAssaultState::Active;
	Saved.DecisionSeed = 94731;
	Saved.DecisionRoll = 0.271f;
	Saved.PlannedForce = 8;
	Saved.AliveForce = 3;
	Saved.PendingReserveForce = 2;
	Saved.KilledForce = 2;
	Saved.WithdrawnForce = 1;
	Saved.WaveSize = 3;

	Subsystem->RestorePersistentState({Saved});
	FTerritoryAssaultRecord Restored;
	TestTrue(TEXT("Valid saved assault is restored"), Subsystem->GetAssault(Saved.AssaultID, Restored));
	TestEqual(TEXT("Decision seed is not rerolled"), Restored.DecisionSeed, Saved.DecisionSeed);
	TestEqual(TEXT("Decision roll is not rerolled"), Restored.DecisionRoll, Saved.DecisionRoll);
	TestEqual(TEXT("Killed force remains consumed"), Restored.KilledForce, 2);
	TestEqual(TEXT("Withdrawn force remains consumed"), Restored.WithdrawnForce, 1);
	TestEqual(TEXT("Live pawn pointers/counts are not restored"), Restored.AliveForce, 0);
	TestEqual(TEXT("All saved survivors become finite pending reconstruction"),
		Restored.PendingReserveForce, 5);
	TestEqual(TEXT("Every planned participant remains accounted for"),
		Restored.GetAccountedForce(), Restored.PlannedForce);
	TestTrue(TEXT("Restored physically active record reports active"),
		Subsystem->IsAssaultActive(Saved.AssaultID));
	TestTrue(TEXT("Restored active record is also pending-or-active"),
		Subsystem->IsAssaultPendingOrActive(Saved.AssaultID));

	FTerritoryAssaultRecord Warning = Saved;
	Warning.AssaultID = FGuid::NewGuid();
	Warning.State = ETerritoryAssaultState::ScheduledWarning;
	Warning.AliveForce = 0;
	Warning.PendingReserveForce = Warning.PlannedForce - Warning.KilledForce - Warning.WithdrawnForce;
	Subsystem->RestorePersistentState({Warning});
	TestFalse(TEXT("A scheduled warning is not a physically active assault"),
		Subsystem->IsAssaultActive(Warning.AssaultID));
	TestTrue(TEXT("A scheduled warning remains a pending assault"),
		Subsystem->IsAssaultPendingOrActive(Warning.AssaultID));

	FTerritoryAssaultRecord Invalid = Saved;
	Invalid.AssaultID.Invalidate();
	Subsystem->RestorePersistentState({Invalid});
	TestEqual(TEXT("Invalid durable identity is rejected"), Subsystem->GetAllAssaults().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackCycleHighWater,
	"TerritoryFramework.CounterAttack.SaveLoad.TrimmedHistoryDoesNotReuseDecisionCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackCycleHighWater::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Test world created"), World);
	if (!World) return false;
	UTerritoryCounterAttackSubsystem* Subsystem =
		NewObject<UTerritoryCounterAttackSubsystem>(World);
	TestNotNull(TEXT("Counterattack subsystem created"), Subsystem);
	if (!Subsystem)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGuid TerritoryGUID = FGuid::NewGuid();
	const FGameplayTag AttackingFaction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	FTerritoryAssaultCycleRecord SavedCycle;
	SavedCycle.TargetTerritoryGUID = TerritoryGUID;
	SavedCycle.AttackingFaction = AttackingFaction;
	SavedCycle.HighestEvaluationCycle = 41;

	// No assault history is retained: this is the exact history-trimming regression.
	Subsystem->RestorePersistentState({}, {SavedCycle});
	TestEqual(TEXT("The durable high-water mark owns the next decision cycle"),
		Subsystem->ReserveNextEvaluationCycle(TerritoryGUID, AttackingFaction), 42);

	const TArray<FTerritoryAssaultCycleRecord> Exported =
		Subsystem->GetPersistentCycleState();
	TestEqual(TEXT("One cycle ledger entry is exported"), Exported.Num(), 1);
	if (Exported.Num() == 1)
	{
		TestEqual(TEXT("Updated high-water mark persists"),
			Exported[0].HighestEvaluationCycle, 42);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackWarningInvariant,
	"TerritoryFramework.CounterAttack.Regression.WarningHasNoPhysicalForceOrPressure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackWarningInvariant::RunTest(const FString& Parameters)
{
	FTerritoryAssaultRecord Warning;
	Warning.State = ETerritoryAssaultState::ScheduledWarning;
	Warning.PlannedForce = 6;
	Warning.AliveForce = 0;
	Warning.PendingReserveForce = 6;
	TestFalse(TEXT("A warning is not an active assault"),
		Warning.State == ETerritoryAssaultState::Active);
	TestEqual(TEXT("A warning has zero spawned attackers"), Warning.AliveForce, 0);
	TestEqual(TEXT("A warning preserves finite pending force"), Warning.PendingReserveForce, 6);
	TestEqual(TEXT("A warning alone accounts for no casualties"), Warning.KilledForce, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCaptureAtomicContestTransition,
	"TerritoryFramework.CounterAttack.Regression.CaptureContestReadModelIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCaptureAtomicContestTransition::RunTest(const FString& Parameters)
{
	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	UTerritoryControlSubsystem* Control = NewObject<UTerritoryControlSubsystem>();
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	TestNotNull(TEXT("Territory exists"), Territory);
	TestNotNull(TEXT("Control subsystem exists"), Control);
	TestTrue(TEXT("Bandit faction exists"), Bandits.IsValid());
	if (!Territory || !Control || !Bandits.IsValid()) return false;
	// NewObject actors are ROLE_None; assign the authority role this native
	// mutation contract requires without constructing an entire PIE world.
	Territory->SetRole(ROLE_Authority);

	TestTrue(TEXT("Contest state and faction commit through one ownership write"),
		Control->CommitCaptureReadModel(Territory, ETerritoryState::Contested,
			Bandits, 0.f));
	const FTerritoryOwnershipData Contested = Territory->GetOwnershipData();
	TestEqual(TEXT("Atomic write exposes Contested state"),
		Contested.State, ETerritoryState::Contested);
	TestEqual(TEXT("The same write exposes the attacking faction"),
		Contested.ContestingFaction, Bandits);
	TestTrue(TEXT("An active assault accepts the committed snapshot"),
		UTerritoryCounterAttackSubsystem::IsTerritoryControlStateValidForAssault(
			ETerritoryAssaultState::Active, Contested.State,
			Contested.ContestingFaction, Bandits));

	TestTrue(TEXT("Recovery clears state, faction, and progress through one ownership write"),
		Control->CommitCaptureReadModel(Territory, ETerritoryState::Unclaimed,
			FGameplayTag(), 0.f));
	const FTerritoryOwnershipData Recovered = Territory->GetOwnershipData();
	TestEqual(TEXT("Recovery state is unclaimed"),
		Recovered.State, ETerritoryState::Unclaimed);
	TestFalse(TEXT("Recovery clears contesting faction"),
		Recovered.ContestingFaction.IsValid());
	TestEqual(TEXT("Recovery clears capture progress"), Recovered.ControlProgress, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackContracts,
	"TerritoryFramework.CounterAttack.Contract.AuthorityReplicationNarrative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackContracts::RunTest(const FString& Parameters)
{
	using namespace TerritoryCounterAttackTests;
	const UClass* CounterClass = UTerritoryCounterAttackSubsystem::StaticClass();
	TestTrue(TEXT("Scheduling is Blueprint authority-only"),
		HasFunctionFlag(CounterClass, TEXT("ScheduleCounterAttack"), FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Strongest eligible automatic scheduling is Blueprint authority-only"),
		HasFunctionFlag(CounterClass, TEXT("ScheduleBestCounterAttack"), FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Strategic candidate preview is side-effect-free Blueprint data"),
		HasFunctionFlag(CounterClass, TEXT("GetBestEligibleAttackerPreview"), FUNC_BlueprintPure));
	TestTrue(TEXT("Cancellation is Blueprint authority-only"),
		HasFunctionFlag(CounterClass, TEXT("CancelAssault"), FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Capture registration is Blueprint authority-only"),
		HasFunctionFlag(UTerritoryControlSubsystem::StaticClass(), TEXT("TryRegisterAttacker"),
			FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Explicit-context force capture is Blueprint authority-only"),
		HasFunctionFlag(UTerritoryControlSubsystem::StaticClass(), TEXT("ForceCaptureWithContext"),
			FUNC_BlueprintAuthorityOnly));
	const FProperty* ApproachIDProperty =
		FTerritoryAssaultApproach::StaticStruct()->FindPropertyByName(TEXT("ApproachID"));
	TestNotNull(TEXT("ApproachID remains a typed reflected field"), ApproachIDProperty);
	if (ApproachIDProperty)
	{
		TestEqual(TEXT("Approach ID has an understandable editor label"),
			ApproachIDProperty->GetMetaData(TEXT("DisplayName")), FString(TEXT("Approach ID")));
	}

	TestTrue(TEXT("WorldState assault read model replicates"),
		HasPropertyFlag(ATerritoryWorldState::StaticClass(), TEXT("ReplicatedAssaults"), CPF_Net));
	TestTrue(TEXT("WorldState assault campaign state saves"),
		HasPropertyFlag(ATerritoryWorldState::StaticClass(), TEXT("SavedAssaults"), CPF_SaveGame));
	TestTrue(TEXT("WorldState decision-cycle ledger saves"),
		HasPropertyFlag(ATerritoryWorldState::StaticClass(), TEXT("SavedAssaultCycles"), CPF_SaveGame));
	for (const FName Field : {TEXT("TargetTerritoryGUID"), TEXT("AttackingFaction"),
		TEXT("HighestEvaluationCycle")})
	{
		TestTrue(FString::Printf(TEXT("Cycle ledger %s is SaveGame"), *Field.ToString()),
			HasPropertyFlag(FTerritoryAssaultCycleRecord::StaticStruct(), Field, CPF_SaveGame));
	}
	for (const FName Field : {TEXT("AssaultID"), TEXT("TargetTerritoryGUID"), TEXT("State"),
		TEXT("DecisionSeed"), TEXT("DecisionRoll"), TEXT("PlannedForce"), TEXT("AliveForce"),
		TEXT("PendingReserveForce"), TEXT("KilledForce"), TEXT("WithdrawnForce"),
		TEXT("ConsecutiveSpawnFailures")})
	{
		TestTrue(FString::Printf(TEXT("%s is SaveGame"), *Field.ToString()),
			HasPropertyFlag(FTerritoryAssaultRecord::StaticStruct(), Field, CPF_SaveGame));
	}

	const UClass* ParticipantClass = UTerritoryAssaultParticipantComponent::StaticClass();
	for (const FName Field : {TEXT("AssaultID"), TEXT("TargetTerritory"),
		TEXT("AttackingFaction"), TEXT("bCaptureRegistered")})
	{
		TestTrue(FString::Printf(TEXT("Participant %s replicates"), *Field.ToString()),
			HasPropertyFlag(ParticipantClass, Field, CPF_Net));
	}
	TestTrue(TEXT("Physical attacker derives from Narrative NPC"),
		ATerritoryAssaultCharacter::StaticClass()->IsChildOf(ANarrativeNPCCharacter::StaticClass()));
	TestTrue(TEXT("Assault activity consumes Territory assault goals"),
		GetDefault<UTerritoryAssaultActivity>()->SupportsAssaultGoals());
	TestTrue(TEXT("Assault activity is combat interruptible"),
		GetDefault<UTerritoryAssaultActivity>()->IsInterruptable());
	TestTrue(TEXT("Physical spawn retries have a bounded default"),
		GetDefault<UTerritoryCounterAttackProfile>()->MaxConsecutiveSpawnFailures > 0);
	TestEqual(TEXT("An empty defence cascade defaults to certain launch after hard admission gates"),
		GetDefault<UTerritoryCounterAttackProfile>()->UnguardedLaunchProbability, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_GuardRestoreCount,
	"TerritoryFramework.Guards.Regression.FreshAndSavedGarrisonCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_GuardRestoreCount::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Fresh territory deploys authored desired guards"),
		ATerritoryVolume::CalculateGuardRestoreCount(false, 4, 0, 0), 4);
	TestEqual(TEXT("A saved defeated garrison remains defeated"),
		ATerritoryVolume::CalculateGuardRestoreCount(true, 4, 0, 0), 0);
	TestEqual(TEXT("Legacy defender count migrates saves without per-post counts"),
		ATerritoryVolume::CalculateGuardRestoreCount(true, 6, 0, 3), 3);
	TestEqual(TEXT("Saved active guards cannot exceed desired guards"),
		ATerritoryVolume::CalculateGuardRestoreCount(true, 5, 12, 12), 5);
	TestEqual(TEXT("Negative saved values are bounded"),
		ATerritoryVolume::CalculateGuardRestoreCount(true, 5, -2, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_TagBoundSpawnPointRegistration,
	"TerritoryFramework.Guards.Regression.TagBoundPostsJoinTerritoryCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_TagBoundSpawnPointRegistration::RunTest(const FString& Parameters)
{
	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	ATerritoryGuardSpawnPoint* First = NewObject<ATerritoryGuardSpawnPoint>();
	ATerritoryGuardSpawnPoint* Second = NewObject<ATerritoryGuardSpawnPoint>();
	TestNotNull(TEXT("Territory exists"), Territory);
	TestNotNull(TEXT("First tag-bound post exists"), First);
	TestNotNull(TEXT("Second tag-bound post exists"), Second);
	if (!Territory || !First || !Second) return false;

	TestEqual(TEXT("No authored spawn points means zero active garrison capacity"),
		Territory->GetMaxGuardCount(), 0);
	First->MaxGuards = 20;
	Second->MaxGuards = 20;
	Territory->RegisterResolvedGuardSpawnPoint(First);
	Territory->RegisterResolvedGuardSpawnPoint(Second);
	TestEqual(TEXT("Both resolved posts join the existing guard-post read model"),
		Territory->GetGuardSpawnPoints().Num(), 2);
	TestEqual(TEXT("Each unique post contributes exactly one active combat slot"),
		Territory->GetMaxGuardCount(), 2);
	TestEqual(TEXT("Legacy per-post MaxGuards cannot stack NPCs on one marker"),
		First->GetEffectiveMaxGuards(), 1);

	Territory->UnregisterResolvedGuardSpawnPoint(First);
	TestEqual(TEXT("Streamed-out post is removed exactly once"),
		Territory->GetGuardSpawnPoints().Num(), 1);
	TestEqual(TEXT("Remaining capacity equals remaining point count"), Territory->GetMaxGuardCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_AuthoredGuardSlotPlacement,
	"TerritoryFramework.Guards.Regression.AuthoredSlotPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_AuthoredGuardSlotPlacement::RunTest(const FString& Parameters)
{
	ATerritoryGuardSpawnPoint* SpawnPoint = NewObject<ATerritoryGuardSpawnPoint>();
	TestNotNull(TEXT("Spawn point exists"), SpawnPoint);
	if (!SpawnPoint) return false;

	const FTransform Marker(
		FRotator(0.f, 137.f, 0.f),
		FVector(12345.f, -6789.f, 250.f),
		FVector(2.f));
	SpawnPoint->SetActorTransform(Marker);
	TestTrue(TEXT("Authored marker transform is returned without broad navmesh projection"),
		SpawnPoint->GetSpawnTransform().Equals(Marker));

	FTransform Deployment;
	TestTrue(TEXT("Territory guard class resolves a deployment transform"),
		SpawnPoint->ResolveGuardDeploymentTransform(ATerritoryGuardCharacter::StaticClass(), Deployment));
	TestTrue(TEXT("Deployment preserves exact authored X"),
		FMath::IsNearlyEqual(Deployment.GetLocation().X, Marker.GetLocation().X));
	TestTrue(TEXT("Deployment preserves exact authored Y"),
		FMath::IsNearlyEqual(Deployment.GetLocation().Y, Marker.GetLocation().Y));
	TestTrue(TEXT("Deployment preserves authored facing"),
		FMath::IsNearlyEqual(Deployment.Rotator().Yaw, Marker.Rotator().Yaw));
	TestTrue(TEXT("Deployment aligns the capsule above the foot marker"),
		Deployment.GetLocation().Z > Marker.GetLocation().Z);
	TestTrue(TEXT("Spawn-point scale never scales the guard actor"),
		Deployment.GetScale3D().Equals(FVector::OneVector));
	FTransform InvalidDeployment;
	TestFalse(TEXT("Deployment rejects a missing typed Territory guard class"),
		SpawnPoint->ResolveGuardDeploymentTransform(nullptr, InvalidDeployment));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_GuardSlotSaveMigration,
	"TerritoryFramework.Guards.SaveLoad.LegacyMultiSlotMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_GuardSlotSaveMigration::RunTest(const FString& Parameters)
{
	ATerritoryGuardSpawnPoint* SpawnPoint = NewObject<ATerritoryGuardSpawnPoint>();
	TestNotNull(TEXT("Spawn point exists"), SpawnPoint);
	if (!SpawnPoint) return false;

	SpawnPoint->SavedActiveGuardCount = 7;
	SpawnPoint->PendingReserveSpawns = 4;
	SpawnPoint->CurrentReserveCount = 5;
	SpawnPoint->Load_Implementation();
	TestEqual(TEXT("Legacy saved active count is bounded to the physical slot"),
		SpawnPoint->SavedActiveGuardCount, 1);
	TestEqual(TEXT("Only one replacement can be pending for one active slot"),
		SpawnPoint->PendingReserveSpawns, 1);
	TestEqual(TEXT("Finite reserve inventory remains durable across migration"),
		SpawnPoint->CurrentReserveCount, 5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
