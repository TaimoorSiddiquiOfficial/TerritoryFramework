#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeExit.h"
#include "Tests/TerritoryAuditEventProbe.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryHierarchy.h"
#include "AI/TerritoryNPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "AI/TerritoryNPCController.h"
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

	/**
	 * Captures the plugin's own log exactly as a developer running with -log would read it.
	 *
	 * Asserting on the log rather than on an internal variable is the point: the complaint this
	 * test answers is that a guard's decision could only be discovered by reading the source.
	 * Only LogTerritory is kept, so this is the plugin's own voice and not the engine's.
	 *
	 * UE_LOG can be reached from any thread, so the buffer is guarded; the assertions that read
	 * it run on the game thread after Flush and see a stable snapshot.
	 */
	struct FTerritoryLogCapture : public FOutputDevice
	{
		mutable FCriticalSection Guard;
		TArray<FString> Lines;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type,
			const FName& Category) override
		{
			// Match the category by its name, which is the string a developer types into the
			// Output Log filter. This module cannot reference the LogTerritory symbol itself:
			// DEFINE_LOG_CATEGORY does not export it from the runtime module's DLL, so naming
			// it here is an unresolved external. The name is the contract anyway.
			static const FName TerritoryCategory(TEXT("LogTerritory"));
			if (Category != TerritoryCategory) return;
			FScopeLock Lock(&Guard);
			Lines.Add(FString(V));
		}

		int32 CountContaining(const FString& Needle) const
		{
			FScopeLock Lock(&Guard);
			int32 Count = 0;
			for (const FString& Line : Lines)
			{
				Count += Line.Contains(Needle) ? 1 : 0;
			}
			return Count;
		}
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

	// Faction-level hostility has no other channel in the guard decision: Narrative's
	// personal aggressiveness test is a per-actor Hostiles set, not an attitude. Without
	// this policy a broken alliance left every Claimed Place the player captured for that
	// faction completely safe, which is exactly the betrayal having no teeth.
	//
	// Stealth infiltration is switched off so the decision can reach the faction policy at
	// all. While it is on, a hidden player is refused earlier (a hidden player is never
	// attacked, policy or not) and an exposed one is admitted earlier by
	// bDefendAgainstExposedEnemies — either way the faction-War policy is never consulted.
	// Reporting damage as the exposure would not work either: that escalates the Place to
	// Contested, which is itself an allow. Turning infiltration off leaves faction War as
	// the only reason left to engage, which is exactly what these assertions are about.
	const bool bStealthWasEnabled = F.Profile->bAllowStealthInfiltration;
	F.Profile->bAllowStealthInfiltration = false;
	TestFalse(TEXT("Stealth infiltration is off, so visibility cannot decide this"),
		F.Control->IsStealthInfiltrationEnabled(F.Place));
	TestEqual(TEXT("The Place is Claimed rather than Contested"),
		F.Place->GetTerritoryState(), ETerritoryState::Claimed);
	TestFalse(TEXT("A Claimed Place stays safe at War while the policy is off"),
		Guard->CanEngageTerritoryTarget(Target));
	F.Definition->GuardBehavior.bEngageAtWarInClaimedTerritory = true;
	TestTrue(TEXT("Authoring the policy lets a faction at War defend its Claimed Place"),
		Guard->CanEngageTerritoryTarget(Target));
	F.Definition->GuardBehavior.bEngageAtWarInClaimedTerritory = false;
	TestFalse(TEXT("Clearing the policy restores the peaceful Claimed Place"),
		Guard->CanEngageTerritoryTarget(Target));
	F.Profile->bAllowStealthInfiltration = bStealthWasEnabled;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFAttitudePricePolicy,
	"TerritoryFramework.Economy.Regression.AttitudeChangesGuardPrice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFAttitudePricePolicy::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	TerritoryGuardResponseTests::FFixture F;
	auto* Buyer = F.Player();
	if (!TestNotNull(TEXT("Real Narrative player buyer with the Heroes faction"), Buyer)) return false;
	if (!TestNotNull(TEXT("Diplomacy subsystem"), F.Diplomacy)) return false;

	// FTerritoryOwnershipData defaults GuardRecruitmentCost to 0, and the shared fixture
	// commits ownership without one, so this Place would otherwise recruit for free and
	// every multiplier assertion below would pass on 0 == 0. Give it the price a captured
	// place actually adopts from its definition, so the arithmetic has something to scale.
	FTerritoryOwnershipData Priced;
	Priced.OwningFaction = F.Bandits;
	Priced.State = ETerritoryState::Claimed;
	Priced.GuardRecruitmentCost = F.Definition->GuardRecruitmentCost;
	F.Place->CommitOwnershipData(Priced);

	const int32 BaseCost = F.Place->GetGuardRecruitmentCost(1);
	if (!TestTrue(TEXT("The Place carries a real base guard price to scale"), BaseCost > 0)) return false;

	// Off by default, so a betrayal cannot silently reprice a project that never asked
	// for attitude pricing. This is the assertion that protects every existing user.
	TestFalse(TEXT("Attitude pricing is off by default"), F.Definition->bAttitudeAffectsPrices);
	F.Diplomacy->DeclareWar(F.Bandits, F.Heroes);
	TestEqual(TEXT("With the policy off, War does not move the price"),
		F.Place->GetGuardRecruitmentCostFor(Buyer, 1), BaseCost);
	TestEqual(TEXT("The base overload still ignores attitude, preserving existing callers"),
		F.Place->GetGuardRecruitmentCost(1), BaseCost);

	F.Definition->bAttitudeAffectsPrices = true;

	const int32 WarCost = F.Place->GetGuardRecruitmentCostFor(Buyer, 1);
	TestTrue(TEXT("A faction at War pays more than the base price"), WarCost > BaseCost);
	TestEqual(TEXT("War applies the authored multiplier"),
		WarCost, FMath::RoundToInt32(BaseCost * F.Definition->WarPriceMultiplier));

	F.Diplomacy->SetDiplomacyState(F.Bandits, F.Heroes, EDiplomacyState::Alliance);
	const int32 AllyCost = F.Place->GetGuardRecruitmentCostFor(Buyer, 1);
	TestTrue(TEXT("An allied faction pays less than the base price"), AllyCost < BaseCost);
	TestEqual(TEXT("Alliance applies the authored multiplier"),
		AllyCost, FMath::RoundToInt32(BaseCost * F.Definition->FriendlyPriceMultiplier));

	F.Diplomacy->SetDiplomacyState(F.Bandits, F.Heroes, EDiplomacyState::None);
	TestEqual(TEXT("A neutral faction pays the neutral multiplier"),
		F.Place->GetGuardRecruitmentCostFor(Buyer, 1),
		FMath::RoundToInt32(BaseCost * F.Definition->NeutralPriceMultiplier));

	// A whole batch is priced in one step, so the surcharge cannot be lost by applying
	// it to a per-guard price that was already rounded.
	F.Diplomacy->DeclareWar(F.Bandits, F.Heroes);
	TestTrue(TEXT("A War batch of three costs more than three base-priced guards"),
		F.Place->GetGuardRecruitmentCostFor(Buyer, 3) > BaseCost * 3);

	// A zero multiplier must floor at free rather than invert the price into a refund.
	F.Definition->WarPriceMultiplier = 0.f;
	TestEqual(TEXT("A zero multiplier makes the price free, never negative"),
		F.Place->GetGuardRecruitmentCostFor(Buyer, 1), 0);
	F.Definition->WarPriceMultiplier = 1.5f;

	F.Diplomacy->SetDiplomacyState(F.Bandits, F.Heroes, EDiplomacyState::None);
	F.Definition->bAttitudeAffectsPrices = false;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardDecisionIsObservable,
	"TerritoryFramework.Guards.Regression.DecisionReasonIsObservable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The reported bug is a guard standing still - or chasing the wrong actor - and the reason was
// only discoverable by reading thirteen return statements. EvaluateTerritoryTarget already
// computes a human-readable reason for every one of its exits; this test holds the plugin to
// actually writing that reason to the log, so the next occurrence is a one-line answer.
bool FTFGuardDecisionIsObservable::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	TerritoryGuardResponseTests::FFixture F;
	auto* Guard = F.Character(F.Bandits);
	auto* Target = F.Player();
	if (!TestNotNull(TEXT("Real Narrative player target"), Target)) return false;
	FindFProperty<FObjectPropertyBase>(Guard->GetClass(), TEXT("OwningTerritory"))
		->SetObjectPropertyValue_InContainer(Guard, F.Place);

	auto* Settings = GetMutableDefault<UTerritoryDeveloperSettings>();
	FText Reason;

	// Negative control. The master gate is forced off rather than asserted off, so this stays a
	// statement about behaviour on any project - including one whose config turns debug on.
	{
		TGuardValue<bool> Debug(Settings->bEnableDebug, false);
		TerritoryGuardResponseTests::FTerritoryLogCapture Capture;
		GLog->AddOutputDevice(&Capture);
		ON_SCOPE_EXIT { GLog->RemoveOutputDevice(&Capture); };

		TestFalse(TEXT("A neutral visitor is refused"), Guard->EvaluateTerritoryTarget(Target, Reason));
		GLog->Flush();
		TestFalse(TEXT("Even a refusal carries a human-readable reason"), Reason.IsEmpty());
		TestEqual(TEXT("With the debug system off the plugin logs no guard decision"),
			Capture.CountContaining(Reason.ToString()), 0);
	}

	{
		TGuardValue<bool> Debug(Settings->bEnableDebug, true);
		TGuardValue<bool> Combat(Settings->bDebugCombat, true);
		TerritoryGuardResponseTests::FTerritoryLogCapture Capture;
		GLog->AddOutputDevice(&Capture);
		ON_SCOPE_EXIT { GLog->RemoveOutputDevice(&Capture); };

		TestFalse(TEXT("The same neutral visitor is refused with debug on"),
			Guard->EvaluateTerritoryTarget(Target, Reason));
		GLog->Flush();
		const FString Refusal = Reason.ToString();
		TestEqual(TEXT("The refusal reason reaches the log exactly once"),
			Capture.CountContaining(Refusal), 1);
		TestEqual(TEXT("The logged line names the guard that decided"),
			Capture.CountContaining(Guard->GetName()), 1);
		TestEqual(TEXT("The logged line names the actor it decided about"),
			Capture.CountContaining(Target->GetName()), 1);

		// The real runtime path is GetTeamAttitudeTowards -> CanEngageTerritoryTarget -> this
		// same decision. One log site serves both, which is what keeps the change small: it is
		// in the single place every exit already funnels through, not repeated at thirteen exits.
		TestFalse(TEXT("The runtime wrapper refuses too"), Guard->CanEngageTerritoryTarget(Target));
		GLog->Flush();
		TestEqual(TEXT("The runtime entry point reports the same decision through the same line"),
			Capture.CountContaining(Refusal), 2);

		// The other direction matters just as much: the reported bug is a guard engaging an
		// actor it should have ignored, so an allow must be as visible as a refusal.
		F.Diplomacy->DeclareWar(F.Bandits, F.Heroes);
		F.Evidence(Target);
		TestTrue(TEXT("A confirmed enemy at War is engaged"),
			Guard->EvaluateTerritoryTarget(Target, Reason));
		GLog->Flush();
		TestEqual(TEXT("The allow reason reaches the log too"),
			Capture.CountContaining(Reason.ToString()), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFGuardControllerAttitude,
	"TerritoryFramework.Guards.Regression.ControllerAndCharacterShareOneAttitude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * A guard has TWO Narrative team agents: ATerritoryGuardCharacter and the ATerritoryNPCController
 * possessing it. The inherited controller implementation answers from the raw faction table, and
 * UArsenalStatics::GetAttitude - the Blueprint-facing helper read by UEnvQueryTest_Team - answers
 * for whichever of the two it is handed. A consumer handed the controller therefore never sees the
 * gate at all.
 *
 * Measured in live PIE before this test existed (HopDistrictTest, seven guards on the
 * authoritative Blacksmith volume): the raw table said HOSTILE, the guard pawn said NEUTRAL (its
 * gates refused, and the downgrade at TerritoryGuardCharacter.cpp:273 is deliberate), and the
 * controller said HOSTILE. Two answers from one NPC - and the controller's answer bypassed every
 * Territory gate, and the downgrade with it.
 *
 * The attitude is read through the team-agent interface in every assertion here, because that is
 * how the shipped consumers reach it and because both base classes declare the name. A C++
 * cross-check through UArsenalStatics is not available to this module: its public header includes
 * Vehicles/NarrativeArsenalVehicleTypes.h, which exists only in the vendor module's Private tree,
 * so including it from here fails to compile. The interface call is the same virtual the helper
 * makes, so nothing is lost by asserting on it directly.
 */
bool FTFGuardControllerAttitude::RunTest(const FString& Parameters)
{
	TGuardValue<bool> Callbacks(GAllowActorScriptExecutionInEditor, true);
	TerritoryGuardResponseTests::FFixture F;
	auto* State = Cast<ANarrativeGameState>(F.World->GetGameState());
	if (!TestNotNull(TEXT("Authored GameState owns the Narrative faction table"), State)) return false;
	auto* Guard = F.Character(F.Bandits);
	auto* Target = F.Player();
	if (!TestNotNull(TEXT("Real Narrative player target"), Target)) return false;
	FindFProperty<FObjectPropertyBase>(Guard->GetClass(), TEXT("OwningTerritory"))
		->SetObjectPropertyValue_InContainer(Guard, F.Place);

	// Runtime supplies this controller through AIControllerClass. An isolated world has not run the
	// possession, so take whichever controller the guard actually has and only make one if it has
	// none - the test must describe the guard that exists, not one it assembled to suit itself.
	auto* Controller = Cast<ATerritoryNPCController>(Guard->GetController());
	if (!Controller)
	{
		Controller = F.World->SpawnActor<ATerritoryNPCController>();
		if (!TestNotNull(TEXT("Territory NPC controller spawns"), Controller)) return false;
		F.World->AddController(Controller);
		Controller->Possess(Guard);
	}
	if (!TestNotNull(TEXT("The guard is controlled by the Territory NPC controller"), Controller)) return false;
	TestTrue(TEXT("The guard reports the Territory controller as its controller"),
		Guard->GetController() == static_cast<AController*>(Controller));

	auto* GuardTeam = Cast<INarrativeTeamAgentInterface>(Guard);
	auto* ControllerTeam = Cast<INarrativeTeamAgentInterface>(Controller);
	auto* TargetTeam = Cast<INarrativeTeamAgentInterface>(Target);
	if (!TestNotNull(TEXT("The guard is a team agent"), GuardTeam)
		|| !TestNotNull(TEXT("The controller is a team agent"), ControllerTeam)
		|| !TestNotNull(TEXT("The target is a team agent"), TargetTeam)) return false;

	// Validity of the whole scenario, asserted rather than assumed. An empty faction set makes the
	// raw table answer Neutral - which is also what a gated pawn answers - so without this the
	// comparison below would pass for entirely the wrong reason.
	TestTrue(TEXT("The controller reports the possessed guard's factions, so the table is consulted with them"),
		ControllerTeam->GetFactions().HasTag(F.Bandits));
	TestTrue(TEXT("The target reports its own faction"),
		TargetTeam->GetFactions().HasTag(F.Heroes));

	State->SetFactionAttitude(F.Bandits, F.Heroes, ETeamAttitude::Hostile);
	TestEqual(TEXT("The raw Narrative faction table really does call this pair Hostile"),
		static_cast<int32>(State->GetFactionsAttitudeTowardsFactions(
			GuardTeam->GetFactions(), TargetTeam->GetFactions())),
		static_cast<int32>(ETeamAttitude::Hostile));

	// Territory refuses on its own authorities - diplomacy, stealth, territory state - none of which
	// is the Narrative faction table. The guarded pawn therefore reports Neutral on purpose.
	FText Reason;
	TestFalse(TEXT("No Territory gate admits this target"), Guard->EvaluateTerritoryTarget(Target, Reason));
	const ETeamAttitude::Type PawnAttitude = GuardTeam->GetTeamAttitudeTowards(*Target);
	TestEqual(TEXT("The guard pawn refuses stale Narrative hostility"),
		static_cast<int32>(PawnAttitude), static_cast<int32>(ETeamAttitude::Neutral));

	// One NPC, one answer. This is the assertion that failed before the controller forwarded.
	TestEqual(TEXT("The controller reports the guard's own attitude, not the raw faction table"),
		static_cast<int32>(ControllerTeam->GetTeamAttitudeTowards(*Target)),
		static_cast<int32>(ETeamAttitude::Neutral));
	TestEqual(TEXT("The controller and the guard agree"),
		static_cast<int32>(ControllerTeam->GetTeamAttitudeTowards(*Target)),
		static_cast<int32>(PawnAttitude));
	return true;
}

#endif
