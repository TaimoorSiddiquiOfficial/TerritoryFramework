#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultTargetStreamingWait,
	"TerritoryFramework.CounterAttack.SaveLoad.UnloadedTargetDoesNotExhaustGoalInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultTargetStreamingWait::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Target streaming fixture world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	auto* Territory = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	auto* Profile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(440, 1, 2, 3);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->InitialOwningFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	Definition->CounterAttackProfile = Profile;
	Profile->bContinueFiniteWavesAfterActivation = false;
	Definition->ApplyToTerritory(Territory);
	Registry->RegisterTerritory(Territory);
	auto* NPCDefinition = NewObject<UNPCDefinition>();
	NPCDefinition->CharacterID = TEXT("StreamingWaitCharacter");
	NPCDefinition->NPCID = TEXT("StreamingWaitNPC");
	NPCDefinition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPCDefinition->bAllowMultipleInstances = true;
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NPCDefinition;
	Profile->FactionForces = {Force};
	FTerritoryAssaultApproach Approach;
	Approach.ApproachID = TEXT("StreamingWaitFoot");
	Definition->CounterAttackApproaches = {Approach};
	Definition->ApplyToTerritory(Territory);
	Territory->ForceSetOwningFaction(Definition->InitialOwningFaction);
	if (!TestEqual(TEXT("Fixture has an authoritative claimed defender"), Territory->GetTerritoryState(), ETerritoryState::Claimed)) return false;
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(440, 4, 5, 6);
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = Force.Faction;
	Record.DefendingFaction = Definition->InitialOwningFaction;
	Record.bAllowsTerritoryCapture = false;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = Record.PendingReserveForce = 2;
	Record.PhysicalStateVersion = 1;
	Record.DecisionSeed = 440;
	Record.DecisionRoll = 0.375f;
	World->GetSubsystem<UTerritoryDiplomacySubsystem>()->DeclareWar(Record.AttackingFaction, Record.DefendingFaction);
	Counter->RestorePersistentState({Record});
	auto* NPC = Counter->SpawnParticipant(Counter->Assaults.FindChecked(Record.AssaultID),
		Territory, Force, NPCDefinition, Approach, FTransform(FVector(1000, 1000, 100)), INDEX_NONE);
	if (!TestNotNull(TEXT("Real Narrative spawn is admitted before its visual is ready"), NPC)) return false;
	UTerritoryAssaultParticipantComponent* Participant = NPC->AssaultParticipant;
	TestFalse(TEXT("Fixture has not initialized a Narrative assault goal"), NPC->IsNarrativeSpawnReady());
	Registry->UnregisterTerritory(Territory);
	TestNull(TEXT("The target is absent from the existing registry"), Participant->GetTargetTerritory());
	for (int32 Attempt = 0; Attempt < 45; ++Attempt) Participant->UpdateParticipation();
	TestFalse(TEXT("An unloaded target cannot retire a living admitted attacker"), Participant->HasRetired());
	TestEqual(TEXT("Streaming wait does not consume AI initialization retries"), Participant->GoalInitializationAttempts, 0);
	TestFalse(TEXT("An unloaded target receives no capture pressure"), Participant->IsCaptureRegistered());
	const auto Waiting = Counter->GetPersistentState()[0];
	TestEqual(TEXT("Living finite force survives target unload"), Waiting.AliveForce, 1);
	TestEqual(TEXT("Target unload is not a casualty or withdrawal"), Waiting.WithdrawnForce, 0);
	TestEqual(TEXT("Only the original unspawned reserve remains"), Waiting.PendingReserveForce, 1);
	TestEqual(TEXT("Save includes the same physical survivor"), Waiting.PendingSurvivors.Num(), 1);
	if (Participant->HasRetired()) return false;

	// A different actor using the same tag must not end the GUID-based wait.
	auto* WrongTarget = World->SpawnActor<ATerritoryProperty>();
	auto* WrongDefinition = DuplicateObject<UTerritoryPlaceDefinition>(Definition, GetTransientPackage());
	WrongDefinition->StableTerritoryGUID = FGuid(440, 7, 8, 9);
	WrongDefinition->ApplyToTerritory(WrongTarget);
	Registry->RegisterTerritory(WrongTarget);
	for (int32 Attempt = 0; Attempt < 45; ++Attempt) Participant->UpdateParticipation();
	TestNull(TEXT("A reused tag cannot substitute for the streamed target GUID"), Participant->GetTargetTerritory());
	TestFalse(TEXT("Wrong-GUID admission does not withdraw the waiting force"), Participant->HasRetired());
	Registry->UnregisterTerritory(WrongTarget);
	WrongTarget->Destroy();
	Registry->RegisterTerritory(Territory);
	TestTrue(TEXT("Returning target rebinds through its original stable GUID"), Participant->GetTargetTerritory() == Territory);
	if (!TestTrue(TEXT("Returning valid target keeps the assault active"), Counter->IsAssaultActive(Record.AssaultID))) return false;
	NPC->SetRole(ROLE_SimulatedProxy);
	Participant->UpdateParticipation();
	TestEqual(TEXT("Client updates cannot consume initialization retries"), Participant->GoalInitializationAttempts, 0);
	NPC->SetRole(ROLE_Authority);
	Participant->UpdateParticipation();
	TestEqual(TEXT("A loaded target resumes the real initialization budget"), Participant->GoalInitializationAttempts, 1);
	TestFalse(TEXT("First genuine initialization retry does not retire the NPC"), Participant->HasRetired());
	Registry->UnregisterTerritory(Territory);
	for (int32 Attempt = 0; Attempt < 45; ++Attempt) Participant->UpdateParticipation();
	TestEqual(TEXT("A later streaming wait preserves already consumed retries"), Participant->GoalInitializationAttempts, 1);
	Registry->RegisterTerritory(Territory);
	AddExpectedError(TEXT("could not initialize its Narrative goal/activity"), EAutomationExpectedErrorFlags::Contains, 1);
	for (int32 Attempt = 0; Attempt < 39; ++Attempt) Participant->UpdateParticipation();
	TestTrue(TEXT("A genuine loaded-target initialization failure still retires the NPC"), Participant->HasRetired());
	TestEqual(TEXT("The genuine failure is charged exactly once"), Counter->GetPersistentState()[0].WithdrawnForce, 1);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Save(Writer, false);
	Save.ArIsSaveGame = true;
	FTerritoryAssaultRecord Saved = Waiting;
	FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Save, &Saved, nullptr);
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive Load(Reader, true);
	Load.ArIsSaveGame = true;
	FTerritoryAssaultRecord Loaded;
	FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Load, &Loaded, nullptr);
	Registry->UnregisterTerritory(Territory);
	Counter->RestorePersistentState({Loaded});
	Counter->AdvanceAssault(Counter->Assaults.FindChecked(Record.AssaultID));
	const auto Restored = Counter->GetPersistentState()[0];
	if (TestEqual(TEXT("Reload preserves one physical survivor"), Restored.PendingSurvivors.Num(), 1)
		&& Waiting.PendingSurvivors.Num() == 1)
	{
		TestEqual(TEXT("Reload retains the exact original physical survivor GUID"), Restored.PendingSurvivors[0].SpawnGUID, Waiting.PendingSurvivors[0].SpawnGUID);
	}
	TestEqual(TEXT("Reloaded unloaded target remains active"), Restored.State, ETerritoryAssaultState::Active);
	TestEqual(TEXT("The finite budget is conserved across save/load"), Restored.GetAccountedForce(), 2);
	TestEqual(TEXT("Reload does not invent a withdrawal"), Restored.WithdrawnForce, 0);
	TestEqual(TEXT("Streaming and reload never reroll the decision"), Restored.DecisionRoll, Record.DecisionRoll);
	return true;
}

#endif
