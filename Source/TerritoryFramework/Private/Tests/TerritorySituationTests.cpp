#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include <limits>
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Tales/TerritoryCaptureEligibilityCondition.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Tales/DialogueSM.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritorySituationCondition.h"
#include "UObject/UnrealType.h"

namespace
{
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(Name); }
	TArray<FReplicatedCaptureSummary> MakeDirectory()
	{
		TArray<FReplicatedCaptureSummary> Rows;
		const auto Add = [&](const TCHAR* Name, const TCHAR* Parent, ETerritoryHierarchyLevel Level, int32 Children)
		{
			FReplicatedCaptureSummary Row;
			Row.TerritoryTag = Tag(Name);
			Row.TerritoryGUID = FGuid(420, 0, 0, Rows.Num() + 1);
			Row.ParentTerritoryTag = Parent ? Tag(Parent) : FGameplayTag();
			Row.HierarchyLevel = Level;
			Row.TotalChildren = Children;
			Row.bDefinitionBacked = true;
			Row.State = ETerritoryState::Claimed;
			Row.CurrentOwner = Tag(TEXT("Narrative.Factions.Heroes"));
			Rows.Add(Row);
		};
		Add(TEXT("Territory.HavenReach"), nullptr, ETerritoryHierarchyLevel::City, 2);
		Add(TEXT("Territory.HavenReach.MarketSquare"), TEXT("Territory.HavenReach"), ETerritoryHierarchyLevel::District, 1);
		Add(TEXT("Territory.HavenReach.CastleHill"), TEXT("Territory.HavenReach"), ETerritoryHierarchyLevel::District, 1);
		Add(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), TEXT("Territory.HavenReach.MarketSquare"), ETerritoryHierarchyLevel::Place, 0);
		Add(TEXT("Territory.HavenReach.CastleHill.Farm"), TEXT("Territory.HavenReach.CastleHill"), ETerritoryHierarchyLevel::Place, 0);
		return Rows;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFSituationHoldings,
	"TerritoryFramework.Dialogue.Behavior.SituationHoldingsAndUnknownData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSituationHoldings::RunTest(const FString& Parameters)
{
	TArray<FReplicatedCaptureSummary> Rows = MakeDirectory();
	const FGameplayTag Heroes = Rows[3].CurrentOwner;
	const FGameplayTag Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
	const FGameplayTag City = Rows[0].TerritoryTag;
	FTerritorySituationReport Report;
	TestTrue(TEXT("Complete city directory is readable without any loaded actors"),
		UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report));
	TestEqual(TEXT("Places, not aggregate Districts, are counted"), Report.FactionPlaces, 2);
	TestEqual(TEXT("Two of two is one hundred percent"), Report.FactionSharePercent, 100.f);
	TestEqual(TEXT("Existing strict majority reducer selects Heroes"), Report.DominantFaction, Heroes);
	Rows[4].CurrentOwner = Bandits;
	UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report);
	TestFalse(TEXT("Equal faction control invents no dominant faction"), Report.DominantFaction.IsValid());
	TestEqual(TEXT("Split city control is fifty percent"), Report.FactionSharePercent, 50.f);
	Rows[4].State = ETerritoryState::Contested;
	UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report);
	TestEqual(TEXT("Contested Places remain in the denominator"), Report.AvailablePlaces, 2);
	TestFalse(TEXT("A contested holding does not confer dominance"), Report.DominantFaction.IsValid());
	Rows[4].Availability = ETerritoryAvailability::Locked;
	UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report);
	TestEqual(TEXT("Story-locked Places do not disclose or confer holdings"), Report.AvailablePlaces, 1);
	const auto Complete = Rows;
	Rows.RemoveAt(4);
	TestFalse(TEXT("Missing streamed directory row is unknown, not a smaller denominator"),
		UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report));
	Rows = Complete;
	const FReplicatedCaptureSummary DuplicateRow = Rows[3];
	Rows.Add(DuplicateRow);
	TestFalse(TEXT("Duplicate tag/GUID fails closed"), UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report));
	Rows = Complete;
	Rows[4].TerritoryGUID = Rows[3].TerritoryGUID;
	TestFalse(TEXT("Conflicting stable identity cannot inflate control"), UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report));
	Rows = Complete;
	Algo::Reverse(Rows);
	TestTrue(TEXT("Arrival order does not change a valid directory"), UTerritorySituationProfile::ReadPlaceHoldings(Rows, City, Heroes, Report));
	TestEqual(TEXT("Reordered directory retains the same dominant faction"), Report.DominantFaction, Heroes);
	auto* Condition = NewObject<UTerritorySituationCondition>();
	Condition->Query = ETerritorySituationQuery::DistrictDefencePower;
	Condition->Comparison = ETerritoryFloatComparison::AtMost;
	Condition->Value = 50.f;
	Report.bContextKnown = true;
	TestFalse(TEXT("Unknown streamed defence cannot pass as weak or zero"), Condition->MatchesReport(Report));
	Report.bDefencePowerKnown = true;
	Report.DistrictDefencePower = 25.f;
	TestTrue(TEXT("Known weak defence passes the authored numeric threshold"), Condition->MatchesReport(Report));
	Report.DistrictDefencePower = 75.f;
	TestFalse(TEXT("Stronger defence fails the weak branch"), Condition->MatchesReport(Report));
	Condition->Value = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Nonfinite authoring fails closed"), Condition->MatchesReport(Report));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFSituationHistoryAndNarrative,
	"TerritoryFramework.Dialogue.SaveLoad.RetakeHistoryAndNativeBranches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFSituationHistoryAndNarrative::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Narrative test world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	World->SetGameMode(FURL());
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryWorldState* State = World->SpawnActor<ATerritoryWorldState>();
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	APawn* Pawn = World->SpawnActor<APawn>();
	UTalesComponent* Tales = NewObject<UTalesComponent>(Pawn);
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	if (!State || !Place || !Pawn || !Save || !Registry || !Diplomacy)
	{
		AddError(TEXT("Narrative/Territory fixture did not initialize"));
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}
	TArray<FReplicatedCaptureSummary> Rows = MakeDirectory();
	const FGameplayTag Heroes = Rows[3].CurrentOwner;
	const FGameplayTag Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
	const FGameplayTag PlaceTag = Rows[3].TerritoryTag;
	Place->SetActorGUID_Implementation(Rows[3].TerritoryGUID);
	*FindFProperty<FStructProperty>(Place->GetClass(), TEXT("TerritoryTag"))->ContainerPtrToValuePtr<FGameplayTag>(Place) = PlaceTag;
	*FindFProperty<FStructProperty>(Place->GetClass(), TEXT("ParentTerritoryTag"))->ContainerPtrToValuePtr<FGameplayTag>(Place) = Rows[1].TerritoryTag;
	for (const auto& Row : Rows) State->SetCaptureSummary(Row);
	Registry->RegisterTerritory(Place);
	auto* Profile = NewObject<UTerritorySituationProfile>();
	Profile->Territory = PlaceTag;
	Profile->FactionSource = ETerritoryCaptureFactionSource::ExplicitFaction;
	Profile->ExplicitFaction = Heroes;
	auto* Retake = NewObject<UTerritorySituationCondition>();
	Retake->Profile = Profile;
	FTerritoryOwnershipData Candidate = Place->GetOwnershipData();
	Candidate.OwningFaction = Heroes;
	Candidate.State = ETerritoryState::Claimed;
	Candidate.DesiredGuardCount = 0;
	TestTrue(TEXT("First real ownership commit"), Place->CommitOwnershipData(Candidate));
	FNarrativeActorRecord FirstOwnership;
	TestTrue(TEXT("Native saves first ownership without invented history"), Save->CreateActorRecord(Place, FirstOwnership));
	TestFalse(TEXT("Current ownership is not a retake"), Retake->CheckCondition(Pawn, nullptr, Tales));
	Candidate = Place->GetOwnershipData();
	Candidate.OwningFaction = Bandits;
	Place->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Clients cannot record a loss"), Place->CommitOwnershipData(Candidate));
	TestTrue(TEXT("Rejected client mutation adds no history"), Place->GetOwnershipData().FormerOwningFactions.IsEmpty());
	Place->SetRole(ROLE_Authority);
	TestTrue(TEXT("Server commits a real loss"), Place->CommitOwnershipData(Candidate));
	TestTrue(TEXT("Losing one's former Place enables retake dialogue"), Retake->CheckCondition(Pawn, nullptr, Tales));
	TestTrue(TEXT("History is published with the ownership snapshot"), State->GetCaptureSummary(PlaceTag).FormerOwningFactions.HasTagExact(Heroes));
	FNarrativeActorRecord LostOwnership;
	TestTrue(TEXT("Native saves the lost Place"), Save->CreateActorRecord(Place, LostOwnership));
	auto* OwnerWar = NewObject<UTerritorySituationCondition>();
	OwnerWar->Profile = Profile;
	OwnerWar->Query = ETerritorySituationQuery::RelationshipWithOwner;
	auto* Root = NewObject<UDialogueNode_NPC>(Tales);
	auto* WarReply = NewObject<UDialogueNode_NPC>(Root);
	auto* SafeReply = NewObject<UDialogueNode_NPC>(Root);
	WarReply->NodePos = FVector2D(0, 0);
	SafeReply->NodePos = FVector2D(1000, 1000);
	WarReply->Conditions = {Retake, OwnerWar};
	Root->NPCReplies = {SafeReply, WarReply};
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
	TestEqual(TEXT("Native selects the retake branch only when both conditions pass"), Root->GetFirstValidNPCReply(nullptr, Pawn, Tales), WarReply);
	for (const EDiplomacyState Treaty : {EDiplomacyState::Alliance, EDiplomacyState::TradeAgreement,
		EDiplomacyState::NonAggression, EDiplomacyState::Ceasefire, EDiplomacyState::None})
	{
		Diplomacy->SetDiplomacyState(Heroes, Bandits, Treaty);
		TestEqual(TEXT("Changed diplomacy selects safe dialogue through Narrative"), Root->GetFirstValidNPCReply(nullptr, Pawn, Tales), SafeReply);
	}
	Save->LoadActorFromRecord(Place, FirstOwnership);
	TestTrue(TEXT("Loading an earlier campaign clears later history"), Place->GetOwnershipData().FormerOwningFactions.IsEmpty());
	TestFalse(TEXT("Earlier ownership reload also refreshes Native dialogue's directory"), Retake->CheckCondition(Pawn, nullptr, Tales));
	Save->LoadActorFromRecord(Place, LostOwnership);
	TestTrue(TEXT("Actual Native actor reload restores the retake condition"), Retake->CheckCondition(Pawn, nullptr, Tales));
	*FindFProperty<FFloatProperty>(Place->GetClass(), TEXT("FortificationStrength"))->ContainerPtrToValuePtr<float>(Place) = 12.f;
	const auto LoadedReport = Profile->InspectSituation(Pawn, nullptr, Tales, ETerritorySituationScope::District);
	TestTrue(TEXT("A complete loaded defence front is known"), LoadedReport.bDefencePowerKnown);
	TestEqual(TEXT("Dialogue reuses the scheduler's authored fortification power"), LoadedReport.DistrictDefencePower, 12.f);
	auto* Eligibility = NewObject<UTerritoryCaptureEligibilityCondition>();
	Eligibility->SituationProfile = Profile;
	auto* Handover = NewObject<UTerritoryCaptureEvent>();
	Handover->SituationProfile = Profile;
	Handover->Conditions = {Eligibility};
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
	TestTrue(TEXT("The shared profile binds the eligible handover without legacy target fields"), Eligibility->CheckCondition(Pawn, nullptr, Tales));
	Eligibility->bRequireStoryCaptureFlow = true;
	TestFalse(TEXT("Story handover cannot bypass automatic multiplayer flag capture"), Eligibility->CheckCondition(Pawn, nullptr, Tales));
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("A cached story offer cannot change a flag Place's owner"), Place->GetOwningFaction(), Bandits);
	auto* StoryCapture = FindFProperty<FBoolProperty>(Place->GetClass(), TEXT("bStoryCaptureFromBounds"));
	StoryCapture->SetPropertyValue_InContainer(Place, true);
	TestTrue(TEXT("Explicit story capture enables the optional handover"), Eligibility->CheckCondition(Pawn, nullptr, Tales));
	StoryCapture->SetPropertyValue_InContainer(Place, false);
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("Switching back to flag capture invalidates a cached story offer"), Place->GetOwningFaction(), Bandits);
	StoryCapture->SetPropertyValue_InContainer(Place, true);
	auto* Configs = FindFProperty<FMapProperty>(Place->GetClass(), TEXT("RuntimeStateConfigs"))
		->ContainerPtrToValuePtr<TMap<ETerritoryState, FTerritoryStateConfig>>(Place);
	auto* BlockEntry = NewObject<UTerritorySituationCondition>(Place);
	BlockEntry->Profile = Profile;
	BlockEntry->bNot = true; // This Place needs a retake, so its inverse blocks entry.
	Configs->FindOrAdd(ETerritoryState::Claimed).FactionOverrides.FindOrAdd(Heroes).EntryConditions = {BlockEntry};
	TestFalse(TEXT("An incoming-faction state condition hides the handover offer"), Eligibility->CheckCondition(Pawn, nullptr, Tales));
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("Faction state conditions also block cached handover execution"), Place->GetOwningFaction(), Bandits);
	Configs->Reset();
	TestTrue(TEXT("Removing the blocking story rule restores eligibility"), Eligibility->CheckCondition(Pawn, nullptr, Tales));
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::Ceasefire);
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("A cached offer cannot bypass a new ceasefire"), Place->GetOwningFaction(), Bandits);
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
	AActor* Defender = World->SpawnActor<ATerritoryGuardCharacter>();
	Place->RegisterDefender(Defender);
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("A newly arrived defender blocks the cached handover"), Place->GetOwningFaction(), Bandits);
	Place->UnregisterDefender(Defender);
	Place->SetRole(ROLE_SimulatedProxy);
	AddExpectedError(TEXT("Mutation rejected"), EAutomationExpectedErrorFlags::Contains, 1);
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("A client-role target cannot execute the handover"), Place->GetOwningFaction(), Bandits);
	Place->SetRole(ROLE_Authority);
	Handover->ExecuteEvent(Pawn, nullptr, Tales);
	TestEqual(TEXT("An eligible Native event commits through the capture authority"), Place->GetOwningFaction(), Heroes);
	TestFalse(TEXT("Verified handover closes the retake branch"), Retake->CheckCondition(Pawn, nullptr, Tales));
	Save->LoadActorFromRecord(Place, LostOwnership);
	TestEqual(TEXT("An in-place Native reload replaces the published later owner"), State->GetCaptureSummary(PlaceTag).CurrentOwner, Bandits);
	Registry->UnregisterTerritory(Place);
	TestTrue(TEXT("Retake remains readable while its actor is streamed out"), Retake->CheckCondition(Pawn, nullptr, Tales));
	const auto Streamed = Profile->InspectSituation(Pawn, nullptr, Tales, ETerritorySituationScope::City);
	TestTrue(TEXT("City holdings remain known from the directory"), Streamed.bHoldingsKnown);
	TestFalse(TEXT("Unloaded garrison never reports zero known power"), Streamed.bDefencePowerKnown);
	Candidate = Place->GetOwnershipData();
	Candidate.OwningFaction = Heroes;
	TestTrue(TEXT("Recapture follows the same ownership authority"), Place->CommitOwnershipData(Candidate));
	TestFalse(TEXT("After recapture the retake-needed branch is closed"), Retake->CheckCondition(Pawn, nullptr, Tales));
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRecurrenceSavedBoundary,
	"TerritoryFramework.CounterAttack.SaveLoad.ExhaustedRecurrenceCannotWrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFRecurrenceSavedBoundary::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) return false;
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	FTerritoryAssaultRecord Saved;
	Saved.AssaultID = FGuid(42, 1, 2, 3);
	Saved.ScheduleSeriesID = FGuid(42, 4, 5, 6);
	Saved.TargetTerritoryGUID = FGuid(42, 7, 8, 9);
	Saved.TargetTerritory = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Saved.AttackingFaction = Tag(TEXT("Narrative.Factions.Bandits"));
	Saved.DefendingFaction = Tag(TEXT("Narrative.Factions.Heroes"));
	Saved.State = ETerritoryAssaultState::Defeated;
	Saved.ScheduleOccurrence = MAX_int32;
	Saved.EvaluationCycle = 42;
	Saved.DecisionRoll = 0.375f;
	Counter->RestorePersistentState({Saved});
	FTerritoryAssaultRecord Restored;
	TestTrue(TEXT("Boundary record survives restoration"), Counter->GetAssault(Saved.AssaultID, Restored));
	TestEqual(TEXT("Occurrence is preserved rather than reset into a new series"), Restored.ScheduleOccurrence, MAX_int32);
	TestEqual(TEXT("The saved strategic roll is unchanged"), Restored.DecisionRoll, Saved.DecisionRoll);
	TestFalse(TEXT("An unlimited series cannot overflow its durable occurrence counter"),
		UTerritoryCounterAttackSubsystem::CanContinueSchedule(ETerritoryCounterScheduleMode::UnlimitedSeries, Restored.ScheduleOccurrence, 1));
	TestTrue(TEXT("The last representable next occurrence remains valid"),
		UTerritoryCounterAttackSubsystem::CanContinueSchedule(ETerritoryCounterScheduleMode::UnlimitedSeries, MAX_int32 - 1, 1));
	World->DestroyWorld(false);
	return true;
}

#endif
