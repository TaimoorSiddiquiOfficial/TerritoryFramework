#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryWorldState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDirectoryRetirement,
	"TerritoryFramework.WorldPartition.Regression.ExplicitDirectoryRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDirectoryRetirement::RunTest(const FString& Parameters)
{
	auto* State = NewObject<ATerritoryWorldState>();
	const auto Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const auto Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	const auto OldTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	const auto LiveTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.CastleHill"));
	FReplicatedCaptureSummary Removed;
	Removed.TerritoryGUID = FGuid(741, 1, 1, 1); Removed.TerritoryTag = OldTag;
	Removed.HierarchyLevel = ETerritoryHierarchyLevel::District;
	Removed.State = ETerritoryState::Claimed; Removed.CurrentOwner = Heroes;
	FReplicatedCaptureSummary Unloaded = Removed;
	Unloaded.TerritoryGUID = FGuid(741, 2, 2, 2); Unloaded.TerritoryTag = LiveTag;
	State->SetCaptureSummary(Removed); State->SetCaptureSummary(Unloaded);
	State->ExportPersistentState();
	TestEqual(TEXT("Unloaded rows initially retain both districts"), State->GetClaimedDistrictCountForFaction(Heroes), 2);
	State->RetiredDirectoryGUIDs.Add(Removed.TerritoryGUID);
	State->ImportPersistentState();
	TestEqual(TEXT("Explicit retirement removes ghost staging eligibility"), State->GetClaimedDistrictCountForFaction(Heroes), 1);
	TestEqual(TEXT("Unloaded current territory survives"), State->GetCaptureSummary(LiveTag).TerritoryGUID, Unloaded.TerritoryGUID);
	TestFalse(TEXT("Retired identity is absent"), State->GetCaptureSummary(OldTag).TerritoryGUID.IsValid());
	State->SetCaptureSummary(Removed);
	TestFalse(TEXT("A stale publisher cannot resurrect the tombstone"), State->GetCaptureSummary(OldTag).TerritoryGUID.IsValid());
	Removed.State = ETerritoryState::Contested; Removed.ContestingFaction = Bandits;
	State->SavedStrategicDirectory = {Removed, Unloaded};
	State->ImportPersistentState();
	TestFalse(TEXT("Retired conflict cannot keep factions in conflict"), State->HasContestedTerritoryBetweenFactions(Heroes, Bandits, {}));
	FReplicatedCaptureSummary ReusedTag = Removed;
	ReusedTag.TerritoryGUID = FGuid(741, 3, 3, 3);
	State->SetCaptureSummary(ReusedTag);
	TestEqual(TEXT("New GUID reusing the old tag is retained"), State->GetCaptureSummary(OldTag).TerritoryGUID, ReusedTag.TerritoryGUID);
	FReplicatedCaptureSummary Renamed = Unloaded;
	Renamed.TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.CastleHill.Farm"));
	State->SetCaptureSummary(Renamed);
	TestFalse(TEXT("Renaming the same stable identity removes the old label"), State->GetCaptureSummary(LiveTag).TerritoryGUID.IsValid());
	TestEqual(TEXT("Renaming preserves political state"), State->GetCaptureSummary(Renamed.TerritoryTag).CurrentOwner, Heroes);
	State->ExportPersistentState();
	TestFalse(TEXT("A new save no longer contains the retired row"), State->SavedStrategicDirectory.ContainsByPredicate(
		[&](const auto& Row) { return Row.TerritoryGUID == Removed.TerritoryGUID; }));
	State->ImportPersistentState();
	TestEqual(TEXT("A repeated load is idempotent"), State->GetAllCaptureSummaries().Num(), 2);
	State->SetRole(ROLE_SimulatedProxy);
	State->RetiredDirectoryGUIDs.Add(Renamed.TerritoryGUID);
	State->RefreshStrategicDirectory();
	TestEqual(TEXT("Client cannot remove an authoritative directory row"), State->GetCaptureSummary(Renamed.TerritoryTag).TerritoryGUID, Renamed.TerritoryGUID);
	State->SetRole(ROLE_Authority);
	auto* City = NewObject<UTerritoryCityDefinition>();
	City->StableTerritoryGUID = Removed.TerritoryGUID;
	State->CampaignCities = {City};
	FDataValidationContext Validation;
	TestEqual(TEXT("Retired GUID still used by authored content fails validation"), State->IsDataValid(Validation), EDataValidationResult::Invalid);
	return true;
}
#endif
