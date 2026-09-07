#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/AbilityConfiguration.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameplayEffect.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "SaveSystemStatics.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/UnrealType.h"
#include "Vehicles/NarrativeVehicleBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSurvivorRestore,
	"TerritoryFramework.CounterAttack.SaveLoad.PhysicalSurvivorIdentityAndVehicleBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSurvivorRestore::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Physical restore world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->CreateAISystem();
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	World->GetSubsystem<UNarrativeSaveSubsystem>()->UpdateSaveObject(true);
	auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	auto* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	auto* Territory = World->SpawnActor<ATerritoryProperty>();
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(361, 362, 363, 364);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	auto* Profile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->CounterAttackProfile = Profile;
	FTerritoryAssaultApproach Approach;
	Approach.ApproachID = TEXT("RestoreRoad");
	Approach.EntryType = ETerritoryAssaultEntryType::NarrativeVehicle;
	Approach.VehicleClass = TSoftClassPtr<ANarrativeVehicleBase>(FSoftObjectPath(TEXT("/NarrativePro/Pro/Core/BP/Vehicles/Demo/vehicle03_Car/BPV_Sedan.BPV_Sedan_C")));
	Definition->CounterAttackApproaches.Add(Approach);
	Definition->ApplyToTerritory(Territory);
	auto* NPCDefinition = NewObject<UNPCDefinition>();
	NPCDefinition->CharacterID = TEXT("TerritoryRestoreCharacter");
	NPCDefinition->NPCID = TEXT("TerritoryRestoreNPC");
	NPCDefinition->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPCDefinition->bAllowMultipleInstances = true;
	NPCDefinition->AbilityConfiguration = NewObject<UAbilityConfiguration>();
	NPCDefinition->AbilityConfiguration->DefaultAttributes = UGameplayEffect::StaticClass();
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NPCDefinition;
	Profile->FactionForces.Add(Force);
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(365, 366, 367, 368);
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = Force.Faction;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = Record.PendingReserveForce = 8;
	Record.WaveSize = 4;
	Record.WaveStrategy = ETerritoryAssaultWaveStrategy::AfterDefeated;
	Record.MaximumVehicleDeployments = 2;
	Record.VehicleDeploymentsUsed = 1;
	Record.VehicleDeploymentsByApproach.Add({Approach.ApproachID, 1});
	Record.PhysicalStateVersion = 1;
	Record.SelectedApproaches.Add(Approach.ApproachID);
	FTerritoryAssaultVehicleCheckpoint Car;
	Car.VehicleID = FGuid(369, 370, 371, 372);
	Car.ApproachID = Approach.ApproachID;
	Car.VehicleClass = Approach.VehicleClass;
	Car.Transform = FTransform(FVector(4000.f, 4000.f, 100.f));
	Car.RoutePoints = {FVector(4000,4000,0), FVector(8000,4000,0)};
	Car.ParkDestination = FTransform(FVector(8000,4000,100));
	Record.VehicleCheckpoints.Add(Car);
	for (int32 Seat = 0; Seat < 4; ++Seat)
	{
		FTerritoryAssaultSurvivor Member;
		Member.SpawnGUID = FGuid(380 + Seat, 381, 382, 383);
		Member.ApproachID = Approach.ApproachID;
		Member.VehicleID = Car.VehicleID;
		Member.SeatIndex = Seat;
		Member.Transform = Car.Transform;
		Record.PendingSurvivors.Add(Member);
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);
	FTerritoryAssaultRecord StreamedOut = Record;
	StreamedOut.TargetTerritoryGUID = FGuid(395,396,397,398);
	Counter->RestorePersistentState({StreamedOut});
	Counter->AdvanceAssault(Counter->Assaults.FindChecked(Record.AssaultID));
	TestTrue(TEXT("An unloaded target spawns no physical participants"), Counter->LiveParticipants.IsEmpty());
	TestEqual(TEXT("A same-tag actor with the wrong GUID cannot consume the reconstruction roster"), Counter->Assaults.FindChecked(Record.AssaultID).PendingSurvivors.Num(), 4);
	Counter->RestorePersistentState({Record});
	Territory->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("A client actor cannot reconstruct saved force"), Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Territory).IsEmpty());
	Territory->SetRole(ROLE_Authority);
	auto* Probe = NewObject<UTerritoryAuditEventProbe>();
	auto* SpawnEvent = FindFProperty<FMulticastDelegateProperty>(Characters->GetClass(), TEXT("OnNPCSpawned"));
	FScriptDelegate SpawnDelegate;
	SpawnDelegate.BindUFunction(Probe, GET_FUNCTION_NAME_CHECKED(UTerritoryAuditEventProbe, NPCSpawned));
	SpawnEvent->AddDelegate(SpawnDelegate, Characters);
	Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter*)
	{
		const auto DuringSpawn = Counter->GetPersistentState()[0];
		TestEqual(TEXT("Every in-spawn checkpoint accounts for all finite force"), DuringSpawn.GetAccountedForce(), 8);
		TestEqual(TEXT("Saving during construction includes every committed seat"), DuringSpawn.PendingSurvivors.Num(), 4);
		TestEqual(TEXT("Saving during construction retains the already charged car"), DuringSpawn.VehicleDeploymentsUsed, 1);
	};
	auto First = Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Territory);
	Probe->NPCSpawnCallback = nullptr;
	TestEqual(TEXT("Four actual Narrative NPCs reconstruct in one actual sedan"), First.Num(), 4);
	if (First.Num() == 4)
	{
		TestEqual(TEXT("The fresh second wave remains unused"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 4);
		First[0]->AssaultParticipant->Retire(true);
		TSet<FGuid> SurvivingIDs;
		for (int32 Index = 1; Index < First.Num(); ++Index) SurvivingIDs.Add(First[Index]->GetActorGUID_Implementation());
		const FGuid NativeSavedGUID = First[1]->GetActorGUID_Implementation();
		auto* SurvivorASC = First[1]->GetNarrativeAbilitySystemComponent();
		SurvivorASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
		SurvivorASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 37.f);
		if (auto* Vehicle = Counter->PhysicalVehicles.FindRef(Car.VehicleID).Get())
			Vehicle->GetAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), Vehicle->GetMaxHealth() * 0.6f);
		TestTrue(TEXT("Narrative accepts the survivor's original save key"), USaveSystemStatics::SaveSingleActor(First[1]));
		const FTransform NativeSavedTransform = First[1]->GetActorTransform();
		auto Saved = Counter->GetPersistentState();
		TestEqual(TEXT("Only living members enter the durable roster"), Saved[0].PendingSurvivors.Num(), 3);
		TestEqual(TEXT("Killed driver is permanently consumed"), Saved[0].KilledForce, 1);
		TestEqual(TEXT("Only its one charged car enters the roster"), Saved[0].VehicleCheckpoints.Num(), 1);
		TestTrue(TEXT("The car checkpoint preserves damage"), FMath::IsNearlyEqual(Saved[0].VehicleCheckpoints[0].HealthFraction, 0.6f));
		// Round-trip the actual SaveGame struct, including nested non-replicated values.
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
		SaveArchive.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(SaveArchive, &Saved[0], nullptr);
		FTerritoryAssaultRecord Loaded;
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
		LoadArchive.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(LoadArchive, &Loaded, nullptr);
		Counter->RestorePersistentState({Loaded});
		TestTrue(TEXT("Immediate reconstruction waits for old GUID lookup retirement"), Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Territory).IsEmpty());
		const auto Waiting = Counter->GetPersistentState();
		Counter->RestorePersistentState(Waiting);
		TestEqual(TEXT("Repeated load before physical readiness retains three survivors"), Counter->Assaults.FindChecked(Record.AssaultID).PendingSurvivors.Num(), 3);
		for (auto* NPC : First) if (IsValid(NPC)) NPC->Destroy();
		if (auto* OldCar = Counter->PhysicalVehicles.FindRef(Car.VehicleID).Get()) OldCar->Destroy();
		auto Restored = Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Territory);
		TestEqual(TEXT("Three survivors return even with AfterDefeated wave policy"), Restored.Num(), 3);
		for (auto* NPC : Restored) TestTrue(TEXT("A restored participant keeps its original Native save GUID"), SurvivingIDs.Contains(NPC->GetActorGUID_Implementation()));
		TestEqual(TEXT("Reconstruction does not consume the second deployment"), Counter->Assaults.FindChecked(Record.AssaultID).VehicleDeploymentsUsed, 1);
		TestEqual(TEXT("Reconstruction does not consume the four-person reserve wave"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 4);
		TestEqual(TEXT("Repeated load creates one replacement physical car"), Counter->LiveAssaultVehicles.FindRef(Record.AssaultID).Num(), 1);
		if (auto* Vehicle = Counter->PhysicalVehicles.FindRef(Car.VehicleID).Get())
			TestTrue(TEXT("Reconstructing a damaged car does not heal it"), FMath::IsNearlyEqual(Vehicle->GetHealth() / Vehicle->GetMaxHealth(), 0.6f));
		int32 Drivers = 0;
		for (auto* NPC : Restored)
		{
			if (NPC->AssaultParticipant->GetIngressSeatIndex() == 0) ++Drivers;
			if (NPC->GetActorGUID_Implementation() == NativeSavedGUID)
			{
				NPC->SetActorLocation(FVector(20000, 20000, 100));
				TestTrue(TEXT("Native loads the saved survivor record under the preserved key"), USaveSystemStatics::LoadSingleActor(NPC));
				TestTrue(TEXT("Native's saved transform confirms the same record was loaded"), NPC->GetActorTransform().Equals(NativeSavedTransform));
				TestEqual(TEXT("Native restores the survivor's saved health"), NPC->GetHealth(), 37.f);
			}
		}
		TestEqual(TEXT("Exactly one surviving passenger becomes the replacement driver"), Drivers, 1);
		// Fully spent car allowance must not block the surviving occupants of that car.
		auto Exhausted = Counter->GetPersistentState();
		Exhausted[0].MaximumVehicleDeployments = Exhausted[0].VehicleDeploymentsUsed;
		Counter->RestorePersistentState(Exhausted);
		for (auto* NPC : Restored) if (IsValid(NPC)) NPC->Destroy();
		if (auto* OldCar = Counter->PhysicalVehicles.FindRef(Car.VehicleID).Get()) OldCar->Destroy();
		AActor* Debris = World->SpawnActor<AActor>();
		UBoxComponent* DebrisCollision = NewObject<UBoxComponent>(Debris);
		Debris->SetRootComponent(DebrisCollision);
		DebrisCollision->SetBoxExtent(FVector(500.f, 350.f, 250.f));
		DebrisCollision->SetCollisionProfileName(TEXT("BlockAll"));
		Debris->SetActorLocation(Exhausted[0].VehicleCheckpoints[0].Transform.GetLocation());
		DebrisCollision->RegisterComponent();
		auto Final = Counter->ReconstructParticipants(Counter->Assaults.FindChecked(Record.AssaultID), Territory);
		TestEqual(TEXT("Spent vehicle allowance still reconstructs its three survivors"), Final.Num(), 3);
		if (auto* Recovered = Counter->PhysicalVehicles.FindRef(Car.VehicleID).Get())
		{
			TestTrue(*FString::Printf(TEXT("A blocked saved car recovers forward on its remaining route: %s"), *Recovered->GetActorLocation().ToString()),
				Recovered->GetActorLocation().X >= Car.Transform.GetLocation().X + 899.f);
			TestTrue(TEXT("Collision recovery preserves the car's damage"),
				FMath::IsNearlyEqual(Recovered->GetHealth() / Recovered->GetMaxHealth(), 0.6f));
		}
		TestEqual(TEXT("Collision recovery keeps the original charged deployment"), Counter->Assaults.FindChecked(Record.AssaultID).VehicleDeploymentsUsed, 1);
		TestEqual(TEXT("All recovered passengers share one physical car"), Counter->LiveAssaultVehicles.FindRef(Record.AssaultID).Num(), 1);
		Debris->Destroy();
		for (auto* NPC : Final) NPC->AssaultParticipant->CompleteVehicleIngress();
		const auto OnFoot = Counter->GetPersistentState()[0];
		TestTrue(TEXT("Already dismounted survivors no longer request a vehicle on reload"), OnFoot.VehicleCheckpoints.IsEmpty());
		TestTrue(TEXT("Already dismounted members retain their foot transforms and original GUIDs"),
			OnFoot.PendingSurvivors.Num() == 3 && !OnFoot.PendingSurvivors.ContainsByPredicate([](const auto& Member) { return Member.VehicleID.IsValid(); }));
		TestEqual(TEXT("Dismount does not refund the spent vehicle"), OnFoot.VehicleDeploymentsUsed, 1);
		TestEqual(TEXT("Dismount preserves the later four-person wave"), OnFoot.PendingReserveForce, 4);
	}
	SpawnEvent->RemoveDelegate(SpawnDelegate, Characters);
	FTerritoryAssaultRecord Dense = Record;
	Dense.AssaultID = FGuid(401,402,403,404);
	Dense.PlannedForce = Dense.PendingReserveForce = 2;
	Dense.PendingSurvivors.Reset();
	Dense.VehicleCheckpoints.Reset();
	Counter->RestorePersistentState({Dense});
	auto* Anchor = Counter->SpawnParticipant(Counter->Assaults.FindChecked(Dense.AssaultID), Territory,
		Force, NPCDefinition, Approach, FTransform(FVector(40000,40000,100)), INDEX_NONE);
	if (TestNotNull(TEXT("A close-combat restore anchor spawns"), Anchor))
	{
		const float CapsuleSpacing = Anchor->GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.f + 5.f;
		TestTrue(TEXT("The saved spacing is tighter than fresh formation spacing"), CapsuleSpacing < Profile->ParticipantSpacing * 0.7f);
		auto* Neighbor = Counter->SpawnParticipant(Counter->Assaults.FindChecked(Dense.AssaultID), Territory,
			Force, NPCDefinition, Approach, FTransform(Anchor->GetActorLocation() + FVector(CapsuleSpacing,0,0)),
			INDEX_NONE, FGuid(405,406,407,408));
		TestNotNull(TEXT("Valid physical clearance restores a close-combat survivor"), Neighbor);
		TestEqual(TEXT("Close-combat restoration consumes exactly its one pending slot"), Counter->Assaults.FindChecked(Dense.AssaultID).PendingReserveForce, 0);
	}
	World->EndPlay(EEndPlayReason::Quit);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultCheckpointValidation,
	"TerritoryFramework.CounterAttack.SaveLoad.MalformedCheckpointAndLegacyMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultCheckpointValidation::RunTest(const FString& Parameters)
{
	FTerritoryAssaultRecord Record;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 8;
	Record.AliveForce = 3;
	Record.PendingReserveForce = 4;
	Record.KilledForce = 1;
	Record.VehicleDeploymentsUsed = 1;
	Record.VehicleDeploymentsByApproach.Add({TEXT("Road"), 1});
	UTerritoryCounterAttackSubsystem::NormalizePhysicalCheckpoint(Record);
	TestEqual(TEXT("Legacy saved living force becomes a separate reconstruction count"), Record.LegacySurvivorsToRestore, 3);
	TestEqual(TEXT("Legacy migration retains the spent car budget"), Record.VehicleDeploymentsUsed, 1);
	TestEqual(TEXT("Legacy restore credits are bounded by the previously used cars"), Record.LegacyVehicleRestoreCredits[0].Count, 1);
	Record.AliveForce = 0;
	Record.LegacyVehicleRestoreCredits[0].Count = 0;
	UTerritoryCounterAttackSubsystem::NormalizePhysicalCheckpoint(Record);
	TestTrue(TEXT("A second migration does not refill consumed legacy car credits"), Record.LegacyVehicleRestoreCredits.IsEmpty());
	Record.LegacySurvivorsToRestore = 0;
	FTerritoryAssaultSurvivor Valid;
	Valid.SpawnGUID = FGuid(390,391,392,393);
	Valid.ApproachID = TEXT("Foot");
	Record.PendingSurvivors = {Valid, Valid};
	UTerritoryCounterAttackSubsystem::NormalizePhysicalCheckpoint(Record);
	TestEqual(TEXT("Duplicate GUIDs cannot create another attacker"), Record.PendingSurvivors.Num(), 1);
	TestEqual(TEXT("Invalid physical history is withdrawn instead of converted to reserve"), Record.WithdrawnForce, 1);
	UTerritoryCounterAttackSubsystem::NormalizePhysicalCheckpoint(Record);
	TestEqual(TEXT("Normalization is idempotent for casualty accounting"), Record.WithdrawnForce, 1);
	const FProperty* Physical = FindFProperty<FProperty>(FTerritoryAssaultRecord::StaticStruct(), TEXT("PendingSurvivors"));
	TestTrue(TEXT("Physical roster participates in the real SaveGame archive"), Physical && Physical->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("Physical roster is excluded from replicated/RPC read models"), Physical && Physical->HasAnyPropertyFlags(CPF_RepSkip));
	return true;
}

#endif
