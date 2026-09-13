#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryCharacterActionTask.h"
#include "Tales/TerritoryCombatProgressTask.h"
#include "Tales/TerritoryGameplayStateTask.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryTaskSubjectLifecycle,
	"TerritoryFramework.Tales.Regression.LivePlayerSubjectPossessionAndReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryTaskSubjectLifecycle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Task subject world"), World)) return false;
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	auto* Tales = Controller->GetTalesComponent();
	auto* Movement = NewObject<UTerritoryCharacterActionTask>(Tales);
	auto* Combat = NewObject<UTerritoryCombatProgressTask>(Tales);
	auto* StateTask = NewObject<UTerritoryGameplayStateTask>(Tales);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Narrative.State.Stumbling"));
	Movement->Objective = ETerritoryCharacterActionObjective::Jump;
	Combat->Objective = ETerritoryCombatProgressObjective::DealHitCount;
	StateTask->RequiredTags.AddTag(Tag);
	StateTask->bCompleteIfAlreadySatisfied = false;
	const auto Begin = [&](auto* Task)
	{
		Task->OwningComp = Tales;
		Task->RequiredQuantity = 10;
		// Native IsComplete also means optional for branch eligibility. An optional
		// task must still listen and earn progress while its branch remains active.
		Task->bOptional = true;
		Task->MarkerSettings.bAddNavigationMarker = false;
		Task->BeginTask();
	};
	Begin(Movement);
	Begin(Combat);
	Begin(StateTask);
	TestNull(TEXT("Task can begin before the player's pawn exists"), Movement->CachedCharacter.Get());
	TestTrue(TEXT("Event-only tasks still retry late readiness"), Movement->TickInterval > 0.f && Combat->TickInterval > 0.f && StateTask->TickInterval > 0.f);
	const auto MakeCharacter = [&]()
	{
		auto* State = NewObject<ANarrativePlayerState>(World->PersistentLevel);
		auto* Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
		State->SetRole(ROLE_Authority);
		Character->SetRole(ROLE_Authority);
		Character->SetPlayerState(State);
		return Character;
	};
	auto* First = MakeCharacter();
	auto* Second = MakeCharacter();
	auto* FirstASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(First);
	auto* SecondASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(Second);
	if (!FirstASC || !SecondASC) { World->DestroyWorld(false); return false; }
	Controller->SetOwnedCharacter(First);
	Controller->SetPawn(First);
	// This transient controller has no project HUD or stable spawn identity.
	// SetPawn alone does not notify on authority; Possess broadcasts this public
	// delegate after Native OnPossess sets OwnedCharacter and initializes the ASC.
	Controller->OnPossessedPawnChanged.Broadcast(nullptr, First);
	TestTrue(TEXT("Possession binds the movement task immediately"), Movement->CachedCharacter.Get() == First);
	TestTrue(TEXT("Possession binds the Native combat ASC"), Combat->CachedAbilitySystem.Get() == FirstASC);
	First->OnJumpedDelegate.Broadcast();
	FirstASC->OnDealtDamage.Broadcast(SecondASC, 5.f, FGameplayEffectSpec());
	TestEqual(TEXT("Real Native jump event counts once"), Movement->CurrentProgress, 1);
	TestEqual(TEXT("Real Native damage event counts once"), Combat->CurrentProgress, 1);

	auto* Vehicle = World->SpawnActor<APawn>();
	Controller->SetPawn(Vehicle);
	Controller->OnPossessedPawnChanged.Broadcast(First, Vehicle);
	TestTrue(TEXT("Driving keeps the Native owned character"), TerritoryTales::ResolveTaskPawn(Tales, nullptr, Controller) == First);
	TestTrue(TEXT("Driving keeps the player ASC"), StateTask->CachedAbilitySystem.Get() == FirstASC);
	First->OnJumpedDelegate.Broadcast();
	TestEqual(TEXT("Vehicle possession does not duplicate action listeners"), Movement->CurrentProgress, 2);

	// Native also permits SetOwnedCharacter without a pawn change. The bounded
	// task tick must detect this even while the previous character remains alive.
	Controller->SetOwnedCharacter(Second);
	Movement->TickTask_Implementation();
	Combat->TickTask_Implementation();
	StateTask->TickTask_Implementation();
	TestTrue(TEXT("Readiness retry follows a changed live character"), Movement->CachedCharacter.Get() == Second);
	TestTrue(TEXT("GAS task releases the previous live ASC"), StateTask->CachedAbilitySystem.Get() == SecondASC);
	First->OnJumpedDelegate.Broadcast();
	FirstASC->OnDealtDamage.Broadcast(SecondASC, 5.f, FGameplayEffectSpec());
	FirstASC->AddLooseGameplayTag(Tag);
	TestEqual(TEXT("Old character cannot progress movement"), Movement->CurrentProgress, 2);
	TestEqual(TEXT("Old ASC cannot progress combat"), Combat->CurrentProgress, 1);
	TestEqual(TEXT("Old ASC cannot complete state objective"), StateTask->CurrentProgress, 0);
	Second->OnJumpedDelegate.Broadcast();
	SecondASC->OnDealtDamage.Broadcast(FirstASC, 5.f, FGameplayEffectSpec());
	SecondASC->AddLooseGameplayTag(Tag);
	TestEqual(TEXT("New character contributes movement"), Movement->CurrentProgress, 3);
	TestEqual(TEXT("New ASC contributes combat"), Combat->CurrentProgress, 2);
	TestEqual(TEXT("New ASC earns optional state objective"), StateTask->CurrentProgress, StateTask->RequiredQuantity);

	Movement->HandleProviderActorReady(nullptr);
	Second->OnJumpedDelegate.Broadcast();
	TestEqual(TEXT("Missing provider actor releases old movement listener"), Movement->CurrentProgress, 3);
	Movement->HandleProviderActorReady(Second);
	Movement->EndTask();
	Combat->EndTask();
	StateTask->EndTask();
	TestFalse(TEXT("End unbinds possession listener"), Controller->OnPossessedPawnChanged.IsAlreadyBound(Movement, &UTerritoryCharacterActionTask::HandleSubjectPawnChanged));
	TestFalse(TEXT("End unbinds combat listener"), SecondASC->OnDealtDamage.IsAlreadyBound(Combat, &UTerritoryCombatProgressTask::HandleDealtDamage));
	Movement->HandleProviderActorReady(First);
	TestNull(TEXT("A late provider callback cannot rebind an ended task"), Movement->CachedCharacter.Get());
	FirstASC->RemoveLooseGameplayTag(Tag);
	SecondASC->RemoveLooseGameplayTag(Tag);
	Controller->SetPawn(nullptr);
	World->DestroyWorld(false);
	return true;
}

#endif
