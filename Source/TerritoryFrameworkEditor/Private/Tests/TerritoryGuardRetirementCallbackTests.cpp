#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardRetirementCallbacks,
	"TerritoryFramework.Guards.Regression.RetirementCallbackKeepsReloadedGarrison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardRetirementCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Guard cleanup world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	Territory->SetActorGUID_Implementation(FGuid(271, 272, 273, 274));
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord SavedTerritory;
	TestTrue(TEXT("Native archive captures the replacement campaign state"), Save->CreateActorRecord(Territory, SavedTerritory));
	ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	Post->SetActorGUID_Implementation(FGuid(275, 276, 277, 278));
	Post->ReserveSlots = 3;
	Post->InitializeReserves();
	Post->SetResolvedTerritory(Territory);
	UNPCDefinition* Definition = NewObject<UNPCDefinition>();
	Definition->CharacterID = TEXT("AuditCleanupGuardCharacter");
	Definition->NPCID = TEXT("AuditCleanupGuardNPC");
	Definition->NPCClassPath = ATerritoryGuardCharacter::StaticClass();
	Definition->bAllowMultipleInstances = true;
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const auto AddGuard = [&](float X)
	{
		ATerritoryGuardCharacter* Guard = ATerritoryGuardCharacter::SpawnThroughNarrative(
			World->GetSubsystem<UNarrativeCharacterSubsystem>(), Definition, Heroes,
			Territory->GetActorGUID_Implementation(), FGuid::NewGuid(),
			FTransform(FVector(X, 0.f, 100.f)), TEXT("AuditCleanupPost"), nullptr, {}, Territory, Post);
		if (!Guard) return Guard;
		Territory->SpawnedGuards.Add(Guard);
		Territory->RegisterDefender(Guard);
		Post->RegisterSpawnedGuard(Guard);
		return Guard;
	};
	ATerritoryGuardCharacter* OldGuard = AddGuard(1000.f);
	if (!TestNotNull(TEXT("Native guard spawning succeeds"), OldGuard))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	UNPCActivityComponent* Activities = OldGuard->GetActivityComponent();
	if (!TestNotNull(TEXT("Guard has its actual Native activity component"), Activities))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	Activities->SetActiveFlag(true);
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	Activities->OnComponentDeactivated.AddDynamic(Probe, &UTerritoryAuditEventProbe::ComponentDeactivated);
	ATerritoryGuardCharacter* NewGuard = nullptr;
	int32 CallbackCount = 0;
	Probe->ComponentDeactivationCallback = [&]()
	{
		++CallbackCount;
		Save->LoadActorFromRecord(Territory, SavedTerritory);
		NewGuard = AddGuard(2000.f);
	};
	Territory->DespawnGuards();
	Probe->ComponentDeactivationCallback = nullptr;
	TestEqual(TEXT("Actual Native deactivation performs one nested reload"), CallbackCount, 1);
	TestTrue(TEXT("The old guard is retired"), !IsValid(OldGuard) || OldGuard->IsActorBeingDestroyed());
	TestTrue(TEXT("The replacement guard stays alive"), IsValid(NewGuard) && !NewGuard->IsActorBeingDestroyed());
	TestEqual(TEXT("Old cleanup retains the replacement guard list"), Territory->GetSpawnedGuardCount(), 1);
	TestTrue(TEXT("Old cleanup retains the replacement defender"), Territory->GetRegisteredDefenders().Contains(NewGuard));
	TestEqual(TEXT("The same post retains the replacement occupant"), Post->GetActiveGuardCount(), 1);
	TestEqual(TEXT("Reload cleanup spends no reserves"), Post->GetReserveCount(), 3);
	FNarrativeActorRecord PostRecord;
	TestTrue(TEXT("Native save captures the replacement post count"), Save->CreateActorRecord(Post, PostRecord));
	ATerritoryGuardSpawnPoint* LoadedPost = World->SpawnActor<ATerritoryGuardSpawnPoint>();
	Save->LoadActorFromRecord(LoadedPost, PostRecord);
	TestEqual(TEXT("Native post round-trip preserves one living guard"), LoadedPost->GetSavedActiveGuardCount(), 1);
	TestEqual(TEXT("Native post round-trip preserves unspent reserves"), LoadedPost->GetReserveCount(), 3);
	Territory->SetRole(ROLE_SimulatedProxy);
	Territory->DespawnGuards();
	TestTrue(TEXT("Client cleanup cannot erase the authoritative guard list"), Territory->SpawnedGuards.Contains(NewGuard));
	TestTrue(TEXT("Client cleanup cannot destroy a guard"), IsValid(NewGuard) && !NewGuard->IsActorBeingDestroyed());
	Territory->SetRole(ROLE_Authority);
	Territory->DespawnGuards();
	TestEqual(TEXT("A later independent server cleanup still works"), Territory->GetSpawnedGuardCount(), 0);
	TestEqual(TEXT("A later server cleanup frees the post"), Post->GetActiveGuardCount(), 0);
	TestEqual(TEXT("Manual cleanup never queues replacement"), Post->GetPendingReserveCount(), 0);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
