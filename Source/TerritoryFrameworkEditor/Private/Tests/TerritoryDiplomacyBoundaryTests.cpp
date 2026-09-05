#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyMutationBoundaries,
	"TerritoryFramework.Diplomacy.Regression.VerifiedEventsAndBoundedReputation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyMutationBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Diplomacy world exists"), World)) return false;
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Diplomacy->DeclareWar(Heroes, Heroes);
	Diplomacy->DeclarePeace(Heroes, FGameplayTag());
	Diplomacy->FormAlliance(FGameplayTag(), Bandits);
	Diplomacy->SignNonAggression(Heroes, Heroes);
	TestEqual(TEXT("Rejected pairs never produce successful diplomacy events"), Diplomacy->GetDiplomacyHistory().Num(), 0);
	TestEqual(TEXT("Rejected pairs never create treaties"), Diplomacy->GetAllTreaties().Num(), 0);
	Diplomacy->RestorePersistentState({}, {}, {});
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>(World);
	Probe->DiplomacyCallback = [&](FGameplayTag A, FGameplayTag B, EDiplomacyState State)
	{
		if (State == EDiplomacyState::Alliance) Diplomacy->DeclareWar(A, B);
	};
	Diplomacy->OnDiplomacyStateChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::DiplomacyChanged);
	Diplomacy->FormAlliance(Heroes, Bandits);
	TestEqual(TEXT("Callback-authored final treaty is authoritative"), Diplomacy->GetDiplomacyState(Heroes, Bandits), EDiplomacyState::War);
	TestEqual(TEXT("Superseded alliance does not record false success"), Diplomacy->GetDiplomacyHistory().Num(), 1);
	Diplomacy->OnDiplomacyStateChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::DiplomacyChanged);
	Diplomacy->DeclareWar(Heroes, Bandits);
	TestEqual(TEXT("Idempotent war does not duplicate history"), Diplomacy->GetDiplomacyHistory().Num(), 1);
	Diplomacy->SetReputation(Heroes, MAX_int32);
	Diplomacy->AddReputation(Heroes, 10);
	TestEqual(TEXT("Positive reputation saturates safely"), Diplomacy->GetReputation(Heroes), MAX_int32);
	Diplomacy->SetReputation(Heroes, MIN_int32);
	Diplomacy->AddReputation(Heroes, -10);
	TestEqual(TEXT("Negative reputation saturates safely"), Diplomacy->GetReputation(Heroes), MIN_int32);
	Diplomacy->RestorePersistentState({}, {}, {});
	Diplomacy->SignTradeAgreement(Heroes, Bandits, 10.f);
	TestEqual(TEXT("Timed treaty without a Narrative clock is rejected"), Diplomacy->GetAllTreaties().Num(), 0);
	TestEqual(TEXT("Rejected timed treaty has no success event"), Diplomacy->GetDiplomacyHistory().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyRestoreBoundaries,
	"TerritoryFramework.Diplomacy.Regression.RestoreCanonicalPairs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyRestoreBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Treaty restore world exists"), World)) return false;
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	FTreatyRecord First;
	First.FactionA = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	First.FactionB = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	First.State = EDiplomacyState::Alliance;
	First.SignedGameTime = 1.f;
	FTreatyRecord Newer = First;
	Swap(Newer.FactionA, Newer.FactionB);
	Newer.State = EDiplomacyState::Ceasefire;
	Newer.SignedGameTime = 2.f;
	Newer.bPermanent = false;
	Newer.ExpiryGameTime = 20.f;
	Diplomacy->RestorePersistentState({First, Newer}, {}, {});
	TestEqual(TEXT("A canonical pair has one restored authority"), Diplomacy->GetAllTreaties().Num(), 1);
	TestEqual(TEXT("Newest signed duplicate survives migration"), Diplomacy->GetDiplomacyState(First.FactionA, First.FactionB), EDiplomacyState::Ceasefire);
	const TArray<FTreatyRecord> Saved = Diplomacy->GetAllTreaties();
	Diplomacy->RestorePersistentState({}, {}, {});
	Diplomacy->RestorePersistentState(Saved, {}, {});
	TestEqual(TEXT("Restoring twice stays idempotent"), Diplomacy->GetAllTreaties().Num(), 1);
	if (Diplomacy->GetAllTreaties().Num() == 1)
	{
		TestEqual(TEXT("Migration preserves timed expiry"), Diplomacy->GetAllTreaties()[0].ExpiryGameTime, 20.f);
	}
	Diplomacy->RestorePersistentState({Newer, First}, {}, {});
	TestEqual(TEXT("Duplicate selection is independent of save array order"), Diplomacy->GetDiplomacyState(First.FactionA, First.FactionB), EDiplomacyState::Ceasefire);
	TestEqual(TEXT("Restoration never invents story events"), Diplomacy->GetDiplomacyHistory().Num(), 0);
	World->DestroyWorld(false);
	return true;
}

#endif
