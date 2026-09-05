#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/AbilityConfiguration.h"
#include "GameplayEffect.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultSpawnCasualties,
	"TerritoryFramework.CounterAttack.Regression.NarrativeSpawnCasualtiesConsumeFiniteForce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultSpawnCasualties::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Spawn casualty world exists"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	UTerritoryCounterAttackSubsystem* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
	UNarrativeCharacterSubsystem* Characters = World->GetSubsystem<UNarrativeCharacterSubsystem>();
	ATerritoryProperty* Territory = World->SpawnActor<ATerritoryProperty>();
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
	Definition->StableTerritoryGUID = FGuid(231, 232, 233, 234);
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
	Definition->ApplyToTerritory(Territory);
	UNPCDefinition* NPC = NewObject<UNPCDefinition>();
	NPC->CharacterID = TEXT("TerritoryAuditCasualtyCharacter");
	NPC->NPCID = TEXT("TerritoryAuditCasualtyNPC");
	NPC->NPCClassPath = ATerritoryAssaultCharacter::StaticClass();
	NPC->bAllowMultipleInstances = true;
	NPC->AbilityConfiguration = NewObject<UAbilityConfiguration>();
	NPC->AbilityConfiguration->DefaultAttributes = UGameplayEffect::StaticClass();
	FTerritoryFactionAssaultConfig Force;
	Force.Faction = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
	Force.AttackerDefinition = NPC;
	FTerritoryAssaultRecord Record;
	Record.AssaultID = FGuid(235, 236, 237, 238);
	Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
	Record.TargetTerritory = Definition->TerritoryTag;
	Record.AttackingFaction = Force.Faction;
	Record.State = ETerritoryAssaultState::Active;
	Record.PlannedForce = 2;
	Record.PendingReserveForce = 2;
	FTerritoryAssaultApproach Approach;
	Approach.ApproachID = TEXT("AuditFoot");
	UTerritoryAuditEventProbe* Probe = NewObject<UTerritoryAuditEventProbe>();
	FMulticastDelegateProperty* SpawnEvent = FindFProperty<FMulticastDelegateProperty>(Characters->GetClass(), TEXT("OnNPCSpawned"));
	FScriptDelegate SpawnDelegate;
	SpawnDelegate.BindUFunction(Probe, GET_FUNCTION_NAME_CHECKED(UTerritoryAuditEventProbe, NPCSpawned));
	SpawnEvent->AddDelegate(SpawnDelegate, Characters);
	int32 SpawnIndex = 0;
	const auto Spawn = [&]()
	{
		return Counter->SpawnParticipant(Counter->Assaults.FindChecked(Record.AssaultID),
			Territory, Force, NPC, Approach,
			FTransform(FVector(1000.f + 1000.f * SpawnIndex++, 1000.f, 100.f)), INDEX_NONE);
	};
	Counter->RestorePersistentState({Record});
	ATerritoryAssaultCharacter* First = Spawn();
	TestNotNull(TEXT("The first real Narrative attacker is admitted"), First);
	TestEqual(TEXT("Admission immediately counts the first living attacker"), Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 1);
	TestEqual(TEXT("Admission immediately consumes one pending attacker"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 1);
	Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter*)
	{
		if (First) First->AssaultParticipant->Retire(true);
		TestEqual(TEXT("A later NPC's spawn callback records the earlier casualty immediately"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 1);
	};
	ATerritoryAssaultCharacter* Second = Spawn();
	Probe->NPCSpawnCallback = nullptr;
	TestNotNull(TEXT("The second real Narrative attacker is admitted"), Second);
	TestEqual(TEXT("Only the second attacker remains alive"), Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 1);
	TestEqual(TEXT("The two-person finite reserve is exhausted"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 0);
	if (Second) Second->AssaultParticipant->Retire(false);
	if (First) First->AssaultParticipant->Retire(true);
	TestEqual(TEXT("Repeated death cannot duplicate the casualty"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 1);
	TestEqual(TEXT("The final withdrawal is counted once"), Counter->Assaults.FindChecked(Record.AssaultID).WithdrawnForce, 1);
	TestEqual(TEXT("No living or pending force resolves as defeated"), Counter->Assaults.FindChecked(Record.AssaultID).State, ETerritoryAssaultState::Defeated);
	const TArray<FTerritoryAssaultRecord> Saved = Counter->GetPersistentState();
	Counter->RestorePersistentState(Saved);
	TestEqual(TEXT("Reload preserves the casualty instead of refilling force"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 1);
	TestEqual(TEXT("Reload preserves final withdrawal"), Counter->Assaults.FindChecked(Record.AssaultID).WithdrawnForce, 1);

	Counter->RestorePersistentState({Record});
	ATerritoryAssaultCharacter* PlacementAnchor = Spawn();
	Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter* Created)
	{
		if (PlacementAnchor) Created->SetActorLocation(PlacementAnchor->GetActorLocation());
	};
	AddExpectedError(TEXT("collision-adjusted into an occupied deployment slot"), EAutomationExpectedErrorFlags::Contains, 1);
	ATerritoryAssaultCharacter* RejectedPlacement = Spawn();
	Probe->NPCSpawnCallback = nullptr;
	TestNull(TEXT("An invalid final placement is rejected"), RejectedPlacement);
	TestEqual(TEXT("Placement cleanup preserves the unspent reserve"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 1);
	TestEqual(TEXT("Placement cleanup does not record a withdrawal"), Counter->Assaults.FindChecked(Record.AssaultID).WithdrawnForce, 0);
	TestEqual(TEXT("Placement cleanup keeps the earlier live attacker"), Counter->Assaults.FindChecked(Record.AssaultID).AliveForce, 1);

	Record.PlannedForce = 1;
	Record.PendingReserveForce = 1;
	for (bool bKilled : {true, false})
	{
		Counter->RestorePersistentState({Record});
		if (bKilled)
		{
			// The synthetic definition intentionally has no presentation mesh. The
			// actual Native death path must diagnose that while still accounting loss.
			AddExpectedError(TEXT("died without a valid Narrative ragdoll mesh/physics asset"), EAutomationExpectedErrorFlags::Contains, 1);
		}
		Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter* Created)
		{
			ATerritoryAssaultCharacter* Current = CastChecked<ATerritoryAssaultCharacter>(Created);
			ATerritoryAssaultCharacter* Untracked = World->SpawnActor<ATerritoryAssaultCharacter>();
			Counter->NotifyParticipantRemoved(Record.AssaultID, Untracked, true);
			TestEqual(TEXT("An unrelated NPC cannot consume this construction's reserve"), Counter->Assaults.FindChecked(Record.AssaultID).PendingReserveForce, 1);
			TestEqual(TEXT("An unrelated NPC cannot report this construction's casualty"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 0);
			Untracked->Destroy();
			if (bKilled)
			{
				UNarrativeAbilitySystemComponent* ASC = Current->GetNarrativeAbilitySystemComponent();
				TestTrue(TEXT("Narrative has bound the character death handler before SpawnNPC returns"),
					ASC && ASC->OnDeathStateChanged.Contains(Current, FName(TEXT("HandleDeath"))));
				if (ASC)
				{
					const FBoolProperty* Dead = FindFProperty<FBoolProperty>(ASC->GetClass(), TEXT("bIsDead"));
					Dead->SetPropertyValue_InContainer(ASC, true);
					ASC->OnDeathStateChanged.Broadcast(Current, ASC, true);
					TestTrue(TEXT("Native death retires before the participant readiness timer"), Current->AssaultParticipant->HasRetired());
				}
			}
			Current->AssaultParticipant->Retire(bKilled);
			Current->AssaultParticipant->Retire(bKilled);
			const FTerritoryAssaultRecord DuringSpawn = Counter->GetPersistentState()[0];
			TestEqual(TEXT("In-spawn removal consumes its pending force before returning"), DuringSpawn.PendingReserveForce, 0);
			TestEqual(TEXT("In-spawn casualty is stored exactly once"), DuringSpawn.KilledForce, bKilled ? 1 : 0);
			TestEqual(TEXT("In-spawn withdrawal is stored exactly once"), DuringSpawn.WithdrawnForce, bKilled ? 0 : 1);
		};
		ATerritoryAssaultCharacter* Removed = Spawn();
		Probe->NPCSpawnCallback = nullptr;
		TestNull(TEXT("A retired NPC cannot be admitted after its spawn callback"), Removed);
		TestTrue(TEXT("An in-spawn casualty leaves no live participation entry"), Counter->LiveParticipants.IsEmpty());
		TestEqual(TEXT("Exhaustion inside spawn resolves the finite assault"), Counter->Assaults.FindChecked(Record.AssaultID).State, ETerritoryAssaultState::Defeated);
	}
	Counter->RestorePersistentState({Record});
	Probe->NPCSpawnCallback = [&](ANarrativeNPCCharacter* Created)
	{
		ATerritoryAssaultCharacter* Current = CastChecked<ATerritoryAssaultCharacter>(Created);
		Current->SetRole(ROLE_SimulatedProxy);
		Current->AssaultParticipant->Retire(true);
		TestFalse(TEXT("A client actor cannot mark itself authoritatively retired"), Current->AssaultParticipant->HasRetired());
		Current->SetRole(ROLE_Authority);
	};
	ATerritoryAssaultCharacter* Authoritative = Spawn();
	Probe->NPCSpawnCallback = nullptr;
	TestNotNull(TEXT("Authority rejection does not poison a later valid admission"), Authoritative);
	if (Authoritative) Authoritative->AssaultParticipant->Retire(true);
	TestEqual(TEXT("The real server death is counted after client rejection"), Counter->Assaults.FindChecked(Record.AssaultID).KilledForce, 1);
	Counter->RestorePersistentState({});
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
