#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"
#include "Tales/TerritoryAIObservationTask.h"
#include "Tales/TerritoryAssaultTask.h"
#include "Tales/TerritoryCaptureTask.h"
#include "Tales/TerritoryCharacterActionTask.h"
#include "Tales/TerritoryCombatProgressTask.h"
#include "Tales/TerritoryDisguiseTask.h"
#include "Tales/TerritoryGameplayStateTask.h"
#include "Tales/TerritoryStateTask.h"

namespace TerritoryNarrativeTaskPickerTests
{
	void VerifyTaskBlueprint(FAutomationTestBase& Test, const TCHAR* AssetPath,
		UClass* ExpectedParent, const TCHAR* ExpectedDisplayName,
		const TCHAR* ExpectedCategory = TEXT("Territory"))
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, AssetPath);
		Test.TestNotNull(FString::Printf(TEXT("%s loads"), AssetPath), Blueprint);
		if (!Blueprint)
		{
			return;
		}

		Test.TestEqual(FString::Printf(TEXT("%s uses the correct Narrative Task parent"), AssetPath),
			Blueprint->ParentClass.Get(), ExpectedParent);
		Test.TestEqual(FString::Printf(TEXT("%s has a friendly picker name"), AssetPath),
			Blueprint->BlueprintDisplayName, FString(ExpectedDisplayName));
		Test.TestEqual(FString::Printf(TEXT("%s uses its expected Narrative task category"), AssetPath),
			Blueprint->BlueprintCategory, FString(ExpectedCategory));
		Test.TestTrue(FString::Printf(TEXT("%s explains the objective in easy English"), AssetPath),
			!Blueprint->BlueprintDescription.IsEmpty()
			&& Blueprint->BlueprintDescription.Contains(TEXT("Easy example:")));
		Test.TestNotNull(FString::Printf(TEXT("%s has a compiled generated class"), AssetPath),
			Blueprint->GeneratedClass.Get());
		if (Blueprint->GeneratedClass)
		{
			Test.TestTrue(FString::Printf(TEXT("%s generated class inherits the expected task"), AssetPath),
				Blueprint->GeneratedClass->IsChildOf(ExpectedParent));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryNarrativeTaskPickerAssets,
	"TerritoryFramework.Editor.Tales.NarrativeTaskPickerAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryNarrativeTaskPickerAssets::RunTest(const FString& Parameters)
{
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_TerritoryCapture.BPT_TerritoryCapture"),
		UTerritoryCaptureTask::StaticClass(), TEXT("Capture or Lose Territory"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_TerritoryState.BPT_TerritoryState"),
		UTerritoryStateTask::StaticClass(), TEXT("Territory State or Garrison"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_TerritoryCounterattack.BPT_TerritoryCounterattack"),
		UTerritoryAssaultTask::StaticClass(), TEXT("Territory Counterattack or Chase"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_TerritoryDisguise.BPT_TerritoryDisguise"),
		UTerritoryDisguiseTask::StaticClass(), TEXT("Territory Disguise Mission"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_TerritoryBossFight.BPT_TerritoryBossFight"),
		UTerritoryAssaultTask::StaticClass(), TEXT("Territory Boss Fight or Chase Outcome"),
		TEXT("Territory Story"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_CharacterMovementAction.BPT_CharacterMovementAction"),
		UTerritoryCharacterActionTask::StaticClass(), TEXT("Character Movement Action"),
		TEXT("Community - Movement"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_GameplayState.BPT_GameplayState"),
		UTerritoryGameplayStateTask::StaticClass(), TEXT("Gameplay Tag or Attribute State"),
		TEXT("Community - GAS"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_CombatProgress.BPT_CombatProgress"),
		UTerritoryCombatProgressTask::StaticClass(), TEXT("Combat Progress"),
		TEXT("Community - Combat"));
	TerritoryNarrativeTaskPickerTests::VerifyTaskBlueprint(*this,
		TEXT("/TerritoryFramework/Tales/Tasks/BPT_AIObservation.BPT_AIObservation"),
		UTerritoryAIObservationTask::StaticClass(), TEXT("AI Story Observation"),
		TEXT("Community - AI"));

	UClass* SettingsClass = FindObject<UClass>(nullptr,
		TEXT("/Script/NarrativeQuestEditor.QuestEditorSettings"));
	TestNotNull(TEXT("Narrative Quest Editor settings class is loaded"), SettingsClass);
	FArrayProperty* SearchPathsProperty = SettingsClass
		? FindFProperty<FArrayProperty>(SettingsClass, TEXT("QuestTaskSearchPaths"))
		: nullptr;
	FStrProperty* PathProperty = SearchPathsProperty
		? CastField<FStrProperty>(SearchPathsProperty->Inner) : nullptr;
	TestNotNull(TEXT("Narrative exposes its task search-path array"), SearchPathsProperty);
	TestNotNull(TEXT("Narrative task search paths are strings"), PathProperty);

	bool bHasTerritoryPath = false;
	if (SettingsClass && SearchPathsProperty && PathProperty)
	{
		FScriptArrayHelper SearchPaths(SearchPathsProperty,
			SearchPathsProperty->ContainerPtrToValuePtr<void>(
				SettingsClass->GetDefaultObject()));
		for (int32 Index = 0; Index < SearchPaths.Num(); ++Index)
		{
			bHasTerritoryPath |= PathProperty->GetPropertyValue(
				SearchPaths.GetRawPtr(Index)).Equals(
					TEXT("/TerritoryFramework/Tales/Tasks/"),
					ESearchCase::IgnoreCase);
		}
	}
	TestTrue(TEXT("Narrative Quest Editor searches the Territory task asset folder"),
		bHasTerritoryPath);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
