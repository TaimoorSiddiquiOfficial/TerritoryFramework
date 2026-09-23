#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUnloadedHierarchyReconciliation,
	"TerritoryFramework.WorldPartition.Regression.UnloadedAncestorsFollowChildSnapshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUnloadedHierarchyReconciliation::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Hierarchy world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	const auto Tag = [](const TCHAR* Value) { return FGameplayTag::RequestGameplayTag(Value); };
	const FGameplayTag Heroes = Tag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = Tag(TEXT("Narrative.Factions.Bandits"));
	auto* City = NewObject<UTerritoryCityDefinition>();
	auto* District = NewObject<UTerritoryDistrictDefinition>();
	auto* First = NewObject<UTerritoryPlaceDefinition>();
	auto* Second = NewObject<UTerritoryPlaceDefinition>();
	City->TerritoryTag = Tag(TEXT("Territory.HavenReach"));
	District->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare"));
	First->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Second->TerritoryTag = Tag(TEXT("Territory.HavenReach.MarketSquare.Warehouse"));
	for (UTerritoryDefinition* Definition : TArray<UTerritoryDefinition*>{City, District, First, Second})
	{
		Definition->StableTerritoryGUID = FGuid::NewGuid();
		Definition->InitialState = ETerritoryInitialState::Unclaimed;
		Definition->InitialAvailability = ETerritoryAvailability::Unlocked;
		Definition->InitialGuardCount = 0;
	}
	District->Places = {First, Second};
	City->Districts = {District};
	City->RefreshHierarchyLinks();
	auto* State = World->SpawnActor<ATerritoryWorldState>();
	State->CampaignCities = {City};
	State->RefreshStrategicDirectory();
	auto Publish = [&](UTerritoryPlaceDefinition* Definition, FGameplayTag Owner,
		ETerritoryAvailability Availability = ETerritoryAvailability::Unlocked)
	{
		FReplicatedCaptureSummary Row = State->GetCaptureSummary(Definition->TerritoryTag);
		Row.CurrentOwner = Owner;
		Row.State = Owner.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
		Row.Availability = Availability;
		State->SetCaptureSummary(Row);
	};
	Publish(First, Heroes);
	TestEqual(TEXT("One child cannot secure an absent District"), State->GetCaptureSummary(District->TerritoryTag).State, ETerritoryState::Contested);
	Publish(Second, Heroes);
	TestEqual(TEXT("Never-loaded District follows complete ownership"), State->GetCaptureSummary(District->TerritoryTag).CurrentOwner, Heroes);
	TestEqual(TEXT("Never-loaded City follows its reduced District"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	TestEqual(TEXT("Strategic staging sees the new secure District"), State->GetClaimedDistrictCountForFaction(Heroes), 1);
	State->ExportPersistentState();
	Publish(Second, Bandits);
	TestFalse(TEXT("Mixed ownership clears the City owner"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner.IsValid());
	TestEqual(TEXT("Mixed ownership removes staging eligibility"), State->GetClaimedDistrictCountForFaction(Heroes), 0);
	State->ImportPersistentState();
	TestEqual(TEXT("Save/load rebuilds unloaded ancestors from saved children"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	Publish(Second, Heroes, ETerritoryAvailability::Locked);
	TestEqual(TEXT("Locked child cannot secure the District"), State->GetCaptureSummary(District->TerritoryTag).State, ETerritoryState::Contested);
	Publish(Second, Heroes);
	State->ReplicatedCaptureSummaries.RemoveAll([Second](const auto& Row) { return Row.TerritoryTag == Second->TerritoryTag; });
	Publish(First, Heroes);
	TestFalse(TEXT("Missing child snapshot fails closed"), State->GetCaptureSummary(District->TerritoryTag).CurrentOwner.IsValid());
	State->RegisterDefinitionHierarchy(City);
	Publish(Second, Heroes);
	District->Places.Add(First);
	State->RegisterDefinitionHierarchy(City);
	TestFalse(TEXT("Duplicate authored slot cannot secure parent"), State->GetCaptureSummary(District->TerritoryTag).CurrentOwner.IsValid());
	District->Places.Pop();
	State->RegisterDefinitionHierarchy(City);
	TestEqual(TEXT("Corrected topology recovers"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	FReplicatedCaptureSummary WrongIdentity = State->GetCaptureSummary(Second->TerritoryTag);
	WrongIdentity.TerritoryGUID = FGuid::NewGuid();
	State->SetCaptureSummary(WrongIdentity);
	TestFalse(TEXT("Reused tag with wrong GUID cannot secure parent"), State->GetCaptureSummary(District->TerritoryTag).CurrentOwner.IsValid());
	State->RegisterDefinitionHierarchy(City);
	Publish(Second, Heroes);
	State->SetRole(ROLE_SimulatedProxy);
	Publish(Second, Bandits);
	TestEqual(TEXT("Client cannot mutate snapshot ownership"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	State->SetRole(ROLE_Authority);

	auto* LoadedDistrict = World->SpawnActor<ATerritoryDistrict>();
	District->ApplyToTerritory(LoadedDistrict);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(LoadedDistrict);
	State->PublishTerritorySummary(LoadedDistrict);
	Publish(First, Bandits);
	TestEqual(TEXT("Loaded District reduces its absent children through Volume"), LoadedDistrict->GetTerritoryState(), ETerritoryState::Contested);
	Publish(First, Heroes);
	TestEqual(TEXT("Loaded District secures when absent children agree"), LoadedDistrict->GetOwningFaction(), Heroes);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->UnregisterTerritory(LoadedDistrict);
	LoadedDistrict->Destroy();
	auto* LoadedCity = World->SpawnActor<ATerritoryCity>();
	City->ApplyToTerritory(LoadedCity);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(LoadedCity);
	auto* LoadedPlace = World->SpawnActor<ATerritoryProperty>();
	First->ApplyToTerritory(LoadedPlace);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(LoadedPlace);
	LoadedPlace->ForceSetOwningFaction(Heroes);
	TestEqual(TEXT("Loaded City reads the absent District after a loaded Place captures"), LoadedCity->GetOwningFaction(), Heroes);
	LoadedPlace->ForceSetOwningFaction(Bandits);
	TestFalse(TEXT("Loaded City loses control through absent District immediately"), LoadedCity->GetOwningFaction().IsValid());
	TestEqual(TEXT("Mixed-lifetime City snapshot matches its actor"), State->GetCaptureSummary(City->TerritoryTag).State, LoadedCity->GetTerritoryState());
	State->ExportPersistentState();
	LoadedPlace->ForceSetOwningFaction(Heroes);
	TestEqual(TEXT("Control recovers without reloading the City"), LoadedCity->GetOwningFaction(), Heroes);
	State->ImportPersistentState();
	TestEqual(TEXT("Directory import does not overwrite loaded actor ownership"), LoadedCity->GetOwningFaction(), Heroes);
	State->PublishTerritorySummary(LoadedPlace);
	TestEqual(TEXT("Restored actor publication reconciles all ancestor snapshots"), State->GetCaptureSummary(City->TerritoryTag).CurrentOwner, Heroes);
	return true;
}
#endif
