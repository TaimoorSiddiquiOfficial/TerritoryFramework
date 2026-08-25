#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryWorldState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyWorldStateLiveBridge,
	"TerritoryFramework.Diplomacy.Replication.RichTreatyAndHistoryStayCurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyWorldStateLiveBridge::RunTest(const FString& Parameters)
{
	// An EditorPreview world assigns spawned actors ROLE_None because it is neither a
	// server nor a client. That fixture made every authority-only live handler return
	// before exercising the replication read model. A transient actor has the same
	// authority role as a server-side WorldState without inventing a fake net world.
	ATerritoryWorldState* WorldState = NewObject<ATerritoryWorldState>();
	TestNotNull(TEXT("Territory WorldState created"), WorldState);
	if (!WorldState) return false;
	TestTrue(TEXT("Live bridge fixture executes with server authority"),
		WorldState->HasAuthority());

	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	TestTrue(TEXT("Test faction tags resolve"), Bandits.IsValid() && Heroes.IsValid());

	FReplicatedTreaty Existing;
	Existing.TreatyID = FGuid::NewGuid();
	Existing.FactionA = Bandits;
	Existing.FactionB = Heroes;
	Existing.State = EDiplomacyState::TradeAgreement;
	Existing.SignedGameTime = 12.0;
	Existing.ExpiryGameTime = 97.0;
	Existing.bPermanent = false;
	WorldState->SetTreaty(Existing);

	UFunction* StateHandler = WorldState->FindFunction(TEXT("OnDiplomacyChangedLive"));
	TestNotNull(TEXT("Live treaty bridge is bound as a UFUNCTION"), StateHandler);
	if (StateHandler)
	{
		WorldState->OnDiplomacyChangedLive(Bandits, Heroes, EDiplomacyState::Ceasefire);
		const FReplicatedTreaty Changed = WorldState->GetTreatyBetween(Bandits, Heroes);
		TestEqual(TEXT("Live state mutation reaches the client treaty row"),
			Changed.State, EDiplomacyState::Ceasefire);
		TestEqual(TEXT("A transient lookup gap does not erase signed time"),
			Changed.SignedGameTime, 12.0);
		TestEqual(TEXT("A transient lookup gap does not erase treaty expiry"),
			Changed.ExpiryGameTime, 97.0);
		TestFalse(TEXT("A transient lookup gap preserves timed-treaty policy"),
			Changed.bPermanent);

		WorldState->OnDiplomacyChangedLive(Bandits, Heroes, EDiplomacyState::None);
		TestEqual(TEXT("None removes the replicated treaty instead of retaining a ghost row"),
			WorldState->GetAllTreaties().Num(), 0);
	}

	UFunction* EventHandler = WorldState->FindFunction(TEXT("OnDiplomacyEventLive"));
	TestNotNull(TEXT("Live diplomacy-history bridge is bound as a UFUNCTION"), EventHandler);
	if (EventHandler)
	{
		for (int32 Index = 0; Index < 505; ++Index)
		{
			FDiplomacyEvent Event;
			Event.FactionA = Bandits;
			Event.FactionB = Heroes;
			Event.GameTime = static_cast<float>(Index);
			WorldState->OnDiplomacyEventLive(Event);
		}

		const FArrayProperty* HistoryProperty = FindFProperty<FArrayProperty>(
			ATerritoryWorldState::StaticClass(), TEXT("ReplicatedDiplomacyHistory"));
		TestNotNull(TEXT("Replicated diplomacy history property exists"), HistoryProperty);
		if (HistoryProperty)
		{
			void* HistoryAddress = HistoryProperty->ContainerPtrToValuePtr<void>(WorldState);
			FScriptArrayHelper History(HistoryProperty, HistoryAddress);
			TestEqual(TEXT("Live diplomacy history remains bounded"), History.Num(), 500);
			if (History.Num() == 500)
			{
				const FDiplomacyEvent* First =
					reinterpret_cast<const FDiplomacyEvent*>(History.GetRawPtr(0));
				const FDiplomacyEvent* Last =
					reinterpret_cast<const FDiplomacyEvent*>(History.GetRawPtr(History.Num() - 1));
				TestEqual(TEXT("History trims oldest live events"), First->GameTime, 5.f);
				TestEqual(TEXT("History retains newest live event"), Last->GameTime, 504.f);
			}
		}
	}

	return true;
}

#endif
