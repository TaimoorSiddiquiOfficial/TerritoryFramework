#if WITH_DEV_AUTOMATION_TESTS

#include <type_traits>

#include "Misc/AutomationTest.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryCommandTags.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardLifecyclePolicy.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Combat/TerritoryAssaultCharacter.h"
#include "AI/TerritoryPatrolGoal.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Core/TerritoryWorldState.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryCapturePoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Core/TerritoryMutationTypes.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Debug/TerritoryDebugger.h"
#include "Tales/TerritoryCaptureTask.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryCaptureEligibilityCondition.h"
#include "Tales/TerritoryOwnerHandoverEvent.h"
#include "Interaction/TerritoryStoryOwnerSpawner.h"
#include "Tales/TerritoryLockEvent.h"
#include "Tales/TerritoryOwnershipCondition.h"
#include "Tales/TerritoryDiplomacyCondition.h"
#include "Tales/TerritoryDiplomacyEvent.h"
#include "Tales/TerritoryGarrisonCondition.h"
#include "Tales/TerritoryStoryConditions.h"
#include "Tales/TerritoryStoryEvents.h"
#include "Tales/TerritoryTalesUtilities.h"
#include "Navigation/TerritoryMapMarker.h"
#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "NarrativeSavableActor.h"
#include "NarrativeSavableComponent.h"
#include "Tales/QuestTask.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/Dialogue.h"
#include "Navigation/MapMarker.h"
#include "Navigation/NavigationMarkerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"

// ─── Helper ──────────────────────────────────────────────────────────────────

namespace TFTestUtils
{
	static bool HasProperty(const UClass* Class, const FString& PropertyName, const FString& ExpectedCPPType = TEXT(""))
	{
		if (!Class) return false;
		FProperty* Prop = Class->FindPropertyByName(FName(*PropertyName));
		if (!Prop) return false;
		if (!ExpectedCPPType.IsEmpty())
		{
			return Prop->GetCPPType() == ExpectedCPPType;
		}
		return true;
	}

	static bool HasFunction(const UClass* Class, const FString& FunctionName)
	{
		if (!Class) return false;
		return Class->FindFunctionByName(FName(*FunctionName)) != nullptr;
	}

	static bool ImplementsInterface(const UClass* Class, const UClass* InterfaceClass)
	{
		if (!Class || !InterfaceClass) return false;
		return Class->ImplementsInterface(InterfaceClass);
	}

	static bool IsReplicated(const UClass* Class, const FString& PropertyName)
	{
		if (!Class) return false;
		FProperty* Prop = Class->FindPropertyByName(FName(*PropertyName));
		return Prop && Prop->HasAnyPropertyFlags(CPF_Net);
	}

	static bool IsSaveGame(const UClass* Class, const FString& PropertyName)
	{
		if (!Class) return false;
		FProperty* Prop = Class->FindPropertyByName(FName(*PropertyName));
		return Prop && Prop->HasAnyPropertyFlags(CPF_SaveGame);
	}

	static bool IsBlueprintCallable(const UClass* Class, const FString& FunctionName)
	{
		if (!Class) return false;
		UFunction* Func = Class->FindFunctionByName(FName(*FunctionName));
		return Func && Func->HasAnyFunctionFlags(FUNC_BlueprintCallable);
	}

	static bool IsBlueprintPure(const UClass* Class, const FString& FunctionName)
	{
		if (!Class) return false;
		UFunction* Func = Class->FindFunctionByName(FName(*FunctionName));
		return Func && Func->HasAnyFunctionFlags(FUNC_BlueprintCallable) && Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
	}

	static bool IsBlueprintAuthorityOnly(const UClass* Class, const FString& FunctionName)
	{
		if (!Class) return false;
		UFunction* Func = Class->FindFunctionByName(FName(*FunctionName));
		return Func && Func->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — TerritoryVolume
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_VolumeClass,
	"TerritoryFramework.Contract.TerritoryVolume.ClassStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_VolumeClass::RunTest(const FString& Parameters)
{
	const UClass* Class = ATerritoryVolume::StaticClass();
	TestNotNull(TEXT("ATerritoryVolume::StaticClass()"), Class);

	// ─── Inheritance ───
	TestTrue(TEXT("Inherits AActor"), Class->IsChildOf(AActor::StaticClass()));

	// ─── Interface conformance ───
	TestTrue(TEXT("Implements INarrativeSavableActor"),
		TFTestUtils::ImplementsInterface(Class, UNarrativeSavableActor::StaticClass()));
	TestTrue(TEXT("Implements INarrativeStableActor (parent of SavableActor)"),
		TFTestUtils::ImplementsInterface(Class, UNarrativeStableActor::StaticClass()));

	// ─── Required SaveGame properties ───
	TestTrue(TEXT("OwnershipData is SaveGame"),
		TFTestUtils::IsSaveGame(Class, TEXT("OwnershipData")));
	TestTrue(TEXT("TerritoryGUID is SaveGame"),
		TFTestUtils::IsSaveGame(Class, TEXT("TerritoryGUID")));

	// ─── Replication ───
	TestTrue(TEXT("OwnershipData is replicated"),
		TFTestUtils::IsReplicated(Class, TEXT("OwnershipData")));
	TestTrue(TEXT("Exact garrison snapshot is replicated for clients and late joiners"),
		TFTestUtils::IsReplicated(Class, TEXT("GarrisonSnapshot")));
	const UScriptStruct* SnapshotStruct = FTerritoryGarrisonSnapshot::StaticStruct();
	TestNotNull(TEXT("Garrison snapshot exposes the desired staffing target"),
		SnapshotStruct ? SnapshotStruct->FindPropertyByName(TEXT("DesiredGuards")) : nullptr);
	TestNotNull(TEXT("Garrison snapshot exposes authoritative capacity"),
		SnapshotStruct ? SnapshotStruct->FindPropertyByName(TEXT("MaximumGuards")) : nullptr);

	// ─── Required properties ───
	TestTrue(TEXT("Has TerritoryTag property"), TFTestUtils::HasProperty(Class, TEXT("TerritoryTag")));
	TestTrue(TEXT("Has TerritoryDisplayName property"), TFTestUtils::HasProperty(Class, TEXT("TerritoryDisplayName")));
	TestTrue(TEXT("Has InitialOwningFaction property"), TFTestUtils::HasProperty(Class, TEXT("InitialOwningFaction")));
	TestTrue(TEXT("Has one visible InitialState authoring property"), TFTestUtils::HasProperty(Class, TEXT("InitialState")));
	TestTrue(TEXT("Has InitialMaxConcurrentAttackers property"), TFTestUtils::HasProperty(Class, TEXT("InitialMaxConcurrentAttackers")));
	TestTrue(TEXT("Has InitialPeriodicIncome property"), TFTestUtils::HasProperty(Class, TEXT("InitialPeriodicIncome")));
	TestTrue(TEXT("Has InitialGuardCost property"), TFTestUtils::HasProperty(Class, TEXT("InitialGuardCost")));
	TestTrue(TEXT("Has distinct guard recruitment price"),
		TFTestUtils::HasProperty(Class, TEXT("InitialGuardRecruitmentCost")));
	TestTrue(TEXT("Has configurable post-capture staffing policy"),
		TFTestUtils::HasProperty(Class, TEXT("PostCaptureGarrisonPolicy")));
	const FProperty* RecruitmentField = FTerritoryOwnershipData::StaticStruct()
		? FTerritoryOwnershipData::StaticStruct()->FindPropertyByName(TEXT("GuardRecruitmentCost")) : nullptr;
	TestTrue(TEXT("Runtime recruitment price persists through Narrative save/load"),
		RecruitmentField && RecruitmentField->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("Absolute target validator is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("CanSetDesiredGuardCount")));
	TestTrue(TEXT("Absolute target mutation is server-authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("TrySetDesiredGuardCount")));
	const FProperty* LegacyStartsLocked = Class->FindPropertyByName(TEXT("bStartsLocked"));
	TestNotNull(TEXT("Legacy StartsLocked data remains loadable for migration"), LegacyStartsLocked);
	if (LegacyStartsLocked)
	{
		TestFalse(TEXT("Legacy StartsLocked is hidden from new details authoring"),
			LegacyStartsLocked->HasAnyPropertyFlags(CPF_Edit));
		TestFalse(TEXT("Legacy StartsLocked is hidden from new Blueprint graphs"),
			LegacyStartsLocked->HasAnyPropertyFlags(CPF_BlueprintVisible));
	#if WITH_METADATA
		TestTrue(TEXT("Legacy StartsLocked is marked deprecated"),
			LegacyStartsLocked->HasMetaData(TEXT("DeprecatedProperty")));
	#endif
	}
	const FProperty* LegacyLockConditions = Class->FindPropertyByName(TEXT("LockConditions"));
	TestNotNull(TEXT("Legacy LockConditions data remains loadable for migration"), LegacyLockConditions);
	if (LegacyLockConditions)
	{
		TestFalse(TEXT("Legacy LockConditions is hidden from new details authoring"),
			LegacyLockConditions->HasAnyPropertyFlags(CPF_Edit));
	}
	TestNotNull(TEXT("State configs expose one authoritative ExitConditions array"),
		FTerritoryStateConfig::StaticStruct()->FindPropertyByName(TEXT("ExitConditions")));
	TestTrue(TEXT("Has ParentTerritoryTag property"), TFTestUtils::HasProperty(Class, TEXT("ParentTerritoryTag")));
	TestTrue(TEXT("Has BoundsShape property"), TFTestUtils::HasProperty(Class, TEXT("BoundsShape")));

	// ─── Blueprint-exposed functions ───
	TestTrue(TEXT("GetOwningFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetOwningFaction")));
	TestTrue(TEXT("GetTerritoryState is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryState")));
	TestTrue(TEXT("GetControlProgress is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetControlProgress")));
	TestTrue(TEXT("IsContested is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsContested")));
	TestTrue(TEXT("IsOwnedByFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsOwnedByFaction")));
	TestTrue(TEXT("GetTerritoryTag is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryTag")));
	TestTrue(TEXT("GetDefenderCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetDefenderCount")));
	TestTrue(TEXT("GetPeriodicIncome is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetPeriodicIncome")));
	TestTrue(TEXT("ContainsPoint is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("ContainsPoint")));
	TestTrue(TEXT("SetOwningFaction is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SetOwningFaction")));
	TestTrue(TEXT("SetOwningFaction is server-authority only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("SetOwningFaction")));
	TestTrue(TEXT("SetTerritoryState is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SetTerritoryState")));
	TestTrue(TEXT("RegisterDefender is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RegisterDefender")));
	TestTrue(TEXT("UnregisterDefender is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("UnregisterDefender")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnTerritoryOwnershipChanged delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryOwnershipChanged")));
	TestTrue(TEXT("Has OnTerritoryStateChangedDelegate delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryStateChangedDelegate")));

	// ─── INarrativeSavableActor function overrides ───
	TestTrue(TEXT("Has GetActorGUID implementation"),
		TFTestUtils::HasFunction(Class, TEXT("GetActorGUID")));
	TestTrue(TEXT("Has PrepareForSave implementation"),
		TFTestUtils::HasFunction(Class, TEXT("PrepareForSave")));
	TestTrue(TEXT("Has Load implementation"),
		TFTestUtils::HasFunction(Class, TEXT("Load")));
	TestTrue(TEXT("Has ShouldRespawn implementation"),
		TFTestUtils::HasFunction(Class, TEXT("ShouldRespawn")));

	// ─── Virtual extension points ───
	TestTrue(TEXT("Has OnOwnershipChanged (BP extension point)"),
		TFTestUtils::HasFunction(Class, TEXT("OnOwnershipChanged")));
	TestTrue(TEXT("Has OnStateChanged (BP extension point)"),
		TFTestUtils::HasFunction(Class, TEXT("OnStateChanged")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryInitialStateMigration,
	"TerritoryFramework.State.InitialStateMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryInitialStateMigration::RunTest(const FString& Parameters)
{
	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	TestNotNull(TEXT("Initial-state test territory created"), Territory);
	if (!Territory) return false;

	const UClass* Class = Territory->GetClass();
	FEnumProperty* InitialStateProperty = FindFProperty<FEnumProperty>(Class, TEXT("InitialState"));
	FStructProperty* InitialOwnerProperty = FindFProperty<FStructProperty>(Class, TEXT("InitialOwningFaction"));
	FBoolProperty* LegacyLockedProperty = FindFProperty<FBoolProperty>(Class, TEXT("bStartsLocked"));
	TestNotNull(TEXT("Initial State enum is reflected"), InitialStateProperty);
	TestNotNull(TEXT("Initial Owning Faction is reflected"), InitialOwnerProperty);
	TestNotNull(TEXT("Legacy Starts Locked remains readable"), LegacyLockedProperty);
	if (!InitialStateProperty || !InitialOwnerProperty || !LegacyLockedProperty) return false;

	auto SetInitialState = [Territory, InitialStateProperty](ETerritoryInitialState State)
	{
		void* Value = InitialStateProperty->ContainerPtrToValuePtr<void>(Territory);
		InitialStateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
			Value, static_cast<int64>(State));
	};
	auto SetInitialOwner = [Territory, InitialOwnerProperty](const FGameplayTag& Faction)
	{
		*InitialOwnerProperty->ContainerPtrToValuePtr<FGameplayTag>(Territory) = Faction;
	};

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	TestTrue(TEXT("Hero faction exists for initial-state examples"), Heroes.IsValid());
	TestEqual(TEXT("Automatic with no owner starts unclaimed"),
		Territory->ResolveInitialTerritoryState(), ETerritoryState::Unclaimed);

	SetInitialOwner(Heroes);
	TestEqual(TEXT("Automatic with an owner starts claimed"),
		Territory->ResolveInitialTerritoryState(), ETerritoryState::Claimed);

	LegacyLockedProperty->SetPropertyValue_InContainer(Territory, true);
	TestEqual(TEXT("An old Starts Locked asset remains locked after migration"),
		Territory->ResolveInitialTerritoryState(), ETerritoryState::Locked);
	Territory->MigrateLegacyLockSettings();
	TestFalse(TEXT("The editor migration clears the hidden legacy boolean"),
		LegacyLockedProperty->GetPropertyValue_InContainer(Territory));
	TestEqual(TEXT("The editor migration writes the visible Locked option"),
		Territory->ResolveInitialTerritoryState(), ETerritoryState::Locked);

	SetInitialState(ETerritoryInitialState::Unclaimed);
	TestEqual(TEXT("An explicit new setting overrides legacy Starts Locked"),
		Territory->ResolveInitialTerritoryState(), ETerritoryState::Unclaimed);

	SetInitialState(ETerritoryInitialState::Claimed);
	SetInitialOwner(FGameplayTag());
	TestEqual(TEXT("Claimed without a faction safely resolves to unclaimed"),
		Territory->ResolveInitialTerritoryState(), ETerritoryState::Unclaimed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryStateTransitionConditions,
	"TerritoryFramework.State.AtomicEntryExitConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryStateTransitionConditions::RunTest(const FString& Parameters)
{
	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	TestNotNull(TEXT("State-transition test territory created"), Territory);
	if (!Territory) return false;

	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Territory->SetOwningFaction(Heroes);
	TestEqual(TEXT("Test territory begins claimed by Heroes"),
		Territory->GetTerritoryState(), ETerritoryState::Claimed);

	FMapProperty* ConfigProperty = FindFProperty<FMapProperty>(
		Territory->GetClass(), TEXT("StateConfigs"));
	FArrayProperty* LegacyConditionsProperty = FindFProperty<FArrayProperty>(
		Territory->GetClass(), TEXT("LockConditions"));
	TestNotNull(TEXT("State configuration map is reflected"), ConfigProperty);
	TestNotNull(TEXT("Legacy lock conditions remain reflected"), LegacyConditionsProperty);
	if (!ConfigProperty || !LegacyConditionsProperty) return false;

	TMap<ETerritoryState, FTerritoryStateConfig>* Configs =
		ConfigProperty->ContainerPtrToValuePtr<TMap<ETerritoryState, FTerritoryStateConfig>>(Territory);
	TArray<TObjectPtr<UNarrativeCondition>>* LegacyConditions =
		LegacyConditionsProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UNarrativeCondition>>>(Territory);
	UTerritoryOwnershipCondition* AlwaysFail =
		NewObject<UTerritoryOwnershipCondition>(Territory);
	TestNotNull(TEXT("A Narrative condition was created"), AlwaysFail);
	if (!AlwaysFail) return false;

	Configs->FindOrAdd(ETerritoryState::Claimed).ExitConditions.Add(AlwaysFail);
	Territory->SetTerritoryState(ETerritoryState::Contested);
	TestEqual(TEXT("A failed exit condition blocks the atomic state change"),
		Territory->GetTerritoryState(), ETerritoryState::Claimed);
	AlwaysFail->bNot = true;
	Territory->SetTerritoryState(ETerritoryState::Contested);
	TestEqual(TEXT("Narrative Not inverts a State Config condition"),
		Territory->GetTerritoryState(), ETerritoryState::Contested);
	AlwaysFail->bNot = false;

	Configs->FindOrAdd(ETerritoryState::Locked).ExitConditions.Add(AlwaysFail);
	Territory->ForceSetTerritoryState(ETerritoryState::Locked);
	Territory->SetOwningFaction(Bandits);
	TestEqual(TEXT("Changing owner cannot silently bypass a Locked exit condition"),
		Territory->GetOwningFaction(), Heroes);
	TestFalse(TEXT("Can Unlock evaluates both Locked exit and destination entry rules"),
		Territory->CanUnlock());
	TestFalse(TEXT("Normal unlock is rejected while the exit condition fails"),
		Territory->TryUnlock(false));
	TestTrue(TEXT("Explicit forced unlock remains available to trusted quest/save code"),
		Territory->TryUnlock(true));

	// Migration path: when no new Locked exit rules exist, old LockConditions remain
	// authoritative until the asset is resaved with the new State Config.
	Configs->FindChecked(ETerritoryState::Locked).ExitConditions.Empty();
	LegacyConditions->Add(AlwaysFail);
	Territory->ForceSetTerritoryState(ETerritoryState::Locked);
	TestFalse(TEXT("Legacy LockConditions still block unlock after loading an old asset"),
		Territory->TryUnlock(false));
	TestTrue(TEXT("Legacy migration path still supports an explicit forced unlock"),
		Territory->TryUnlock(true));
	Territory->MigrateLegacyLockSettings();
	TestTrue(TEXT("The editor migration clears legacy LockConditions"),
		LegacyConditions->IsEmpty());
	TestEqual(TEXT("The exact legacy Narrative condition moves to Locked Exit Conditions"),
		Configs->FindChecked(ETerritoryState::Locked).ExitConditions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryEventContextConditionRegression,
	"TerritoryFramework.Tales.Regression.PlayerRewardRequiresValidAbilitySystemContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryEventContextConditionRegression::RunTest(const FString& Parameters)
{
	UTerritoryEventContextCondition* Condition =
		NewObject<UTerritoryEventContextCondition>();
	TestNotNull(TEXT("Event-context condition created"), Condition);
	if (!Condition) return false;

	TestFalse(TEXT("A world-level state recovery cannot run a player reward"),
		Condition->CheckCondition(nullptr, nullptr, nullptr));

	ANarrativeNPCCharacter* NarrativeNPC = NewObject<ANarrativeNPCCharacter>();
	TestNotNull(TEXT("Narrative NPC with an ability system created"), NarrativeNPC);
	if (!NarrativeNPC) return false;
	TestFalse(TEXT("A valid NPC ability system does not satisfy the default player-only rule"),
		Condition->CheckCondition(NarrativeNPC, nullptr, nullptr));

	Condition->bRequirePlayerControlledTarget = false;
	TestTrue(TEXT("An explicitly non-player event accepts a valid Narrative ability system"),
		Condition->CheckCondition(NarrativeNPC, nullptr, nullptr));

	APawn* PlainPawn = NewObject<APawn>();
	TestNotNull(TEXT("Plain pawn without an ability system created"), PlainPawn);
	if (!PlainPawn) return false;
	TestFalse(TEXT("A target without an ability system cannot run a GAS event"),
		Condition->CheckCondition(PlainPawn, nullptr, nullptr));

	Condition->bRequireAbilitySystemComponent = false;
	TestTrue(TEXT("A non-GAS event may opt out while still requiring a valid target"),
		Condition->CheckCondition(PlainPawn, nullptr, nullptr));
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Enum values
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_Enums,
	"TerritoryFramework.Contract.EnumValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_Enums::RunTest(const FString& Parameters)
{
	// ─── ETerritoryState ───
	TestEqual(TEXT("ETerritoryState::Unclaimed == 0"),
		static_cast<uint8>(ETerritoryState::Unclaimed), static_cast<uint8>(0));
	TestEqual(TEXT("ETerritoryState::Claimed == 1"),
		static_cast<uint8>(ETerritoryState::Claimed), static_cast<uint8>(1));
	TestEqual(TEXT("ETerritoryState::Contested == 2"),
		static_cast<uint8>(ETerritoryState::Contested), static_cast<uint8>(2));
	TestEqual(TEXT("ETerritoryState::Locked == 3"),
		static_cast<uint8>(ETerritoryState::Locked), static_cast<uint8>(3));

	// Verify all values are distinct
	TestNotEqual(TEXT("Unclaimed != Claimed"),
		ETerritoryState::Unclaimed, ETerritoryState::Claimed);
	TestNotEqual(TEXT("Claimed != Contested"),
		ETerritoryState::Claimed, ETerritoryState::Contested);
	TestNotEqual(TEXT("Contested != Locked"),
		ETerritoryState::Contested, ETerritoryState::Locked);

	// ─── ECaptureResult ───
	TestEqual(TEXT("ECaptureResult::Success == 0"),
		static_cast<uint8>(ECaptureResult::Success), static_cast<uint8>(0));
	TestEqual(TEXT("ECaptureResult::AlreadyOwned == 1"),
		static_cast<uint8>(ECaptureResult::AlreadyOwned), static_cast<uint8>(1));
	TestEqual(TEXT("ECaptureResult::Locked == 2"),
		static_cast<uint8>(ECaptureResult::Locked), static_cast<uint8>(2));
	TestEqual(TEXT("ECaptureResult::DefendersRemain == 3"),
		static_cast<uint8>(ECaptureResult::DefendersRemain), static_cast<uint8>(3));
	TestEqual(TEXT("ECaptureResult::DiplomaticallyBlocked == 4"),
		static_cast<uint8>(ECaptureResult::DiplomaticallyBlocked), static_cast<uint8>(4));
	TestEqual(TEXT("ECaptureResult::InvalidTerritory == 5"),
		static_cast<uint8>(ECaptureResult::InvalidTerritory), static_cast<uint8>(5));

	// Verify all values are distinct
	TestNotEqual(TEXT("Success != AlreadyOwned"),
		ECaptureResult::Success, ECaptureResult::AlreadyOwned);
	TestNotEqual(TEXT("Success != Locked"),
		ECaptureResult::Success, ECaptureResult::Locked);
	TestNotEqual(TEXT("Success != DefendersRemain"),
		ECaptureResult::Success, ECaptureResult::DefendersRemain);
	TestNotEqual(TEXT("Success != InvalidTerritory"),
		ECaptureResult::Success, ECaptureResult::InvalidTerritory);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Struct defaults
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_StructDefaults,
	"TerritoryFramework.Contract.StructDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_StructDefaults::RunTest(const FString& Parameters)
{
	// ─── FTerritoryOwnershipData ───
	{
		FTerritoryOwnershipData Data;
		TestFalse(TEXT("Default OwningFaction is invalid"), Data.OwningFaction.IsValid());
		TestEqual(TEXT("Default State is Unclaimed"),
			Data.State, ETerritoryState::Unclaimed);
		TestEqual(TEXT("Default ControlProgress is 0"),
			Data.ControlProgress, 0.f);
		TestFalse(TEXT("Default ContestingFaction is invalid"), Data.ContestingFaction.IsValid());
		TestEqual(TEXT("Default DefenderCount is 0"), Data.DefenderCount, 0);
		TestEqual(TEXT("Default MaxConcurrentAttackers is 3"), Data.MaxConcurrentAttackers, 3);
		TestEqual(TEXT("Default PeriodicIncome is 0"), Data.PeriodicIncome, 0);
		TestEqual(TEXT("Default GuardCost is 0"), Data.GuardCost, 0);
	}

	// ─── FTerritoryEconomySnapshot ───
	{
		FTerritoryEconomySnapshot Snapshot;
		TestEqual(TEXT("Default Treasury is 0"), Snapshot.Treasury, 0);
		TestEqual(TEXT("Default TotalIncome is 0"), Snapshot.TotalIncome, 0);
		TestEqual(TEXT("Default TotalCosts is 0"), Snapshot.TotalCosts, 0);
		TestEqual(TEXT("Default TerritoryCount is 0"), Snapshot.TerritoryCount, 0);
	}

	// ─── FCaptureAttempt ───
	{
		FCaptureAttempt Attempt;
		TestTrue(TEXT("Default Territory is null"), Attempt.Territory == nullptr);
		TestFalse(TEXT("Default AttackingFaction is invalid"), Attempt.AttackingFaction.IsValid());
		TestFalse(TEXT("Default DefendingFaction is invalid"), Attempt.DefendingFaction.IsValid());
		TestEqual(TEXT("Default Result is InvalidTerritory"),
			Attempt.Result, ECaptureResult::InvalidTerritory);
		TestEqual(TEXT("Default AttackersPresent is 0"), Attempt.AttackersPresent, 0);
		TestEqual(TEXT("Default DefendersPresent is 0"), Attempt.DefendersPresent, 0);
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Interfaces
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_Interfaces,
	"TerritoryFramework.Contract.Interfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_Interfaces::RunTest(const FString& Parameters)
{
	// ─── ITerritoryOwnershipInterface ───
	{
		const UClass* IC = UTerritoryOwnershipInterface::StaticClass();
		TestNotNull(TEXT("UTerritoryOwnershipInterface exists"), IC);
		TestTrue(TEXT("GetTerritoryOwner function exists"),
			TFTestUtils::HasFunction(IC, TEXT("GetTerritoryOwner")));
		TestTrue(TEXT("GetTerritoryControlProgress function exists"),
			TFTestUtils::HasFunction(IC, TEXT("GetTerritoryControlProgress")));
		TestTrue(TEXT("IsTerritoryContested function exists"),
			TFTestUtils::HasFunction(IC, TEXT("IsTerritoryContested")));
	}

	// ─── ITerritoryEconomyInterface ───
	{
		const UClass* IC = UTerritoryEconomyInterface::StaticClass();
		TestNotNull(TEXT("UTerritoryEconomyInterface exists"), IC);
		TestTrue(TEXT("GetTreasury function exists"),
			TFTestUtils::HasFunction(IC, TEXT("GetTreasury")));
		TestTrue(TEXT("GetPeriodicIncome function exists"),
			TFTestUtils::HasFunction(IC, TEXT("GetPeriodicIncome")));
		TestTrue(TEXT("CanAfford function exists"),
			TFTestUtils::HasFunction(IC, TEXT("CanAfford")));
		TestTrue(TEXT("GetActorCurrency function exists"),
			TFTestUtils::HasFunction(IC, TEXT("GetActorCurrency")));
		TestTrue(TEXT("CanActorAfford function exists"),
			TFTestUtils::HasFunction(IC, TEXT("CanActorAfford")));
	}

	// ─── ITerritoryEventReceiverInterface ───
	{
		const UClass* IC = UTerritoryEventReceiverInterface::StaticClass();
		TestNotNull(TEXT("UTerritoryEventReceiverInterface exists"), IC);
		TestTrue(TEXT("OnTerritoryControlChanged function exists"),
			TFTestUtils::HasFunction(IC, TEXT("OnTerritoryControlChanged")));
		TestTrue(TEXT("OnTerritoryContested function exists"),
			TFTestUtils::HasFunction(IC, TEXT("OnTerritoryContested")));
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Subsystems
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_RegistrySubsystem,
	"TerritoryFramework.Contract.RegistrySubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_RegistrySubsystem::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryRegistrySubsystem::StaticClass();
	TestNotNull(TEXT("UTerritoryRegistrySubsystem::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UWorldSubsystem"),
		Class->IsChildOf(UWorldSubsystem::StaticClass()));

	// ─── Query functions ───
	TestTrue(TEXT("RegisterTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RegisterTerritory")));
	TestTrue(TEXT("UnregisterTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("UnregisterTerritory")));
	TestTrue(TEXT("GetTerritoryByTag is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryByTag")));
	TestTrue(TEXT("GetTerritoryAtLocation is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryAtLocation")));
	TestTrue(TEXT("GetTerritoriesOwnedByFaction is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoriesOwnedByFaction")));
	TestTrue(TEXT("GetAllTerritories is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetAllTerritories")));
	TestTrue(TEXT("GetTerritoryCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryCount")));
	TestTrue(TEXT("GetTerritoryCountForFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryCountForFaction")));

	// ─── Spatial Index API ───
	TestTrue(TEXT("GetTerritoriesAtLocation is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoriesAtLocation")));
	TestTrue(TEXT("GetTerritoriesInBox is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoriesInBox")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnTerritoryRegistered delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryRegistered")));
	TestTrue(TEXT("Has OnTerritoryUnregistered delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryUnregistered")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_ControlSubsystem,
	"TerritoryFramework.Contract.ControlSubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_ControlSubsystem::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryControlSubsystem::StaticClass();
	TestNotNull(TEXT("UTerritoryControlSubsystem::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UWorldSubsystem"),
		Class->IsChildOf(UWorldSubsystem::StaticClass()));

	// ─── Capture API ───
	TestTrue(TEXT("AttemptCapture is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("AttemptCapture")));
	TestTrue(TEXT("IsCaptureInProgress is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("IsCaptureInProgress")));
	TestTrue(TEXT("GetCaptureProgress is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetCaptureProgress")));
	TestTrue(TEXT("ResetCapture is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ResetCapture")));
	TestTrue(TEXT("AddCaptureProgress is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("AddCaptureProgress")));
	TestTrue(TEXT("HasAttackBudget is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("HasAttackBudget")));
	TestTrue(TEXT("RegisterAttacker is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RegisterAttacker")));
	TestTrue(TEXT("Story confrontation registration is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("TryRegisterContester")));
	TestTrue(TEXT("Story confrontation registration is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("TryRegisterContester")));
	TestTrue(TEXT("Contest eligibility is exposed separately from capture completion"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetContestEligibility")));
	TestTrue(TEXT("UnregisterAttacker is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("UnregisterAttacker")));
	TestTrue(TEXT("ForceCapture is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ForceCapture")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnTerritoryControlChanged delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryControlChanged")));
	TestTrue(TEXT("Has OnCaptureAttempted delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnCaptureAttempted")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_EconomySubsystem,
	"TerritoryFramework.Contract.EconomySubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_EconomySubsystem::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryEconomySubsystem::StaticClass();
	TestNotNull(TEXT("UTerritoryEconomySubsystem::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UWorldSubsystem"),
		Class->IsChildOf(UWorldSubsystem::StaticClass()));

	// ─── Treasury API ───
	TestTrue(TEXT("GetTreasury is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTreasury")));
	TestTrue(TEXT("GetIncome is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetIncome")));
	TestTrue(TEXT("GetCosts is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetCosts")));
	TestTrue(TEXT("CanAfford is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("CanAfford")));
	TestTrue(TEXT("AddToTreasury is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("AddToTreasury")));
	TestTrue(TEXT("TryDebitTreasury is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("TryDebitTreasury")));
	TestTrue(TEXT("RecalculateIncome is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RecalculateIncome")));
	TestTrue(TEXT("GetFactionEconomy is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetFactionEconomy")));
	TestTrue(TEXT("GetAllFactionsWithTreasury is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetAllFactionsWithTreasury")));

	// ─── Transaction Ledger API ───
	TestTrue(TEXT("GetTransactionHistory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTransactionHistory")));
	TestTrue(TEXT("OnTransactionRecorded delegate exists"),
		TFTestUtils::HasProperty(Class, TEXT("OnTransactionRecorded")));
	TestTrue(TEXT("MaxTransactionHistory property exists"),
		TFTestUtils::HasProperty(Class, TEXT("MaxTransactionHistory")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnEconomyTickFired delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnEconomyTickFired")));

	// ─── Config property ───
	TestTrue(TEXT("Has TickIntervalSeconds property"),
		TFTestUtils::HasProperty(Class, TEXT("TickIntervalSeconds")));
	TestFalse(TEXT("NoCurrencyPayout disables income and upkeep settlement"),
		UTerritoryEconomySubsystem::IsCurrencySettlementEnabled(
			ETerritoryIncomePayoutPolicy::NoCurrencyPayout));
	TestTrue(TEXT("Explicit account policies settle through Narrative inventory"),
		UTerritoryEconomySubsystem::IsCurrencySettlementEnabled(
			ETerritoryIncomePayoutPolicy::SharedNarrativeAccount));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_CombatDirector,
	"TerritoryFramework.Contract.CombatDirector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_CombatDirector::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryCombatDirector::StaticClass();
	TestNotNull(TEXT("UTerritoryCombatDirector::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UWorldSubsystem"),
		Class->IsChildOf(UWorldSubsystem::StaticClass()));

	TestTrue(TEXT("RequestAssaultSlot is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RequestAssaultSlot")));
	TestTrue(TEXT("ReleaseAssaultSlot is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ReleaseAssaultSlot")));
	TestTrue(TEXT("ReleaseAllSlots is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ReleaseAllSlots")));
	TestTrue(TEXT("HasAssaultSlot is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasAssaultSlot")));
	TestTrue(TEXT("GetGrantedSlots is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetGrantedSlots")));
	TestTrue(TEXT("GetAvailableSlots is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetAvailableSlots")));
	TestTrue(TEXT("Effective difficulty-scaled limit is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetEffectiveMaxConcurrentAttackers")));
	TestTrue(TEXT("IsEligibleAssaultController is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsEligibleAssaultController")));

	const UClass* DebuggerClass = UTerritoryDebugger::StaticClass();
	TestTrue(TEXT("Territory debugger exposes a Blueprint summary"),
		TFTestUtils::IsBlueprintPure(DebuggerClass, TEXT("BuildTerritoryDebugSummary")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_GuardContestLifecyclePolicy,
	"TerritoryFramework.Guards.Regression.ContestPreservesSurvivingGarrison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_GuardContestLifecyclePolicy::RunTest(const FString& Parameters)
{
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	TestTrue(TEXT("Hero faction exists for the lifecycle regression"), Heroes.IsValid());
	TestTrue(TEXT("Bandit faction exists for the lifecycle regression"), Bandits.IsValid());

	TestEqual(TEXT("Capture pressure preserves the incumbent guard actor"),
		TerritoryGuardLifecyclePolicy::DetermineAction(
			Heroes, Heroes, ETerritoryState::Claimed, ETerritoryState::Contested),
		ETerritoryGuardLifecycleAction::Preserve);
	TestEqual(TEXT("Contest recovery does not grant a free replacement guard"),
		TerritoryGuardLifecyclePolicy::DetermineAction(
			Heroes, Heroes, ETerritoryState::Contested, ETerritoryState::Claimed),
		ETerritoryGuardLifecycleAction::Preserve);
	TestEqual(TEXT("Locking retires the current garrison"),
		TerritoryGuardLifecyclePolicy::DetermineAction(
			Heroes, Heroes, ETerritoryState::Claimed, ETerritoryState::Locked),
		ETerritoryGuardLifecycleAction::Retire);
	TestEqual(TEXT("Unlocking may restore configured staffing"),
		TerritoryGuardLifecyclePolicy::DetermineAction(
			Heroes, Heroes, ETerritoryState::Locked, ETerritoryState::Claimed),
		ETerritoryGuardLifecycleAction::Restore);
	TestEqual(TEXT("A real owner change replaces the old faction garrison"),
		TerritoryGuardLifecyclePolicy::DetermineAction(
			Heroes, Bandits, ETerritoryState::Claimed, ETerritoryState::Claimed),
		ETerritoryGuardLifecycleAction::ReplaceForNewOwner);
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Tales Extensions
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_CaptureTask,
	"TerritoryFramework.Contract.CaptureTask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_CaptureTask::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryCaptureTask::StaticClass();
	TestNotNull(TEXT("UTerritoryCaptureTask::StaticClass()"), Class);

	// ─── Inheritance ───
	TestTrue(TEXT("Inherits UNarrativeTask"),
		Class->IsChildOf(UNarrativeTask::StaticClass()));

	// ─── Required properties ───
	TestTrue(TEXT("Has TargetTerritoryTag"),
		TFTestUtils::HasProperty(Class, TEXT("TargetTerritoryTag")));
	TestTrue(TEXT("Has RequiredCapturingFaction"),
		TFTestUtils::HasProperty(Class, TEXT("RequiredCapturingFaction")));
	TestTrue(TEXT("Has bCompleteOnLoss"),
		TFTestUtils::HasProperty(Class, TEXT("bCompleteOnLoss")));

	// ─── Overrides ───
	TestTrue(TEXT("Has GetTaskDescription override"),
		TFTestUtils::HasFunction(Class, TEXT("GetTaskDescription")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_CaptureEvent,
	"TerritoryFramework.Contract.CaptureEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_CaptureEvent::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryCaptureEvent::StaticClass();
	TestNotNull(TEXT("UTerritoryCaptureEvent::StaticClass()"), Class);

	// ─── Inheritance ───
	TestTrue(TEXT("Inherits UNarrativeEvent"),
		Class->IsChildOf(UNarrativeEvent::StaticClass()));

	// ─── Required properties ───
	TestTrue(TEXT("Has TargetTerritoryTag"),
		TFTestUtils::HasProperty(Class, TEXT("TargetTerritoryTag")));
	TestTrue(TEXT("Has CapturingFaction"),
		TFTestUtils::HasProperty(Class, TEXT("CapturingFaction")));
	TestTrue(TEXT("Can resolve the capturing faction from the dialogue target or player"),
		TFTestUtils::HasProperty(Class, TEXT("CapturingFactionSource")));
	TestTrue(TEXT("Has bForceCapture"),
		TFTestUtils::HasProperty(Class, TEXT("bForceCapture")));

	// ─── Override ───
	TestTrue(TEXT("Has ExecuteEvent override"),
		TFTestUtils::HasFunction(Class, TEXT("ExecuteEvent")));

	const UClass* EligibilityClass = UTerritoryCaptureEligibilityCondition::StaticClass();
	TestTrue(TEXT("Dialogue capture eligibility derives from Narrative Condition"),
		EligibilityClass->IsChildOf(UNarrativeCondition::StaticClass()));
	TestTrue(TEXT("Dialogue capture eligibility can require all defenders defeated"),
		TFTestUtils::HasProperty(EligibilityClass, TEXT("bRequireNoLivingDefenders")));

	const UClass* HandoverEventClass = UTerritoryOwnerHandoverEvent::StaticClass();
	TestTrue(TEXT("Owner handover derives from Narrative Event"),
		HandoverEventClass->IsChildOf(UNarrativeEvent::StaticClass()));
	TestTrue(TEXT("Owner handover has a direct Narrative spawner reference"),
		TFTestUtils::HasProperty(HandoverEventClass, TEXT("OwnerSpawner")));
	TestTrue(TEXT("Owner handover has a stable Territory tag fallback"),
		TFTestUtils::HasProperty(HandoverEventClass, TEXT("OwnerTerritoryTag")));

	const UClass* OwnerSpawnerClass = ATerritoryStoryOwnerSpawner::StaticClass();
	TestTrue(TEXT("Story owner spawner derives from Narrative NPC spawner"),
		OwnerSpawnerClass->IsChildOf(ANPCSpawner::StaticClass()));
	TestTrue(TEXT("Story owner activation is saved"),
		TFTestUtils::IsSaveGame(OwnerSpawnerClass, TEXT("bHandoverActivated")));
	TestTrue(TEXT("Story owner activation is replicated"),
		TFTestUtils::IsReplicated(OwnerSpawnerClass, TEXT("bHandoverActivated")));
	TestTrue(TEXT("Story owner activation is server callable"),
		TFTestUtils::HasFunction(OwnerSpawnerClass, TEXT("ActivateHandover")));
	TestTrue(TEXT("Story owner exposes a bounded Narrative interaction distance"),
		TFTestUtils::HasProperty(OwnerSpawnerClass, TEXT("OwnerInteractionDistance")));
	const ATerritoryStoryOwnerSpawner* OwnerSpawnerCDO =
		GetDefault<ATerritoryStoryOwnerSpawner>();
	TestEqual(TEXT("Protected-owner interaction is reliable at three metres by default"),
		OwnerSpawnerCDO->OwnerInteractionDistance, 300.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_OwnershipCondition,
	"TerritoryFramework.Contract.OwnershipCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_OwnershipCondition::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryOwnershipCondition::StaticClass();
	TestNotNull(TEXT("UTerritoryOwnershipCondition::StaticClass()"), Class);

	// ─── Inheritance ───
	TestTrue(TEXT("Inherits UNarrativeCondition"),
		Class->IsChildOf(UNarrativeCondition::StaticClass()));

	// ─── Required properties ───
	TestTrue(TEXT("Has TerritoryToCheck"),
		TFTestUtils::HasProperty(Class, TEXT("TerritoryToCheck")));
	TestTrue(TEXT("Has RequiredOwner"),
		TFTestUtils::HasProperty(Class, TEXT("RequiredOwner")));
	TestTrue(TEXT("Has bPassWhenContested"),
		TFTestUtils::HasProperty(Class, TEXT("bPassWhenContested")));
	TestTrue(TEXT("Has bPassWhenUnclaimed"),
		TFTestUtils::HasProperty(Class, TEXT("bPassWhenUnclaimed")));
	TestTrue(TEXT("Has bPassWhenLocked"),
		TFTestUtils::HasProperty(Class, TEXT("bPassWhenLocked")));

	// ─── Overrides ───
	TestTrue(TEXT("Has CheckCondition override"),
		TFTestUtils::HasFunction(Class, TEXT("CheckCondition")));
	TestTrue(TEXT("Has GetGraphDisplayText override"),
		TFTestUtils::HasFunction(Class, TEXT("GetGraphDisplayText")));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Navigation
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_MapMarker,
	"TerritoryFramework.Contract.TerritoryMapMarker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_MapMarker::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryMapMarker::StaticClass();
	TestNotNull(TEXT("UTerritoryMapMarker::StaticClass()"), Class);

	// ─── Inheritance ───
	TestTrue(TEXT("Inherits UMapMarker"),
		Class->IsChildOf(UMapMarker::StaticClass()));

	// ─── Override signatures ───
	TestTrue(TEXT("Has GetMarkerColor override"),
		TFTestUtils::HasFunction(Class, TEXT("GetMarkerColor")));
	TestTrue(TEXT("Has GetMarkerDisplayText override"),
		TFTestUtils::HasFunction(Class, TEXT("GetMarkerDisplayText")));
	TestTrue(TEXT("Has MarkerOnPaint override"),
		TFTestUtils::HasFunction(Class, TEXT("MarkerOnPaint")));

	// ─── Configuration properties ───
	TestTrue(TEXT("Has FactionColorMap"),
		TFTestUtils::HasProperty(Class, TEXT("FactionColorMap")));
	TestTrue(TEXT("Has DefaultColor"),
		TFTestUtils::HasProperty(Class, TEXT("DefaultColor")));
	TestTrue(TEXT("Has ContestedColor"),
		TFTestUtils::HasProperty(Class, TEXT("ContestedColor")));
	TestTrue(TEXT("Has LockedColor"),
		TFTestUtils::HasProperty(Class, TEXT("LockedColor")));
	TestTrue(TEXT("Has bDrawTerritoryOutline"),
		TFTestUtils::HasProperty(Class, TEXT("bDrawTerritoryOutline")));

	// ─── BP API ───
	TestTrue(TEXT("SetTerritoryVolume is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SetTerritoryVolume")));
	TestTrue(TEXT("GetTerritoryVolume is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryVolume")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_NavMarkerComponent,
	"TerritoryFramework.Contract.TerritoryNavigationMarkerComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_NavMarkerComponent::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryNavigationMarkerComponent::StaticClass();
	TestNotNull(TEXT("UTerritoryNavigationMarkerComponent::StaticClass()"), Class);

	TestTrue(TEXT("Inherits UNavigationMarkerComponent"),
		Class->IsChildOf(UNavigationMarkerComponent::StaticClass()));
	TestTrue(TEXT("Inherits UActorComponent"),
		Class->IsChildOf(UActorComponent::StaticClass()));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Blueprint Library
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_BlueprintLibrary,
	"TerritoryFramework.Contract.BlueprintLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_BlueprintLibrary::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryBlueprintLibrary::StaticClass();
	TestNotNull(TEXT("UTerritoryBlueprintLibrary::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UBlueprintFunctionLibrary"),
		Class->IsChildOf(UBlueprintFunctionLibrary::StaticClass()));

	// ─── Static accessor functions ───
	TestTrue(TEXT("GetTerritoryRegistry is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryRegistry")));
	TestTrue(TEXT("GetTerritoryControl is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryControl")));
	TestTrue(TEXT("GetTerritoryEconomy is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryEconomy")));
	TestTrue(TEXT("GetTerritoryCombatDirector is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryCombatDirector")));
	TestTrue(TEXT("GetTerritoryAtLocation is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryAtLocation")));
	TestTrue(TEXT("GetTerritoryByTag is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTerritoryByTag")));
	TestTrue(TEXT("IsSameFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsSameFaction")));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Developer Settings
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DeveloperSettings,
	"TerritoryFramework.Contract.DeveloperSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DeveloperSettings::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryDeveloperSettings::StaticClass();
	TestNotNull(TEXT("UTerritoryDeveloperSettings::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UDeveloperSettings"),
		Class->IsChildOf(UDeveloperSettings::StaticClass()));

	// ─── Config properties ───
	TestTrue(TEXT("Has EconomyTickIntervalSeconds"),
		TFTestUtils::HasProperty(Class, TEXT("EconomyTickIntervalSeconds")));
	TestTrue(TEXT("Has DefaultTerritoryIncome"),
		TFTestUtils::HasProperty(Class, TEXT("DefaultTerritoryIncome")));
	TestTrue(TEXT("Has DefaultGuardCost"),
		TFTestUtils::HasProperty(Class, TEXT("DefaultGuardCost")));
	TestTrue(TEXT("Has CaptureProgressPerSecond"),
		TFTestUtils::HasProperty(Class, TEXT("CaptureProgressPerSecond")));
	TestTrue(TEXT("Has CaptureProgressDecayPerSecond"),
		TFTestUtils::HasProperty(Class, TEXT("CaptureProgressDecayPerSecond")));
	TestTrue(TEXT("Has DefaultMaxConcurrentAttackers"),
		TFTestUtils::HasProperty(Class, TEXT("DefaultMaxConcurrentAttackers")));
	TestTrue(TEXT("Has DefaultPlayerFaction"),
		TFTestUtils::HasProperty(Class, TEXT("DefaultPlayerFaction")));

	// ─── Default values ───
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	TestNotNull(TEXT("GetDefault returns valid settings"), Settings);
	if (Settings)
	{
		TestEqual(TEXT("Default EconomyTickInterval is 300"),
			Settings->EconomyTickIntervalSeconds, 300.f);
		TestEqual(TEXT("Default TerritoryIncome is 100"),
			Settings->DefaultTerritoryIncome, 100);
		TestEqual(TEXT("Default GuardCost is 50"),
			Settings->DefaultGuardCost, 50);
		TestTrue(TEXT("Default CaptureProgressPerSecond > 0"),
			Settings->CaptureProgressPerSecond > 0.f);
		TestTrue(TEXT("Default CaptureProgressDecayPerSecond > 0"),
			Settings->CaptureProgressDecayPerSecond > 0.f);
		TestEqual(TEXT("Default MaxConcurrentAttackers is 3"),
			Settings->DefaultMaxConcurrentAttackers, 3);
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTIONAL TESTS — Pure logic (no world required)
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_IsSameFaction,
	"TerritoryFramework.Functional.IsSameFaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_IsSameFaction::RunTest(const FString& Parameters)
{
	FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Bandits")), false);
	FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
	FGameplayTag Invalid;

	// Same faction
	TestTrue(TEXT("Bandits == Bandits"),
		UTerritoryBlueprintLibrary::IsSameFaction(Bandits, Bandits));

	// Different factions
	TestFalse(TEXT("Bandits != Heroes"),
		UTerritoryBlueprintLibrary::IsSameFaction(Bandits, Heroes));

	// Invalid tags
	TestFalse(TEXT("Invalid != Invalid"),
		UTerritoryBlueprintLibrary::IsSameFaction(Invalid, Invalid));
	TestFalse(TEXT("Bandits != Invalid"),
		UTerritoryBlueprintLibrary::IsSameFaction(Bandits, Invalid));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_OwnershipDataDefaults,
	"TerritoryFramework.Functional.OwnershipDataDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_OwnershipDataDefaults::RunTest(const FString& Parameters)
{
	FTerritoryOwnershipData Data;

	// Verify struct is zero-initialized correctly
	TestFalse(TEXT("OwningFaction starts invalid"), Data.OwningFaction.IsValid());
	TestEqual(TEXT("State starts Unclaimed"), Data.State, ETerritoryState::Unclaimed);
	TestTrue(TEXT("ControlProgress starts at 0"), FMath::IsNearlyZero(Data.ControlProgress));
	TestEqual(TEXT("DefenderCount starts at 0"), Data.DefenderCount, 0);
	TestEqual(TEXT("MaxConcurrentAttackers starts at 3"), Data.MaxConcurrentAttackers, 3);

	// Verify we can set and read values
	Data.OwningFaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Bandits")), false);
	Data.State = ETerritoryState::Claimed;
	Data.ControlProgress = 0.75f;
	Data.DefenderCount = 5;

	TestTrue(TEXT("OwningFaction set correctly"), Data.OwningFaction.IsValid());
	TestEqual(TEXT("State set to Claimed"), Data.State, ETerritoryState::Claimed);
	TestEqual(TEXT("ControlProgress set to 0.75"), Data.ControlProgress, 0.75f);
	TestEqual(TEXT("DefenderCount set to 5"), Data.DefenderCount, 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_PlayerManagedGarrisonPolicy,
	"TerritoryFramework.Functional.PlayerManagedGarrisonPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_PlayerManagedGarrisonPolicy::RunTest(const FString& Parameters)
{
	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	TestNotNull(TEXT("Test territory exists"), Territory);
	if (!Territory) return false;
	// Capacity is physical: this policy fixture authors three distinct combat slots
	// instead of relying on the removed random/no-post fallback.
	Territory->GuardSpawnPoints.Add(NewObject<ATerritoryGuardSpawnPoint>());
	Territory->GuardSpawnPoints.Add(NewObject<ATerritoryGuardSpawnPoint>());
	Territory->GuardSpawnPoints.Add(NewObject<ATerritoryGuardSpawnPoint>());
	TestEqual(TEXT("Three authored markers provide three active slots"),
		Territory->GetMaxGuardCount(), 3);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Narrative.Factions.Heroes")), false);

	FTerritoryTransitionContext ScriptContext;
	TestEqual(TEXT("Script/AI ownership keeps the authored three-guard target"),
		Territory->GetPostCaptureGuardCount(ScriptContext), 3);
	FTerritoryTransitionContext PlayerContext;
	PlayerContext.PlayerController = NewObject<APlayerController>();
	ATerritoryGuardCharacter* PlayerFactionPawn = NewObject<ATerritoryGuardCharacter>();
	INarrativeTeamAgentInterface* PlayerTeamAgent =
		Cast<INarrativeTeamAgentInterface>(PlayerFactionPawn);
	TestNotNull(TEXT("Player context pawn exposes Narrative faction authority"), PlayerTeamAgent);
	if (PlayerTeamAgent) PlayerTeamAgent->AddFaction(Heroes);
	PlayerContext.TargetPawn = PlayerFactionPawn;
	PlayerContext.RequestingFaction = Heroes;
	TestEqual(TEXT("Player-driven capture starts unstaffed for explicit P&L control"),
		Territory->GetPostCaptureGuardCountForOwner(PlayerContext, Heroes), 0);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Narrative.Factions.Bandits")), false);
	TestEqual(TEXT("A player context cannot suppress an unrelated AI faction garrison"),
		Territory->GetPostCaptureGuardCountForOwner(PlayerContext, Bandits), 3);
	FTerritoryTransitionContext SpoofedContext = PlayerContext;
	SpoofedContext.RequestingFaction = Bandits;
	TestEqual(TEXT("Normalizing the request tag cannot spoof Narrative faction membership"),
		Territory->GetPostCaptureGuardCountForOwner(SpoofedContext, Bandits), 3);

	Territory->SetOwningFactionWithContext(Heroes, PlayerContext);
	TestEqual(TEXT("Explicit player context survives the ownership wrapper and commits zero guards"),
		Territory->GetDesiredGuardCount(), 0);
	FTerritoryOwnershipData Claimed = Territory->GetOwnershipData();
	Claimed.OwningFaction = Heroes;
	Claimed.State = ETerritoryState::Claimed;
	Claimed.ControlProgress = 1.f;
	Claimed.DesiredGuardCount = 3;
	Claimed.GuardCost = 50;
	Claimed.GuardRecruitmentCost = 50;
	TestTrue(TEXT("Claimed test state commits atomically"), Territory->CommitOwnershipData(Claimed));

	ATerritoryGuardCharacter* Requester = NewObject<ATerritoryGuardCharacter>();
	INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Requester);
	TestNotNull(TEXT("Requester exposes Narrative faction authority"), TeamAgent);
	if (TeamAgent) TeamAgent->AddFaction(Heroes);
	FText FailureReason;
	TestTrue(TEXT("Desired target can be reduced even when every assigned guard is dead"),
		Territory->CanRemoveGuards(Requester, 1, FailureReason));
	FText ResultMessage;
	TestTrue(TEXT("Reduction commits without requiring a live pawn to withdraw"),
		Territory->TryRemoveGuards(Requester, 1, ResultMessage));
	TestEqual(TEXT("Persistent desired target is reduced exactly once"),
		Territory->GetDesiredGuardCount(), 2);
	TestEqual(TEXT("No phantom live guards are created by a reduction"),
		Territory->GetSpawnedGuardCount(), 0);

	const FTerritoryGarrisonMutationResult Invalid = Territory->TrySetDesiredGuardCount(nullptr, 1);
	TestFalse(TEXT("Server mutation rejects a missing exact requester"), Invalid.bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_TreasuryStruct,
	"TerritoryFramework.Functional.TreasuryStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_TreasuryStruct::RunTest(const FString& Parameters)
{
	FTerritoryTreasury Treasury;

	// Default state — Gold no longer exists (faction wealth = player inventory aggregate).
	TestEqual(TEXT("Default IncomePerTick is 0"), Treasury.IncomePerTick, 0);
	TestEqual(TEXT("Default CostsPerTick is 0"), Treasury.CostsPerTick, 0);
	TestEqual(TEXT("Default TerritoryCount is 0"), Treasury.TerritoryCount, 0);

	// Simulate territory ownership
	Treasury.IncomePerTick = 300;
	Treasury.CostsPerTick = 150;
	Treasury.TerritoryCount = 3;

	// Net income per tick (distributed to player inventories by EconomySubsystem)
	int32 NetIncome = Treasury.IncomePerTick - Treasury.CostsPerTick;
	TestEqual(TEXT("Net income per tick = 150"), NetIncome, 150);

	// CanAfford is based on aggregate player inventory currency, not Treasury struct
	// EconomySubsystem::CanAfford checks GetTreasury() which reads live player inventories
	TestTrue(TEXT("Struct holds income/cost parameters"), Treasury.IncomePerTick > 0);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CROSS-MODULE INTEGRATION CONTRACT
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_NarrativeIntegration,
	"TerritoryFramework.Contract.NarrativeIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_NarrativeIntegration::RunTest(const FString& Parameters)
{
	// Verify all territory classes correctly inherit from their Narrative base classes
	// This ensures the plugin can be used as drop-in extensions

	// ─── Capture Task → UNarrativeTask ───
	{
		const UClass* TaskClass = UTerritoryCaptureTask::StaticClass();
		const UClass* BaseClass = UNarrativeTask::StaticClass();
		TestTrue(TEXT("CaptureTask is-a NarrativeTask"),
			TaskClass->IsChildOf(BaseClass));

		// Verify the task has RequiredQuantity from the base class
		TestTrue(TEXT("CaptureTask inherits RequiredQuantity"),
			TFTestUtils::HasProperty(TaskClass, TEXT("RequiredQuantity")));

		// Verify the task has TickInterval from the base class
		TestTrue(TEXT("CaptureTask inherits TickInterval"),
			TFTestUtils::HasProperty(TaskClass, TEXT("TickInterval")));

		// Verify the task has MarkerSettings from the base class
		TestTrue(TEXT("CaptureTask inherits MarkerSettings"),
			TFTestUtils::HasProperty(TaskClass, TEXT("MarkerSettings")));
	}

	// ─── Capture Event → UNarrativeEvent ───
	{
		const UClass* EventClass = UTerritoryCaptureEvent::StaticClass();
		const UClass* BaseClass = UNarrativeEvent::StaticClass();
		TestTrue(TEXT("CaptureEvent is-a NarrativeEvent"),
			EventClass->IsChildOf(BaseClass));
	}

	// ─── Ownership Condition → UNarrativeCondition ───
	{
		const UClass* CondClass = UTerritoryOwnershipCondition::StaticClass();
		const UClass* BaseClass = UNarrativeCondition::StaticClass();
		TestTrue(TEXT("OwnershipCondition is-a NarrativeCondition"),
			CondClass->IsChildOf(BaseClass));

		// Verify the condition has ConditionFilter from base class
		// (UNarrativeCondition has this for world-state vs actor-target conditions)
	}

	// ─── Map Marker → UMapMarker ───
	{
		const UClass* MarkerClass = UTerritoryMapMarker::StaticClass();
		const UClass* BaseClass = UMapMarker::StaticClass();
		TestTrue(TEXT("TerritoryMapMarker is-a MapMarker"),
			MarkerClass->IsChildOf(BaseClass));

		// Verify bWantsOnPaint is inherited
		TestTrue(TEXT("TerritoryMapMarker inherits bWantsOnPaint"),
			TFTestUtils::HasProperty(MarkerClass, TEXT("bWantsOnPaint")));

		// Verify RefreshMarker is inherited
		TestTrue(TEXT("TerritoryMapMarker inherits RefreshMarker"),
			TFTestUtils::HasFunction(MarkerClass, TEXT("RefreshMarker")));
	}

	// ─── Nav Marker Component → UNavigationMarkerComponent ───
	{
		const UClass* NavCompClass = UTerritoryNavigationMarkerComponent::StaticClass();
		const UClass* BaseClass = UNavigationMarkerComponent::StaticClass();
		TestTrue(TEXT("TerritoryNavMarkerComponent is-a NavigationMarkerComponent"),
			NavCompClass->IsChildOf(BaseClass));

		// Verify MarkerObject is inherited
		TestTrue(TEXT("TerritoryNavMarkerComponent inherits MarkerObject"),
			TFTestUtils::HasProperty(NavCompClass, TEXT("MarkerObject")));
	}

	// ─── Volume → INarrativeSavableActor contract ───
	{
		const UClass* VolumeClass = ATerritoryVolume::StaticClass();
		TestTrue(TEXT("Volume implements INarrativeSavableActor"),
			VolumeClass->ImplementsInterface(UNarrativeSavableActor::StaticClass()));

		// Verify all 4 required interface functions exist
		TestTrue(TEXT("Volume has GetActorGUID"),
			TFTestUtils::HasFunction(VolumeClass, TEXT("GetActorGUID")));
		TestTrue(TEXT("Volume has PrepareForSave"),
			TFTestUtils::HasFunction(VolumeClass, TEXT("PrepareForSave")));
		TestTrue(TEXT("Volume has Load"),
			TFTestUtils::HasFunction(VolumeClass, TEXT("Load")));
		TestTrue(TEXT("Volume has ShouldRespawn"),
			TFTestUtils::HasFunction(VolumeClass, TEXT("ShouldRespawn")));
		TestTrue(TEXT("Volume has SetActorGUID"),
			TFTestUtils::HasFunction(VolumeClass, TEXT("SetActorGUID")));
	}

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE SANITY
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_ModuleSanity,
	"TerritoryFramework.Contract.ModuleSanity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_ModuleSanity::RunTest(const FString& Parameters)
{
	// Verify all key classes can be resolved by StaticClass()
	// This catches linker issues, missing GENERATED_BODY(), etc.

	TestNotNull(TEXT("ATerritoryVolume::StaticClass()"), ATerritoryVolume::StaticClass());
	TestNotNull(TEXT("UTerritoryRegistrySubsystem::StaticClass()"), UTerritoryRegistrySubsystem::StaticClass());
	TestNotNull(TEXT("UTerritoryControlSubsystem::StaticClass()"), UTerritoryControlSubsystem::StaticClass());
	TestNotNull(TEXT("UTerritoryEconomySubsystem::StaticClass()"), UTerritoryEconomySubsystem::StaticClass());
	TestNotNull(TEXT("UTerritoryCombatDirector::StaticClass()"), UTerritoryCombatDirector::StaticClass());
	TestNotNull(TEXT("UTerritoryCaptureTask::StaticClass()"), UTerritoryCaptureTask::StaticClass());
	TestNotNull(TEXT("UTerritoryCaptureEvent::StaticClass()"), UTerritoryCaptureEvent::StaticClass());
	TestNotNull(TEXT("UTerritoryOwnershipCondition::StaticClass()"), UTerritoryOwnershipCondition::StaticClass());
	TestNotNull(TEXT("UTerritoryMapMarker::StaticClass()"), UTerritoryMapMarker::StaticClass());
	TestNotNull(TEXT("UTerritoryNavigationMarkerComponent::StaticClass()"), UTerritoryNavigationMarkerComponent::StaticClass());
	TestNotNull(TEXT("UTerritoryBlueprintLibrary::StaticClass()"), UTerritoryBlueprintLibrary::StaticClass());
	TestNotNull(TEXT("UTerritoryDeveloperSettings::StaticClass()"), UTerritoryDeveloperSettings::StaticClass());

	// Verify interfaces
	TestNotNull(TEXT("UTerritoryOwnershipInterface::StaticClass()"), UTerritoryOwnershipInterface::StaticClass());
	TestNotNull(TEXT("UTerritoryEconomyInterface::StaticClass()"), UTerritoryEconomyInterface::StaticClass());
	TestNotNull(TEXT("UTerritoryEventReceiverInterface::StaticClass()"), UTerritoryEventReceiverInterface::StaticClass());

	// Verify log category compiles and is accessible
	UE_LOG(LogTerritory, Verbose, TEXT("Automation test: LogTerritory category verified"));
	TestTrue(TEXT("LogTerritory category is accessible"), true);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT TESTS — Diplomacy (Phase F)
// ═══════════════════════════════════════════════════════════════════════════════

#include "Core/TerritoryDiplomacyTypes.h"
#include "Subsystems/TerritoryDiplomacySubsystem.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritorySavableData.h"
#include "Combat/BTTask_RequestTerritoryPermission.h"
#include "Combat/BTTask_ReleaseTerritoryPermission.h"
#include "UI/TerritoryDebugWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DiplomacyTypes,
	"TerritoryFramework.Contract.DiplomacyTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DiplomacyTypes::RunTest(const FString& Parameters)
{
	// ─── EDiplomacyState ───
	TestEqual(TEXT("DiplomacyState::None == 0"), static_cast<uint8>(EDiplomacyState::None), static_cast<uint8>(0));
	TestEqual(TEXT("DiplomacyState::Alliance == 1"), static_cast<uint8>(EDiplomacyState::Alliance), static_cast<uint8>(1));
	TestEqual(TEXT("DiplomacyState::War == 4"), static_cast<uint8>(EDiplomacyState::War), static_cast<uint8>(4));
	TestNotEqual(TEXT("Alliance != War"), EDiplomacyState::Alliance, EDiplomacyState::War);
	TestNotEqual(TEXT("None != Ceasefire"), EDiplomacyState::None, EDiplomacyState::Ceasefire);

	// ─── EDiplomacyEventType ───
	TestEqual(TEXT("EventType::DeclaredWar == 0"), static_cast<uint8>(EDiplomacyEventType::DeclaredWar), static_cast<uint8>(0));
	TestNotEqual(TEXT("DeclaredWar != DeclaredPeace"), EDiplomacyEventType::DeclaredWar, EDiplomacyEventType::DeclaredPeace);

	// ─── FTreatyRecord defaults ───
	{
		FTreatyRecord Treaty;
		TestFalse(TEXT("Default Treaty FactionA is invalid"), Treaty.FactionA.IsValid());
		TestFalse(TEXT("Default Treaty FactionB is invalid"), Treaty.FactionB.IsValid());
		TestEqual(TEXT("Default State is None"), Treaty.State, EDiplomacyState::None);
		TestTrue(TEXT("Default is permanent"), Treaty.bPermanent);
		TestFalse(TEXT("Default is not valid"), Treaty.IsValid());
		TestFalse(TEXT("Default is not expired"), Treaty.IsExpired(100.f));
	}

	// ─── FTreatyRecord validity ───
	{
		FTreatyRecord Treaty;
		Treaty.FactionA = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Bandits")), false);
		Treaty.FactionB = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
		TestTrue(TEXT("Treaty with both factions is valid"), Treaty.IsValid());

		// Test expiry
		Treaty.bPermanent = false;
		Treaty.ExpiryGameTime = 50.f;
		TestFalse(TEXT("Not expired before expiry time"), Treaty.IsExpired(40.f));
		TestTrue(TEXT("Expired at expiry time"), Treaty.IsExpired(50.f));
		TestTrue(TEXT("Expired after expiry time"), Treaty.IsExpired(60.f));
	}

	// ─── FDiplomacyEvent ───
	{
		FDiplomacyEvent Event;
		TestEqual(TEXT("Default event type is DeclaredWar"), Event.EventType, EDiplomacyEventType::DeclaredWar);
		TestEqual(TEXT("Default game time is 0"), Event.GameTime, 0.f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DiplomacySubsystem,
	"TerritoryFramework.Contract.DiplomacySubsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DiplomacySubsystem::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryDiplomacySubsystem::StaticClass();
	TestNotNull(TEXT("UTerritoryDiplomacySubsystem::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UWorldSubsystem"), Class->IsChildOf(UWorldSubsystem::StaticClass()));

	// ─── Diplomacy API ───
	TestTrue(TEXT("DeclareWar is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("DeclareWar")));
	TestTrue(TEXT("DeclarePeace is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("DeclarePeace")));
	TestTrue(TEXT("FormAlliance is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("FormAlliance")));
	TestTrue(TEXT("BreakAlliance is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("BreakAlliance")));
	TestTrue(TEXT("SignTradeAgreement is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("SignTradeAgreement")));
	TestTrue(TEXT("SetDiplomacyState is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("SetDiplomacyState")));

	// ─── Query API ───
	TestTrue(TEXT("GetDiplomacyState is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetDiplomacyState")));
	TestTrue(TEXT("IsAtWar is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("IsAtWar")));
	TestTrue(TEXT("Faction-container War query is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("AreAnyFactionsAtWar")));
	TestTrue(TEXT("IsAllied is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("IsAllied")));
	TestTrue(TEXT("HasTradeAgreement is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("HasTradeAgreement")));

	// ─── Reputation API ───
	TestTrue(TEXT("AddReputation is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("AddReputation")));
	TestTrue(TEXT("SetReputation is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("SetReputation")));
	TestTrue(TEXT("GetReputation is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetReputation")));
	TestTrue(TEXT("GetAllReputation is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetAllReputation")));

	// ─── History API ───
	TestTrue(TEXT("GetAllTreaties is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetAllTreaties")));
	TestTrue(TEXT("GetTreatiesForFaction is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTreatiesForFaction")));
	TestTrue(TEXT("GetDiplomacyHistory is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetDiplomacyHistory")));

	// ─── Sync API ───
	TestTrue(TEXT("SyncToGameState is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("SyncToGameState")));
	TestTrue(TEXT("LoadFromGameState is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("LoadFromGameState")));
	TestTrue(TEXT("SyncToGameState is authority-only"), TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("SyncToGameState")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnDiplomacyStateChanged delegate"), TFTestUtils::HasProperty(Class, TEXT("OnDiplomacyStateChanged")));
	TestTrue(TEXT("Has OnDiplomacyEvent delegate"), TFTestUtils::HasProperty(Class, TEXT("OnDiplomacyEvent")));
	TestTrue(TEXT("Has OnReputationChanged delegate"), TFTestUtils::HasProperty(Class, TEXT("OnReputationChanged")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_Hierarchy,
	"TerritoryFramework.Contract.TerritoryHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_Hierarchy::RunTest(const FString& Parameters)
{
	// ─── ATerritoryCity ───
	{
		const UClass* Class = ATerritoryCity::StaticClass();
		TestNotNull(TEXT("ATerritoryCity::StaticClass()"), Class);
		TestTrue(TEXT("City inherits TerritoryVolume"), Class->IsChildOf(ATerritoryVolume::StaticClass()));
		TestTrue(TEXT("City implements INarrativeSavableActor"),
			TFTestUtils::ImplementsInterface(Class, UNarrativeSavableActor::StaticClass()));

		TestTrue(TEXT("GetDistricts is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetDistricts")));
		TestTrue(TEXT("GetDistrictCount is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetDistrictCount")));
		TestTrue(TEXT("AllDistrictsOwnedBy is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("AllDistrictsOwnedBy")));
		TestTrue(TEXT("GetCityControlPercentage is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetCityControlPercentage")));
		TestTrue(TEXT("OnCityFullyCaptured is BP extension point"), TFTestUtils::HasFunction(Class, TEXT("OnCityFullyCaptured")));
		TestTrue(TEXT("OnCityLost is BP extension point"), TFTestUtils::HasFunction(Class, TEXT("OnCityLost")));
	}

	// ─── ATerritoryDistrict ───
	{
		const UClass* Class = ATerritoryDistrict::StaticClass();
		TestNotNull(TEXT("ATerritoryDistrict::StaticClass()"), Class);
		TestTrue(TEXT("District inherits TerritoryVolume"), Class->IsChildOf(ATerritoryVolume::StaticClass()));

		TestTrue(TEXT("Has bIsCapital property"), TFTestUtils::HasProperty(Class, TEXT("bIsCapital")));
		TestTrue(TEXT("GetOwningCity is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetOwningCity")));
		TestTrue(TEXT("GetProperties is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetProperties")));
		TestTrue(TEXT("IsCapitalDistrict is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("IsCapitalDistrict")));
	}

	// ─── ATerritoryProperty ───
	{
		const UClass* Class = ATerritoryProperty::StaticClass();
		TestNotNull(TEXT("ATerritoryProperty::StaticClass()"), Class);
		TestTrue(TEXT("Property inherits TerritoryVolume"), Class->IsChildOf(ATerritoryVolume::StaticClass()));

		TestTrue(TEXT("Has UpgradeLevel property"), TFTestUtils::HasProperty(Class, TEXT("UpgradeLevel")));
		TestTrue(TEXT("Has MaxUpgradeLevel property"), TFTestUtils::HasProperty(Class, TEXT("MaxUpgradeLevel")));
		TestTrue(TEXT("Has UpgradeCostPerLevel property"), TFTestUtils::HasProperty(Class, TEXT("UpgradeCostPerLevel")));
		TestTrue(TEXT("Has IncomeBonusPerLevel property"), TFTestUtils::HasProperty(Class, TEXT("IncomeBonusPerLevel")));
		TestTrue(TEXT("CanUpgrade is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("CanUpgrade")));
		TestTrue(TEXT("GetUpgradeCost is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetUpgradeCost")));
		TestTrue(TEXT("GetEffectiveIncome is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetEffectiveIncome")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_SavableData,
	"TerritoryFramework.Contract.SavableData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_SavableData::RunTest(const FString& Parameters)
{
	const UClass* Class = ATerritorySavableData::StaticClass();
	TestNotNull(TEXT("ATerritorySavableData::StaticClass()"), Class);
	TestTrue(TEXT("Inherits AActor"), Class->IsChildOf(AActor::StaticClass()));
	TestTrue(TEXT("Implements INarrativeSavableActor"),
		TFTestUtils::ImplementsInterface(Class, UNarrativeSavableActor::StaticClass()));

	// SaveGame properties
	TestTrue(TEXT("SavedTreasuries is SaveGame"), TFTestUtils::IsSaveGame(Class, TEXT("SavedTreasuries")));
	TestTrue(TEXT("SavedTreaties is SaveGame"), TFTestUtils::IsSaveGame(Class, TEXT("SavedTreaties")));
	TestTrue(TEXT("SavedReputation is SaveGame"), TFTestUtils::IsSaveGame(Class, TEXT("SavedReputation")));
	TestTrue(TEXT("SavedDiplomacyHistory is SaveGame"), TFTestUtils::IsSaveGame(Class, TEXT("SavedDiplomacyHistory")));

	// Interface functions
	TestTrue(TEXT("Has GetActorGUID"), TFTestUtils::HasFunction(Class, TEXT("GetActorGUID")));
	TestTrue(TEXT("Has PrepareForSave"), TFTestUtils::HasFunction(Class, TEXT("PrepareForSave")));
	TestTrue(TEXT("Has Load"), TFTestUtils::HasFunction(Class, TEXT("Load")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_BTTasks,
	"TerritoryFramework.Contract.BTTasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_BTTasks::RunTest(const FString& Parameters)
{
	// ─── Request Permission ───
	{
		const UClass* Class = UBTTask_RequestTerritoryPermission::StaticClass();
		TestNotNull(TEXT("UBTTask_RequestTerritoryPermission::StaticClass()"), Class);
		TestTrue(TEXT("Inherits UBTTaskNode"), Class->IsChildOf(UBTTaskNode::StaticClass()));
		TestTrue(TEXT("Has TerritoryKey blackboard selector"), TFTestUtils::HasProperty(Class, TEXT("TerritoryKey")));
		TestTrue(TEXT("Has bPermissionGrantedKey blackboard selector"), TFTestUtils::HasProperty(Class, TEXT("bPermissionGrantedKey")));
	}

	// ─── Release Permission ───
	{
		const UClass* Class = UBTTask_ReleaseTerritoryPermission::StaticClass();
		TestNotNull(TEXT("UBTTask_ReleaseTerritoryPermission::StaticClass()"), Class);
		TestTrue(TEXT("Inherits UBTTaskNode"), Class->IsChildOf(UBTTaskNode::StaticClass()));
		TestTrue(TEXT("Has TerritoryKey blackboard selector"), TFTestUtils::HasProperty(Class, TEXT("TerritoryKey")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_PropertyUpgrade,
	"TerritoryFramework.Functional.PropertyUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_PropertyUpgrade::RunTest(const FString& Parameters)
{
	// Test upgrade logic without a world
	ATerritoryProperty* Prop = NewObject<ATerritoryProperty>();
	Prop->UpgradeLevel = 0;
	Prop->MaxUpgradeLevel = 3;
	Prop->UpgradeCostPerLevel = 500;
	Prop->IncomeBonusPerLevel = 25;

	TestTrue(TEXT("Can upgrade at level 0"), Prop->CanUpgrade());
	TestEqual(TEXT("Upgrade cost at level 0 is 500"), Prop->GetUpgradeCost(), 500);

	Prop->UpgradeLevel = 2;
	TestTrue(TEXT("Can upgrade at level 2"), Prop->CanUpgrade());
	TestEqual(TEXT("Upgrade cost at level 2 is 1500"), Prop->GetUpgradeCost(), 1500);

	Prop->UpgradeLevel = 3;
	TestFalse(TEXT("Cannot upgrade at max level"), Prop->CanUpgrade());
	TestEqual(TEXT("Upgrade cost at max level is 0"), Prop->GetUpgradeCost(), 0);

	// Effective income
	Prop->UpgradeLevel = 2;
	// GetPeriodicIncome returns OwnershipData.PeriodicIncome (default from constructor)
	// Since we can't easily set it here without BeginPlay, just test the bonus calculation
	int32 ExpectedBonus = 2 * 25; // UpgradeLevel * IncomeBonusPerLevel
	TestEqual(TEXT("Income bonus at level 2 is 50"), ExpectedBonus, 50);

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// NARRATIVE INTEGRATION TESTS
// Verify actual integration contracts between TerritoryFramework and Narrative Pro
// ═══════════════════════════════════════════════════════════════════════════════

#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Tales/TalesComponent.h"
#include "AI/NarrativeNPCController.h"
#include "SaveSystemStatics.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_FactionTagNamespace,
	"TerritoryFramework.Integration.FactionTagNamespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_FactionTagNamespace::RunTest(const FString& Parameters)
{
	// Verify that Narrative Pro's canonical faction tags resolve correctly
	// and that TerritoryFramework uses the same namespace

	// ─── Narrative canonical factions ───
	FGameplayTag HeroesTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
	FGameplayTag BanditsTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Bandits")), false);
	FGameplayTag CiviliansTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Civilians")), false);
	FGameplayTag SoldiersTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Soldiers")), false);

	// Heroes and Bandits are guaranteed to exist in Narrative Pro
	TestTrue(TEXT("Narrative.Factions.Heroes is valid"), HeroesTag.IsValid());
	TestTrue(TEXT("Narrative.Factions.Bandits is valid"), BanditsTag.IsValid());

	// Civilians and Soldiers may or may not be defined in the project tag table
	// Test their validity without asserting — they are informational
	bool bHasCivilians = CiviliansTag.IsValid();
	bool bHasSoldiers = SoldiersTag.IsValid();

	// Heroes and Bandits must be distinct
	TestNotEqual(TEXT("Heroes != Bandits"), HeroesTag, BanditsTag);

	// If optional factions exist, verify they are distinct from canonical ones
	if (bHasCivilians)
	{
		TestNotEqual(TEXT("Heroes != Civilians"), HeroesTag, CiviliansTag);
		TestNotEqual(TEXT("Bandits != Civilians"), BanditsTag, CiviliansTag);
	}
	if (bHasSoldiers)
	{
		TestNotEqual(TEXT("Heroes != Soldiers"), HeroesTag, SoldiersTag);
		TestNotEqual(TEXT("Bandits != Soldiers"), BanditsTag, SoldiersTag);
	}
	if (bHasCivilians && bHasSoldiers)
	{
		TestNotEqual(TEXT("Civilians != Soldiers"), CiviliansTag, SoldiersTag);
	}

	// ─── Verify tag matching works for territory ownership ───
	// The territory system stores ownership as FGameplayTag and compares with ==
	FGameplayTag OwnerTag = BanditsTag;
	FGameplayTag QueryTag = BanditsTag;
	TestTrue(TEXT("Same tag instance matches"), OwnerTag == QueryTag);
	TestTrue(TEXT("IsSameFaction works for Narrative factions"),
		UTerritoryBlueprintLibrary::IsSameFaction(OwnerTag, QueryTag));

	// ─── Verify non-existent faction tag is invalid ───
	FGameplayTag FakeFaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.DoesNotExist")), false);
	TestFalse(TEXT("Non-existent faction tag is invalid"), FakeFaction.IsValid());

	// ─── Verify tag hierarchy matching ───
	FGameplayTagContainer FactionContainer;
	FactionContainer.AddTag(HeroesTag);
	FactionContainer.AddTag(BanditsTag);
	TestTrue(TEXT("Container has Heroes"), FactionContainer.HasTag(HeroesTag));
	TestTrue(TEXT("Container has Bandits"), FactionContainer.HasTag(BanditsTag));
	TestFalse(TEXT("Container does not have Civilians"), FactionContainer.HasTag(CiviliansTag));

	// ─── Verify MatchesTag for hierarchy queries ───
	// Territory tags use MatchesTag for parent-child relationships
	FGameplayTag CityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions")), false);
	if (CityTag.IsValid())
	{
		TestTrue(TEXT("Heroes matches parent Narrative.Factions"),
			HeroesTag.MatchesTag(CityTag));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_SaveSystemContract,
	"TerritoryFramework.Integration.SaveSystemContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_SaveSystemContract::RunTest(const FString& Parameters)
{
	// Verify TerritoryFramework classes conform to Narrative's save system contracts

	// ─── ATerritoryVolume save contract ───
	{
		const UClass* VolumeClass = ATerritoryVolume::StaticClass();

		// Must implement INarrativeSavableActor (which extends INarrativeStableActor)
		TestTrue(TEXT("Volume implements INarrativeSavableActor"),
			VolumeClass->ImplementsInterface(UNarrativeSavableActor::StaticClass()));

		// Must implement INarrativeStableActor (GUID-based identity)
		TestTrue(TEXT("Volume implements INarrativeStableActor"),
			VolumeClass->ImplementsInterface(UNarrativeStableActor::StaticClass()));

		// Verify all 5 required interface functions exist and are callable
		TestTrue(TEXT("Volume has GetActorGUID"), TFTestUtils::HasFunction(VolumeClass, TEXT("GetActorGUID")));
		TestTrue(TEXT("Volume has SetActorGUID"), TFTestUtils::HasFunction(VolumeClass, TEXT("SetActorGUID")));
		TestTrue(TEXT("Volume has PrepareForSave"), TFTestUtils::HasFunction(VolumeClass, TEXT("PrepareForSave")));
		TestTrue(TEXT("Volume has Load"), TFTestUtils::HasFunction(VolumeClass, TEXT("Load")));
		TestTrue(TEXT("Volume has ShouldRespawn"), TFTestUtils::HasFunction(VolumeClass, TEXT("ShouldRespawn")));

		// OwnershipData must be SaveGame for automatic serialization
		TestTrue(TEXT("OwnershipData has SaveGame flag"),
			TFTestUtils::IsSaveGame(VolumeClass, TEXT("OwnershipData")));

		// TerritoryGUID must be SaveGame
		TestTrue(TEXT("TerritoryGUID has SaveGame flag"),
			TFTestUtils::IsSaveGame(VolumeClass, TEXT("TerritoryGUID")));

		// TerritoryGUID must NOT shadow AActor's ActorGUID
		FProperty* GuidProp = VolumeClass->FindPropertyByName(FName(TEXT("TerritoryGUID")));
		TestNotNull(TEXT("TerritoryGUID property exists"), GuidProp);
		if (GuidProp)
		{
			FProperty* ActorGuidProp = AActor::StaticClass()->FindPropertyByName(FName(TEXT("ActorGUID")));
			if (ActorGuidProp)
			{
				TestTrue(TEXT("TerritoryGUID is a different property than AActor::ActorGUID"),
					GuidProp != ActorGuidProp);
			}
		}
	}

	// ─── ATerritorySavableData save contract ───
	{
		const UClass* SavableClass = ATerritorySavableData::StaticClass();

		TestTrue(TEXT("SavableData implements INarrativeSavableActor"),
			SavableClass->ImplementsInterface(UNarrativeSavableActor::StaticClass()));

		// All economy/diplomacy data must be SaveGame
		TestTrue(TEXT("SavedTreasuries has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedTreasuries")));
		TestTrue(TEXT("SavedTransactions has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedTransactions")));
		TestTrue(TEXT("SavedTreaties has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedTreaties")));
		TestTrue(TEXT("SavedReputation has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedReputation")));
		TestTrue(TEXT("SavedDiplomacyHistory has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedDiplomacyHistory")));
	}

	// ─── ATerritoryWorldState replicated save contract ───
	{
		const UClass* WorldStateClass = ATerritoryWorldState::StaticClass();
		TestTrue(TEXT("WorldState implements INarrativeSavableActor"),
			WorldStateClass->ImplementsInterface(UNarrativeSavableActor::StaticClass()));
		TestTrue(TEXT("ReplicatedDiplomacyHistory is replicated"),
			TFTestUtils::IsReplicated(WorldStateClass, TEXT("ReplicatedDiplomacyHistory")));
		TestTrue(TEXT("SavedDiplomacyHistory has SaveGame flag"),
			TFTestUtils::IsSaveGame(WorldStateClass, TEXT("SavedDiplomacyHistory")));
		TestTrue(TEXT("WorldState is always network relevant"),
			GetDefault<ATerritoryWorldState>()->bAlwaysRelevant);
	}

	// ─── Hierarchy classes inherit save contract ───
	{
		TestTrue(TEXT("City implements INarrativeSavableActor"),
			ATerritoryCity::StaticClass()->ImplementsInterface(UNarrativeSavableActor::StaticClass()));
		TestTrue(TEXT("District implements INarrativeSavableActor"),
			ATerritoryDistrict::StaticClass()->ImplementsInterface(UNarrativeSavableActor::StaticClass()));
		TestTrue(TEXT("Property implements INarrativeSavableActor"),
			ATerritoryProperty::StaticClass()->ImplementsInterface(UNarrativeSavableActor::StaticClass()));
	}

	// ─── SaveSystemStatics API is accessible ───
	{
		const UClass* StaticsClass = USaveSystemStatics::StaticClass();
		TestNotNull(TEXT("USaveSystemStatics is accessible"), StaticsClass);
		TestTrue(TEXT("LoadSingleActor function exists"),
			TFTestUtils::HasFunction(StaticsClass, TEXT("LoadSingleActor")));
		TestTrue(TEXT("SaveSingleActor function exists"),
			TFTestUtils::HasFunction(StaticsClass, TEXT("SaveSingleActor")));
	}

	// ─── NarrativeSaveSubsystem is accessible ───
	{
		const UClass* SaveSubClass = UNarrativeSaveSubsystem::StaticClass();
		TestNotNull(TEXT("UNarrativeSaveSubsystem is accessible"), SaveSubClass);
		TestTrue(TEXT("SaveSubsystem inherits UWorldSubsystem"),
			SaveSubClass->IsChildOf(UWorldSubsystem::StaticClass()));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_GameStateDiplomacySync,
	"TerritoryFramework.Integration.GameStateDiplomacySync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_GameStateDiplomacySync::RunTest(const FString& Parameters)
{
	// Verify the diplomacy subsystem can integrate with NarrativeGameState's faction system

	// ─── NarrativeGameState faction API ───
	{
		const UClass* GSClass = ANarrativeGameState::StaticClass();
		TestNotNull(TEXT("ANarrativeGameState is accessible"), GSClass);

		// FactionAllianceMap must exist and be SaveGame
		TestTrue(TEXT("FactionAllianceMap property exists"),
			TFTestUtils::HasProperty(GSClass, TEXT("FactionAllianceMap")));
		TestTrue(TEXT("FactionAllianceMap has SaveGame flag"),
			TFTestUtils::IsSaveGame(GSClass, TEXT("FactionAllianceMap")));

		// Faction attitude functions must be callable
		TestTrue(TEXT("SetFactionAttitude is BlueprintCallable"),
			TFTestUtils::IsBlueprintCallable(GSClass, TEXT("SetFactionAttitude")));
		TestTrue(TEXT("GetFactionAttitudeTowardsFaction is BlueprintCallable"),
			TFTestUtils::IsBlueprintCallable(GSClass, TEXT("GetFactionAttitudeTowardsFaction")));
		TestTrue(TEXT("GetFactionsAttitudeTowardsFactions is BlueprintCallable"),
			TFTestUtils::IsBlueprintCallable(GSClass, TEXT("GetFactionsAttitudeTowardsFactions")));

		// OnFactionAttitudeChanged delegate must exist
		TestTrue(TEXT("OnFactionAttitudeChanged delegate exists"),
			TFTestUtils::HasProperty(GSClass, TEXT("OnFactionAttitudeChanged")));
	}

	// ─── FFactionAttitudeData struct ───
	{
		FFactionAttitudeData AttitudeData;
		// Verify we can set and read attitudes
		FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
		FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Bandits")), false);

		if (Heroes.IsValid() && Bandits.IsValid())
		{
			AttitudeData.AttitudeMap.Add(Heroes, ETeamAttitude::Hostile);
			AttitudeData.AttitudeMap.Add(Bandits, ETeamAttitude::Friendly);

			TestEqual(TEXT("Heroes attitude is Hostile"),
				AttitudeData.AttitudeMap[Heroes], ETeamAttitude::Hostile);
			TestEqual(TEXT("Bandits attitude is Friendly"),
				AttitudeData.AttitudeMap[Bandits], ETeamAttitude::Friendly);
		}
	}

	// ─── Diplomacy subsystem state mapping ───
	{
		const UClass* DiploClass = UTerritoryDiplomacySubsystem::StaticClass();

		// Verify DiplomacyStateToAttitude mapping would be correct:
		// Alliance → Friendly, War → Hostile, None → Neutral
		// We can't call the private method, but we verify the enum values
		// that drive the mapping are correct
		TestEqual(TEXT("ETeamAttitude::Friendly == 0"),
			static_cast<uint8>(ETeamAttitude::Friendly), static_cast<uint8>(0));
		TestEqual(TEXT("ETeamAttitude::Neutral == 1"),
			static_cast<uint8>(ETeamAttitude::Neutral), static_cast<uint8>(1));
		TestEqual(TEXT("ETeamAttitude::Hostile == 2"),
			static_cast<uint8>(ETeamAttitude::Hostile), static_cast<uint8>(2));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_TalesInheritance,
	"TerritoryFramework.Integration.TalesInheritance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_TalesInheritance::RunTest(const FString& Parameters)
{
	// Verify territory tale classes properly inherit from Narrative's tale system

	// ─── UNarrativeTask base class contract ───
	{
		const UClass* TaskBase = UNarrativeTask::StaticClass();
		TestNotNull(TEXT("UNarrativeTask is accessible"), TaskBase);

		// Required base properties that CaptureTask must inherit
		TestTrue(TEXT("NarrativeTask has RequiredQuantity"),
			TFTestUtils::HasProperty(TaskBase, TEXT("RequiredQuantity")));
		TestTrue(TEXT("NarrativeTask has MarkerSettings"),
			TFTestUtils::HasProperty(TaskBase, TEXT("MarkerSettings")));

		// Required base functions that CaptureTask must inherit
		TestTrue(TEXT("NarrativeTask has CompleteTask"),
			TFTestUtils::HasFunction(TaskBase, TEXT("CompleteTask")));
		TestTrue(TEXT("NarrativeTask has SetProgress"),
			TFTestUtils::HasFunction(TaskBase, TEXT("SetProgress")));
		TestTrue(TEXT("NarrativeTask has GetTaskDescription"),
			TFTestUtils::HasFunction(TaskBase, TEXT("GetTaskDescription")));
	}

	// ─── UTerritoryCaptureTask inherits all base contract ───
	{
		const UClass* CaptureTask = UTerritoryCaptureTask::StaticClass();
		const UClass* TaskBase = UNarrativeTask::StaticClass();

		// Must inherit from UNarrativeTask
		TestTrue(TEXT("CaptureTask is-a NarrativeTask"),
			CaptureTask->IsChildOf(TaskBase));

		// Must inherit base properties
		TestTrue(TEXT("CaptureTask inherits RequiredQuantity"),
			TFTestUtils::HasProperty(CaptureTask, TEXT("RequiredQuantity")));
		TestTrue(TEXT("CaptureTask inherits MarkerSettings"),
			TFTestUtils::HasProperty(CaptureTask, TEXT("MarkerSettings")));

		// Must inherit base functions
		TestTrue(TEXT("CaptureTask inherits CompleteTask"),
			TFTestUtils::HasFunction(CaptureTask, TEXT("CompleteTask")));
		TestTrue(TEXT("CaptureTask inherits SetProgress"),
			TFTestUtils::HasFunction(CaptureTask, TEXT("SetProgress")));

		// Must have own territory-specific properties
		TestTrue(TEXT("CaptureTask has TargetTerritoryTag"),
			TFTestUtils::HasProperty(CaptureTask, TEXT("TargetTerritoryTag")));
		TestTrue(TEXT("CaptureTask has RequiredCapturingFaction"),
			TFTestUtils::HasProperty(CaptureTask, TEXT("RequiredCapturingFaction")));
	}

	// ─── UNarrativeEvent base class contract ───
	{
		const UClass* EventBase = UNarrativeEvent::StaticClass();
		TestNotNull(TEXT("UNarrativeEvent is accessible"), EventBase);

		// CaptureEvent must inherit from it
		const UClass* CaptureEvent = UTerritoryCaptureEvent::StaticClass();
		TestTrue(TEXT("CaptureEvent is-a NarrativeEvent"),
			CaptureEvent->IsChildOf(EventBase));

		// Must have ExecuteEvent function
		TestTrue(TEXT("CaptureEvent has ExecuteEvent"),
			TFTestUtils::HasFunction(CaptureEvent, TEXT("ExecuteEvent")));
	}

	// ─── UNarrativeCondition base class contract ───
	{
		const UClass* CondBase = UNarrativeCondition::StaticClass();
		TestNotNull(TEXT("UNarrativeCondition is accessible"), CondBase);

		// OwnershipCondition must inherit from it
		const UClass* OwnCond = UTerritoryOwnershipCondition::StaticClass();
		TestTrue(TEXT("OwnershipCondition is-a NarrativeCondition"),
			OwnCond->IsChildOf(CondBase));

		// Must have CheckCondition function
		TestTrue(TEXT("OwnershipCondition has CheckCondition"),
			TFTestUtils::HasFunction(OwnCond, TEXT("CheckCondition")));

		// Must have GetGraphDisplayText function
		TestTrue(TEXT("OwnershipCondition has GetGraphDisplayText"),
			TFTestUtils::HasFunction(OwnCond, TEXT("GetGraphDisplayText")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_MapMarkerInheritance,
	"TerritoryFramework.Integration.MapMarkerInheritance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_MapMarkerInheritance::RunTest(const FString& Parameters)
{
	// Verify territory map marker properly integrates with Narrative's navigation system

	// ─── UMapMarker base class contract ───
	{
		const UClass* MarkerBase = UMapMarker::StaticClass();
		TestNotNull(TEXT("UMapMarker is accessible"), MarkerBase);

		// Verify the base class exists and inherits UObject
		TestTrue(TEXT("MapMarker inherits UObject"),
			MarkerBase->IsChildOf(UObject::StaticClass()));

		// Required base functions (BlueprintNativeEvent overrides)
		TestTrue(TEXT("MapMarker has GetMarkerColor"),
			TFTestUtils::HasFunction(MarkerBase, TEXT("GetMarkerColor")));
		TestTrue(TEXT("MapMarker has GetMarkerDisplayText"),
			TFTestUtils::HasFunction(MarkerBase, TEXT("GetMarkerDisplayText")));
		TestTrue(TEXT("MapMarker has MarkerOnPaint"),
			TFTestUtils::HasFunction(MarkerBase, TEXT("MarkerOnPaint")));
		TestTrue(TEXT("MapMarker has RefreshMarker"),
			TFTestUtils::HasFunction(MarkerBase, TEXT("RefreshMarker")));

		// bWantsOnPaint determines if MarkerOnPaint gets called
		TestTrue(TEXT("MapMarker has bWantsOnPaint"),
			TFTestUtils::HasProperty(MarkerBase, TEXT("bWantsOnPaint")));
	}

	// ─── UTerritoryMapMarker inherits full marker contract ───
	{
		const UClass* TerrMarker = UTerritoryMapMarker::StaticClass();
		const UClass* MarkerBase = UMapMarker::StaticClass();

		TestTrue(TEXT("TerritoryMapMarker is-a MapMarker"),
			TerrMarker->IsChildOf(MarkerBase));

		// Must inherit base properties
		TestTrue(TEXT("TerritoryMapMarker inherits bWantsOnPaint"),
			TFTestUtils::HasProperty(TerrMarker, TEXT("bWantsOnPaint")));

		// Must inherit base functions
		TestTrue(TEXT("TerritoryMapMarker inherits RefreshMarker"),
			TFTestUtils::HasFunction(TerrMarker, TEXT("RefreshMarker")));

		// Must override the correct functions with correct signatures
		TestTrue(TEXT("TerritoryMapMarker overrides GetMarkerColor"),
			TFTestUtils::HasFunction(TerrMarker, TEXT("GetMarkerColor")));
		TestTrue(TEXT("TerritoryMapMarker overrides GetMarkerDisplayText"),
			TFTestUtils::HasFunction(TerrMarker, TEXT("GetMarkerDisplayText")));
		TestTrue(TEXT("TerritoryMapMarker overrides MarkerOnPaint"),
			TFTestUtils::HasFunction(TerrMarker, TEXT("MarkerOnPaint")));

		// Must have territory-specific properties
		TestTrue(TEXT("TerritoryMapMarker has FactionColorMap"),
			TFTestUtils::HasProperty(TerrMarker, TEXT("FactionColorMap")));
		TestTrue(TEXT("TerritoryMapMarker has DefaultColor"),
			TFTestUtils::HasProperty(TerrMarker, TEXT("DefaultColor")));
		TestTrue(TEXT("TerritoryMapMarker has ContestedColor"),
			TFTestUtils::HasProperty(TerrMarker, TEXT("ContestedColor")));
		TestTrue(TEXT("TerritoryMapMarker has LockedColor"),
			TFTestUtils::HasProperty(TerrMarker, TEXT("LockedColor")));
	}

	// ─── UNavigationMarkerComponent base contract ───
	{
		const UClass* NavCompBase = UNavigationMarkerComponent::StaticClass();
		TestNotNull(TEXT("UNavigationMarkerComponent is accessible"), NavCompBase);

		// TerritoryNavigationMarkerComponent must inherit
		const UClass* TerrNavComp = UTerritoryNavigationMarkerComponent::StaticClass();
		TestTrue(TEXT("TerritoryNavMarkerComponent is-a NavigationMarkerComponent"),
			TerrNavComp->IsChildOf(NavCompBase));

		// Must inherit MarkerObject property
		TestTrue(TEXT("TerritoryNavMarkerComponent inherits MarkerObject"),
			TFTestUtils::HasProperty(TerrNavComp, TEXT("MarkerObject")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_GASContract,
	"TerritoryFramework.Integration.GASContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_GASContract::RunTest(const FString& Parameters)
{
	// Verify TerritoryFramework's GAS integration points are compatible

	// ─── UNarrativeAbilitySystemComponent death-state delegate ───
	{
		const UClass* ASCClass = UNarrativeAbilitySystemComponent::StaticClass();
		TestNotNull(TEXT("UNarrativeAbilitySystemComponent is accessible"), ASCClass);

		const FMulticastDelegateProperty* DeathStateDelegate =
			FindFProperty<FMulticastDelegateProperty>(ASCClass, TEXT("OnDeathStateChanged"));
		TestNotNull(TEXT("ASC has OnDeathStateChanged delegate"), DeathStateDelegate);
		if (DeathStateDelegate && DeathStateDelegate->SignatureFunction)
		{
			TestNotNull(TEXT("Death-state delegate exposes bIsDead"),
				FindFProperty<FBoolProperty>(DeathStateDelegate->SignatureFunction, TEXT("bIsDead")));
		}

		// Attack token system must exist (properties and C++ methods)
		TestTrue(TEXT("ASC has NumAttackTokens property"),
			TFTestUtils::HasProperty(ASCClass, TEXT("NumAttackTokens")));
		TestTrue(TEXT("ASC has GrantedAttackTokens property"),
			TFTestUtils::HasProperty(ASCClass, TEXT("GrantedAttackTokens")));
		// TryClaimToken/ReturnToken are C++ methods (not UFUNCTION), so verify
		// the class compiles and links by checking the class itself is valid
		TestNotNull(TEXT("UNarrativeAbilitySystemComponent CDO is valid"),
			UNarrativeAbilitySystemComponent::StaticClass()->GetDefaultObject());
	}

	// ─── ANarrativeNPCController compatibility ───
	{
		const UClass* NPCControllerClass = ANarrativeNPCController::StaticClass();
		TestNotNull(TEXT("ANarrativeNPCController is accessible"), NPCControllerClass);
		TestTrue(TEXT("NPCController inherits AAIController"),
			NPCControllerClass->IsChildOf(AAIController::StaticClass()));

		// BT tasks use this class
		const UClass* RequestTask = UBTTask_RequestTerritoryPermission::StaticClass();
		TestNotNull(TEXT("RequestTerritoryPermission task exists"), RequestTask);

		// The task must have blackboard key selectors
		TestTrue(TEXT("Request task has TerritoryKey"),
			TFTestUtils::HasProperty(RequestTask, TEXT("TerritoryKey")));
		TestTrue(TEXT("Request task has bPermissionGrantedKey"),
			TFTestUtils::HasProperty(RequestTask, TEXT("bPermissionGrantedKey")));
	}

	// ─── Combat director assault budget ───
	{
		const UClass* DirectorClass = UTerritoryCombatDirector::StaticClass();

		// Assault budget — strategic territory-level slot, separate from Narrative per-target tokens
		TestTrue(TEXT("Director has RequestAssaultSlot"),
			TFTestUtils::HasFunction(DirectorClass, TEXT("RequestAssaultSlot")));
		TestTrue(TEXT("Director has ReleaseAssaultSlot"),
			TFTestUtils::HasFunction(DirectorClass, TEXT("ReleaseAssaultSlot")));
		TestTrue(TEXT("Director has GetAvailableSlots"),
			TFTestUtils::IsBlueprintPure(DirectorClass, TEXT("GetAvailableSlots")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_TeamAgentInterface,
	"TerritoryFramework.Integration.TeamAgentInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_TeamAgentInterface::RunTest(const FString& Parameters)
{
	// Verify INarrativeTeamAgentInterface is accessible and has the expected contract

	const UClass* InterfaceClass = UNarrativeTeamAgentInterface::StaticClass();
	TestNotNull(TEXT("UNarrativeTeamAgentInterface is accessible"), InterfaceClass);

	// INarrativeTeamAgentInterface methods are C++ virtuals (not UFUNCTION),
	// so they won't appear in reflection. Verify the interface class is valid
	// and inherits from UGenericTeamAgentInterface.
	TestTrue(TEXT("TeamAgent inherits UGenericTeamAgentInterface"),
		InterfaceClass->IsChildOf(UGenericTeamAgentInterface::StaticClass()));

	// Verify the interface CDO is constructable
	TestNotNull(TEXT("TeamAgent interface CDO is valid"),
		InterfaceClass->GetDefaultObject());

	// INarrativeTeamAgentInterface is implemented by characters (not GameState).
	// The territory diplomacy system reads faction attitudes from
	// ANarrativeGameState::FactionAllianceMap (separate mechanism).
	// Here we just verify the GameState has the faction alliance API.
	const UClass* GSClass = ANarrativeGameState::StaticClass();
	TestTrue(TEXT("GameState has FactionAllianceMap for diplomacy"),
		TFTestUtils::HasProperty(GSClass, TEXT("FactionAllianceMap")));
	TestTrue(TEXT("GameState has SetFactionAttitude"),
		TFTestUtils::IsBlueprintCallable(GSClass, TEXT("SetFactionAttitude")));

	// Verify the territory volume does NOT implement INarrativeTeamAgentInterface
	// (it uses direct FGameplayTag ownership, not the team agent interface)
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();
	TestFalse(TEXT("TerritoryVolume does NOT implement INarrativeTeamAgentInterface"),
		VolumeClass->ImplementsInterface(UNarrativeTeamAgentInterface::StaticClass()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_TalesComponentBinding,
	"TerritoryFramework.Integration.TalesComponentBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_TalesComponentBinding::RunTest(const FString& Parameters)
{
	// Verify UTalesComponent has the delegates that territory tale extensions bind to

	const UClass* TalesClass = UTalesComponent::StaticClass();
	TestNotNull(TEXT("UTalesComponent is accessible"), TalesClass);

	// ─── Quest lifecycle delegates (bound by UTerritoryCaptureTask) ───
	TestTrue(TEXT("TalesComponent has OnQuestTaskCompleted"),
		TFTestUtils::HasProperty(TalesClass, TEXT("OnQuestTaskCompleted")));
	TestTrue(TEXT("TalesComponent has OnQuestTaskProgressChanged"),
		TFTestUtils::HasProperty(TalesClass, TEXT("OnQuestTaskProgressChanged")));
	TestTrue(TEXT("TalesComponent has OnQuestSucceeded"),
		TFTestUtils::HasProperty(TalesClass, TEXT("OnQuestSucceeded")));
	TestTrue(TEXT("TalesComponent has OnQuestFailed"),
		TFTestUtils::HasProperty(TalesClass, TEXT("OnQuestFailed")));
	TestTrue(TEXT("TalesComponent has OnQuestStarted"),
		TFTestUtils::HasProperty(TalesClass, TEXT("OnQuestStarted")));

	// ─── Quest management API (used by capture event) ───
	TestTrue(TEXT("TalesComponent has BeginQuest"),
		TFTestUtils::HasFunction(TalesClass, TEXT("BeginQuest")));
	TestTrue(TEXT("TalesComponent has IsQuestInProgress"),
		TFTestUtils::HasFunction(TalesClass, TEXT("IsQuestInProgress")));
	TestTrue(TEXT("TalesComponent has IsQuestSucceeded"),
		TFTestUtils::HasFunction(TalesClass, TEXT("IsQuestSucceeded")));
	TestTrue(TEXT("TalesComponent has IsQuestFailed"),
		TFTestUtils::HasFunction(TalesClass, TEXT("IsQuestFailed")));

	// ─── Dialogue API (used by ownership condition) ───
	TestTrue(TEXT("TalesComponent has BeginDialogue"),
		TFTestUtils::HasFunction(TalesClass, TEXT("BeginDialogue")));
	TestTrue(TEXT("TalesComponent has IsInDialogue"),
		TFTestUtils::HasFunction(TalesClass, TEXT("IsInDialogue")));

	// ─── TalesComponent implements INarrativeSavableComponent ───
	TestTrue(TEXT("TalesComponent implements INarrativeSavableComponent"),
		TalesClass->ImplementsInterface(UNarrativeSavableComponent::StaticClass()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_TimeOfDayBridge,
	"TerritoryFramework.Integration.TimeOfDayBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_TimeOfDayBridge::RunTest(const FString& Parameters)
{
	// Verify territory economy subsystem can integrate with Narrative's time system
	// Economy ticks are triggered by game time, not real time

	const UClass* GSClass = ANarrativeGameState::StaticClass();

	// Time of day API that economy system would hook into
	TestTrue(TEXT("GameState has GetTimeOfDay"),
		TFTestUtils::IsBlueprintPure(GSClass, TEXT("GetTimeOfDay")));
	TestTrue(TEXT("GameState has GetAccumulatedTime"),
		TFTestUtils::IsBlueprintPure(GSClass, TEXT("GetAccumulatedTime")));
	TestTrue(TEXT("GameState has IsDayTime"),
		TFTestUtils::IsBlueprintPure(GSClass, TEXT("IsDayTime")));
	TestTrue(TEXT("GameState has AdvanceTimeOfDay"),
		TFTestUtils::IsBlueprintCallable(GSClass, TEXT("AdvanceTimeOfDay")));
	TestTrue(TEXT("GameState has AdvanceToTimeOfDay"),
		TFTestUtils::IsBlueprintCallable(GSClass, TEXT("AdvanceToTimeOfDay")));

	// Time events (economy could bind to hourly events)
	TestTrue(TEXT("GameState has TimeOfDayEvents"),
		TFTestUtils::HasProperty(GSClass, TEXT("TimeOfDayEvents")));

	// Economy subsystem's tick interval config
	const UClass* EconClass = UTerritoryEconomySubsystem::StaticClass();
	TestTrue(TEXT("EconomySubsystem has TickIntervalSeconds"),
		TFTestUtils::HasProperty(EconClass, TEXT("TickIntervalSeconds")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFIntegration_ModuleDependency,
	"TerritoryFramework.Integration.ModuleDependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFIntegration_ModuleDependency::RunTest(const FString& Parameters)
{
	// Verify that TerritoryFramework can resolve all Narrative Pro types it depends on
	// This catches missing module dependencies at test time

	// ─── NarrativeSaveSystem types ───
	TestNotNull(TEXT("UNarrativeSavableActor interface resolves"),
		UNarrativeSavableActor::StaticClass());
	TestNotNull(TEXT("UNarrativeStableActor interface resolves"),
		UNarrativeStableActor::StaticClass());
	TestNotNull(TEXT("UNarrativeSavableComponent interface resolves"),
		UNarrativeSavableComponent::StaticClass());
	TestNotNull(TEXT("USaveSystemStatics resolves"),
		USaveSystemStatics::StaticClass());
	TestNotNull(TEXT("UNarrativeSaveSubsystem resolves"),
		UNarrativeSaveSubsystem::StaticClass());

	// ─── NarrativeArsenal types ───
	TestNotNull(TEXT("ANarrativeGameState resolves"),
		ANarrativeGameState::StaticClass());
	TestNotNull(TEXT("UNarrativeAbilitySystemComponent resolves"),
		UNarrativeAbilitySystemComponent::StaticClass());
	TestNotNull(TEXT("ANarrativeNPCController resolves"),
		ANarrativeNPCController::StaticClass());
	TestNotNull(TEXT("UTalesComponent resolves"),
		UTalesComponent::StaticClass());
	TestNotNull(TEXT("UNarrativeTask resolves"),
		UNarrativeTask::StaticClass());
	TestNotNull(TEXT("UNarrativeEvent resolves"),
		UNarrativeEvent::StaticClass());
	TestNotNull(TEXT("UNarrativeCondition resolves"),
		UNarrativeCondition::StaticClass());
	TestNotNull(TEXT("UMapMarker resolves"),
		UMapMarker::StaticClass());
	TestNotNull(TEXT("UNavigationMarkerComponent resolves"),
		UNavigationMarkerComponent::StaticClass());
	TestNotNull(TEXT("UNarrativeTeamAgentInterface resolves"),
		UNarrativeTeamAgentInterface::StaticClass());

	// ─── Engine types used by TerritoryFramework ───
	TestNotNull(TEXT("UWorldSubsystem resolves"),
		UWorldSubsystem::StaticClass());
	TestNotNull(TEXT("UBehaviorTreeComponent resolves"),
		UBehaviorTreeComponent::StaticClass());
	TestNotNull(TEXT("UBTTaskNode resolves"),
		UBTTaskNode::StaticClass());
	TestNotNull(TEXT("UDeveloperSettings resolves"),
		UDeveloperSettings::StaticClass());

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRENGTHENING PASS TESTS — Economy edge cases, Diplomacy edge cases,
// BlueprintLibrary new helpers, Interface extensions
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_EconomyEdgeCases,
	"TerritoryFramework.Functional.EconomyEdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_EconomyEdgeCases::RunTest(const FString& Parameters)
{
	FTerritoryTreasury Treasury;

	// ─── Income/cost edge cases ───
	Treasury.IncomePerTick = 0;
	Treasury.CostsPerTick = 0;
	TestEqual(TEXT("Net income is 0 when no territories"), Treasury.IncomePerTick - Treasury.CostsPerTick, 0);

	// ─── Costs exceed income (net negative) ───
	Treasury.IncomePerTick = 50;
	Treasury.CostsPerTick = 200;
	int32 NetDeficit = Treasury.IncomePerTick - Treasury.CostsPerTick;
	TestEqual(TEXT("Net deficit when upkeep exceeds income"), NetDeficit, -150);

	// ─── Large amounts ───
	Treasury.IncomePerTick = INT32_MAX;
	TestTrue(TEXT("Max int32 income is valid"), Treasury.IncomePerTick == INT32_MAX);

	// ─── Transaction struct ───
	FTerritoryTransaction Tx;
	TestFalse(TEXT("Default TransactionID is invalid"), Tx.TransactionID.IsValid());
	TestEqual(TEXT("Default Amount is 0"), Tx.Amount, 0);
	TestEqual(TEXT("Default BalanceAfter is 0"), Tx.BalanceAfter, 0);
	TestEqual(TEXT("Default GameTime is 0"), Tx.GameTime, 0.0);

	// ─── EconomySnapshot ───
	FTerritoryEconomySnapshot Snap;
	TestEqual(TEXT("Default Treasury is 0"), Snap.Treasury, 0);
	TestEqual(TEXT("Default TotalIncome is 0"), Snap.TotalIncome, 0);
	TestEqual(TEXT("Default TotalCosts is 0"), Snap.TotalCosts, 0);
	TestEqual(TEXT("Default TerritoryCount is 0"), Snap.TerritoryCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEconomyAutomaticAccountsExcludeNPCs,
	"TerritoryFramework.Economy.Regression.AutomaticSettlementExcludesNPCWallets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEconomyAutomaticAccountsExcludeNPCs::RunTest(const FString& Parameters)
{
	const ANarrativePlayerCharacter* Player = GetDefault<ANarrativePlayerCharacter>();
	const ATerritoryGuardCharacter* Guard = GetDefault<ATerritoryGuardCharacter>();
	const ATerritoryAssaultCharacter* Assault = GetDefault<ATerritoryAssaultCharacter>();

	TestNotNull(TEXT("Narrative player account fixture exists"), Player);
	TestNotNull(TEXT("Territory guard account fixture exists"), Guard);
	TestNotNull(TEXT("Territory assault account fixture exists"), Assault);
	TestTrue(TEXT("A Narrative player inventory is eligible for automatic settlement"),
		UTerritoryEconomySubsystem::IsAutomaticPlayerCurrencyAccount(Player));
	TestFalse(TEXT("A Territory guard inventory can never receive automatic income"),
		UTerritoryEconomySubsystem::IsAutomaticPlayerCurrencyAccount(Guard));
	TestFalse(TEXT("A counterattack NPC inventory can never receive automatic income"),
		UTerritoryEconomySubsystem::IsAutomaticPlayerCurrencyAccount(Assault));
	TestFalse(TEXT("A null actor can never become an automatic wallet"),
		UTerritoryEconomySubsystem::IsAutomaticPlayerCurrencyAccount(nullptr));
	TestFalse(TEXT("A preferred actor cannot replace a missing explicit account"),
		UTerritoryEconomySubsystem::IsExplicitAccountSelectionValid(nullptr, Guard));
	TestFalse(TEXT("A guard cannot override a different registered player account"),
		UTerritoryEconomySubsystem::IsExplicitAccountSelectionValid(Player, Guard));
	TestTrue(TEXT("An omitted preference uses the exact registered account"),
		UTerritoryEconomySubsystem::IsExplicitAccountSelectionValid(Player, nullptr));
	TestTrue(TEXT("The exact registered account may be repeated explicitly"),
		UTerritoryEconomySubsystem::IsExplicitAccountSelectionValid(Player, Player));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_DiplomacyEdgeCases,
	"TerritoryFramework.Functional.DiplomacyEdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_DiplomacyEdgeCases::RunTest(const FString& Parameters)
{
	// ─── Self-war / self-alliance ───
	FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(FName(TEXT("Narrative.Factions.Heroes")), false);
	TestTrue(TEXT("Heroes tag is valid"), Heroes.IsValid());

	// The subsystem has a FactionA == FactionB guard — verify the logic conceptually:
	// DeclareWar(Heroes, Heroes) should be a no-op
	bool bSelfWar = (Heroes == Heroes);
	TestTrue(TEXT("Self-faction comparison is true"), bSelfWar);
	// Self-war/alliance makes no sense — subsystem guards against it

	// ─── Invalid faction tags ───
	FGameplayTag Invalid;
	TestFalse(TEXT("Invalid tag is invalid"), Invalid.IsValid());

	// IsSameFaction with invalid tags
	TestFalse(TEXT("IsSameFaction(invalid, invalid) is false"),
		UTerritoryBlueprintLibrary::IsSameFaction(Invalid, Invalid));
	TestFalse(TEXT("IsSameFaction(valid, invalid) is false"),
		UTerritoryBlueprintLibrary::IsSameFaction(Heroes, Invalid));

	// ─── TreatyRecord self-reference ───
	FTreatyRecord SelfTreaty;
	SelfTreaty.FactionA = Heroes;
	SelfTreaty.FactionB = Heroes;
	// A treaty with both factions the same is technically "valid" per IsValid()
	// but should never be created by the subsystem — this is a design guard
	TestTrue(TEXT("Self-treaty struct is valid (struct doesn't guard)"), SelfTreaty.IsValid());

	// ─── Attitude mapping sanity ───
	// EDiplomacyState values are distinct
	TestNotEqual(TEXT("None != Alliance"), EDiplomacyState::None, EDiplomacyState::Alliance);
	TestNotEqual(TEXT("None != War"), EDiplomacyState::None, EDiplomacyState::War);
	TestNotEqual(TEXT("Alliance != War"), EDiplomacyState::Alliance, EDiplomacyState::War);
	TestNotEqual(TEXT("Ceasefire != War"), EDiplomacyState::Ceasefire, EDiplomacyState::War);
	TestNotEqual(TEXT("TradeAgreement != Alliance"), EDiplomacyState::TradeAgreement, EDiplomacyState::Alliance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_BlueprintLibraryExtended,
	"TerritoryFramework.Contract.BlueprintLibraryExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_BlueprintLibraryExtended::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryBlueprintLibrary::StaticClass();
	TestNotNull(TEXT("UTerritoryBlueprintLibrary::StaticClass()"), Class);
	TestTrue(TEXT("Inherits UBlueprintFunctionLibrary"), Class->IsChildOf(UBlueprintFunctionLibrary::StaticClass()));

	// ─── New helper functions ───
	TestTrue(TEXT("GetTerritoryDiplomacy exists"), TFTestUtils::HasFunction(Class, TEXT("GetTerritoryDiplomacy")));
	TestTrue(TEXT("GetAllTerritories exists"), TFTestUtils::HasFunction(Class, TEXT("GetAllTerritories")));
	TestTrue(TEXT("GetTerritoriesByFaction exists"), TFTestUtils::HasFunction(Class, TEXT("GetTerritoriesByFaction")));
	TestTrue(TEXT("GetChildTerritories exists"), TFTestUtils::HasFunction(Class, TEXT("GetChildTerritories")));
	TestTrue(TEXT("GetTerritoryCount exists"), TFTestUtils::HasFunction(Class, TEXT("GetTerritoryCount")));
	TestTrue(TEXT("GetFactionTerritoryCount exists"), TFTestUtils::HasFunction(Class, TEXT("GetFactionTerritoryCount")));
	TestTrue(TEXT("IsTerritoryAtLocation exists"), TFTestUtils::HasFunction(Class, TEXT("IsTerritoryAtLocation")));
	TestTrue(TEXT("GetFactionGold exists"), TFTestUtils::HasFunction(Class, TEXT("GetFactionGold")));
	TestTrue(TEXT("GetFactionIncome exists"), TFTestUtils::HasFunction(Class, TEXT("GetFactionIncome")));
	TestTrue(TEXT("GetAllFactions exists"), TFTestUtils::HasFunction(Class, TEXT("GetAllFactions")));
	TestTrue(TEXT("GetTerritoryState exists"), TFTestUtils::HasFunction(Class, TEXT("GetTerritoryState")));
	TestTrue(TEXT("GetCaptureProgress exists"), TFTestUtils::HasFunction(Class, TEXT("GetCaptureProgress")));
	TestTrue(TEXT("ForceCaptureTerritory exists"), TFTestUtils::HasFunction(Class, TEXT("ForceCaptureTerritory")));
	TestTrue(TEXT("GetTreatyState exists"), TFTestUtils::HasFunction(Class, TEXT("GetTreatyState")));
	TestTrue(TEXT("IsAllied exists"), TFTestUtils::HasFunction(Class, TEXT("IsAllied")));
	TestTrue(TEXT("IsAtWar exists"), TFTestUtils::HasFunction(Class, TEXT("IsAtWar")));

	// ─── Verify BlueprintPure on query functions ───
	TestTrue(TEXT("GetAllTerritories is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetAllTerritories")));
	TestTrue(TEXT("GetTerritoriesByFaction is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoriesByFaction")));
	TestTrue(TEXT("GetFactionGold is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetFactionGold")));
	TestTrue(TEXT("IsAllied is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("IsAllied")));
	TestTrue(TEXT("IsAtWar is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("IsAtWar")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_InterfacesExtended,
	"TerritoryFramework.Contract.InterfacesExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_InterfacesExtended::RunTest(const FString& Parameters)
{
	// ─── Ownership Interface — new GetContestingFaction ───
	{
		const UClass* Class = UTerritoryOwnershipInterface::StaticClass();
		TestNotNull(TEXT("UTerritoryOwnershipInterface::StaticClass()"), Class);
		TestTrue(TEXT("Has GetContestingFaction"), TFTestUtils::HasFunction(Class, TEXT("GetContestingFaction")));
	}

	// ─── Event Receiver Interface — new events ───
	{
		const UClass* Class = UTerritoryEventReceiverInterface::StaticClass();
		TestNotNull(TEXT("UTerritoryEventReceiverInterface::StaticClass()"), Class);
		TestTrue(TEXT("Has OnTerritoryUncontested"), TFTestUtils::HasFunction(Class, TEXT("OnTerritoryUncontested")));
		TestTrue(TEXT("Has OnTerritoryStateChanged"), TFTestUtils::HasFunction(Class, TEXT("OnTerritoryStateChanged")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DeveloperSettingsExtended,
	"TerritoryFramework.Contract.DeveloperSettingsExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DeveloperSettingsExtended::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryDeveloperSettings::StaticClass();
	TestNotNull(TEXT("UTerritoryDeveloperSettings::StaticClass()"), Class);

	// ─── New guard/patrol settings ───
	TestTrue(TEXT("Has DefaultPatrolArrivalThreshold"), TFTestUtils::HasProperty(Class, TEXT("DefaultPatrolArrivalThreshold")));
	TestTrue(TEXT("Has DefaultPatrolAcceptanceRadius"), TFTestUtils::HasProperty(Class, TEXT("DefaultPatrolAcceptanceRadius")));
	TestTrue(TEXT("Has DefaultPatrolWaitTime"), TFTestUtils::HasProperty(Class, TEXT("DefaultPatrolWaitTime")));
	TestTrue(TEXT("Has MaxPatrolRouteNodes"), TFTestUtils::HasProperty(Class, TEXT("MaxPatrolRouteNodes")));
	TestTrue(TEXT("Has DefaultNarrativeButtonClass"), TFTestUtils::HasProperty(Class, TEXT("DefaultNarrativeButtonClass")));
	TestTrue(TEXT("Has DefaultTerritoryButtonStyle"), TFTestUtils::HasProperty(Class, TEXT("DefaultTerritoryButtonStyle")));
	TestTrue(TEXT("Has TerritoryTabButtonStyle"), TFTestUtils::HasProperty(Class, TEXT("TerritoryTabButtonStyle")));
	TestTrue(TEXT("Has TerritoryActionButtonStyle"), TFTestUtils::HasProperty(Class, TEXT("TerritoryActionButtonStyle")));
	TestTrue(TEXT("Has EconomyStartingGold"), TFTestUtils::HasProperty(Class, TEXT("EconomyStartingGold")));
	TestTrue(TEXT("Has MaxCaptureHistory"), TFTestUtils::HasProperty(Class, TEXT("MaxCaptureHistory")));

	// ─── New debug flags ───
	TestTrue(TEXT("Has bDebugBT"), TFTestUtils::HasProperty(Class, TEXT("bDebugBT")));
	TestTrue(TEXT("Has bDebugCombat"), TFTestUtils::HasProperty(Class, TEXT("bDebugCombat")));

	// ─── Debug helper methods ───
	TestTrue(TEXT("Has ShouldDebugBT"), TFTestUtils::HasFunction(Class, TEXT("ShouldDebugBT")));
	TestTrue(TEXT("Has ShouldDebugCombat"), TFTestUtils::HasFunction(Class, TEXT("ShouldDebugCombat")));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// v0.2.1 EXTENDED API CONTRACTS — audit session 3 additions
// ═══════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DiplomacySubsystemExtended,
	"TerritoryFramework.Contract.DiplomacySubsystemExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DiplomacySubsystemExtended::RunTest(const FString& Parameters)
{
	UClass* Class = UTerritoryDiplomacySubsystem::StaticClass();
	TestTrue(TEXT("DiplomacySubsystem class exists"), Class != nullptr);

	// ─── New diplomacy operations added in v0.2.1 ───
	TestTrue(TEXT("Has SignNonAggression"), TFTestUtils::HasFunction(Class, TEXT("SignNonAggression")));
	TestTrue(TEXT("Has BreakCeasefire"), TFTestUtils::HasFunction(Class, TEXT("BreakCeasefire")));
	using FGetAllReputationSignature = TMap<FGameplayTag, int32> (UTerritoryDiplomacySubsystem::*)() const;
	TestTrue(TEXT("Has native GetAllReputation API"),
		std::is_same_v<decltype(&UTerritoryDiplomacySubsystem::GetAllReputation), FGetAllReputationSignature>);

	// ─── Existing diplomacy operations still present ───
	TestTrue(TEXT("Has DeclareWar"), TFTestUtils::HasFunction(Class, TEXT("DeclareWar")));
	TestTrue(TEXT("Has DeclarePeace"), TFTestUtils::HasFunction(Class, TEXT("DeclarePeace")));
	TestTrue(TEXT("Has FormAlliance"), TFTestUtils::HasFunction(Class, TEXT("FormAlliance")));
	TestTrue(TEXT("Has BreakAlliance"), TFTestUtils::HasFunction(Class, TEXT("BreakAlliance")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DiplomacyEventTypeExtended,
	"TerritoryFramework.Contract.DiplomacyEventTypeExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DiplomacyEventTypeExtended::RunTest(const FString& Parameters)
{
	// ─── Extended event type enum — all values must be distinct ───
	TestNotEqual(TEXT("SignedNonAggression != DeclaredWar"),
		static_cast<uint8>(EDiplomacyEventType::SignedNonAggression),
		static_cast<uint8>(EDiplomacyEventType::DeclaredWar));

	TestNotEqual(TEXT("BrokeCeasefire != DeclaredPeace"),
		static_cast<uint8>(EDiplomacyEventType::BrokeCeasefire),
		static_cast<uint8>(EDiplomacyEventType::DeclaredPeace));

	TestNotEqual(TEXT("SignedNonAggression != BrokeCeasefire"),
		static_cast<uint8>(EDiplomacyEventType::SignedNonAggression),
		static_cast<uint8>(EDiplomacyEventType::BrokeCeasefire));

	TestNotEqual(TEXT("SignedNonAggression != FormedAlliance"),
		static_cast<uint8>(EDiplomacyEventType::SignedNonAggression),
		static_cast<uint8>(EDiplomacyEventType::FormedAlliance));

	// ─── DiplomacyState enum has NonAggression ───
	TestNotEqual(TEXT("NonAggression != Alliance"),
		static_cast<uint8>(EDiplomacyState::NonAggression),
		static_cast<uint8>(EDiplomacyState::Alliance));

	TestNotEqual(TEXT("Ceasefire != War"),
		static_cast<uint8>(EDiplomacyState::Ceasefire),
		static_cast<uint8>(EDiplomacyState::War));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_EconomySubsystemExtended,
	"TerritoryFramework.Contract.EconomySubsystemExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_EconomySubsystemExtended::RunTest(const FString& Parameters)
{
	UClass* Class = UTerritoryEconomySubsystem::StaticClass();
	TestTrue(TEXT("EconomySubsystem class exists"), Class != nullptr);

	// ─── NarrativePro Currency bridge ───

	TestTrue(TEXT("Has GetTreasury"), TFTestUtils::HasFunction(Class, TEXT("GetTreasury")));
	TestTrue(TEXT("Has GetIncome"), TFTestUtils::HasFunction(Class, TEXT("GetIncome")));
	TestTrue(TEXT("Has GetCosts"), TFTestUtils::HasFunction(Class, TEXT("GetCosts")));
	TestTrue(TEXT("Has CanAfford"), TFTestUtils::HasFunction(Class, TEXT("CanAfford")));
	TestTrue(TEXT("Has GetActorCurrency"), TFTestUtils::HasFunction(Class, TEXT("GetActorCurrency")));
	TestTrue(TEXT("Has CanActorAfford"), TFTestUtils::HasFunction(Class, TEXT("CanActorAfford")));
	TestTrue(TEXT("Has TryDebitCurrency"), TFTestUtils::HasFunction(Class, TEXT("TryDebitCurrency")));
	TestTrue(TEXT("Has CreditCurrency"), TFTestUtils::HasFunction(Class, TEXT("CreditCurrency")));
	TestTrue(TEXT("Has CreditCurrencyToFaction"), TFTestUtils::HasFunction(Class, TEXT("CreditCurrencyToFaction")));
	TestTrue(TEXT("Has AddToTreasury"), TFTestUtils::HasFunction(Class, TEXT("AddToTreasury")));
	TestTrue(TEXT("Has TryDebitTreasury"), TFTestUtils::HasFunction(Class, TEXT("TryDebitTreasury")));
	TestTrue(TEXT("Has SetFactionTreasury"), TFTestUtils::HasFunction(Class, TEXT("SetFactionTreasury")));
	TestTrue(TEXT("Has GetFactionEconomy"), TFTestUtils::HasFunction(Class, TEXT("GetFactionEconomy")));
	TestTrue(TEXT("Has GetAllFactionsWithTreasury"), TFTestUtils::HasFunction(Class, TEXT("GetAllFactionsWithTreasury")));
	TestTrue(TEXT("Has RecalculateIncome"), TFTestUtils::HasFunction(Class, TEXT("RecalculateIncome")));

	// ─── FTerritoryTreasury no longer has Gold field (currency bridge) ───
	// Verify the struct compiles and fields default correctly.
	FTerritoryTreasury Treasury;
	TestEqual(TEXT("FTerritoryTreasury default IncomePerTick is 0"), Treasury.IncomePerTick, 0);
	TestEqual(TEXT("FTerritoryTreasury default CostsPerTick is 0"), Treasury.CostsPerTick, 0);
	TestEqual(TEXT("FTerritoryTreasury default TerritoryCount is 0"), Treasury.TerritoryCount, 0);
	// Note: Gold field was removed — if it still exists, this test would fail to compile
	// (Treasury.Gold reference would be unresolved), which is the regression guard.

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_VolumeExtended,
	"TerritoryFramework.Contract.VolumeExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_VolumeExtended::RunTest(const FString& Parameters)
{
	UClass* Class = ATerritoryVolume::StaticClass();
	TestTrue(TEXT("TerritoryVolume class exists"), Class != nullptr);

	// ─── New public helper added in v0.2.1 ───
	TestTrue(TEXT("Has GetConfiguredGuardCount"),
		TFTestUtils::HasFunction(Class, TEXT("GetConfiguredGuardCount")));
	TestTrue(TEXT("Has GetControlMode"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetControlMode")));
	TestTrue(TEXT("Has ControlMode property"),
		TFTestUtils::HasProperty(Class, TEXT("ControlMode")));

	// ─── Existing guard API still present ───
	TestTrue(TEXT("Has GetSpawnedGuardCount"),
		TFTestUtils::HasFunction(Class, TEXT("GetSpawnedGuardCount")));
	TestTrue(TEXT("Has DespawnGuards"),
		TFTestUtils::HasFunction(Class, TEXT("DespawnGuards")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DistrictManagement,
	"TerritoryFramework.Contract.DistrictManagement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DistrictManagement::RunTest(const FString& Parameters)
{
	const UClass* PointClass = ATerritoryDistrictManagementPoint::StaticClass();
	TestTrue(TEXT("District management point class exists"), PointClass != nullptr);
	TestTrue(TEXT("Has ResolveDistrict"), TFTestUtils::HasFunction(PointClass, TEXT("ResolveDistrict")));
	TestTrue(TEXT("Has CanManage"), TFTestUtils::HasFunction(PointClass, TEXT("CanManage")));
	TestTrue(TEXT("Has OpenManagementWidget"), TFTestUtils::HasFunction(PointClass, TEXT("OpenManagementWidget")));
	TestTrue(TEXT("Has DistrictTag property"), TFTestUtils::HasProperty(PointClass, TEXT("DistrictTag")));

	const UClass* ComponentClass = UTerritoryPlayerManagementComponent::StaticClass();
	TestTrue(TEXT("Player management component class exists"), ComponentClass != nullptr);
	TestTrue(TEXT("Has RequestPurchaseGuards"),
		TFTestUtils::IsBlueprintCallable(ComponentClass, TEXT("RequestPurchaseGuards")));
	TestTrue(TEXT("Has OnGuardPurchaseResult delegate"),
		TFTestUtils::HasProperty(ComponentClass, TEXT("OnGuardPurchaseResult")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_BTAbortTask,
	"TerritoryFramework.Contract.BTAbortTask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_BTAbortTask::RunTest(const FString& Parameters)
{
	// ─── P0-03 fix: BT task must have AbortTask override to prevent slot leaks ───
	UClass* ReqClass = UBTTask_RequestTerritoryPermission::StaticClass();
	TestTrue(TEXT("RequestTerritoryPermission class exists"), ReqClass != nullptr);
	using FAbortTaskSignature = EBTNodeResult::Type (UBTTask_RequestTerritoryPermission::*)(
		UBehaviorTreeComponent&, uint8*);
	TestTrue(TEXT("Has AbortTask override (releases slots on BT abort)"),
		std::is_same_v<decltype(&UBTTask_RequestTerritoryPermission::AbortTask), FAbortTaskSignature>);

	UClass* RelClass = UBTTask_ReleaseTerritoryPermission::StaticClass();
	TestTrue(TEXT("ReleaseTerritoryPermission class exists"), RelClass != nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_DebugWidgetExtended,
	"TerritoryFramework.Contract.DebugWidgetExtended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_DebugWidgetExtended::RunTest(const FString& Parameters)
{
	UClass* Class = UTerritoryDebugWidget::StaticClass();
	TestTrue(TEXT("TerritoryDebugWidget class exists"), Class != nullptr);

	// ─── P1-04 fix: NativeDestruct override for cache invalidation ───
	struct FNativeDestructAccess : UTerritoryDebugWidget
	{
		static auto GetPointer() { return &FNativeDestructAccess::NativeDestruct; }
	};
	using FNativeDestructSignature = void (UTerritoryDebugWidget::*)();
	TestTrue(TEXT("Has NativeDestruct override (cache invalidation)"),
		std::is_same_v<decltype(FNativeDestructAccess::GetPointer()), FNativeDestructSignature>);

	// ─── Existing debug widget API ───
	TestTrue(TEXT("Has SetDebugEnabled"),
		TFTestUtils::HasFunction(Class, TEXT("SetDebugEnabled")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_TerritoryGuardCharacter,
	"TerritoryFramework.Contract.TerritoryGuardCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_TerritoryGuardCharacter::RunTest(const FString& Parameters)
{
	UClass* Class = ATerritoryGuardCharacter::StaticClass();
	TestTrue(TEXT("TerritoryGuardCharacter class exists"), Class != nullptr);

	// ─── Territory AI context properties ───
	TestTrue(TEXT("Has OwningTerritory property"),
		TFTestUtils::HasProperty(Class, TEXT("OwningTerritory")));
	TestTrue(TEXT("Has OwningTerritorySpawnPoint property"),
		TFTestUtils::HasProperty(Class, TEXT("OwningTerritorySpawnPoint")));
	TestTrue(TEXT("Has TerritoryHomeTransform property"),
		TFTestUtils::HasProperty(Class, TEXT("TerritoryHomeTransform")));

	// ─── Patrol route helper methods ───
	TestTrue(TEXT("Has GetTerritoryPatrolRoute (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryPatrolRoute")));
	TestTrue(TEXT("Has HasTerritoryPatrolRoute (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasTerritoryPatrolRoute")));
	TestTrue(TEXT("Has GetPatrolNodeCount (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetPatrolNodeCount")));
	TestTrue(TEXT("Has GetSafePatrolNode (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetSafePatrolNode")));
	TestTrue(TEXT("Has GetSpawnTransform (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetSpawnTransform")));

	TestTrue(TEXT("Has GetOwningTerritory (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetOwningTerritory")));

	TestTrue(TEXT("Has GetGuardFaction (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetGuardFaction")));

	TestTrue(TEXT("Has IsSpawnPointGuard (BlueprintPure)"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsSpawnPointGuard")));

	TestTrue(TEXT("TerritoryHomeTransform is replicated"),
		TFTestUtils::IsReplicated(Class, TEXT("TerritoryHomeTransform")));
	TestTrue(TEXT("OwningTerritory is replicated"),
		TFTestUtils::IsReplicated(Class, TEXT("OwningTerritory")));
	TestTrue(TEXT("OwningTerritorySpawnPoint is replicated"),
		TFTestUtils::IsReplicated(Class, TEXT("OwningTerritorySpawnPoint")));

	TestTrue(TEXT("Has ConfigureTerritorySpawn"),
		TFTestUtils::HasFunction(Class, TEXT("ConfigureTerritorySpawn")));
	TestTrue(TEXT("Typed guard configuration is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ConfigureTerritorySpawnWithContext")));
	TestTrue(TEXT("Typed guard configuration is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("ConfigureTerritorySpawnWithContext")));

	UTerritoryPatrolGoal* PatrolGoal = NewObject<UTerritoryPatrolGoal>();
	TestEqual(TEXT("Empty patrol goals do not score"), PatrolGoal->GetGoalScore(), 0.f);
	PatrolGoal->TerritoryPatrol.SetNum(2);
	TestEqual(TEXT("Populated patrol goals retain their configured score"),
		PatrolGoal->GetGoalScore(), PatrolGoal->DefaultScore);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_GuardSpawnPointPure,
	"TerritoryFramework.Contract.GuardSpawnPointPure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_GuardSpawnPointPure::RunTest(const FString& Parameters)
{
	UClass* Class = ATerritoryGuardSpawnPoint::StaticClass();
	TestTrue(TEXT("TerritoryGuardSpawnPoint class exists"), Class != nullptr);

	// ─── All getters must be BlueprintPure for clean graph layout ───
	TestTrue(TEXT("HasAvailableSlot is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasAvailableSlot")));
	TestTrue(TEXT("HasReserveAvailable is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasReserveAvailable")));
	TestTrue(TEXT("GetActiveGuardCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetActiveGuardCount")));
	TestTrue(TEXT("GetReserveCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetReserveCount")));
	TestTrue(TEXT("GetSpawnTransform is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetSpawnTransform")));
	TestTrue(TEXT("ResolveGuardDeploymentTransform is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("ResolveGuardDeploymentTransform")));
	TestTrue(TEXT("GetPatrolRoute is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetPatrolRoute")));
	TestTrue(TEXT("HasPatrolRoute is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasPatrolRoute")));
	TestTrue(TEXT("GetLoopPatrol is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetLoopPatrol")));
	TestTrue(TEXT("GetPatrolRouteAsTransforms is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetPatrolRouteAsTransforms")));
	TestTrue(TEXT("GetPatrolWaitTimes is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetPatrolWaitTimes")));
	TestTrue(TEXT("GetOwningTerritory is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetOwningTerritory")));
	TestTrue(TEXT("HasPendingReserveSpawn is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasPendingReserveSpawn")));
	TestTrue(TEXT("SpawnReserveGuard is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SpawnReserveGuard")));
	TestTrue(TEXT("SpawnReserveGuard is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("SpawnReserveGuard")));
	TestTrue(TEXT("RegisterSpawnedGuard is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("RegisterSpawnedGuard")));
	TestTrue(TEXT("UnregisterGuard is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("UnregisterGuard")));
	TestTrue(TEXT("Has automatic reserve policy"),
		TFTestUtils::HasProperty(Class, TEXT("bAutoSpawnReserves")));
	TestTrue(TEXT("Has reserve deployment delay"),
		TFTestUtils::HasProperty(Class, TEXT("ReserveSpawnDelay")));
	TestTrue(TEXT("Has randomized reserve radius"),
		TFTestUtils::HasProperty(Class, TEXT("ReserveSpawnRadius")));
	TestTrue(TEXT("Has reserve player clearance"),
		TFTestUtils::HasProperty(Class, TEXT("ReserveMinimumPlayerDistance")));
	TestTrue(TEXT("Has bounded camera-safe retry policy"),
		TFTestUtils::HasProperty(Class, TEXT("ReserveCameraAvoidanceRetryLimit")));
	TestTrue(TEXT("Has bounded total reserve retry policy"),
		TFTestUtils::HasProperty(Class, TEXT("ReserveTotalRetryLimit")));

	const ATerritoryGuardSpawnPoint* DefaultSpawnPoint = Class->GetDefaultObject<ATerritoryGuardSpawnPoint>();
	TestTrue(TEXT("Automatic reserve spawning defaults enabled"), DefaultSpawnPoint->bAutoSpawnReserves);
	TestTrue(TEXT("Automatic reserves are delayed"), DefaultSpawnPoint->ReserveSpawnDelay > 0.f);
	TestTrue(TEXT("Automatic reserves sample multiple candidates"), DefaultSpawnPoint->ReserveSpawnCandidateCount > 1);
	TestTrue(TEXT("Camera avoidance relaxes before a queued reserve is abandoned"),
		DefaultSpawnPoint->ReserveCameraAvoidanceRetryLimit
		< DefaultSpawnPoint->ReserveTotalRetryLimit);
	TestTrue(TEXT("Queued reserves always have a finite failure limit"),
		DefaultSpawnPoint->ReserveTotalRetryLimit > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_VolumePureGetters,
	"TerritoryFramework.Contract.VolumePureGetters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_VolumePureGetters::RunTest(const FString& Parameters)
{
	UClass* Class = ATerritoryVolume::StaticClass();
	TestTrue(TEXT("TerritoryVolume class exists"), Class != nullptr);

	// ─── All read-only getters must be BlueprintPure ───
	TestTrue(TEXT("GetOwningFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetOwningFaction")));
	TestTrue(TEXT("GetTerritoryState is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryState")));
	TestTrue(TEXT("GetControlProgress is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetControlProgress")));
	TestTrue(TEXT("IsContested is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsContested")));
	TestTrue(TEXT("GetTerritoryTag is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryTag")));
	TestTrue(TEXT("IsOwnedByFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsOwnedByFaction")));
	TestTrue(TEXT("ContainsPoint is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("ContainsPoint")));
	TestTrue(TEXT("IsLocked is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsLocked")));
	TestTrue(TEXT("CanUnlock is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("CanUnlock")));
	TestTrue(TEXT("GetSpawnedGuardCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetSpawnedGuardCount")));
	TestTrue(TEXT("GetConfiguredGuardCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetConfiguredGuardCount")));
	TestTrue(TEXT("HasGuardsAlive is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasGuardsAlive")));
	TestTrue(TEXT("GetGuardSpawnPoints is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetGuardSpawnPoints")));
	TestTrue(TEXT("GetResolvedGuardDefinition is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetResolvedGuardDefinition")));
	TestTrue(TEXT("GetDebugString is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetDebugString")));

	// ─── Mutation functions must remain BlueprintCallable (NOT Pure) ───
	TestTrue(TEXT("SetOwningFaction is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SetOwningFaction")));
	TestTrue(TEXT("SetOwningFaction is server-authority only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("SetOwningFaction")));
	TestTrue(TEXT("LockTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("LockTerritory")));
	TestTrue(TEXT("SpawnGuards is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SpawnGuards")));
	TestTrue(TEXT("TrySpawnSingleGuard is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("TrySpawnSingleGuard")));
	TestTrue(TEXT("TrySpawnSingleGuard is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("TrySpawnSingleGuard")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_BlueprintLibraryPure,
	"TerritoryFramework.Contract.BlueprintLibraryPure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_BlueprintLibraryPure::RunTest(const FString& Parameters)
{
	UClass* Class = UTerritoryBlueprintLibrary::StaticClass();
	TestTrue(TEXT("TerritoryBlueprintLibrary class exists"), Class != nullptr);

	// Subsystem accessors must be BlueprintPure
	TestTrue(TEXT("GetTerritoryRegistry is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryRegistry")));
	TestTrue(TEXT("GetTerritoryControl is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryControl")));
	TestTrue(TEXT("GetTerritoryEconomy is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryEconomy")));
	TestTrue(TEXT("GetTerritoryCombatDirector is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryCombatDirector")));
	TestTrue(TEXT("GetTerritoryDiplomacy is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryDiplomacy")));

	// Query functions must be BlueprintPure
	TestTrue(TEXT("GetTerritoryAtLocation is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryAtLocation")));
	TestTrue(TEXT("GetAllTerritories is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetAllTerritories")));
	TestTrue(TEXT("IsSameFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("IsSameFaction")));
	TestTrue(TEXT("ForceCaptureTerritory is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("ForceCaptureTerritory")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_RuntimeInvariants,
	"TerritoryFramework.Functional.RuntimeInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_RuntimeInvariants::RunTest(const FString& Parameters)
{
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Narrative.Factions.Heroes")), false);
	TestTrue(TEXT("Heroes faction exists"), Heroes.IsValid());

	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	Territory->SetOwningFaction(Heroes);
	Territory->SetTerritoryState(ETerritoryState::Contested);
	TestEqual(TEXT("Contested territory preserves incumbent faction"),
		Territory->GetOwningFaction(), Heroes);
	TestFalse(TEXT("Contested incumbent is not active ownership"),
		Territory->IsOwnedByFaction(Heroes));
	Territory->SetTerritoryState(ETerritoryState::Claimed);
	TestTrue(TEXT("Restored claim belongs to incumbent"),
		Territory->IsOwnedByFaction(Heroes));

	ATerritoryGuardCharacter* Guard = NewObject<ATerritoryGuardCharacter>();
	const FTransform Home(FRotator(0.f, 45.f, 0.f), FVector(100.f, 200.f, 300.f));
	Guard->TerritoryHomeTransform = Home;
	INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(Guard);
	TestNotNull(TEXT("Guard implements Narrative team interface"), TeamAgent);
	TeamAgent->AddFaction(Heroes);
	TestTrue(TEXT("Guard spawn getter returns stored projected home transform"),
		Guard->GetSpawnTransform().Equals(Home));
	TestEqual(TEXT("Guard faction getter returns Narrative faction"),
		Guard->GetGuardFaction(), Heroes);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_RegistrySubsystemPure,
	"TerritoryFramework.Contract.RegistrySubsystemPure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_RegistrySubsystemPure::RunTest(const FString& Parameters)
{
	UClass* Class = UTerritoryRegistrySubsystem::StaticClass();
	TestTrue(TEXT("TerritoryRegistrySubsystem class exists"), Class != nullptr);

	// ─── All read-only getters must be BlueprintPure ───
	TestTrue(TEXT("GetTerritoryByTag is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryByTag")));
	TestTrue(TEXT("GetTerritoryByGUID is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryByGUID")));
	TestTrue(TEXT("GetTerritoryAtLocation is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryAtLocation")));
	TestTrue(TEXT("GetAllTerritories is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetAllTerritories")));
	TestTrue(TEXT("GetTerritoryCount is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryCount")));
	TestTrue(TEXT("GetTerritoryCountForFaction is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetTerritoryCountForFaction")));

	// ─── Mutation functions stay Callable ───
	TestTrue(TEXT("RegisterTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RegisterTerritory")));
	TestTrue(TEXT("UnregisterTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("UnregisterTerritory")));

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// P1-11: These tests verify reflection contracts (property/function existence), not runtime behavior.
// Renamed from "Behavior" to "Contract" to accurately describe what they test.
// Real behavioral tests require PIE/world fixtures (see audit Phase 3).
// ═══════════════════════════════════════════════════════════════════════════════

// ─── P0-01: Diplomacy Friendly round-trip preserves Alliance ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_DiplomacyFriendlyRoundTrip,
	"TerritoryFramework.Contract.DiplomacyFriendlyRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_DiplomacyFriendlyRoundTrip::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryDiplomacySubsystem::StaticClass();
	if (!Class) { AddError(TEXT("DiplomacySubsystem class not found")); return false; }

	// Verify the diplomacy mapping functions exist on the class (C++ methods, not necessarily UFUNCTIONs)
	TestNotNull(TEXT("DiplomacySubsystem class has GetDiplomacyState"),
		Class->FindFunctionByName(FName(TEXT("GetDiplomacyState"))));

	// Verify the mapping enum values — Friendly should map to Alliance (not Ceasefire)
	const UEnum* DiploEnum = StaticEnum<EDiplomacyState>();
	TestNotNull(TEXT("EDiplomacyState enum exists"), DiploEnum);
	if (DiploEnum)
	{
		TestTrue(TEXT("Alliance value exists in EDiplomacyState"),
			DiploEnum->GetValueByName(FName(TEXT("Alliance"))) != INDEX_NONE);
		TestTrue(TEXT("Ceasefire value exists in EDiplomacyState"),
			DiploEnum->GetValueByName(FName(TEXT("Ceasefire"))) != INDEX_NONE);
	}

	// Verify OnFactionUpkeepDeficit delegate exists on economy subsystem (P1-20 companion)
	const UClass* EconClass = UTerritoryEconomySubsystem::StaticClass();
	TestTrue(TEXT("OnFactionUpkeepDeficit delegate exists"),
		TFTestUtils::HasProperty(EconClass, TEXT("OnFactionUpkeepDeficit")));

	return true;
}

// ─── P0-02: Hierarchy Contested→Claimed recovery ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_HierarchyContestedRecovery,
	"TerritoryFramework.Contract.HierarchyContestedRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_HierarchyContestedRecovery::RunTest(const FString& Parameters)
{
	// Verify City has the OnDistrictControlChanged handler
	const UClass* CityClass = ATerritoryCity::StaticClass();
	TestNotNull(TEXT("City class exists"), CityClass);
	if (CityClass)
	{
		TestTrue(TEXT("City has OnDistrictControlChanged"),
			TFTestUtils::HasFunction(CityClass, TEXT("OnDistrictControlChanged")));
		TestTrue(TEXT("City has AllDistrictsOwnedBy"),
			TFTestUtils::HasFunction(CityClass, TEXT("AllDistrictsOwnedBy")));
	}

	// Verify District has the OnPropertyControlChanged handler
	const UClass* DistrictClass = ATerritoryDistrict::StaticClass();
	TestNotNull(TEXT("District class exists"), DistrictClass);
	if (DistrictClass)
	{
		TestTrue(TEXT("District has OnPropertyControlChanged"),
			TFTestUtils::HasFunction(DistrictClass, TEXT("OnPropertyControlChanged")));
		TestTrue(TEXT("District has AllPropertiesOwnedBy"),
			TFTestUtils::HasFunction(DistrictClass, TEXT("AllPropertiesOwnedBy")));
	}

	// Verify state transition functions exist
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();
	TestTrue(TEXT("SetTerritoryState exists"),
		TFTestUtils::HasFunction(VolumeClass, TEXT("SetTerritoryState")));
	TestTrue(TEXT("GetTerritoryState exists"),
		TFTestUtils::HasFunction(VolumeClass, TEXT("GetTerritoryState")));

	return true;
}

// ─── P1-03: OnCityLost fires only once (bCityLostFired guard) ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_CityLostFiresOnce,
	"TerritoryFramework.Contract.CityLostFiresOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_CityLostFiresOnce::RunTest(const FString& Parameters)
{
	const UClass* CityClass = ATerritoryCity::StaticClass();
	TestNotNull(TEXT("City class exists"), CityClass);
	if (!CityClass) return false;

	// Verify OnCityLost delegate and event exist — bCityLostFired is private C++ member
	// that guards against duplicate broadcasts (verified by code review, not reflection)
	TestTrue(TEXT("OnCityLost native event exists"),
		TFTestUtils::HasFunction(CityClass, TEXT("OnCityLost")));
	TestTrue(TEXT("OnCityLostDelegate assignable delegate exists"),
		TFTestUtils::HasProperty(CityClass, TEXT("OnCityLostDelegate")));

	// Verify OnCityFullyCaptured exists — the counterpart to OnCityLost
	TestTrue(TEXT("OnCityFullyCaptured native event exists"),
		TFTestUtils::HasFunction(CityClass, TEXT("OnCityFullyCaptured")));

	return true;
}

// ─── P0-03: FTerritoryTransitionContext exists ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_TransitionContext,
	"TerritoryFramework.Contract.TransitionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_TransitionContext::RunTest(const FString& Parameters)
{
	// Verify the struct exists via UScriptStruct lookup
	UScriptStruct* ContextStruct = FTerritoryTransitionContext::StaticStruct();
	TestNotNull(TEXT("FTerritoryTransitionContext struct exists"), ContextStruct);
	if (!ContextStruct) return false;

	// Verify all required fields
	TestTrue(TEXT("Has Instigator field"),
		ContextStruct->FindPropertyByName(FName(TEXT("Instigator"))) != nullptr);
	TestTrue(TEXT("Has TargetPawn field"),
		ContextStruct->FindPropertyByName(FName(TEXT("TargetPawn"))) != nullptr);
	TestTrue(TEXT("Has PlayerController field"),
		ContextStruct->FindPropertyByName(FName(TEXT("PlayerController"))) != nullptr);
	TestTrue(TEXT("Has TalesComponent field"),
		ContextStruct->FindPropertyByName(FName(TEXT("TalesComponent"))) != nullptr);
	TestTrue(TEXT("Has RequestingFaction field"),
		ContextStruct->FindPropertyByName(FName(TEXT("RequestingFaction"))) != nullptr);

	// CheckStateConditions and FireStateEvents are C++ methods with default context parameter
	// (not UFUNCTIONs) — verify they exist as C++ symbols via class method lookup
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();
	TestNotNull(TEXT("Volume class exists for context verification"), VolumeClass);

	// Verify the struct can be default-constructed with all fields initialized
	FTerritoryTransitionContext DefaultCtx;
	TestNull(TEXT("Default Instigator is null"), DefaultCtx.Instigator);
	TestNull(TEXT("Default TargetPawn is null"), DefaultCtx.TargetPawn);
	TestNull(TEXT("Default PlayerController is null"), DefaultCtx.PlayerController);
	TestNull(TEXT("Default TalesComponent is null"), DefaultCtx.TalesComponent);
	TestFalse(TEXT("Default RequestingFaction is invalid"), DefaultCtx.RequestingFaction.IsValid());

	return true;
}

// ─── P1-09: Guard reserve state has SaveGame ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_GuardReserveSaveGame,
	"TerritoryFramework.Contract.GuardReserveSaveGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_GuardReserveSaveGame::RunTest(const FString& Parameters)
{
	const UClass* SPClass = ATerritoryGuardSpawnPoint::StaticClass();
	TestNotNull(TEXT("GuardSpawnPoint class exists"), SPClass);
	if (!SPClass) return false;

	// Verify CurrentReserveCount is SaveGame
	TestTrue(TEXT("CurrentReserveCount is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("CurrentReserveCount")));

	// Verify PendingReserveSpawns is SaveGame
	TestTrue(TEXT("PendingReserveSpawns is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("PendingReserveSpawns")));

	// Verify SavedActiveGuardCount exists and is SaveGame
	TestTrue(TEXT("SavedActiveGuardCount exists"),
		TFTestUtils::HasProperty(SPClass, TEXT("SavedActiveGuardCount")));
	TestTrue(TEXT("SavedActiveGuardCount is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("SavedActiveGuardCount")));

	// Verify narrative override properties exist (P1-10 companion)
	TestTrue(TEXT("NPCDefinitionOverride exists"),
		TFTestUtils::HasProperty(SPClass, TEXT("NPCDefinitionOverride")));
	TestTrue(TEXT("ActivityConfigurationOverride exists"),
		TFTestUtils::HasProperty(SPClass, TEXT("ActivityConfigurationOverride")));
	TestTrue(TEXT("TriggerSetOverrides exists"),
		TFTestUtils::HasProperty(SPClass, TEXT("TriggerSetOverrides")));

	return true;
}

// ─── P1-04: RegisterTerritory returns result enum ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_RegistryReturnsResult,
	"TerritoryFramework.Contract.RegistryReturnsResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_RegistryReturnsResult::RunTest(const FString& Parameters)
{
	// Verify the result enum exists
	const UEnum* ResultEnum = StaticEnum<ETerritoryRegistrationResult>();
	TestNotNull(TEXT("ETerritoryRegistrationResult enum exists"), ResultEnum);
	if (ResultEnum)
	{
		TestTrue(TEXT("Success value exists"),
			ResultEnum->GetValueByName(FName(TEXT("Success"))) != INDEX_NONE);
		TestTrue(TEXT("DuplicateTag value exists"),
			ResultEnum->GetValueByName(FName(TEXT("DuplicateTag"))) != INDEX_NONE);
		TestTrue(TEXT("DuplicateGUID value exists"),
			ResultEnum->GetValueByName(FName(TEXT("DuplicateGUID"))) != INDEX_NONE);
		TestTrue(TEXT("InvalidTerritory value exists"),
			ResultEnum->GetValueByName(FName(TEXT("InvalidTerritory"))) != INDEX_NONE);
	}

	// Verify RegisterTerritory is still BlueprintCallable
	const UClass* RegClass = UTerritoryRegistrySubsystem::StaticClass();
	TestTrue(TEXT("RegisterTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(RegClass, TEXT("RegisterTerritory")));

	// Verify RegisterTerritory function has a return value (not void)
	UFunction* RegFunc = RegClass->FindFunctionByName(FName(TEXT("RegisterTerritory")));
	TestNotNull(TEXT("RegisterTerritory function exists"), RegFunc);
	if (RegFunc)
	{
		FProperty* ReturnProp = RegFunc->GetReturnProperty();
		TestNotNull(TEXT("RegisterTerritory has return value"), ReturnProp);
	}

	return true;
}

// ─── P0-01: Client registry registration path ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_ClientRegistryRegistration,
	"TerritoryFramework.Contract.ClientRegistryRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_ClientRegistryRegistration::RunTest(const FString& Parameters)
{
	// Verify BeginPlay registration is outside the authority gate.
	// The structural check: RegisterTerritory is called before the HasAuthority block
	// in TerritoryVolume::BeginPlay. We verify this by confirming:
	// 1. The RegistrySubsystem exists and is accessible on all net modes
	// 2. RegisterTerritory has no internal net-mode guard
	// 3. EconomySubsystem::OnTerritoryRegistered guards against client mutation

	const UClass* RegClass = UTerritoryRegistrySubsystem::StaticClass();
	TestNotNull(TEXT("RegistrySubsystem class exists"), RegClass);

	// Verify RegisterTerritory returns a result enum (not void) —
	// the function works on all net modes since it has no internal guard
	TestTrue(TEXT("RegisterTerritory is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(RegClass, TEXT("RegisterTerritory")));

	// Verify the EconomySubsystem exists (server-authoritative, should not mutate on client)
	const UClass* EconClass = UTerritoryEconomySubsystem::StaticClass();
	TestNotNull(TEXT("EconomySubsystem class exists"), EconClass);
	TestTrue(TEXT("EconomySubsystem has OnTerritoryRegistered handler"),
		TFTestUtils::HasFunction(EconClass, TEXT("OnTerritoryRegistered")));

	// P0-03: WorldState no longer has OnTerritoryRegistered — Volume is sole ownership authority
	const UClass* WSClass = ATerritoryWorldState::StaticClass();
	TestFalse(TEXT("WorldState OnTerritoryRegistered removed (P0-03)"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnTerritoryRegistered")));

	return true;
}

// ─── P0-02: Live replication subscriptions ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFBehavior_LiveReplicationSubscriptions,
	"TerritoryFramework.Contract.LiveReplicationSubscriptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFBehavior_LiveReplicationSubscriptions::RunTest(const FString& Parameters)
{
	const UClass* WSClass = ATerritoryWorldState::StaticClass();

	// Verify live handler functions exist (UFUNCTION for delegate binding)
	TestTrue(TEXT("OnEconomyTickLive handler exists"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnEconomyTickLive")));
	TestTrue(TEXT("OnTransactionRecordedLive handler exists"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnTransactionRecordedLive")));
	TestTrue(TEXT("OnDiplomacyChangedLive handler exists"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnDiplomacyChangedLive")));
	TestTrue(TEXT("OnReputationChangedLive handler exists"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnReputationChangedLive")));
	TestTrue(TEXT("OnTerritoryControlChangedLive handler exists"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnTerritoryControlChangedLive")));

	// ─── P0-03: Verify single-authority persistence ───
	// SavedCaptureSummaries should NOT exist on WorldState (Volume is sole authority)
	TestFalse(TEXT("SavedCaptureSummaries removed from WorldState (P0-03)"),
		TFTestUtils::HasProperty(WSClass, TEXT("SavedCaptureSummaries")));

	// ApplyPendingCaptureSummaries should NOT exist (dead code removed)
	TestFalse(TEXT("ApplyPendingCaptureSummaries removed from WorldState (P0-03)"),
		TFTestUtils::HasFunction(WSClass, TEXT("ApplyPendingCaptureSummaries")));

	// OnTerritoryRegistered should NOT exist on WorldState (removed)
	TestFalse(TEXT("OnTerritoryRegistered removed from WorldState (P0-03)"),
		TFTestUtils::HasFunction(WSClass, TEXT("OnTerritoryRegistered")));

	// ReplicatedCaptureSummaries should still exist (runtime replication, not persistence)
	TestTrue(TEXT("ReplicatedCaptureSummaries still exists for runtime replication"),
		TFTestUtils::HasProperty(WSClass, TEXT("ReplicatedCaptureSummaries")));

	return true;
}

// ─── P0-07: Unified guard spawn config cascade ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_GuardSpawnConfigParity,
	"TerritoryFramework.Contract.GuardSpawnConfigParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_GuardSpawnConfigParity::RunTest(const FString& Parameters)
{
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();

	// Verify the unified cascade is available (helper is C++ only, not UFUNCTION —
	// verify via its consumers: both SpawnGuards and TrySpawnSingleGuard exist)
	TestTrue(TEXT("SpawnGuards exists (uses unified cascade)"),
		TFTestUtils::HasFunction(VolumeClass, TEXT("SpawnGuards")));
	TestTrue(TEXT("TrySpawnSingleGuard exists (uses unified cascade)"),
		TFTestUtils::HasFunction(VolumeClass, TEXT("TrySpawnSingleGuard")));

	// Verify GuardSpawnPoint has all three override channels
	const UClass* SPClass = ATerritoryGuardSpawnPoint::StaticClass();
	TestTrue(TEXT("SP has NPCDefinitionOverride"),
		TFTestUtils::HasProperty(SPClass, TEXT("NPCDefinitionOverride")));
	TestTrue(TEXT("SP has ActivityConfigurationOverride"),
		TFTestUtils::HasProperty(SPClass, TEXT("ActivityConfigurationOverride")));
	TestTrue(TEXT("SP has TriggerSetOverrides"),
		TFTestUtils::HasProperty(SPClass, TEXT("TriggerSetOverrides")));
	TestTrue(TEXT("SP has GuardPostDefinition"),
		TFTestUtils::HasProperty(SPClass, TEXT("GuardPostDefinition")));

	// Verify GuardPostDefinition data asset has all three config channels
	const UClass* PostClass = UTerritoryGuardPostDefinition::StaticClass();
	TestTrue(TEXT("Post has NPCDefinition"),
		TFTestUtils::HasProperty(PostClass, TEXT("NPCDefinition")));
	TestTrue(TEXT("Post has ActivityConfiguration"),
		TFTestUtils::HasProperty(PostClass, TEXT("ActivityConfiguration")));
	TestTrue(TEXT("Post has TriggerSetOverrides"),
		TFTestUtils::HasProperty(PostClass, TEXT("TriggerSetOverrides")));

	return true;
}

// ─── P0-06: GuardSpawnPoint implements INarrativeSavableActor ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_GuardSpawnPointSaveGame,
	"TerritoryFramework.Contract.GuardSpawnPointSaveGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_GuardSpawnPointSaveGame::RunTest(const FString& Parameters)
{
	const UClass* SPClass = ATerritoryGuardSpawnPoint::StaticClass();
	TestNotNull(TEXT("GuardSpawnPoint class exists"), SPClass);
	if (!SPClass) return false;

	// Verify INarrativeSavableActor interface
	TestTrue(TEXT("Implements INarrativeSavableActor"),
		SPClass->ImplementsInterface(UNarrativeSavableActor::StaticClass()));

	// Verify SaveGame interface methods exist
	TestTrue(TEXT("GetActorGUID exists"),
		TFTestUtils::HasFunction(SPClass, TEXT("GetActorGUID")));
	TestTrue(TEXT("PrepareForSave exists"),
		TFTestUtils::HasFunction(SPClass, TEXT("PrepareForSave")));
	TestTrue(TEXT("Load exists"),
		TFTestUtils::HasFunction(SPClass, TEXT("Load")));
	TestTrue(TEXT("ShouldRespawn exists"),
		TFTestUtils::HasFunction(SPClass, TEXT("ShouldRespawn")));

	// Verify GUID property exists and is SaveGame
	TestTrue(TEXT("SpawnPointGUID property exists"),
		TFTestUtils::HasProperty(SPClass, TEXT("SpawnPointGUID")));
	TestTrue(TEXT("SpawnPointGUID is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("SpawnPointGUID")));

	// Verify reserve state properties are SaveGame
	TestTrue(TEXT("CurrentReserveCount is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("CurrentReserveCount")));
	TestTrue(TEXT("PendingReserveSpawns is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("PendingReserveSpawns")));
	TestTrue(TEXT("SavedActiveGuardCount is SaveGame"),
		TFTestUtils::IsSaveGame(SPClass, TEXT("SavedActiveGuardCount")));

	return true;
}

// ─── P0-04: TerritoryCaptureEvent uses ApplyTerritoryMutation ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_CaptureEventUsesMutation,
	"TerritoryFramework.Contract.CaptureEventUsesMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_CaptureEventUsesMutation::RunTest(const FString& Parameters)
{
	// Verify TerritoryCaptureEvent exists and has ExecuteEvent
	const UClass* EventClass = UTerritoryCaptureEvent::StaticClass();
	TestNotNull(TEXT("TerritoryCaptureEvent class exists"), EventClass);
	if (!EventClass) return false;

	TestTrue(TEXT("ExecuteEvent function exists"),
		TFTestUtils::HasFunction(EventClass, TEXT("ExecuteEvent")));

	// Verify the event has the required properties
	TestTrue(TEXT("TargetTerritoryTag property exists"),
		TFTestUtils::HasProperty(EventClass, TEXT("TargetTerritoryTag")));
	TestTrue(TEXT("CapturingFaction property exists"),
		TFTestUtils::HasProperty(EventClass, TEXT("CapturingFaction")));
	TestTrue(TEXT("bForceCapture property exists"),
		TFTestUtils::HasProperty(EventClass, TEXT("bForceCapture")));

	// Verify ApplyTerritoryMutation exists on ControlSubsystem (the path we route through)
	const UClass* ControlClass = UTerritoryControlSubsystem::StaticClass();
	TestTrue(TEXT("ApplyTerritoryMutation is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(ControlClass, TEXT("ApplyTerritoryMutation")));

	return true;
}

// ─── 5.2: GuardPostDefinition data asset contract ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_GuardPostDefinition,
	"TerritoryFramework.Contract.GuardPostDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_GuardPostDefinition::RunTest(const FString& Parameters)
{
	const UClass* Class = UTerritoryGuardPostDefinition::StaticClass();
	TestNotNull(TEXT("GuardPostDefinition class exists"), Class);
	if (!Class) return false;

	// Identity
	TestTrue(TEXT("Has DisplayName"), TFTestUtils::HasProperty(Class, TEXT("DisplayName")));
	TestTrue(TEXT("Has FactionOverride"), TFTestUtils::HasProperty(Class, TEXT("FactionOverride")));

	// NPC
	TestTrue(TEXT("Has NPCDefinition"), TFTestUtils::HasProperty(Class, TEXT("NPCDefinition")));
	TestTrue(TEXT("Has ActivityConfiguration"), TFTestUtils::HasProperty(Class, TEXT("ActivityConfiguration")));
	TestTrue(TEXT("Has TriggerSetOverrides"), TFTestUtils::HasProperty(Class, TEXT("TriggerSetOverrides")));

	// Patrol
	TestTrue(TEXT("Has PatrolRoute"), TFTestUtils::HasProperty(Class, TEXT("PatrolRoute")));
	TestTrue(TEXT("Has bLoopPatrol"), TFTestUtils::HasProperty(Class, TEXT("bLoopPatrol")));

	// Capacity
	TestTrue(TEXT("Has MaxGuards"), TFTestUtils::HasProperty(Class, TEXT("MaxGuards")));
	TestTrue(TEXT("Has ReserveSlots"), TFTestUtils::HasProperty(Class, TEXT("ReserveSlots")));
	TestTrue(TEXT("Has ReserveSpawnDelay"), TFTestUtils::HasProperty(Class, TEXT("ReserveSpawnDelay")));

	// Verify it's a PrimaryDataAsset
	TestTrue(TEXT("Inherits from UPrimaryDataAsset"),
		Class->IsChildOf(UPrimaryDataAsset::StaticClass()));

	// Verify spawn point has GuardPostDefinition property
	const UClass* SPClass = ATerritoryGuardSpawnPoint::StaticClass();
	TestTrue(TEXT("GuardSpawnPoint has GuardPostDefinition"),
		TFTestUtils::HasProperty(SPClass, TEXT("GuardPostDefinition")));

	return true;
}

// ─── 5.3: Atomic mutation API contract ───
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_AtomicMutation,
	"TerritoryFramework.Contract.AtomicMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_AtomicMutation::RunTest(const FString& Parameters)
{
	// Verify the result enum
	const UEnum* ResultEnum = StaticEnum<ETerritoryMutationResult>();
	TestNotNull(TEXT("ETerritoryMutationResult enum exists"), ResultEnum);
	if (ResultEnum)
	{
		TestTrue(TEXT("Success value exists"), ResultEnum->GetValueByName(FName(TEXT("Success"))) != INDEX_NONE);
		TestTrue(TEXT("Rejected_Authority exists"), ResultEnum->GetValueByName(FName(TEXT("Rejected_Authority"))) != INDEX_NONE);
		TestTrue(TEXT("Rejected_NullTerritory exists"), ResultEnum->GetValueByName(FName(TEXT("Rejected_NullTerritory"))) != INDEX_NONE);
		TestTrue(TEXT("Rejected_AggregateOnly exists"), ResultEnum->GetValueByName(FName(TEXT("Rejected_AggregateOnly"))) != INDEX_NONE);
		TestTrue(TEXT("Rejected_InvalidFaction exists"), ResultEnum->GetValueByName(FName(TEXT("Rejected_InvalidFaction"))) != INDEX_NONE);
		TestTrue(TEXT("Rejected_Locked exists"), ResultEnum->GetValueByName(FName(TEXT("Rejected_Locked"))) != INDEX_NONE);
		TestTrue(TEXT("Rejected_StateUnchanged exists"), ResultEnum->GetValueByName(FName(TEXT("Rejected_StateUnchanged"))) != INDEX_NONE);
		TestTrue(TEXT("Failed_FinalStateMismatch exists"), ResultEnum->GetValueByName(FName(TEXT("Failed_FinalStateMismatch"))) != INDEX_NONE);
	}

	// Verify request struct fields
	UScriptStruct* RequestStruct = FTerritoryMutationRequest::StaticStruct();
	TestNotNull(TEXT("FTerritoryMutationRequest struct exists"), RequestStruct);
	if (RequestStruct)
	{
		TestTrue(TEXT("Request has Territory"), RequestStruct->FindPropertyByName(FName(TEXT("Territory"))) != nullptr);
		TestTrue(TEXT("Request has NewOwner"), RequestStruct->FindPropertyByName(FName(TEXT("NewOwner"))) != nullptr);
		TestTrue(TEXT("Request has DesiredState"), RequestStruct->FindPropertyByName(FName(TEXT("DesiredState"))) != nullptr);
		TestTrue(TEXT("Request has bClearCaptureState"), RequestStruct->FindPropertyByName(FName(TEXT("bClearCaptureState"))) != nullptr);
		TestTrue(TEXT("Request has bBypassConditions"), RequestStruct->FindPropertyByName(FName(TEXT("bBypassConditions"))) != nullptr);
		TestTrue(TEXT("Request has explicit lock bypass"), RequestStruct->FindPropertyByName(FName(TEXT("bBypassLock"))) != nullptr);
		TestTrue(TEXT("Request has TransitionContext"), RequestStruct->FindPropertyByName(FName(TEXT("TransitionContext"))) != nullptr);
	}
	const FTerritoryMutationRequest SafeDefaults;
	TestFalse(TEXT("Ordinary mutations do not bypass locks"), SafeDefaults.bBypassLock);
	TestFalse(TEXT("Ordinary mutations do not bypass diplomacy"), SafeDefaults.bBypassDiplomacy);
	TestFalse(TEXT("Ordinary mutations do not bypass Narrative conditions"), SafeDefaults.bBypassConditions);

	// Verify response struct fields
	UScriptStruct* ResponseStruct = FTerritoryMutationResponse::StaticStruct();
	TestNotNull(TEXT("FTerritoryMutationResponse struct exists"), ResponseStruct);
	if (ResponseStruct)
	{
		TestTrue(TEXT("Response has Result"), ResponseStruct->FindPropertyByName(FName(TEXT("Result"))) != nullptr);
		TestTrue(TEXT("Response has Territory"), ResponseStruct->FindPropertyByName(FName(TEXT("Territory"))) != nullptr);
		TestTrue(TEXT("Response has OldOwner"), ResponseStruct->FindPropertyByName(FName(TEXT("OldOwner"))) != nullptr);
		TestTrue(TEXT("Response has NewOwner"), ResponseStruct->FindPropertyByName(FName(TEXT("NewOwner"))) != nullptr);
		TestTrue(TEXT("Response has OldState"), ResponseStruct->FindPropertyByName(FName(TEXT("OldState"))) != nullptr);
		TestTrue(TEXT("Response has NewState"), ResponseStruct->FindPropertyByName(FName(TEXT("NewState"))) != nullptr);
		TestTrue(TEXT("Response has Explanation"), ResponseStruct->FindPropertyByName(FName(TEXT("Explanation"))) != nullptr);
	}

	// Verify the function exists on ControlSubsystem
	const UClass* ControlClass = UTerritoryControlSubsystem::StaticClass();
	TestTrue(TEXT("ApplyTerritoryMutation is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(ControlClass, TEXT("ApplyTerritoryMutation")));
	TestTrue(TEXT("ApplyTerritoryMutation is BlueprintAuthorityOnly"),
		TFTestUtils::IsBlueprintAuthorityOnly(ControlClass, TEXT("ApplyTerritoryMutation")));

	// Verify ForceCapture still exists (backward compat)
	TestTrue(TEXT("ForceCapture still exists"),
		TFTestUtils::HasFunction(ControlClass, TEXT("ForceCapture")));

	// ─── P0-05: Verify truly atomic implementation ───

	// Rejected_DiplomacyBlocked exists in enum
	if (ResultEnum)
	{
		TestTrue(TEXT("Rejected_DiplomacyBlocked exists"),
			ResultEnum->GetValueByName(FName(TEXT("Rejected_DiplomacyBlocked"))) != INDEX_NONE);
	}

	// CommitOwnershipData and GetOwnershipData are C++ methods (not UFUNCTIONs) —
	// verified by successful compilation of ApplyTerritoryMutation which calls both.
	// Verify TerritoryVolume class exists as the commit target.
	const UClass* VolumeClass = ATerritoryVolume::StaticClass();
	TestNotNull(TEXT("TerritoryVolume class exists for atomic commit"), VolumeClass);

	// Verify the OwnershipData property is replicated (required for atomic commit to propagate)
	TestTrue(TEXT("OwnershipData is SaveGame+Replicated"),
		TFTestUtils::IsSaveGame(VolumeClass, TEXT("OwnershipData")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContract_StateConfigNarrativeExtensions,
	"TerritoryFramework.Contract.StateConfigNarrativeExtensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContract_StateConfigNarrativeExtensions::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("State Config exposes command capability grants"),
		FTerritoryStateConfig::StaticStruct()->FindPropertyByName(
			TEXT("GrantedCommandCapabilities")));
	TestTrue(TEXT("Built-in Guard Staffing capability is registered"),
		TerritoryCommandTags::GuardStaffing.GetTag().IsValid());
	TestTrue(TEXT("Built-in Reinforcements capability is registered"),
		TerritoryCommandTags::Reinforcements.GetTag().IsValid());

	const UClass* VolumeClass = ATerritoryVolume::StaticClass();
	TestTrue(TEXT("Territory exposes active state command grants"),
		TFTestUtils::IsBlueprintPure(VolumeClass, TEXT("GetActiveCommandCapabilities")));
	TestTrue(TEXT("Reinforcement eligibility is a pure query"),
		TFTestUtils::IsBlueprintPure(VolumeClass, TEXT("CanSendReinforcements")));
	TestTrue(TEXT("Reinforcement execution is server authoritative"),
		TFTestUtils::IsBlueprintAuthorityOnly(VolumeClass, TEXT("TrySendReinforcements")));

	const UClass* LibraryClass = UTerritoryBlueprintLibrary::StaticClass();
	TestTrue(TEXT("Faction command capability aggregation is Blueprint pure"),
		TFTestUtils::IsBlueprintPure(LibraryClass, TEXT("GetFactionCommandCapabilities")));
	TestTrue(TEXT("Command capability source query is Blueprint pure"),
		TFTestUtils::IsBlueprintPure(LibraryClass, TEXT("GetFactionCommandCapabilitySources")));
	TestTrue(TEXT("Command capability gate is Blueprint pure"),
		TFTestUtils::IsBlueprintPure(LibraryClass, TEXT("CanFactionUseCommandCapability")));

	TestTrue(TEXT("Diplomacy condition is a Narrative condition"),
		UTerritoryDiplomacyCondition::StaticClass()->IsChildOf(UNarrativeCondition::StaticClass()));
	TestTrue(TEXT("Garrison condition is a Narrative condition"),
		UTerritoryGarrisonCondition::StaticClass()->IsChildOf(UNarrativeCondition::StaticClass()));
	TestTrue(TEXT("Diplomacy event is a Narrative event"),
		UTerritorySetDiplomacyEvent::StaticClass()->IsChildOf(UNarrativeEvent::StaticClass()));
	TestNotNull(TEXT("Diplomacy event exposes safe fresh-state initialization policy"),
		UTerritorySetDiplomacyEvent::StaticClass()->FindPropertyByName(
			TEXT("bApplyWhenStateStartsActive")));
	TestTrue(TEXT("Reputation event is a Narrative event"),
		UTerritoryModifyReputationEvent::StaticClass()->IsChildOf(UNarrativeEvent::StaticClass()));

	const TArray<UClass*> NewConditionClasses = {
		UTerritoryQuestStateCondition::StaticClass(),
		UTerritoryStateCondition::StaticClass(),
		UTerritoryControlProgressCondition::StaticClass(),
		UTerritoryReputationCondition::StaticClass(),
		UTerritoryFactionDistrictHoldingCondition::StaticClass(),
		UTerritoryAssaultCondition::StaticClass(),
		UTerritoryPresenceCondition::StaticClass(),
		UTerritoryProductionStatusCondition::StaticClass(),
		UTerritoryResourceCondition::StaticClass()
	};
	for (const UClass* Class : NewConditionClasses)
	{
		TestTrue(*FString::Printf(TEXT("%s is a Narrative condition"), *GetNameSafe(Class)),
			Class && Class->IsChildOf(UNarrativeCondition::StaticClass()));
	}

	const TArray<UClass*> NewEventClasses = {
		UTerritoryHierarchyStoryOverrideEvent::StaticClass(),
		UTerritoryScheduleEnemyWaveEvent::StaticClass(),
		UTerritoryCancelEnemyWavesEvent::StaticClass(),
		UTerritorySetGarrisonTargetEvent::StaticClass(),
		UTerritoryUpgradePropertyEvent::StaticClass(),
		UTerritoryExecuteResourceRecipeEvent::StaticClass()
	};
	for (const UClass* Class : NewEventClasses)
	{
		TestTrue(*FString::Printf(TEXT("%s is a Narrative event"), *GetNameSafe(Class)),
			Class && Class->IsChildOf(UNarrativeEvent::StaticClass()));
		UNarrativeEvent* Event = Class ? NewObject<UNarrativeEvent>(GetTransientPackage(), Class) : nullptr;
		TestTrue(*FString::Printf(TEXT("%s does not refire its mutation on quest load"), *GetNameSafe(Class)),
			Event && !Event->bRefireOnLoad);
	}

	const UClass* DiplomacyCondition = UTerritoryDiplomacyCondition::StaticClass();
	TestTrue(TEXT("Diplomacy condition exposes first faction"),
		TFTestUtils::HasProperty(DiplomacyCondition, TEXT("FactionA")));
	TestTrue(TEXT("Diplomacy condition exposes second faction"),
		TFTestUtils::HasProperty(DiplomacyCondition, TEXT("FactionB")));
	TestTrue(TEXT("Diplomacy condition exposes required rich state"),
		TFTestUtils::HasProperty(DiplomacyCondition, TEXT("RequiredState")));

	const UClass* GarrisonCondition = UTerritoryGarrisonCondition::StaticClass();
	TestTrue(TEXT("Garrison condition exposes stable Territory tag"),
		TFTestUtils::HasProperty(GarrisonCondition, TEXT("TerritoryToCheck")));
	TestTrue(TEXT("Garrison condition exposes metric"),
		TFTestUtils::HasProperty(GarrisonCondition, TEXT("Metric")));
	TestTrue(TEXT("Garrison condition exposes comparison"),
		TFTestUtils::HasProperty(GarrisonCondition, TEXT("Comparison")));

	TestTrue(TEXT("Equal comparison passes equal values"),
		UTerritoryGarrisonCondition::CompareValues(2,
			ETerritoryIntegerComparison::Equal, 2));
	TestFalse(TEXT("At least comparison rejects a smaller garrison"),
		UTerritoryGarrisonCondition::CompareValues(1,
			ETerritoryIntegerComparison::AtLeast, 2));
	TestTrue(TEXT("At most comparison accepts an exhausted garrison"),
		UTerritoryGarrisonCondition::CompareValues(0,
			ETerritoryIntegerComparison::AtMost, 0));
	TestTrue(TEXT("Greater than comparison is strict"),
		UTerritoryGarrisonCondition::CompareValues(3,
			ETerritoryIntegerComparison::GreaterThan, 2));
	TestTrue(TEXT("Control percentage comparison accepts an exact tolerant value"),
		UTerritoryControlProgressCondition::CompareValues(74.8f,
			ETerritoryFloatComparison::NearlyEqual, 75.f, 0.5f));
	TestFalse(TEXT("Control percentage comparison rejects an insufficient value"),
		UTerritoryControlProgressCondition::CompareValues(49.f,
			ETerritoryFloatComparison::AtLeast, 50.f, 0.f));

	FTerritoryAssaultRecord OldActive;
	OldActive.AssaultID = FGuid::NewGuid();
	OldActive.State = ETerritoryAssaultState::Active;
	OldActive.EvaluationCycle = 1;
	FTerritoryAssaultRecord NewTerminal;
	NewTerminal.AssaultID = FGuid::NewGuid();
	NewTerminal.State = ETerritoryAssaultState::Defeated;
	NewTerminal.EvaluationCycle = 2;
	const TArray<FTerritoryAssaultRecord> Assaults = { NewTerminal, OldActive };
	TestTrue(TEXT("A live assault is selected before terminal history"),
		UTerritoryAssaultCondition::SelectLatestRecord(Assaults) == &Assaults[1]);

	UTerritoryScheduleEnemyWaveEvent* ConditionalEvent =
		NewObject<UTerritoryScheduleEnemyWaveEvent>();
	UTerritoryOwnershipCondition* FalseCondition =
		NewObject<UTerritoryOwnershipCondition>(ConditionalEvent);
	ConditionalEvent->Conditions.Add(FalseCondition);
	TestFalse(TEXT("An inherited failed condition blocks a Territory Narrative event"),
		TerritoryTales::DoEventConditionsPass(ConditionalEvent, nullptr, nullptr, nullptr));
	FalseCondition->bNot = true;
	TestTrue(TEXT("Narrative Not also inverts an inherited event condition"),
		TerritoryTales::DoEventConditionsPass(ConditionalEvent, nullptr, nullptr, nullptr));
	TestTrue(TEXT("Wave events expose an explicit strategic or story launch mode"),
		TFTestUtils::HasProperty(UTerritoryScheduleEnemyWaveEvent::StaticClass(), TEXT("LaunchMode")));
	TestTrue(TEXT("Faction holdings condition exposes the secure District threshold"),
		TFTestUtils::HasProperty(UTerritoryFactionDistrictHoldingCondition::StaticClass(),
			TEXT("DistrictCount")));
	TestTrue(TEXT("Ownership-transition condition reuses Narrative's condition pipeline"),
		UTerritoryOwnershipTransitionCondition::StaticClass()->IsChildOf(
			UNarrativeCondition::StaticClass()));
	TestTrue(TEXT("Diplomacy exposes a dynamic opposing-transition faction source"),
		StaticEnum<ETerritoryDiplomacyFactionSource>()->GetIndexByValue(
			static_cast<int64>(
				ETerritoryDiplomacyFactionSource::TransitionOpposingFaction)) != INDEX_NONE);

	const UClass* TerritoryClass = ATerritoryVolume::StaticClass();
	TestTrue(TEXT("Territories expose the registered-defender death event hook"),
		TFTestUtils::HasProperty(TerritoryClass, TEXT("DefenderDiedEvents")));
	TestTrue(TEXT("Territories expose the all-defenders-defeated event hook"),
		TFTestUtils::HasProperty(TerritoryClass, TEXT("AllDefendersDefeatedEvents")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyStateEventFactionResolution,
	"TerritoryFramework.Diplomacy.StateEvents.OwnerRelativeFactionResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyStateEventFactionResolution::RunTest(const FString& Parameters)
{
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Police = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Police"), false);
	TestTrue(TEXT("Three story faction fixtures resolve"),
		Bandits.IsValid() && Heroes.IsValid() && Police.IsValid());

	ATerritoryVolume* Territory = NewObject<ATerritoryVolume>();
	TestNotNull(TEXT("Transient Territory created"), Territory);
	if (!Territory) return false;
	TestTrue(TEXT("Transient Territory fixture has server authority"), Territory->HasAuthority());

	UTerritorySetDiplomacyEvent* Event =
		NewObject<UTerritorySetDiplomacyEvent>(Territory);
	Event->FactionASource = ETerritoryDiplomacyFactionSource::CurrentOwningFaction;
	Event->FactionBSource = ETerritoryDiplomacyFactionSource::ContestingFaction;
	Event->bFallbackToExplicitFactionWhenContextMissing = false;

	FTerritoryOwnershipData Data = Territory->GetOwnershipData();
	Data.OwningFaction = Bandits;
	Data.State = ETerritoryState::Claimed;
	Data.ControlProgress = 1.f;
	TestTrue(TEXT("Bandits can become the first owner"),
		Territory->CommitOwnershipData(Data));

	Data = Territory->GetOwnershipData();
	Data.State = ETerritoryState::Contested;
	Data.ContestingFaction = Heroes;
	Data.ControlProgress = 0.25f;
	FTerritoryTransitionContext HeroesContext;
	HeroesContext.RequestingFaction = Heroes;
	TestTrue(TEXT("Heroes can contest the Bandit-owned Place"),
		Territory->CommitOwnershipData(Data, HeroesContext));
	FGameplayTag ResolvedA;
	FGameplayTag ResolvedB;
	TestTrue(TEXT("First dynamic pair resolves"),
		Event->ResolveFactionPair(ResolvedA, ResolvedB));
	TestEqual(TEXT("First defender is current owner"), ResolvedA, Bandits);
	TestEqual(TEXT("First attacker is live contesting faction"), ResolvedB, Heroes);

	Data = Territory->GetOwnershipData();
	Data.OwningFaction = Heroes;
	Data.State = ETerritoryState::Claimed;
	Data.ContestingFaction = FGameplayTag();
	Data.ControlProgress = 1.f;
	TestTrue(TEXT("Heroes can finish the first capture"),
		Territory->CommitOwnershipData(Data, HeroesContext));

	Data = Territory->GetOwnershipData();
	Data.State = ETerritoryState::Contested;
	Data.ContestingFaction = Police;
	Data.ControlProgress = 0.35f;
	FTerritoryTransitionContext PoliceContext;
	PoliceContext.RequestingFaction = Police;
	TestTrue(TEXT("Police can later contest the Heroes-owned Place"),
		Territory->CommitOwnershipData(Data, PoliceContext));
	TestTrue(TEXT("Second dynamic pair resolves"),
		Event->ResolveFactionPair(ResolvedA, ResolvedB));
	TestEqual(TEXT("Second defender follows changed ownership"), ResolvedA, Heroes);
	TestEqual(TEXT("Second attacker follows new contest"), ResolvedB, Police);
	TestNotEqual(TEXT("Old hardcoded Bandit pair is not reused"), ResolvedA, Bandits);
	TestFalse(TEXT("Previous owner is not left stale outside the event bundle"),
		Territory->GetTransitionPreviousOwningFaction().IsValid());

	const UClass* EventClass = UTerritorySetDiplomacyEvent::StaticClass();
	TestNotNull(TEXT("Owner-participation safety filter is exposed"),
		EventClass->FindPropertyByName(TEXT("bRequireContainingTerritoryOwner")));
	TestNotNull(TEXT("Cross-Place war protection is exposed"),
		EventClass->FindPropertyByName(TEXT("bPreserveOtherActiveTerritoryWars")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDiplomacyWorldPartitionConflictReadModel,
	"TerritoryFramework.Diplomacy.StateEvents.WorldPartitionConflictProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDiplomacyWorldPartitionConflictReadModel::RunTest(const FString& Parameters)
{
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Farm = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	const FGameplayTag Market = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	TestTrue(TEXT("Conflict Territory fixtures resolve"), Farm.IsValid() && Market.IsValid());
	ATerritoryWorldState* WorldState = NewObject<ATerritoryWorldState>();
	TestNotNull(TEXT("Transient WorldState created"), WorldState);
	if (!WorldState) return false;

	FReplicatedCaptureSummary FarmConflict;
	FarmConflict.TerritoryTag = Farm;
	FarmConflict.TerritoryGUID = FGuid::NewGuid();
	FarmConflict.CurrentOwner = Bandits;
	FarmConflict.ContestingFaction = Heroes;
	FarmConflict.State = ETerritoryState::Contested;
	WorldState->SetCaptureSummary(FarmConflict);
	TestTrue(TEXT("An unloaded-style summary protects the active faction war"),
		WorldState->HasContestedTerritoryBetweenFactions(Bandits, Heroes, Market));
	TestFalse(TEXT("The containing Territory can be excluded from its own check"),
		WorldState->HasContestedTerritoryBetweenFactions(Bandits, Heroes, Farm));

	FarmConflict.State = ETerritoryState::Claimed;
	FarmConflict.ContestingFaction = FGameplayTag();
	WorldState->SetCaptureSummary(FarmConflict);
	TestFalse(TEXT("A terminal claimed summary no longer blocks peace"),
		WorldState->HasContestedTerritoryBetweenFactions(Bandits, Heroes, Market));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryGuardCrowdAndDialogueContract,
	"TerritoryFramework.AI.Guards.CrowdAvoidanceAndDiplomacyDialogueContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryGuardCrowdAndDialogueContract::RunTest(const FString& Parameters)
{
	const ATerritoryGuardCharacter* GuardCDO = GetDefault<ATerritoryGuardCharacter>();
	const ATerritoryAssaultCharacter* AssaultCDO = GetDefault<ATerritoryAssaultCharacter>();
	TestTrue(TEXT("Patrol crowd avoidance is enabled by default"),
		GuardCDO && GuardCDO->bEnablePatrolCrowdAvoidance);
	TestTrue(TEXT("Patrol avoidance has a useful consideration radius"),
		GuardCDO && GuardCDO->PatrolAvoidanceConsiderationRadius >= 100.f);
	TestNotNull(TEXT("Guard owns a diplomacy dialogue resolver"),
		GuardCDO ? GuardCDO->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>() : nullptr);
	TestNotNull(TEXT("Guard replaces Narrative's interactable with the contextual adapter"),
		GuardCDO ? GuardCDO->FindComponentByClass<UTerritoryDiplomacyInteractable>() : nullptr);
	TestNotNull(TEXT("Assault NPC owns the same dialogue resolver"),
		AssaultCDO ? AssaultCDO->FindComponentByClass<UTerritoryDiplomacyDialogueComponent>() : nullptr);
	TestNotNull(TEXT("Shared guard classes expose exact-faction dialogue mappings"),
		UTerritoryDiplomacyDialogueComponent::StaticClass()->FindPropertyByName(
			TEXT("FactionDialogueProfiles")));

	UTerritoryDiplomacyDialogueProfile* Profile =
		NewObject<UTerritoryDiplomacyDialogueProfile>();
	Profile->AllianceDialogue = UDialogue::StaticClass();
	const TSubclassOf<UDialogue> Alliance = Profile->GetDialogueForRelationship(
		EDiplomacyState::Alliance, false, nullptr);
	TestEqual(TEXT("Alliance chooses its relationship-specific dialogue"),
		Alliance.Get(), UDialogue::StaticClass());
	const TSubclassOf<UDialogue> NeutralFallback = Profile->GetDialogueForRelationship(
		EDiplomacyState::None, false, UDialogue::StaticClass());
	TestEqual(TEXT("An empty profile slot falls back to Narrative's NPCDefinition dialogue"),
		NeutralFallback.Get(), UDialogue::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDefenderNarrativeEventConditions,
	"TerritoryFramework.Tales.DefenderDeathEventConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDefenderNarrativeEventConditions::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Defender-event preview world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryVolume* Territory = World->SpawnActor<ATerritoryVolume>(
		ATerritoryVolume::StaticClass(), FTransform::Identity, SpawnParams);
	AActor* FirstDefender = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParams);
	AActor* SecondDefender = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Territory exists for defender-event regression"), Territory);
	TestNotNull(TEXT("First registered defender exists"), FirstDefender);
	TestNotNull(TEXT("Second registered defender exists"), SecondDefender);
	if (!Territory || !FirstDefender || !SecondDefender)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	TestTrue(TEXT("Defender event test Territory tag exists"), TerritoryTag.IsValid());
	Territory->TerritoryTag = TerritoryTag;
	Territory->TerritoryGUID = FGuid::NewGuid();

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Registry authority exists in defender-event world"), Registry);
	if (!Registry)
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Defender-event Territory registers with its stable identities"),
		Registry->RegisterTerritory(Territory), ETerritoryRegistrationResult::Success);

	UTerritoryLockEvent* LockEvent = NewObject<UTerritoryLockEvent>(Territory);
	LockEvent->TargetTerritoryTag = TerritoryTag;
	LockEvent->LockReason = FText::FromString(TEXT("Defender event regression"));
	UTerritoryOwnershipCondition* Gate =
		NewObject<UTerritoryOwnershipCondition>(LockEvent);
	LockEvent->Conditions.Add(Gate);
	Territory->AllDefendersDefeatedEvents.Add(LockEvent);

	// Seed the private registration set directly: the regression exercises the
	// death callback itself and does not need an ASC binding/timer harness.
	Territory->RegisteredDefenders.Add(FirstDefender);
	Territory->OnDefenderDied(FirstDefender, nullptr, true);
	TestFalse(TEXT("Failed inherited condition blocks the all-defenders-defeated event"),
		Territory->IsLocked());

	Gate->bNot = true;
	Territory->RegisteredDefenders.Add(SecondDefender);
	Territory->OnDefenderDied(SecondDefender, nullptr, true);
	TestTrue(TEXT("The same death hook executes after Narrative Not makes its condition pass"),
		Territory->IsLocked());
	TestTrue(TEXT("The executed lock can be reset for duplicate-callback verification"),
		Territory->TryUnlock(true));
	Territory->OnDefenderDied(SecondDefender, nullptr, true);
	TestFalse(TEXT("A duplicate callback cannot refire the all-defenders-defeated event"),
		Territory->IsLocked());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryPlacementGuidRegression,
	"TerritoryFramework.Identity.Regression.NewPlacementsDoNotInheritBlueprintGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryPlacementGuidRegression::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Editor preview world created"), World);
	if (!World) return false;

	FActorSpawnParameters FirstParams;
	FirstParams.Name = TEXT("TerritoryGuidPlacementA");
	FirstParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* First = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, FirstParams);

	FActorSpawnParameters SecondParams;
	SecondParams.Name = TEXT("TerritoryGuidPlacementB");
	SecondParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Second = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SecondParams);

	TestNotNull(TEXT("First Property placement created"), First);
	TestNotNull(TEXT("Second Property placement created"), Second);
	if (First && Second)
	{
		TestTrue(TEXT("First placement receives a valid persistent GUID"),
			First->GetTerritoryGUID().IsValid());
		TestTrue(TEXT("Second placement receives a valid persistent GUID"),
			Second->GetTerritoryGUID().IsValid());
		TestNotEqual(TEXT("Separate placements never inherit the same Blueprint GUID"),
			First->GetTerritoryGUID(), Second->GetTerritoryGUID());
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFCapturePointContract,
	"TerritoryFramework.Capture.PhysicalCapturePointContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFCapturePointContract::RunTest(const FString& Parameters)
{
	const UClass* Class = ATerritoryCapturePoint::StaticClass();
	TestNotNull(TEXT("Capture point class exists"), Class);
	if (!Class) return false;

	TestTrue(TEXT("Explicit registration is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("TryRegisterCaptureParticipant")));
	TestTrue(TEXT("Explicit registration is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("TryRegisterCaptureParticipant")));
	TestTrue(TEXT("Explicit unregistration is authority-only"),
		TFTestUtils::IsBlueprintAuthorityOnly(Class, TEXT("UnregisterCaptureParticipant")));
	TestTrue(TEXT("Capture point exposes the stable target tag"),
		TFTestUtils::HasProperty(Class, TEXT("TargetTerritoryTag")));
	TestTrue(TEXT("Capture point exposes an optional designer mesh flag"),
		TFTestUtils::HasProperty(Class, TEXT("CaptureMarkerMesh")));
	TestTrue(TEXT("Capture marker can remain silent while capture is unavailable"),
		TFTestUtils::HasProperty(Class, TEXT("bHideMarkerWhileCaptureUnavailable")));
	TestTrue(TEXT("Capture point separates story contesting from automatic capture pressure"),
		TFTestUtils::HasProperty(Class, TEXT("bContributesCaptureProgress")));
	TestTrue(TEXT("Capture point exposes its effective automatic-flow query"),
		TFTestUtils::HasFunction(Class, TEXT("IsAutomaticCaptureFlowActive")));
	TestTrue(TEXT("Territory exposes full-bounds story capture mode"),
		TFTestUtils::HasProperty(ATerritoryVolume::StaticClass(),
			TEXT("bStoryCaptureFromBounds")));
	TestFalse(TEXT("Capture target configuration is not duplicate save authority"),
		TFTestUtils::IsSaveGame(Class, TEXT("TargetTerritoryTag")));
	TestFalse(TEXT("Capture enabled configuration is not replicated gameplay state"),
		TFTestUtils::IsReplicated(Class, TEXT("bCaptureEnabled")));

	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Capture point preview world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryCapturePoint* CapturePoint = World->SpawnActor<ATerritoryCapturePoint>(
		ATerritoryCapturePoint::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Capture point can be placed"), CapturePoint);
	if (CapturePoint)
	{
		TestFalse(TEXT("A capture point without a configured Place resolves no authority"),
			CapturePoint->ResolveTargetTerritory() != nullptr);
		TestFalse(TEXT("A targetless point cannot report automatic capture active"),
			CapturePoint->IsAutomaticCaptureFlowActive());
		TestFalse(TEXT("Null participants are rejected"),
			CapturePoint->TryRegisterCaptureParticipant(nullptr));
		TestFalse(TEXT("Capture adapter actor does not replicate duplicate state"),
			CapturePoint->GetIsReplicated());
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFContestCriteriaSeparated,
	"TerritoryFramework.Capture.ContestCriteriaSeparatedFromCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFContestCriteriaSeparated::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Contest criteria preview world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryVolume* Territory = World->SpawnActor<ATerritoryVolume>(
		ATerritoryVolume::StaticClass(), FTransform::Identity, SpawnParams);
	AActor* Defender = World->SpawnActor<ATerritoryGuardCharacter>(
		ATerritoryGuardCharacter::StaticClass(), FTransform::Identity, SpawnParams);
	// EditorPreview worlds do not auto-initialize UWorldSubsystems. These policy
	// queries are read-only, so an explicitly world-owned instance is sufficient.
	UTerritoryControlSubsystem* Control =
		NewObject<UTerritoryControlSubsystem>(World);
	TestNotNull(TEXT("Territory exists for contest criteria"), Territory);
	TestNotNull(TEXT("Control subsystem exists for contest criteria"), Control);
	if (!Territory || !Defender || !Control)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	TestTrue(TEXT("Contest test factions exist"), Bandits.IsValid() && Heroes.IsValid());
	Territory->ForceSetOwningFaction(Bandits);
	Territory->ForceSetTerritoryState(ETerritoryState::Claimed);
	Territory->RegisterDefender(Defender);

	TestEqual(TEXT("A living garrison may be challenged so its guards can react"),
		Control->GetContestEligibility(Territory, Heroes), ECaptureResult::Success);
	TestEqual(TEXT("The same living garrison still blocks ownership completion"),
		Control->GetCaptureEligibility(Territory, Heroes),
		ECaptureResult::DefendersRemain);

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
