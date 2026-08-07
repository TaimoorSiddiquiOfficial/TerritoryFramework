#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NarrativeActivatableWidget.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryDistrictManagementWidget.h"
#include "UI/TerritoryDistrictRowWidget.h"
#include "UI/TerritoryHUDWidget.h"
#include "UI/TerritoryJournalWidget.h"
#include "UI/TerritoryUIBlueprintLibrary.h"

namespace TerritoryUITest
{
	bool IsBlueprintPure(const UClass* Class, FName FunctionName)
	{
		const UFunction* Function = Class ? Class->FindFunctionByName(FunctionName) : nullptr;
		return Function
			&& Function->HasAnyFunctionFlags(FUNC_BlueprintCallable)
			&& Function->HasAnyFunctionFlags(FUNC_BlueprintPure);
	}

	bool IsBlueprintCallable(const UClass* Class, FName FunctionName)
	{
		const UFunction* Function = Class ? Class->FindFunctionByName(FunctionName) : nullptr;
		return Function && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUICommonUIContractTest,
	"TerritoryFramework.UI.CommonUIContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUICommonUIContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Territory menu base subclasses Narrative activatable widget"),
		UTerritoryActivatableWidget::StaticClass()->IsChildOf(UNarrativeActivatableWidget::StaticClass()));
	TestTrue(TEXT("District management uses the Territory Narrative menu base"),
		UTerritoryDistrictManagementWidget::StaticClass()->IsChildOf(UTerritoryActivatableWidget::StaticClass()));
	TestTrue(TEXT("Journal uses the Territory Narrative menu base"),
		UTerritoryJournalWidget::StaticClass()->IsChildOf(UTerritoryActivatableWidget::StaticClass()));
	TestTrue(TEXT("Embedded Territory activatables accept focus"),
		GetDefault<UTerritoryActivatableWidget>()->IsFocusable());

	const UClass* LibraryClass = UTerritoryUIBlueprintLibrary::StaticClass();
	TestTrue(TEXT("OpenTerritoryMenu is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("OpenTerritoryMenu")));
	TestTrue(TEXT("District operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildDistrictOperationsView")));
	TestTrue(TEXT("District operations list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetDistrictOperationsViews")));
	TestTrue(TEXT("Per-garrison operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildGarrisonOperationsView")));
	TestTrue(TEXT("District garrison list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetDistrictGarrisonOperationsViews")));
	TestTrue(TEXT("Economy operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildEconomyOperationsView")));
	TestTrue(TEXT("Capture eligibility planning query is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(
			UTerritoryControlSubsystem::StaticClass(), TEXT("GetCaptureEligibility")));
	const UFunction* CounterAttackAlert = UTerritoryHUDWidget::StaticClass()->
		FindFunctionByName(TEXT("OnCounterAttackAlert"));
	TestNotNull(TEXT("Territory HUD exposes a non-modal counterattack styling hook"),
		CounterAttackAlert);
	if (CounterAttackAlert)
	{
		TestTrue(TEXT("Counterattack styling hook is a Blueprint event"),
			CounterAttackAlert->HasAnyFunctionFlags(FUNC_BlueprintEvent));
	}
	const UFunction* CounterHappened = UTerritoryHUDWidget::StaticClass()->
		FindFunctionByName(TEXT("OnCounterHappened"));
	TestNotNull(TEXT("Territory HUD exposes the targeted state-wise counterattack event"),
		CounterHappened);
	if (CounterHappened)
	{
		TestTrue(TEXT("Counterattack state hook is a Blueprint event"),
			CounterHappened->HasAnyFunctionFlags(FUNC_BlueprintEvent));
	}

	const UScriptStruct* ViewStruct = FTerritoryDistrictOperationsView::StaticStruct();
	TestNotNull(TEXT("District operations view is reflected"), ViewStruct);
	if (ViewStruct)
	{
		TestNotNull(TEXT("View exposes exact add-guard failure"),
			ViewStruct->FindPropertyByName(TEXT("AddGuardFailureReason")));
		TestNotNull(TEXT("View exposes exact remove-guard failure"),
			ViewStruct->FindPropertyByName(TEXT("RemoveGuardFailureReason")));
		TestNotNull(TEXT("View exposes finite assault reserve"),
			ViewStruct->FindPropertyByName(TEXT("PendingReserveAttackers")));
		TestNotNull(TEXT("View exposes assault casualties"),
			ViewStruct->FindPropertyByName(TEXT("KilledAttackers")));
		TestNotNull(TEXT("View exposes finance net"),
			ViewStruct->FindPropertyByName(TEXT("NetIncome")));
		TestNotNull(TEXT("District view exposes child Property garrisons"),
			ViewStruct->FindPropertyByName(TEXT("GarrisonTargets")));
		TestNotNull(TEXT("District view exposes Property hierarchy completion"),
			ViewStruct->FindPropertyByName(TEXT("OwnedProperties")));
		TestNotNull(TEXT("District view distinguishes a projected threat from a scheduled assault"),
			ViewStruct->FindPropertyByName(TEXT("bThreatPreviewAvailable")));
		TestNotNull(TEXT("District view exposes the leaf Territory targeted by a cascaded assault"),
			ViewStruct->FindPropertyByName(TEXT("ThreatTargetTerritory")));
		TestNotNull(TEXT("District view exposes deterministic strategic priority"),
			ViewStruct->FindPropertyByName(TEXT("AttackPriority")));
	}
	const UScriptStruct* GarrisonStruct = FTerritoryGarrisonOperationsView::StaticStruct();
	TestNotNull(TEXT("Garrison operations view is reflected"), GarrisonStruct);
	if (GarrisonStruct)
	{
		TestNotNull(TEXT("Garrison view exposes absolute desired target"),
			GarrisonStruct->FindPropertyByName(TEXT("DesiredGuards")));
		TestNotNull(TEXT("Garrison view exposes pending reserve deployments"),
			GarrisonStruct->FindPropertyByName(TEXT("PendingDeployments")));
		TestNotNull(TEXT("Garrison view separates recruitment price"),
			GarrisonStruct->FindPropertyByName(TEXT("RecruitmentCostPerGuard")));
		TestNotNull(TEXT("Garrison view exposes local profit and loss"),
			GarrisonStruct->FindPropertyByName(TEXT("NetIncome")));
	}
	const UClass* ManagementClass = UTerritoryPlayerManagementComponent::StaticClass();
	const FProperty* CounterEvent = ManagementClass->FindPropertyByName(TEXT("OnCounterHappened"));
	TestNotNull(TEXT("Owned bridge exposes the state-wise counterattack delegate"), CounterEvent);
	if (CounterEvent)
	{
		TestTrue(TEXT("Owning-client counterattack delegate is Blueprint assignable"),
			CounterEvent->HasAnyPropertyFlags(CPF_BlueprintAssignable));
	}
	TestTrue(TEXT("Owned bridge exposes remote absolute target RPC request"),
		TerritoryUITest::IsBlueprintCallable(ManagementClass, TEXT("RequestSetGuardTargetForTerritory")));
	TestTrue(TEXT("Owned bridge exposes management-point absolute target request"),
		TerritoryUITest::IsBlueprintCallable(ManagementClass, TEXT("RequestSetGuardTarget")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIOperationsFilterTest,
	"TerritoryFramework.UI.OperationsFilters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIOperationsFilterTest::RunTest(const FString& Parameters)
{
	FTerritoryDistrictOperationsView View;
	View.bRegistered = true;
	View.bUnlocked = true;
	View.bAvailable = true;
	View.bOwnedByViewer = true;
	View.bManageable = true;

	TestTrue(TEXT("Registered district appears in All"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::All));
	TestTrue(TEXT("Unlocked filter uses unlocked state"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::Unlocked));
	TestTrue(TEXT("Available filter uses viewer-relative availability"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::Available));
	TestTrue(TEXT("Owned filter uses viewer ownership"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::Owned));
	TestTrue(TEXT("Manageable filter uses server-compatible management eligibility"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::Manageable));
	TestFalse(TEXT("Secure district does not appear under attack"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::UnderAttack));

	View.bAttackScheduled = true;
	TestTrue(TEXT("Scheduled warning appears under attack"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::UnderAttack));
	View.bAttackScheduled = false;
	View.bThreatPreviewAvailable = true;
	TestTrue(TEXT("Projected eligible threat appears in the threat filter without claiming it is scheduled"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::UnderAttack));
	View.bThreatPreviewAvailable = false;
	View.bCaptureInProgress = true;
	TestTrue(TEXT("Capture pressure appears as contested"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::Contested));
	View.bUnlocked = false;
	TestTrue(TEXT("Locked registered District remains visible in the complete directory"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::All));
	TestTrue(TEXT("Locked district appears in locked filter"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::Locked));
	View.bFinancialRisk = true;
	TestTrue(TEXT("Guard or operating risk appears in financial filter"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, ETerritoryOperationsFilter::FinancialRisk));

	View = FTerritoryDistrictOperationsView();
	View.bRegistered = true;
	View.bUnlocked = true;
	TestFalse(TEXT("Unlocked alone is not enough for the available/unlocked command list"),
		UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View));
	View.bAvailable = true;
	TestTrue(TEXT("A registered unlocked actionable district appears in the available list"),
		UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View));
	View.bOwnedByViewer = true;
	TestFalse(TEXT("Owned districts never duplicate into the available list"),
		UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View));
	View.TerritoryState = ETerritoryState::Claimed;
	TestTrue(TEXT("A registered viewer-owned claimed district appears in captured/owned"),
		UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(View));
	View.TerritoryState = ETerritoryState::Unclaimed;
	TestFalse(TEXT("An unclaimed district cannot appear as captured/owned"),
		UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(View));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIRevisionRegressionTest,
	"TerritoryFramework.UI.LiveRevisionRegression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIRevisionRegressionTest::RunTest(const FString& Parameters)
{
	FTerritoryDistrictOperationsView View;
	View.bRegistered = true;
	const int32 Baseline = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);

	View.ActiveGuards = 1;
	const int32 GuardRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Guard change invalidates the journal row"), Baseline, GuardRevision);

	View.AliveAttackers = 3;
	const int32 AssaultRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Assault force change invalidates the journal row"), GuardRevision, AssaultRevision);

	View.AvailableFunds = 500;
	const int32 FundsRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Narrative account change invalidates finance controls"), AssaultRevision, FundsRevision);

	View.LockReason = FText::FromString(TEXT("Quest gate"));
	const int32 LockRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Lock reason change invalidates availability text"), FundsRevision, LockRevision);

	View.bThreatPreviewAvailable = true;
	View.LaunchProbability = 1.f;
	View.ThreatTargetTerritory = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Territory.District.MarketSquare.Blacksmith")), false);
	const int32 PreviewRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Projected counterattack change invalidates the Journal row"),
		LockRevision, PreviewRevision);

	FTerritoryGarrisonOperationsView Garrison;
	Garrison.TerritoryTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Territory.District.MarketSquare.Blacksmith")), false);
	View.GarrisonTargets.Add(Garrison);
	const int32 GarrisonRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	Garrison.PendingDeployments = 1;
	View.GarrisonTargets[0] = Garrison;
	const int32 PendingRevision = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View);
	TestNotEqual(TEXT("Pending reserve change invalidates the command-center read model"),
		GarrisonRevision, PendingRevision);
	return true;
}

#endif
