#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryVolume.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Economy/TerritoryFactionResourceAccountComponent.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TerritoryDiplomacyCondition.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "Tales/TerritoryStoryConditions.h"
#include "Tales/TerritoryStoryEvents.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/GameModeBase.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace TerritoryNarrativeMigrationTests
{
	UBlueprint* LoadBlueprint(const TCHAR* PackagePath)
	{
		return LoadObject<UBlueprint>(nullptr, PackagePath);
	}

	TArray<UK2Node_CallFunction*> FindCalls(UBlueprint* Blueprint, const FName FunctionName)
	{
		TArray<UK2Node_CallFunction*> Calls;
		if (!Blueprint) return Calls;

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!Graph) continue;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				if (Call && Call->FunctionReference.GetMemberName() == FunctionName)
				{
					Calls.Add(Call);
				}
			}
		}
		return Calls;
	}

	bool AnyProjectFixtureExists(const TConstArrayView<const TCHAR*> PackageNames)
	{
		for (const TCHAR* PackageName : PackageNames)
		{
			if (FPackageName::DoesPackageExist(PackageName))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFNarrativePro242MigrationContract,
	"TerritoryFramework.Integration.NarrativePro242MigrationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFNarrativePro242MigrationContract::RunTest(const FString& Parameters)
{
	using namespace TerritoryNarrativeMigrationTests;
	static const TCHAR* ProjectFixturePackages[] = {
		TEXT("/Game/TerritoryFramework/Framework/BP_TerritoryPlayerCharacter"),
		TEXT("/Game/TerritoryFramework/Framework/BP_TerritoryGameMode"),
		TEXT("/Game/TerritoryFramework/Framework/Controller_Reworked/BP_HopNarrativePlayerController_DemoMap"),
		TEXT("/Game/TerritoryFramework/UI/WBP_TerritoryGameplayHUD_Modular"),
		TEXT("/Game/TerritoryFramework/AI/BP_TerritoryGuard"),
		TEXT("/Game/TerritoryFramework/Blueprints/BP_Property_Blacksmith"),
		TEXT("/Game/TerritoryFramework/AI/BPA_ReturnToTerritory")
	};
	if (!AnyProjectFixtureExists(ProjectFixturePackages))
	{
		AddInfo(TEXT("Skipped optional TDA Narrative Pro migration fixture; no project integration assets are installed."));
		return true;
	}

	UBlueprint* VendorPlayer = LoadBlueprint(
		TEXT("/NarrativePro/Pro/Core/Character/BP/BP_NarrativePlayer_GASP.BP_NarrativePlayer_GASP"));
	UBlueprint* ProjectPlayer = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/Framework/BP_TerritoryPlayerCharacter.BP_TerritoryPlayerCharacter"));
	UBlueprint* TerritoryGameMode = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/Framework/BP_TerritoryGameMode.BP_TerritoryGameMode"));
	UBlueprint* ProjectController = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/Framework/Controller_Reworked/BP_HopNarrativePlayerController_DemoMap.BP_HopNarrativePlayerController_DemoMap"));
	UBlueprint* ProjectGameplayHUD = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/UI/WBP_TerritoryGameplayHUD_Modular.WBP_TerritoryGameplayHUD_Modular"));

	TestNotNull(TEXT("Narrative Pro player Blueprint loads"), VendorPlayer);
	TestNotNull(TEXT("Project-owned Territory player Blueprint loads"), ProjectPlayer);
	TestNotNull(TEXT("Territory GameMode Blueprint loads"), TerritoryGameMode);
	TestNotNull(TEXT("Project Narrative controller Blueprint loads"), ProjectController);
	TestNotNull(TEXT("Project Territory GameplayHUD Blueprint loads"), ProjectGameplayHUD);
	if (!VendorPlayer || !ProjectPlayer || !TerritoryGameMode || !ProjectController
		|| !ProjectGameplayHUD) return false;

	TestFalse(TEXT("Vendor player has no Territory ownership contract"),
		VendorPlayer->GeneratedClass->ImplementsInterface(UTerritoryOwnershipInterface::StaticClass()));
	TestTrue(TEXT("Project player derives from the Marketplace player"),
		ProjectPlayer->GeneratedClass->IsChildOf(VendorPlayer->GeneratedClass));
	TestFalse(TEXT("Project player does not implement the Territory ownership contract"),
		ProjectPlayer->GeneratedClass->ImplementsInterface(UTerritoryOwnershipInterface::StaticClass()));
	TestTrue(TEXT("Project player retains Narrative faction identity"),
		ProjectPlayer->GeneratedClass->ImplementsInterface(UNarrativeTeamAgentInterface::StaticClass()));

	const AGameModeBase* GameModeCDO = Cast<AGameModeBase>(
		TerritoryGameMode->GeneratedClass->GetDefaultObject());
	TestNotNull(TEXT("Territory GameMode CDO exists"), GameModeCDO);
	if (GameModeCDO)
	{
		TestTrue(TEXT("Territory GameMode uses the project-owned player"),
			GameModeCDO->DefaultPawnClass.Get() == ProjectPlayer->GeneratedClass.Get());
		TestTrue(TEXT("Territory GameMode uses the project Narrative controller"),
			GameModeCDO->PlayerControllerClass.Get() == ProjectController->GeneratedClass.Get());
	}

	const USimpleConstructionScript* ControllerSCS = ProjectController->SimpleConstructionScript;
	TestNotNull(TEXT("Project controller construction script exists"), ControllerSCS);
	if (ControllerSCS)
	{
		bool bHasManagement = false;
		bool bHasResourceAccount = false;
		for (const USCS_Node* Node : ControllerSCS->GetAllNodes())
		{
			const UClass* ComponentClass = Node ? Node->ComponentClass : nullptr;
			bHasManagement |= ComponentClass
				&& ComponentClass->IsChildOf(UTerritoryPlayerManagementComponent::StaticClass());
			bHasResourceAccount |= ComponentClass
				&& ComponentClass->IsChildOf(UTerritoryFactionResourceAccountComponent::StaticClass());
		}
		TestTrue(TEXT("Project controller owns Territory management"), bHasManagement);
		TestTrue(TEXT("Project controller owns the Narrative-backed resource account adapter"),
			bHasResourceAccount);
	}

	const FClassProperty* GameplayHUDClassProperty = FindFProperty<FClassProperty>(
		ProjectController->GeneratedClass, TEXT("GameplayHUDClass"));
	TestNotNull(TEXT("Narrative controller exposes its GameplayHUD class contract"),
		GameplayHUDClassProperty);
	if (GameplayHUDClassProperty)
	{
		const UClass* ConfiguredGameplayHUD = Cast<UClass>(
			GameplayHUDClassProperty->GetObjectPropertyValue_InContainer(
				ProjectController->GeneratedClass->GetDefaultObject()));
		TestTrue(TEXT("Project controller selects the modular Territory GameplayHUD"),
			ConfiguredGameplayHUD == ProjectGameplayHUD->GeneratedClass.Get());
	}

	const TArray<UK2Node_CallFunction*> RegisterLayerCalls =
		FindCalls(ProjectGameplayHUD, TEXT("RegisterLayer"));
	TestEqual(TEXT("Territory GameplayHUD registers exactly the Narrative game, menu, and modal layers"),
		RegisterLayerCalls.Num(), 3);

	TMap<FString, FName> RegisteredLayers;
	for (const UK2Node_CallFunction* RegisterLayerCall : RegisterLayerCalls)
	{
		const UEdGraphPin* ExecutePin = RegisterLayerCall->GetExecPin();
		TestTrue(TEXT("Every layer registration is part of the initialized execution chain"),
			ExecutePin && ExecutePin->LinkedTo.Num() == 1);

		const UEdGraphPin* LayerTagPin = RegisterLayerCall->FindPin(TEXT("LayerTag"));
		const UEdGraphPin* LayerWidgetPin = RegisterLayerCall->FindPin(TEXT("LayerWidget"));
		const UK2Node_VariableGet* LayerWidgetGet = LayerWidgetPin
			&& LayerWidgetPin->LinkedTo.Num() == 1
			? Cast<UK2Node_VariableGet>(LayerWidgetPin->LinkedTo[0]->GetOwningNode()) : nullptr;
		TestNotNull(TEXT("Every Narrative layer registration uses a concrete widget stack"),
			LayerWidgetGet);
		if (LayerTagPin && LayerWidgetGet)
		{
			RegisteredLayers.Add(LayerTagPin->DefaultValue,
				LayerWidgetGet->VariableReference.GetMemberName());
		}
	}

	TestEqual(TEXT("UI.Layer.Game is registered to GameStack"),
		RegisteredLayers.FindRef(TEXT("(TagName=\"UI.Layer.Game\")")), FName(TEXT("GameStack")));
	TestEqual(TEXT("UI.Layer.Menu is registered to MenuStack"),
		RegisteredLayers.FindRef(TEXT("(TagName=\"UI.Layer.Menu\")")), FName(TEXT("MenuStack")));
	TestEqual(TEXT("UI.Layer.Modal is registered to ModalStack"),
		RegisteredLayers.FindRef(TEXT("(TagName=\"UI.Layer.Modal\")")), FName(TEXT("ModalStack")));
	TestNotNull(TEXT("Territory capture presentation remains composed into the Narrative HUD"),
		FindFProperty<FObjectPropertyBase>(ProjectGameplayHUD->GeneratedClass,
			TEXT("TerritoryCaptureHUD")));

	UBlueprint* TerritoryGuard = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/AI/BP_TerritoryGuard.BP_TerritoryGuard"));
	TestNotNull(TEXT("Territory guard Blueprint loads"), TerritoryGuard);
	if (TerritoryGuard)
	{
		UK2Node_CallParentFunction* ParentDeath = nullptr;
		for (UK2Node_CallFunction* Call : FindCalls(TerritoryGuard, TEXT("HandleDeath")))
		{
			ParentDeath = Cast<UK2Node_CallParentFunction>(Call);
			if (ParentDeath) break;
		}
		TestNotNull(TEXT("Guard death override calls the Territory parent"), ParentDeath);
		if (ParentDeath)
		{
			const UEdGraphPin* DeathStatePin = ParentDeath->FindPin(TEXT("bIsDead"));
			TestTrue(TEXT("Narrative 2.4 death state is wired into the parent call"),
				DeathStatePin && DeathStatePin->LinkedTo.Num() == 1);
		}

		const TArray<UK2Node_CallFunction*> RemoveGoals =
			FindCalls(TerritoryGuard, TEXT("RemoveAllGoals"));
		TestEqual(TEXT("Guard death has one activity cleanup call"), RemoveGoals.Num(), 1);
		if (RemoveGoals.Num() == 1)
		{
			const UEdGraphPin* ExecutePin = RemoveGoals[0]->GetExecPin();
			UK2Node_IfThenElse* ValidityBranch = ExecutePin && ExecutePin->LinkedTo.Num() == 1
				? Cast<UK2Node_IfThenElse>(ExecutePin->LinkedTo[0]->GetOwningNode()) : nullptr;
			TestNotNull(TEXT("Activity cleanup is guarded by a validity branch"), ValidityBranch);
			if (ValidityBranch)
			{
				const UEdGraphPin* ConditionPin = ValidityBranch->GetConditionPin();
				const UK2Node_CallFunction* ValidityCall =
					ConditionPin && ConditionPin->LinkedTo.Num() == 1
					? Cast<UK2Node_CallFunction>(ConditionPin->LinkedTo[0]->GetOwningNode()) : nullptr;
				TestTrue(TEXT("Activity cleanup validates the asynchronous component"),
					ValidityCall && ValidityCall->FunctionReference.GetMemberName() == TEXT("IsValid"));
			}
		}
	}

	UBlueprint* Blacksmith = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/Blueprints/BP_Property_Blacksmith.BP_Property_Blacksmith"));
	TestNotNull(TEXT("Blacksmith property Blueprint loads"), Blacksmith);
	if (Blacksmith)
	{
		TestEqual(TEXT("Guard death never bypasses physical capture with ForceCapture"),
			FindCalls(Blacksmith, GET_FUNCTION_NAME_CHECKED(UTerritoryControlSubsystem, ForceCapture)).Num(), 0);
		TestEqual(TEXT("Guard death never bypasses physical capture through the Blueprint library"),
			FindCalls(Blacksmith, TEXT("ForceCaptureTerritory")).Num(), 0);
	}

	UBlueprint* ReturnActivity = LoadBlueprint(
		TEXT("/Game/TerritoryFramework/AI/BPA_ReturnToTerritory.BPA_ReturnToTerritory"));
	TestNotNull(TEXT("Return-to-Territory activity loads"), ReturnActivity);
	if (ReturnActivity)
	{
		TestTrue(TEXT("Return-to-Territory compiles after Narrative 2.4 cinematic migration"),
			ReturnActivity->Status != BS_Error);
		TestEqual(TEXT("Return activity does not select player controller index zero"),
			FindCalls(ReturnActivity, TEXT("GetPlayerController")).Num(), 0);

		const TArray<UK2Node_CallFunction*> GameplayEvents =
			FindCalls(ReturnActivity, TEXT("SendGameplayEventToActor"));
		TestEqual(TEXT("Return activity sends one weapon-sheathe gameplay event"),
			GameplayEvents.Num(), 1);
		if (GameplayEvents.Num() == 1)
		{
			const UEdGraphPin* ExecutePin = GameplayEvents[0]->GetExecPin();
			const UK2Node_IfThenElse* ValidityBranch = ExecutePin
				&& ExecutePin->LinkedTo.Num() == 1
				? Cast<UK2Node_IfThenElse>(ExecutePin->LinkedTo[0]->GetOwningNode()) : nullptr;
			TestNotNull(TEXT("Return activity validates OwnerController before the gameplay event"),
				ValidityBranch);
			if (ValidityBranch)
			{
				const UEdGraphPin* ConditionPin = ValidityBranch->GetConditionPin();
				const UK2Node_CallFunction* ValidityCall = ConditionPin
					&& ConditionPin->LinkedTo.Num() == 1
					? Cast<UK2Node_CallFunction>(ConditionPin->LinkedTo[0]->GetOwningNode()) : nullptr;
				TestTrue(TEXT("Return activity uses the engine IsValid check"),
					ValidityCall
					&& ValidityCall->FunctionReference.GetMemberName() == TEXT("IsValid"));
				const UEdGraphPin* ObjectPin = ValidityCall
					? ValidityCall->FindPin(TEXT("Object")) : nullptr;
				const UK2Node_VariableGet* OwnerControllerGet = ObjectPin
					&& ObjectPin->LinkedTo.Num() == 1
					? Cast<UK2Node_VariableGet>(ObjectPin->LinkedTo[0]->GetOwningNode()) : nullptr;
				TestTrue(TEXT("Return activity validates the actual OwnerController"),
					OwnerControllerGet
					&& OwnerControllerGet->VariableReference.GetMemberName()
						== FName(TEXT("OwnerController")));
			}
		}

		const TArray<UK2Node_CallFunction*> CreatePlayers =
			FindCalls(ReturnActivity, TEXT("CreateNarrativeLevelSequencePlayer"));
		TestEqual(TEXT("Return activity has one Narrative sequence player call"), CreatePlayers.Num(), 1);
		if (CreatePlayers.Num() == 1)
		{
			const UEdGraphPin* PlayersPin = CreatePlayers[0]->FindPin(TEXT("Players"));
			TestTrue(TEXT("Narrative sequence Players input is explicitly wired"),
				PlayersPin && PlayersPin->LinkedTo.Num() == 1
				&& Cast<UK2Node_VariableGet>(PlayersPin->LinkedTo[0]->GetOwningNode()) != nullptr);
		}

		const FArrayProperty* ViewersProperty = FindFProperty<FArrayProperty>(
			ReturnActivity->GeneratedClass, TEXT("IdleSequenceViewers"));
		TestNotNull(TEXT("Return activity owns an explicit viewer array"), ViewersProperty);
		if (ViewersProperty)
		{
			const void* ViewersValue = ViewersProperty->ContainerPtrToValuePtr<void>(
				ReturnActivity->GeneratedClass->GetDefaultObject());
			const FScriptArrayHelper Viewers(ViewersProperty, ViewersValue);
			TestEqual(TEXT("Empty viewer array uses Narrative relevancy for all players"), Viewers.Num(), 0);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCounterAttackMapConfigurationRegression,
	"TerritoryFramework.Integration.CounterAttackMapConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCounterAttackMapConfigurationRegression::RunTest(const FString& Parameters)
{
	if (!FPackageName::DoesPackageExist(TEXT("/Game/HopDistrictTest")))
	{
		AddInfo(TEXT("Skipped optional TDA counterattack map fixture; /Game/HopDistrictTest is not installed."));
		return true;
	}

	UWorld* World = LoadObject<UWorld>(nullptr,
		TEXT("/Game/HopDistrictTest.HopDistrictTest"));
	TestNotNull(TEXT("Territory integration map loads"), World);
	if (!World || !World->PersistentLevel) return false;

	ATerritoryVolume* Blacksmith = nullptr;
	ATerritoryVolume* Farm = nullptr;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		ATerritoryVolume* Territory = Cast<ATerritoryVolume>(Actor);
		if (!Territory) continue;
		const FString Tag = Territory->GetTerritoryTag().ToString();
		if (Tag == TEXT("Territory.HavenReach.MarketSquare.Blacksmith")) Blacksmith = Territory;
		if (Tag == TEXT("Territory.HavenReach.CastleHill.Farm")) Farm = Territory;
	}
	TestNotNull(TEXT("Blacksmith Property exists by stable tag"), Blacksmith);
	TestNotNull(TEXT("Farm Property exists by stable tag"), Farm);
	if (!Blacksmith || !Farm) return false;

	TestNotNull(TEXT("Blacksmith has a physical counterattack profile"),
		Blacksmith->GetCounterAttackProfile());
	TestNotNull(TEXT("Farm has a physical counterattack profile"),
		Farm->GetCounterAttackProfile());

	const FTerritoryAssaultApproach* BlacksmithRoute =
		Blacksmith->GetCounterAttackApproaches().FindByPredicate(
			[](const FTerritoryAssaultApproach& Route)
			{
				return Route.bEnabled && Route.ApproachID == TEXT("Blacksmith_WestRoad");
			});
	const FTerritoryAssaultApproach* FarmRoute =
		Farm->GetCounterAttackApproaches().FindByPredicate(
			[](const FTerritoryAssaultApproach& Route)
			{
				return Route.bEnabled && Route.ApproachID == TEXT("Farm_WestField");
			});
	TestNotNull(TEXT("Blacksmith owns its stable enabled road approach"), BlacksmithRoute);
	TestNotNull(TEXT("Farm owns its stable enabled road approach"), FarmRoute);
	if (BlacksmithRoute)
	{
		const FVector WorldSpawn = (BlacksmithRoute->RelativeSpawnTransform
			* Blacksmith->GetActorTransform()).GetLocation();
		TestTrue(TEXT("Blacksmith route remains on the repaired nav-side point"),
			WorldSpawn.Equals(FVector(-3952.f, 795.4146f, 3.0717f), 1.f));
	}
	if (FarmRoute)
	{
		const FVector WorldSpawn = (FarmRoute->RelativeSpawnTransform
			* Farm->GetActorTransform()).GetLocation();
		TestTrue(TEXT("Farm route remains on its validated west-field point"),
			WorldSpawn.Equals(FVector(2200.f, 0.f, 3.0717f), 1.f));
	}

	const FMapProperty* StateConfigsProperty = FindFProperty<FMapProperty>(
		ATerritoryVolume::StaticClass(), TEXT("StateConfigs"));
	TestNotNull(TEXT("State Config map is reflected"), StateConfigsProperty);
	if (!StateConfigsProperty) return false;
	auto GetStateConfigs = [StateConfigsProperty](ATerritoryVolume* Territory)
	{
		return StateConfigsProperty->ContainerPtrToValuePtr<
			TMap<ETerritoryState, FTerritoryStateConfig>>(Territory);
	};

	const FTerritoryStateConfig* ClaimedConfig =
		GetStateConfigs(Blacksmith)->Find(ETerritoryState::Claimed);
	const FTerritoryStateConfig* ContestedConfig =
		GetStateConfigs(Blacksmith)->Find(ETerritoryState::Contested);
	const UTerritorySetDiplomacyEvent* ClaimedDiplomacyEvent = nullptr;
	const UTerritorySetDiplomacyEvent* ContestedDiplomacyEvent = nullptr;
	const UNarrativeEvent* GiveXPEvent = nullptr;
	if (ClaimedConfig)
	{
		for (const TObjectPtr<UNarrativeEvent>& Event : ClaimedConfig->EntryEvents)
		{
			if (Event && Event->GetClass()->GetName() == TEXT("NE_GiveXP_C"))
			{
				GiveXPEvent = Event;
			}
			if (const UTerritorySetDiplomacyEvent* Typed =
				Cast<UTerritorySetDiplomacyEvent>(Event))
			{
				ClaimedDiplomacyEvent = Typed;
			}
		}
	}
	if (ContestedConfig)
	{
		for (const TObjectPtr<UNarrativeEvent>& Event : ContestedConfig->EntryEvents)
		{
			if (const UTerritorySetDiplomacyEvent* Typed =
				Cast<UTerritorySetDiplomacyEvent>(Event))
			{
				ContestedDiplomacyEvent = Typed;
				break;
			}
		}
	}
	TestNotNull(TEXT("Claimed entry still owns its Narrative XP event"), GiveXPEvent);
	const UTerritoryEventContextCondition* RewardContext = nullptr;
	if (GiveXPEvent)
	{
		for (const TObjectPtr<UNarrativeCondition>& Condition : GiveXPEvent->Conditions)
		{
			if (const UTerritoryEventContextCondition* Typed =
				Cast<UTerritoryEventContextCondition>(Condition))
			{
				RewardContext = Typed;
				break;
			}
		}
	}
	TestNotNull(TEXT("Blacksmith XP is gated by an explicit event context"), RewardContext);
	if (RewardContext)
	{
		TestTrue(TEXT("XP requires a target pawn"), RewardContext->bRequireTargetPawn);
		TestTrue(TEXT("XP requires a player-controlled target"),
			RewardContext->bRequirePlayerControlledTarget);
		TestTrue(TEXT("XP requires a valid target ability system"),
			RewardContext->bRequireAbilitySystemComponent);
	}
	TestNotNull(TEXT("Claimed entry uses the reusable diplomacy Narrative event"),
		ClaimedDiplomacyEvent);
	if (ClaimedDiplomacyEvent)
	{
		TestEqual(TEXT("Claimed entry keeps Heroes and Bandits Neutral"),
			ClaimedDiplomacyEvent->NewState, EDiplomacyState::None);
		TestTrue(TEXT("Claimed diplomacy policy applies on a fresh already-active state"),
			ClaimedDiplomacyEvent->bApplyWhenStateStartsActive);
		TestEqual(TEXT("Claimed diplomacy first party follows the current owner"),
			ClaimedDiplomacyEvent->FactionASource,
			ETerritoryDiplomacyFactionSource::CurrentOwningFaction);
		TestEqual(TEXT("Claimed diplomacy second party follows the previous owner"),
			ClaimedDiplomacyEvent->FactionBSource,
			ETerritoryDiplomacyFactionSource::PreviousOwningFaction);
		TestEqual(TEXT("Claimed initial-state fallback first party is Bandits"),
			ClaimedDiplomacyEvent->FactionA.ToString(),
			FString(TEXT("Narrative.Factions.Bandits")));
		TestEqual(TEXT("Claimed initial-state fallback second party is Heroes"),
			ClaimedDiplomacyEvent->FactionB.ToString(),
			FString(TEXT("Narrative.Factions.Heroes")));
		TestTrue(TEXT("Claimed diplomacy requires the live owner in its pair"),
			ClaimedDiplomacyEvent->bRequireContainingTerritoryOwner);
		TestTrue(TEXT("Claimed peace preserves other active Place conflicts"),
			ClaimedDiplomacyEvent->bPreserveOtherActiveTerritoryWars);
	}
	TestNotNull(TEXT("Contested entry explicitly declares the Territory War"),
		ContestedDiplomacyEvent);
	if (ContestedDiplomacyEvent)
	{
		TestEqual(TEXT("Contested entry changes Heroes and Bandits to War"),
			ContestedDiplomacyEvent->NewState, EDiplomacyState::War);
		TestEqual(TEXT("Contested diplomacy first party follows the defending owner"),
			ContestedDiplomacyEvent->FactionASource,
			ETerritoryDiplomacyFactionSource::CurrentOwningFaction);
		TestEqual(TEXT("Contested diplomacy second party follows the attacker"),
			ContestedDiplomacyEvent->FactionBSource,
			ETerritoryDiplomacyFactionSource::ContestingFaction);
		TestEqual(TEXT("Contested fallback first party is Bandits"),
			ContestedDiplomacyEvent->FactionA.ToString(),
			FString(TEXT("Narrative.Factions.Bandits")));
		TestEqual(TEXT("Contested fallback second party is Heroes"),
			ContestedDiplomacyEvent->FactionB.ToString(),
			FString(TEXT("Narrative.Factions.Heroes")));
		TestTrue(TEXT("Contested diplomacy rejects stale unrelated pairs"),
			ContestedDiplomacyEvent->bRequireContainingTerritoryOwner);
	}

	const FTerritoryStateConfig* LockedConfig =
		GetStateConfigs(Farm)->Find(ETerritoryState::Locked);
	const UTerritoryDiplomacyCondition* WarCondition = nullptr;
	if (LockedConfig)
	{
		for (const TObjectPtr<UNarrativeCondition>& Condition : LockedConfig->ExitConditions)
		{
			if (const UTerritoryDiplomacyCondition* Typed =
				Cast<UTerritoryDiplomacyCondition>(Condition))
			{
				WarCondition = Typed;
				break;
			}
		}
	}
	TestNotNull(TEXT("Farm Locked exit uses the reusable diplomacy Narrative condition"),
		WarCondition);
	if (WarCondition)
	{
		TestEqual(TEXT("Farm unlock requires the rich War state"),
			WarCondition->RequiredState, EDiplomacyState::War);
	}

	const FArrayProperty* DefeatedEventsProperty = FindFProperty<FArrayProperty>(
		ATerritoryVolume::StaticClass(), TEXT("AllDefendersDefeatedEvents"));
	TestNotNull(TEXT("All-defenders-defeated Narrative hook is reflected"),
		DefeatedEventsProperty);
	if (DefeatedEventsProperty)
	{
		const TArray<TObjectPtr<UNarrativeEvent>>* Events =
			DefeatedEventsProperty->ContainerPtrToValuePtr<
				TArray<TObjectPtr<UNarrativeEvent>>>(Blacksmith);
		const UTerritoryScheduleEnemyWaveEvent* WaveEvent = nullptr;
		if (Events)
		{
			for (const TObjectPtr<UNarrativeEvent>& Event : *Events)
			{
				if (const UTerritoryScheduleEnemyWaveEvent* Typed =
					Cast<UTerritoryScheduleEnemyWaveEvent>(Event))
				{
					WaveEvent = Typed;
					break;
				}
			}
		}
		TestNotNull(TEXT("Blacksmith schedules a finite wave after all defenders are defeated"),
			WaveEvent);
		if (WaveEvent)
		{
			TestEqual(TEXT("Defeat wave targets Blacksmith"), WaveEvent->TargetTerritory.ToString(),
				FString(TEXT("Territory.HavenReach.MarketSquare.Blacksmith")));
			TestEqual(TEXT("Defeat wave uses Bandits as the exact attacker"),
				WaveEvent->AttackingFaction.ToString(),
				FString(TEXT("Narrative.Factions.Bandits")));
			TestEqual(TEXT("Defeat wave has one inherited Narrative condition"),
				WaveEvent->Conditions.Num(), 1);
			const UTerritoryDiplomacyCondition* WaveDiplomacy = WaveEvent->Conditions.Num() == 1
				? Cast<UTerritoryDiplomacyCondition>(WaveEvent->Conditions[0]) : nullptr;
			TestNotNull(TEXT("Defeat wave is gated by the reusable diplomacy condition"),
				WaveDiplomacy);
			if (WaveDiplomacy)
			{
				TestEqual(TEXT("Defeat wave requires War"), WaveDiplomacy->RequiredState,
					EDiplomacyState::War);
			}
		}
	}

	return true;
}

#endif
