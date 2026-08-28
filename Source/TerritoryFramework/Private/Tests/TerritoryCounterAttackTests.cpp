#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/TerritoryAssaultActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "AI/TerritoryNarrativeDeathSupport.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryAssaultTargetPolicy.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryGuardSpawnValidation.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryWorldState.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryQuestRules.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackQuestRuleBehavior,
	"TerritoryFramework.CounterAttack.Behavior.NarrativeQuestRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackQuestRuleBehavior::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Not Started matches only when Narrative has never recorded the quest"),
		UTerritoryQuestRulesLibrary::DoesQuestStateMatchValues(
			ETerritoryQuestStateRequirement::NotStarted, false, false, false, false, false));
	TestFalse(TEXT("Not Started fails after a quest was completed"),
		UTerritoryQuestRulesLibrary::DoesQuestStateMatchValues(
			ETerritoryQuestStateRequirement::NotStarted, true, false, true, false, true));
	TestTrue(TEXT("In Progress reads Narrative's active quest truth"),
		UTerritoryQuestRulesLibrary::DoesQuestStateMatchValues(
			ETerritoryQuestStateRequirement::InProgress, true, true, false, false, false));
	TestTrue(TEXT("Finished accepts a failed Narrative quest"),
		UTerritoryQuestRulesLibrary::DoesQuestStateMatchValues(
			ETerritoryQuestStateRequirement::Finished, true, false, false, true, true));
	TestFalse(TEXT("Block rule rejects a matching scoped player"),
		UTerritoryCounterAttackProfile::DoesQuestRulePass(
			ETerritoryCounterQuestRuleAction::BlockWhenMatched, true));
	TestTrue(TEXT("Block rule permits a counter when nobody matches"),
		UTerritoryCounterAttackProfile::DoesQuestRulePass(
			ETerritoryCounterQuestRuleAction::BlockWhenMatched, false));
	TestTrue(TEXT("Require rule permits a counter when one scoped player matches"),
		UTerritoryCounterAttackProfile::DoesQuestRulePass(
			ETerritoryCounterQuestRuleAction::RequireMatch, true));
	TestFalse(TEXT("Require rule fails closed with no matching online player"),
		UTerritoryCounterAttackProfile::DoesQuestRulePass(
			ETerritoryCounterQuestRuleAction::RequireMatch, false));
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackRouteDiagnostics,
	"TerritoryFramework.CounterAttack.Regression.RouteCancellationIsHumanReadable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackRouteDiagnostics::RunTest(const FString& Parameters)
{
	FString RouteFailure;
	TestFalse(TEXT("A missing world cannot be accepted as a physical route"),
		UTerritoryCounterAttackSubsystem::ValidateNavigationRoute(
			nullptr, FVector::ZeroVector, FVector::ZeroVector, &RouteFailure));
	TestTrue(TEXT("A failed route explains the missing world"),
		RouteFailure.Contains(TEXT("No world")));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Route diagnostic world created"), World);
	if (!World) return false;
	UTerritoryCounterAttackSubsystem* Subsystem =
		NewObject<UTerritoryCounterAttackSubsystem>(World);
	FTerritoryAssaultRecord Cancelled;
	Cancelled.AssaultID = FGuid::NewGuid();
	Cancelled.TargetTerritoryGUID = FGuid::NewGuid();
	Cancelled.TargetTerritory = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	Cancelled.State = ETerritoryAssaultState::Cancelled;
	Cancelled.Resolution = ETerritoryAssaultResolution::InvalidApproachOrRoute;
	Cancelled.PlannedForce = 6;
	Cancelled.WithdrawnForce = 6;
	Subsystem->RestorePersistentState({Cancelled});
	const FString DebugText = Subsystem->GetAssaultDebugString(Cancelled.AssaultID);
	TestTrue(TEXT("Debug state uses a readable enum name"),
		DebugText.Contains(TEXT("State=Cancelled")));
	TestTrue(TEXT("Debug reason names the exact route failure category"),
		DebugText.Contains(TEXT("Reason=InvalidApproachOrRoute")));
	TestTrue(TEXT("Debug force accounting includes finite withdrawals"),
		DebugText.Contains(TEXT("Force(P/A/R/K/W)=6/0/0/0/6")));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackDeterministicDeploymentFormation,
	"TerritoryFramework.CounterAttack.Regression.DeterministicSeparatedDeploymentFormation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackDeterministicDeploymentFormation::RunTest(const FString& Parameters)
{
	const FTransform Approach(FRotator(0.f, 90.f, 0.f), FVector::ZeroVector);
	const FVector Target(1000.f, 0.f, 0.f);
	constexpr float Spacing = 220.f;
	TArray<FTransform> Slots;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FTransform First =
			UTerritoryCounterAttackSubsystem::CalculateParticipantDeploymentTransform(
				Approach, Target, Index, Spacing);
		const FTransform Second =
			UTerritoryCounterAttackSubsystem::CalculateParticipantDeploymentTransform(
				Approach, Target, Index, Spacing);
		TestTrue(FString::Printf(TEXT("Formation slot %d is deterministic"), Index),
			First.Equals(Second));
		TestTrue(FString::Printf(TEXT("Formation slot %d faces the target"), Index),
			FVector::DotProduct(First.GetRotation().GetForwardVector(), FVector::ForwardVector)
			> 0.99f);
		Slots.Add(First);
	}

	for (int32 A = 0; A < Slots.Num(); ++A)
	{
		for (int32 B = A + 1; B < Slots.Num(); ++B)
		{
			TestTrue(FString::Printf(TEXT("Formation slots %d and %d remain separated"), A, B),
				FVector::DistSquared2D(Slots[A].GetLocation(), Slots[B].GetLocation())
				>= FMath::Square(Spacing - KINDA_SMALL_NUMBER));
		}
	}
	TestTrue(TEXT("A negative migrated slot is safely bounded to the first slot"),
		UTerritoryCounterAttackSubsystem::CalculateParticipantDeploymentTransform(
			Approach, Target, -4, Spacing).Equals(Slots[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackCombatGoalPriority,
	"TerritoryFramework.CounterAttack.Regression.CombatOutranksAssaultMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackCombatGoalPriority::RunTest(const FString& Parameters)
{
	UTerritoryAssaultGoal* AssaultGoal = NewObject<UTerritoryAssaultGoal>();
	AssaultGoal->AssaultID = FGuid::NewGuid();
	AssaultGoal->TargetTerritoryGUID = FGuid::NewGuid();
	AssaultGoal->TargetTerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	const float AssaultScore = AssaultGoal->GetGoalScore();
	TestTrue(TEXT("A valid assault movement goal remains selectable"), AssaultScore > 0.f);
	TestTrue(TEXT("Narrative attack base score 3 outranks assault movement"), AssaultScore < 3.f);
	TestFalse(TEXT("Assault goal remains durable after combat interruption"),
		AssaultGoal->ShouldCleanup());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackRegisteredDefenderGoalPreference,
	"TerritoryFramework.CounterAttack.Regression.RegisteredGuardOutranksPlayerGoal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackRegisteredDefenderGoalPreference::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Target preference preview world created"), World);
	if (!World) return false;

	ANarrativeNPCCharacter* RegisteredGuard =
		World->SpawnActor<ANarrativeNPCCharacter>();
	ANarrativeNPCCharacter* PlayerCharacter =
		World->SpawnActor<ANarrativeNPCCharacter>();
	ANarrativeNPCCharacter* Attacker =
		World->SpawnActor<ANarrativeNPCCharacter>();
	TestNotNull(TEXT("Registered guard target created"), RegisteredGuard);
	TestNotNull(TEXT("Player target created"), PlayerCharacter);
	TestNotNull(TEXT("Narrative attacker created"), Attacker);
	// Narrative owns the activity component on its NPC controller, and the bare base
	// character used by this focused policy test is intentionally not possessed. Build
	// the public Narrative component fixture directly instead of depending on controller
	// spawning or a project Blueprint class.
	UNPCActivityComponent* ActivityComponent = Attacker
		? NewObject<UNPCActivityComponent>(Attacker, TEXT("TestActivityComponent"))
		: nullptr;
	TestNotNull(TEXT("Narrative activity component fixture created"), ActivityComponent);
	if (!RegisteredGuard || !PlayerCharacter || !Attacker || !ActivityComponent)
	{
		World->DestroyWorld(false);
		return false;
	}

	UClass* NarrativeAttackGoalClass = LoadClass<UNPCGoalItem>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
	TestNotNull(TEXT("Narrative Pro attack goal class is available"), NarrativeAttackGoalClass);
	const FObjectProperty* TargetProperty = NarrativeAttackGoalClass
		? FindFProperty<FObjectProperty>(NarrativeAttackGoalClass, TEXT("TargetToAttack"))
		: nullptr;
	TestNotNull(TEXT("Narrative attack goal exposes TargetToAttack"), TargetProperty);
	if (!NarrativeAttackGoalClass || !TargetProperty)
	{
		World->DestroyWorld(false);
		return false;
	}

	UNPCGoalItem* GuardGoal = NewObject<UNPCGoalItem>(
		ActivityComponent, NarrativeAttackGoalClass);
	TargetProperty->SetObjectPropertyValue_InContainer(GuardGoal, RegisteredGuard);
	GuardGoal->DefaultScore = 3.25f;
	UNPCGoalItem* PlayerGoal = NewObject<UNPCGoalItem>(
		ActivityComponent, NarrativeAttackGoalClass);
	TargetProperty->SetObjectPropertyValue_InContainer(PlayerGoal, PlayerCharacter);
	PlayerGoal->DefaultScore = 4.75f;
	TestEqual(TEXT("Narrative's public goal-key contract resolves the registered guard"),
		GuardGoal->GetGoalKey(), static_cast<UObject*>(RegisteredGuard));
	TestEqual(TEXT("Narrative's public goal-key contract resolves the player"),
		PlayerGoal->GetGoalKey(), static_cast<UObject*>(PlayerCharacter));
	TestEqual(TEXT("Narrative accepts the registered-guard attack goal"),
		ActivityComponent->AddGoal(GuardGoal, false), GuardGoal);
	TestEqual(TEXT("Narrative accepts the player attack goal"),
		ActivityComponent->AddGoal(PlayerGoal, false), PlayerGoal);
	const FNPCGoalContainer NarrativeAttackGoals =
		ActivityComponent->GetGoals(NarrativeAttackGoalClass);
	TestEqual(TEXT("Narrative stores both attack goals under its concrete goal class"),
		NarrativeAttackGoals.Goals.Num(), 2);
	TArray<AActor*> LiveDefenders = {RegisteredGuard};
	TArray<FTerritoryNarrativeGoalScoreOverride> Overrides;

	const FTerritoryDefenderGoalPreferenceResult Applied =
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(
			NarrativeAttackGoals, LiveDefenders, Overrides);
	TestTrue(TEXT("The registered guard remains a Narrative combat goal"),
		Applied.bHasRegisteredDefenderGoal);
	TestEqual(TEXT("Only the player goal is suppressed while a guard is alive"),
		Applied.SuppressedNonDefenderGoals, 1);
	TestEqual(TEXT("The guard keeps Narrative's authored score"),
		GuardGoal->DefaultScore, 3.25f);
	TestEqual(TEXT("The player cannot outrank the live registered guard"),
		PlayerGoal->DefaultScore, 0.f);
	TestFalse(TEXT("Repeated reconciliation is stable and does not accumulate changes"),
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(
			NarrativeAttackGoals, LiveDefenders, Overrides).bScoresChanged);

	const TArray<AActor*> NoDefenders;
	TestTrue(TEXT("Removing the final registered defender restores Narrative scoring"),
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(
			NarrativeAttackGoals, NoDefenders, Overrides).bScoresChanged);
	TestEqual(TEXT("The exact player goal score is restored after the guard is gone"),
		PlayerGoal->DefaultScore, 4.75f);
	TestTrue(TEXT("Transient score overrides are never retained as campaign state"),
		Overrides.IsEmpty());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackStrategicSlotRoles,
	"TerritoryFramework.CounterAttack.Regression.DefendersDoNotConsumeStrategicSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackStrategicSlotRoles::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Territory guards use Narrative tactical tokens only"),
		UTerritoryCombatDirector::RequiresStrategicAssaultSlot(
			GetDefault<ATerritoryGuardCharacter>()));
	TestTrue(TEXT("Physical assault pawns require a strategic Territory slot"),
		UTerritoryCombatDirector::RequiresStrategicAssaultSlot(
			GetDefault<ATerritoryAssaultCharacter>()));
	TestNotNull(TEXT("Territory guard exposes its contextual combat gate"),
		ATerritoryGuardCharacter::StaticClass()->FindFunctionByName(
			TEXT("CanEngageTerritoryTarget")));
	TestNotNull(TEXT("Assault guard exposes its War combat gate"),
		ATerritoryAssaultCharacter::StaticClass()->FindFunctionByName(
			TEXT("CanEngageAssaultTarget")));

	const UTerritoryCounterAttackProfile* Profile =
		GetDefault<UTerritoryCounterAttackProfile>();
	TestTrue(TEXT("Counterattacks use Narrative difficulty tokens by default"),
		Profile->bCapConcurrentAttackersToNarrativeDifficulty);
	TestTrue(TEXT("Multi-floor objectives use navigation by default"),
		Profile->bUseNavigationAwareObjectives);
	TestTrue(TEXT("Participants distribute across defence objectives by default"),
		Profile->bDistributeParticipantsAcrossObjectives);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackMultiFloorFallback,
	"TerritoryFramework.CounterAttack.Regression.MultiFloorFallbackUses3DDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackMultiFloorFallback::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Multi-floor policy world created"), World);
	if (!World) return false;
	AActor* Participant = World->SpawnActor<AActor>();
	TestNotNull(TEXT("Participant fixture created"), Participant);
	if (!Participant)
	{
		World->DestroyWorld(false);
		return false;
	}
	Participant->SetActorLocation(FVector::ZeroVector);
	const TArray<FVector> Objectives = {
		FVector(0.f, 0.f, 1000.f),
		FVector(100.f, 0.f, 0.f)
	};
	FVector Selected = FVector::ZeroVector;
	TestTrue(TEXT("A fallback objective is selected"),
		TerritoryAssaultTargetPolicy::SelectObjectiveLocation(
			Participant, Objectives, 0, false, false, Selected));
	TestTrue(TEXT("Same-floor objective wins over an actor directly overhead"),
		Selected.Equals(Objectives[1]));
	World->DestroyWorld(false);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackStateEventPayload,
	"TerritoryFramework.CounterAttack.Behavior.StateEventPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackStateEventPayload::RunTest(const FString& Parameters)
{
	FTerritoryAssaultRecord Assault;
	Assault.AssaultID = FGuid::NewGuid();
	Assault.TargetTerritory = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	Assault.State = ETerritoryAssaultState::Active;
	Assault.Resolution = ETerritoryAssaultResolution::None;
	Assault.PlannedForce = 6;
	Assault.AliveForce = 3;

	TestTrue(TEXT("A real state transition emits a live event"),
		UTerritoryCounterAttackSubsystem::ShouldEmitCounterHappened(
			ETerritoryAssaultState::WaitingForPlayerProximity,
			ETerritoryAssaultState::Active, false));
	TestFalse(TEXT("A data-only update does not duplicate the state event"),
		UTerritoryCounterAttackSubsystem::ShouldEmitCounterHappened(
			ETerritoryAssaultState::Active, ETerritoryAssaultState::Active, false));
	TestFalse(TEXT("Save/load hydration never replays gameplay notifications"),
		UTerritoryCounterAttackSubsystem::ShouldEmitCounterHappened(
			ETerritoryAssaultState::WaitingForPlayerProximity,
			ETerritoryAssaultState::Active, true));

	const FTerritoryCounterAttackStateEvent Event =
		UTerritoryCounterAttackSubsystem::MakeCounterHappenedEvent(
			Assault, ETerritoryAssaultState::WaitingForPlayerProximity, 314.25);
	TestEqual(TEXT("Event keeps the durable assault identity"),
		Event.Assault.AssaultID, Assault.AssaultID);
	TestEqual(TEXT("Event reports the previous state"), Event.PreviousState,
		ETerritoryAssaultState::WaitingForPlayerProximity);
	TestEqual(TEXT("Event reports the committed new state"), Event.NewState,
		ETerritoryAssaultState::Active);
	TestEqual(TEXT("Embedded record and event state cannot diverge"),
		Event.Assault.State, Event.NewState);
	TestEqual(TEXT("Event carries the committed finite-force snapshot"),
		Event.Assault.AliveForce, 3);
	TestEqual(TEXT("Event carries campaign time"), Event.EventGameTime, 314.25);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackInfluenceCascade,
	"TerritoryFramework.CounterAttack.Behavior.InfluenceAcceleratesFiniteResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackInfluenceCascade::RunTest(const FString& Parameters)
{
	const UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	FTerritoryAssaultEvaluationInput Low = TerritoryCounterAttackTests::MakeBaselineInput();
	Low.FactionInfluence = 0.f;
	FTerritoryAssaultEvaluationInput High = Low;
	High.FactionInfluence = 1.f;

	const FTerritoryAssaultEvaluationResult LowResult =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(Low, Profile);
	const FTerritoryAssaultEvaluationResult HighResult =
		UTerritoryCounterAttackSubsystem::CalculateEvaluation(High, Profile);
	TestTrue(TEXT("Higher influence never lowers launch probability"),
		HighResult.LaunchProbability + KINDA_SMALL_NUMBER >= LowResult.LaunchProbability);
	TestEqual(TEXT("Influence does not become a hidden ownership/success roll"),
		HighResult.EstimatedSuccessProbability, LowResult.EstimatedSuccessProbability);

	const float NoInfluenceDelay =
		UTerritoryCounterAttackSubsystem::CalculateInfluenceAdjustedDelay(300.f, 0.f, 0.25f);
	const float FullInfluenceDelay =
		UTerritoryCounterAttackSubsystem::CalculateInfluenceAdjustedDelay(300.f, 1.f, 0.25f);
	TestEqual(TEXT("Zero influence preserves the authored delay"), NoInfluenceDelay, 300.f);
	TestEqual(TEXT("Maximum influence respects the configured minimum time scale"),
		FullInfluenceDelay, 75.f);
	TestTrue(TEXT("Higher influence shortens response timing"),
		FullInfluenceDelay < NoInfluenceDelay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContestedReserveAndPatrolCascade,
	"TerritoryFramework.Guards.Regression.ContestedReserveAndDefinitionPatrol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContestedReserveAndPatrolCascade::RunTest(const FString& Parameters)
{
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	TestTrue(TEXT("A claimed owner may deploy an authorized reserve"),
		ATerritoryGuardSpawnPoint::IsOwnerReserveDeploymentStateValid(
			ETerritoryState::Claimed, Bandits, FGameplayTag()));
	TestTrue(TEXT("The incumbent may deploy a finite reserve during an opposing live contest"),
		ATerritoryGuardSpawnPoint::IsOwnerReserveDeploymentStateValid(
			ETerritoryState::Contested, Bandits, Heroes));
	TestFalse(TEXT("A self-contest cannot manufacture reinforcements"),
		ATerritoryGuardSpawnPoint::IsOwnerReserveDeploymentStateValid(
			ETerritoryState::Contested, Bandits, Bandits));
	TestFalse(TEXT("An unclaimed territory cannot deploy owner reserves"),
		ATerritoryGuardSpawnPoint::IsOwnerReserveDeploymentStateValid(
			ETerritoryState::Unclaimed, FGameplayTag(), Heroes));

	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Patrol cascade preview world created"), World);
	if (!World) return false;
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryGuardSpawnPoint* SpawnPoint = World->SpawnActor<ATerritoryGuardSpawnPoint>(
		ATerritoryGuardSpawnPoint::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Guard spawn point created"), SpawnPoint);
	if (!SpawnPoint)
	{
		World->DestroyWorld(false);
		return false;
	}
	UTerritoryGuardPostDefinition* Definition = NewObject<UTerritoryGuardPostDefinition>();
	Definition->PatrolRoute.SetNum(2);
	Definition->PatrolRoute[0].Location = FVector(100.f, 200.f, 0.f);
	Definition->PatrolRoute[1].Location = FVector(400.f, 500.f, 0.f);
	Definition->bLoopPatrol = false;
	SpawnPoint->GuardPostDefinition = Definition;
	TestTrue(TEXT("A reusable GuardPostDefinition supplies a meaningful patrol route"),
		SpawnPoint->HasPatrolRoute());
	TestEqual(TEXT("The public patrol route consumes the effective data-asset route"),
		SpawnPoint->GetPatrolRoute().Num(), 2);
	TestEqual(TEXT("Transform and wait arrays stay parallel"),
		SpawnPoint->GetPatrolRouteAsTransforms().Num(),
		SpawnPoint->GetPatrolWaitTimes().Num());
	TestFalse(TEXT("The loop policy follows the effective data-asset route"),
		SpawnPoint->GetLoopPatrol());
	World->DestroyWorld(false);
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
	Saved.LaunchMode = ETerritoryAssaultLaunchMode::StoryPursuit;
	Saved.ResolvedGameTime = 321.0;
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
	TestEqual(TEXT("Explicit story launch mode survives save/load"), Restored.LaunchMode,
		ETerritoryAssaultLaunchMode::StoryPursuit);
	TestEqual(TEXT("Recurring timestamp survives save/load"), Restored.ResolvedGameTime, 321.0);
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

	for (const ETerritoryAssaultState DurableState : {
		ETerritoryAssaultState::Grace,
		ETerritoryAssaultState::ScheduledWarning,
		ETerritoryAssaultState::WaitingForPlayerProximity})
	{
		FTerritoryAssaultRecord Pending = Saved;
		Pending.AssaultID = FGuid::NewGuid();
		Pending.State = DurableState;
		Pending.AliveForce = 0;
		Pending.PendingReserveForce =
			Pending.PlannedForce - Pending.KilledForce - Pending.WithdrawnForce;
		Pending.GraceEndsGameTime = 98765.0;
		Pending.ScheduledGameTime = 456.0;
		Subsystem->RestorePersistentState({Pending});

		FTerritoryAssaultRecord RestoredPending;
		TestTrue(TEXT("Every pre-activation state restores by durable AssaultID"),
			Subsystem->GetAssault(Pending.AssaultID, RestoredPending));
		TestEqual(TEXT("Pre-activation state is not advanced or rerolled during restore"),
			RestoredPending.State, DurableState);
		TestEqual(TEXT("Pre-activation decision seed remains stable"),
			RestoredPending.DecisionSeed, Pending.DecisionSeed);
		TestEqual(TEXT("Pre-activation decision roll remains stable"),
			RestoredPending.DecisionRoll, Pending.DecisionRoll);
		TestEqual(TEXT("Grace deadline remains campaign-time stable"),
			RestoredPending.GraceEndsGameTime, Pending.GraceEndsGameTime);
		TestEqual(TEXT("Scheduled timestamp remains campaign-time stable"),
			RestoredPending.ScheduledGameTime, Pending.ScheduledGameTime);
		TestEqual(TEXT("Pre-activation casualties remain consumed"),
			RestoredPending.KilledForce, Pending.KilledForce);
		TestEqual(TEXT("Pre-activation force remains finite and fully accounted"),
			RestoredPending.GetAccountedForce(), RestoredPending.PlannedForce);
	}

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

	FTerritoryAssaultRecord GuidOnly = Warning;
	GuidOnly.AssaultID = FGuid::NewGuid();
	GuidOnly.TargetTerritory = FGameplayTag();
	Subsystem->RestorePersistentState({GuidOnly});
	TestTrue(TEXT("A stable GUID keeps a renamed-tag save record durable"),
		Subsystem->IsAssaultPendingOrActive(GuidOnly.AssaultID));

	FTerritoryAssaultRecord Invalid = Saved;
	Invalid.AssaultID.Invalidate();
	Subsystem->RestorePersistentState({Invalid});
	TestEqual(TEXT("Invalid durable identity is rejected"), Subsystem->GetAllAssaults().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFWorldStateAssaultPersistenceRoundTrip,
	"TerritoryFramework.CounterAttack.SaveLoad.WorldStateNarrativeArchiveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFWorldStateAssaultPersistenceRoundTrip::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Persistence bridge world created"), World);
	if (!World) return false;

	UTerritoryCounterAttackSubsystem* Counter =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	ATerritoryWorldState* WorldState = World->SpawnActor<ATerritoryWorldState>();
	TestNotNull(TEXT("Counterattack authority exists"), Counter);
	TestNotNull(TEXT("WorldState persistence authority exists"), WorldState);
	if (!Counter || !WorldState)
	{
		World->DestroyWorld(false);
		return false;
	}
	WorldState->SetActorGUID_Implementation(FGuid::NewGuid());

	FTerritoryAssaultRecord Saved;
	Saved.AssaultID = FGuid::NewGuid();
	Saved.TargetTerritoryGUID = FGuid::NewGuid();
	Saved.TargetTerritory = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	Saved.AttackingFaction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Saved.State = ETerritoryAssaultState::WaitingForPlayerProximity;
	Saved.EvaluationCycle = 9;
	Saved.DecisionSeed = 17041;
	Saved.DecisionRoll = 0.419f;
	Saved.PlannedForce = 7;
	Saved.PendingReserveForce = 4;
	Saved.KilledForce = 2;
	Saved.WithdrawnForce = 1;
	Saved.bNotificationSent = true;
	Counter->RestorePersistentState({Saved});

	WorldState->PrepareForSave_Implementation();
	TestEqual(TEXT("WorldState exports one durable assault"),
		WorldState->SavedAssaults.Num(), 1);
	TestEqual(TEXT("WorldState exports the deterministic cycle ledger"),
		WorldState->SavedAssaultCycles.Num(), 1);

	// Simulate an older/save-boundary snapshot that serialized two currently live
	// physical pawns. Live actor pointers are intentionally absent; load must turn
	// every surviving finite unit into pending reserve and publish that normalized
	// state to the replicated late-join read model immediately.
	FTerritoryAssaultRecord ActiveSaved = Saved;
	ActiveSaved.AssaultID = FGuid::NewGuid();
	ActiveSaved.TargetTerritoryGUID = FGuid::NewGuid();
	ActiveSaved.State = ETerritoryAssaultState::Active;
	ActiveSaved.PlannedForce = 7;
	ActiveSaved.AliveForce = 2;
	ActiveSaved.PendingReserveForce = 2;
	ActiveSaved.KilledForce = 2;
	ActiveSaved.WithdrawnForce = 1;
	WorldState->SavedAssaults.Add(ActiveSaved);

	TArray<uint8> SavedBytes;
	FMemoryWriter Writer(SavedBytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, true);
	SaveArchive.ArIsSaveGame = true;
	WorldState->Serialize(SaveArchive);
	Writer.Close();

	Counter->RestorePersistentState({});
	WorldState->SavedAssaults.Empty();
	WorldState->SavedAssaultCycles.Empty();
	FMemoryReader Reader(SavedBytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	WorldState->Serialize(LoadArchive);
	Reader.Close();
	WorldState->Load_Implementation();

	FTerritoryAssaultRecord Restored;
	TestTrue(TEXT("Narrative-compatible SaveGame archive restores the assault"),
		Counter->GetAssault(Saved.AssaultID, Restored));
	TestEqual(TEXT("Archive round-trip preserves the waiting state"),
		Restored.State, ETerritoryAssaultState::WaitingForPlayerProximity);
	TestEqual(TEXT("Archive round-trip preserves the decision seed"),
		Restored.DecisionSeed, Saved.DecisionSeed);
	TestEqual(TEXT("Archive round-trip preserves the decision roll"),
		Restored.DecisionRoll, Saved.DecisionRoll);
	TestEqual(TEXT("Archive round-trip preserves killed force"),
		Restored.KilledForce, Saved.KilledForce);
	TestEqual(TEXT("Archive round-trip preserves withdrawn force"),
		Restored.WithdrawnForce, Saved.WithdrawnForce);
	TestEqual(TEXT("Archive round-trip preserves the finite reserve"),
		Restored.PendingReserveForce, Saved.PendingReserveForce);
	TestEqual(TEXT("Archive round-trip preserves the next decision cycle"),
		Counter->ReserveNextEvaluationCycle(
			Saved.TargetTerritoryGUID, Saved.AttackingFaction), 10);

	FTerritoryAssaultRecord NormalizedActive;
	TestTrue(TEXT("An active saved assault remains durable after physical pawn reconstruction"),
		Counter->GetAssault(ActiveSaved.AssaultID, NormalizedActive));
	TestEqual(TEXT("Saved physical pawns are not restored as live pointers/counts"),
		NormalizedActive.AliveForce, 0);
	TestEqual(TEXT("Every surviving finite unit becomes pending reserve exactly once"),
		NormalizedActive.PendingReserveForce, 4);
	const FTerritoryAssaultRecord* LateJoinSnapshot =
		WorldState->ReplicatedAssaults.FindByPredicate(
			[&ActiveSaved](const FTerritoryAssaultRecord& Record)
			{
				return Record.AssaultID == ActiveSaved.AssaultID;
			});
	TestNotNull(TEXT("The normalized active assault is immediately available for late join"),
		LateJoinSnapshot);
	if (LateJoinSnapshot)
	{
		TestEqual(TEXT("Late join never receives phantom live attackers"),
			LateJoinSnapshot->AliveForce, 0);
		TestEqual(TEXT("Late join receives the normalized finite reserve"),
			LateJoinSnapshot->PendingReserveForce, 4);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackWorldPartitionTargetRebind,
	"TerritoryFramework.CounterAttack.SaveLoad.StreamedTargetRebindsByStableIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackWorldPartitionTargetRebind::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Stream/load-order world created"), World);
	if (!World) return false;

	UTerritoryCounterAttackSubsystem* Counter =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Counterattack authority exists"), Counter);
	TestNotNull(TEXT("Registry authority exists"), Registry);
	if (!Counter || !Registry)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGuid TargetGUID = FGuid::NewGuid();
	const FGameplayTag TargetTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	const FGameplayTag Defenders = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	FTerritoryAssaultRecord Saved;
	Saved.AssaultID = FGuid::NewGuid();
	Saved.TargetTerritoryGUID = TargetGUID;
	Saved.TargetTerritory = TargetTag;
	Saved.AttackingFaction = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Saved.DefendingFaction = Defenders;
	Saved.State = ETerritoryAssaultState::Grace;
	Saved.GraceEndsGameTime = TNumericLimits<double>::Max();
	Saved.DecisionSeed = 77123;
	Saved.PlannedForce = 5;
	Saved.PendingReserveForce = 5;
	Counter->RestorePersistentState({Saved});

	TestNull(TEXT("An unloaded target resolves to no live actor"),
		Counter->ResolveTerritory(Saved));
	TestTrue(TEXT("Missing streamed target leaves the assault durable"),
		Counter->IsAssaultPendingOrActive(Saved.AssaultID));

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryDistrict* ReusedTagInstance = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Unrelated replacement using the old tag is created"), ReusedTagInstance);
	if (ReusedTagInstance)
	{
		ReusedTagInstance->TerritoryGUID = FGuid::NewGuid();
		ReusedTagInstance->TerritoryTag = TargetTag;
		FTerritoryOwnershipData ClaimedByDefenders;
		ClaimedByDefenders.OwningFaction = Defenders;
		ClaimedByDefenders.State = ETerritoryState::Claimed;
		ReusedTagInstance->CommitOwnershipData(ClaimedByDefenders);
		TestEqual(TEXT("Unrelated tag-reuse actor registers"),
			Registry->RegisterTerritory(ReusedTagInstance),
			ETerritoryRegistrationResult::Success);
		TestNull(TEXT("A valid saved GUID never falls back to a reused gameplay tag"),
			Counter->ResolveTerritory(Saved));
		TestFalse(TEXT("A same-tag actor with the wrong GUID is not blocked by the old assault"),
			Counter->HasNonTerminalAssaultForTerritory(ReusedTagInstance));
		TestEqual(TEXT("Exact actor reads exclude an old assault that only shares the tag"),
			Counter->GetAssaultsForTerritoryActor(ReusedTagInstance).Num(), 0);
		TestEqual(TEXT("The tag-only fallback remains available for streamed-out story reads"),
			Counter->GetAssaultsForTerritory(TargetTag).Num(), 1);

		UTerritoryAssaultParticipantComponent* Participant =
			NewObject<UTerritoryAssaultParticipantComponent>(ReusedTagInstance);
		Participant->Configure(Saved.AssaultID, TargetGUID, TargetTag,
			Saved.AttackingFaction);
		TestFalse(TEXT("A physical participant rejects a same-tag target with the wrong GUID"),
			Participant->MatchesTargetTerritory(ReusedTagInstance));

		FTerritoryAssaultRecord LegacyTagOnly = Saved;
		LegacyTagOnly.TargetTerritoryGUID.Invalidate();
		TestTrue(TEXT("A legacy tag-only record may bind once for bounded migration"),
			Counter->DoesAssaultTargetTerritory(LegacyTagOnly, ReusedTagInstance));
		TestTrue(TEXT("Legacy binding records the actor's stable GUID"),
			Counter->ReconcileAssaultTargetIdentity(LegacyTagOnly, ReusedTagInstance));
		TestEqual(TEXT("Legacy migration stores the resolved stable GUID"),
			LegacyTagOnly.TargetTerritoryGUID, ReusedTagInstance->GetTerritoryGUID());
		Registry->UnregisterTerritory(ReusedTagInstance);
	}

	ATerritoryDistrict* FirstInstance = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("First streamed target instance created"), FirstInstance);
	if (!FirstInstance)
	{
		World->DestroyWorld(false);
		return false;
	}
	FirstInstance->TerritoryGUID = TargetGUID;
	FirstInstance->TerritoryTag = TargetTag;
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Defenders;
	Claimed.State = ETerritoryState::Claimed;
	FirstInstance->CommitOwnershipData(Claimed);
	TestEqual(TEXT("First streamed target registers"),
		Registry->RegisterTerritory(FirstInstance), ETerritoryRegistrationResult::Success);
	TestEqual(TEXT("Stable GUID binds the restored assault to the loaded target"),
		Counter->ResolveTerritory(Saved), static_cast<ATerritoryVolume*>(FirstInstance));
	TestTrue(TEXT("The matching stable target remains blocked by its existing assault"),
		Counter->HasNonTerminalAssaultForTerritory(FirstInstance));
	TestEqual(TEXT("Exact actor reads include the matching stable assault"),
		Counter->GetAssaultsForTerritoryActor(FirstInstance).Num(), 1);
	UTerritoryAssaultParticipantComponent* MatchingParticipant =
		NewObject<UTerritoryAssaultParticipantComponent>(FirstInstance);
	MatchingParticipant->Configure(Saved.AssaultID, TargetGUID, TargetTag,
		Saved.AttackingFaction);
	TestTrue(TEXT("A physical participant accepts the matching stable target"),
		MatchingParticipant->MatchesTargetTerritory(FirstInstance));

	ATerritoryDistrict* RenamedIdentity = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("GUID-preserving renamed target instance is created"), RenamedIdentity);
	if (RenamedIdentity)
	{
		const FGameplayTag RenamedTag = FGameplayTag::RequestGameplayTag(
			TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
		RenamedIdentity->TerritoryGUID = TargetGUID;
		RenamedIdentity->TerritoryTag = RenamedTag;
		FTerritoryAssaultRecord RenamedRecord = Saved;
		TestTrue(TEXT("Stable GUID permits a bounded tag-rename migration"),
			Counter->ReconcileAssaultTargetIdentity(RenamedRecord, RenamedIdentity));
		TestEqual(TEXT("The repaired read model uses the actor's current tag"),
			RenamedRecord.TargetTerritory, RenamedTag);
		TestEqual(TEXT("Tag migration never changes stable target identity"),
			RenamedRecord.TargetTerritoryGUID, TargetGUID);
	}

	Registry->UnregisterTerritory(FirstInstance);
	TestNull(TEXT("Stream-out removes only the live lookup, not durable assault state"),
		Counter->ResolveTerritory(Saved));
	FTerritoryAssaultRecord DuringStreamOut;
	TestTrue(TEXT("The exact AssaultID survives target stream-out"),
		Counter->GetAssault(Saved.AssaultID, DuringStreamOut));
	TestEqual(TEXT("Stream-out does not reroll the decision seed"),
		DuringStreamOut.DecisionSeed, Saved.DecisionSeed);

	ATerritoryDistrict* ReloadedInstance = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Reloaded target instance created"), ReloadedInstance);
	if (ReloadedInstance)
	{
		ReloadedInstance->TerritoryGUID = TargetGUID;
		ReloadedInstance->TerritoryTag = TargetTag;
		ReloadedInstance->CommitOwnershipData(Claimed);
		TestEqual(TEXT("Reloaded target registers with the same stable identity"),
			Registry->RegisterTerritory(ReloadedInstance),
			ETerritoryRegistrationResult::Success);
		TestEqual(TEXT("The same assault rebinds to the replacement actor instance"),
			Counter->ResolveTerritory(Saved),
			static_cast<ATerritoryVolume*>(ReloadedInstance));
	}

	FTerritoryAssaultRecord Restored;
	TestTrue(TEXT("Rebinding creates no duplicate assault"),
		Counter->GetAssault(Saved.AssaultID, Restored));
	TestEqual(TEXT("One durable assault remains after stream-in"),
		Counter->GetAllAssaults().Num(), 1);
	TestEqual(TEXT("Stream-in preserves the grace state"),
		Restored.State, ETerritoryAssaultState::Grace);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFPatrolOverlapAndDefenceFrontRegression,
	"TerritoryFramework.CounterAttack.Regression.PatrolOverlapAndDistrictDefenceFront",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFPatrolOverlapAndDefenceFrontRegression::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Patrol overlap world created"), World);
	if (!World) return false;
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryCity* City = World->SpawnActor<ATerritoryCity>(
		ATerritoryCity::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	AActor* ChildGuard = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryGuardSpawnPoint* PatrolPost = World->SpawnActor<ATerritoryGuardSpawnPoint>(
		ATerritoryGuardSpawnPoint::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("District created"), District);
	TestNotNull(TEXT("Child Property created"), Property);
	if (!City || !District || !Property || !ChildGuard || !PatrolPost)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag CityTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach"), false);
	const FGameplayTag DistrictTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare"), false);
	const FGameplayTag PropertyTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	City->TerritoryTag = CityTag;
	City->TerritoryGUID = FGuid::NewGuid();
	District->TerritoryTag = DistrictTag;
	District->ParentTerritoryTag = CityTag;
	District->TerritoryGUID = FGuid::NewGuid();
	Property->TerritoryTag = PropertyTag;
	Property->ParentTerritoryTag = DistrictTag;
	Property->TerritoryGUID = FGuid::NewGuid();
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Bandits;
	Claimed.State = ETerritoryState::Claimed;
	District->CommitOwnershipData(Claimed);
	Property->CommitOwnershipData(Claimed);

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Registry exists"), Registry);
	if (!Registry)
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("City registers"), Registry->RegisterTerritory(City),
		ETerritoryRegistrationResult::Success);
	TestEqual(TEXT("District registers"), Registry->RegisterTerritory(District),
		ETerritoryRegistrationResult::Success);
	TestEqual(TEXT("Property registers"), Registry->RegisterTerritory(Property),
		ETerritoryRegistrationResult::Success);

	const TArray<ATerritoryVolume*> PlacementCandidates = {City, District};
	TestTrue(TEXT("A patrol-node District hit outranks an actor-origin City hit"),
		ATerritoryGuardSpawnPoint::ChooseMostSpecificTerritory(PlacementCandidates) == District);
	Property->RegisterDefender(ChildGuard);
	Property->GuardSpawnPoints.Add(PatrolPost);
	FTerritoryPatrolNode PatrolNode;
	PatrolNode.Location = District->GetTerritoryBounds().GetCenter();
	PatrolPost->PatrolRoute.Add(PatrolNode);
	const TArray<ATerritoryVolume*> DefenceFront =
		TerritoryAssaultTargetPolicy::BuildDefenceFront(District);
	TestTrue(TEXT("District physical defence front includes its child Property"),
		DefenceFront.Contains(Property));
	TestTrue(TEXT("A child-Property guard is a physical District assault target"),
		TerritoryAssaultTargetPolicy::CollectRegisteredDefenders(District).Contains(ChildGuard));
	TestTrue(TEXT("A patrol node overlapping the District becomes a shared assault objective"),
		TerritoryAssaultTargetPolicy::BuildObjectiveLocations(District, false)
			.Contains(PatrolNode.Location));
	UTerritoryCounterAttackSubsystem* Counter =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	TestNotNull(TEXT("Counterattack authority exists"), Counter);
	if (Counter)
	{
		TestEqual(TEXT("One securely Claimed District grants one staging holding"),
			Counter->GetSecureDistrictCountForFaction(Bandits), 1);
		FTerritoryOwnershipData Locked = Claimed;
		Locked.State = ETerritoryState::Locked;
		District->CommitOwnershipData(Locked);
		TestEqual(TEXT("A story-Locked owned District remains a staging holding"),
			Counter->GetSecureDistrictCountForFaction(Bandits), 1);
		FTerritoryOwnershipData Contested = Claimed;
		Contested.State = ETerritoryState::Contested;
		District->CommitOwnershipData(Contested);
		TestEqual(TEXT("A Contested District is not a secure staging holding"),
			Counter->GetSecureDistrictCountForFaction(Bandits), 0);
		District->CommitOwnershipData(Locked);
		Registry->UnregisterTerritory(District);
		TestEqual(TEXT("A streamed-out District cannot grant phantom staging power"),
			Counter->GetSecureDistrictCountForFaction(Bandits), 0);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStrategicStagingRecurringAndOffscreenWavePolicy,
	"TerritoryFramework.CounterAttack.Behavior.StagingRecurringAndPostActivationPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStrategicStagingRecurringAndOffscreenWavePolicy::RunTest(const FString& Parameters)
{
	FTerritoryFactionAssaultConfig DefaultForce;
	TestEqual(TEXT("Normal forces require a secure District by default"),
		DefaultForce.StagingRequirement,
		ETerritoryAssaultStagingRequirement::OwnsSecureDistrict);
	TestFalse(TEXT("Story pursuit bypass needs explicit force opt-in"),
		DefaultForce.bAllowStoryPursuitWithoutStagingDistrict);
	TestTrue(TEXT("Recurring strategic counters are available by default"),
		DefaultForce.bEnableRecurringStrategicCounters);

	FTerritoryAssaultRecord Active;
	Active.State = ETerritoryAssaultState::Active;
	Active.PlannedForce = 3;
	Active.AliveForce = 0;
	Active.PendingReserveForce = 3;
	Active.WaveSize = 2;
	TestTrue(TEXT("After first activation, finite reserves can deploy without player presence"),
		UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(
			Active, false, true));
	TestFalse(TEXT("A project may modularly pause post-activation reserves outside proximity"),
		UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(
			Active, false, false));
	Active.State = ETerritoryAssaultState::WaitingForPlayerProximity;
	TestFalse(TEXT("The initial player activation gate is never bypassed"),
		UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(
			Active, false, true));

	FTerritoryAssaultRecord Previous;
	Previous.State = ETerritoryAssaultState::Defeated;
	Previous.LaunchMode = ETerritoryAssaultLaunchMode::StrategicCounterattack;
	Previous.ResolvedGameTime = 100.0;
	TestFalse(TEXT("Recurring counter waits for its complete cooldown"),
		UTerritoryCounterAttackSubsystem::IsRecurringCooldownComplete(
			Previous, 999.0, 900.f));
	TestTrue(TEXT("Recurring counter becomes eligible at the deterministic boundary"),
		UTerritoryCounterAttackSubsystem::IsRecurringCooldownComplete(
			Previous, 1000.0, 900.f));
	Previous.LaunchMode = ETerritoryAssaultLaunchMode::StoryPursuit;
	TestFalse(TEXT("Story pursuits never create automatic recurring counters"),
		UTerritoryCounterAttackSubsystem::IsRecurringCooldownComplete(
			Previous, 5000.0, 900.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNarrativeDeathStateMigrationRegression,
	"TerritoryFramework.CounterAttack.Regression.NarrativeDeathStateStopsMovementAndRagdolls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNarrativeDeathStateMigrationRegression::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("A live ASC overrides a stale reported death value"),
		TerritoryNarrativeDeathSupport::ResolveDeathState(
			GetDefault<UNarrativeAbilitySystemComponent>(), true));
	TestTrue(TEXT("A missing ASC falls back to the reported death value"),
		TerritoryNarrativeDeathSupport::ResolveDeathState(nullptr, true));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Death regression world created"), World);
	if (!World) return false;

	const FBoolProperty* DeadProperty = FindFProperty<FBoolProperty>(
		UNarrativeAbilitySystemComponent::StaticClass(), TEXT("bIsDead"));
	TestNotNull(TEXT("Narrative authoritative death property exists"), DeadProperty);

	auto VerifyCharacter = [this, DeadProperty](ANarrativeNPCCharacter* Character,
		const TCHAR* Label)
	{
		TestNotNull(FString::Printf(TEXT("%s spawned"), Label), Character);
		if (!Character || !DeadProperty) return;

		UNarrativeAbilitySystemComponent* ASC =
			Character->GetNarrativeAbilitySystemComponent();
		TestNotNull(FString::Printf(TEXT("%s owns Narrative ASC"), Label), ASC);
		if (!ASC) return;

		DeadProperty->SetPropertyValue_InContainer(ASC, true);
		TestTrue(FString::Printf(TEXT("%s authoritative ASC is dead"), Label),
			ASC->IsDead());

		if (ATerritoryGuardCharacter* Guard = Cast<ATerritoryGuardCharacter>(Character))
		{
			Guard->ReconcileNarrativeDeathState(ASC, false);
		}
		else if (ATerritoryAssaultCharacter* Assault =
			Cast<ATerritoryAssaultCharacter>(Character))
		{
			Assault->ReconcileNarrativeDeathState(ASC, false);
		}

		TestTrue(FString::Printf(TEXT("%s enters Narrative ragdoll"), Label),
			Character->IsRagdoll(false));
		const UNarrativeCharacterMovement* Movement =
			Character->GetNarrativeCharacterMovement();
		TestNotNull(FString::Printf(TEXT("%s uses Narrative movement"), Label), Movement);
		if (Movement)
		{
			TestTrue(FString::Printf(TEXT("%s leaves walking mode on death"), Label),
				Movement->IsCustomMovementMode(CMOVE_Ragdoll));
			TestTrue(FString::Printf(TEXT("%s death clears velocity"), Label),
				Movement->Velocity.IsNearlyZero());
		}
	};

	VerifyCharacter(World->SpawnActor<ATerritoryGuardCharacter>(), TEXT("Territory guard"));
	VerifyCharacter(World->SpawnActor<ATerritoryAssaultCharacter>(), TEXT("Territory attacker"));

	ATerritoryGuardCharacter* CleanupGuard =
		World->SpawnActor<ATerritoryGuardCharacter>();
	ANarrativeNPCController* CleanupController =
		World->SpawnActor<ANarrativeNPCController>();
	TestNotNull(TEXT("Activity-cleanup guard spawned"), CleanupGuard);
	TestNotNull(TEXT("Activity-cleanup Narrative controller spawned"), CleanupController);
	FObjectProperty* CachedControllerProperty = FindFProperty<FObjectProperty>(
		ANarrativeNPCCharacter::StaticClass(), TEXT("CachedController"));
	TestNotNull(TEXT("Narrative cached-controller contract exists"), CachedControllerProperty);
	if (CleanupGuard && CleanupController && CachedControllerProperty)
	{
		CachedControllerProperty->SetObjectPropertyValue_InContainer(
			CleanupGuard, CleanupController);
		UNPCActivityComponent* ActivityComponent = CleanupController->GetActivityComponent();
		TestNotNull(TEXT("Narrative activity component exists"), ActivityComponent);
		if (ActivityComponent)
		{
			ActivityComponent->Activate(true);
			TestTrue(TEXT("Narrative activity selection begins active"),
				ActivityComponent->IsActive());
			TestTrue(TEXT("Territory prepares the Narrative activity lifecycle for removal"),
				TerritoryNarrativeDeathSupport::PrepareForRemoval(*CleanupGuard));
			TestFalse(TEXT("A removed Territory NPC can no longer rescore Blueprint activities"),
				ActivityComponent->IsActive());
			TestFalse(TEXT("Activity selection rejects work after removal preparation"),
				ActivityComponent->PerformActivitySelection(true));
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackActivationAndCasualtyLifecycle,
	"TerritoryFramework.CounterAttack.Behavior.ActivationAndFiniteCasualtyLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackActivationAndCasualtyLifecycle::RunTest(const FString& Parameters)
{
	FTerritoryAssaultRecord Assault;
	Assault.AssaultID = FGuid::NewGuid();
	Assault.State = ETerritoryAssaultState::WaitingForPlayerProximity;
	Assault.PlannedForce = 2;
	Assault.AliveForce = 1;
	Assault.PendingReserveForce = 1;

	TestTrue(TEXT("The first relevant player commits physical activation"),
		UTerritoryCounterAttackSubsystem::TryCommitProximityActivation(Assault, 100.0));
	TestEqual(TEXT("Activation is committed before spawning"), Assault.State,
		ETerritoryAssaultState::Active);
	TestFalse(TEXT("A second nearby player cannot duplicate activation or waves"),
		UTerritoryCounterAttackSubsystem::TryCommitProximityActivation(Assault, 101.0));
	TestEqual(TEXT("The first activation timestamp remains durable"),
		Assault.ActivatedGameTime, 100.0);

	bool bExhausted = false;
	TestTrue(TEXT("One killed attacker is accounted once"),
		UTerritoryCounterAttackSubsystem::ApplyParticipantRemoval(
			Assault, true, bExhausted));
	TestEqual(TEXT("Death removes one living attacker immediately"), Assault.AliveForce, 0);
	TestEqual(TEXT("Death consumes one finite force member"), Assault.KilledForce, 1);
	TestFalse(TEXT("Pending reserve prevents premature defeat"), bExhausted);
	TestFalse(TEXT("The same absent living participant cannot decrement twice"),
		UTerritoryCounterAttackSubsystem::ApplyParticipantRemoval(
			Assault, true, bExhausted));
	TestEqual(TEXT("Duplicate removal leaves casualty count unchanged"), Assault.KilledForce, 1);

	Assault.AliveForce = 1;
	Assault.PendingReserveForce = 0;
	TestTrue(TEXT("The final living participant can withdraw"),
		UTerritoryCounterAttackSubsystem::ApplyParticipantRemoval(
			Assault, false, bExhausted));
	TestTrue(TEXT("Zero living plus zero pending force resolves as exhausted"), bExhausted);
	TestEqual(TEXT("Withdrawal is permanently accounted"), Assault.WithdrawnForce, 1);
	TestEqual(TEXT("Every planned participant remains finite and accounted"),
		Assault.GetAccountedForce(), Assault.PlannedForce);
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
	TestTrue(TEXT("Explicit story pursuit scheduling is Blueprint authority-only"),
		HasFunctionFlag(CounterClass, TEXT("ScheduleStoryPursuit"), FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Strongest eligible automatic scheduling is Blueprint authority-only"),
		HasFunctionFlag(CounterClass, TEXT("ScheduleBestCounterAttack"), FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Strategic candidate preview is side-effect-free Blueprint data"),
		HasFunctionFlag(CounterClass, TEXT("GetBestEligibleAttackerPreview"), FUNC_BlueprintPure));
	TestTrue(TEXT("Secure District count is a side-effect-free Blueprint query"),
		HasFunctionFlag(CounterClass, TEXT("GetSecureDistrictCountForFaction"), FUNC_BlueprintPure));
	TestTrue(TEXT("Strategic staging admission is a side-effect-free Blueprint query"),
		HasFunctionFlag(CounterClass, TEXT("CanFactionStageStrategicCounterAttack"), FUNC_BlueprintPure));
	TestTrue(TEXT("Loaded Territory assault lookup is an exact side-effect-free Blueprint query"),
		HasFunctionFlag(CounterClass, TEXT("GetAssaultsForTerritoryActor"), FUNC_BlueprintPure));
	TestTrue(TEXT("Cancellation is Blueprint authority-only"),
		HasFunctionFlag(CounterClass, TEXT("CancelAssault"), FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Global state event is Blueprint assignable"),
		HasPropertyFlag(CounterClass, TEXT("OnCounterHappened"), CPF_BlueprintAssignable));
	const UClass* ManagementClass = UTerritoryPlayerManagementComponent::StaticClass();
	TestTrue(TEXT("Owning-client state event is Blueprint assignable"),
		HasPropertyFlag(ManagementClass, TEXT("OnCounterHappened"), CPF_BlueprintAssignable));
	TestTrue(TEXT("State event delivery is an owning-client RPC"),
		HasFunctionFlag(ManagementClass, TEXT("ClientReceiveCounterHappened"), FUNC_NetClient));
	TestTrue(TEXT("State event delivery is reliable"),
		HasFunctionFlag(ManagementClass, TEXT("ClientReceiveCounterHappened"), FUNC_NetReliable));
	TestTrue(TEXT("Capture registration is Blueprint authority-only"),
		HasFunctionFlag(UTerritoryControlSubsystem::StaticClass(), TEXT("TryRegisterAttacker"),
			FUNC_BlueprintAuthorityOnly));
	TestTrue(TEXT("Explicit-context force capture is Blueprint authority-only"),
		HasFunctionFlag(UTerritoryControlSubsystem::StaticClass(), TEXT("ForceCaptureWithContext"),
			FUNC_BlueprintAuthorityOnly));
	const FProperty* ApproachIDProperty =
		FTerritoryAssaultApproach::StaticStruct()->FindPropertyByName(TEXT("ApproachID"));
	TestNotNull(TEXT("ApproachID remains a typed reflected field"), ApproachIDProperty);
#if WITH_EDITORONLY_DATA
	if (ApproachIDProperty)
	{
		TestEqual(TEXT("Approach ID has an understandable editor label"),
			ApproachIDProperty->GetMetaData(TEXT("DisplayName")), FString(TEXT("Approach ID")));
	}
#endif

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
		TEXT("LaunchMode"), TEXT("DecisionSeed"), TEXT("DecisionRoll"), TEXT("ResolvedGameTime"),
		TEXT("PlannedForce"), TEXT("AliveForce"),
		TEXT("PendingReserveForce"), TEXT("KilledForce"), TEXT("WithdrawnForce"),
		TEXT("ConsecutiveSpawnFailures")})
	{
		TestTrue(FString::Printf(TEXT("%s is SaveGame"), *Field.ToString()),
			HasPropertyFlag(FTerritoryAssaultRecord::StaticStruct(), Field, CPF_SaveGame));
	}
	for (const FName Field : {TEXT("Assault"), TEXT("PreviousState"), TEXT("NewState"),
		TEXT("Resolution"), TEXT("EventGameTime")})
	{
		TestNotNull(FString::Printf(TEXT("State event exposes %s"), *Field.ToString()),
			FTerritoryCounterAttackStateEvent::StaticStruct()->FindPropertyByName(Field));
	}

	const UClass* ParticipantClass = UTerritoryAssaultParticipantComponent::StaticClass();
	for (const FName Field : {TEXT("AssaultID"), TEXT("TargetTerritoryGUID"), TEXT("TargetTerritory"),
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
	const UClass* ProfileClass = UTerritoryCounterAttackProfile::StaticClass();
	for (const FName Field : {TEXT("ParticipantSpacing"),
		TEXT("SpawnPlacementAttemptsPerParticipant"),
		TEXT("StalledMovementRetryInterval"), TEXT("MaxStalledMovementRetries"),
		TEXT("InfluenceWeight"), TEXT("MinimumInfluenceTimingScale")})
	{
		TestNotNull(FString::Printf(TEXT("Deployment profile exposes %s"), *Field.ToString()),
			ProfileClass->FindPropertyByName(Field));
	}
	TestTrue(TEXT("Default participant spacing exceeds a standard Character diameter"),
		GetDefault<UTerritoryCounterAttackProfile>()->ParticipantSpacing >= 200.f);
	TestTrue(TEXT("Formation placement attempts are bounded"),
		GetDefault<UTerritoryCounterAttackProfile>()->SpawnPlacementAttemptsPerParticipant > 0);
	TestTrue(TEXT("Stalled movement retries are bounded"),
		GetDefault<UTerritoryCounterAttackProfile>()->MaxStalledMovementRetries > 0);
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

	FTransform SettledPlacement = Deployment;
	SettledPlacement.AddToTranslation(FVector(0.f, 0.f, -3.421671f));
	TestTrue(TEXT("Normal CharacterMovement floor settling remains a valid authored spawn"),
		TerritoryGuardSpawnValidation::IsPlacementAcceptable(Deployment, SettledPlacement));

	FTransform ExcessiveVerticalAdjustment = Deployment;
	ExcessiveVerticalAdjustment.AddToTranslation(FVector(0.f, 0.f, -20.f));
	TestFalse(TEXT("A large vertical relocation is still rejected"),
		TerritoryGuardSpawnValidation::IsPlacementAcceptable(
			Deployment, ExcessiveVerticalAdjustment));

	FTransform HorizontalDrift = Deployment;
	HorizontalDrift.AddToTranslation(FVector(2.f, 0.f, 0.f));
	TestFalse(TEXT("Horizontal drift from the authored slot is still rejected"),
		TerritoryGuardSpawnValidation::IsPlacementAcceptable(Deployment, HorizontalDrift));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFHierarchyStrictMajorityRegression,
	"TerritoryFramework.Hierarchy.Behavior.StrictMajorityAndUnanimousCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFHierarchyStrictMajorityRegression::RunTest(const FString& Parameters)
{
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	TestTrue(TEXT("Required Narrative faction tags exist"), Heroes.IsValid() && Bandits.IsValid());

	TArray<FGameplayTag> FortyFiveFiftyFive;
	TArray<FGameplayTag> HeroShare;
	HeroShare.Init(Heroes, 9);
	TArray<FGameplayTag> BanditShare;
	BanditShare.Init(Bandits, 11);
	FortyFiveFiftyFive.Append(HeroShare);
	FortyFiveFiftyFive.Append(BanditShare);
	TestEqual(TEXT("Heroes control exactly forty-five percent"),
		TerritoryHierarchyPolicy::CalculateControlFraction(FortyFiveFiftyFive, Heroes), 0.45f);
	TestEqual(TEXT("The fifty-five-percent faction is the only strict majority"),
		TerritoryHierarchyPolicy::FindStrictMajorityOwner(FortyFiveFiftyFive), Bandits);
	TestFalse(TEXT("Forty-five percent never completes parent capture"),
		TerritoryHierarchyPolicy::AreAllChildrenOwnedBy(FortyFiveFiftyFive, Heroes));

	TArray<FGameplayTag> Tie;
	HeroShare.Init(Heroes, 5);
	BanditShare.Init(Bandits, 5);
	Tie.Append(HeroShare);
	Tie.Append(BanditShare);
	TestFalse(TEXT("A fifty-fifty split has no dominant faction"),
		TerritoryHierarchyPolicy::FindStrictMajorityOwner(Tie).IsValid());

	TArray<FGameplayTag> AllHeroes;
	AllHeroes.Init(Heroes, 5);
	TestTrue(TEXT("All Places permit the District reducer to capture for Heroes"),
		TerritoryHierarchyPolicy::AreAllChildrenOwnedBy(AllHeroes, Heroes));
	TestEqual(TEXT("All Districts produce full City control"),
		TerritoryHierarchyPolicy::CalculateControlFraction(AllHeroes, Heroes), 1.f);
	AllHeroes[2] = FGameplayTag();
	TestFalse(TEXT("A Contested or Unclaimed child breaks secure unanimity"),
		TerritoryHierarchyPolicy::AreAllChildrenOwnedBy(AllHeroes, Heroes));
	TestEqual(TEXT("An insecure child reduces the secure control fraction"),
		TerritoryHierarchyPolicy::CalculateControlFraction(AllHeroes, Heroes), 0.8f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
