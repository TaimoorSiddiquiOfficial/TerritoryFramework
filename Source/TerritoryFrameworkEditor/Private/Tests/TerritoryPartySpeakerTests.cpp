#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryPartyReplyProbe.h"
#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "AbilitySystemComponent.h"
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
	FPartySpeakerFixture()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Actor = World->SpawnActor<ATerritoryNarrativeParty>();
		Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			auto* PC = NewObject<ATerritoryPartyRemoteControllerProbe>(World->PersistentLevel);
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
	TestEqual(TEXT("Avatar contribution is deliberately left for Native end"), F.Count(0), 3);
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
	F.Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
	TestEqual(TEXT("Level teardown lets Native clean the original avatar"), F.Count(0), 0);
	TestEqual(TEXT("Level teardown lets Native clean the remaining member"), F.Count(2), 0);
	Restored->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestEqual(TEXT("New party end does not touch the old counts"), F.Count(1), 0);
	return true;
}
#endif
