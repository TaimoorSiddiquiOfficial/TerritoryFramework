#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "AI/TerritoryNPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCGoalGenerator.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Tales/Quest.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UObject/UnrealType.h"

namespace TerritoryGuardResponseTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ATerritoryProperty* Place;
		UTerritoryPlaceDefinition* Definition;
		UTerritoryStealthProfile* Profile;
		UTerritoryControlSubsystem* Control;
		UTerritoryDiplomacySubsystem* Diplomacy;
		FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
		FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
		FFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameInstance(NewObject<UGameInstance>(GEngine));
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());
			UClass* GameStateClass = LoadClass<ANarrativeGameState>(nullptr,
				TEXT("/NarrativePro/Pro/Core/BP/Framework/BP_NarrativeGameState.BP_NarrativeGameState_C"));
			if (GameStateClass) World->SetGameState(World->SpawnActor<ANarrativeGameState>(GameStateClass));
			World->CreateAISystem();
			Control = World->GetSubsystem<UTerritoryControlSubsystem>();
			Diplomacy = World->GetSubsystem<UTerritoryDiplomacySubsystem>();
			Place = World->SpawnActor<ATerritoryProperty>();
			Definition = NewObject<UTerritoryPlaceDefinition>();
			Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(TEXT("Territory.HavenReach.MarketSquare.Blacksmith"));
			Definition->StableTerritoryGUID = FGuid(101, 102, 103, 104);
			Profile = NewObject<UTerritoryStealthProfile>(Definition);
			Profile->EscalationScope = ETerritoryStealthEscalationScope::TerritoryConflict;
			Profile->MaximumInvestigators = 0;
			Definition->DefaultStealthProfile = Profile;
			Definition->ApplyToTerritory(Place);
			FTerritoryOwnershipData Owned;
			Owned.OwningFaction = Bandits;
			Owned.State = ETerritoryState::Claimed;
			Place->CommitOwnershipData(Owned);
		}
		~FFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		ATerritoryGuardCharacter* Character(FGameplayTag Faction)
		{
			auto* NPC = World->SpawnActor<ATerritoryGuardCharacter>();
			Cast<INarrativeTeamAgentInterface>(NPC)->AddFaction(Faction);
			NPC->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(NPC, NPC);
			return NPC;
		}
		ANarrativePlayerCharacter* Player()
		{
			UClass* PlayerClass = LoadClass<ANarrativePlayerCharacter>(nullptr,
				TEXT("/TerritoryFramework/Framework/BP_TerritoryPlayerCharacter.BP_TerritoryPlayerCharacter_C"));
			if (!PlayerClass) return nullptr;
			auto* Player = World->SpawnActor<ANarrativePlayerCharacter>(PlayerClass);
			auto* State = World->SpawnActor<ANarrativePlayerState>();
			Player->SetPlayerState(State);
			State->SetFactions(FGameplayTagContainer(Heroes));
			auto* ASC = CastChecked<UNarrativeAbilitySystemComponent>(State->GetAbilitySystemComponent());
			ASC->InitAbilityActorInfo(State, Player);
			FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("AbilitySystemComponent"))->SetObjectPropertyValue_InContainer(Player, ASC);
			return Player;
		}
		bool Evidence(AActor* Target, ETerritoryStealthEvidence Type = ETerritoryStealthEvidence::Damage)
		{
			return Control->ReportStealthEvidence(Place, Target, nullptr, Type, 1.f,
				Target->GetActorLocation(), FVector::ZeroVector, Type == ETerritoryStealthEvidence::Damage, 0.f);
		}
		void ControlTick() { Control->ProcessEvent(Control->FindFunction(TEXT("OnCaptureTick")), nullptr); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardResponsePolicy,
	"TerritoryFramework.Guards.Regression.QuestIndependentDefenceAndPerPlayerStealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGuardResponsePolicy::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	TerritoryGuardResponseTests::FFixture F;
	if (!TestNotNull(TEXT("Native authored GameState implements stable identity"), Cast<ANarrativeGameState>(F.World->GetGameState()))) return false;
	auto* Guard = F.Character(F.Bandits);
	auto* Target = F.Player();
	auto* HiddenPlayer = F.Player();
	if (!TestNotNull(TEXT("Real Narrative player target"), Target) || !TestNotNull(TEXT("Second real player"), HiddenPlayer)) return false;
	TestTrue(TEXT("Native player semantics are active"), static_cast<APawn*>(Target)->IsPlayerControlled()
		&& static_cast<APawn*>(HiddenPlayer)->IsPlayerControlled());
	FindFProperty<FObjectPropertyBase>(Guard->GetClass(), TEXT("OwningTerritory"))->SetObjectPropertyValue_InContainer(Guard, F.Place);
	FText Reason;
	TestFalse(TEXT("Neutral visitors do not start combat"), Guard->EvaluateTerritoryTarget(Target, Reason));
	TestFalse(TEXT("Decision supplies a useful reason"), Reason.IsEmpty());
	F.Diplomacy->DeclareWar(F.Bandits, F.Heroes);
	F.Evidence(Target);
	TestTrue(TEXT("Confirmed enemy may be fought"), Guard->CanEngageTerritoryTarget(Target));
	TestFalse(TEXT("Another player remains hidden during a global contest"), Guard->CanEngageTerritoryTarget(HiddenPlayer));
	F.Profile->EscalationScope = ETerritoryStealthEscalationScope::LocalAlarm;
	TestFalse(TEXT("Local Alarm alone never starts combat"), Guard->CanEngageTerritoryTarget(Target));
	F.Profile->EscalationScope = ETerritoryStealthEscalationScope::TerritoryConflict;
	F.Control->ClearInfiltratorExposure(F.Place, Target);
	TestFalse(TEXT("Exposure clearing immediately closes this player's combat gate"), Guard->CanEngageTerritoryTarget(Target));
	F.ControlTick();
	TestEqual(TEXT("Retaliation fixture starts from a Claimed Place"), F.Place->GetTerritoryState(), ETerritoryState::Claimed);

	// The real online Tales context activates a normal quest override. There is no
	// fake quest-state subsystem and no bypass of the production guard decision.
	UClass* PlayerControllerClass = LoadClass<ANarrativePlayerController>(nullptr,
		TEXT("/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController.BP_NarrativePlayerController_C"));
	if (!TestNotNull(TEXT("Native authored player controller supplies stable identity"), PlayerControllerClass)) return false;
	auto* QuestController = F.World->SpawnActor<ANarrativePlayerController>(PlayerControllerClass);
	F.World->AddController(QuestController); // This isolated world has not run actor initialization/BeginPlay.
	FTerritoryQuestRuntimeOverrideRule Rule;
	Rule.QuestClass = UQuest::StaticClass();
	Rule.ActiveQuestState = ETerritoryQuestStateRequirement::NotStarted;
	F.Definition->QuestRuntimeOverrides.Add(Rule);
	TestTrue(TEXT("Fixture quest actually blocks automatic capture"), F.Place->IsPrimaryRuntimeRuleSuspendedWithContext(ETerritoryQuestOverrideEffect::AutomaticCapture, nullptr));
	F.Diplomacy->SetDiplomacyState(F.Bandits, F.Heroes, EDiplomacyState::None);
	TestFalse(TEXT("Quest-owned target is not admitted for capture"), F.Control->TryRegisterAttacker(F.Place, Target, F.Heroes));
	FScriptDelegate DamageListener;
	DamageListener.BindUFunction(Guard, TEXT("HandleNarrativeDamagedBy"));
	Guard->GetNarrativeAbilitySystemComponent()->OnDamagedBy.Add(DamageListener);
	Target->SetActorLocation(FVector(50000.f, 50000.f, 0.f));
	FGameplayEffectSpec Spec;
	Guard->GetNarrativeAbilitySystemComponent()->DamagedBy(Target->GetNarrativeAbilitySystemComponent(), 12.f, Spec);
	TestTrue(TEXT("Actual Native damage permits local retaliation while War/state events are paused"), Guard->CanEngageTerritoryTarget(Target));
	TestTrue(TEXT("Actual outside damage records exposure even before perception binds"), F.Control->IsInfiltratorExposed(F.Place, Target));
	F.Control->ClearInfiltratorExposure(F.Place, Target);
	F.Profile->bDamageImmediatelyExposes = false;
	Guard->GetNarrativeAbilitySystemComponent()->DamagedBy(Target->GetNarrativeAbilitySystemComponent(), 12.f, Spec);
	TestFalse(TEXT("Direct damage respects the authored exposure option"), F.Control->IsInfiltratorExposed(F.Place, Target));
	F.Profile->bDamageImmediatelyExposes = true;
	Target->SetActorLocation(FVector::ZeroVector);
	TestEqual(TEXT("Personal defence does not declare faction War"), F.Diplomacy->GetDiplomacyState(F.Bandits, F.Heroes), EDiplomacyState::None);
	TestEqual(TEXT("Personal defence does not register capture pressure"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 0);
	TestFalse(TEXT("A second hidden player is not blamed for the attack"), Guard->CanEngageTerritoryTarget(HiddenPlayer));
	F.Definition->QuestRuntimeOverrides[0].bPauseDefenderCombat = true;
	TestFalse(TEXT("Explicit quest combat pause blocks retaliation"), Guard->CanEngageTerritoryTarget(Target));
	F.Definition->QuestRuntimeOverrides[0].bPauseDefenderCombat = false;
	TestTrue(TEXT("Removing only combat pause resumes defence"), Guard->CanEngageTerritoryTarget(Target));
	for (EDiplomacyState State : {EDiplomacyState::Alliance, EDiplomacyState::TradeAgreement,
		EDiplomacyState::NonAggression, EDiplomacyState::Ceasefire})
	{
		F.Diplomacy->SetDiplomacyState(F.Bandits, F.Heroes, State);
		TestFalse(TEXT("Protective treaties block personal retaliation"), Guard->CanEngageTerritoryTarget(Target));
	}
	F.Diplomacy->SetDiplomacyState(F.Bandits, F.Heroes, EDiplomacyState::None);
	F.Definition->GuardBehavior.bAllowPersonalRetaliation = false;
	TestFalse(TEXT("Authored non-retaliating guard respects its policy"), Guard->CanEngageTerritoryTarget(Target));
	F.Definition->GuardBehavior.bAllowPersonalRetaliation = true;
	F.Definition->GuardBehavior.CombatTargetFactions.AddTag(F.Bandits);
	TestFalse(TEXT("Optional exact faction filter applies to retaliation too"), Guard->CanEngageTerritoryTarget(Target));
	F.Definition->GuardBehavior.CombatTargetFactions.Reset();
	Guard->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Clients cannot derive combat from server-only personal records"), Guard->CanEngageTerritoryTarget(Target));
	Guard->SetRole(ROLE_Authority);
	FindFProperty<FObjectPropertyBase>(Guard->GetClass(), TEXT("OwningTerritory"))->SetObjectPropertyValue_InContainer(Guard, nullptr);
	TestFalse(TEXT("Streamed-out Place cannot drive guard combat"), Guard->CanEngageTerritoryTarget(Target));
	FindFProperty<FObjectPropertyBase>(Guard->GetClass(), TEXT("OwningTerritory"))->SetObjectPropertyValue_InContainer(Guard, F.Place);
	TestTrue(TEXT("Owner resolved after stream-in restores the policy"), Guard->CanEngageTerritoryTarget(Target));

	// Prove Native consumes a real stored damage stimulus under the same quest lock.
	UClass* ControllerClass = LoadClass<ANarrativeNPCController>(nullptr, TEXT("/TerritoryFramework/AI/BP_TerritoryNPCController.BP_TerritoryNPCController_C"));
	auto* Controller = F.World->SpawnActor<ANarrativeNPCController>(ControllerClass);
	Controller->SetPawn(Guard);
	auto* Visual = F.World->SpawnActor<ANarrativeCharacterVisual>();
	Visual->bBaseAppearanceLoaded = true;
	FindFProperty<FObjectPropertyBase>(Guard->GetClass(), TEXT("CharVisual"))->SetObjectPropertyValue_InContainer(Guard, Visual);
	auto* Activity = CastChecked<UTerritoryNPCActivityComponent>(Controller->GetActivityComponent());
	FindFProperty<FObjectPropertyBase>(UNPCActivityComponent::StaticClass(), TEXT("OwnerController"))->SetObjectPropertyValue_InContainer(Activity, Controller);
	Activity->Activate();
	UClass* GeneratorClass = LoadClass<UNPCGoalGenerator>(nullptr, TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/GoalGenerator_Attack.GoalGenerator_Attack_C"));
	TestNotNull(TEXT("Native attack generator installs"), Activity->AddGoalGenerator(GeneratorClass, true));
	auto* Perception = Controller->GetAIPerceptionComponent();
	Perception->ConfigureSense(*NewObject<UAISenseConfig_Damage>(Perception));
	Perception->RegisterStimulus(Target, FAIStimulus(*GetDefault<UAISense_Damage>(), 12.f, Target->GetActorLocation(), Guard->GetActorLocation()));
	Perception->ProcessStimuli();
	Activity->RefreshStoredPerception();
	UClass* AttackClass = LoadClass<UNPCGoalItem>(nullptr, TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
	bool Found = false;
	TestNotNull(TEXT("Narrative creates its real attack goal without a War or Contested event"), Activity->GetGoalByKey(AttackClass, Target, Found));
	TestTrue(TEXT("Native goal key is the actual attacker"), Found);

	// Exercise the real bounds reconciliation, including a quest lock that starts
	// after the player entered. Combat awareness must not flicker with capture locks.
	QuestController->SetPawn(Target);
	F.Place->bStoryCaptureFromBounds = true;
	F.Definition->QuestRuntimeOverrides[0].bPauseAutomaticCapture = false;
	Target->SetActorLocation(FVector::ZeroVector);
	F.Evidence(Target);
	F.Place->ReconcileStoryBoundsContesters();
	F.Definition->QuestRuntimeOverrides[0].bPauseAutomaticCapture = true;
	F.Place->ReconcileStoryBoundsContesters();
	TestTrue(TEXT("Starting a quest capture lock preserves known in-bounds exposure"), F.Control->IsInfiltratorExposed(F.Place, Target));
	Target->SetActorLocation(FVector(50000.f, 50000.f, 0.f));
	F.Place->ReconcileStoryBoundsContesters();
	TestTrue(TEXT("Crossing the Place boundary does not hide an exposed outside threat"), F.Control->IsInfiltratorExposed(F.Place, Target));
	F.Control->ClearInfiltratorExposure(F.Place, Target);
	TestFalse(TEXT("Explicit clearing still releases outside awareness"), F.Control->IsInfiltratorExposed(F.Place, Target));
	Target->SetActorLocation(FVector::ZeroVector);
	F.Place->ReconcileStoryBoundsContesters();
	F.Evidence(Target);
	F.Profile->bRespondToOutsideThreats = false;
	Target->SetActorLocation(FVector(50000.f, 50000.f, 0.f));
	F.Place->ReconcileStoryBoundsContesters();
	TestFalse(TEXT("Disabling outside evidence retains bounds-exit cleanup"), F.Control->IsInfiltratorExposed(F.Place, Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthParticipationSources,
	"TerritoryFramework.Stealth.Regression.ExposureReleasesOnlyOwnedParticipation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthParticipationSources::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	TerritoryGuardResponseTests::FFixture F;
	auto* Target = F.Character(F.Heroes);
	F.Diplomacy->DeclareWar(F.Bandits, F.Heroes);
	TestTrue(TEXT("Exposure registers a contester"), F.Evidence(Target));
	TestEqual(TEXT("One physical identity after exposure"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 1);
	TestTrue(TEXT("Bounds can also hold that same actor"), F.Control->TryRegisterStoryBoundsContester(F.Place, Target, F.Heroes));
	F.Control->ClearInfiltratorExposure(F.Place, Target);
	TestEqual(TEXT("Clearing releases exposure and bounds origins"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 0);
	F.ControlTick();
	TestEqual(TEXT("Empty exposure-only contest recovers on the authority tick"), F.Place->GetTerritoryState(), ETerritoryState::Claimed);
	F.Evidence(Target);
	F.Control->TryRegisterAttacker(F.Place, Target, F.Heroes);
	F.Control->TryRegisterStoryBoundsContester(F.Place, Target, F.Heroes);
	F.Control->ClearInfiltratorExposure(F.Place, Target);
	F.Control->UnregisterStoryBoundsContester(F.Place, Target, F.Heroes);
	TestEqual(TEXT("Flag capture participation survives both stealth and bounds cleanup"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 1);
	F.ControlTick();
	TestTrue(TEXT("Surviving real capture participant still generates pressure"), F.Control->GetCaptureProgress(F.Place) > 0.f);
	F.Control->ResetCapture(F.Place);
	F.Evidence(Target);
	F.Control->TryRegisterContester(F.Place, Target, F.Heroes);
	F.Control->ClearInfiltratorExposure(F.Place, Target);
	TestEqual(TEXT("An explicit story contester survives exposure clearing"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 1);
	F.ControlTick();
	TestEqual(TEXT("Explicit story contest never gains capture pressure"), F.Control->GetCaptureProgress(F.Place), 0.f);
	F.Control->ResetCapture(F.Place);
	F.Control->UnregisterInfiltrator(F.Place, Target);
	// A real exposure callback can hand the scene to a different story profile.
	// Reconcile participation against that final policy, not the old asset pointer.
	auto* Probe = NewObject<UTerritoryAuditEventProbe>(F.World);
	auto* LocalProfile = NewObject<UTerritoryStealthProfile>(F.Definition);
	LocalProfile->EscalationScope = ETerritoryStealthEscalationScope::LocalAlarm;
	LocalProfile->MaximumInvestigators = 0;
	Probe->ExposureCallback = [&](ATerritoryVolume*, AActor*, ETerritoryExposureState NewState)
	{
		if (NewState == ETerritoryExposureState::Exposed) F.Definition->DefaultStealthProfile = LocalProfile;
	};
	F.Control->OnExposureChanged.AddDynamic(Probe, &UTerritoryAuditEventProbe::ExposureChanged);
	TestTrue(TEXT("Evidence callback can switch to a local alarm profile"), F.Evidence(Target));
	TestEqual(TEXT("Final local policy cannot register stale territory conflict"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 0);
	F.Control->OnExposureChanged.RemoveDynamic(Probe, &UTerritoryAuditEventProbe::ExposureChanged);
	F.Definition->DefaultStealthProfile = F.Profile;
	F.Control->UnregisterInfiltrator(F.Place, Target);
	Target->SetActorLocation(FVector(50000.f, 50000.f, 0.f));
	TestTrue(TEXT("Outside gunshot is accepted as a clue"), F.Evidence(Target, ETerritoryStealthEvidence::Gunshot));
	TestFalse(TEXT("Outside anonymous clue does not identify its owner"), F.Control->IsInfiltratorExposed(F.Place, Target));
	TestFalse(TEXT("Outside ordinary sight cannot expose a passer-by"), F.Evidence(Target, ETerritoryStealthEvidence::Sight));
	TestTrue(TEXT("Identified outside damage may expose the shooter"), F.Evidence(Target));
	FTerritoryInfiltrationSnapshot Snapshot;
	F.Control->GetInfiltrationSnapshot(F.Place, Target, Snapshot);
	TestFalse(TEXT("Outside shooter never pretends to be physically inside"), Snapshot.bInsideTerritory);
	F.Profile->bRespondToOutsideThreats = false;
	TestFalse(TEXT("Authored outside-threat policy is respected by every evidence caller"), F.Evidence(Target));
	F.Control->UnregisterInfiltrator(F.Place, Target);
	F.Control->RestoreCaptureState(F.Place, F.Heroes, 0.2f);
	auto* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Native saves the contested political snapshot"), Save->CreateActorRecord(F.Place, Record));
	F.Control->ResetCapture(F.Place);
	Save->LoadActorFromRecord(F.Place, Record);
	TestTrue(TEXT("Native load restores existing capture progress"), F.Control->GetCaptureProgress(F.Place) > 0.f);
	TestFalse(TEXT("Restoring political capture state does not manufacture exposure"), F.Control->IsInfiltratorExposed(F.Place, Target));
	TestEqual(TEXT("Transient source registrations are not invented by save restore"), F.Control->GetActiveAttackers(F.Place, F.Heroes), 0);
	return true;
}

#endif
