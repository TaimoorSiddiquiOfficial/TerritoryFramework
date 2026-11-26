#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryEvidenceTypes.h"
#include "Core/TerritoryWorldState.h"
#include "Subsystems/TerritoryEvidenceLedgerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
	bool HasFunctionFlag(const UClass* Class, const FName Name, const EFunctionFlags Flag)
	{
		if (!Class)
		{
			return false;
		}
		const UFunction* Function = Class->FindFunctionByName(Name);
		return Function != nullptr && Function->HasAnyFunctionFlags(Flag);
	}
}

/**
 * AGENTS.md §10.6 (Blueprint contract test) for the evidence ledger: the surface a designer
 * script is allowed to touch, and the flags that decide who may touch it. Reflection only —
 * the behavioural cases live in TerritoryEvidenceLedgerTests.cpp.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFEvidenceLedgerContract,
	"TerritoryFramework.Evidence.Contract.BlueprintSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFEvidenceLedgerContract::RunTest(const FString& Parameters)
{
	const UClass* LedgerClass = UTerritoryEvidenceLedgerSubsystem::StaticClass();
	const UClass* StateClass = ATerritoryWorldState::StaticClass();
	const UClass* LibraryClass = UTerritoryBlueprintLibrary::StaticClass();
	if (!TestNotNull(TEXT("The ledger class exists"), LedgerClass)
		|| !TestNotNull(TEXT("The WorldState class exists"), StateClass)
		|| !TestNotNull(TEXT("The blueprint library exists"), LibraryClass))
	{
		return false;
	}

	// The state authority is a World subsystem (AGENTS.md §4.1: one owner for this state).
	TestTrue(TEXT("The ledger is a World subsystem"),
		LedgerClass->IsChildOf(UWorldSubsystem::StaticClass()));

	// The mutator is reachable from Blueprint but authority-only (§4.2, §8).
	TestTrue(TEXT("RecordEvidence is BlueprintCallable"),
		HasFunctionFlag(LedgerClass, TEXT("RecordEvidence"), FUNC_BlueprintCallable));
	TestTrue(TEXT("RecordEvidence is BlueprintAuthorityOnly"),
		HasFunctionFlag(LedgerClass, TEXT("RecordEvidence"), FUNC_BlueprintAuthorityOnly));

	// Reads are pure, so a widget or condition can query without changing state (§8).
	const FName PureQueries[] =
	{
		TEXT("IsEvidenceLedgerReady"),
		TEXT("GetEvidenceRecords"),
		TEXT("GetEvidenceForSubject"),
		TEXT("GetEvidenceForTag"),
		TEXT("GetEvidenceForTerritory"),
		TEXT("GetEvidenceWeight"),
		TEXT("GetEvidenceWeightForSubject"),
		TEXT("GetRetainedEvidenceCount"),
		TEXT("GetEvictedEvidenceCount"),
		TEXT("GetTotalRecordedEvidenceCount")
	};
	for (const FName Name : PureQueries)
	{
		TestTrue(FString::Printf(TEXT("%s is BlueprintPure"), *Name.ToString()),
			HasFunctionFlag(LedgerClass, Name, FUNC_BlueprintPure));
	}

	// The live delegate has to be assignable from Blueprint, or a HUD cannot react to a fact.
	const FMulticastDelegateProperty* Recorded = CastField<FMulticastDelegateProperty>(
		LedgerClass->FindPropertyByName(TEXT("OnEvidenceRecorded")));
	TestNotNull(TEXT("OnEvidenceRecorded exists"), Recorded);
	if (Recorded)
	{
		TestTrue(TEXT("OnEvidenceRecorded is BlueprintAssignable"),
			Recorded->HasAnyPropertyFlags(CPF_BlueprintAssignable));
	}

	// The durable record survives a save, so every field has to carry SaveGame.
	const UScriptStruct* RecordStruct = FTerritoryEvidenceRecord::StaticStruct();
	TestNotNull(TEXT("The evidence record is a reflected struct"), RecordStruct);
	if (RecordStruct)
	{
		for (TFieldIterator<FProperty> It(RecordStruct); It; ++It)
		{
			TestTrue(FString::Printf(TEXT("Record field %s is SaveGame"), *It->GetName()),
				It->HasAnyPropertyFlags(CPF_SaveGame));
		}
	}

	// The projection surface: three replicated/saved fields and two pure reads.
	TestNotNull(TEXT("ReplicatedEvidence is reflected"),
		StateClass->FindPropertyByName(TEXT("ReplicatedEvidence")));
	TestNotNull(TEXT("bEvidenceLedgerPublished is reflected"),
		StateClass->FindPropertyByName(TEXT("bEvidenceLedgerPublished")));
	TestNotNull(TEXT("SavedEvidence is reflected"),
		StateClass->FindPropertyByName(TEXT("SavedEvidence")));
	TestTrue(TEXT("WorldState::GetEvidenceLedger is BlueprintPure"),
		HasFunctionFlag(StateClass, TEXT("GetEvidenceLedger"), FUNC_BlueprintPure));
	TestTrue(TEXT("WorldState::IsEvidenceLedgerReady is BlueprintPure"),
		HasFunctionFlag(StateClass, TEXT("IsEvidenceLedgerReady"), FUNC_BlueprintPure));

	// The Blueprint accessor recipe used by every other Territory subsystem.
	TestTrue(TEXT("GetTerritoryEvidenceLedger is BlueprintPure"),
		HasFunctionFlag(LibraryClass, TEXT("GetTerritoryEvidenceLedger"), FUNC_BlueprintPure));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
