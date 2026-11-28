// Copyright (c) TerritoryFramework. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Misc/AutomationTest.h"
#include "UI/TerritoryDossierRowWidget.h"
#include "UI/TerritoryDossierWidget.h"
#include "Widgets/NarrativeCommonButtonBase.h"

/**
 * Verifies the authored Dossier widgets match the C++ binding contract.
 *
 * The Dossier screen binds its widgets with BindWidgetOptional, so a mismatched name or
 * class stays silent at runtime: the widget compiles, the binding simply never resolves.
 * These tests read the authored trees back off the widget classes
 * (UWidgetBlueprintGeneratedClass::GetWidgetTreeArchetype) and pin every bound name and
 * class, the switcher page count against the drawer enum, and the row class default the
 * shell instantiates its rows from.
 */
namespace TerritoryDossierWidgetAssetTests
{
	static const TCHAR* ShellClassPath =
		TEXT("/Game/TerritoryFramework/UI/WBP_HopDossier.WBP_HopDossier_C");
	static const TCHAR* RowClassPath =
		TEXT("/Game/TerritoryFramework/UI/WBP_DossierRecordRow.WBP_DossierRecordRow_C");

	UWidgetBlueprintGeneratedClass* LoadWidgetClass(FAutomationTestBase& Test, const TCHAR* ClassPath)
	{
		UWidgetBlueprintGeneratedClass* WidgetClass =
			LoadObject<UWidgetBlueprintGeneratedClass>(nullptr, ClassPath);
		Test.TestNotNull(
			FString::Printf(TEXT("The authored widget class %s exists"), ClassPath),
			WidgetClass);
		return WidgetClass;
	}

	UWidgetTree* LoadAuthoredTree(FAutomationTestBase& Test, UWidgetBlueprintGeneratedClass* WidgetClass)
	{
		if (!WidgetClass)
		{
			return nullptr;
		}

		UWidgetTree* Tree = WidgetClass->GetWidgetTreeArchetype();
		Test.TestNotNull(
			FString::Printf(TEXT("%s exposes its authored widget tree"), *WidgetClass->GetName()),
			Tree);
		return Tree;
	}

	template <typename WidgetType>
	void ExpectBoundWidget(FAutomationTestBase& Test, UWidgetTree* Tree, const TCHAR* WidgetName)
	{
		if (!Tree)
		{
			Test.AddError(FString::Printf(
				TEXT("Cannot verify '%s': the authored widget tree is missing."), WidgetName));
			return;
		}

		UWidget* Found = Tree->FindWidget(FName(WidgetName));
		Test.TestNotNull(
			FString::Printf(
				TEXT("The authored tree exposes a widget named '%s' for the C++ binding"), WidgetName),
			Found);

		if (Found && !Found->IsA<WidgetType>())
		{
			Test.AddError(FString::Printf(
				TEXT("'%s' is a %s, but the C++ binding expects a %s, so the binding never resolves."),
				WidgetName,
				*Found->GetClass()->GetName(),
				*WidgetType::StaticClass()->GetName()));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFDossierRowWidgetContract,
	"TerritoryFramework.Dossier.Contract.AuthoredRowWidgetBindsItsTextsAndButton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDossierRowWidgetContract::RunTest(const FString& Parameters)
{
	using namespace TerritoryDossierWidgetAssetTests;

	UWidgetBlueprintGeneratedClass* RowClass = LoadWidgetClass(*this, RowClassPath);
	UWidgetTree* RowTree = LoadAuthoredTree(*this, RowClass);

	ExpectBoundWidget<UTextBlock>(*this, RowTree, TEXT("HeadlineText"));
	ExpectBoundWidget<UTextBlock>(*this, RowTree, TEXT("MetaText"));
	ExpectBoundWidget<UTextBlock>(*this, RowTree, TEXT("WeightText"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, RowTree, TEXT("FocusButton"));

	if (RowClass)
	{
		const UClass* ParentClass = RowClass->GetSuperClass();
		TestEqual(
			TEXT("The authored row derives from the Territory dossier row widget"),
			ParentClass ? ParentClass->GetName() : FString(),
			FString(UTerritoryDossierRowWidget::StaticClass()->GetName()));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFDossierShellWidgetContract,
	"TerritoryFramework.Dossier.Contract.AuthoredShellWidgetBindsItsTabsSwitcherAndRowClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDossierShellWidgetContract::RunTest(const FString& Parameters)
{
	using namespace TerritoryDossierWidgetAssetTests;

	UWidgetBlueprintGeneratedClass* ShellClass = LoadWidgetClass(*this, ShellClassPath);
	UWidgetTree* ShellTree = LoadAuthoredTree(*this, ShellClass);

	ExpectBoundWidget<UWidgetSwitcher>(*this, ShellTree, TEXT("DossierSwitcher"));
	ExpectBoundWidget<UVerticalBox>(*this, ShellTree, TEXT("HistoryEntries"));
	ExpectBoundWidget<UTextBlock>(*this, ShellTree, TEXT("HistoryEmptyText"));
	ExpectBoundWidget<UVerticalBox>(*this, ShellTree, TEXT("LogsEntries"));
	ExpectBoundWidget<UTextBlock>(*this, ShellTree, TEXT("LogsEmptyText"));
	ExpectBoundWidget<UVerticalBox>(*this, ShellTree, TEXT("StatisticsEntries"));
	ExpectBoundWidget<UTextBlock>(*this, ShellTree, TEXT("StatisticsEmptyText"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_HistoryTab"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_CollectiblesTab"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_RelationshipsTab"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_LogsTab"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_StatisticsTab"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_AchievementsTab"));
	ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellTree, TEXT("Btn_LoreTab"));

	if (ShellClass)
	{
		const UClass* ParentClass = ShellClass->GetSuperClass();
		TestEqual(
			TEXT("The authored shell derives from the Territory dossier widget"),
			ParentClass ? ParentClass->GetName() : FString(),
			FString(UTerritoryDossierWidget::StaticClass()->GetName()));
	}

	// The switcher pages must be ordered exactly like ETerritoryDossierDrawer, because
	// SetDossierDrawer maps the drawer straight onto the switcher index.
	if (UWidgetSwitcher* Switcher = ShellTree
		? Cast<UWidgetSwitcher>(ShellTree->FindWidget(TEXT("DossierSwitcher")))
		: nullptr)
	{
		TestEqual(
			TEXT("The switcher carries one page per dossier drawer"),
			Switcher->GetChildrenCount(),
			static_cast<int32>(ETerritoryDossierDrawer::Lore) + 1);
	}

	// The shell instantiates its rows from this class default, so an empty value would
	// leave the History drawer permanently blank without any compile error.
	UWidgetBlueprintGeneratedClass* RowClass = LoadWidgetClass(*this, RowClassPath);
	const UUserWidget* ShellDefaults = ShellClass ? ShellClass->GetDefaultObject<UUserWidget>() : nullptr;
	TestNotNull(TEXT("The authored shell has a class default object"), ShellDefaults);

	if (ShellDefaults)
	{
		if (const FObjectPropertyBase* RowClassProperty =
			FindFProperty<FObjectPropertyBase>(
				UTerritoryDossierWidget::StaticClass(), TEXT("RecordRowWidgetClass")))
		{
			const UClass* AuthoredRowClass =
				Cast<UClass>(RowClassProperty->GetObjectPropertyValue_InContainer(ShellDefaults));
			TestNotNull(TEXT("The authored shell names a record row widget class"), AuthoredRowClass);
			TestEqual(
				TEXT("The authored shell instantiates the authored row widget class"),
				AuthoredRowClass ? AuthoredRowClass->GetName() : FString(),
				RowClass ? RowClass->GetName() : FString());
		}
		else
		{
			AddError(TEXT("UTerritoryDossierWidget no longer exposes RecordRowWidgetClass."));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
