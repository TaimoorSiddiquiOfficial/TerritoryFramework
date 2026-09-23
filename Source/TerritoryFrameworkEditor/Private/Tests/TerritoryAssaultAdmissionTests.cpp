#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/NPCDefinition.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GAS/AbilityConfiguration.h"
#include "GameplayEffect.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/Quest.h"
#include "Tales/QuestSM.h"
#include "Tales/QuestBlueprintGeneratedClass.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryAssaultAdmissionTask.h"
#include "Tales/TerritoryAssaultTask.h"
#include "Tales/TerritoryStoryEvents.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultAdmissionLifecycle,
	"TerritoryFramework.Tales.Regression.DurableAssaultAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultAdmissionLifecycle::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Admission world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	auto* Clock = NewObject<ANarrativeGameState>(World->PersistentLevel);
	Clock->SetRole(ROLE_Authority);
	World->SetGameState(Clock);
	auto* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	auto* Tales = Controller->GetTalesComponent();
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	const FGameplayTag PlaceTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	auto MakeRequest = [&](UObject* Outer)
	{
		auto* Request = NewObject<UTerritoryScheduleEnemyWaveEvent>(Outer);
		Request->TargetTerritory = PlaceTag;
		Request->AttackingFaction = Bandits;
		Request->ScenarioID = TEXT("AdmissionRegression");
		return Request;
	};
	auto* Invalid = NewObject<UTerritoryAssaultAdmissionTask>(Tales);
	Invalid->OwningComp = Tales;
	Invalid->BeginTask();
	TestFalse(TEXT("Missing request does not complete"), Invalid->IsComplete());
	TestFalse(TEXT("Missing request explains refusal"), Invalid->LastAdmissionFailure.IsEmpty());
	Invalid->EndTask();

	// Use a shipped Native generated class and real Tales save/load. The scoped
	// two-state template makes this regression independent of a host game's story.
	auto* QuestClass = LoadClass<UQuest>(nullptr,
		TEXT("/TerritoryFramework/NQ_CaptureBlacksmith.NQ_CaptureBlacksmith_C"));
	if (!TestNotNull(TEXT("Portable Native integration quest"), QuestClass)) return false;
	auto* GeneratedClass = CastChecked<UQuestBlueprintGeneratedClass>(QuestClass);
	UQuest* OriginalTemplate = GeneratedClass->GetQuestTemplate();
	ON_SCOPE_EXIT { GeneratedClass->SetQuestTemplate(OriginalTemplate); };
	auto* Template = NewObject<UQuest>();
	auto* TemplateState = NewObject<UQuestState>(Template);
	TemplateState->SetID(TEXT("QuestState_10"));
	auto* Success = NewObject<UQuestState>(Template);
	Success->SetID(TEXT("AdmissionSuccess"));
	Success->StateNodeType = EStateNodeType::Success;
	auto* TemplateBranch = NewObject<UQuestBranch>(Template);
	TemplateBranch->SetID(TEXT("QuestBranch_309"));
	TemplateBranch->DestinationState = Success;
	TemplateState->Branches.Add(TemplateBranch);
	Template->AddState(TemplateState);
	Template->AddState(Success);
	Template->AddBranch(TemplateBranch);
	Template->SetQuestStartState(TemplateState);
	auto* Victory = NewObject<UTerritoryAssaultTask>(TemplateBranch);
	Victory->TargetTerritory = PlaceTag;
	Victory->AttackingFaction = Bandits;
	Victory->ScenarioID = TEXT("AdmissionRegression");
	TemplateBranch->QuestTasks.Add(Victory);
	auto* Prototype = NewObject<UTerritoryAssaultAdmissionTask>(TemplateBranch);
	Prototype->Request = MakeRequest(Prototype);
	TemplateBranch->QuestTasks.Add(Prototype);
	GeneratedClass->SetQuestTemplate(Template);
	const int32 TaskIndex = TemplateBranch->QuestTasks.Num() - 1;
	UQuest* Quest = Tales->BeginQuest(QuestClass, TEXT("QuestState_10"));
	if (!TestNotNull(TEXT("Native defence quest starts"), Quest)) return false;
	ON_SCOPE_EXIT { Tales->ForgetQuest(QuestClass); };
	auto FindTask = [&]()
	{
		return CastChecked<UTerritoryAssaultAdmissionTask>(
			Tales->GetQuestInstance(QuestClass)->GetBranch(TEXT("QuestBranch_309"))->QuestTasks[TaskIndex]);
	};
	auto RoundTrip = [&]()
	{
		Tales->PrepareForSave_Implementation();
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Save(Writer, true); Save.ArIsSaveGame = true;
		Tales->Serialize(Save);
		Tales->ForgetQuest(QuestClass);
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Load(Reader, true); Load.ArIsSaveGame = true;
		Tales->Serialize(Load);
		Tales->Load_Implementation();
	};
	TestFalse(TEXT("Unloaded target leaves Native task pending"), FindTask()->IsComplete());
	TestTrue(TEXT("No unloaded force is admitted"), Counter->GetAllAssaults().IsEmpty());
	RoundTrip();
	TestFalse(TEXT("Native save retains pre-admission intent"), FindTask()->IsComplete());

	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = PlaceTag;
	Definition->StableTerritoryGUID = FGuid(93, 21, 45, 8);
	Definition->InitialGuardCount = 0;
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	auto* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("AdmissionTest"); NPC->NPCID = NPC->CharacterID;
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	NPC->AbilityConfiguration = NewObject<UAbilityConfiguration>();
	NPC->AbilityConfiguration->DefaultAttributes = UGameplayEffect::StaticClass();
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = Bandits; Force.AttackerDefinition = NPC; Force.PlannedForce = 2; Force.WaveSize = 2;
	Definition->CounterAttackProfile->FactionForces = {Force};
	Definition->ApplyToTerritory(Place);
	FTerritoryOwnershipData Ownership = Place->GetOwnershipData();
	Ownership.OwningFaction = Heroes; Ownership.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Ownership);
	Registry->RegisterTerritory(Place);
	World->GetSubsystem<UTerritoryDiplomacySubsystem>()->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::NonAggression);
	FindTask()->TickTask_Implementation();
	TestTrue(TEXT("Peace refuses scheduling without losing the pending task"), Counter->GetAllAssaults().IsEmpty());
	TestFalse(TEXT("Scheduler refusal is inspectable"), FindTask()->LastAdmissionFailure.IsEmpty());
	// A retired, never-loaded target must release its reservation before this
	// unrelated pending quest can enter a one-slot global/per-faction budget.
	auto* WorldState = World->SpawnActor<ATerritoryWorldState>();
	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();
	TGuardValue<int32> GlobalBudget(Settings->MaxConcurrentScheduledAssaults, 1);
	TGuardValue<int32> FactionBudget(Settings->MaxConcurrentAssaultsPerFaction, 1);
	FTerritoryAssaultRecord Retired;
	Retired.AssaultID = FGuid::NewGuid();
	Retired.TargetTerritoryGUID = FGuid::NewGuid();
	Retired.TargetTerritory = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Warehouse"));
	Retired.AttackingFaction = Bandits; Retired.DefendingFaction = Heroes;
	Retired.State = ETerritoryAssaultState::WaitingForPlayerProximity;
	Retired.PlannedForce = 4; Retired.PendingReserveForce = 3; Retired.KilledForce = 1;
	Counter->RestorePersistentState({Retired});
	World->GetSubsystem<UTerritoryDiplomacySubsystem>()->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
	FindTask()->TickTask_Implementation();
	TestFalse(TEXT("An ordinary unloaded target still reserves the sole budget slot"), FindTask()->IsComplete());
	WorldState->RetiredDirectoryGUIDs.Add(Retired.TargetTerritoryGUID);
	WorldState->RefreshStrategicDirectory();
	FTerritoryAssaultRecord RetirementResult;
	TestTrue(TEXT("Retirement preserves a terminal receipt"), Counter->GetAssault(Retired.AssaultID, RetirementResult));
	TestEqual(TEXT("Retirement cancels through scheduler"), RetirementResult.State, ETerritoryAssaultState::Cancelled);
	TestEqual(TEXT("Retirement is not a victory"), RetirementResult.Resolution, ETerritoryAssaultResolution::InvalidTerritory);
	TestEqual(TEXT("Retirement preserves casualties"), RetirementResult.KilledForce, 1);
	TestEqual(TEXT("Retirement releases surviving force"), RetirementResult.WithdrawnForce, 3);
	TestEqual(TEXT("Retirement publishes zero active/reserve force"), RetirementResult.AliveForce + RetirementResult.PendingReserveForce, 0);
	Counter->RestorePersistentState({Retired});
	Counter->GetAssault(Retired.AssaultID, RetirementResult);
	TestTrue(TEXT("Loading an older pending record applies the same retirement migration"), RetirementResult.IsTerminal());
	WorldState->ExportPersistentState();
	WorldState->ImportPersistentState();
	TestTrue(TEXT("Late-join projection contains terminal migration"), WorldState->GetAllAssaultSummaries().ContainsByPredicate(
		[&](const auto& Row) { return Row.AssaultID == Retired.AssaultID && Row.IsTerminal() && Row.KilledForce == 1; }));
	FindTask()->TickTask_Implementation();
	const auto Records = Counter->GetAllAssaults();
	const auto* NewAdmission = Records.FindByPredicate([&](const auto& Row) { return Row.AssaultID != Retired.AssaultID; });
	if (!TestNotNull(TEXT("Retired reservation permits one new force in a one-slot budget"), NewAdmission)) return false;
	const FTerritoryAssaultRecord Admitted = *NewAdmission;
	TestTrue(TEXT("Admission completes Native progress"), FindTask()->IsComplete());
	TestEqual(TEXT("Admission does not advance the victory-gated story"),
		Tales->GetQuestInstance(QuestClass)->GetCurrentState()->GetID(), FName(TEXT("QuestState_10")));
	TestEqual(TEXT("Admission alone spawns no physical attackers"), Admitted.AliveForce, 0);
	FindTask()->TickTask_Implementation();
	RoundTrip();
	FindTask()->TickTask_Implementation();
	TestTrue(TEXT("Native save restores admission receipt"), FindTask()->IsComplete());
	TestEqual(TEXT("Reload keeps the admission and retirement receipts"), Counter->GetAllAssaults().Num(), 2);
    FTerritoryAssaultRecord Reloaded;
    TestTrue(TEXT("Reload finds the admitted identity"), Counter->GetAssault(Admitted.AssaultID, Reloaded));
	TestEqual(TEXT("Reload preserves durable identity"), Reloaded.AssaultID, Admitted.AssaultID);
	TestEqual(TEXT("Reload preserves deterministic seed"), Reloaded.DecisionSeed, Admitted.DecisionSeed);
	Counter->RestorePersistentState({});
	FindTask()->TickTask_Implementation();
	TestTrue(TEXT("Completed Native receipt cannot recreate a trimmed record"), Counter->GetAllAssaults().IsEmpty());

	FTerritoryAssaultRecord Cancelled = Admitted;
	Cancelled.State = ETerritoryAssaultState::Cancelled;
	Cancelled.Resolution = ETerritoryAssaultResolution::ManuallyCancelled;
	Counter->RestorePersistentState({Cancelled});
	auto* Second = NewObject<UTerritoryAssaultAdmissionTask>(Tales);
	Second->Request = MakeRequest(Second); Second->OwningComp = Tales;
	Second->BeginTask();
	TestTrue(TEXT("Repeated requester acknowledges the prior attempt"), Second->IsComplete());
	TestEqual(TEXT("Cancelled attempt is not automatically rerolled"), Counter->GetAllAssaults().Num(), 1);
	Second->EndTask();
	Counter->RestorePersistentState({});
	World->NextURL = TEXT("127.0.0.1");
	auto* ClientTask = NewObject<UTerritoryAssaultAdmissionTask>(Tales);
	ClientTask->Request = MakeRequest(ClientTask); ClientTask->OwningComp = Tales;
	ClientTask->BeginTask();
	TestFalse(TEXT("Client cannot grant admission progress"), ClientTask->IsComplete());
	TestTrue(TEXT("Client cannot schedule a force"), Counter->GetAllAssaults().IsEmpty());
	ClientTask->EndTask();
	World->NextURL.Empty();
	return true;
}
#endif
