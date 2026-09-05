#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "UObject/UnrealType.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSpawnCallbacks,
	"TerritoryFramework.CounterAttack.Regression.NarrativeSpawnRestoreCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSpawnCallbacks::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Narrative spawn callback world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	UNarrativeCharacterSubsystem* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* TerritoryDefinition = NewObject<UTerritoryPlaceDefinition>();
	TerritoryDefinition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	TerritoryDefinition->StableTerritoryGUID = FGuid(201, 202, 203, 204);
	TerritoryDefinition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	UTerritoryCounterAttackProfile* Profile = NewObject<UTerritoryCounterAttackProfile>();
	TerritoryDefinition->CounterAttackProfile = Profile;
	TerritoryDefinition->ApplyToTerritory(Territory);
	UNPCDefinition* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("TerritoryAuditSpawnCharacter");
	NPC->NPCID = TEXT("TerritoryAuditSpawnNPC");
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NPC;
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(205, 206, 207, 208);
	Record.TargetTerritoryGUID = Territory->GetTerritoryGUID();
	Record.TargetTerritory = Territory->GetTerritoryTag();
	Record.AttackingFaction = Force.Faction;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 1;
	Record.PendingReserveForce = 1;
	FTerritoryAssaultApproach Approach;
	Approach.ApproachID = TEXT("AuditFoot");
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	FMulticastDelegateProperty* SpawnEvent = FindFProperty<FMulticastDelegateProperty>(
		Characters->GetClass(), TEXT("OnNPCSpawned"));
	if (!TestNotNull(TEXT("Narrative exposes its Blueprint-assignable spawn callback"), SpawnEvent))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	FScriptDelegate SpawnDelegate;
	SpawnDelegate.BindUFunction(Probe, GET_FUNCTION_NAME_CHECKED(UTerritoryAuditEventProbe, NPCSpawned));
	SpawnEvent->AddDelegate(SpawnDelegate, Characters);
	for (bool bKeepSameIdentity : {false, true})
	{
		Counter->RestorePersistentState({Record});
		ATerritoryAssaultCharacter* Created = nullptr;
		Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter* Spawned)
		{
			Created = Cast<ATerritoryAssaultCharacter>(Spawned);
			if (bKeepSameIdentity)
			{
				FTerritoryAssaultRecord Replacement = Record;
				Replacement.State = ETerritoryAssaultState::Grace;
				Counter->RestorePersistentState({Replacement});
			}
			else
			{
				Counter->RestorePersistentState({});
			}
		};
		ATerritoryAssaultCharacter* Returned = Counter->SpawnParticipant(
			Counter->Assaults.FindChecked(Record.AssaultID), Territory, Force, NPC,
			Approach, FTransform(FVector(1000.f, bKeepSameIdentity ? 1000.f : 0.f, 100.f)), INDEX_NONE);
		TestNotNull(TEXT("The actual Narrative spawn API reached its public callback"), Created);
		TestNull(TEXT("A superseded spawn cannot return an admitted attacker"), Returned);
		TestTrue(TEXT("The unadmitted physical NPC is removed"), !IsValid(Created) || Created->IsActorBeingDestroyed());
		TestTrue(TEXT("No stale live attacker set is inserted into the restored campaign"), Counter->LiveParticipants.IsEmpty());
		TestEqual(TEXT("Restore keeps its exact authoritative assault set"), Counter->Assaults.Num(), bKeepSameIdentity ? 1 : 0);
		if (bKeepSameIdentity)
		{
			TestEqual(TEXT("Same-ID restore keeps its replacement phase"), Counter->Assaults.FindChecked(Record.AssaultID).State, ETerritoryAssaultState::Grace);
		}
	}
	Probe->NPCSpawnCallback = nullptr;
	Counter->RestorePersistentState({Record});
	Territory->SetRole(ROLE_SimulatedProxy);
	TestNull(TEXT("A non-authoritative target cannot spawn a physical attacker"), Counter->SpawnParticipant(
		Counter->Assaults.FindChecked(Record.AssaultID), Territory, Force, NPC,
		Approach, FTransform(FVector(2000.f, 2000.f, 100.f)), INDEX_NONE));
	TestTrue(TEXT("Authority rejection leaves no live participants"), Counter->LiveParticipants.IsEmpty());
	Territory->SetRole(ROLE_Authority);
	ATerritoryAssaultCharacter* ValidAttacker = Counter->SpawnParticipant(
		Counter->Assaults.FindChecked(Record.AssaultID), Territory, Force, NPC,
		Approach, FTransform(FVector(2000.f, 2000.f, 100.f)), INDEX_NONE);
	TestNotNull(TEXT("A subsequent valid Narrative spawn can still be admitted"), ValidAttacker);
	TestEqual(TEXT("The valid spawn is tracked once"), Counter->LiveParticipants.FindRef(Record.AssaultID).Num(), 1);
	Counter->RestorePersistentState({Record});
	ANarrativeVehicleBase* CreatedVehicle = nullptr;
	const FDelegateHandle VehicleSpawnCallback = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateLambda([&](AActor* Spawned)
		{
			if (ANarrativeVehicleBase* Vehicle = Cast<ANarrativeVehicleBase>(Spawned))
			{
				CreatedVehicle = Vehicle;
				Counter->RestorePersistentState({});
			}
		}));
	Approach.VehicleClass = TSoftClassPtr<ANarrativeVehicleBase>(FSoftObjectPath(
		TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C")));
	const TArray<ATerritoryAssaultCharacter*> VehicleOccupants = Counter->SpawnNarrativeVehicleParticipants(
		Counter->Assaults.FindChecked(Record.AssaultID), Territory, Force, NPC, Approach,
		FTransform(FVector(3000.f, 3000.f, 100.f)), FTransform::Identity, FTransform::Identity,
		FVector::ZeroVector, 1, INDEX_NONE);
	World->RemoveOnActorSpawnedHandler(VehicleSpawnCallback);
	TestNotNull(TEXT("The real Narrative vehicle actor was created"), CreatedVehicle);
	TestTrue(TEXT("A superseded vehicle deployment creates no occupants"), VehicleOccupants.IsEmpty());
	TestTrue(TEXT("An unadmitted vehicle is removed after restore"), !IsValid(CreatedVehicle) || CreatedVehicle->IsActorBeingDestroyed());
	TestTrue(TEXT("Vehicle spawn restore keeps the assault map empty"), Counter->Assaults.IsEmpty());
	TestTrue(TEXT("Vehicle spawn restore keeps the live vehicle map empty"), Counter->LiveAssaultVehicles.IsEmpty());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
