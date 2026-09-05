#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryDefinition.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFUIVehicleIdentity,
	"TerritoryFramework.UI.Regression.VehiclePossessionKeepsPlayerIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFUIVehicleIdentity::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("UI identity world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>();
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	FindFProperty<FObjectProperty>(ATerritoryVolume::StaticClass(), TEXT("TerritoryDefinition"))
		->SetObjectPropertyValue_InContainer(Property, Definition);
	Property->ApplyTerritoryDefinition();
	Property->SetActorGUID_Implementation(FGuid(251, 252, 253, 254));
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Property);
	FTerritoryOwnershipData Claimed = Property->GetOwnershipData();
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Claimed.ControlProgress = 1.f;
	Property->CommitOwnershipData(Claimed);
	ANarrativePlayerState* State = World->SpawnActor<ANarrativePlayerState>();
	State->AddFaction(Heroes);
	ANarrativePlayerCharacter* Character = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
	Character->SetRole(ROLE_Authority);
	Character->SetPlayerState(State);
	ANarrativePlayerController* Controller = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	Controller->SetRole(ROLE_Authority);
	Controller->SetPlayerState(State);
	Controller->SetOwnedCharacter(Character);
	World->AddController(Controller);
	Controller->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	APawn* Vehicle = World->SpawnActor<APawn>();
	Controller->SetPawn(Vehicle);
	UNarrativeInventoryComponent* Inventory = Character->GetInventoryComponent();
	Inventory->SetCurrency(1379);
	UTerritoryAuditEconomyWidget* Widget = NewObject<UTerritoryAuditEconomyWidget>(Controller);
	Widget->SetOwningPlayer(Controller);
	Widget->ConstructForAudit();
	TestEqual(TEXT("Widget construction resolves the driving player's faction"), Widget->GetDisplayFaction(), Heroes);
	TestEqual(TEXT("The real widget resolves its owning controller"), Widget->GetOwningPlayer(), static_cast<APlayerController*>(Controller));
	TestEqual(TEXT("Economy widget reads the retained player's wallet while driving"), Widget->GetCurrentGold(), 1379);

	for (bool bClient : {false, true})
	{
		Controller->SetRole(bClient ? ROLE_SimulatedProxy : ROLE_Authority);
		Character->SetRole(bClient ? ROLE_SimulatedProxy : ROLE_Authority);
		const FTerritoryEconomyOperationsView Economy = UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(World, Controller, {}, 0);
		TestEqual(TEXT("Economy view reads the player's faction while driving"), Economy.Faction, Heroes);
		TestEqual(TEXT("Economy view reads the same retained wallet for either actor role"), Economy.AvailableFunds, int64(1379));
		FTerritoryHierarchyOperationsView Hierarchy;
		TestTrue(TEXT("Hierarchy view builds"), UTerritoryUIBlueprintLibrary::BuildHierarchyOperationsView(World, Property, Controller, Hierarchy));
		TestEqual(TEXT("Hierarchy view retains player faction"), Hierarchy.ViewerFaction, Heroes);
		TestTrue(TEXT("Hierarchy view recognizes the player's property"), Hierarchy.bOwnedByViewer);
		FTerritoryGarrisonOperationsView Garrison;
		TestTrue(TEXT("Garrison view builds"), UTerritoryUIBlueprintLibrary::BuildGarrisonOperationsView(World, Property, Controller, Garrison));
		TestTrue(TEXT("Garrison view recognizes the player's property"), Garrison.bOwnedByViewer);
		FTerritoryDistrictOperationsView DistrictView;
		TestTrue(TEXT("District view builds"), UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(World, District, Controller, DistrictView));
		TestEqual(TEXT("District view retains player faction"), DistrictView.ViewerFaction, Heroes);
		TestEqual(TEXT("District view reads the player's wallet"), DistrictView.AvailableFunds, int64(1379));
	}
	Character->SetRole(ROLE_Authority);
	Controller->SetRole(ROLE_Authority);
	Inventory->PrepareForSave_Implementation();
	Inventory->SetCurrency(5);
	Inventory->Load_Implementation();
	TestEqual(TEXT("Native inventory save/load remains visible while driving"), Widget->GetCurrentGold(), 1379);
	Controller->SetPawn(Character);
	TestEqual(TEXT("Exiting the car preserves the displayed wallet"), Widget->GetCurrentGold(), 1379);
	Controller->SetPawn(nullptr);
	TestEqual(TEXT("Possession gaps retain Narrative's owned character wallet"), Widget->GetCurrentGold(), 1379);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	ANarrativePlayerState* SecondState = World->SpawnActor<ANarrativePlayerState>();
	SecondState->AddFaction(Bandits);
	ANarrativePlayerCharacter* SecondCharacter = NewObject<ANarrativePlayerCharacter>(World->PersistentLevel);
	SecondCharacter->SetRole(ROLE_Authority);
	SecondCharacter->SetPlayerState(SecondState);
	SecondCharacter->GetInventoryComponent()->SetCurrency(420);
	ANarrativePlayerController* SecondController = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	SecondController->SetRole(ROLE_Authority);
	SecondController->SetPlayerState(SecondState);
	SecondController->SetOwnedCharacter(SecondCharacter);
	SecondController->SetPawn(Vehicle);
	const FTerritoryEconomyOperationsView SecondView = UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(World, SecondController, {}, 0);
	TestEqual(TEXT("A later viewer resolves its own faction"), SecondView.Faction, Bandits);
	TestEqual(TEXT("A later viewer resolves its own wallet"), SecondView.AvailableFunds, int64(420));
	TestEqual(TEXT("The second viewer does not alter the first widget's wallet"), Widget->GetCurrentGold(), 1379);
	APlayerController* PlainController = World->SpawnActor<APlayerController>();
	PlainController->SetPawn(Vehicle);
	TestEqual(TEXT("Non-Narrative controllers retain their pawn fallback"), FTerritoryNarrativeProAdapter::ResolvePlayerCharacter(PlainController), Vehicle);
	TestNull(TEXT("Missing controller has no player character"), FTerritoryNarrativeProAdapter::ResolvePlayerCharacter(nullptr));
	TestEqual(TEXT("Missing viewer has no account"), UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(World, nullptr, {}, 0).AvailableFunds, int64(0));
	ANarrativePlayerController* JoiningController = NewObject<ANarrativePlayerController>(World->PersistentLevel);
	JoiningController->SetRole(ROLE_Authority);
	JoiningController->SetPlayerState(SecondState);
	World->AddController(JoiningController);
	JoiningController->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	UTerritoryAuditEconomyWidget* JoiningWidget = NewObject<UTerritoryAuditEconomyWidget>(JoiningController);
	JoiningWidget->SetOwningPlayer(JoiningController);
	JoiningWidget->ConstructForAudit();
	TestEqual(TEXT("A joining player can resolve faction before its character arrives"), JoiningWidget->GetDisplayFaction(), Bandits);
	JoiningWidget->DestructForAudit();
	Widget->DestructForAudit();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
