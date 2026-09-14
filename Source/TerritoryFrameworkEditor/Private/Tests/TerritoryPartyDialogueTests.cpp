#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryDialogueLifecycleProbe.h"
#include "Cinematics/TerritoryCinematicPresentationSubsystem.h"
#include "Tales/TerritoryDialogueLifecycleComponent.h"
#include "Tales/Dialogue.h"
#include "Components/LODSyncComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyDialogueLifecycle,
	"TerritoryFramework.Presentation.Cinematics.PartyReplacementUsesOneNativeGroupExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyDialogueLifecycle::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> FrameCounter(GFrameCounter, GFrameCounter);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Owner = World->SpawnActor<AActor>();
	auto* Party = NewObject<UTerritoryPartyDialogueProbe>(Owner);
	auto* OtherParty = NewObject<UTerritoryPartyDialogueProbe>(Owner);
	Owner->AddInstanceComponent(Party);
	Owner->AddInstanceComponent(OtherParty);
	Party->RegisterComponent();
	OtherParty->RegisterComponent();
	TArray<UTerritoryDialogueLifecycleProbe*> Members;
	TArray<UTerritoryDialogueLifecycleComponent*> MemberObservers;
	const auto* Current = FindFProperty<FObjectPropertyBase>(UTalesComponent::StaticClass(), TEXT("CurrentDialogue"));
	const auto* ControllerTales = FindFProperty<FObjectPropertyBase>(ANarrativePlayerController::StaticClass(), TEXT("TalesComponent"));
	if (!Current || !ControllerTales) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* PC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
		PC->SetRole(ROLE_Authority);
		auto* Member = NewObject<UTerritoryDialogueLifecycleProbe>(PC);
		ControllerTales->SetObjectPropertyValue_InContainer(PC, Member);
		PC->AddInstanceComponent(Member);
		Member->RegisterComponent();
		Members.Add(Member);
		MemberObservers.Add(UTerritoryDialogueLifecycleComponent::FindOrCreate(PC));
		TestTrue(TEXT("Native join installs the party observer through the member delegate"), Party->AddPartyMember(Member));
	}
	auto* Observer = UTerritoryDialogueLifecycleComponent::FindOrCreateForTales(Party);
	TInlineComponentArray<UTerritoryDialogueLifecycleComponent*> PartyObservers(Owner);
	TestEqual(TEXT("Multiple members install exactly one observer for the party"), PartyObservers.Num(), 1);
	TestFalse(TEXT("Party observer adds no replicated component"), Observer->GetIsReplicated());
	auto* OtherObserver = UTerritoryDialogueLifecycleComponent::FindOrCreateForTales(OtherParty);
	TestTrue(TEXT("Different Tales components on one actor receive independent observers"), Observer != OtherObserver);
	const auto Seed = [&](UTerritoryPartyDialogueProbe* Target)
	{
		auto* Dialogue = NewObject<UDialogue>(Target);
		Dialogue->OwningComp = Target;
		Dialogue->OwningController = Members[0]->GetOwningController();
		Current->SetObjectPropertyValue_InContainer(Target, Dialogue);
		for (auto* Member : Target->GetPartyMembers()) Current->SetObjectPropertyValue_InContainer(Member, Dialogue);
		Target->OnDialogueBegan.Broadcast(Dialogue);
		return Dialogue;
	};
	const auto Advance = [&]() { ++GFrameCounter; World->GetTimerManager().Tick(0.01f); };
	UDialogue* Original = Seed(Party);
	UDialogue* Unrelated = Seed(OtherParty);
	AddExpectedError(TEXT("MakeDialogue was passed UDialogue"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Native rejects the replacement"), Party->BeginDialogue(UDialogue::StaticClass()));
	TestNull(TEXT("Party authority is empty before reconciliation"), Party->GetCurrentDialogue());
	TestTrue(TEXT("Fixture reaches Native's stale member-alias failure"), Members[0]->GetCurrentDialogue() == Original);
	Advance();
	TestEqual(TEXT("Exactly one Native group exit is requested"), Party->GroupExits, 1);
	for (auto* Member : Members)
	{
		TestNull(TEXT("Native group exit clears the authoritative member alias"), Member->GetCurrentDialogue());
		TestEqual(TEXT("Each member receives one Native party-exit dispatch"), Member->PartyExitDispatches, 1);
	}
	TestTrue(TEXT("Another party on the same actor is unaffected"), OtherParty->GetCurrentDialogue() == Unrelated);
	Advance();
	TestEqual(TEXT("Group exits are not repeated by other members or a polling loop"), Party->GroupExits, 1);

	Original = Seed(Party);
	Party->OnDialogueFinished.Broadcast(Original, true, EExitDialogueReason::EDR_NewDialogueStarted);
	auto* Replacement = Seed(Party);
	Advance();
	TestEqual(TEXT("Successful party replacement cancels the deferred exit"), Party->GroupExits, 1);
	TestTrue(TEXT("Successful replacement stays current"), Party->GetCurrentDialogue() == Replacement);
	Party->OnDialogueFinished.Broadcast(Original, true, EExitDialogueReason::EDR_NewDialogueStarted);
	Advance();
	TestEqual(TEXT("Late old finish cannot close the replacement"), Party->GroupExits, 1);

	Party->OnDialogueFinished.Broadcast(Replacement, true, EExitDialogueReason::EDR_NewDialogueStarted);
	Current->SetObjectPropertyValue_InContainer(Party, nullptr);
	Owner->SetRole(ROLE_SimulatedProxy);
	Advance();
	TestEqual(TEXT("Lost server authority prevents group exit"), Party->GroupExits, 1);
	TestNull(TEXT("Clients cannot install a party observer"), UTerritoryDialogueLifecycleComponent::FindOrCreateForTales(Party));
	Owner->SetRole(ROLE_Authority);
	Original = Seed(Party);
	Party->OnDialogueFinished.Broadcast(Original, true, EExitDialogueReason::EDR_NewDialogueStarted);
	Current->SetObjectPropertyValue_InContainer(Party, nullptr);
	Observer->UnregisterComponent();
	Advance();
	TestEqual(TEXT("Unregister cancels deferred group exit"), Party->GroupExits, 1);
	TestFalse(TEXT("Party delegates are released"), Party->OnDialogueFinished.IsAlreadyBound(Observer, &UTerritoryDialogueLifecycleComponent::HandleDialogueFinished));
	Observer->RegisterComponent();
	TestTrue(TEXT("Re-register binds the same exact party component"), Party->OnDialogueFinished.IsAlreadyBound(Observer, &UTerritoryDialogueLifecycleComponent::HandleDialogueFinished));
	for (auto* MemberObserver : MemberObservers) MemberObserver->DestroyComponent();
	Observer->DestroyComponent();
	OtherObserver->DestroyComponent();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyDialoguePresentation,
	"TerritoryFramework.Presentation.Cinematics.PartyPresentationFollowsMembershipAndNativeEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyDialoguePresentation::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> FrameCounter(GFrameCounter, GFrameCounter);
	TGuardValue<bool> ScriptCallbacks(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Owner = World->SpawnActor<AActor>();
	auto* Party = NewObject<UNarrativePartyComponent>(Owner);
	auto* NextParty = NewObject<UNarrativePartyComponent>(Owner);
	auto* FirstPC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	auto* SecondPC = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	FirstPC->SetRole(ROLE_Authority);
	SecondPC->SetRole(ROLE_Authority);
	auto* FirstTales = FirstPC->GetTalesComponent();
	auto* SecondTales = SecondPC->GetTalesComponent();
	auto* First = NewObject<UTerritoryCinematicPresentationSubsystem>(NewObject<ULocalPlayer>(GEngine));
	auto* Second = NewObject<UTerritoryCinematicPresentationSubsystem>(NewObject<ULocalPlayer>(GEngine));
	const auto* Current = FindFProperty<FObjectPropertyBase>(UTalesComponent::StaticClass(), TEXT("CurrentDialogue"));
	if (!Current) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	First->BindToController(FirstPC);
	Party->AddPartyMember(FirstTales);
	Party->AddPartyMember(SecondTales);
	const auto Seed = [&](UNarrativePartyComponent* Target)
	{
		auto* Dialogue = NewObject<UDialogue>(Target);
		Dialogue->OwningComp = Target;
		Current->SetObjectPropertyValue_InContainer(Target, Dialogue);
		for (auto* Member : Target->GetPartyMembers()) Current->SetObjectPropertyValue_InContainer(Member, Dialogue);
		Target->OnDialogueBegan.Broadcast(Dialogue);
		return Dialogue;
	};
	auto* Original = Seed(Party);
	TestTrue(TEXT("Joined party's Native Began activates presentation"), First->IsNarrativeCinematicActive());
	Second->BindToController(SecondPC);
	TestTrue(TEXT("Late local binding recovers the already-running Native party dialogue"), Second->IsNarrativeCinematicActive());
	TestTrue(TEXT("Party line events refresh speaker visuals"), Party->OnNPCDialogueLineStarted.IsAlreadyBound(First, &UTerritoryCinematicPresentationSubsystem::HandleNPCDialogueLineStarted));
	auto* Subject = World->SpawnActor<AActor>();
	auto* LOD = NewObject<ULODSyncComponent>(Subject);
	Subject->AddInstanceComponent(LOD);
	LOD->RegisterComponent();
	LOD->ForcedLOD = 3;
	First->RegisterCinematicSubject(Subject);
	Second->RegisterCinematicSubject(Subject);
	Party->RemovePartyMember(FirstTales);
	TestFalse(TEXT("Leaving releases old party presentation even if Native keeps a member alias"), First->IsNarrativeCinematicActive());
	TestTrue(TEXT("Remaining member retains presentation"), Second->IsNarrativeCinematicActive());
	TestEqual(TEXT("A remaining local member keeps the shared speaker detailed"), LOD->ForcedLOD, 0);
	TestFalse(TEXT("Leaving unbinds old party dialogue callbacks"), Party->OnDialogueBegan.IsAlreadyBound(First, &UTerritoryCinematicPresentationSubsystem::HandleDialogueBegan));
	Party->OnDialogueFinished.Broadcast(Original, true, EExitDialogueReason::EDR_NewDialogueStarted);
	Original->Deinitialize();
	Current->SetObjectPropertyValue_InContainer(Party, nullptr);
	++GFrameCounter;
	World->GetTimerManager().Tick(0.01f);
	TestFalse(TEXT("A deinitialized member alias cannot resurrect presentation"), Second->IsNarrativeCinematicActive());
	TestEqual(TEXT("Last presentation restores original detail"), LOD->ForcedLOD, 3);

	Original = Seed(Party);
	Party->OnDialogueFinished.Broadcast(Original, true, EExitDialogueReason::EDR_NewDialogueStarted);
	auto* Next = Seed(NextParty);
	NextParty->AddPartyMember(SecondTales);
	TestTrue(TEXT("Party switch observes the new authority immediately"), Second->ActiveDialogue == Next);
	++GFrameCounter;
	World->GetTimerManager().Tick(0.01f);
	TestTrue(TEXT("Old pending reconciliation cannot close the new party dialogue"), Second->ActiveDialogue == Next);
	Party->OnDialogueFinished.Broadcast(Original, false, EExitDialogueReason::EDR_NewDialogueStarted);
	TestTrue(TEXT("Old party no longer controls local presentation"), Second->ActiveDialogue == Next);
	SecondTales->OnLeaveParty.Broadcast(NextParty);
	TestTrue(TEXT("An older leave broadcast cannot unbind a party already rejoined"), Second->BoundPartyComponent == NextParty);
	TestTrue(TEXT("Current Native membership wins over a stale leave event"), Second->ActiveDialogue == Next);
	Second->BindToController(nullptr);
	TestFalse(TEXT("Controller removal releases party presentation"), Second->IsNarrativeCinematicActive());
	TestFalse(TEXT("Controller removal releases membership delegates"), SecondTales->OnJoinedParty.IsAlreadyBound(Second, &UTerritoryCinematicPresentationSubsystem::HandleJoinedParty));
	TestFalse(TEXT("Controller removal releases new party delegates"), NextParty->OnDialogueBegan.IsAlreadyBound(Second, &UTerritoryCinematicPresentationSubsystem::HandleDialogueBegan));
	First->Deinitialize();
	Second->Deinitialize();
	Subject->Destroy();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
