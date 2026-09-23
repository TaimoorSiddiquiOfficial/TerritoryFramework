#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryPartyReplyProbe.h"
#include "TerritoryAuditEventProbe.h"
#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
struct FPartySpeakerFixture
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ATerritoryNarrativeParty* Actor;
	UTerritoryNarrativePartyComponent* Party;
	TArray<UTerritoryPartyReplyMemberProbe*> Members;
	TArray<ANarrativePlayerState*> States;
	TArray<UAbilitySystemComponent*> ASCs;
	FGameplayTag Tag = GetTerritoryPartySpeakerTestTag();
	FPartySpeakerFixture(bool bLocalLeader = false)
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Actor = World->SpawnActor<ATerritoryNarrativeParty>();
		Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto* PC = NewObject<ANarrativePlayerController>(World->PersistentLevel,
				bLocalLeader && Index == 0 ? ATerritoryPartyLocalControllerProbe::StaticClass() : ATerritoryPartyRemoteControllerProbe::StaticClass());
			PC->SetRole(ROLE_Authority);
			auto* State = NewObject<ANarrativePlayerState>(World->PersistentLevel);
			State->SetOwner(PC);
			PC->PlayerState = State;
			auto* Member = NewObject<UTerritoryPartyReplyMemberProbe>(PC);
			PC->AddInstanceComponent(Member);
			Member->RegisterComponent();
			InstallTerritoryPartyMemberProbe(PC, Member);
			auto* Avatar = NewObject<ATerritoryPartySpeakerAvatarProbe>(World->PersistentLevel);
			Avatar->SetRole(ROLE_Authority);
			Avatar->PlayerASC = State->GetAbilitySystemComponent();
			PC->SetPawn(Avatar);
			Members.Add(Member);
			States.Add(State);
			ASCs.Add(State->GetAbilitySystemComponent());
			ASCs.Last()->RegisterComponent();
			ASCs.Last()->InitAbilityActorInfo(State, Avatar);
			Party->AddPartyMember(Member);
		}
	}
	bool Begin() { return Party->BeginDialogue(UTerritoryPartySpeakerTestDialogue::StaticClass()); }
	void Logout(int32 Index)
	{
		auto* GameMode = NewObject<AGameModeBase>(World->PersistentLevel);
		GameMode->Logout(Members[Index]->GetOwningController());
	}
	int32 Count(int32 Index) const { return ASCs[Index]->GetTagCount(Tag); }
	~FPartySpeakerFixture()
	{
		Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartySpeakerDeparture,
	"TerritoryFramework.Tales.PartySpeakerTags.NativeGrantDepartureAndExternalOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyContextMigration,
	"TerritoryFramework.Tales.PartyDeparture.ModularContextMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyContextMigration::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	{
		FPartySpeakerFixture F;
		TestTrue(TEXT("Start a Native conversation through the optional adapter"), F.Begin());
		auto* Dialogue = CastChecked<UTerritoryPartyReplyTestDialogue>(F.Party->GetCurrentDialogue());
		APawn* PreviousPawn = F.Members[0]->GetOwningPawn();
		APawn* NextPawn = F.Members[1]->GetOwningPawn();
		Dialogue->AddDistanceProbe(F.Members[2]->GetOwningPawn());
		Dialogue->EndDialogueDist = 500.f;
		UDialogueNode* Line = Dialogue->GetCurrentNode();
		bool bCorrectContext = false;
		auto* Event = NewObject<UTerritoryAuditNarrativeEvent>(Line);
		Event->EventRuntime = EEventRuntime::Start;
		Event->ContextCallback = [&](APawn* Pawn, APlayerController* PC, UTalesComponent* Tales)
		{ bCorrectContext = Pawn == NextPawn && PC == F.Members[1]->GetOwningController() && Tales == F.Party; };
		Line->Events.Add(Event);
		TestTrue(TEXT("Remote owner departure transfers context"), F.Party->RemovePartyMember(F.Members[0]));
		TestTrue(TEXT("No restart or synthetic skip"), F.Party->GetCurrentDialogue() == Dialogue && Dialogue->GetCurrentNode() == Line);
		TestTrue(TEXT("Player speaker map uses the remaining member"), Dialogue->GetPlayerAvatar() == NextPawn);
		PreviousPawn->SetActorLocation(FVector(10000.f, 0.f, 0.f));
		TestTrue(TEXT("Regression fixture really moves old pawn beyond end distance"), PreviousPawn->GetDistanceTo(NextPawn) > Dialogue->EndDialogueDist);
		Dialogue->TickDialogue_Implementation(1.f);
		TestTrue(TEXT("Old pawn movement cannot end the remaining party dialogue"), F.Party->GetCurrentDialogue() == Dialogue);
		Dialogue->RunNodeEventsForProbe(Line);
		TestTrue(TEXT("Native node event receives the replacement member context"), bCorrectContext);
		TestEqual(TEXT("Departed player retains no avatar or member grant"), F.Count(0), 0);
		F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
		for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Final exit balances all Native grants"), F.Count(Index), 0);
	}
	for (bool bLocalOwner : {false, true})
	{
		FPartySpeakerFixture F(bLocalOwner);
		if (!bLocalOwner) F.Party->OwnerDeparturePolicy = ETerritoryPartyOwnerDeparturePolicy::EndConversation;
		TestTrue(TEXT("Start safe-end policy fixture"), F.Begin());
		UDialogue* Dialogue = F.Party->GetCurrentDialogue();
		TestTrue(TEXT("Owner can leave under either policy"), F.Party->RemovePartyMember(F.Members[0]));
		TestNull(TEXT("Explicit End or unsupported local viewport transfer ends through Native"), F.Party->GetCurrentDialogue());
		TestFalse(TEXT("Old dialogue cannot continue with stale context"), Dialogue->IsInitialized());
		for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Safe end balances every Native grant"), F.Count(Index), 0);
	}
	return true;
}

bool FTFTerritoryPartySpeakerDeparture::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	FPartySpeakerFixture F;
	for (auto* ASC : F.ASCs) ASC->AddLooseGameplayTag(F.Tag, 2, EGameplayTagReplicationState::CountToOwner);
	TestTrue(TEXT("Native starts the tagged dialogue"), F.Begin());
	UDialogue* Shared = F.Party->GetCurrentDialogue();
	TestEqual(TEXT("Initial avatar has two Native contributions plus external tags"), F.Count(0), 4);
	TestEqual(TEXT("Other party member has one Native contribution"), F.Count(1), 3);
	F.Actor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot release server speaker tags"), F.Party->RemovePartyMember(F.Members[1]));
	TestEqual(TEXT("Rejected removal leaves every contribution"), F.Count(1), 3);
	F.Actor->SetRole(ROLE_Authority);
	TestFalse(TEXT("Null replacement is rejected"), F.Party->BeginDialogue(nullptr));
	FDialoguePlayParams LowerPriority;
	LowerPriority.Priority = Shared->Priority + 10;
	TestFalse(TEXT("Native priority rejection retains the grant record"), F.Party->BeginDialogue(UTerritoryPartySpeakerTestDialogue::StaticClass(), LowerPriority));
	bool bObservedCleanLeave = false;
	F.Members[1]->OnLeaveParty.AddDynamic(F.Members[1], &UTerritoryPartyReplyMemberProbe::ObserveLeave);
	F.Members[1]->LeaveAction = [&](UNarrativePartyComponent*)
	{
		bObservedCleanLeave = F.Count(1) == 2;
		TestTrue(TEXT("Personal conversation can start inside Leave callback"), F.Members[1]->BeginDialogue(UTerritoryPartySpeakerTestDialogue::StaticClass()));
	};
	TestTrue(TEXT("Nonleader leaves"), F.Party->RemovePartyMember(F.Members[1]));
	TestTrue(TEXT("Native Leave observes the old party grant already released"), bObservedCleanLeave);
	TestEqual(TEXT("New personal avatar contribution is preserved"), F.Count(1), 3);
	TestFalse(TEXT("Repeated removal cannot subtract another contribution"), F.Party->RemovePartyMember(F.Members[1]));
	TestEqual(TEXT("Repeat leaves external and personal tags unchanged"), F.Count(1), 3);
	TestTrue(TEXT("Shared dialogue is the original initialized object"), F.Party->GetCurrentDialogue() == Shared && Shared->IsInitialized());
	F.Members[1]->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("Native personal end restores only external tags"), F.Count(1), 2);
	F.Party->UnregisterComponent();
	F.Party->RegisterComponent();
	TestTrue(TEXT("Original avatar member can leave after re-registration"), F.Party->RemovePartyMember(F.Members[0]));
	TestEqual(TEXT("Departed avatar grant moves without touching external tags"), F.Count(0), 2);
	TestEqual(TEXT("New context receives the avatar grant as well as its member grant"), F.Count(2), 4);
	F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Native end and Territory departure preserve all external counts"), F.Count(Index), 2);
	F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("Repeated group end is harmless"), F.Count(0), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartySpeakerCallbacks,
	"TerritoryFramework.Tales.PartySpeakerTags.ReentrantExitAndLateJoinAreBounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartySpeakerCallbacks::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	FPartySpeakerFixture F;
	bool bBeginRemovalAccepted = true;
	FDelegateHandle BeginHandle = F.ASCs[1]->RegisterGameplayTagEvent(F.Tag, EGameplayTagEventType::AnyCountChange).AddLambda(
		[&](const FGameplayTag, int32 Count)
		{
			if (Count > 0)
			{
				bBeginRemovalAccepted = F.Party->RemovePartyMember(F.Members[1]);
				F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
			}
		});
	TestTrue(TEXT("Native begin finishes before its requested exit"), F.Begin());
	TestFalse(TEXT("Tag setup cannot mutate its own member iteration"), bBeginRemovalAccepted);
	TestNull(TEXT("Begin callback exit closes that exact session afterward"), F.Party->GetCurrentDialogue());
	for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Deferred begin exit balances all grants"), F.Count(Index), 0);
	F.ASCs[1]->RegisterGameplayTagEvent(F.Tag, EGameplayTagEventType::AnyCountChange).Remove(BeginHandle);
	TestTrue(TEXT("A fresh tagged conversation starts"), F.Begin());
	bool bExitRequested = false;
	FDelegateHandle RemoveHandle = F.ASCs[1]->RegisterGameplayTagEvent(F.Tag, EGameplayTagEventType::AnyCountChange).AddLambda(
		[&](const FGameplayTag, int32 Count)
		{
			if (Count == 0)
			{
				bExitRequested = true;
				F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
			}
		});
	TestTrue(TEXT("Departure commits before a tag callback ends the group"), F.Party->RemovePartyMember(F.Members[1]));
	TestTrue(TEXT("The callback really requested exit"), bExitRequested);
	TestNull(TEXT("Deferred departure exit reaches Native"), F.Party->GetCurrentDialogue());
	for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Departure-triggered exit never double-removes"), F.Count(Index), 0);
	F.ASCs[1]->RegisterGameplayTagEvent(F.Tag, EGameplayTagEventType::AnyCountChange).Remove(RemoveHandle);
	TestTrue(TEXT("Restart with the remaining members"), F.Begin());
	auto* Other = F.World->SpawnActor<ATerritoryNarrativeParty>();
	TestTrue(TEXT("Departed member joins a separate idle party"), Other->PartyTalesComponent->AddPartyMember(F.Members[1]));
	TestFalse(TEXT("Joining a tagged live session is rejected before transfer"), F.Party->AddPartyMember(F.Members[1]));
	TestTrue(TEXT("Rejected late join retains its source membership"), F.Members[1]->GetParty() == Other->PartyTalesComponent);
	TestEqual(TEXT("Rejected late join receives no unbalanced tag"), F.Count(1), 0);
	F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestTrue(TEXT("Join succeeds after the conversation ends"), F.Party->AddPartyMember(F.Members[1]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartySpeakerSave,
	"TerritoryFramework.Tales.PartySpeakerTags.ReplacementSaveAndEndPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartySpeakerSave::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	FPartySpeakerFixture F;
	F.Actor->PreInitializeComponents();
	F.Actor->InitializeComponents();
	F.Actor->PostInitializeComponents();
	F.Actor->DispatchBeginPlay();
	TestTrue(TEXT("Teardown fixture completed actor initialization and BeginPlay"), F.Actor->IsActorInitialized() && F.Party->HasBegunPlay());
	TestTrue(TEXT("Initial tagged session starts"), F.Begin());
	UDialogue* Old = F.Party->GetCurrentDialogue();
	TestTrue(TEXT("Native replacement starts"), F.Begin());
	TestFalse(TEXT("Old session is deinitialized"), Old->IsInitialized());
	TestEqual(TEXT("Replacement keeps one party plus one avatar grant"), F.Count(0), 2);
	TestEqual(TEXT("Replacement does not stack party member grants"), F.Count(1), 1);
	F.Party->MasterTaskList.Add(TEXT("SpeakerSaveRegression"), 5);
	F.Party->PrepareForSave_Implementation();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	SaveArchive.ArIsSaveGame = true;
	F.Party->Serialize(SaveArchive);
	auto* RestoredActor = F.World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Restored = RestoredActor->PartyTalesComponent.Get();
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	Restored->Serialize(LoadArchive);
	Restored->Load_Implementation();
	Restored->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("Freshly loaded party cannot release a live party's tags"), F.Count(1), 1);
	TestEqual(TEXT("Native task history remains intact"), Restored->MasterTaskList.FindRef(TEXT("SpeakerSaveRegression")), 5);
	TestTrue(TEXT("Post-load transfer releases the old party's member contribution"), Restored->AddPartyMember(F.Members[1]));
	TestEqual(TEXT("Transfer leaves no old member tag"), F.Count(1), 0);
	TestTrue(TEXT("Explicitly reconnected saved party can start a fresh conversation"), Restored->BeginDialogue(UTerritoryPartySpeakerTestDialogue::StaticClass()));
	TestEqual(TEXT("Reconnected member receives Native avatar and member grants"), F.Count(1), 2);
	TestTrue(TEXT("Final post-load departure succeeds"), Restored->RemovePartyMember(F.Members[1]));
	TestNull(TEXT("Final post-load departure closes the restored party"), Restored->GetCurrentDialogue());
	TestEqual(TEXT("Final post-load departure releases both Native grants"), F.Count(1), 0);
	TestFalse(TEXT("Empty restored party cannot start a conversation"), Restored->BeginDialogue(UTerritoryPartySpeakerTestDialogue::StaticClass()));
	F.Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
	TestEqual(TEXT("Level teardown lets Native clean the original avatar"), F.Count(0), 0);
	TestEqual(TEXT("Level teardown lets Native clean the remaining member"), F.Count(2), 0);
	Restored->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("New party end does not touch the old counts"), F.Count(1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyFinalMember,
	"TerritoryFramework.Tales.PartyDeparture.FinalMemberEndsNativeBeforeLeave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyFinalMember::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	FPartySpeakerFixture F;
	for (auto* ASC : F.ASCs) ASC->AddLooseGameplayTag(F.Tag, 2, EGameplayTagReplicationState::CountToOwner);
	TestTrue(TEXT("Tagged group starts through Native"), F.Begin());
	UDialogue* Shared = F.Party->GetCurrentDialogue();
	TestTrue(TEXT("First nonleader leaves"), F.Party->RemovePartyMember(F.Members[1]));
	TestTrue(TEXT("Second nonleader leaves"), F.Party->RemovePartyMember(F.Members[2]));
	TestTrue(TEXT("The remaining original player keeps the exact live dialogue"), Shared == F.Party->GetCurrentDialogue() && Shared->IsInitialized());
	F.Actor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot trigger final-member cleanup"), F.Party->RemovePartyMember(F.Members[0]));
	TestEqual(TEXT("Rejected client cleanup preserves all original grants"), F.Count(0), 4);
	F.Actor->SetRole(ROLE_Authority);
	F.Party->UnregisterComponent();
	F.Party->RegisterComponent();
	Shared->bCanBeExited = false;
	int32 NativeEnds = 0;
	bool bEndSawFinalMember = false;
	bool bNestedJoinAccepted = true;
	const FDelegateHandle EndHandle = FDialogueDelegates::OnDialogueEnd.AddLambda(
		[&](UTalesComponent* Component, UDialogue* Dialogue)
		{
			if (Component != F.Party || Dialogue != Shared) return;
			++NativeEnds;
			bEndSawFinalMember = F.Party->GetPartyMembers().Contains(F.Members[0]);
			bNestedJoinAccepted = F.Party->AddPartyMember(F.Members[1]);
			F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
		});
	bool bLeaveSawCompleteCleanup = false;
	F.Members[0]->OnLeaveParty.AddDynamic(F.Members[0], &UTerritoryPartyReplyMemberProbe::ObserveLeave);
	F.Members[0]->LeaveAction = [&](UNarrativePartyComponent*)
	{
		bLeaveSawCompleteCleanup = NativeEnds == 1 && !Shared->IsInitialized()
			&& !F.Party->GetCurrentDialogue() && F.Count(0) == 2;
		TestTrue(TEXT("Leave starts a personal conversation after Native cleanup"), F.Members[0]->BeginDialogue(UTerritoryPartySpeakerTestDialogue::StaticClass()));
	};
	TestTrue(TEXT("Final departure closes even an unskippable conversation"), F.Party->RemovePartyMember(F.Members[0]));
	FDialogueDelegates::OnDialogueEnd.Remove(EndHandle);
	TestEqual(TEXT("Native end is called exactly once"), NativeEnds, 1);
	TestTrue(TEXT("Native end still sees its final member grant"), bEndSawFinalMember);
	TestFalse(TEXT("End callback cannot join while Native is closing"), bNestedJoinAccepted);
	TestTrue(TEXT("Leave sees tags and old dialogue fully cleaned"), bLeaveSawCompleteCleanup);
	TestNull(TEXT("Empty party has no current conversation"), F.Party->GetCurrentDialogue());
	TestFalse(TEXT("Empty party cannot begin another conversation"), F.Begin());
	UDialogue* Personal = F.Members[0]->GetCurrentDialogue();
	TestTrue(TEXT("Personal conversation survives the deferred old exit"), Personal && Personal != Shared && Personal->IsInitialized());
	TestEqual(TEXT("Only personal and external grants remain"), F.Count(0), 3);
	TestFalse(TEXT("Repeated final departure is inert"), F.Party->RemovePartyMember(F.Members[0]));
	TestTrue(TEXT("Repeated final departure preserves the personal conversation"), F.Members[0]->GetCurrentDialogue() == Personal);
	F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("Empty-party exit cannot remove personal grants"), F.Count(0), 3);
	F.Members[0]->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Only external grants remain after all Native ends"), F.Count(Index), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyLogout,
    "TerritoryFramework.Tales.PartyDeparture.GameModeLogoutUsesModularPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyLogout::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    FPartySpeakerFixture F;
    for (auto* ASC : F.ASCs) ASC->AddLooseGameplayTag(F.Tag, 2, EGameplayTagReplicationState::CountToOwner);
    TestTrue(TEXT("Begin logout fixture"), F.Begin());
    UDialogue* Dialogue = F.Party->GetCurrentDialogue();
    UDialogueNode* Line = Dialogue->GetCurrentNode();
    F.Logout(2);
    TestNull(TEXT("Non-owner logout clears membership"), F.Members[2]->GetParty());
    TestNull(TEXT("Non-owner logout detaches shared alias before personal EndPlay"), F.Members[2]->GetCurrentDialogue());
    TestTrue(TEXT("Non-owner logout preserves exact playback"), F.Party->GetCurrentDialogue() == Dialogue && Dialogue->GetCurrentNode() == Line);
    F.Members[0]->OnLeaveParty.AddDynamic(F.Members[0], &UTerritoryPartyReplyMemberProbe::ObserveLeave);
    bool bRejoin = true;
    F.Members[0]->LeaveAction = [&](UNarrativePartyComponent*) { bRejoin = F.Party->AddPartyMember(F.Members[0]); };
    // PlayerController::Destroyed unpossesses before Controller::Destroyed emits Logout.
    Dialogue->SetPartyCurrentSpeaker(F.States[0]);
    F.Members[0]->GetOwningController()->SetPawn(nullptr);
    F.Logout(0);
    TestFalse(TEXT("Logout callback cannot rejoin the same party"), bRejoin);
    TestTrue(TEXT("Remote owner logout transfers cached controller"), Dialogue->OwningController == F.Members[1]->GetOwningController());
    TestTrue(TEXT("Party speaker uses PlayerState even after unpossession"), CastChecked<UTerritoryPartyReplyTestDialogue>(Dialogue)->GetPartySpeakerForProbe() == F.States[1]);
    TestTrue(TEXT("Owner logout preserves same dialogue and line"), F.Party->GetCurrentDialogue() == Dialogue && Dialogue->GetCurrentNode() == Line);
    TestEqual(TEXT("Actor read model retains one member"), F.Actor->PartyMembers.Num(), 1);
    TestEqual(TEXT("Native replicated membership retains one member"), F.Party->GetPartyMemberStates().Num(), 1);
    TestEqual(TEXT("Departed avatar/member grants balanced"), F.Count(0), 2);
    TestEqual(TEXT("Departed non-owner grant balanced"), F.Count(2), 2);
    F.Logout(1);
    TestNull(TEXT("Final logout ends through Native"), F.Party->GetCurrentDialogue());
    TestTrue(TEXT("Final logout empties Native members"), F.Party->GetPartyMembers().IsEmpty());
    TestTrue(TEXT("Final logout empties replicated states"), F.Party->GetPartyMemberStates().IsEmpty());
    for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("External grants survive every logout"), F.Count(Index), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyLogoutIsolation,
    "TerritoryFramework.Tales.PartyDeparture.LogoutWorldAuthorityAndRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyLogoutIsolation::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    FPartySpeakerFixture F;
    FPartySpeakerFixture Other;
    auto* OtherMode = NewObject<AGameModeBase>(Other.World->PersistentLevel);
    OtherMode->Logout(F.Members[0]->GetOwningController());
    TestTrue(TEXT("Another world's logout cannot alter membership"), F.Members[0]->GetParty() == F.Party);
    F.Actor->SetRole(ROLE_SimulatedProxy);
    F.Logout(0);
    TestTrue(TEXT("Client cannot process logout mutations"), F.Members[0]->GetParty() == F.Party);
    F.Actor->SetRole(ROLE_Authority);
    F.Party->UnregisterComponent();
    F.Logout(0);
    TestTrue(TEXT("Unregistered component has no global listener"), F.Members[0]->GetParty() == F.Party);
    F.Party->RegisterComponent();
    int32 Leaves = 0;
    F.Members[0]->OnLeaveParty.AddDynamic(F.Members[0], &UTerritoryPartyReplyMemberProbe::ObserveLeave);
    F.Members[0]->LeaveAction = [&](UNarrativePartyComponent*) { ++Leaves; };
    F.Logout(0);
    F.Logout(0);
    TestNull(TEXT("Re-register restores cleanup"), F.Members[0]->GetParty());
    TestEqual(TEXT("Repeat notification produces one Native Leave"), Leaves, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyNestedLogout,
    "TerritoryFramework.Tales.PartyDeparture.LogoutDuringNativeBegin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyNestedLogout::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    for (bool bDestroyReferences : {false, true})
    {
        FPartySpeakerFixture F;
        F.Members[0]->BeginPlayForProbe();
        bool bCallback = false;
        F.Party->OnDialogueBegan.AddDynamic(F.Members[2], &UTerritoryPartyReplyMemberProbe::ObserveDialogueBegin);
        F.Members[2]->DialogueBeginAction = [&](UDialogue* Dialogue)
        {
            bCallback = true;
            F.Members[0]->CurrentDialogue = Dialogue;
            F.Logout(0);
            TestNull(TEXT("Nested logout immediately removes personal alias used by EndPlay"), F.Members[0]->CurrentDialogue);
            F.Members[0]->EndPlayForProbe();
            TestTrue(TEXT("Personal EndPlay cannot deinitialize the group's current Native stack"), Dialogue->IsInitialized());
            if (bDestroyReferences)
            {
                // Models completed controller/PlayerState teardown before Native's outer Begin returns.
                F.Members[0]->GetOwningController()->PlayerState = nullptr;
                F.Members[0]->MarkAsGarbage();
                F.States[0]->MarkAsGarbage();
            }
        };
        F.Begin();
        TestTrue(TEXT("Logout really occurred inside Native Begin"), bCallback);
        TestNull(TEXT("Nested teardown safely ends after Native returns"), F.Party->GetCurrentDialogue());
        TestFalse(TEXT("Captured member removed even after invalidation"), F.Party->GetPartyMembers().Contains(F.Members[0]));
        TestFalse(TEXT("Captured PlayerState removed even after invalidation"), F.Party->GetPartyMemberStates().Contains(F.States[0]));
        TestEqual(TEXT("Remaining Native members preserved"), F.Party->GetPartyMembers().Num(), 2);
        for (int32 Index : {1, 2})
        {
            TestNull(TEXT("Remaining aliases end cleanly"), F.Members[Index]->GetCurrentDialogue());
            TestEqual(TEXT("Remaining speaker grants balance"), F.Count(Index), 0);
        }
        if (!bDestroyReferences) TestNull(TEXT("Live deferred member leaves through Native"), F.Members[0]->GetParty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyTagCallbackLogout,
    "TerritoryFramework.Tales.PartyDeparture.LogoutDuringSpeakerGrant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyTagCallbackLogout::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    FPartySpeakerFixture F;
    bool bLogout = false;
    const FDelegateHandle Handle = F.ASCs[1]->RegisterGameplayTagEvent(F.Tag, EGameplayTagEventType::AnyCountChange).AddLambda(
        [&](const FGameplayTag, int32 Count)
        {
            if (Count > 0 && !bLogout) { bLogout = true; F.Logout(1); }
        });
    F.Begin();
    F.ASCs[1]->RegisterGameplayTagEvent(F.Tag, EGameplayTagEventType::AnyCountChange).Remove(Handle);
    TestTrue(TEXT("Native grant callback triggered logout"), bLogout);
    TestNull(TEXT("Nested logout finishes group safely"), F.Party->GetCurrentDialogue());
    TestNull(TEXT("Nested logout clears member's Native party"), F.Members[1]->GetParty());
    for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("No Native contribution remains"), F.Count(Index), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyUntaggedJoin,
    "TerritoryFramework.Tales.PartyDeparture.ActiveDialogueRejectsUntaggedLateJoin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyUntaggedJoin::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    FPartySpeakerFixture F;
    TestTrue(TEXT("Prepare outsider"), F.Party->RemovePartyMember(F.Members[2]));
    auto* OtherActor = F.World->SpawnActor<ATerritoryNarrativeParty>();
    auto* OtherParty = CastChecked<UTerritoryNarrativePartyComponent>(OtherActor->PartyTalesComponent);
    TestTrue(TEXT("Outsider has an existing Native party"), OtherParty->AddPartyMember(F.Members[2]));
    TestTrue(TEXT("Begin dialogue with no owned speaker tags"), F.Party->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
    TestTrue(TEXT("Fixture has no owned tags"), F.Party->GetCurrentDialogue()->PlayerSpeakerInfo.OwnedTags.IsEmpty());
    TestFalse(TEXT("All active conversations reject late join"), F.Party->AddPartyMember(F.Members[2]));
    TestTrue(TEXT("Rejected join preserves old party"), F.Members[2]->GetParty() == OtherParty && OtherParty->GetPartyMembers().Contains(F.Members[2]));
    F.Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
    TestTrue(TEXT("Normal Native transfer works after conversation ends"), F.Party->AddPartyMember(F.Members[2]));
    TestTrue(TEXT("Old party released member exactly once"), OtherParty->GetPartyMembers().IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyLogoutDuringPlayerLine,
    "TerritoryFramework.Tales.PartyDeparture.LogoutDuringPlayerReplyEndsSafely",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyLogoutDuringPlayerLine::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
    for (int32 DepartingIndex : {0, 1})
    {
        FPartySpeakerFixture F;
        TestTrue(TEXT("Begin reply teardown fixture"), F.Begin());
        TestTrue(TEXT("Skip first NPC line through Native"), F.Members[0]->TrySkipCurrentDialogueLine());
        TestTrue(TEXT("Skip second NPC line through Native"), F.Members[0]->TrySkipCurrentDialogueLine());
        UDialogue* Dialogue = F.Party->GetCurrentDialogue();
        auto* Reply = Dialogue->GetPlayerReplyByID(TEXT("Reply0"));
        F.Party->SelectDialogueOption(Reply, F.States[0]);
        TestTrue(TEXT("Native is actually playing the selected reply"), Dialogue->GetCurrentNode() == Reply);
        F.Logout(DepartingIndex);
        TestNull(TEXT("Departure during player speech closes all copies through Native"), F.Party->GetCurrentDialogue());
        TestFalse(TEXT("No stale speaker context survives"), Dialogue->IsInitialized());
        for (int32 Index = 0; Index < 3; ++Index) TestEqual(TEXT("Speaker grants remain balanced"), F.Count(Index), 0);
    }
    return true;
}
#endif
