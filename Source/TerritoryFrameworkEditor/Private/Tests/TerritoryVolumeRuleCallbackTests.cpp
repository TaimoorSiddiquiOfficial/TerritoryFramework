#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFVolumeRuleCallbacks,
	"TerritoryFramework.Capture.Regression.StateAndDefenderRuleCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFVolumeRuleCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Rule callback world exists"), World)) return false;
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	Territory->SetActorGUID_Implementation(FGuid(161, 162, 163, 164));
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	UTerritoryAuditCondition* First = NewObject<UTerritoryAuditCondition>(Territory);
	UTerritoryAuditCondition* Second = NewObject<UTerritoryAuditCondition>(Territory);
	UTerritoryAuditCondition* Replacement = NewObject<UTerritoryAuditCondition>(Territory);
	Second->Callback = []() { return false; };
	Replacement->Callback = []() { return true; };
	bool bNestedCommit = true;
	First->Callback = [Territory, &bNestedCommit]()
	{
		FTerritoryOwnershipData Nested = Territory->GetOwnershipData();
		Nested.PeriodicIncome = 777;
		bNestedCommit = Territory->CommitOwnershipData(Nested);
		return true;
	};
	Territory->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Claimed).EntryConditions = {First};
	FTerritoryOwnershipData Claimed = Territory->GetOwnershipData();
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Claimed.ControlProgress = 1.f;
	Claimed.DesiredGuardCount = 0;
	TestTrue(TEXT("The outer validated claim completes"), Territory->CommitOwnershipData(Claimed));
	TestFalse(TEXT("An entry condition cannot commit competing ownership data"), bNestedCommit);
	TestEqual(TEXT("The resulting state is the validated outer claim"), Territory->GetTerritoryState(), ETerritoryState::Claimed);
	FText Failure;
	for (bool bExit : {false, true})
	{
		FTerritoryStateConfig& Config = Territory->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Claimed);
		(bExit ? Config.ExitConditions : Config.EntryConditions) = {First, Second};
		First->Callback = [Territory, Replacement, bExit]()
		{
			FTerritoryStateConfig& Live = Territory->RuntimeStateConfigs.FindChecked(ETerritoryState::Claimed);
			(bExit ? Live.ExitConditions : Live.EntryConditions)[1] = Replacement;
			return true;
		};
		TestFalse(TEXT("A condition cannot replace a remaining requirement during this evaluation"), bExit
			? Territory->CheckStateExitConditions(ETerritoryState::Claimed, Failure)
			: Territory->CheckStateConditions(ETerritoryState::Claimed, Failure));
		(bExit ? Config.ExitConditions : Config.EntryConditions) = {First};
		bool bNestedQuery = true;
		First->Callback = [Territory, bExit, &bNestedQuery]()
		{
			FText NestedFailure;
			bNestedQuery = bExit
				? Territory->CheckStateExitConditions(ETerritoryState::Claimed, NestedFailure)
				: Territory->CheckStateConditions(ETerritoryState::Claimed, NestedFailure);
			return true;
		};
		TestTrue(TEXT("An outer condition query can complete without infinite recursion"), bExit
			? Territory->CheckStateExitConditions(ETerritoryState::Claimed, Failure)
			: Territory->CheckStateConditions(ETerritoryState::Claimed, Failure));
		TestFalse(TEXT("The same state/phase condition cannot recursively evaluate itself"), bNestedQuery);
	}
	First->Callback = []() { return false; };
	FTerritoryOwnershipData Rejected = Territory->GetOwnershipData();
	Rejected.State = ETerritoryState::Contested;
	TestFalse(TEXT("A rejected exit condition prevents the transition"), Territory->CommitOwnershipData(Rejected));
	First->Callback = []() { return true; };
	TestTrue(TEXT("Failure releases the validation guard for a later valid transition"),
		Territory->CommitOwnershipData(Rejected));
	TestTrue(TEXT("The Territory can subsequently return to the claimed state"), Territory->CommitOwnershipData(Claimed));

	UTerritoryAuditNarrativeEvent* FirstEvent = NewObject<UTerritoryAuditNarrativeEvent>(Territory);
	UTerritoryAuditNarrativeEvent* SecondEvent = NewObject<UTerritoryAuditNarrativeEvent>(Territory);
	UTerritoryAuditNarrativeEvent* ReplacementEvent = NewObject<UTerritoryAuditNarrativeEvent>(Territory);
	int32 OriginalEvents = 0;
	int32 ReplacementEvents = 0;
	SecondEvent->Callback = [&OriginalEvents]() { ++OriginalEvents; };
	ReplacementEvent->Callback = [&ReplacementEvents]() { ++ReplacementEvents; };
	for (bool bEntering : {false, true})
	{
		FTerritoryStateConfig& Config = Territory->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Claimed);
		(bEntering ? Config.EntryEvents : Config.ExitEvents) = {FirstEvent, SecondEvent};
		FirstEvent->Callback = [Territory, ReplacementEvent, bEntering]()
		{
			FTerritoryStateConfig& Live = Territory->RuntimeStateConfigs.FindChecked(ETerritoryState::Claimed);
			(bEntering ? Live.EntryEvents : Live.ExitEvents)[1] = ReplacementEvent;
		};
		Territory->FireStateEvents(ETerritoryState::Claimed, bEntering, FTerritoryTransitionContext());
	}
	TestEqual(TEXT("Each state-event bundle retains its original remaining event"), OriginalEvents, 2);
	TestEqual(TEXT("Replacement events are deferred to a future bundle"), ReplacementEvents, 0);
	Territory->RuntimeDefenderDiedEvents = {FirstEvent, SecondEvent};
	FirstEvent->Callback = [Territory, ReplacementEvent]()
	{
		Territory->RuntimeDefenderDiedEvents[1] = ReplacementEvent;
	};
	ATerritoryGuardCharacter* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	Territory->RegisterDefender(Guard);
	Territory->OnDefenderDied(Guard, Guard->GetNarrativeAbilitySystemComponent(), true);
	TestEqual(TEXT("The original defender-death event also survives callback edits"), OriginalEvents, 3);
	TestEqual(TEXT("Defender event replacement cannot execute in the current bundle"), ReplacementEvents, 0);
	Territory->RuntimeStateConfigs.Empty();
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative can save the committed state after callbacks"), Save->CreateActorRecord(Territory, Record));
	Territory->ForceSetTerritoryState(ETerritoryState::Unclaimed);
	Save->LoadActorFromRecord(Territory, Record);
	TestEqual(TEXT("Narrative reload retains the committed faction"), Territory->GetOwningFaction(), Heroes);
	Territory->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client state commits remain rejected"), Territory->CommitOwnershipData(Claimed));
	Territory->SetRole(ROLE_Authority);
	Territory->RuntimeStateConfigs.FindOrAdd(ETerritoryState::Unclaimed).EntryConditions = {First};
	First->Callback = [Territory]() { Territory->Destroy(); return true; };
	FTerritoryOwnershipData DestroyedCandidate = Claimed;
	DestroyedCandidate.State = ETerritoryState::Unclaimed;
	DestroyedCandidate.OwningFaction = FGameplayTag();
	DestroyedCandidate.ControlProgress = 0.f;
	TestFalse(TEXT("A condition cannot commit success after destroying its target"),
		Territory->CommitOwnershipData(DestroyedCandidate));
	World->DestroyWorld(false);
	return true;
}

#endif
