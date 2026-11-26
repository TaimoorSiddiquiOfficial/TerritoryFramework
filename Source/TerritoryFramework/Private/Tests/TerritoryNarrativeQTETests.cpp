#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Cinematics/TerritoryNarrativeQTEBlueprintLibrary.h"
#include "LevelSequenceDirector.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryNarrativeQTEBlueprintContract,
	"TerritoryFramework.Cinematics.QTE.BlueprintContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryNarrativeQTEBlueprintContract::RunTest(const FString& Parameters)
{
	const UFunction* Function = UTerritoryNarrativeQTEBlueprintLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UTerritoryNarrativeQTEBlueprintLibrary,
			ApplyNarrativeQTEFailureInstakill));
	if (!TestNotNull(TEXT("QTE failure adapter is exposed to Blueprint"), Function))
	{
		return false;
	}

	TestTrue(TEXT("QTE failure adapter is Blueprint callable"),
		Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));

	// BlueprintAuthorityOnly is a UHT specifier that always sets FUNC_BlueprintAuthorityOnly
	// (Programs/Shared/EpicGames.UHT/UhtFunctionSpecifiers.cs), so the flag is the portable
	// contract. HasMetaData only exists when WITH_METADATA is enabled (WITH_METADATA ==
	// WITH_EDITORONLY_DATA in CoreMiscDefines.h), so it must be guarded or the test fails to
	// compile in Game/Client/Server targets even though WITH_DEV_AUTOMATION_TESTS is set there.
	bool bAuthorityOnly = Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly);
#if WITH_METADATA
	bAuthorityOnly = bAuthorityOnly || Function->HasMetaData(TEXT("BlueprintAuthorityOnly"));
#endif
	TestTrue(TEXT("QTE failure adapter is explicitly authority-only"), bAuthorityOnly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryNarrativeQTERejectsInvalidContext,
	"TerritoryFramework.Cinematics.QTE.InvalidContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryNarrativeQTERejectsInvalidContext::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Null Director is rejected without mutating gameplay state"),
		UTerritoryNarrativeQTEBlueprintLibrary::ApplyNarrativeQTEFailureInstakill(nullptr),
		ETerritoryNarrativeQTEFailureResult::InvalidDirector);

	ULevelSequenceDirector* Director = NewObject<ULevelSequenceDirector>();
	TestEqual(TEXT("Director without a sequence player is rejected"),
		UTerritoryNarrativeQTEBlueprintLibrary::ApplyNarrativeQTEFailureInstakill(Director),
		ETerritoryNarrativeQTEFailureResult::InvalidDirector);
	return true;
}

#endif
