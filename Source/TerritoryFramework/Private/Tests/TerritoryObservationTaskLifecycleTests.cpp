#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/NarrativeNPCController.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeASCActor.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryAIObservationTask.h"
#include "Tales/TerritoryAssaultTask.h"
#include "Tales/TerritoryDisguiseTask.h"
#include "Tales/TerritoryNarrativeConditionTask.h"
#include "Tales/TerritoryStateTask.h"
#include "Tales/TerritoryStealthConditions.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryObservationTaskLifecycle,
	"TerritoryFramework.Tales.Regression.OptionalObjectivesAndObservationIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryObservationTaskLifecycle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Observation world"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	auto* Disguises = World->GetSubsystem<UTerritoryDisguiseSubsystem>();
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	if (!Registry || !Disguises || !Counter) { World->DestroyWorld(false); return false; }
	auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	auto* Tales = Controller->GetTalesComponent();
	const auto MakePlayer = [&]()
	{
		auto* PlayerState = NewObject<ANarrativePlayerState>(World->PersistentLevel);
		auto* Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
		PlayerState->SetRole(ROLE_Authority);
		Character->SetRole(ROLE_Authority);
		Character->SetPlayerState(PlayerState);
		auto* ASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Character);
		ASC->RegisterComponent();
		ASC->InitAbilityActorInfo(PlayerState, Character);
		return Character;
	};
	auto* First = MakePlayer();
	auto* Second = MakePlayer();
	const FVector Inside = FVector::ZeroVector;
	const FVector Outside(5000.f, 0.f, 0.f);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = Tag;
	Definition->StableTerritoryGUID = FGuid::NewGuid();
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->DefaultStealthProfile = NewObject<UTerritoryStealthProfile>(Definition);
	TestTrue(TEXT("Real place definition applied"), Definition->ApplyToTerritory(Place));
	FTerritoryOwnershipData Ownership = Place->GetOwnershipData();
	Ownership.OwningFaction = Bandits;
	Ownership.State = ETerritoryState::Claimed;
	Ownership.Availability = ETerritoryAvailability::Unlocked;
	TestTrue(TEXT("Real place ownership committed"), Place->CommitOwnershipData(Ownership));
	Registry->RegisterTerritory(Place);
	auto* Uniform = NewObject<UTerritoryDisguiseProfile>();
	Uniform->PerceivedFaction = Bandits;
	const auto Begin = [&](auto* Task)
	{
		Task->OwningComp = Tales;
		Task->OwningController = Controller;
		Task->OwningPawn = First; // Deliberately stale after the character changes.
		Task->bOptional = true;
		Task->MarkerSettings.bAddNavigationMarker = false;
		Task->BeginTask();
	};
	const auto SetPlayer = [&](ANarrativePlayerCharacter* Character)
	{
		Controller->SetOwnedCharacter(Character);
		Controller->SetPawn(Character);
	};

	auto* Equip = NewObject<UTerritoryDisguiseTask>(Tales);
	Begin(Equip);
	TestEqual(TEXT("Optional disguise waits when the player is not ready"), Equip->CurrentProgress, 0);
	SetPlayer(First);
	TestTrue(TEXT("Native-compatible uniform activation succeeds"), Disguises->ActivateDisguise(First, Uniform));
	TestEqual(TEXT("Real disguise delegate progresses the optional task started without a pawn"), Equip->CurrentProgress, 1);
	Equip->EndTask();
	TestFalse(TEXT("Disguise listener removed at end"), Disguises->OnDisguiseChanged.IsAlreadyBound(Equip, &UTerritoryDisguiseTask::HandleDisguiseChanged));

	auto* Enter = NewObject<UTerritoryStateTask>(Tales);
	Enter->TargetTerritory = Tag;
	Enter->Objective = ETerritoryStateTaskObjective::EnterTerritory;
	Enter->bCompleteIfAlreadySatisfied = false;
	First->SetActorLocation(Inside);
	Begin(Enter);
	Enter->TickTask_Implementation();
	TestEqual(TEXT("Require-new-entry is not bypassed by Native's immediate first tick"), Enter->CurrentProgress, 0);
	First->SetActorLocation(Outside);
	Enter->TickTask_Implementation();
	First->SetActorLocation(Inside);
	Enter->TickTask_Implementation();
	TestEqual(TEXT("An actual entry earns optional progress"), Enter->CurrentProgress, 1);
	Enter->EndTask();

	auto* Leave = NewObject<UTerritoryStateTask>(Tales);
	Leave->TargetTerritory = Tag;
	Leave->Objective = ETerritoryStateTaskObjective::LeaveTerritory;
	Begin(Leave);
	Second->SetActorLocation(Outside);
	SetPlayer(Second);
	Leave->TickTask_Implementation();
	TestEqual(TEXT("Respawn outside is not the old character leaving"), Leave->CurrentProgress, 0);
	Second->SetActorLocation(Inside);
	Leave->TickTask_Implementation();
	Registry->UnregisterTerritory(Place);
	Second->SetActorLocation(Outside);
	Registry->RegisterTerritory(Place);
	Leave->TickTask_Implementation();
	TestEqual(TEXT("Registry unload/reload cannot synthesize an exit"), Leave->CurrentProgress, 0);
	Second->SetActorLocation(Inside);
	Leave->TickTask_Implementation();
	Second->SetActorLocation(Outside);
	Leave->TickTask_Implementation();
	TestEqual(TEXT("The current character can subsequently leave"), Leave->CurrentProgress, 1);
	Leave->EndTask();

	// The condition adapter must pass the same live character context as tasks.
	auto* Gate = NewObject<UTerritoryNarrativeConditionTask>(Tales);
	auto* Condition = NewObject<UTerritoryDisguiseCondition>(Gate);
	Condition->ConditionFilter = EConditionFilter::CF_AnyCharacter;
	Gate->Conditions.Add(Condition);
	Begin(Gate);
	TestEqual(TEXT("Old character's disguise does not satisfy the new player's condition"), Gate->CurrentProgress, 0);
	Disguises->ActivateDisguise(Second, Uniform);
	Gate->TickTask_Implementation();
	TestEqual(TEXT("Live character satisfies the Native condition evaluator"), Gate->CurrentProgress, 1);
	Gate->EndTask();

	auto* ExitCover = NewObject<UTerritoryDisguiseTask>(Tales);
	ExitCover->Objective = ETerritoryDisguiseTaskObjective::ExitTerritoryUndetected;
	ExitCover->TargetTerritory = Tag;
	ExitCover->Faction = Bandits;
	SetPlayer(First);
	First->SetActorLocation(Inside);
	Begin(ExitCover);
	SetPlayer(Second);
	ExitCover->TickTask_Implementation();
	TestEqual(TEXT("New disguised player outside is not an undetected exit"), ExitCover->CurrentProgress, 0);
	Second->SetActorLocation(Inside);
	ExitCover->TickTask_Implementation();
	Disguises->CompromiseDisguise(Second, Bandits, Place);
	Second->SetActorLocation(Outside);
	ExitCover->TickTask_Implementation();
	Disguises->RestoreDisguise(Second, Bandits);
	ExitCover->TickTask_Implementation();
	TestEqual(TEXT("Restoring cover outside does not rewrite a detected exit"), ExitCover->CurrentProgress, 0);
	Second->SetActorLocation(Inside);
	ExitCover->TickTask_Implementation();
	Second->SetActorLocation(Outside);
	ExitCover->TickTask_Implementation();
	TestEqual(TEXT("A fresh accepted visit can earn an undetected exit"), ExitCover->CurrentProgress, 1);
	ExitCover->EndTask();

	auto* Target = World->SpawnActor<ATargetPoint>();
	auto* Reach = NewObject<UTerritoryAIObservationTask>(Tales);
	Reach->Objective = ETerritoryAIObservationObjective::ReachQuestOwner;
	auto* ReachProvider = NewObject<UNarrativeActorProvider_LevelReference>(Reach);
	ReachProvider->SoftActorReference = Target;
	Reach->TargetProvider = ReachProvider;
	Begin(Reach);
	TestEqual(TEXT("AI reaching an old player does not complete the current player's task"), Reach->CurrentProgress, 0);
	Target->SetActorLocation(Outside);
	Reach->TickTask_Implementation();
	TestEqual(TEXT("AI reaching the live player earns optional progress"), Reach->CurrentProgress, 1);
	Reach->EndTask();

	auto* A = NewObject<ANarrativeNPCController>(World->PersistentLevel);
	auto* B = NewObject<ANarrativeNPCController>(World->PersistentLevel);
	auto* FirstASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(First);
	auto* SecondASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Second);
	FirstASC->GrantedAttackTokens.Add(FAttackToken(A, 0.f));
	SetPlayer(First);
	auto* Release = NewObject<UTerritoryAIObservationTask>(Tales);
	Release->Objective = ETerritoryAIObservationObjective::ReleasesAttackToken;
	auto* TokenProvider = NewObject<UNarrativeActorProvider_LevelReference>(Release);
	TokenProvider->SoftActorReference = A;
	Release->TargetProvider = TokenProvider;
	Begin(Release);
	SetPlayer(Second);
	Release->TickTask_Implementation();
	TestEqual(TEXT("Changing player cannot release another player's token"), Release->CurrentProgress, 0);
	SecondASC->GrantedAttackTokens.Add(FAttackToken(A, 0.f));
	Release->TickTask_Implementation();
	TokenProvider->SoftActorReference = B;
	Release->TickTask_Implementation();
	TestEqual(TEXT("Changing provider cannot release the old AI's token"), Release->CurrentProgress, 0);
	SecondASC->GrantedAttackTokens.Add(FAttackToken(B, 0.f));
	Release->TickTask_Implementation();
	SecondASC->GrantedAttackTokens.RemoveAll([B](const FAttackToken& Token) { return Token.Owner == B; });
	Release->TickTask_Implementation();
	TestEqual(TEXT("Observed Native token removal earns optional progress"), Release->CurrentProgress, 1);
	Release->EndTask();

	auto* Available = NewObject<UTerritoryAIObservationTask>(Tales);
	Available->bCompleteIfAlreadySatisfied = false;
	auto* ReadyProvider = NewObject<UNarrativeActorProvider_LevelReference>(Available);
	Available->TargetProvider = ReadyProvider;
	Begin(Available);
	ReadyProvider->SoftActorReference = Target;
	ReadyProvider->OnProviderActorReady.Broadcast(Target);
	Available->TickTask_Implementation();
	TestEqual(TEXT("A late provider is an observed availability transition"), Available->CurrentProgress, 1);
	Available->EndTask();
	ReadyProvider->OnProviderActorReady.Broadcast(Target);
	Available->HandleTargetReady(Target);
	TestNull(TEXT("A late callback cannot bind an ended observation task"), Available->CachedTarget.Get());

	auto* Death = NewObject<UTerritoryAIObservationTask>(Tales);
	Death->Objective = ETerritoryAIObservationObjective::NPCDead;
	auto* DeathProvider = NewObject<UNarrativeActorProvider_LevelReference>(Death);
	auto* OldActor = World->SpawnActor<ANarrativeASCActor>();
	auto* OldASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(OldActor);
	DeathProvider->SoftActorReference = OldActor;
	Death->TargetProvider = DeathProvider;
	Begin(Death);
	DeathProvider->SoftActorReference.Reset();
	Death->TickTask_Implementation();
	OldASC->OnDeathStateChanged.Broadcast(OldActor, OldASC, true);
	TestEqual(TEXT("An unavailable provider releases the old death listener"), Death->CurrentProgress, 0);
	TestFalse(TEXT("Old death delegate is unbound"), OldASC->OnDeathStateChanged.IsAlreadyBound(Death, &UTerritoryAIObservationTask::HandleDeathStateChanged));
	Death->EndTask();

	auto* Assault = NewObject<UTerritoryAssaultTask>(Tales);
	Assault->TargetTerritory = Tag;
	Assault->Objective = ETerritoryAssaultTaskObjective::KillAttackers;
	Assault->RequiredQuantity = 3;
	Begin(Assault);
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid::NewGuid();
	Record.TargetTerritory = Tag;
	Record.KilledForce = 2;
	Counter->OnAssaultChanged.Broadcast(Record);
	TestEqual(TEXT("Optional assault task reads real publisher's final count"), Assault->CurrentProgress, 2);
	Assault->EndTask();
	Record.KilledForce = 3;
	Counter->OnAssaultChanged.Broadcast(Record);
	TestEqual(TEXT("Ended assault task stops listening"), Assault->CurrentProgress, 2);
	Tales->bIsLoading = true;
	Begin(Assault);
	Assault->SetProgress(2);
	Tales->bIsLoading = false;
	Counter->OnAssaultChanged.Broadcast(Record);
	TestEqual(TEXT("Native saved progress replay resumes optional record observation"), Assault->CurrentProgress, 3);
	Assault->EndTask();

	Controller->SetRole(ROLE_SimulatedProxy);
	Begin(Assault);
	Counter->OnAssaultChanged.Broadcast(Record);
	TestEqual(TEXT("Native authority gate rejects client objective progress"), Assault->CurrentProgress, 0);
	Assault->EndTask();
	Controller->SetRole(ROLE_Authority);
	Disguises->RemoveDisguise(First);
	Disguises->RemoveDisguise(Second);
	FirstASC->GrantedAttackTokens.Reset();
	SecondASC->GrantedAttackTokens.Reset();
	Controller->SetPawn(nullptr);
	Registry->UnregisterTerritory(Place);
	World->DestroyWorld(false);
	return true;
}

#endif
