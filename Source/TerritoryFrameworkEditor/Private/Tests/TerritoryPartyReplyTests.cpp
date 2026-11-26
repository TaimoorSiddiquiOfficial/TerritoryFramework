#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryPartyReplyProbe.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"
#include "Tales/NarrativeDialogueSettings.h"
#include "Tales/TerritoryReplyTimeoutProfile.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyReplyAuthority,
	"TerritoryFramework.Tales.PartyReplies.ValidateMemberPolicyTimingAndNativeRPC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyReplyAuthority::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	TGuardValue<bool> AutoSetting(GetMutableDefault<UNarrativeDialogueSettings>()->bAutoSelectSingleResponse, false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Actor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
	TArray<UTerritoryPartyReplyMemberProbe*> Members;
	TArray<APlayerState*> States;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* PC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		PC->SetRole(ROLE_Authority);
		auto* PS = NewObject<ANarrativePlayerState>(World->PersistentLevel);
		PS->SetOwner(PC);
		PC->PlayerState = PS;
		auto* Member = NewObject<UTerritoryPartyReplyMemberProbe>(PC);
		PC->AddInstanceComponent(Member);
		Member->RegisterComponent();
		InstallTerritoryPartyMemberProbe(PC, Member);
		Members.Add(Member);
		States.Add(PS);
		if (Index < 2) TestTrue(TEXT("Native membership is established"), Party->AddPartyMember(Member));
	}
	const auto Seed = [&](bool bReady)
	{
		auto* Dialogue = NewObject<UDialogue>(Actor);
		Dialogue->OwningComp = Party;
		Dialogue->OwningController = Members[0]->GetOwningController();
		auto* Option = NewObject<UDialogueNode_Player>(Dialogue);
		Option->SetID(TEXT("Choice"));
		Option->Line.Text = FText::FromString(TEXT("Chosen reply"));
		Option->Line.Duration = ELineDuration::LD_Never;
		Option->OwningDialogue = Dialogue;
		Option->OwningComponent = Party;
		Dialogue->PlayerReplies.Add(Option);
		Dialogue->AvailableResponses.Add(Option);
		Party->CurrentDialogue = Dialogue;
		for (auto* Member : Members) Member->CurrentDialogue = Dialogue;
		Party->OnDialogueBegan.Broadcast(Dialogue);
		if (bReady) Party->OnDialogueRepliesAvailable.Broadcast(Dialogue, Dialogue->AvailableResponses);
		return Dialogue;
	};
	const auto Request = [&](int32 Index) { Members[Index]->ServerSelectDialogueOption_Implementation(TEXT("Choice")); };
	UDialogue* Dialogue = Seed(false);
	Request(0);
	TestNull(TEXT("Early reply while the NPC speaks is rejected"), Dialogue->GetCurrentNode());
	Party->OnDialogueRepliesAvailable.Broadcast(Dialogue, Dialogue->AvailableResponses);
	Request(1);
	Request(2);
	Party->SelectDialogueOption(Dialogue->PlayerReplies[0], nullptr);
	TestNull(TEXT("Nonleader, outsider and missing selector cannot choose a leader-only reply"), Dialogue->GetCurrentNode());
	Party->ServerSelectDialogueOption_Implementation(TEXT("Choice"));
	TestNull(TEXT("A directly owned party RPC cannot impersonate the leader"), Dialogue->GetCurrentNode());
	Request(0);
	TestTrue(TEXT("Leader's personal Native RPC selects the real player node"), Dialogue->GetCurrentNode() == Dialogue->PlayerReplies[0]);
	TestEqual(TEXT("Native sends one accepted reply to each member"), Members[1]->ReplyDispatches, 1);
	Request(0);
	TestEqual(TEXT("Repeated RPC cannot replay the consumed reply"), Members[1]->ReplyDispatches, 1);

	Party->PartyDialogueControlPolicy = EPartyDialogueControlPolicy::AllPlayers;
	Dialogue = Seed(true);
	Request(1);
	TestTrue(TEXT("All Players allows a current nonleader"), Dialogue->GetCurrentNode() != nullptr);
	TestTrue(TEXT("Native message preserves the actual selector"), Members[0]->LastSelector == States[1]);
	Dialogue = Seed(true);
	Party->RemovePartyMember(Members[1]);
	Request(1);
	TestNull(TEXT("A departed member's stale Native alias cannot choose a reply"), Dialogue->GetCurrentNode());
	TestFalse(TEXT("Removed member cannot rejoin an ongoing conversation"), Party->AddPartyMember(Members[1]));
	Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestTrue(TEXT("Removed member can join after the conversation ends"), Party->AddPartyMember(Members[1]));
	Dialogue = Seed(true);
	Actor->SetRole(ROLE_SimulatedProxy);
	Request(0);
	TestNull(TEXT("Client party cannot mutate a reply"), Dialogue->GetCurrentNode());
	Actor->SetRole(ROLE_Authority);
	Party->PartyDialogueControlPolicy = static_cast<EPartyDialogueControlPolicy>(255);
	Request(0);
	TestNull(TEXT("Unknown policy fails closed"), Dialogue->GetCurrentNode());
	Party->PartyDialogueControlPolicy = EPartyDialogueControlPolicy::PartyLeaderControlled;
	auto* OtherOption = NewObject<UDialogueNode_Player>(Actor);
	Party->SelectDialogueOption(OtherOption, States[0]);
	TestNull(TEXT("A foreign reply object is rejected"), Dialogue->GetCurrentNode());
	Dialogue->NPCReplyChain.Add(NewObject<UDialogueNode_NPC>(Dialogue));
	Request(0);
	TestNull(TEXT("Pending NPC chain cannot reach Native playback assertion"), Dialogue->GetCurrentNode());
	Dialogue->NPCReplyChain.Empty();
	Party->OnNPCDialogueLineStarted.Broadcast(Dialogue, nullptr, FDialogueLine(), FSpeakerInfo());
	Request(0);
	TestNull(TEXT("A new NPC line invalidates old readiness"), Dialogue->GetCurrentNode());
	Party->OnDialogueRepliesAvailable.Broadcast(Dialogue, Dialogue->AvailableResponses);
	Party->UnregisterComponent();
	Request(0);
	TestNull(TEXT("Unregister clears reply readiness"), Dialogue->GetCurrentNode());
	Party->RegisterComponent();
	Party->OnDialogueRepliesAvailable.Broadcast(Dialogue, Dialogue->AvailableResponses);
	Request(0);
	TestTrue(TEXT("Re-register resumes on the next Native ready event"), Dialogue->GetCurrentNode() != nullptr);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyAutomaticReplies,
	"TerritoryFramework.Tales.PartyReplies.PreserveNativeAutomaticSelectionAndFreshLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyAutomaticReplies::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	TGuardValue<bool> AutoSetting(GetMutableDefault<UNarrativeDialogueSettings>()->bAutoSelectSingleResponse, false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Actor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
	auto* PC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	PC->SetRole(ROLE_Authority);
	auto* PS = NewObject<ANarrativePlayerState>(World->PersistentLevel);
	PS->SetOwner(PC);
	PC->PlayerState = PS;
	auto* Member = NewObject<UTerritoryPartyReplyMemberProbe>(PC);
	PC->AddInstanceComponent(Member);
	Member->RegisterComponent();
	InstallTerritoryPartyMemberProbe(PC, Member);
	Party->AddPartyMember(Member);
	TestTrue(TEXT("Native can construct the held dialogue fixture"), Party->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
	UDialogue* Dialogue = Party->GetCurrentDialogue();
	if (!Dialogue) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	// Native automatically selects a lone response before broadcasting RepliesAvailable.
	Dialogue->NPCReplyChain.Empty();
	Dialogue->AvailableResponses.SetNum(1);
	UDialogueNode_Player* AutomaticOption = Dialogue->AvailableResponses[0];
	Dialogue->NPCFinishedTalking();
	TestTrue(TEXT("Native single-response auto selection still plays the node"), Dialogue->GetCurrentNode() == AutomaticOption);
	TestTrue(TEXT("Automatic selection is attributed to the actual Native leader"), Member->LastSelector == PS);
	Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	TestNull(TEXT("Native group exit retains lifecycle ownership"), Party->GetCurrentDialogue());
	TestNull(TEXT("Native exit clears the member alias"), Member->GetCurrentDialogue());
	for (int32 Mode = 0; Mode < 2; ++Mode)
	{
		TestTrue(TEXT("Native starts the automatic-reply variant"), Party->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
		Dialogue = Party->GetCurrentDialogue();
		Dialogue->NPCReplyChain.Empty();
		AutomaticOption = Dialogue->AvailableResponses[0];
		if (Mode == 0)
		{
			const auto* AutoSelect = FindFProperty<FBoolProperty>(UDialogueNode_Player::StaticClass(), TEXT("bAutoSelect"));
			if (!TestNotNull(TEXT("Native authored automatic flag exists"), AutoSelect)) break;
			AutoSelect->SetPropertyValue_InContainer(AutomaticOption, true);
		}
		else GetMutableDefault<UNarrativeDialogueSettings>()->bAutoSelectSingleResponse = true;
		Dialogue->NPCFinishedTalking();
		TestTrue(TEXT("Native authored/global auto choice remains playable"), Dialogue->GetCurrentNode() == AutomaticOption);
		GetMutableDefault<UNarrativeDialogueSettings>()->bAutoSelectSingleResponse = false;
		Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	}
	TestTrue(TEXT("A fresh Native session starts after cleanup"), Party->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
	Dialogue = Party->GetCurrentDialogue();
	Member->ServerSelectDialogueOption_Implementation(TEXT("Reply0"));
	TestTrue(TEXT("New session does not inherit prior reply readiness"), Dialogue->GetCurrentNode() == Dialogue->RootDialogue);
	Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	const auto* Query = Party->FindFunction(TEXT("CanMemberChooseDialogueReply"));
	TestTrue(TEXT("Blueprint policy query is pure and authority-only"), Query && Query->HasAllFunctionFlags(FUNC_BlueprintPure | FUNC_BlueprintAuthorityOnly));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryRemotePartyContext,
	"TerritoryFramework.Tales.PartyReplies.RemoteOnlyPartyUsesNativeLeaderContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryRemotePartyContext::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Actor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Party = CastChecked<UTerritoryNarrativePartyComponent>(Actor->PartyTalesComponent);
	TArray<UTerritoryPartyReplyMemberProbe*> Members;
	for (int32 Index = 0; Index < 2; ++Index)
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
		TestTrue(TEXT("Remote member joins through Native"), Party->AddPartyMember(Member));
	}
	TestNull(TEXT("Native local-viewer search reproduces the remote-only gap"),
		Party->UNarrativePartyComponent::GetOwningController());
	TestTrue(TEXT("Server context comes from its real Native leader"),
		Party->GetOwningController() == Members[0]->GetOwningController());
	TestTrue(TEXT("Native begins a dialogue using that context"),
		Party->BeginDialogue(UTerritoryPartyReplyTestDialogue::StaticClass()));
	if (UDialogue* Dialogue = Party->GetCurrentDialogue())
	{
		TestTrue(TEXT("Native dialogue receives the remote leader controller"),
			Dialogue->OwningController == Members[0]->GetOwningController());
	}
	else AddError(TEXT("Native dialogue was not created"));
	Party->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	Actor->SetRole(ROLE_SimulatedProxy);
	TestNull(TEXT("A client cannot use the server fallback to choose a remote viewer"), Party->GetOwningController());
	Actor->SetRole(ROLE_Authority);
	Party->RemovePartyMember(Members[0]);
	TestTrue(TEXT("Fresh lookup follows Native's next leader"),
		Party->GetOwningController() == Members[1]->GetOwningController());
	Members[1]->GetOwningController()->SetRole(ROLE_SimulatedProxy);
	TestNull(TEXT("Unauthoritative controller cannot supply server context"), Party->GetOwningController());
	Members[1]->GetOwningController()->SetRole(ROLE_Authority);
	Party->RemovePartyMember(Members[1]);
	TestNull(TEXT("Empty party has deliberately empty context"), Party->GetOwningController());
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

void UTerritoryPartyReplyTimeoutProbe::ServerSelectDialogueOption_Implementation(const FName& OptionID)
{
	RecordedSelection = OptionID;
	++SelectionCount;
}

/**
 * Server reply countdown: the timer only runs when the authored silence reply is really offered, it
 * selects that reply through the same server path players use, and a player who answers first is never
 * overridden. Profile policy (override, zero seconds, disabled, missing asset) is covered too.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryReplyTimeoutAuthority,
	"TerritoryFramework.Tales.PartyReplies.ReplyTimeoutAuthorityAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryReplyTimeoutAuthority::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

	auto* Host = World->SpawnActor<AActor>();
	auto* Party = NewObject<UTerritoryPartyReplyTimeoutProbe>(Host);
	Host->AddInstanceComponent(Party);
	Party->RegisterComponent();

	UTerritoryReplyTimeoutProfile* Profile = NewObject<UTerritoryReplyTimeoutProfile>();
	Profile->DefaultSeconds = 5.f;
	Profile->SilentReplyID = TEXT("SayNothing");
	Party->ReplyTimeoutProfile = Profile;

	const auto MakeDialogue = [&](bool bOfferSilence)
	{
		auto* Dialogue = NewObject<UDialogue>(Host);
		Dialogue->OwningComp = Party;
		auto* Real = NewObject<UDialogueNode_Player>(Dialogue);
		Real->SetID(TEXT("Agree"));
		Real->OwningDialogue = Dialogue;
		Real->OwningComponent = Party;
		Dialogue->AvailableResponses.Add(Real);
		if (bOfferSilence)
		{
			auto* Silent = NewObject<UDialogueNode_Player>(Dialogue);
			Silent->SetID(TEXT("SayNothing"));
			Silent->OwningDialogue = Dialogue;
			Silent->OwningComponent = Party;
			Dialogue->AvailableResponses.Add(Silent);
		}
		Party->CurrentDialogue = Dialogue;
		return Dialogue;
	};

	// 1. The authored silence reply is offered, so one server countdown starts.
	UDialogue* WithSilence = MakeDialogue(true);
	Party->OnDialogueRepliesAvailable.Broadcast(WithSilence, WithSilence->AvailableResponses);
	TestTrue(TEXT("Countdown starts when the silence reply is offered"), Party->IsReplyTimeoutActive());
	TestTrue(TEXT("Countdown uses the profile duration"),
		FMath::IsNearlyEqual(Party->GetReplyTimeoutDuration(), 5.f));
	TestTrue(TEXT("Countdown targets the authored silence reply"),
		Party->GetPendingSilentReplyID() == FName(TEXT("SayNothing")));
	TestTrue(TEXT("Countdown reports remaining time inside its duration"),
		Party->GetReplyTimeRemaining() > 0.f && Party->GetReplyTimeRemaining() <= 5.f);

	// 2. Expiry reports and selects the silence reply exactly once.
	TestTrue(TEXT("Resolving the countdown reports the silence selection"),
		Party->ResolveReplyCountdownNow());
	TestFalse(TEXT("Countdown stops after it expires"), Party->IsReplyTimeoutActive());
	TestEqual(TEXT("Expiry selects the silence reply once"), Party->SelectionCount, 1);
	TestTrue(TEXT("Expiry selects the authored silence reply"),
		Party->RecordedSelection == FName(TEXT("SayNothing")));
	TestTrue(TEXT("Countdown state clears after expiry"), Party->GetPendingSilentReplyID().IsNone());
	TestTrue(TEXT("Duration clears with the countdown"),
		FMath::IsNearlyZero(Party->GetReplyTimeoutDuration()));

	// 3. A player who answers first is never overridden: the offered replies are gone.
	UDialogue* Answered = MakeDialogue(true);
	Party->OnDialogueRepliesAvailable.Broadcast(Answered, Answered->AvailableResponses);
	TestTrue(TEXT("A later dialogue starts its own countdown"), Party->IsReplyTimeoutActive());
	Answered->AvailableResponses.Reset();
	TestFalse(TEXT("An answered prompt cannot resolve to silence"),
		Party->ResolveReplyCountdownNow());
	TestFalse(TEXT("Countdown stops when the player already answered"), Party->IsReplyTimeoutActive());
	TestEqual(TEXT("An answered prompt never gets a forced silence reply"), Party->SelectionCount, 1);

	// 4. Without the authored silence reply Native's indefinite wait is preserved.
	UDialogue* NoSilence = MakeDialogue(false);
	Party->OnDialogueRepliesAvailable.Broadcast(NoSilence, NoSilence->AvailableResponses);
	TestFalse(TEXT("No countdown without the authored silence reply"), Party->IsReplyTimeoutActive());
	TestEqual(TEXT("Without a countdown nothing is selected"), Party->SelectionCount, 1);

	// 5. Profile policy: override wins, disabled and zero seconds stop the countdown.
	FTerritoryReplyTimeoutRule Rule;
	Rule.Dialogue = WithSilence;
	Rule.Seconds = 12.f;
	Rule.SilentReplyID = TEXT("SilentOverride");
	Profile->DialogueOverrides.Add(Rule);
	TestTrue(TEXT("A dialogue override replaces the default duration"),
		FMath::IsNearlyEqual(Profile->ResolveSeconds(WithSilence), 12.f));
	TestTrue(TEXT("A dialogue override replaces the silence id"),
		Profile->ResolveSilentReplyID(WithSilence) == FName(TEXT("SilentOverride")));
	TestTrue(TEXT("An unmatched dialogue keeps the default duration"),
		FMath::IsNearlyEqual(Profile->ResolveSeconds(Answered), 5.f));
	Profile->bEnabled = false;
	TestFalse(TEXT("A disabled profile resolves no countdown"),
		Profile->IsEnabledForDialogue(Answered));
	Profile->bEnabled = true;
	Profile->DefaultSeconds = 0.f;
	TestFalse(TEXT("Zero seconds disables the countdown"),
		Profile->IsEnabledForDialogue(Answered));
	Profile->DefaultSeconds = 5.f;
	Party->ReplyTimeoutProfile = nullptr;
	Party->OnDialogueRepliesAvailable.Broadcast(NoSilence, NoSilence->AvailableResponses);
	TestFalse(TEXT("Without a profile the countdown never starts"), Party->IsReplyTimeoutActive());

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
