#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFWorldStateDistrictIdentityBounds,
	"TerritoryFramework.WorldState.Regression.DistrictIdentityBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFWorldStateDistrictIdentityBounds::RunTest(const FString& Parameters)
{
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	FReplicatedCaptureSummary First;
	First.TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	First.TerritoryGUID = FGuid(71, 72, 73, 74);
	First.HierarchyLevel = ETerritoryHierarchyLevel::District;
	First.State = ETerritoryState::Claimed;
	First.Availability = ETerritoryAvailability::Unlocked;
	First.CurrentOwner = Heroes;
	FReplicatedCaptureSummary Duplicate = First;
	Duplicate.TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	TestEqual(TEXT("Distinct tags cannot double-count the same saved district GUID"),
		ATerritoryWorldState::CountClaimedDistrictsForFaction({First, Duplicate}, Heroes), 1);
	Duplicate.TerritoryTag = FGameplayTag();
	TestEqual(TEXT("Tagged and GUID-only forms cannot double-count one district"),
		ATerritoryWorldState::CountClaimedDistrictsForFaction({First, Duplicate}, Heroes), 1);
	Duplicate.TerritoryGUID = FGuid(75, 76, 77, 78);
	TestEqual(TEXT("A distinct GUID-only district remains countable"),
		ATerritoryWorldState::CountClaimedDistrictsForFaction({First, Duplicate}, Heroes), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFWorldStateTreatyMigration,
	"TerritoryFramework.WorldState.Regression.CanonicalTreatyMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFWorldStateTreatyMigration::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("WorldState migration world exists"), World)) return false;
	ATerritoryWorldState* State = NewObject<ATerritoryWorldState>(World->PersistentLevel);
	State->SetRole(ROLE_Authority);
	FReplicatedTreaty Old;
	Old.FactionA = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Old.FactionB = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Old.TreatyID = FGuid(1, 2, 0, 0);
	Old.State = EDiplomacyState::Alliance;
	Old.SignedGameTime = 1.f;
	FReplicatedTreaty Newer = Old;
	Swap(Newer.FactionA, Newer.FactionB);
	Newer.State = EDiplomacyState::Ceasefire;
	Newer.SignedGameTime = 2.f;
	Newer.bPermanent = false;
	Newer.ExpiryGameTime = 20.f;
	const FArrayProperty* SavedProperty = FindFProperty<FArrayProperty>(State->GetClass(), TEXT("SavedTreaties"));
	if (!TestNotNull(TEXT("Native saved treaty array exists"), SavedProperty))
	{
		World->DestroyWorld(false);
		return false;
	}
	TArray<FReplicatedTreaty>* Saved = SavedProperty->ContainerPtrToValuePtr<TArray<FReplicatedTreaty>>(State);
	*Saved = {Old, Newer};
	State->ImportPersistentState();
	TestEqual(TEXT("Migrated authoritative treaty cache has one pair"), State->GetAllTreaties().Num(), 1);
	TestEqual(TEXT("Late-join treaty snapshot matches the restored subsystem"),
		State->GetTreatyBetween(Old.FactionA, Old.FactionB).State, EDiplomacyState::Ceasefire);
	TestEqual(TEXT("Late-join snapshot retains timed expiry"),
		State->GetTreatyBetween(Old.FactionA, Old.FactionB).ExpiryGameTime, 20.0);
	FTreatyRecord Record;
	Record.FactionA = Old.FactionA;
	Record.FactionB = Old.FactionB;
	const FGuid StableID = FGuid::NewDeterministicGuid(
		TEXT("TerritoryFramework.Treaty|Narrative.Factions.Bandits|Narrative.Factions.Heroes"));
	TestEqual(TEXT("Treaty ID uses stable names instead of process-local FName hashes"), Record.GetCanonicalKey(), StableID);
	Swap(Record.FactionA, Record.FactionB);
	TestEqual(TEXT("Reversing a pair preserves its durable key"), Record.GetCanonicalKey(), StableID);
	TestEqual(TEXT("Legacy hash IDs migrate with the authoritative cache"),
		State->GetTreatyBetween(Old.FactionA, Old.FactionB).TreatyID, StableID);
	State->ExportPersistentState();
	State->ImportPersistentState();
	TestEqual(TEXT("Save/reload preserves the canonical pair"), State->GetAllTreaties().Num(), 1);
	TestEqual(TEXT("Save/reload preserves the durable treaty ID"),
		State->GetTreatyBetween(Old.FactionA, Old.FactionB).TreatyID, StableID);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFWorldStateHistoryLimits,
	"TerritoryFramework.WorldState.Regression.HistoryLimitBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFWorldStateHistoryLimits::RunTest(const FString& Parameters)
{
	// The isolated world has not begun play; permit its reflected native actor callback.
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	FReplicatedTransaction Transaction;
	Transaction.TransactionID = FGuid::NewGuid();
	Transaction.Faction = Heroes;
	ATerritoryWorldState* Detached = NewObject<ATerritoryWorldState>();
	Detached->SetRole(ROLE_Authority);
	Detached->RecordTransaction(Transaction);
	TestEqual(TEXT("A detached authoritative snapshot does not dereference a missing world"),
		Detached->GetTransactionHistory(Heroes, 10).Num(), 1);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("History world exists"), World)) return false;
	ATerritoryWorldState* State = NewObject<ATerritoryWorldState>(World->PersistentLevel);
	State->SetRole(ROLE_Authority);
	UTerritoryEconomySubsystem* Economy = World->GetSubsystem<UTerritoryEconomySubsystem>();
	FScriptDelegate TransactionListener;
	TransactionListener.BindUFunction(State, TEXT("OnTransactionRecordedLive"));
	Economy->OnTransactionRecorded.Add(TransactionListener);
	Economy->MaxTransactionHistory = -3;
	State->RecordTransaction(Transaction);
	TestEqual(TEXT("Negative history limit safely stores no snapshot entries"), State->GetTransactionHistory(Heroes, 10).Num(), 0);
	FTerritoryTransaction NativeTransaction;
	NativeTransaction.TransactionID = Transaction.TransactionID;
	NativeTransaction.Faction = Heroes;
	Economy->RestoreTransactionHistory({NativeTransaction});
	TestEqual(TEXT("Negative history limit safely trims restored economy entries"), Economy->GetAllTransactionHistory().Num(), 0);
	Economy->MaxTransactionHistory = 2;
	for (int32 Amount = 1; Amount <= 3; ++Amount)
	{
		Transaction.Amount = Amount;
		State->RecordTransaction(Transaction);
	}
	TestEqual(TEXT("Configured history retains exactly the newest entries"), State->GetTransactionHistory(Heroes, 10).Num(), 2);
	NativeTransaction.Amount = 4;
	Economy->OnTransactionRecorded.Broadcast(NativeTransaction);
	TestEqual(TEXT("Live replication uses the same configured limit"), State->GetTransactionHistory(Heroes, 10).Num(), 2);
	TestEqual(TEXT("Live history still contains the newest transaction"), State->GetTransactionHistory(Heroes, 1)[0].Amount, 4);
	State->SetRole(ROLE_SimulatedProxy);
	NativeTransaction.Amount = 5;
	Economy->OnTransactionRecorded.Broadcast(NativeTransaction);
	TestEqual(TEXT("Client role cannot append authoritative history"), State->GetTransactionHistory(Heroes, 1)[0].Amount, 4);
	World->DestroyWorld(false);
	return true;
}

#endif
