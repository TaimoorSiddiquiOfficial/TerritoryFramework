#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFManagementInteractionRouting,
	"TerritoryFramework.Interaction.Regression.ManagementClientRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFManagementInteractionRouting::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Management world exists"), World)) return false;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	ATerritoryGuardCharacter* Pawn = World->SpawnActor<ATerritoryGuardCharacter>();
	Controller->SetPawn(Pawn);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Cast<INarrativeTeamAgentInterface>(Pawn)->AddFaction(Heroes);
	UTerritoryPlayerManagementComponent* Bridge =
		UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(Controller);
	ATerritoryDistrictManagementPoint* Point = World->SpawnActor<ATerritoryDistrictManagementPoint>();
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>();
	UTerritoryDistrictDefinition* Definition = NewObject<UTerritoryDistrictDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare"));
	Definition->StableTerritoryGUID = FGuid(221, 222, 223, 224);
	Definition->ManagementPoint.bEnabled = true;
	Definition->ApplyToTerritory(District);
	Point->SetTerritoryDefinition(Definition);
	Point->ApplyTerritoryDefinition();
	UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>();
	Registry->RegisterTerritory(District);
	FTerritoryOwnershipData State = District->GetOwnershipData();
	auto ApplyState = [&]()
	{
		// Seed the existing hierarchy authority, not the physical capture path.
		District->SetDerivedControl(State.OwningFaction, State.State);
		FTerritoryOwnershipData Candidate = District->GetOwnershipData();
		Candidate.Availability = State.Availability;
		District->CommitOwnershipData(Candidate);
	};
	State.State = ETerritoryState::Claimed;
	State.OwningFaction = Heroes;
	State.Availability = ETerritoryAvailability::Unlocked;
	State.ControlProgress = 1.f;
	ApplyState();

	TestTrue(TEXT("Owning connection has a replicated bridge"), Bridge && Bridge->GetIsReplicated());
	if (!Bridge) { World->DestroyWorld(false); return false; }
	TestTrue(TEXT("Server accepts a nearby owning player"), Bridge->SendOpenManagementPoint(Point));
	TestFalse(TEXT("Missing point is rejected"), Bridge->SendOpenManagementPoint(nullptr));
	Controller->SetRole(ROLE_AutonomousProxy);
	TestFalse(TEXT("Client cannot send an approved server interaction"), Bridge->SendOpenManagementPoint(Point));
	Controller->SetRole(ROLE_Authority);
	Pawn->SetActorLocation(Point->GetActorLocation() + FVector(10000.f, 0.f, 0.f));
	TestFalse(TEXT("Out-of-range player is rejected"), Bridge->SendOpenManagementPoint(Point));
	Pawn->SetActorLocation(Point->GetActorLocation());
	State.OwningFaction = Bandits;
	ApplyState();
	TestFalse(TEXT("Another faction cannot open management"), Bridge->SendOpenManagementPoint(Point));
	State.OwningFaction = Heroes;
	State.Availability = ETerritoryAvailability::Locked;
	ApplyState();
	TestFalse(TEXT("Story-locked district is rejected"), Bridge->SendOpenManagementPoint(Point));
	State.Availability = ETerritoryAvailability::Unlocked;
	State.State = ETerritoryState::Contested;
	ApplyState();
	TestFalse(TEXT("Unclaimed district is rejected"), Bridge->SendOpenManagementPoint(Point));
	State.State = ETerritoryState::Claimed;
	ApplyState();

	// The menu owns no durable state. Narrative restores the existing District
	// authority, and the next interaction must use that restored state.
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative saves management eligibility"), Save->CreateActorRecord(District, Record));
	State.OwningFaction = Bandits;
	ApplyState();
	Save->LoadActorFromRecord(District, Record);
	TestTrue(TEXT("Reloaded ownership permits management"), Bridge->SendOpenManagementPoint(Point));
	Registry->UnregisterTerritory(District);
	TestFalse(TEXT("Streamed-out District cannot open a stale menu"), Bridge->SendOpenManagementPoint(Point));
	Registry->RegisterTerritory(District);
	TestTrue(TEXT("Registering the District again restores access"), Bridge->SendOpenManagementPoint(Point));
	UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
	ATerritoryDistrictManagementPoint* OtherPoint = OtherWorld->SpawnActor<ATerritoryDistrictManagementPoint>();
	TestFalse(TEXT("Point from another world is rejected"), Bridge->SendOpenManagementPoint(OtherPoint));
	OtherWorld->DestroyWorld(false);

	const UFunction* RPC = Bridge->FindFunction(TEXT("ClientOpenManagementPoint"));
	TestTrue(TEXT("UI delivery uses a reliable owning-client RPC"), RPC
		&& RPC->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable));
	const UFunction* Request = Bridge->FindFunction(TEXT("SendOpenManagementPoint"));
	TestTrue(TEXT("Blueprint request is authority-only"), Request
		&& Request->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly));
	World->DestroyWorld(false);
	return true;
}

#endif
