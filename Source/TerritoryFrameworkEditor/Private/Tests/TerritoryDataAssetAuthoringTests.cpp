#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/TerritoryDiplomacyDialogue.h"
#include "Assets/TerritoryAssetFactories.h"
#include "AssetToolsModule.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardPostDefinition.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "DataValidation/TerritoryDataValidator.h"
#include "Engine/DataAsset.h"
#include "IAssetTools.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Misc/DataValidation.h"
#include "PropertyEditorModule.h"
#include "Tales/Dialogue.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	TArray<UClass*> TerritoryAssetClasses()
	{
		TArray<UClass*> Result;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetOutermost()->GetName() == TEXT("/Script/TerritoryFramework")
				&& It->IsChildOf<UDataAsset>()
				&& !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
				Result.Add(*It);
		}
		Result.Sort([](const UClass& A, const UClass& B) { return A.GetName() < B.GetName(); });
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryDataAssetCategoryOrder,
	"TerritoryFramework.Editor.Authoring.DataAssetCategoryOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryDataAssetCategoryOrder::RunTest(const FString& Parameters)
{
	auto& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	for (UClass* Class : TerritoryAssetClasses())
	{
		UObject* Asset = NewObject<UObject>(GetTransientPackage(), Class, NAME_None, RF_Transient);
		const auto Generator = PropertyEditor.CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Generator->SetObjects({Asset});
		TArray<FString> Categories;
		for (const auto& Node : Generator->GetRootTreeNodes())
			if (Node->GetNodeType() == EDetailNodeType::Category)
				Categories.Add(Node->GetNodeName().ToString());
		AddInfo(Class->GetName() + TEXT(": ") + FString::Join(Categories, TEXT(", ")));
		TArray<FString> Numbered;
		for (const FString& Category : Categories)
			if (!Category.IsEmpty() && FChar::IsDigit(Category[0])) Numbered.Add(Category);
		for (int32 Index = 1; Index < Numbered.Num(); ++Index)
			TestTrue(*FString::Printf(TEXT("%s: %s precedes %s"), *Class->GetName(),
				*Numbered[Index - 1], *Numbered[Index]), Numbered[Index - 1] < Numbered[Index]);
		if (Class->IsChildOf<UTerritoryDefinition>())
		{
			TestTrue(TEXT("Inherited identity category is present"), Categories.Contains(TEXT("01 Identity")));
			TestEqual(TEXT("Exactly one inherited story panel"), Categories.FilterByPredicate(
				[](const FString& Name) { return Name == TEXT("00 Story Outcome (Read Only)"); }).Num(), 1);
			if (Class == UTerritoryPlaceDefinition::StaticClass())
			{
				TestTrue(TEXT("Place benefits/upgrades category is present"), Categories.Contains(TEXT("10 Place")));
				TestTrue(TEXT("Place story category is present"), Categories.Contains(TEXT("11 Place")));
				TestTrue(TEXT("Place capture category is present"), Categories.Contains(TEXT("06 Capture")));
			}
			else TestFalse(TEXT("Aggregate context does not expose physical capture"), Categories.Contains(TEXT("06 Capture")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryAllDataAssetMenus,
	"TerritoryFramework.Editor.Authoring.AllDataAssetMenusAndFactories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryAllDataAssetMenus::RunTest(const FString& Parameters)
{
	auto& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	const uint32 Category = AssetTools.FindAdvancedAssetCategory(TEXT("TerritoryFramework"));
	const TArray<UClass*> Classes = TerritoryAssetClasses();
	TestTrue(TEXT("The complete current data asset family is inspected"), Classes.Num() >= 12);
	for (UClass* Class : Classes)
	{
		const auto Action = AssetTools.GetAssetTypeActionsForClass(Class).Pin();
		TestTrue(*FString::Printf(TEXT("%s has exact Territory asset actions"), *Class->GetName()),
			Action && Action->GetSupportedClass() == Class && (Action->GetCategories() & Category) != 0
			&& !Action->GetSubMenus().IsEmpty());
		int32 FactoryCount = 0;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf<UTerritoryAssetFactoryBase>() || It->HasAnyClassFlags(CLASS_Abstract)) continue;
			auto* Factory = It->GetDefaultObject<UTerritoryAssetFactoryBase>();
			if (Factory->GetSupportedClass() != Class) continue;
			++FactoryCount;
			TestTrue(TEXT("Named factory is visible"), Factory->ShouldShowInNewMenu());
			UObject* Created = Factory->FactoryCreateNew(Class, GetTransientPackage(),
				MakeUniqueObjectName(GetTransientPackage(), Class), RF_Transient, nullptr, GWarn);
			TestTrue(TEXT("Factory creates the selected asset class"), Created && Created->GetClass() == Class);
			TestNull(TEXT("Factory rejects an incompatible requested class"), Factory->FactoryCreateNew(
				UDataAsset::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient, nullptr, GWarn));
		}
		TestEqual(*FString::Printf(TEXT("%s has one named creation factory"), *Class->GetName()), FactoryCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryDataAssetValidationCoverage,
	"TerritoryFramework.Editor.Authoring.DataAssetValidationCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryDataAssetValidationCoverage::RunTest(const FString& Parameters)
{
	auto* Validator = NewObject<UTerritoryDataValidator>();
	const TArray<FAssetData> Associated;
	for (UClass* Class : TerritoryAssetClasses())
	{
		UObject* Asset = NewObject<UObject>(GetTransientPackage(), Class, NAME_None, RF_Transient);
		FDataValidationContext Context(false, EDataValidationUsecase::Script, Associated);
		TestTrue(*FString::Printf(TEXT("%s has a real validation owner"), *Class->GetName()),
			Validator->CanValidateAsset_Implementation(FAssetData(Asset), Asset, Context)
			|| Asset->IsDataValid(Context) != EDataValidationResult::NotValidated);
	}
	for (float Value : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		for (int32 Field = 0; Field < 4; ++Field)
		{
			auto* Post = NewObject<UTerritoryGuardPostDefinition>();
			if (Field == 0) Post->ReserveSpawnDelay = Value;
			if (Field == 1) Post->ReserveSpawnRetryInterval = Value;
			if (Field == 2) Post->ReserveSpawnRadius = Value;
			if (Field == 3) Post->ReserveMinimumPlayerDistance = Value;
			FDataValidationContext Context(false, EDataValidationUsecase::Script, Associated);
			TestEqual(TEXT("Guard post rejects non-finite reserve configuration"),
				Validator->ValidateLoadedAsset_Implementation(FAssetData(Post), Post, Context), EDataValidationResult::Invalid);
		}
	}
	auto* Profile = NewObject<UTerritoryDiplomacyDialogueProfile>();
	FDataValidationContext EmptyContext(false, EDataValidationUsecase::Script, Associated);
	TestEqual(TEXT("Empty diplomacy slots intentionally use the Native fallback"),
		Validator->ValidateLoadedAsset_Implementation(FAssetData(Profile), Profile, EmptyContext), EDataValidationResult::Valid);
	Profile->WarDialogue = UDialogue::StaticClass();
	FDataValidationContext BadContext(false, EDataValidationUsecase::Script, Associated);
	TestEqual(TEXT("An assigned Native dialogue without a playable graph is rejected"),
		Validator->ValidateLoadedAsset_Implementation(FAssetData(Profile), Profile, BadContext), EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryDataAssetNumberContracts,
	"TerritoryFramework.Editor.Authoring.DataAssetNumberContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryDataAssetNumberContracts::RunTest(const FString& Parameters)
{
	auto* Validator = NewObject<UTerritoryDataValidator>();
	const TArray<FAssetData> Associated;
	const auto HasNumericIssue = [&](UObject* Asset, const FString& Expected)
	{
		FDataValidationContext Context(false, EDataValidationUsecase::Script, Associated);
		Validator->ValidateLoadedAsset_Implementation(FAssetData(Asset), Asset, Context);
		return Context.GetIssues().ContainsByPredicate([&](const FDataValidationContext::FIssue& Issue)
		{
			return Issue.Message.ToString().Contains(Expected);
		});
	};
	int32 FieldCount = 0;
	for (UClass* Class : TerritoryAssetClasses())
	{
		UObject* Asset = NewObject<UObject>(GetTransientPackage(), Class, NAME_None, RF_Transient);
		for (TFieldIterator<FNumericProperty> It(Class); It; ++It)
		{
			if (!It->IsFloatingPoint() || !It->HasAnyPropertyFlags(CPF_Edit)) continue;
			void* Value = It->ContainerPtrToValuePtr<void>(Asset);
			const double Original = It->GetFloatingPointPropertyValue(Value);
			for (double Bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
			{
				It->SetFloatingPointPropertyValue(Value, Bad);
				TestTrue(*FString::Printf(TEXT("%s.%s rejects non-finite values explicitly"), *Class->GetName(), *It->GetName()),
					HasNumericIssue(Asset, It->GetName() + TEXT(" must be finite")));
			}
			It->SetFloatingPointPropertyValue(Value, Original);
			++FieldCount;
		}
	}
	AddInfo(FString::Printf(TEXT("Checked NaN and infinity on %d editable floating-point fields across every concrete data asset class"), FieldCount));
	TestTrue(TEXT("Numeric sweep includes the complete authored profile surface"), FieldCount > 40);
	auto* Place = NewObject<UTerritoryPlaceDefinition>();
	Place->GuardBehavior.PatrolAvoidanceWeight = std::numeric_limits<float>::infinity();
	TestTrue(TEXT("Struct member is checked by its complete property path"),
		HasNumericIssue(Place, TEXT("GuardBehavior.PatrolAvoidanceWeight must be finite")));
	Place->StateConfigs.FindChecked(ETerritoryState::Claimed).Audio.StateEffectPitch = -1.f;
	TestTrue(TEXT("Map value's nested audio clamp is enforced"),
		HasNumericIssue(Place, TEXT("Audio.StateEffectPitch is below its minimum")));
	auto* Counter = NewObject<UTerritoryCounterAttackProfile>();
	Counter->FactionForces.AddDefaulted_GetRef().MilitaryPower = std::numeric_limits<float>::quiet_NaN();
	TestTrue(TEXT("Array member is checked by its complete property path"),
		HasNumericIssue(Counter, TEXT("FactionForces[0].MilitaryPower must be finite")));
	auto* Post = NewObject<UTerritoryGuardPostDefinition>();
	Post->ReserveSpawnDelay = 0.1f;
	TestFalse(TEXT("Valid float boundary is compared in its storage precision"),
		HasNumericIssue(Post, TEXT("ReserveSpawnDelay is below")));
	return true;
}

#endif
