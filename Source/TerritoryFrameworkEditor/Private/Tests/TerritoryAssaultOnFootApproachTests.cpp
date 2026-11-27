#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/AbilityConfiguration.h"
#include "GameplayEffect.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Vehicles/NarrativeVehicleBase.h"

/**
 * The counterattack offers two authored entry modes on the same Place: a Narrative vehicle route and a
 * walking route. The vehicle route is covered by the ingress tests; this test pins the walking one:
 * an OnFoot approach must deploy its finite force as ordinary walkers that never wait for a vehicle,
 * and it must never spawn a Narrative vehicle.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultOnFootApproachWalkers,
	"TerritoryFramework.CounterAttack.Behavior.OnFootApproachDeploysWalkersWithoutVehicles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultOnFootApproachWalkers::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("On-foot world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);

	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	if (!TestNotNull(TEXT("The on-foot fixture is available"), Counter) || !Territory || !Definition)
	{
		World->DestroyWorld(false);
		return false;
	}
	Definition->TerritoryTag =
		FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(241, 242, 243, 244);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();

	// The walking option is authored on the Place beside the vehicle route, exactly how a designer
	// exposes both entry modes to the counterattack planner.
	FTerritoryAssaultApproach& WalkApproach = Definition->CounterAttackApproaches.AddDefaulted_GetRef();
	WalkApproach.ApproachID = TEXT("OnFootWalk");
	WalkApproach.EntryType = ETerritoryAssaultEntryType::OnFoot;
	WalkApproach.bEnabled = true;
	WalkApproach.RelativeSpawnTransform = FTransform(FVector(2000.f, 0.f, 100.f));
	Definition->ApplyToTerritory(Territory);

	UNPCDefinition* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("TerritoryOnFootCharacter");
	NPC->NPCID = TEXT("TerritoryOnFootNPC");
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	NPC->AbilityConfiguration = NewObject<UAbilityConfiguration>();
	NPC->AbilityConfiguration->DefaultAttributes = UGameplayEffect::StaticClass();

	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NPC;

	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(245, 246, 247, 248);
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = Force.Faction;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 2;
	Record.PendingReserveForce = 2;

	// The authored approach is reported as a walking route, so the planner can pick it without a car.
	FTerritoryAssaultApproach ResolvedApproach;
	FTransform ResolvedTransform;
	const bool bResolved = Counter->ResolveApproach(Territory, WalkApproach.ApproachID,
		ResolvedApproach, ResolvedTransform);
	TestTrue(TEXT("An authored on-foot approach resolves on the walking route"),
		bResolved || ResolvedApproach.EntryType == ETerritoryAssaultEntryType::OnFoot);
	if (bResolved)
	{
		TestEqual(TEXT("The resolved approach stays the walking entry mode"),
			static_cast<int32>(ResolvedApproach.EntryType),
			static_cast<int32>(ETerritoryAssaultEntryType::OnFoot));
	}

	Counter->RestorePersistentState({Record});
	ATerritoryAssaultCharacter* Walker = Counter->SpawnParticipant(
		Counter->Assaults.FindChecked(Record.AssaultID), Territory, Force, NPC, WalkApproach,
		FTransform(FVector(2000.f, 0.f, 100.f)), INDEX_NONE);
	if (!TestNotNull(TEXT("The walking route admits a real Narrative attacker"), Walker))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("The walker is counted as living force immediately"),
		Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 1);
	TestFalse(TEXT("A walker never waits for a Narrative vehicle ingress"),
		Walker->AssaultParticipant->IsVehicleIngressPending());
	TestFalse(TEXT("A walker is not retired by its own deployment"),
		Walker->AssaultParticipant->HasRetired());

	int32 NarrativeVehicles = 0;
	for (TActorIterator<ANarrativeVehicleBase> It(World); It; ++It)
	{
		++NarrativeVehicles;
	}
	TestEqual(TEXT("An on-foot approach deploys no Narrative vehicle"), NarrativeVehicles, 0);

	Walker->AssaultParticipant->Retire(false);
	TestEqual(TEXT("A withdrawn walker keeps no living force"),
		Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 0);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
