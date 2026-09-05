#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Engine/World.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "NarrativeSave.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardReserveReconciliation,
	"TerritoryFramework.Guards.Regression.FiniteReserveReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardReserveReconciliation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Reserve world exists"), World)) return false;
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	Post->ReserveSlots = 3;
	Post->InitializeReserves();
	TestEqual(TEXT("A fresh post receives its authored reserves once"), Post->GetReserveCount(), 3);
	Post->CurrentReserveCount = 0;
	Post->ResetReserveState();
	TestEqual(TEXT("Reconciliation cannot refill an exhausted finite reserve"), Post->GetReserveCount(), 0);
	Post->CurrentReserveCount = 0;
	Post->ReserveOwnershipPolicy = EReserveOwnershipPolicy::PersistWithPost;
	Post->HandleOwnershipTransition(EOwnershipTransitionReason::OwnerChanged);
	Post->ResetReserveState();
	TestEqual(TEXT("A new owner cannot bypass the post's preserve-reserve policy"), Post->GetReserveCount(), 0);
	Post->ReserveOwnershipPolicy = EReserveOwnershipPolicy::RefillOnOwnerChange;
	Post->HandleOwnershipTransition(EOwnershipTransitionReason::OwnerChanged);
	TestEqual(TEXT("Explicit refill policy still replenishes on ownership change"), Post->GetReserveCount(), 3);
	Post->CurrentReserveCount = 1;
	Post->ResetReserveState();
	TestEqual(TEXT("Reconciliation retains a partially consumed reserve"), Post->GetReserveCount(), 1);
	ATerritoryGuardSpawnPoint* LatePost = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	LatePost->ReserveSlots = 3;
	LatePost->SavedActiveGuardCount = 1;
	LatePost->InitializeReserves();
	TestEqual(TEXT("An initial guard registered before post initialization does not suppress reserves"),
		LatePost->GetReserveCount(), 3);

	ATerritoryGuardSpawnPoint* Disabled = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	Disabled->ReserveSlots = 3;
	Disabled->GuardPostDefinition = NewObject<UTerritoryGuardPostDefinition>();
	Disabled->GuardPostDefinition->ReserveSlots = 0;
	Disabled->InitializeReserves();
	TestEqual(TEXT("A reusable definition with zero reserves overrides the row fallback"),
		Disabled->GetReserveCount(), 0);
	Disabled->ReserveOwnershipPolicy = EReserveOwnershipPolicy::ResetToDefinitionOnOwnerChange;
	Disabled->HandleOwnershipTransition(EOwnershipTransitionReason::OwnerChanged);
	Disabled->ResetReserveState();
	TestEqual(TEXT("Zero-reserve definition survives ownership reconciliation"), Disabled->GetReserveCount(), 0);

	UNarrativeSaveSubsystem* Save = NewObject<UNarrativeSaveSubsystem>();
	Post->SpawnPointGUID = FGuid(141, 142, 143, 144);
	Post->CurrentReserveCount = 0;
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative creates a stable guard post actor record"), Save->CreateActorRecord(Post, Record));
	FNarrativeActorRecord LegacyRecord = Record;
	LegacyRecord.ByteData.Reset();
	{
		// Reproduce the old plugin serialization through the actor base, using the
		// same delta archive Narrative uses. Zero counts are omitted from this row.
		FMemoryWriter Writer(LegacyRecord.ByteData);
		FObjectAndNameAsStringProxyArchive LegacyArchive(Writer, true);
		LegacyArchive.ArIsSaveGame = true;
		Post->AActor::Serialize(LegacyArchive);
	}
	// Recreated posts represent World Partition load order: initialization may run
	// before or after Narrative loads the actor's saved record.
	for (const FNarrativeActorRecord* SavedRecord : {&Record, &LegacyRecord})
	{
		for (bool bInitializeBeforeLoad : {false, true})
		{
			ATerritoryGuardSpawnPoint* Reloaded = World->SpawnActor<ATerritoryGuardSpawnPoint>();
			Reloaded->ReserveSlots = 3;
			if (bInitializeBeforeLoad) Reloaded->InitializeReserves();
			Save->LoadActorFromRecord(Reloaded, *SavedRecord);
			Reloaded->ResetReserveState();
			TestEqual(TEXT("Narrative load order preserves exhausted reserves"), Reloaded->GetReserveCount(), 0);
			TestEqual(TEXT("Narrative actor record preserves authored identity"),
				Reloaded->GetActorGUID_Implementation(), Record.ActorGUID);
		}
	}
	ATerritoryGuardSpawnPoint* ClientPost = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	ClientPost->SetRole(ROLE_SimulatedProxy);
	ClientPost->ReserveSlots = 3;
	ClientPost->InitializeReserves();
	TestEqual(TEXT("Client initialization cannot mint reserves"), ClientPost->GetReserveCount(), 0);
	ClientPost->HandleOwnershipTransition(EOwnershipTransitionReason::OwnerChanged);
	TestEqual(TEXT("Client ownership callbacks cannot refill reserves"), ClientPost->GetReserveCount(), 0);
	World->DestroyWorld(false);
	return true;
}

#endif
