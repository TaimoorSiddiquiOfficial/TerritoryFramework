#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryPartyReplyProbe.h"
#include "Tales/TerritoryNarrativeParty.h"
#include "Tales/TerritoryNarrativePartyComponent.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyMembership,
	"TerritoryFramework.Tales.PartyMembership.ValidatedTransferAndNativeReadModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyMembership::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* FirstActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* NextActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* First = CastChecked<UTerritoryNarrativePartyComponent>(FirstActor->PartyTalesComponent);
	auto* Next = CastChecked<UTerritoryNarrativePartyComponent>(NextActor->PartyTalesComponent);
	auto* PC = NewObject<ATerritoryPartyRemoteControllerProbe>(World->PersistentLevel);
	PC->SetRole(ROLE_Authority);
	auto* State = NewObject<ANarrativePlayerState>(World->PersistentLevel);
	State->SetOwner(PC);
	PC->PlayerState = State;
	auto* Member = NewObject<UTerritoryPartyReplyMemberProbe>(PC);
	PC->AddInstanceComponent(Member);
	Member->RegisterComponent();
	InstallTerritoryPartyMemberProbe(PC, Member);
	Member->OnJoinedParty.AddDynamic(Member, &UTerritoryPartyReplyMemberProbe::ObserveJoin);
	Member->OnLeaveParty.AddDynamic(Member, &UTerritoryPartyReplyMemberProbe::ObserveLeave);
	int32 Joins = 0;
	int32 Leaves = 0;
	Member->JoinAction = [&](UNarrativePartyComponent* Party)
	{
		++Joins;
		auto* Actor = CastChecked<ANarrativeParty>(Party->GetOwner());
		TestTrue(TEXT("Join callback sees committed Native member and actor state"), Member->GetParty() == Party
			&& Party->GetPartyMembers().Contains(Member) && Party->GetPartyMemberStates().Contains(State)
			&& Actor->PartyMembers.Contains(State) && Actor->IsNetRelevantFor(PC, nullptr, FVector::ZeroVector));
	};
	Member->LeaveAction = [&](UNarrativePartyComponent* Party)
	{
		++Leaves;
		auto* Actor = CastChecked<ANarrativeParty>(Party->GetOwner());
		TestTrue(TEXT("Leave callback sees Native member and relevance removed"), Member->GetParty() != Party
			&& !Party->GetPartyMembers().Contains(Member) && !Party->GetPartyMemberStates().Contains(State)
			&& !Actor->PartyMembers.Contains(State) && !Actor->IsNetRelevantFor(PC, nullptr, FVector::ZeroVector));
	};
	TestFalse(TEXT("Null member rejected"), First->AddPartyMember(nullptr));
	TestFalse(TEXT("Nested party rejected"), First->AddPartyMember(Next));
	FirstActor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot join"), First->AddPartyMember(Member));
	FirstActor->AddPartyMember(State);
	TestTrue(TEXT("Client actor wrapper has no cache side effect"), FirstActor->PartyMembers.IsEmpty());
	FirstActor->SetRole(ROLE_Authority);
	PC->SetRole(ROLE_AutonomousProxy);
	TestFalse(TEXT("Server rejects a non-authoritative member"), First->AddPartyMember(Member));
	PC->SetRole(ROLE_Authority);
	PC->PlayerState = nullptr;
	TestFalse(TEXT("PlayerState load order fails before any mutation"), First->AddPartyMember(Member));
	TestTrue(TEXT("Failed preflight leaves both Native read models empty"), First->GetPartyMembers().IsEmpty() && FirstActor->PartyMembers.IsEmpty());
	PC->PlayerState = State;
	State->SetOwner(NextActor);
	TestFalse(TEXT("Mismatched PlayerState owner rejected"), First->AddPartyMember(Member));
	State->SetOwner(PC);
	TestTrue(TEXT("Retry after PlayerState becomes ready succeeds"), First->AddPartyMember(Member));
	TestFalse(TEXT("Duplicate join rejected"), First->AddPartyMember(Member));
	auto* Duplicate = NewObject<UTalesComponent>(PC);
	TestFalse(TEXT("Second Tales on the same player cannot duplicate identity"), First->AddPartyMember(Duplicate));
	TestTrue(TEXT("Direct component transfer uses Native's removal and join"), Next->AddPartyMember(Member));
	TestEqual(TEXT("One event per verified join"), Joins, 2);
	TestEqual(TEXT("One departure event"), Leaves, 1);
	TestTrue(TEXT("Old actor no longer replicates to transferred player"), FirstActor->PartyMemberControllers.IsEmpty());
	TestTrue(TEXT("Destination uses exactly one member"), Next->GetPartyMembers().Num() == 1 && NextActor->PartyMembers.Num() == 1);
	TestTrue(TEXT("Component removal updates Native actor lists"), Next->RemovePartyMember(Member));
	TestFalse(TEXT("Repeated removal does not succeed"), Next->RemovePartyMember(Member));
	Member->JoinAction = nullptr;
	Member->LeaveAction = nullptr;

	// Use the actual Native actor entry points and its actual personal Tales slot.
	FirstActor->AddPartyMember(State);
	NextActor->AddPartyMember(State);
	TestTrue(TEXT("Actor transfer leaves one component owner"), PC->GetTalesComponent()->GetParty() == Next
		&& First->GetPartyMembers().IsEmpty() && FirstActor->PartyMembers.IsEmpty());
	FirstActor->RemovePartyMember(State);
	TestTrue(TEXT("Removal through an old actor cannot remove the new party"), PC->GetTalesComponent()->GetParty() == Next);
	NextActor->RemovePartyMember(State);
	TestTrue(TEXT("Actor removal clears component, replicated list and relevance"), !PC->GetTalesComponent()->GetParty()
		&& NextActor->PartyMembers.IsEmpty() && NextActor->PartyMemberControllers.IsEmpty());

	// A custom Native party's refusal used to be ignored by Native Add.
	auto* RefusingActor = World->SpawnActor<AActor>();
	auto* Refusing = NewObject<UTerritoryPartyRefusingDepartureProbe>(RefusingActor);
	RefusingActor->AddInstanceComponent(Refusing);
	Refusing->RegisterComponent();
	TestTrue(TEXT("Custom Native source owns member"), Refusing->AddPartyMember(Member));
	TestFalse(TEXT("Failed departure prevents destination join"), Next->AddPartyMember(Member));
	TestTrue(TEXT("Refused transfer keeps sole source membership"), Member->GetParty() == Refusing
		&& Refusing->GetPartyMembers().Num() == 1 && Next->GetPartyMembers().IsEmpty() && NextActor->PartyMembers.IsEmpty());
	TestFalse(TEXT("An extra Tales component cannot bypass the real player's source party"), Next->AddPartyMember(Duplicate));
	TestTrue(TEXT("Release the refusing fixture explicitly for the next independent case"), Refusing->UNarrativePartyComponent::RemovePartyMember(Member));

	// A stock Native actor's cache must also be reconciled after its component leaves.
	auto* StockActor = World->SpawnActor<ANarrativeParty>();
	StockActor->AddPartyMember(State);
	TestTrue(TEXT("Transfer interoperates with a stock Native source"), Next->AddPartyMember(PC->GetTalesComponent()));
	TestTrue(TEXT("Stock source loses its actor relevance cache"), StockActor->PartyMembers.IsEmpty()
		&& StockActor->PartyMemberControllers.IsEmpty() && StockActor->PartyTalesComponent->GetPartyMembers().IsEmpty());
	NextActor->RemovePartyMember(State);
	const UFunction* AddFunction = Next->FindFunction(TEXT("AddPartyMember"));
	TestTrue(TEXT("Inherited Blueprint node retains authority-only contract"), AddFunction
		&& AddFunction->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPartyMembershipCallbacks,
	"TerritoryFramework.Tales.PartyMembership.CallbackRedirectAndCrossWorldRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPartyMembershipCallbacks::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* FirstActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* NextActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* RedirectActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* First = FirstActor->PartyTalesComponent.Get();
	auto* Next = NextActor->PartyTalesComponent.Get();
	auto* Redirect = RedirectActor->PartyTalesComponent.Get();
	auto* PC = NewObject<ATerritoryPartyRemoteControllerProbe>(World->PersistentLevel);
	PC->SetRole(ROLE_Authority);
	auto* State = NewObject<ANarrativePlayerState>(World->PersistentLevel);
	State->SetOwner(PC);
	PC->PlayerState = State;
	auto* Member = NewObject<UTerritoryPartyReplyMemberProbe>(PC);
	PC->AddInstanceComponent(Member);
	Member->RegisterComponent();
	InstallTerritoryPartyMemberProbe(PC, Member);
	TestTrue(TEXT("Initial membership succeeds"), First->AddPartyMember(Member));
	Member->OnLeaveParty.AddDynamic(Member, &UTerritoryPartyReplyMemberProbe::ObserveLeave);
	Member->LeaveAction = [&](UNarrativePartyComponent* Left)
	{
		if (Left == First) TestTrue(TEXT("Story callback chooses its new party"), Redirect->AddPartyMember(Member));
	};
	TestFalse(TEXT("Outer transfer does not override a story redirect"), Next->AddPartyMember(Member));
	TestTrue(TEXT("Redirect is sole owner with no stale source/destination cache"), Member->GetParty() == Redirect
		&& FirstActor->PartyMembers.IsEmpty() && NextActor->PartyMembers.IsEmpty() && RedirectActor->PartyMembers.Num() == 1);
	Member->LeaveAction = nullptr;
	Member->OnJoinedParty.AddDynamic(Member, &UTerritoryPartyReplyMemberProbe::ObserveJoin);
	Member->JoinAction = [&](UNarrativePartyComponent* Joined)
	{
		if (Joined == Next) TestTrue(TEXT("Immediate story departure succeeds"), Next->RemovePartyMember(Member));
	};
	TestFalse(TEXT("Join returns false if story immediately removes member"), Next->AddPartyMember(Member));
	TestTrue(TEXT("Final callback result has no ghost actor membership"), !Member->GetParty()
		&& NextActor->PartyMembers.IsEmpty() && RedirectActor->PartyMembers.IsEmpty());
	Member->JoinAction = nullptr;
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(OtherWorld);
	auto* Foreign = OtherWorld->SpawnActor<ATerritoryNarrativeParty>();
	TestFalse(TEXT("World/travel boundary cannot register a foreign player"), Foreign->PartyTalesComponent->AddPartyMember(Member));
	TestTrue(TEXT("Original world remains ready to accept member"), Next->AddPartyMember(Member));
	Next->UnregisterComponent();
	Next->RegisterComponent();
	TestTrue(TEXT("Component re-registration retains only Native membership"), Member->GetParty() == Next
		&& Next->GetPartyMembers().Num() == 1 && NextActor->PartyMembers.Num() == 1);
	// Campaign data remains Native's save payload; live party connections are not saved.
	Next->MasterTaskList.Add(TEXT("PartyTransferRegression"), 3);
	Next->PrepareForSave_Implementation();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
	SaveArchive.ArIsSaveGame = true;
	Next->Serialize(SaveArchive);
	auto* RestoredActor = World->SpawnActor<ATerritoryNarrativeParty>();
	auto* Restored = RestoredActor->PartyTalesComponent.Get();
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
	LoadArchive.ArIsSaveGame = true;
	Restored->Serialize(LoadArchive);
	Restored->Load_Implementation();
	TestEqual(TEXT("Native task history survives serialization into a fresh party"), Restored->MasterTaskList.FindRef(TEXT("PartyTransferRegression")), 3);
	TestTrue(TEXT("Loading cannot resurrect live membership or relevance"), Restored->GetPartyMembers().IsEmpty()
		&& RestoredActor->PartyMembers.IsEmpty() && RestoredActor->PartyMemberControllers.IsEmpty());
	TestTrue(TEXT("Explicit post-load join reconnects through the same validated flow"), Restored->AddPartyMember(Member));
	TestTrue(TEXT("Post-load transfer does not leave old connection caches"), NextActor->PartyMembers.IsEmpty() && NextActor->PartyMemberControllers.IsEmpty());
	GEngine->DestroyWorldContext(OtherWorld);
	OtherWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
