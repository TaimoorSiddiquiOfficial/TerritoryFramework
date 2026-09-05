#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Components/BoxComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritorySpatialIndex.h"
#include "Engine/World.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFRegistryAdmissionBoundaries,
	"TerritoryFramework.Registry.Regression.IdempotentAdmissionAndBoundsUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFRegistryAdmissionBoundaries::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Registry world exists"), World)) return false;
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	ATerritoryProperty* Rejected = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(61, 62, 63, 64);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->ApplyToTerritory(Territory);
	Definition->ApplyToTerritory(Rejected);
	Rejected->SetActorLocation(FVector(15000.0));
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>(World);
	int32 Registrations = 0;
	int32 Removals = 0;
	Probe->RegistryCallback = [&](ATerritoryVolume*, bool bRemoved)
	{
		if (bRemoved) ++Removals;
		else ++Registrations;
	};
	Registry->OnTerritoryRegistered.AddDynamic(Probe, &UTerritoryAuditEventProbe::RegistryChanged);
	Registry->OnTerritoryUnregistered.AddDynamic(Probe, &UTerritoryAuditEventProbe::RegistryChanged);
	TestEqual(TEXT("Initial registration succeeds"), Registry->RegisterTerritory(Territory), ETerritoryRegistrationResult::Success);
	TestEqual(TEXT("Repeated registration remains successful"), Registry->RegisterTerritory(Territory), ETerritoryRegistrationResult::Success);
	TestEqual(TEXT("Repeated registration emits one admission event"), Registrations, 1);
	AddExpectedError(TEXT("DUPLICATE TAG"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Duplicate actor is rejected"), Registry->RegisterTerritory(Rejected), ETerritoryRegistrationResult::DuplicateTag);
	Registry->UpdateTerritoryBounds(Rejected);
	TestFalse(TEXT("Bounds update cannot admit a rejected actor into spatial queries"),
		Registry->GetTerritoriesAtLocation(Rejected->GetActorLocation()).Contains(Rejected));
	Registry->UnregisterTerritory(Rejected);
	TestEqual(TEXT("Removing a rejected actor emits no removal event"), Removals, 0);
	TestEqual(TEXT("Rejected removal preserves legitimate identity"), Registry->GetTerritoryByTag(Definition->TerritoryTag), static_cast<ATerritoryVolume*>(Territory));
	Registry->UnregisterTerritory(Territory);
	Registry->UnregisterTerritory(Territory);
	TestEqual(TEXT("Repeated removal emits one removal event"), Removals, 1);
	Registry->OnTerritoryRegistered.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::RegistryChanged);
	Registry->OnTerritoryUnregistered.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::RegistryChanged);
	World->DestroyWorld(false);
	return true;
}

#endif
