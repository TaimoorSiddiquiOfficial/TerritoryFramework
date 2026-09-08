#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GAS/AbilityConfiguration.h"
#include "GameplayEffect.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/TerritoryStoryConditions.h"
#include "UnrealFramework/NarrativeGameState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStoryReinforcements,
	"TerritoryFramework.CounterAttack.Story.OwnerReinforcementsBeforeHandover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStoryReinforcements::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Story reinforcement world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	// The Native base relies on its project Blueprint for a stable save GUID.
	// This synthetic clock is not a spawned/saved campaign actor.
	auto* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
	Clock->SetRole(ROLE_Authority);
	World->SetGameState(Clock);
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Pawn = World->SpawnActor<APawn>();
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(48, 1, 2, 3);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->ApplyToTerritory(Place);
	auto* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("OwnerReinforcementTest"); NPC->NPCID = NPC->CharacterID;
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	NPC->AbilityConfiguration = NewObject<UAbilityConfiguration>();
	NPC->AbilityConfiguration->DefaultAttributes = UGameplayEffect::StaticClass();
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = Bandits; Force.AttackerDefinition = NPC; Force.PlannedForce = 2; Force.WaveSize = 2;
	Definition->CounterAttackProfile->FactionForces = {Force};
	FTerritoryOwnershipData Ownership = Place->GetOwnershipData();
	Ownership.OwningFaction = Bandits; Ownership.State = ETerritoryState::Claimed; Ownership.DesiredGuardCount = 0;
	Place->CommitOwnershipData(Ownership);
	Registry->RegisterTerritory(Place);
	Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::War);
	FScriptDelegate Listener; Listener.BindUFunction(State, TEXT("OnAssaultChangedLive"));
	Counter->OnAssaultChanged.Add(Listener);
	FTerritoryStoryPursuitOptions Options;
	Options.OpposingFaction = Heroes; Options.ScenarioID = TEXT("Blacksmith_BeforeHandover");
	FText Reason;
	const auto Schedule = [&](bool bImmediate)
	{
		return Counter->TryScheduleAssaultAdvancedWithReason(Place, Bandits,
			ETerritoryAssaultLaunchMode::StoryReinforcements, Options, bImmediate, Reason);
	};
	TestFalse(TEXT("A normal counterattack still rejects its own Place"),
		Counter->TryScheduleAssaultAdvancedWithReason(Place, Bandits,
			ETerritoryAssaultLaunchMode::StrategicCounterattack, Options, true, Reason));
	Place->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client-role Place cannot schedule owner reinforcements"), Schedule(false));
	Place->SetRole(ROLE_Authority);
	Options.OpposingFaction = Bandits;
	TestFalse(TEXT("Reinforcements cannot fight their own faction"), Schedule(false));
	Options.OpposingFaction = Heroes;
	TestTrue(*FString::Printf(TEXT("Explicit owner reinforcements admitted: %s"), *Reason.ToString()), Schedule(false));
	TestFalse(TEXT("Repeated dialogue or a second player cannot duplicate the scenario"), Schedule(false));
	FTerritoryAssaultRecord Record = Counter->GetPersistentState()[0];
	TestEqual(TEXT("The sender continues to own the Place"), Place->GetOwningFaction(), Bandits);
	TestEqual(TEXT("Diplomacy and notifications use the opponent, not the owner"), Record.DefendingFaction, Heroes);
	TestFalse(TEXT("The record forbids capture participation"), Record.bAllowsTerritoryCapture);
	TestEqual(TEXT("Preparation alone creates no NPCs"), Record.AliveForce, 0);
	TestFalse(TEXT("An immediate request with no route fails honestly"),
		Counter->StartAssaultImmediately(Counter->Assaults.FindChecked(Record.AssaultID), Place, &Reason));
	TestEqual(TEXT("Missing routes cancel rather than count as victory"),
		Counter->Assaults.FindChecked(Record.AssaultID).Resolution, ETerritoryAssaultResolution::InvalidApproachOrRoute);

	// Exercise the same physical admission/death path after route validation. The
	// real map probe separately verifies navigation and immediate vehicle ingress.
	Record.State = ETerritoryAssaultState::Active; Record.Resolution = ETerritoryAssaultResolution::None;
	Record.PlannedForce = Record.PendingReserveForce = 2; Record.WaveSize = 2;
	Record.DecisionSeed = 48048; Record.DecisionRoll = 0.375f;
	Counter->RestorePersistentState({Record});
	FTerritoryAssaultApproach Foot; Foot.ApproachID = TEXT("StoryTestFoot");
	ATerritoryAssaultCharacter* First = Counter->SpawnParticipant(Counter->Assaults.FindChecked(Record.AssaultID),
		Place, Force, NPC, Foot, FTransform(FVector(1000, 1000, 100)), INDEX_NONE);
	ATerritoryAssaultCharacter* Second = Counter->SpawnParticipant(Counter->Assaults.FindChecked(Record.AssaultID),
		Place, Force, NPC, Foot, FTransform(FVector(2500, 1000, 100)), INDEX_NONE);
	TestNotNull(TEXT("First real Narrative reinforcement"), First);
	TestNotNull(TEXT("Second real Narrative reinforcement"), Second);
	if (First && Second)
	{
		TestFalse(TEXT("Physical reinforcements cannot register capture pressure"), First->AssaultParticipant->AllowsTerritoryCapture());
		TestFalse(TEXT("No capture registration is created"), First->AssaultParticipant->IsCaptureRegistered());
		First->AssaultParticipant->Retire(true); First->AssaultParticipant->Retire(true);
		TestEqual(TEXT("Repeated death consumes one finite attacker"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 1);
		Registry->UnregisterTerritory(Place);
		Counter->AdvanceAssault(Counter->Assaults.FindChecked(Record.AssaultID));
		TestEqual(TEXT("Streaming absence cannot erase the living reinforcement"), Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 1);
		Registry->RegisterTerritory(Place);
		Second->AssaultParticipant->Retire(true);
	}
	FTerritoryAssaultRecord Defeated = Counter->Assaults.FindChecked(Record.AssaultID);
	TestTrue(TEXT("A finite defeat becomes the durable story outcome"), Defeated.IsRetainedStoryOutcome());
	TestEqual(TEXT("Victory did not hand over the Place"), Place->GetOwningFaction(), Bandits);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Save(Writer, false); Save.ArIsSaveGame = true;
	FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Save, &Defeated, nullptr);
	FTerritoryAssaultRecord Loaded;
	FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Load(Reader, true); Load.ArIsSaveGame = true;
	FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Load, &Loaded, nullptr);
	Counter->RestorePersistentState({Loaded});
	TestEqual(TEXT("Save preserves the opponent faction"), Loaded.DefendingFaction, Heroes);
	TestEqual(TEXT("Save preserves the same decision"), Loaded.DecisionRoll, Record.DecisionRoll);
	TestEqual(TEXT("Save preserves all finite casualties"), Loaded.KilledForce, 2);
	TestFalse(TEXT("Reopening the owner dialogue after load cannot spawn the completed force"), Schedule(true));
	{
		TGuardValue<int32> History(GetMutableDefault<UTerritoryDeveloperSettings>()->MaxRetainedAssaultRecords, 0);
		Counter->TrimTerminalHistory(); Counter->OnAssaultChanged.Broadcast(Loaded);
		TestEqual(TEXT("Ordinary history trimming retains the story receipt"), Counter->GetPersistentState().Num(), 1);
		TestTrue(TEXT("The replicated late-join model retains the same receipt"),
			State->GetAllAssaultSummaries().ContainsByPredicate([&](const FTerritoryAssaultRecord& R) { return R.AssaultID == Loaded.AssaultID; }));
	}
	auto* Gate = NewObject<UTerritoryAssaultCondition>();
	Gate->TerritoryToCheck = Definition->TerritoryTag; Gate->AttackingFaction = Bandits;
	Gate->ScenarioID = Options.ScenarioID; Gate->Query = ETerritoryAssaultConditionQuery::LatestResolution;
	Gate->RequiredResolution = ETerritoryAssaultResolution::AllAttackersRemoved;
	TestTrue(TEXT("Exact scenario defeat unlocks its Native condition"), Gate->CheckCondition(Pawn, nullptr, nullptr));
	Gate->AttackingFaction = Heroes;
	TestFalse(TEXT("Another faction's victory cannot unlock it"), Gate->CheckCondition(Pawn, nullptr, nullptr));
	Gate->AttackingFaction = Bandits; Gate->ScenarioID = TEXT("DifferentStory");
	TestFalse(TEXT("Another story's victory cannot unlock it"), Gate->CheckCondition(Pawn, nullptr, nullptr));
	Gate->ScenarioID = Options.ScenarioID;
	Record.AssaultID = FGuid::NewGuid(); Record.AliveForce = 0; Record.PendingReserveForce = 2;
	Counter->RestorePersistentState({Record});
	Diplomacy->SetDiplomacyState(Bandits, Heroes, EDiplomacyState::Ceasefire);
	TestEqual(TEXT("A ceasefire cancels owner reinforcements rather than claiming an ownership victory"),
		Counter->Assaults.FindChecked(Record.AssaultID).Resolution, ETerritoryAssaultResolution::DiplomacyBlocked);
	TestFalse(TEXT("Cancellation never unlocks the defeat branch"), Gate->CheckCondition(Pawn, nullptr, nullptr));
	Record.bAllowsTerritoryCapture = true;
	AddExpectedError(TEXT("invalid story reinforcement context"), EAutomationExpectedErrorFlags::Contains, 1);
	Counter->RestorePersistentState({Record});
	TestEqual(TEXT("Malformed saved reinforcement capture permission fails before reconstruction"),
		Counter->Assaults.FindChecked(Record.AssaultID).Resolution, ETerritoryAssaultResolution::ConfigurationInvalid);
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
