// Copyright (c) TerritoryFramework. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/TerritoryDossierRowWidget.h"
#include "UI/TerritoryDossierWidget.h"
#include "WidgetBlueprint.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Blueprint/WidgetTree.h"

/**
 * Verifies the authored Dossier widgets match the C++ binding contract.
 *
 * The Dossier screen binds its widgets with BindWidgetOptional, so a name or a class
 * mismatch stays silent at runtime: the widget compiles, the binding simply never
 * resolves. These tests read the authored trees back and pin the names/classes plus the
 * row class default the shell needs, so the silence can never happen unnoticed.
 */
namespace TerritoryDossierWidgetAssetTests
{
	static const TCHAR* ShellAssetPath = TEXT("/Game/TerritoryFramework/UI/WBP_HopDossier.WBP_HopDossier");
	static const TCHAR* RowAssetPath = TEXT("/Game/TerritoryFramework/UI/WBP_DossierRecordRow.WBP_DossierRecordRow");

	UWidgetBlueprint* LoadWidgetBlueprint(FAutomationTestBase& Test, const TCHAR* AssetPath)
	{
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, AssetPath);
		Test.TestNotNull(FString::Printf(TEXT("The widget blueprint %s exists and compiles"), AssetPath), Blueprint);
		return Blueprint;
	}

	template <typename WidgetType>
	void ExpectBoundWidget(FAutomationTestBase& Test, UWidgetBlueprint* Blueprint, const TCHAR* WidgetName)
	{
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			Test.AddError(FString::Printf(TEXT("Cannot verify '%s': the blueprint or its widget tree is missing."), WidgetName));
			return;
		}

		UWidget* Found = Blueprint->WidgetTree->FindWidget(FName(WidgetName));
		Test.TestNotNull(
			FString::Printf(TEXT("The authored tree exposes a widget named '%s' for the C++ binding"), WidgetName),
			Found);

		if (Found)
		{
			const bool bExpectedClass = Found->IsA<WidgetType>();
			Test.TestTrue(
				FString::Printf(
					TEXT("'%s' is a %s so the binding resolves"),
					WidgetName,
					*WidgetType::StaticClass()->GetName()),
				bExpectedClass);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFDossierRowWidgetContract,
	"TerritoryFramework.Dossier.Contract.AuthoredRowWidgetBindsItsTextsAndButton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDossierRowWidgetContract::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* RowBlueprint =
		TerritoryDossierWidgetAssetTests::LoadWidgetBlueprint(*this, TerritoryDossierWidgetAssetTests::RowAssetPath);
	if (!RowBlueprint)
	{
		return false;
	}

	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UTextBlock>(*this, RowBlueprint, TEXT("HeadlineText"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UTextBlock>(*this, RowBlueprint, TEXT("MetaText"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UTextBlock>(*this, RowBlueprint, TEXT("WeightText"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, RowBlueprint, TEXT("FocusButton"));

	const UClass* ParentClass = RowBlueprint->ParentClass;
	TestEqual(
		TEXT("The authored row reports the Territory dossier row as its parent"),
		ParentClass ? ParentClass->GetName() : FString(),
		FString(UTerritoryDossierRowWidget::StaticClass()->GetName()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFDossierShellWidgetContract,
	"TerritoryFramework.Dossier.Contract.AuthoredShellWidgetBindsItsTabsSwitcherAndRowClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDossierShellWidgetContract::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* ShellBlueprint =
		TerritoryDossierWidgetAssetTests::LoadWidgetBlueprint(*this, TerritoryDossierWidgetAssetTests::ShellAssetPath);
	if (!ShellBlueprint)
	{
		return false;
	}

	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UWidgetSwitcher>(*this, ShellBlueprint, TEXT("DossierSwitcher"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UVerticalBox>(*this, ShellBlueprint, TEXT("HistoryEntries"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UTextBlock>(*this, ShellBlueprint, TEXT("HistoryEmptyText"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellBlueprint, TEXT("Btn_HistoryTab"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellBlueprint, TEXT("Btn_NotesTab"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellBlueprint, TEXT("Btn_CollectiblesTab"));
	TerritoryDossierWidgetAssetTests::ExpectBoundWidget<UNarrativeCommonButtonBase>(*this, ShellBlueprint, TEXT("Btn_RelationshipsTab"));

	const UClass* ParentClass = ShellBlueprint->ParentClass;
	TestEqual(
		TEXT("The authored shell reports the Territory dossier widget as its parent"),
		ParentClass ? ParentClass->GetName() : FString(),
		FString(UTerritoryDossierWidget::StaticClass()->GetName()));

	// The switcher pages must be ordered exactly like ETerritoryDossierDrawer, because
	// SetDossierDrawer maps the drawer straight onto the switcher index.
	if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(ShellBlueprint->WidgetTree->FindWidget(TEXT("DossierSwitcher"))))
	{
		TestEqual(
			TEXT("The switcher carries one page per dossier drawer"),
			Switcher->GetNumWidgets(),
			static_cast<int32>(ETerritoryDossierDrawer::Relationships) + 1);

		TestNotNull(TEXT("The first switcher page is the History drawer"), Switcher->GetChildAt(0));
	}

	// The shell instantiates its rows from this class default, so an empty value would
	// leave the History drawer permanently blank without any compile error.
	UWidgetBlueprint* RowBlueprint =
		TerritoryDossierWidgetAssetTests::LoadWidgetBlueprint(*this, TerritoryDossierWidgetAssetTests::RowAssetPath);
	const UClass* ExpectedRowClass = RowBlueprint ? RowBlueprint->GeneratedClass : nullptr;

	if (const UUserWidget* ShellDefaults = ShellBlueprint->GeneratedClass
		? ShellBlueprint->GeneratedClass->GetDefaultObject<UUserWidget>()
		: nullptr)
	{
		if (const FObjectPropertyBase* RowClassProperty =
			FindFProperty<FObjectPropertyBase>(UTerritoryDossierWidget::StaticClass(), TEXT("RecordRowWidgetClass")))
		{
			const UClass* AuthoredRowClass =
				Cast<UClass>(RowClassProperty->GetObjectPropertyValue_InContainer(ShellDefaults));
			TestNotNull(TEXT("The authored shell names a record row widget class"), AuthoredRowClass);
			if (ExpectedRowClass)
			{
				TestEqual(
					TEXT("The authored shell instantiates the authored row widget class"),
					AuthoredRowClass ? AuthoredRowClass->GetName() : FString(),
					FString(ExpectedRowClass->GetName()));
			}
		}
		else
		{
			AddError(TEXT("UTerritoryDossierWidget no longer exposes RecordRowWidgetClass."));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
