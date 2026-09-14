#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryPartyReplyProbe.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyDepartureAuthority,
	"TerritoryFramework.Tales.PartyDeparture.PersonalExitAndReplacementCannotEndRemainingDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyDepartureAuthority::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Actor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
	TArray<UTerritoryPartyReplyMemberProbe*> Members;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* PC = NewObject<ATerritoryPartyRemoteControllerProbe>(World->PersistentLevel);
		PC->SetRole(ROLE_Authority);
		auto* PS = NewObject<ANarrativePlayerState>(World->PersistentLevel);
		PS->SetOwner(PC);
		PC->PlayerState = PS;
		auto* Member = NewObject<UTerritoryPartyReplyMemberProbe>(PC);
		PC->AddInstanceComponent(Member);
		Member->RegisterComponent();
		InstallTerritoryPartyMemberProbe(PC, Member);
		Members.Add(Member);
		TestTrue(TEXT("Native adds the real member"), Party->AddPartyMember(Member));
	}
	TestTrue(TEXT("Native starts the shared dialogue"), Party->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
	UDialogue* Shared = Party->GetCurrentDialogue();
	if (!Shared) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	UDialogueNode* FirstLine = Shared->GetCurrentNode();
	TestFalse(TEXT("Null removal fails"), Party->RemovePartyMember(nullptr));
	Actor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client removal fails"), Party->RemovePartyMember(Members[1]));
	TestTrue(TEXT("Rejected removal preserves the alias"), Members[1]->GetCurrentDialogue() == Shared);
	Actor->SetRole(ROLE_Authority);
	auto* Outsider = NewObject<UTerritoryPartyReplyMemberProbe>(Members[2]->GetOwner());
	Outsider->CurrentDialogue = Shared;
	TestFalse(TEXT("An outsider cannot use removal to mutate a Tales component"), Party->RemovePartyMember(Outsider));
	TestTrue(TEXT("Rejected outsider alias is untouched"), Outsider->GetCurrentDialogue() == Shared);
	Outsider->CurrentDialogue = nullptr;

	TestTrue(TEXT("A nonleader leaves through Native"), Party->RemovePartyMember(Members[1]));
	TestNull(TEXT("Departing authority loses only its personal shared alias"), Members[1]->GetCurrentDialogue());
	TestFalse(TEXT("Departed player cannot skip the old party line"), Members[1]->TrySkipCurrentDialogueLine());
	TestFalse(TEXT("Departed player cannot exit the old party"), Members[1]->TryExitDialogue(EExitDialogueReason::EDR_PlayerExited));
	Members[1]->ServerTryExitDialogue_Implementation(EExitDialogueReason::EDR_PlayerExited);
	TestTrue(TEXT("Queued Native exit request cannot close the remaining party"), Party->GetCurrentDialogue() == Shared && Shared->IsInitialized());
	TestTrue(TEXT("Remaining member retains the exact same dialogue"), Members[2]->GetCurrentDialogue() == Shared);
	TestTrue(TEXT("Remaining party has not skipped or restarted its line"), Shared->GetCurrentNode() == FirstLine);
	TestTrue(TEXT("Departed player can begin a personal dialogue"), Members[1]->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
	UDialogue* Personal = Members[1]->GetCurrentDialogue();
	TestTrue(TEXT("Personal replacement did not deinitialize the party object"), Personal != Shared && Shared->IsInitialized());
	TestFalse(TEXT("Repeated removal fails"), Party->RemovePartyMember(Members[1]));
	TestTrue(TEXT("Repeated removal preserves the new personal dialogue"), Members[1]->GetCurrentDialogue() == Personal);
	Members[1]->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestTrue(TEXT("Personal completion cannot end the remaining party"), Party->GetCurrentDialogue() == Shared && Shared->IsInitialized());

	// Exercise a synchronous story callback, not just the state after Remove returns.
	Members[0]->bStartPersonalOnLeave = true;
	Members[0]->OnLeaveParty.AddDynamic(Members[0], &UTerritoryPartyReplyMemberProbe::ObserveLeave);
	TestTrue(TEXT("The original leader can detach its personal alias"), Party->RemovePartyMember(Members[0]));
	TestFalse(TEXT("Leave callbacks never see the unsafe shared alias"), Members[0]->bLeaveSawSharedAlias);
	TestTrue(TEXT("Leave callback starts a separate personal session"), Members[0]->GetCurrentDialogue() && Members[0]->GetCurrentDialogue() != Shared);
	TestTrue(TEXT("Synchronous personal replacement leaves the original dialogue alive"), Party->GetCurrentDialogue() == Shared && Shared->IsInitialized());
	TestTrue(TEXT("Native membership appoints the remaining leader"), Party->GetPartyLeader() == Members[2]);
	TestTrue(TEXT("The remaining member can still skip through Native"), Members[2]->TrySkipCurrentDialogueLine());
	TestTrue(TEXT("Existing shared playback advances instead of restarting"), Shared->GetCurrentNode() != FirstLine);
	Members[0]->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestNull(TEXT("Final Native group exit still clears the remaining member"), Members[2]->GetCurrentDialogue());
	TestNull(TEXT("Final Native group exit still owns deinitialization"), Party->GetCurrentDialogue());
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyDepartureAlias,
	"TerritoryFramework.Presentation.Cinematics.PartyDepartureDetachesOnlyExactLocalAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyDepartureAlias::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Actor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
	auto* PC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	PC->SetRole(ROLE_Authority);
	PC->PlayerState = NewObject<ANarrativePlayerState>(World->PersistentLevel);
	PC->PlayerState->SetOwner(PC);
	auto* Tales = PC->GetTalesComponent();
	auto* View = NewObject<UTerritoryCinematicPresentationSubsystem>(NewObject<ULocalPlayer>(GEngine));
	View->BindToController(PC);
	Party->AddPartyMember(Tales);
	auto* Shared = NewObject<UDialogue>(Party);
	Shared->OwningComp = Party;
	Shared->OwningController = PC;
	Party->CurrentDialogue = Shared;
	Tales->CurrentDialogue = Shared;
	Party->OnDialogueBegan.Broadcast(Shared);
	View->DetachDepartedPartyAlias(Party);
	TestTrue(TEXT("Stale leave cannot detach a current or rejoined member"), Tales->GetCurrentDialogue() == Shared);
	Party->RemovePartyMember(Tales);
	TestTrue(TEXT("Listen-server detach preserves the shared authority object"), Shared->IsInitialized());
	// A second local member must retain the client's shared copy too.
	auto* OtherPC = NewObject<ATerritoryPartyLocalControllerProbe>(World->PersistentLevel);
	OtherPC->SetRole(ROLE_Authority);
	OtherPC->PlayerState = NewObject<ANarrativePlayerState>(World->PersistentLevel);
	OtherPC->PlayerState->SetOwner(OtherPC);
	World->AddController(OtherPC);
	auto* OtherTales = OtherPC->GetTalesComponent();
	TestTrue(TEXT("Fixture has a remaining local viewer"), OtherPC->IsLocalController());
	Party->AddPartyMember(OtherTales);
	OtherTales->CurrentDialogue = Shared;
	// Reproduce the alias on a client before its membership RepNotify is observed.
	Actor->SetRole(ROLE_SimulatedProxy);
	PC->SetRole(ROLE_AutonomousProxy);
	Tales->CurrentDialogue = Shared;
	View->BindToParty(Party);
	View->HandleLeftParty(Party);
	TestNull(TEXT("Replicated leave drops the exact old personal alias"), Tales->GetCurrentDialogue());
	TestTrue(TEXT("Remaining local viewer keeps the exact shared dialogue"), Shared->IsInitialized() && Party->GetCurrentDialogue() == Shared);
	TestFalse(TEXT("Local party presentation is released"), View->IsNarrativeCinematicActive());
	Actor->SetRole(ROLE_Authority);
	Party->RemovePartyMember(OtherTales);
	Actor->SetRole(ROLE_SimulatedProxy);
	Tales->CurrentDialogue = Shared;
	View->DetachDepartedPartyAlias(Party);
	TestNull(TEXT("Last local viewer closes only the old client party copy"), Party->GetCurrentDialogue());
	TestFalse(TEXT("Native deinitializes the abandoned client dialogue immediately"), Shared->IsInitialized());
	TestNull(TEXT("Last local viewer still has no personal shared alias"), Tales->GetCurrentDialogue());
	auto* Personal = NewObject<UDialogue>(Tales);
	Personal->OwningComp = Tales;
	Tales->CurrentDialogue = Personal;
	View->DetachDepartedPartyAlias(Party);
	TestTrue(TEXT("Old cleanup cannot erase a new personal dialogue"), Tales->GetCurrentDialogue() == Personal);
	auto* NextActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* NextParty = CastChecked<UTerritoryNarrativePartyComponent>(NextActor->PartyTalesComponent);
	auto* Next = NewObject<UDialogue>(NextParty);
	Next->OwningComp = NextParty;
	Tales->CurrentDialogue = Next;
	View->DetachDepartedPartyAlias(Party);
	TestTrue(TEXT("Old cleanup cannot erase another party's dialogue"), Tales->GetCurrentDialogue() == Next);
	Tales->CurrentDialogue = nullptr;
	View->Deinitialize();
	Actor->SetRole(ROLE_Authority);
	PC->SetRole(ROLE_Authority);
	Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	World->RemoveController(OtherPC);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
