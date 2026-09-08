#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultCombatAutonomy,
	"TerritoryFramework.CounterAttack.Regression.CombatWithoutPlayerBoundaryAndDistantDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultCombatAutonomy::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Autonomy world"), World)) return false;
	TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(311, 312, 313, 314);
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->ApplyToTerritory(Place);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	FTerritoryOwnershipData Claimed;
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Place->CommitOwnershipData(Claimed);
	World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Place);
	UTerritoryDiplomacySubsystem* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(315, 316, 317, 318);
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = Bandits;
	Record.DefendingFaction = Heroes;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 8;
	Record.PendingReserveForce = 8;
	Counter->RestorePersistentState({Record});
	ATerritoryGuardCharacter* Guard = World->SpawnActor<ATerritoryGuardCharacter>();
	ATerritoryAssaultCharacter* Attacker = World->SpawnActor<ATerritoryAssaultCharacter>();
	ANarrativeNPCCharacter* Shooter = World->SpawnActor<ATerritoryGuardCharacter>();
	Guard->OwningTerritory = Place;
	Place->RegisterDefender(Guard);
	static_cast<INarrativeTeamAgentInterface*>(Guard)->AddFaction(Heroes);
	static_cast<INarrativeTeamAgentInterface*>(Attacker)->AddFaction(Bandits);
	static_cast<INarrativeTeamAgentInterface*>(Shooter)->AddFaction(Heroes);
	Shooter->SetActorLocation(FVector(50000.f, 50000.f, 0.f));
	UTerritoryAssaultParticipantComponent* Participant = Attacker->AssaultParticipant;
	Participant->Configure(Record.AssaultID, Record.TargetTerritoryGUID, Record.TargetTerritory, Bandits);
	Participant->ConfigureAssaultRules(true, true);
	TestTrue(TEXT("No player exists and the Place is not Contested"), Place->GetTerritoryState() != ETerritoryState::Contested);
	TestTrue(TEXT("Guard engages an active hostile assault before Contested"), Guard->CanEngageTerritoryTarget(Attacker));
	TestTrue(TEXT("Attacker reciprocates against the guard without a nearby player"), Attacker->CanEngageAssaultTarget(Guard));
	TestFalse(TEXT("An ordinary friendly visitor cannot bypass the Claimed guard policy"), Guard->CanEngageTerritoryTarget(Shooter));
	TestFalse(TEXT("A remote non-participant is not a takeover target"), Participant->CollectTakeoverCombatants(Place).Contains(Shooter));
	TestTrue(TEXT("The real Narrative ASC damage delegate is bound"), Participant->BindNarrativeDeathAfterSpawnReady());
	UNarrativeAbilitySystemComponent* SourceASC = Shooter->GetNarrativeAbilitySystemComponent();
	SourceASC->InitAbilityActorInfo(Shooter, Shooter);
	FGameplayEffectSpec Spec;
	Attacker->GetNarrativeAbilitySystemComponent()->DamagedBy(SourceASC, 12.f, Spec);
	TestTrue(TEXT("Real damage makes the distant shooter eligible"), Participant->CollectTakeoverCombatants(Place).Contains(Shooter));
	TestFalse(TEXT("The damaged NPC responds to the shooter before its previous local target"),
		Participant->CollectTakeoverCombatants(Place).Contains(Guard));
	Participant->DamagingEnemies.Add(Guard, World->GetTimeSeconds() + 10.0);
	TestFalse(TEXT("An older damaging guard cannot mask the latest distant shooter"),
		Participant->CollectTakeoverCombatants(Place).Contains(Guard));

	UClass* AttackGoalClass = LoadClass<UNPCGoalItem>(nullptr,
		TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
	const FObjectProperty* Target = AttackGoalClass ? FindFProperty<FObjectProperty>(AttackGoalClass, TEXT("TargetToAttack")) : nullptr;
	if (TestNotNull(TEXT("Narrative attack goal target contract"), Target))
	{
		UNPCGoalItem* Goal = NewObject<UNPCGoalItem>(GetTransientPackage(), AttackGoalClass);
		Target->SetObjectPropertyValue_InContainer(Goal, Shooter);
		Goal->DefaultScore = 4.f;
		FNPCGoalContainer Goals;
		Goals.Goals.Add(Goal);
		TArray<FTerritoryNarrativeGoalScoreOverride> Overrides;
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(Goals, {}, Overrides, true);
		TestEqual(TEXT("Baseline takeover suppresses an unrelated distant goal"), Goal->DefaultScore, 0.f);
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(Goals, Participant->CollectTakeoverCombatants(Place), Overrides, true);
		TestEqual(TEXT("Damaging enemy regains its actual Narrative combat score"), Goal->DefaultScore, 4.f);
		Participant->DamagingEnemies[Shooter] = -1.0;
		TestTrue(TEXT("Damage expiry restores the local defender objective"),
			Participant->CollectTakeoverCombatants(Place).Contains(Guard));
		TerritoryAssaultTargetPolicy::ApplyDefenderPreference(Goals, Participant->CollectTakeoverCombatants(Place), Overrides, true);
		TestEqual(TEXT("Expired damage memory resumes takeover instead of permanent pursuit"), Goal->DefaultScore, 0.f);
		TerritoryAssaultTargetPolicy::RestoreGoalScores(Overrides);
	}
	Participant->DamagingEnemies.Empty();
	Attacker->GetNarrativeAbilitySystemComponent()->DamagedBy(SourceASC, 0.f, Spec);
	TestTrue(TEXT("Zero damage cannot fabricate a threat"), Participant->DamagingEnemies.IsEmpty());
	Attacker->SetRole(ROLE_SimulatedProxy);
	Attacker->GetNarrativeAbilitySystemComponent()->DamagedBy(SourceASC, 10.f, Spec);
	TestTrue(TEXT("Client damage callbacks cannot mutate threat memory"), Participant->DamagingEnemies.IsEmpty());
	Attacker->SetRole(ROLE_Authority);
	Attacker->GetNarrativeAbilitySystemComponent()->DamagedBy(SourceASC, 10.f, Spec);
	Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::Alliance);
	TestFalse(TEXT("Alliance immediately blocks guard combat"), Guard->CanEngageTerritoryTarget(Attacker));
	TestFalse(TEXT("Remembered damage cannot bypass a new treaty"), Participant->CollectTakeoverCombatants(Place).Contains(Shooter));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultWaveStrategies,
	"TerritoryFramework.CounterAttack.Regression.AuthoredWaveStrategiesAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultWaveStrategies::RunTest(const FString& Parameters)
{
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid::NewGuid();
	Record.TargetTerritoryGUID = FGuid::NewGuid();
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 8;
	Record.AliveForce = 4;
	Record.PendingReserveForce = 4;
	Record.WaveSize = 4;
	auto CanDeploy = [&](bool bIngress) { return UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(Record, false, true, true, bIngress); };
	TestFalse(TEXT("Legacy road policy waits for survivors"), CanDeploy(false));
	Record.WaveStrategy = ETerritoryAssaultWaveStrategy::Simultaneous;
	TestTrue(TEXT("Together permits the second squad during the first car's drive"), CanDeploy(true));
	Record.WaveStrategy = ETerritoryAssaultWaveStrategy::BackToBack;
	TestFalse(TEXT("Back to back waits for driver and passenger arrival"), CanDeploy(true));
	TestTrue(TEXT("Back to back deploys while the first four attackers remain alive"), CanDeploy(false));
	Record.WaveStrategy = ETerritoryAssaultWaveStrategy::AfterDefeated;
	TestFalse(TEXT("After defeat waits for all survivors"), CanDeploy(false));
	Record.AliveForce = 0;
	Record.KilledForce = 4;
	TestTrue(TEXT("After defeat deploys the finite second wave"), CanDeploy(false));
	TestFalse(TEXT("Optional proximity still blocks reserves when explicitly enabled"),
		UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(Record, false, false, true));
	for (ETerritoryAssaultWaveStrategy Strategy : {ETerritoryAssaultWaveStrategy::Legacy,
		ETerritoryAssaultWaveStrategy::Simultaneous, ETerritoryAssaultWaveStrategy::BackToBack,
		ETerritoryAssaultWaveStrategy::AfterDefeated})
	{
		Record.WaveStrategy = Strategy;
		Record.VehicleStagingBlockedSince = 45.0;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FObjectAndNameAsStringProxyArchive Save(Writer, false);
		Save.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Save, &Record, nullptr);
		FTerritoryAssaultRecord Loaded;
		FMemoryReader Reader(Bytes);
		FObjectAndNameAsStringProxyArchive Load(Reader, true);
		Load.ArIsSaveGame = true;
		FTerritoryAssaultRecord::StaticStruct()->SerializeItem(Load, &Loaded, nullptr);
		TestEqual(TEXT("Save preserves the selected strategy"), Loaded.WaveStrategy, Strategy);
		TestEqual(TEXT("Save preserves permanently consumed first-wave casualties"), Loaded.KilledForce, 4);
		TestEqual(TEXT("Save preserves the bounded traffic wait"), Loaded.VehicleStagingBlockedSince, 45.0);
		Loaded.State = ETerritoryAssaultState::ScheduledWarning;
		TestFalse(TEXT("Every strategy spawns zero during a warning"), UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(Loaded, true, true, false));
		Loaded.State = ETerritoryAssaultState::Defeated;
		TestFalse(TEXT("Every strategy respects terminal state"), UTerritoryCounterAttackSubsystem::ShouldDeployActiveReserveWave(Loaded, true, true, false));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultTeardownAccounting,
	"TerritoryFramework.CounterAttack.Regression.TeardownIsNotAForceWithdrawal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultTeardownAccounting::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Departure fixture world"), World)) return false;
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	for (const EEndPlayReason::Type Reason : {EEndPlayReason::Destroyed,
		EEndPlayReason::RemovedFromWorld, EEndPlayReason::Quit,
		EEndPlayReason::LevelTransition, EEndPlayReason::EndPlayInEditor})
	{
		ATerritoryAssaultCharacter* NPC = World->SpawnActor<ATerritoryAssaultCharacter>();
		FTerritoryAssaultRecord Record;
		Record.AssaultID = FGuid::NewGuid();
		Record.TargetTerritoryGUID = FGuid::NewGuid();
		Record.TargetTerritory = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
		Record.AttackingFaction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
		Record.State = ETerritoryAssaultState::Active;
		Record.PlannedForce = 2;
		Record.AliveForce = 1;
		Record.PendingReserveForce = 1;
		Counter->Assaults.Add(Record.AssaultID, Record);
		Counter->LiveParticipants.FindOrAdd(Record.AssaultID).Add(NPC);
		NPC->AssaultParticipant->Configure(Record.AssaultID, Record.TargetTerritoryGUID,
			Record.TargetTerritory, Record.AttackingFaction);
		NPC->AssaultParticipant->BeginPlay();
		World->bIsTearingDown = true;
		Counter->NotifyParticipantRemoved(Record.AssaultID, NPC, false);
		TestEqual(TEXT("A callback during world teardown cannot consume living force"),
			Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 1);
		World->bIsTearingDown = false;
		NPC->AssaultParticipant->EndPlay(Reason);
		const bool bGameplayDeparture = Reason == EEndPlayReason::Destroyed
			|| Reason == EEndPlayReason::RemovedFromWorld;
		const FTerritoryAssaultRecord& After = Counter->Assaults.FindChecked(Record.AssaultID);
		TestEqual(TEXT("Only actual destruction or streaming consumes a survivor"),
			After.WithdrawnForce, bGameplayDeparture ? 1 : 0);
		TestEqual(TEXT("Quit/travel preserve durable living force"), After.AliveForce,
			bGameplayDeparture ? 0 : 1);
		TestEqual(TEXT("No departure rerolls or consumes the saved reserve"), After.PendingReserveForce, 1);
		TestEqual(TEXT("Quit/travel do not mark a surviving participant retired"),
			NPC->AssaultParticipant->HasRetired(), bGameplayDeparture);
		Counter->LiveParticipants.Remove(Record.AssaultID);
		Counter->Assaults.Remove(Record.AssaultID);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
