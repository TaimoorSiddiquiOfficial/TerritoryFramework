#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TerritoryAuditEventProbe.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "UObject/UnrealType.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAssaultHandoverAdmission,
	"TerritoryFramework.CounterAttack.Regression.FinalHandoverAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAssaultHandoverAdmission::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 10; ++Case)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(GEngine));
		World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
		World->SetGameMode(FURL());
		World->TimeSeconds = 2.0; // Deterministic deadline fixture; no scheduler/pawn tick.
		TGuardValue<bool> AllowCallbacks(GAllowActorScriptExecutionInEditor, true);
		const auto Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		const auto Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
		auto* Definition = NewObject<UTerritoryPlaceDefinition>();
		Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
		Definition->StableTerritoryGUID = FGuid(20261005, 5, Case, 1);
		Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
		Definition->CounterAttackProfile = NewObject<UTerritoryCounterAttackProfile>();
		auto* Place = World->SpawnActor<ATerritoryProperty>();
		Definition->ApplyToTerritory(Place);
		World->GetSubsystem<UTerritoryRegistrySubsystem>()->RegisterTerritory(Place);
		auto Ownership = Place->GetOwnershipData();
		Ownership.OwningFaction = Heroes; Ownership.State = ETerritoryState::Contested;
		Ownership.ContestingFaction = Bandits;
		if (Case == 2) Ownership.Availability = ETerritoryAvailability::Locked;
		Place->CommitOwnershipData(Ownership);
		auto* Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
		Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::War);
		auto* Counter = World->GetSubsystem<UTerritoryCounterAttackSubsystem>();
		FTerritoryAssaultRecord Record;
		Record.AssaultID = FGuid(20261005, 5, Case, 2);
		Record.TargetTerritoryGUID = Definition->StableTerritoryGUID;
		Record.TargetTerritory = Definition->TerritoryTag;
		Record.AttackingFaction = Bandits; Record.DefendingFaction = Heroes;
		Record.State = ETerritoryAssaultState::RecaptureCountdown;
		Record.RecaptureEndsGameTime = 1.0;
		Record.PlannedForce = Record.AliveForce = 1;
		Counter->Assaults.Add(Record.AssaultID, Record);
		ATerritoryAssaultCharacter* Attacker = nullptr;
		if (Case != 0)
		{
			Attacker = World->SpawnActor<ATerritoryAssaultCharacter>();
			Cast<INarrativeTeamAgentInterface>(Attacker)->AddFaction(Bandits);
			Attacker->SetActorLocation(Place->GetTerritoryBounds().GetCenter());
			Attacker->AssaultParticipant->Configure(Record.AssaultID, Record.TargetTerritoryGUID, Record.TargetTerritory, Bandits);
			Attacker->AssaultParticipant->ConfigureAssaultRules(true, true);
			Counter->LiveParticipants.FindOrAdd(Record.AssaultID).Add(Attacker);
		}
		if (Case == 1) Place->RegisterDefender(World->SpawnActor<AActor>());
		const auto MarkSavedBodyDead = [Attacker]()
		{
			// Same state Native deserializes before death callbacks clean the registry.
			auto* ASC = Attacker->GetNarrativeAbilitySystemComponent();
			FindFProperty<FBoolProperty>(ASC->GetClass(), TEXT("bIsDead"))->SetPropertyValue_InContainer(ASC, true);
		};
		if (Case == 5)
		{
			MarkSavedBodyDead();
			TestFalse(TEXT("A serialized dead body is not physical capture pressure"), Counter->HasPhysicalAttackerInside(Record, Place));
		}
		bool bAllowCondition = false;
		if (Case == 3)
		{
			auto* Gate = NewObject<UTerritoryAuditCondition>(Place);
			Gate->Callback = [&bAllowCondition] { return bAllowCondition; };
			const_cast<TMap<ETerritoryState, FTerritoryStateConfig>&>(Place->GetStateConfigs())
				.FindOrAdd(ETerritoryState::Claimed).EntryConditions.Add(Gate);
		}
		int32 ConditionCalls = 0;
		if (Case >= 6)
		{
			auto* Gate = NewObject<UTerritoryAuditCondition>(Place);
			Gate->Callback = [&]()
			{
				if (++ConditionCalls == 2) // The final actor commit, after subsystem preflight.
				{
					if (Case == 6) Place->RegisterDefender(World->SpawnActor<AActor>());
					if (Case == 7) Diplomacy->SetDiplomacyState(Heroes, Bandits, EDiplomacyState::Alliance);
					if (Case == 8) MarkSavedBodyDead();
					if (Case == 9) Counter->RestorePersistentState({Record});
				}
				return true;
			};
			const_cast<TMap<ETerritoryState, FTerritoryStateConfig>&>(Place->GetStateConfigs())
				.FindOrAdd(ETerritoryState::Claimed).EntryConditions.Add(Gate);
		}
		auto* Probe = NewObject<UTerritoryAuditEventProbe>();
		int32 OwnershipEvents = 0;
		Probe->OwnershipCallback = [&]() { ++OwnershipEvents; };
		World->GetSubsystem<UTerritoryControlSubsystem>()->OnTerritoryControlChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::OwnershipChanged);
		if (Case == 4) Record.AssaultID = FGuid::NewGuid();
		TestFalse(FString::Printf(TEXT("Handover rejects missing body/defender/lock/condition/stale ID case %d"), Case),
			Counter->CompleteRecapture(Case == 4 ? Record : *Counter->Assaults.Find(Record.AssaultID), Place));
		TestEqual(TEXT("Rejected handover leaves ownership untouched"), Place->GetOwningFaction(), Heroes);
		TestEqual(TEXT("Rejected handover publishes no successful ownership event"), OwnershipEvents, 0);
		if (Case >= 6) TestEqual(TEXT("The race occurs inside final actor conditions"), ConditionCalls, 2);
		if (Case == 3)
		{
			bAllowCondition = true;
			TestTrue(TEXT("Temporary story rejection remains retriable"), Counter->CompleteRecapture(*Counter->Assaults.Find(Record.AssaultID), Place));
			TestEqual(TEXT("Accepted physical handover changes ownership"), Place->GetOwningFaction(), Bandits);
			TestEqual(TEXT("Accepted handover publishes ownership exactly once"), OwnershipEvents, 1);
		}
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	return true;
}
#endif
