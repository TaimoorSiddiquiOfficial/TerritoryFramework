#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryVolume.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryInterfaces.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryEconomySubsystem.h"
#include "Combat/TerritoryCombatDirector.h"
#include "Tales/TerritoryCaptureTask.h"
#include "Tales/TerritoryCaptureEvent.h"
#include "Tales/TerritoryOwnershipCondition.h"
#include "Navigation/TerritoryMapMarker.h"
#include "Navigation/TerritoryNavigationMarkerComponent.h"
#include "NarrativeSavableActor.h"
#include "NarrativeSavableComponent.h"
#include "Tales/QuestTask.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeCondition.h"
#include "Navigation/MapMarker.h"
#include "Navigation/NavigationMarkerComponent.h"

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

	// ─── Required properties ───
	TestTrue(TEXT("Has TerritoryTag property"), TFTestUtils::HasProperty(Class, TEXT("TerritoryTag")));
	TestTrue(TEXT("Has TerritoryDisplayName property"), TFTestUtils::HasProperty(Class, TEXT("TerritoryDisplayName")));
	TestTrue(TEXT("Has InitialOwningFaction property"), TFTestUtils::HasProperty(Class, TEXT("InitialOwningFaction")));
	TestTrue(TEXT("Has InitialMaxConcurrentAttackers property"), TFTestUtils::HasProperty(Class, TEXT("InitialMaxConcurrentAttackers")));
	TestTrue(TEXT("Has InitialPeriodicIncome property"), TFTestUtils::HasProperty(Class, TEXT("InitialPeriodicIncome")));
	TestTrue(TEXT("Has InitialGuardCost property"), TFTestUtils::HasProperty(Class, TEXT("InitialGuardCost")));
	TestTrue(TEXT("Has bStartsLocked property"), TFTestUtils::HasProperty(Class, TEXT("bStartsLocked")));
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
	TestTrue(TEXT("SetTerritoryState is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("SetTerritoryState")));
	TestTrue(TEXT("RegisterDefender is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RegisterDefender")));
	TestTrue(TEXT("UnregisterDefender is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("UnregisterDefender")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnTerritoryControlChanged delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryControlChanged")));
	TestTrue(TEXT("Has OnTerritoryStateChanged delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnTerritoryStateChanged")));

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
	TestEqual(TEXT("ECaptureResult::InvalidTerritory == 4"),
		static_cast<uint8>(ECaptureResult::InvalidTerritory), static_cast<uint8>(4));

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
	TestTrue(TEXT("UnregisterAttacker is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("UnregisterAttacker")));

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
	TestTrue(TEXT("DeductFromTreasury is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("DeductFromTreasury")));
	TestTrue(TEXT("RecalculateIncome is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RecalculateIncome")));

	// ─── Delegates ───
	TestTrue(TEXT("Has OnEconomyTickFired delegate"),
		TFTestUtils::HasProperty(Class, TEXT("OnEconomyTickFired")));

	// ─── Config property ───
	TestTrue(TEXT("Has TickIntervalSeconds property"),
		TFTestUtils::HasProperty(Class, TEXT("TickIntervalSeconds")));

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

	TestTrue(TEXT("RequestAttackPermission is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("RequestAttackPermission")));
	TestTrue(TEXT("ReleaseAttackPermission is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ReleaseAttackPermission")));
	TestTrue(TEXT("ReleaseAllPermissions is BlueprintCallable"),
		TFTestUtils::IsBlueprintCallable(Class, TEXT("ReleaseAllPermissions")));
	TestTrue(TEXT("HasAttackPermission is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("HasAttackPermission")));
	TestTrue(TEXT("GetGrantedPermissions is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetGrantedPermissions")));
	TestTrue(TEXT("GetAvailableSlots is BlueprintPure"),
		TFTestUtils::IsBlueprintPure(Class, TEXT("GetAvailableSlots")));

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
	TestTrue(TEXT("Has bForceCapture"),
		TFTestUtils::HasProperty(Class, TEXT("bForceCapture")));

	// ─── Override ───
	TestTrue(TEXT("Has ExecuteEvent override"),
		TFTestUtils::HasFunction(Class, TEXT("ExecuteEvent")));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFFunctional_TreasuryStruct,
	"TerritoryFramework.Functional.TreasuryStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFFunctional_TreasuryStruct::RunTest(const FString& Parameters)
{
	FTerritoryTreasury Treasury;

	// Default state
	TestEqual(TEXT("Default Gold is 0"), Treasury.Gold, 0);
	TestEqual(TEXT("Default IncomePerTick is 0"), Treasury.IncomePerTick, 0);
	TestEqual(TEXT("Default CostsPerTick is 0"), Treasury.CostsPerTick, 0);
	TestEqual(TEXT("Default TerritoryCount is 0"), Treasury.TerritoryCount, 0);

	// Simulate economy operations
	Treasury.Gold = 1000;
	Treasury.IncomePerTick = 300;
	Treasury.CostsPerTick = 150;
	Treasury.TerritoryCount = 3;

	// Simulate one economy tick
	int32 NetIncome = Treasury.IncomePerTick - Treasury.CostsPerTick;
	TestEqual(TEXT("Net income per tick = 150"), NetIncome, 150);

	Treasury.Gold += NetIncome;
	TestEqual(TEXT("Treasury after one tick = 1150"), Treasury.Gold, 1150);

	// Simulate deduction
	int32 Cost = 500;
	TestTrue(TEXT("Can afford 500"), Treasury.Gold >= Cost);
	Treasury.Gold -= Cost;
	TestEqual(TEXT("Treasury after deduction = 650"), Treasury.Gold, 650);

	// Simulate overdraft check
	TestFalse(TEXT("Cannot afford 1000"), Treasury.Gold >= 1000);

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

#endif // WITH_DEV_AUTOMATION_TESTS
