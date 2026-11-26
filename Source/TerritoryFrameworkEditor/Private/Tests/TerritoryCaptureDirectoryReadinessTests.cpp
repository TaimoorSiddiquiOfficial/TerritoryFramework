#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Tales/TerritoryStoryConditions.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "UObject/UnrealType.h"

/**
 * The capture directory is replicated, so on a client "no rows yet" and "the server holds no rows"
 * are the same empty array. Before bCaptureDirectoryPublished existed, every client-side reader of
 * GetFactionDistrictCount answered 0 during join, which is a false statement about another
 * faction's holdings rather than an unknown one: an authored "At Most 0 Claimed Districts" quest
 * condition therefore PASSED for a faction that actually held Districts.
 *
 * These tests pin the contract that closes that gap:
 *   1. A server is ready the moment it can be asked.
 *   2. A client is not ready until the server's publish flag arrives, and the count is reported as
 *      unknown rather than as zero.
 *   3. Once the flag arrives, a genuinely empty directory is a known zero (ready, count 0) - the
 *      flag must not be confused with "has rows".
 *   4. A story condition refuses to decide on an unknown count instead of comparing it.
 *   5. The replicated ordering that makes the flag trustworthy is a declared property order.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCaptureDirectoryReadiness,
	"TerritoryFramework.WorldPartition.Regression.CaptureDirectoryReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCaptureDirectoryReadiness::RunTest(const FString& Parameters)
{
	const FGameplayTag Heroes =
		FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Market =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"), false);
	if (!TestTrue(TEXT("Readiness fixtures resolve"),
		Heroes.IsValid() && Market.IsValid()))
	{
		return false;
	}

	auto MakeDistrictRow = [](const FGameplayTag& Tag, const FGameplayTag& Owner)
	{
		FReplicatedCaptureSummary Row;
		Row.TerritoryTag = Tag;
		Row.TerritoryGUID = FGuid::NewGuid();
		Row.HierarchyLevel = ETerritoryHierarchyLevel::District;
		Row.CurrentOwner = Owner;
		Row.State = ETerritoryState::Claimed;
		Row.Availability = ETerritoryAvailability::Unlocked;
		return Row;
	};
	const FReplicatedCaptureSummary OwnedRow = MakeDistrictRow(Market, Heroes);

	// ─── 1. Declaration order is the reason the flag can be trusted ───
	// A client compares and applies replicating properties in RepIndex order, not in the order the
	// class called DOREPLIFETIME: FRepLayout::InitFromClass indexes Parents by RepIndex and iterates
	// that array, and UClass::SetUpRuntimeReplicationData assigns RepIndex from the declared
	// property list. So the rows must be DECLARED before the flag that certifies them. Asserting the
	// real mechanism here is what stops a later refactor from moving the flag above the rows and
	// silently certifying an empty snapshot.
	//
	// RepIndex is assigned LAZILY: UClass::SetUpRuntimeReplicationData returns immediately when
	// CLASS_ReplicationDataIsSetUp was already set, and until it has run both properties read back
	// as RepIndex 0. Reading RepIndex without forcing the setup first therefore fails with "the two
	// values are equal" no matter where the flag is declared, which would make this assertion
	// vacuous the moment the class happened to be linked later. Call the setup explicitly so the
	// assertion measures declaration order instead of setup timing.
	{
		UClass* WorldStateClass = ATerritoryWorldState::StaticClass();
		if (!TestNotNull(TEXT("World State class is available"), WorldStateClass)) return false;
		WorldStateClass->SetUpRuntimeReplicationData();

		const FProperty* RowsProperty = WorldStateClass->FindPropertyByName(
			GET_MEMBER_NAME_CHECKED(ATerritoryWorldState, ReplicatedCaptureSummaries));
		const FProperty* PublishedProperty = WorldStateClass->FindPropertyByName(
			GET_MEMBER_NAME_CHECKED(ATerritoryWorldState, bCaptureDirectoryPublished));
		if (!TestNotNull(TEXT("Capture rows property is reflected"), RowsProperty)
			|| !TestNotNull(TEXT("The publish flag property is reflected"), PublishedProperty))
		{
			return false;
		}

		if (!TestNotEqual(TEXT("Both properties were assigned a RepIndex by the runtime setup"),
			RowsProperty->RepIndex, PublishedProperty->RepIndex))
		{
			return false;
		}

		TestTrue(TEXT("The rows are serialized before the flag that certifies them"),
			RowsProperty->RepIndex < PublishedProperty->RepIndex);

		// Registered as well as ordered: an unregistered property is forced to COND_Never and never
		// reaches the wire, which would leave every client permanently unready.
		const ATerritoryWorldState* Defaults = GetDefault<ATerritoryWorldState>();
		if (!TestNotNull(TEXT("World State defaults exist"), Defaults)) return false;

		TArray<FLifetimeProperty> LifetimeProps;
		Defaults->GetLifetimeReplicatedProps(LifetimeProps);

		auto IsRegistered = [&LifetimeProps](const FProperty* Property)
		{
			return LifetimeProps.ContainsByPredicate(
				[Property](const FLifetimeProperty& Entry)
				{
					return Entry.RepIndex == Property->RepIndex
						&& Entry.Condition != COND_Never;
				});
		};
		TestTrue(TEXT("Capture rows actually replicate"), IsRegistered(RowsProperty));
		TestTrue(TEXT("The publish flag actually replicates"), IsRegistered(PublishedProperty));
	}

	// ─── 2. A server is ready immediately, even with an empty directory ───
	UWorld* ServerWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Server readiness world"), ServerWorld)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(ServerWorld);
	ON_SCOPE_EXIT
	{
		ServerWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(ServerWorld);
	};

	ATerritoryWorldState* Server = ServerWorld->SpawnActor<ATerritoryWorldState>();
	if (!TestNotNull(TEXT("Server World State spawned"), Server)) return false;
	TestNotEqual(TEXT("Server fixture is not a client"), ServerWorld->GetNetMode(), NM_Client);
	TestTrue(TEXT("A server's directory is authoritative and ready at once"),
		Server->IsCaptureDirectoryReady());

	int32 ServerCount = 0;
	TestTrue(TEXT("A server answers even with no rows"),
		UTerritoryBlueprintLibrary::TryGetFactionDistrictCount(
			ServerWorld, Heroes, ServerCount));
	TestEqual(TEXT("A server with no rows answers a known zero"), ServerCount, 0);

	Server->SetCaptureSummary(OwnedRow);
	TestTrue(TEXT("A server still answers after publishing"),
		UTerritoryBlueprintLibrary::TryGetFactionDistrictCount(
			ServerWorld, Heroes, ServerCount));
	TestEqual(TEXT("A server counts its own row"), ServerCount, 1);

	// ─── 3. A client that has received nothing reports unknown, not zero ───
	FWorldContext& ClientContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* ClientWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Client readiness world"), ClientWorld)) return false;
	ClientContext.SetCurrentWorld(ClientWorld);
	ClientWorld->NextURL = TEXT("127.0.0.1");
	ON_SCOPE_EXIT
	{
		ClientWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(ClientWorld);
	};
	TestEqual(TEXT("Client fixture has a real client net mode"),
		ClientWorld->GetNetMode(), NM_Client);

	ATerritoryWorldState* Replica = ClientWorld->SpawnActor<ATerritoryWorldState>();
	if (!TestNotNull(TEXT("Client World State replica spawned"), Replica)) return false;
	Replica->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client replica holds no authority"), Replica->HasAuthority());

	TestFalse(TEXT("A client is not ready before the publish flag arrives"),
		Replica->IsCaptureDirectoryReady());

	int32 ClientCount = 0;
	TestFalse(TEXT("An unhydrated client reports the count as unknown"),
		UTerritoryBlueprintLibrary::TryGetFactionDistrictCount(
			ClientWorld, Heroes, ClientCount));
	TestEqual(TEXT("The unknown count leaves the out param at zero"), ClientCount, 0);

	// The convenience accessor keeps its signature, so it can only say 0 here. This is the exact
	// reading the story condition used to compare against - it is not a claim that the faction
	// holds nothing, which is why gameplay must ask the readiness-aware query.
	TestEqual(TEXT("The convenience count cannot express unknown and answers zero"),
		UTerritoryBlueprintLibrary::GetFactionDistrictCount(ClientWorld, Heroes), 0);

	// ─── 4. A story condition refuses to decide on an unknown count ───
	// "At Most 0 Claimed Districts" is the shape that used to pass on a client during join for a
	// faction that really held Districts. It must now stay false until the directory is known.
	UTerritoryFactionDistrictHoldingCondition* Condition =
		NewObject<UTerritoryFactionDistrictHoldingCondition>(GetTransientPackage());
	Condition->FactionSource = ETerritoryCaptureFactionSource::ExplicitFaction;
	Condition->Faction = Heroes;
	Condition->Comparison = ETerritoryIntegerComparison::AtMost;
	Condition->DistrictCount = 0;

	// A pawn supplies the world to the condition; nothing else about it is consulted because the
	// faction source is explicit.
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	APawn* ContextPawn = ClientWorld->SpawnActor<APawn>(
		APawn::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestNotNull(TEXT("Condition context pawn spawned"), ContextPawn)) return false;

	TestFalse(TEXT("A story condition does not pass while the count is unknown"),
		TerritoryTales::DoesConditionPass(Condition, ContextPawn, nullptr, nullptr));

	// ─── 5. Once the rows and the flag arrive, the same condition decides normally ───
	// Assigning the rows and then the flag mirrors the replicated application order asserted in 1.
	Replica->ReplicatedCaptureSummaries = {OwnedRow};
	Replica->bCaptureDirectoryPublished = true;
	TestTrue(TEXT("A hydrated client becomes ready"), Replica->IsCaptureDirectoryReady());
	TestTrue(TEXT("A hydrated client answers the count"),
		UTerritoryBlueprintLibrary::TryGetFactionDistrictCount(
			ClientWorld, Heroes, ClientCount));
	TestEqual(TEXT("A hydrated client counts the replicated row"), ClientCount, 1);
	TestFalse(TEXT("At Most 0 is false once the faction is known to hold a District"),
		TerritoryTales::DoesConditionPass(Condition, ContextPawn, nullptr, nullptr));

	Condition->Comparison = ETerritoryIntegerComparison::AtLeast;
	Condition->DistrictCount = 1;
	TestTrue(TEXT("At Least 1 passes once the faction is known to hold a District"),
		TerritoryTales::DoesConditionPass(Condition, ContextPawn, nullptr, nullptr));

	// ─── 6. A genuinely empty directory after publish is a known zero, not an unknown ───
	// The flag certifies "the authority answered", never "there are rows". A campaign with no
	// authored strategic Districts must resolve to a real zero rather than stay unavailable.
	Replica->ReplicatedCaptureSummaries.Reset();
	TestTrue(TEXT("An empty but published directory is still ready"),
		Replica->IsCaptureDirectoryReady());
	TestTrue(TEXT("An empty but published directory answers the count"),
		UTerritoryBlueprintLibrary::TryGetFactionDistrictCount(
			ClientWorld, Heroes, ClientCount));
	TestEqual(TEXT("An empty but published directory is a known zero"), ClientCount, 0);

	// ─── 7. The authority gate is what keeps a client from publishing itself ───
	// RefreshStrategicDirectory seeds definitions and sets the flag; on a client it must do
	// neither, otherwise a replica could certify its own empty directory.
	Replica->bCaptureDirectoryPublished = false;
	Replica->RefreshStrategicDirectory();
	TestFalse(TEXT("A client cannot publish the directory itself"),
		Replica->bCaptureDirectoryPublished);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
