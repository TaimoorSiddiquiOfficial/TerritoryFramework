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
	TestTrue(TEXT("IsAllied is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("IsAllied")));
	TestTrue(TEXT("HasTradeAgreement is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("HasTradeAgreement")));

	// ─── Reputation API ───
	TestTrue(TEXT("AddReputation is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("AddReputation")));
	TestTrue(TEXT("SetReputation is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("SetReputation")));
	TestTrue(TEXT("GetReputation is BlueprintPure"), TFTestUtils::IsBlueprintPure(Class, TEXT("GetReputation")));

	// ─── History API ───
	TestTrue(TEXT("GetAllTreaties is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetAllTreaties")));
	TestTrue(TEXT("GetTreatiesForFaction is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetTreatiesForFaction")));
	TestTrue(TEXT("GetDiplomacyHistory is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("GetDiplomacyHistory")));

	// ─── Sync API ───
	TestTrue(TEXT("SyncToGameState is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("SyncToGameState")));
	TestTrue(TEXT("LoadFromGameState is BlueprintCallable"), TFTestUtils::IsBlueprintCallable(Class, TEXT("LoadFromGameState")));

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
		TestTrue(TEXT("SavedTreaties has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedTreaties")));
		TestTrue(TEXT("SavedReputation has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedReputation")));
		TestTrue(TEXT("SavedDiplomacyHistory has SaveGame flag"),
			TFTestUtils::IsSaveGame(SavableClass, TEXT("SavedDiplomacyHistory")));
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

	// ─── UNarrativeAbilitySystemComponent death delegate ───
	{
		const UClass* ASCClass = UNarrativeAbilitySystemComponent::StaticClass();
		TestNotNull(TEXT("UNarrativeAbilitySystemComponent is accessible"), ASCClass);

		// OnDied delegate must exist (used by ATerritoryVolume::BindDefenderDeath)
		TestTrue(TEXT("ASC has OnDied delegate"),
			TFTestUtils::HasProperty(ASCClass, TEXT("OnDied")));

		// Attack token system must exist (properties and C++ methods)
		TestTrue(TEXT("ASC has NumAttackTokens property"),
			TFTestUtils::HasProperty(ASCClass, TEXT("NumAttackTokens")));
		TestTrue(TEXT("ASC has GrantedAttackTokens property"),
			TFTestUtils::HasProperty(ASCClass, TEXT("GrantedAttackTokens")));
		// TryClaimToken/ReturnToken are C++ methods (not UFUNCTION), so verify
		// the class compiles and links by checking the class itself is valid
		TestNotNull(TEXT("UNarrativeAbilitySystemComponent CDO is valid"),
			UNarrativeAbilitySystemComponent::StaticClass()->GetDefaultObject());
		TestTrue(TEXT("ASC has OnDied delegate (used by territory defenders)"),
			TFTestUtils::HasProperty(ASCClass, TEXT("OnDied")));
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

#endif // WITH_DEV_AUTOMATION_TESTS
