#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "AI/TerritoryConversationGoal.h"
#include "AI/TerritoryPatrolGoal.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Social/TerritoryConversationMeetingPoint.h"
#include "Social/TerritoryConversationListenerComponent.h"
#include "Social/TerritoryConversationProfile.h"
#include "Tales/TerritoryAmbientDialogue.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativePartyComponent.h"
#include "Tales/DialogueSM.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"
#include "Tests/TerritoryConversationTestActivities.h"
#include "Tests/TerritoryConversationTestGameState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryConversationGraph,
	"TerritoryFramework.Guards.Conversation.ExactRolesAndBoundedGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryConversationGraph::RunTest(const FString& Parameters)
{
	auto* Dialogue = NewObject<UTerritoryAmbientDialogue>();
	auto* First = NewObject<UDialogueNode_NPC>(Dialogue);
	auto* Second = NewObject<UDialogueNode_NPC>(Dialogue);
	First->SetID(TEXT("First")); Second->SetID(TEXT("Second"));
	First->SetSpeakerID(Dialogue->FirstRoleID); Second->SetSpeakerID(Dialogue->SecondRoleID);
	First->Line.Text = FText::FromString(TEXT("First test line."));
	Second->Line.Text = FText::FromString(TEXT("Second test line."));
	First->NPCReplies.Add(Second);
	Dialogue->RootDialogue = First; Dialogue->NPCReplies = {First, Second};
	FString Failure;
	TestTrue(TEXT("Two role graph is accepted"), Dialogue->ValidateGraph(Dialogue, Failure));
	Second->NPCReplies.Add(First);
	TestFalse(TEXT("Cycle is rejected before Native chunk generation"), Dialogue->ValidateGraph(Dialogue, Failure));
	Second->NPCReplies.Reset();
	Dialogue->PlayerReplies.Add(NewObject<UDialogueNode_Player>(Dialogue));
	TestFalse(TEXT("Player choices are rejected"), Dialogue->ValidateGraph(Dialogue, Failure));
	Dialogue->PlayerReplies.Reset();
	Second->SetSpeakerID(Dialogue->FirstRoleID);
	TestFalse(TEXT("A one-speaker script cannot claim two roles"), Dialogue->ValidateGraph(Dialogue, Failure));
	Second->SetSpeakerID(Dialogue->SecondRoleID);
	Second->Line.Duration = ELineDuration::LD_Never;
	TestFalse(TEXT("Never-ending lines are rejected"), Dialogue->ValidateGraph(Dialogue, Failure));
	Second->Line.Duration = ELineDuration::LD_Default;
	Dialogue->Speakers[0].SpeakerID = NAME_None;
	FPropertyChangedEvent Changed(nullptr);
	Dialogue->PostEditChangeProperty(Changed);
	TestEqual(TEXT("Native editor normalization cannot erase First role"), Dialogue->Speakers[0].GetSpeakerID(), Dialogue->FirstRoleID);
	TestEqual(TEXT("Native editor normalization preserves a distinct Second role"), Dialogue->Speakers[1].GetSpeakerID(), Dialogue->SecondRoleID);
	TestTrue(TEXT("Ambient movement remains free"), Dialogue->bFreeMovement);
	TestFalse(TEXT("Ambient never adjusts the player transform"), Dialogue->bAdjustPlayerTransform);
	TestFalse(TEXT("Ambient never rotates the listener"), Dialogue->bAutoRotateSpeakers);
	TestEqual(TEXT("Ambient owns no delayed input unlock"), Dialogue->DialogueBlendOutTime, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryConversationCoordination,
	"TerritoryFramework.Guards.Conversation.OwnedGoalCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryConversationCoordination::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL()); World->SetGameState(World->SpawnActor<ATerritoryConversationTestGameState>());
	World->InitializeActorsForPlay(FURL()); World->CreateAISystem();
	auto* Meeting = World->SpawnActor<ATerritoryConversationMeetingPoint>();
	Meeting->Profile = NewObject<UTerritoryConversationProfile>();
	Meeting->Profile->Cooldown = 0.f;
	const auto MakeGuard = [&]()
	{
		auto* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
		if (!TestNotNull(TEXT("Conversation guard"), Guard)) return static_cast<ATerritoryGuardCharacter*>(nullptr);
		Guard->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(Guard, Guard);
		// Narrative caches the auto-spawned controller. Repossessing with a second
		// controller leaves GetActivityComponent() pointing at the now-unpossessed one.
		auto* NPCController = Guard->GetNPCController();
		if (!TestNotNull(TEXT("Guard's Native controller"), NPCController)
			|| !TestTrue(TEXT("Cached controller is the possessing controller"), Guard->GetController() == NPCController)
			|| !TestTrue(TEXT("Native controller owns this guard"), NPCController->GetControlledNPC() == Guard))
			return static_cast<ATerritoryGuardCharacter*>(nullptr);
		auto* Activities = Guard->GetActivityComponent();
		if (!TestNotNull(TEXT("Guard's Native activity component"), Activities)
			|| !TestTrue(TEXT("Activities belong to the possessing controller"), Activities->GetOwner() == NPCController))
			return static_cast<ATerritoryGuardCharacter*>(nullptr);
		if (!Activities->HasBegunPlay()) Activities->BeginPlay();
		Activities->Deactivate();
		return Guard;
	};
	auto* First = MakeGuard(); auto* Second = MakeGuard();
	if (!First || !Second) return false;
	auto* FirstActivities = First->GetActivityComponent();
	auto* OtherGoal = NewObject<UTerritoryPatrolGoal>(FirstActivities);
	OtherGoal->GoalKey = First;
	OtherGoal->TerritoryPatrol.AddDefaulted();
	FirstActivities->AddGoal(OtherGoal, false);
	UTerritoryConversationGoal* LastGoal = nullptr;
	const auto Arm = [&]()
	{
		Meeting->SessionID = FGuid::NewGuid(); Meeting->Phase = ETerritoryConversationPhase::Staging;
		Meeting->FirstGuard = First; Meeting->SecondGuard = Second;
		const auto Add = [&](ATerritoryGuardCharacter* Guard)
		{
			auto* Activities = Guard->GetActivityComponent();
			auto* Goal = NewObject<UTerritoryConversationGoal>(Activities);
			Goal->Meeting = Meeting; Goal->SessionID = Meeting->SessionID; Goal->GoalKey = Meeting;
			if (Guard == First) Meeting->FirstGoal = Goal; else Meeting->SecondGoal = Goal;
			Activities->AddGoal(Goal, false); return Goal;
		};
		LastGoal = Add(First); Add(Second);
		Meeting->BindGuard(First, true); Meeting->BindGuard(Second, true);
	};
	Arm();
	TestTrue(TEXT("Admitted goal belongs to the guard's possessing controller"), LastGoal->OwnerController == First->GetController());
	TestFalse(TEXT("Meeting goals are excluded from Native save records"), LastGoal->bSaveGoal);
	TestFalse(TEXT("An admitted exact goal is valid"), LastGoal->ShouldCleanup());
	Meeting->SetRole(ROLE_SimulatedProxy);
	Meeting->CancelConversation(TEXT("Client attempt"));
	TestTrue(TEXT("A client cannot release server goals"), FirstActivities->GetGoals(UTerritoryConversationGoal::StaticClass()).Goals.Contains(LastGoal));
	Meeting->SetRole(ROLE_Authority);
	FGameplayEffectSpec Spec;
	First->GetNarrativeAbilitySystemComponent()->DamagedBy(Second->GetNarrativeAbilitySystemComponent(), 1.f, Spec);
	TestEqual(TEXT("Real Native damage delegate cancels the meeting"), Meeting->Phase, ETerritoryConversationPhase::Idle);
	TestTrue(TEXT("Cancelled goal no longer scores"), LastGoal->GetGoalScore() < 0.f);
	TestFalse(TEXT("First participant's owned goal is removed"), FirstActivities->HasGoal(UTerritoryConversationGoal::StaticClass()));
	TestFalse(TEXT("Second participant's owned goal is removed"), Second->GetActivityComponent()->HasGoal(UTerritoryConversationGoal::StaticClass()));
	TestTrue(TEXT("Unrelated patrol goal survives"), FirstActivities->GetGoals(UTerritoryPatrolGoal::StaticClass()).Goals.Contains(OtherGoal));
	Arm();
	auto* Saves = World->GetSubsystem<UNarrativeSaveSubsystem>();
	Saves->OnBeginLoad.AddUniqueDynamic(Meeting, &ATerritoryConversationMeetingPoint::OnBeginLoad);
	Saves->OnBeginLoad.Broadcast();
	TestEqual(TEXT("Native in-place load boundary cancels unfinished session"), Meeting->Phase, ETerritoryConversationPhase::Idle);
	TestFalse(TEXT("Load leaves no transient social goal"), FirstActivities->HasGoal(UTerritoryConversationGoal::StaticClass()));
	Arm();
	Meeting->EndPlay(EEndPlayReason::RemovedFromWorld);
	TestEqual(TEXT("World Partition removal cancels the session"), Meeting->Phase, ETerritoryConversationPhase::Idle);
	TestTrue(TEXT("Stream removal preserves other goal authority"), FirstActivities->GetGoals(UTerritoryPatrolGoal::StaticClass()).Goals.Contains(OtherGoal));
	Arm();
	FirstActivities->AddActivity(UTerritoryConversationTestHoldActivity::StaticClass(), false);
	auto* Resume = CastChecked<UTerritoryConversationTestResumeActivity>(FirstActivities->AddActivity(UTerritoryConversationTestResumeActivity::StaticClass(), false));
	FirstActivities->Activate();
	TestTrue(TEXT("Native selects the social goal before cleanup"), FirstActivities->GetCurrentActivityGoal() == LastGoal);
	int32 ResumeCallbacks = 0;
	Resume->ResumeCallback = [&]()
	{
		++ResumeCallbacks;
		FString Failure;
		TestFalse(TEXT("Cleanup callback cannot start another session even at zero cooldown"), Meeting->TryStartConversation(nullptr, Failure));
		TestTrue(TEXT("Admission stops at the active cleanup gate"), Failure.Contains(TEXT("one listener")));
	};
	Meeting->CancelConversation(TEXT("Test reentry"));
	TestEqual(TEXT("Cleanup genuinely resumed another Native activity"), ResumeCallbacks, 1);
	Resume->ResumeCallback = nullptr;
	FirstActivities->Deactivate();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryConversationTransport,
	"TerritoryFramework.Guards.Conversation.ExactListenerContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryConversationTransport::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* PC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	PC->SetRole(ROLE_Authority);
	// Narrative 2.5 resolves the listener's avatar as an ANarrativeCharacter
	// (UTalesComponent::GetOwningPawn returns the character, not the APawn it used to), and the
	// adapter refuses a listener without an avatar because the ambient cleanup has to hide and
	// unhide that pawn. The fixture therefore possesses a real character, not a bare APawn.
	auto* Pawn = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
	PC->SetPawn(Pawn);
	auto* Adapter = NewObject<UTerritoryConversationListenerComponent>(PC);
	PC->AddInstanceComponent(Adapter); Adapter->RegisterComponent();
	auto* Tales = PC->GetTalesComponent();
	TestTrue(TEXT("Exact personal Tales/controller ownership is accepted"), Adapter->IsSupportedListener(Tales, Adapter));
	TestFalse(TEXT("Another Tales component on the same controller is rejected"), Adapter->IsSupportedListener(NewObject<UTalesComponent>(PC), Adapter));
	const FObjectPropertyBase* PartyProperty = FindFProperty<FObjectPropertyBase>(UTalesComponent::StaticClass(), TEXT("PartyComponent"));
	if (!TestNotNull(TEXT("Native party reference exists"), PartyProperty)) return false;
	PartyProperty->SetObjectPropertyValue_InContainer(Tales, NewObject<UNarrativePartyComponent>(PC));
	TestFalse(TEXT("Party listeners are explicitly rejected"), Adapter->IsSupportedListener(Tales, Adapter));
	PartyProperty->SetObjectPropertyValue_InContainer(Tales, nullptr);
	// Aliveness is an ability-system state: a bare spawned guard has no initialised ASC, so
	// ResolvePlayParams would reject the pair as dead and every later assertion would fail for
	// the wrong reason (the sibling coordination test uses the same recipe).
	auto* First = World->SpawnActor<ATerritoryGuardCharacter>();
	auto* Second = World->SpawnActor<ATerritoryGuardCharacter>();
	First->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(First, First);
	Second->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(Second, Second);
	Adapter->SessionID = FGuid::NewGuid();
	Adapter->PreparedClass = UTerritoryAmbientDialogue::StaticClass();
	Adapter->FirstGuard = First; Adapter->SecondGuard = Second;
	Adapter->ExpiresAt = 10.; Adapter->bAcknowledged = true;
	FDialoguePlayParams Params;
	TestFalse(TEXT("An acknowledged server context cannot authorize arbitrary direct Tales begins"), Adapter->ResolvePlayParams(Tales, Adapter->PreparedClass, Params));
	Adapter->bStartingNativeDialogue = true;
	TestTrue(TEXT("Authorized begin recovers exact actor order"), Adapter->ResolvePlayParams(Tales, Adapter->PreparedClass, Params));
	TestTrue(TEXT("Role A maps to supplied actor"), Params.Speakers.Num() == 2 && Params.Speakers[0] == First);
	TestTrue(TEXT("Role B maps to distinct supplied actor"), Params.Speakers.Num() == 2 && Params.Speakers[1] == Second);
	PC->SetRole(ROLE_SimulatedProxy); Adapter->bStartingNativeDialogue = false;
	FDialoguePlayParams ClientDefaultParams;
	TestTrue(TEXT("Listening client recovers roles even when Native supplies default play params"), Adapter->ResolvePlayParams(Tales, Adapter->PreparedClass, ClientDefaultParams));
	TestTrue(TEXT("Recovered client params preserve both exact actors"), ClientDefaultParams.Speakers.Num() == 2 && ClientDefaultParams.Speakers[0] == First && ClientDefaultParams.Speakers[1] == Second);
	PC->SetRole(ROLE_Authority); Adapter->bStartingNativeDialogue = true;
	Adapter->SecondGuard = First;
	TestFalse(TEXT("One actor cannot occupy both speaker roles"), Adapter->ResolvePlayParams(Tales, Adapter->PreparedClass, Params));
	Adapter->SecondGuard = Second;
	Adapter->ExpiresAt = -1.;
	TestFalse(TEXT("Expired contexts cannot be replayed"), Adapter->ResolvePlayParams(Tales, Adapter->PreparedClass, Params));
	Adapter->ExpiresAt = 10.;
	const FGuid Session = Adapter->SessionID;
	Adapter->ClientRelease_Implementation(FGuid::NewGuid());
	TestEqual(TEXT("Stale release cannot clear current role context"), Adapter->SessionID, Session);
	Adapter->ClientRelease_Implementation(Session);
	TestFalse(TEXT("Matching release clears exact context"), Adapter->HasPreparedConversation());
	TestFalse(TEXT("Stale client initialization cannot recover old actors"), Adapter->ResolvePlayParams(Tales, UTerritoryAmbientDialogue::StaticClass(), Params));
	// Native end delegates can start a cinematic before its stock cleanup unhides
	// the listener. Ambient cleanup must not overwrite either visibility direction.
	auto* Ambient = NewObject<UTerritoryAmbientDialogue>(Tales);
	Ambient->OwningComp = Tales; Ambient->OwningController = PC; Ambient->OwningPawn = Pawn;
	bool bNewSceneHidesPawn = true;
	const FDelegateHandle EndHandle = FDialogueDelegates::OnDialogueEnd.AddLambda([&](UTalesComponent* Context, UDialogue* Ending)
	{
		if (Ending == Ambient)
		{
			TestTrue(TEXT("Native end delegate retains exact Tales listener context"), Context == Tales && Ambient->GetListeningPawn() == Pawn);
			Pawn->SetActorHiddenInGame(bNewSceneHidesPawn);
		}
	});
	Ambient->OnEndDialogue();
	TestTrue(TEXT("Cleanup preserves a new end-callback scene hiding the listener"), Pawn->IsHidden());
	bNewSceneHidesPawn = false;
	Ambient->OnEndDialogue();
	TestFalse(TEXT("Cleanup preserves an end-callback scene unhiding the listener"), Pawn->IsHidden());
	FDialogueDelegates::OnDialogueEnd.Remove(EndHandle);
	Adapter->SessionID = FGuid::NewGuid(); Adapter->PreparedClass = UTerritoryAmbientDialogue::StaticClass();
	Adapter->FirstGuard = First; Adapter->SecondGuard = Second; Adapter->bAcknowledged = true;
	Adapter->bStartingNativeDialogue = true; Adapter->ExpiresAt = 10.;
	TestTrue(TEXT("Initialized ambient instance is captured before Native start callbacks"), Adapter->BindInitializedDialogue(Tales, Ambient));
	auto* Replacement = NewObject<UTerritoryAmbientDialogue>(Tales);
	Replacement->OwningComp = Tales;
	TestFalse(TEXT("A callback replacement cannot be adopted as the original meeting dialogue"), Adapter->BindInitializedDialogue(Tales, Replacement));
	TestTrue(TEXT("Exact initial instance remains owned"), Adapter->GetPreparedDialogue(Adapter->SessionID) == Ambient);
	Adapter->ResetContext();
	return true;
}
#endif
