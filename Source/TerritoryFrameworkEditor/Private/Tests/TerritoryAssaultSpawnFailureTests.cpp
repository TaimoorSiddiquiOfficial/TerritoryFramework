#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AI/NPCDefinition.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSpawnFailureBudget,
	"TerritoryFramework.CounterAttack.SaveLoad.SpawnFailureBudgetSurvivesRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSpawnFailureBudget::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Retry fixture world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	auto* Profile = NewObject<UTerritoryCounterAttackProfile>();
	Profile->MaxConsecutiveSpawnFailures = 3;
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(430, 1, 2, 3);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = Profile;
	Definition->ApplyToTerritory(Place);
	auto* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("RetryBudgetNPC");
	NPC->NPCID = TEXT("RetryBudgetNPC");
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NPC;
	Profile->FactionForces = {Force};
	FScriptDelegate Listener;
	Listener.BindUFunction(State, TEXT("OnAssaultChangedLive"));
	Counter->OnAssaultChanged.Add(Listener);

	const auto RoundTrip = [](FTerritoryAssaultRecord Record)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Save(Writer, false);
		Save.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Save, &Record, nullptr);
		FTerritoryAssaultRecord Loaded;
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Load(Reader, true);
		Load.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Load, &Loaded, nullptr);
		return Loaded;
	};
	for (int32 Path = 0; Path < 3; ++Path)
	{
		for (const int32 SavedFailures : {0, MAX_int32 - 1, MAX_int32})
		{
			FTerritoryAssaultRecord Record;
			Record.AssaultID = FGuid(430, 4, Path, SavedFailures);
			Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
			Record.TargetTerritory = Definition->TerritoryTag;
			Record.AttackingFaction = Force.Faction;
			Record.State = ETerritoryAssaultState::Active;
			Record.PlannedForce = 1;
			Record.PendingReserveForce = 1;
			Record.WaveSize = 1;
			Record.SelectedApproaches = {TEXT("RemovedDeparture")};
			Record.DecisionSeed = 430;
			Record.DecisionRoll = 0.375f;
			Record.ConsecutiveSpawnFailures = SavedFailures;
			Record.PhysicalStateVersion = Path == 2 ? 0 : 1;
			if (Path == 1)
			{
				FTerritoryAssaultSurvivor Member;
				Member.SpawnGUID = FGuid(430, 5, 6, 7);
				Member.ApproachID = TEXT("RemovedDeparture");
				Record.PendingSurvivors = {Member};
			}
			if (Path == 2) Record.AliveForce = 1;
			Counter->RestorePersistentState({RoundTrip(Record)});
			const auto Attempt = [&]()
			{
				auto& Live = Counter->Assaults.FindChecked(Record.AssaultID);
				if (Path == 0) Counter->SpawnNextWave(Live, Place);
				else if (Path == 1) Counter->ReconstructParticipants(Live, Place);
				else Counter->MigrateLegacySurvivors(Live, Place);
			};
			if (Path < 2)
			{
				Place->SetRole(ROLE_SimulatedProxy);
				Attempt();
				TestEqual(TEXT("A client-role target cannot consume a retry"), Counter->Assaults.FindChecked(Record.AssaultID).ConsecutiveSpawnFailures, SavedFailures);
				Place->SetRole(ROLE_Authority);
			}
			Attempt();
			const auto Result = Counter->Assaults.FindChecked(Record.AssaultID);
			TestEqual(TEXT("Failed wave/physical/legacy retries never wrap"), Result.ConsecutiveSpawnFailures,
				SavedFailures == MAX_int32 ? MAX_int32 : SavedFailures + 1);
			TestEqual(TEXT("A failed deployment never becomes living force"), Result.AliveForce, 0);
			TestEqual(TEXT("Retry handling does not reroll the saved decision"), Result.DecisionRoll, Record.DecisionRoll);
			if (SavedFailures > 0)
			{
				TestEqual(TEXT("Exhausted deployment fails finitely"), Result.State, ETerritoryAssaultState::Cancelled);
				TestEqual(TEXT("The one remaining force slot is withdrawn, never replenished"), Result.WithdrawnForce, 1);
				const auto Summaries = State->GetAllAssaultSummaries();
				const auto* Published = Summaries.FindByPredicate([&](const auto& Row) { return Row.AssaultID == Record.AssaultID; });
				TestTrue(TEXT("The existing client/late-join projection receives terminal state"), Published && Published->State == Result.State);
			}
			else
			{
				TestEqual(TEXT("An ordinary first failure remains retryable"), Result.State, ETerritoryAssaultState::Active);
				Counter->RestorePersistentState({RoundTrip(Counter->GetPersistentState()[0])});
				Attempt();
				Attempt();
				TestEqual(TEXT("Save/reload cannot reset the ordinary three-attempt budget"), Counter->Assaults.FindChecked(Record.AssaultID).State, ETerritoryAssaultState::Cancelled);
			}
			Counter->RestorePersistentState({RoundTrip(Counter->GetPersistentState()[0])});
			TestEqual(TEXT("Reload preserves terminal failure without a new force"), Counter->Assaults.FindChecked(Record.AssaultID).State, ETerritoryAssaultState::Cancelled);
		}
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
