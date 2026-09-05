#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardReserveTotals,
	"TerritoryFramework.Guards.Regression.ReserveTotalsRemainMonotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardReserveTotals::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Reserve total world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	UTerritoryDistrictDefinition* DistrictDefinition = NewObject<UTerritoryDistrictDefinition>();
	DistrictDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	for (const TCHAR* Tag : { TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), TEXT("Territory.HavenReach.CastleHill.Farm") })
	{
		UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
		Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(Tag);
		DistrictDefinition->Places.Add(Definition);
	}
	DistrictDefinition->RefreshHierarchyLinks();
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>();
	District->TerritoryDefinition = DistrictDefinition;
	District->ApplyTerritoryDefinition();
	District->SetActorGUID_Implementation(FGuid(301, 302, 303, 304));
	District->OwnershipData.OwningFaction = Heroes;
	District->OwnershipData.State = ETerritoryState::Claimed;
	Registry->RegisterTerritory(District);
	TArray<ATerritoryProperty*> Places;
	TArray<ATerritoryGuardSpawnPoint*> LoadedPosts;
	TArray<FNarrativeActorRecord> PostRecords;
	for (int32 PlaceIndex = 0; PlaceIndex < 2; ++PlaceIndex)
	{
		ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
		Place->TerritoryDefinition = DistrictDefinition->Places[PlaceIndex];
		Place->ApplyTerritoryDefinition();
		Place->SetActorGUID_Implementation(FGuid(305, PlaceIndex + 1, 307, 308));
		Place->OwnershipData.OwningFaction = Heroes;
		Place->OwnershipData.State = ETerritoryState::Claimed;
		Place->OwnershipData.DesiredGuardCount = 2;
		Registry->RegisterTerritory(Place);
		Places.Add(Place);
		for (int32 PostIndex = 0; PostIndex < 2; ++PostIndex)
		{
			ATerritoryGuardSpawnPoint* Post = World->SpawnActor<ATerritoryGuardSpawnPoint>();
			Post->SetActorGUID_Implementation(FGuid(309, PlaceIndex + 1, PostIndex + 1, 312));
			Post->ReserveSlots = MAX_int32;
			Post->InitializeReserves();
			Post->SetResolvedTerritory(Place);
			if (PostIndex == 1)
			{
				FNarrativeActorRecord Record;
				TestTrue(TEXT("Native save captures a valid large per-post reserve"), Save->CreateActorRecord(Post, Record));
				PostRecords.Add(Record);
				LoadedPosts.Add(Post);
				Post->CurrentReserveCount = 0;
			}
		}
		Place->RefreshGarrisonSnapshot();
	}
	TestEqual(TEXT("Authored district resolves both loaded Places"), District->GetProperties().Num(), 2);
	FTerritoryFactionAssaultConfig Force;
	Force.MilitaryPower = 100.f;
	const FTerritoryAssaultEvaluationInput Before = Counter->BuildEvaluationInput(Places[0], Force);
	TestEqual(TEXT("One stocked post per Place supports the existing desired staffing"), Before.ReserveGuards, 4);
	for (int32 Index = 0; Index < LoadedPosts.Num(); ++Index)
	{
		Save->LoadActorFromRecord(LoadedPosts[Index], PostRecords[Index]);
		TestEqual(TEXT("Native load preserves the exact per-post count"), LoadedPosts[Index]->GetReserveCount(), MAX_int32);
		Places[Index]->RefreshGarrisonSnapshot();
		TestEqual(TEXT("Replicated total saturates instead of wrapping negative"),
			Places[Index]->GetGarrisonSnapshot().ReserveGuards, MAX_int32);
	}
	const FTerritoryAssaultEvaluationInput After = Counter->BuildEvaluationInput(Places[0], Force);
	TestEqual(TEXT("More saved reserves cannot erase effective defence"), After.ReserveGuards, Before.ReserveGuards);
	UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	const FTerritoryAssaultEvaluationResult BeforeResult = Counter->CalculateEvaluation(Before, Profile);
	const FTerritoryAssaultEvaluationResult AfterResult = Counter->CalculateEvaluation(After, Profile);
	TestTrue(TEXT("More reserves cannot increase attack priority"), AfterResult.AttackPriority <= BeforeResult.AttackPriority);
	TestTrue(TEXT("More reserves cannot increase launch probability"), AfterResult.LaunchProbability <= BeforeResult.LaunchProbability);
	FTerritoryDistrictOperationsView View;
	TestTrue(TEXT("District operations view builds from loaded authoritative Places"),
		UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(World, District, nullptr, View));
	TestEqual(TEXT("District UI includes both Place garrisons"), View.GarrisonTargets.Num(), 2);
	TestEqual(TEXT("District UI reserve total never wraps"), View.ReserveGuards, MAX_int32);
	Places[0]->SetRole(ROLE_SimulatedProxy);
	const FTerritoryGarrisonSnapshot ServerSnapshot = Places[0]->GetGarrisonSnapshot();
	LoadedPosts[0]->CurrentReserveCount = 0;
	Places[0]->RefreshGarrisonSnapshot();
	TestTrue(TEXT("Client refresh cannot overwrite its server snapshot"), Places[0]->GetGarrisonSnapshot() == ServerSnapshot);
	Places[0]->SetRole(ROLE_Authority);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
