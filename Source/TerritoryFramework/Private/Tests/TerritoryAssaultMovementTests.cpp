#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AI/TerritoryAssaultActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultMovementWatchdog,
	"TerritoryFramework.CounterAttack.Regression.AcceptedPathWithoutPhysicalProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultMovementWatchdog::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->CreateAISystem();
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(20261005, 5, 10, 1);
	auto* Profile = NewObject<UTerritoryCounterAttackProfile>();
	Profile->bUseNavigationAwareObjectives = false;
	Profile->StalledMovementRetryInterval = 0.25f;
	Profile->MaxStalledMovementRetries = 3;
	Definition->CounterAttackProfile = Profile;
	Definition->ApplyToTerritory(Place);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Place);
	auto* NPC = World->SpawnActor<ATerritoryAssaultCharacter>();
	NPC->SetActorLocation(FVector(5000.f, 0.f, 0.f));
	if (!TestTrue(TEXT("Native controller/activity initializes"), NPC->EnsureNarrativeControllerReady()))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Controller = NPC->GetNPCController();
	auto* ActivityComponent = NPC->GetActivityComponent();
	auto* Participant = NPC->AssaultParticipant.Get();
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(20261005, 5, 10, 2);
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Record.DefendingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = Record.AliveForce = 1;
	auto Ownership = Place->GetOwnershipData();
	Ownership.OwningFaction = Record.DefendingFaction; Ownership.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Ownership);
	Cast<INarrativeTeamAgentInterface>(NPC)->AddFaction(Record.AttackingFaction);
	World->GetSubsystem<UTerritoryDiplomacySubsystem>()->SetDiplomacyState(Record.AttackingFaction, Record.DefendingFaction, EDiplomacyState::War);
	Counter->Assaults.Add(Record.AssaultID, Record);
	Counter->LiveParticipants.FindOrAdd(Record.AssaultID).Add(NPC);
	Participant->Configure(Record.AssaultID, Record.TargetTerritoryGUID, Record.TargetTerritory, Record.AttackingFaction);
	Participant->ConfigureAssaultRules(true, true);
	Participant->AssaultGoal = NewObject<UTerritoryAssaultGoal>(Controller);
	Participant->AssaultGoal->TargetLocation = Place->GetTerritoryBounds().GetCenter();
	auto* Activity = NewObject<UTerritoryAssaultActivity>(Controller);
	Activity->SetOwner(Controller, ActivityComponent);
	const auto* GoalProperty = FindFProperty<FObjectPropertyBase>(Activity->GetClass(), TEXT("ActivityGoal"));
	GoalProperty->SetObjectPropertyValue_InContainer(Activity, Participant->AssaultGoal);
	FindFProperty<FObjectPropertyBase>(ActivityComponent->GetClass(), TEXT("CurrentActivity"))
		->SetObjectPropertyValue_InContainer(ActivityComponent, Activity);
	// Native MoveTo includes the capsule radius and measures from the feet.
	// A non-capturing reinforcement can already have arrived while its actor
	// centre is more than the old hard-coded 200 cm sphere from the objective.
	NPC->GetCapsuleComponent()->SetCapsuleSize(42.f, 96.f);
	NPC->SetActorLocation(Place->GetTerritoryBounds().GetCenter() + FVector(190.f, 0.f, 96.f));
	Participant->ConfigureAssaultRules(false, true);
	TestTrue(TEXT("The existing Native path follower considers the destination reached"),
		Controller->GetPathFollowingComponent()->HasReached(Participant->AssaultGoal->TargetLocation,
			EPathFollowingReachMode::OverlapAgent, 150.f));
	for (int32 Attempt = 0; Attempt < 4 && !Participant->HasRetired(); ++Attempt)
	{
		World->TimeSeconds += 0.3;
		Participant->MaintainAssaultMovement(Place);
	}
	if (!TestFalse(TEXT("A arrived reinforcement cannot withdraw for failing to move closer"), Participant->HasRetired()))
	{
		World->DestroyWorld(false); return false;
	}
	Participant->ConfigureAssaultRules(true, true);
	// Physical occupation is already useful progress. A failed optional waypoint
	// must not retire a living, unopposed attacker during the recapture hold.
	const FBox Bounds = Place->GetTerritoryBounds();
	NPC->SetActorLocation(Bounds.GetCenter() + FVector(Bounds.GetExtent().X - 10.f, 0.f, 0.f));
	FindFProperty<FObjectPropertyBase>(ActivityComponent->GetClass(), TEXT("CurrentActivity"))
		->SetObjectPropertyValue_InContainer(ActivityComponent, nullptr);
	for (int32 Attempt = 0; Attempt < 4 && !Participant->HasRetired(); ++Attempt)
	{
		World->TimeSeconds += 0.3;
		Participant->MaintainAssaultMovement(Place);
	}
	if (!TestFalse(TEXT("An unopposed attacker already inside holds without a movement activity"), Participant->HasRetired()))
	{
		World->DestroyWorld(false); return false;
	}
	TestEqual(TEXT("Occupation does not consume movement retries"), Participant->ConsecutiveMovementRestartFailures, 0);
	NPC->SetActorLocation(FVector(5000.f, 0.f, 0.f));
	FindFProperty<FObjectPropertyBase>(ActivityComponent->GetClass(), TEXT("CurrentActivity"))
		->SetObjectPropertyValue_InContainer(ActivityComponent, Activity);
	auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	Guard->OwningTerritory = Place;
	Cast<INarrativeTeamAgentInterface>(Guard)->AddFaction(Record.DefendingFaction);
	Place->RegisterDefender(Guard);
	auto* Shooter = World->SpawnActor<ATerritoryGuardCharacter>();
	Cast<INarrativeTeamAgentInterface>(Shooter)->AddFaction(Record.DefendingFaction);
	Shooter->SetActorLocation(FVector(50000.f, 0.f, 0.f));
	Participant->DamagingEnemies.Add(Shooter, World->GetTimeSeconds() + 60.0);
	Participant->MaintainAssaultMovement(Place);
	TestTrue(TEXT("An unperceived registered guard remains the walking target despite distant damage"), Participant->AssaultGoal->TargetLocation.Equals(Guard->GetActorLocation()));
	Profile->bDamageRetaliationOverridesDefenderPriority = true;
	Participant->MaintainAssaultMovement(Place);
	TestTrue(TEXT("The explicit retaliation override changes the walking target"), Participant->AssaultGoal->TargetLocation.Equals(Shooter->GetActorLocation()));
	Profile->bDamageRetaliationOverridesDefenderPriority = false;
	Participant->DamagingEnemies.Empty();
	Place->UnregisterDefender(Guard);
	Guard->Destroy(); Shooter->Destroy();
	Participant->MaintainAssaultMovement(Place);
	// A real Native activity interruption consumes no stalled-movement interval.
	GoalProperty->SetObjectPropertyValue_InContainer(Activity, NewObject<UTerritoryAssaultGoal>(Controller));
	World->TimeSeconds += 10.0;
	Participant->MaintainAssaultMovement(Place);
	TestEqual(TEXT("Combat/activity interruption does not consume retries"), Participant->ConsecutiveMovementRestartFailures, 0);
	GoalProperty->SetObjectPropertyValue_InContainer(Activity, Participant->AssaultGoal);
	Participant->MaintainAssaultMovement(Place);
	NPC->SetActorLocation(FVector(4500.f, 0.f, 0.f));
	World->TimeSeconds += 0.3;
	Participant->MaintainAssaultMovement(Place);
	TestEqual(TEXT("Real progress preserves the finite force"), Participant->ConsecutiveMovementRestartFailures, 0);
	for (int32 Attempt = 0; Attempt < 3 && !Participant->HasRetired(); ++Attempt)
	{
		// Supply a valid engine path while deliberately not ticking pawn movement.
		// The old implementation treated Moving as proof of progress indefinitely.
		FNavPathSharedPtr Path = MakeShared<FNavigationPath, ESPMode::ThreadSafe>();
		Path->GetPathPoints().Add(FNavPathPoint(NPC->GetActorLocation()));
		Path->GetPathPoints().Add(FNavPathPoint(Participant->AssaultGoal->TargetLocation));
		Path->MarkReady();
		FAIMoveRequest Move(Participant->AssaultGoal->TargetLocation);
		Controller->GetPathFollowingComponent()->RequestMove(Move, Path);
		TestEqual(TEXT("A valid path is actually active in the engine follower"), Controller->GetMoveStatus(), EPathFollowingStatus::Moving);
		World->TimeSeconds += 0.3;
		Participant->MaintainAssaultMovement(Place);
	}
	TestTrue(TEXT("No physical progress exhausts the authored retry budget"), Participant->HasRetired());
	FTerritoryAssaultRecord Result;
	Counter->GetAssault(Record.AssaultID, Result);
	TestEqual(TEXT("Withdrawal consumes this finite participant once"), Result.WithdrawnForce, 1);
	TestEqual(TEXT("Withdrawn body supplies no living force"), Result.AliveForce, 0);
	Participant->Retire(false);
	Counter->GetAssault(Record.AssaultID, Result);
	TestEqual(TEXT("Repeated retirement cannot count another withdrawal"), Result.WithdrawnForce, 1);
	World->DestroyWorld(false);
	return true;
}
#endif
