#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TerritoryDataTaskProbe.h"
#include "Tales/TerritoryNarrativeDataTask.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNarrativeDataTaskRecords,
	"TerritoryFramework.Tales.Regression.DataTaskListenerQuantityAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNarrativeDataTaskRecords::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Data task world exists"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	AActor* Owner = World->SpawnActor<AActor>();
	auto* Tales = NewObject<UTerritoryDataTaskProbe>(Owner);
	auto* Record = NewObject<UNarrativeDataTask>();
	Record->TaskName = TEXT("Overheard");
	const FString Argument(TEXT("Blacksmith Patrol"));
	auto MakeTask = [&](UTalesComponent* Component, bool History, FName TaskName = NAME_None)
	{
		auto* Task = NewObject<UTerritoryNarrativeDataTask>(Component, TaskName);
		Task->DataTask = Record;
		Task->Argument = Argument;
		Task->RequiredQuantity = 20;
		Task->bCountPreviousCompletions = History;
		Task->OwningComp = Component;
		Task->BeginTask();
		return Task;
	};
	Tales->Record(Record, Argument, 2);
	auto* First = MakeTask(Tales, false);
	auto* Second = MakeTask(Tales, false);
	TestEqual(TEXT("New-action task excludes previous records"), First->CurrentProgress, 0);
	Tales->Record(Record, Argument, 5);
	TestEqual(TEXT("Before Native commits, callback does not invent quantity one"), First->CurrentProgress, 0);
	First->RefreshFromRecords();
	Second->RefreshFromRecords();
	TestEqual(TEXT("Full bulk quantity reaches the first task"), First->CurrentProgress, 5);
	TestEqual(TEXT("Full bulk quantity reaches the parallel task"), Second->CurrentProgress, 5);
	First->EndTask();
	TestFalse(TEXT("Ended listener is removed"), Tales->OnNarrativeDataTaskCompleted.IsAlreadyBound(First, &UTerritoryNarrativeDataTask::OnDataTaskRecorded));
	TestTrue(TEXT("Ending one task preserves the other listener"), Tales->OnNarrativeDataTaskCompleted.IsAlreadyBound(Second, &UTerritoryNarrativeDataTask::OnDataTaskRecorded));
	TestTrue(TEXT("Ending a task preserves Native's own listener"), Tales->OnNarrativeDataTaskCompleted.GetAllObjects().Contains(Tales));
	Tales->Record(Record, TEXT("BLACKSMITHPATROL"), 3);
	Second->RefreshFromRecords();
	TestEqual(TEXT("Native argument normalization matches the same record"), Second->CurrentProgress, 8);
	TestEqual(TEXT("Ended task stays unchanged"), First->CurrentProgress, 5);
	Tales->Record(Record, TEXT("AnotherPlace"), 12);
	TestFalse(TEXT("Another argument does not queue a matching update"), World->GetTimerManager().TimerExists(Second->RefreshTimer));
	auto* History = MakeTask(Tales, true);
	TestEqual(TEXT("History task reads the entire Native record count"), History->CurrentProgress, 10);
	History->EndTask();
	Tales->Record(Record, Argument, 1);
	TestTrue(TEXT("Matching notification queues a deferred record read"), World->GetTimerManager().TimerExists(Second->RefreshTimer));
	Second->EndTask();
	TestFalse(TEXT("End clears a pending deferred callback"), World->GetTimerManager().TimerExists(Second->RefreshTimer));
	Second->BeginTask();
	TestEqual(TEXT("Re-entering a task starts a new baseline"), Second->CurrentProgress, 0);
	Tales->Record(Record, Argument, 4);
	Second->RefreshFromRecords();
	TestEqual(TEXT("Restarted task counts only its new actions"), Second->CurrentProgress, 4);

	// Save before a pending progress callback: the ledger has the new quantity,
	// but Native's branch progress snapshot has not caught up yet.
	Tales->Record(Record, Argument, 2);
	TestEqual(TEXT("Immediate save starts with pending task progress"), Second->CurrentProgress, 4);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Save(Writer, true);
	Save.ArIsSaveGame = true;
	Tales->Serialize(Save);
	auto* RestoredTales = NewObject<UTerritoryDataTaskProbe>(World->SpawnActor<AActor>());
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive Load(Reader, true);
	Load.ArIsSaveGame = true;
	RestoredTales->Serialize(Load);
	TestEqual(TEXT("Native SaveGame ledger retains all completed quantities"),
		RestoredTales->GetNumberOfTimesTaskWasCompleted(Record, Argument), 17);
	RestoredTales->bIsLoading = true;
	auto* Restored = MakeTask(RestoredTales, false, Second->GetFName());
	Restored->SetProgressInternal(4);
	RestoredTales->Record(Record, Argument, 3);
	RestoredTales->bIsLoading = false;
	Restored->RefreshFromRecords();
	TestEqual(TEXT("Restore preserves pending progress plus post-load events using the saved start"), Restored->CurrentProgress, 9);
	Owner->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot commit a Native data record"), Tales->Record(Record, Argument, 4));
	Tales->OnNarrativeDataTaskCompleted.Broadcast(Record, Argument);
	Second->RefreshFromRecords();
	TestEqual(TEXT("A client callback cannot advance authoritative quest progress"), Second->CurrentProgress, 4);
	Owner->SetRole(ROLE_Authority);
	First->DataTask = nullptr;
	First->BeginTask();
	TestFalse(TEXT("Missing data asset does not leave a listener bound"), First->bListening);
	Second->EndTask();
	Restored->EndTask();
	return true;
}

#endif
