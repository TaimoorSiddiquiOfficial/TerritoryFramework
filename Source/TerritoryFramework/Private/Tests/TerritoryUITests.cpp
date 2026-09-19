#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "CommonButtonBase.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "ArsenalSettings.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryWorldState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NarrativeActivatableWidget.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/TerritoryMapMarker.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryDistrictManagementWidget.h"
#include "UI/TerritoryDistrictRowWidget.h"
#include "UI/TerritoryHUDWidget.h"
#include "UI/TerritoryJournalWidget.h"
#include "UI/TerritoryLiveEventRowWidget.h"
#include "UI/TerritoryLiveEventTypes.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "UI/TerritoryUITheme.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/StrProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/SoftObjectPath.h"
#include "Internationalization/Text.h"

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
	TestFalse(TEXT("District rows reserve their width for selection details by default"),
		GetDefault<UTerritoryDistrictRowWidget>()->bShowInlineGuardActions);
	TestTrue(TEXT("District rows expose compact accordion control"),
		TerritoryUITest::IsBlueprintCallable(
			UTerritoryDistrictRowWidget::StaticClass(), TEXT("SetExpanded")));
	TestTrue(TEXT("District journal entries expose selected-state control like Quest entries"),
		TerritoryUITest::IsBlueprintCallable(
			UTerritoryDistrictRowWidget::StaticClass(), TEXT("SetSelected")));
	TestTrue(TEXT("Journal exposes one selected-District entry point like Show Quest"),
		TerritoryUITest::IsBlueprintCallable(
			UTerritoryJournalWidget::StaticClass(), TEXT("SelectDistrict")));
	TestTrue(TEXT("Journal exposes Active Territory population diagnostics"),
		TerritoryUITest::IsBlueprintPure(
			UTerritoryJournalWidget::StaticClass(), TEXT("GetActiveTerritoryEntryCount")));
	TestTrue(TEXT("Journal exposes Claimed Territory population diagnostics (legacy function name remains binary compatible)"),
		TerritoryUITest::IsBlueprintPure(
			UTerritoryJournalWidget::StaticClass(), TEXT("GetCapturedTerritoryEntryCount")));
	const FObjectPropertyBase* ActiveTerritoriesProperty = CastField<FObjectPropertyBase>(
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(TEXT("ActiveTerritoriesBox")));
	TestTrue(TEXT("Active Territories uses the Quest Journal ScrollBox template"),
		ActiveTerritoriesProperty
			&& ActiveTerritoriesProperty->PropertyClass->IsChildOf(UScrollBox::StaticClass()));
	const FObjectPropertyBase* CapturedTerritoriesProperty = CastField<FObjectPropertyBase>(
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(TEXT("CapturedTerritoriesBox")));
	TestTrue(TEXT("Claimed Territories uses the Quest Journal ScrollBox template"),
		CapturedTerritoriesProperty
			&& CapturedTerritoriesProperty->PropertyClass->IsChildOf(UScrollBox::StaticClass()));
	TestNotNull(TEXT("Journal exposes the persistent selected Territory information pane"),
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(
			TEXT("SelectedTerritoryInfoBox")));
	TestTrue(TEXT("Embedded Territory activatables accept focus"),
		GetDefault<UTerritoryActivatableWidget>()->IsFocusable());
	TestNotNull(TEXT("Community projects can replace the Command Center background"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("TerritoryScreenBackgroundTexture")));
	TestNotNull(TEXT("Community projects can scale compact Territory typography"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("TerritoryTextScale")));
	TestNotNull(TEXT("Community projects can select a readable Territory interface font"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("TerritoryInterfaceFont")));
	TestNotNull(TEXT("Community projects can recolour the compact HUD capture card"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("TerritoryHUDCardFillColor")));
	TestNotNull(TEXT("Community projects can resize the compact HUD capture card"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("TerritoryHUDCardSize")));
	TestNotNull(TEXT("Community projects can size the counter-attack alert card"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("TerritoryHUDCardAlertExtraHeight")));
	TestNotNull(TEXT("The large panel texture stays off the compact HUD card by default"),
		UTerritoryDeveloperSettings::StaticClass()->FindPropertyByName(
			TEXT("bTerritoryHUDCardUsePanelTexture")));
	TestFalse(TEXT("The compact HUD card does not inherit the large panel texture"),
		GetDefault<UTerritoryDeveloperSettings>()->bTerritoryHUDCardUsePanelTexture);
	TestNotNull(TEXT("Every Definition exposes passive gameplay-HUD visibility"),
		UTerritoryDefinition::StaticClass()->FindPropertyByName(
			TEXT("bShowGameplayHUD")));
	TestTrue(TEXT("Territory actors expose Definition-owned gameplay-HUD visibility"),
		TerritoryUITest::IsBlueprintPure(
			ATerritoryVolume::StaticClass(), TEXT("ShouldShowGameplayHUD")));
	const UScriptStruct* NotificationStruct = FTerritoryNotificationSettings::StaticStruct();
	TestNotNull(TEXT("Money/resource notification policy is reflected"), NotificationStruct);
	if (NotificationStruct)
	{
		TestNotNull(TEXT("Money earning HUD notifications are configurable"),
			NotificationStruct->FindPropertyByName(TEXT("bShowMoneyEarningsOnHUD")));
		TestNotNull(TEXT("Resource earning HUD notifications are configurable"),
			NotificationStruct->FindPropertyByName(TEXT("bShowResourceEarningsOnHUD")));
		TestNotNull(TEXT("Command Center production retention is configurable"),
			NotificationStruct->FindPropertyByName(TEXT("bRecordResourceProduction")));
	}
	const UClass* PlayerManagementClass = UTerritoryPlayerManagementComponent::StaticClass();
	TestTrue(TEXT("Place discovery can be requested from Blueprint"),
		TerritoryUITest::IsBlueprintCallable(
			PlayerManagementClass, TEXT("RefreshTerritoryPOIDiscovery")));
	TestNotNull(TEXT("Place entry discovery is configurable"),
		PlayerManagementClass->FindPropertyByName(TEXT("bDiscoverPlacesOnEnter")));
	TestNotNull(TEXT("Place discovery interval is configurable"),
		PlayerManagementClass->FindPropertyByName(TEXT("PlaceDiscoveryInterval")));

	const UClass* LibraryClass = UTerritoryUIBlueprintLibrary::StaticClass();
	TestTrue(TEXT("OpenTerritoryMenu is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("OpenTerritoryMenu")));
	TestTrue(TEXT("Set Territory waypoint is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("SetTerritoryWaypoint")));
	TestTrue(TEXT("Aggregate waypoint target resolver is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(
			LibraryClass, TEXT("ResolveTerritoryWaypointTarget")));
	TestTrue(TEXT("Clear Territory waypoint is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("ClearTerritoryWaypoint")));
	TestTrue(TEXT("Tracked Territory query is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetTrackedTerritory")));
	TestTrue(TEXT("Aggregate tracked-state query is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(
			LibraryClass, TEXT("IsTerritoryWaypointTracked")));
	TestTrue(TEXT("Player location District query is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetDistrictAtPlayerLocation")));
	TestTrue(TEXT("District operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildDistrictOperationsView")));
	TestTrue(TEXT("District operations list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetDistrictOperationsViews")));
	TestTrue(TEXT("Player-visible District list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetPlayerVisibleDistrictOperationsViews")));
	TestTrue(TEXT("Player-visible spatial Territory resolver is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetVisibleTerritoryAtLocation")));
	TestTrue(TEXT("Hierarchy operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildHierarchyOperationsView")));
	TestTrue(TEXT("Shared status formatter is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetTerritoryStatusText")));
	TestTrue(TEXT("Hierarchy visibility rule is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("IsTerritoryVisibleToPlayer")));
	TestTrue(TEXT("Selected District hierarchy list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetDistrictHierarchyOperationsViews")));
	TestTrue(TEXT("District directory search is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("DoesDistrictMatchSearch")));
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
		TestNotNull(TEXT("District view exposes anonymous locked Place count"),
			ViewStruct->FindPropertyByName(TEXT("HiddenProperties")));
		TestNotNull(TEXT("District view separates discovered Place count"),
			ViewStruct->FindPropertyByName(TEXT("KnownProperties")));
		TestNotNull(TEXT("District view exposes known contestable Place count"),
			ViewStruct->FindPropertyByName(TEXT("ContestableProperties")));
		TestNotNull(TEXT("District view reports when every Place is discovered"),
			ViewStruct->FindPropertyByName(TEXT("bAllPlacesDiscovered")));
		TestNotNull(TEXT("District view exposes its parent City"),
			ViewStruct->FindPropertyByName(TEXT("CityTag")));
		TestNotNull(TEXT("District view exposes effective hierarchy visibility"),
			ViewStruct->FindPropertyByName(TEXT("bHierarchyVisible")));
		TestNotNull(TEXT("District view carries explicit story availability"),
			ViewStruct->FindPropertyByName(TEXT("Availability")));
		TestNotNull(TEXT("District view exposes only player-visible Places"),
			ViewStruct->FindPropertyByName(TEXT("VisiblePlaces")));
		TestNotNull(TEXT("District view exposes diplomacy context"),
			ViewStruct->FindPropertyByName(TEXT("DiplomacySummary")));
		TestNotNull(TEXT("District view distinguishes a projected threat from a scheduled assault"),
			ViewStruct->FindPropertyByName(TEXT("bThreatPreviewAvailable")));
		TestNotNull(TEXT("District view exposes the leaf Territory targeted by a cascaded assault"),
			ViewStruct->FindPropertyByName(TEXT("ThreatTargetTerritory")));
		TestNotNull(TEXT("District view exposes deterministic strategic priority"),
			ViewStruct->FindPropertyByName(TEXT("AttackPriority")));
		TestNotNull(TEXT("District view exposes state-driven command capabilities"),
			ViewStruct->FindPropertyByName(TEXT("CommandCapabilities")));
		TestNotNull(TEXT("District view exposes reinforcement availability"),
			ViewStruct->FindPropertyByName(TEXT("bCanSendReinforcements")));
	}
	const UScriptStruct* CapabilityStruct = FTerritoryCommandCapabilityView::StaticStruct();
	TestNotNull(TEXT("Command capability view is reflected"), CapabilityStruct);
	if (CapabilityStruct)
	{
		TestNotNull(TEXT("Capability view reports active source names without locked-source disclosure"),
			CapabilityStruct->FindPropertyByName(TEXT("ActiveSourceNames")));
		TestNotNull(TEXT("Capability view distinguishes configured gates"),
			CapabilityStruct->FindPropertyByName(TEXT("bConfigured")));
	}
	const UScriptStruct* HierarchyStruct = FTerritoryHierarchyOperationsView::StaticStruct();
	TestNotNull(TEXT("Hierarchy operations view is reflected"), HierarchyStruct);
	if (HierarchyStruct)
	{
		TestNotNull(TEXT("Hierarchy row exposes City/District/Place level"),
			HierarchyStruct->FindPropertyByName(TEXT("HierarchyLevel")));
		TestNotNull(TEXT("Hierarchy row exposes effective player visibility"),
			HierarchyStruct->FindPropertyByName(TEXT("bVisibleToPlayer")));
		TestNotNull(TEXT("Hierarchy row separates availability from political state"),
			HierarchyStruct->FindPropertyByName(TEXT("Availability")));
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
		TestNotNull(TEXT("Garrison view exposes reserve reinforcement eligibility"),
			GarrisonStruct->FindPropertyByName(TEXT("bCanSendReinforcements")));
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
	TestTrue(TEXT("Owned bridge exposes server-validated reinforcement request"),
		TerritoryUITest::IsBlueprintCallable(ManagementClass, TEXT("RequestSendReinforcements")));
	TestTrue(TEXT("Owned bridge exposes the live event feed"),
		TerritoryUITest::IsBlueprintPure(ManagementClass, TEXT("GetLiveEvents")));
	TestTrue(TEXT("Owned bridge exposes the filterable intelligence databank"),
		TerritoryUITest::IsBlueprintPure(ManagementClass, TEXT("GetTerritoryIntelligence")));
	TestTrue(TEXT("Owned bridge exposes the server-authoritative espionage request"),
		TerritoryUITest::IsBlueprintCallable(ManagementClass, TEXT("RequestEspionage")));
	TestTrue(TEXT("Owned bridge exposes the player-hold espionage chance"),
		TerritoryUITest::IsBlueprintPure(ManagementClass, TEXT("GetEspionageSuccessChance")));
	TestTrue(TEXT("Community Blueprints can explain and preview the espionage formula"),
		TerritoryUITest::IsBlueprintPure(ManagementClass,
			TEXT("CalculateEspionageSuccessChance")));
	TestTrue(TEXT("Territory intelligence participates in Narrative Pro component saving"),
		ManagementClass->ImplementsInterface(UNarrativeSavableComponent::StaticClass()));
	const UScriptStruct* IntelligenceStruct = FTerritoryLiveEvent::StaticStruct();
	TestNotNull(TEXT("Intelligence record exposes a category"),
		IntelligenceStruct->FindPropertyByName(TEXT("Category")));
	TestNotNull(TEXT("Intelligence record exposes severity"),
		IntelligenceStruct->FindPropertyByName(TEXT("Severity")));
	TestNotNull(TEXT("Intelligence record exposes command perk impact"),
		IntelligenceStruct->FindPropertyByName(TEXT("CommandCapabilities")));
	TestNotNull(TEXT("Intelligence record exposes recurring income impact"),
		IntelligenceStruct->FindPropertyByName(TEXT("IncomeDelta")));
	TestNotNull(TEXT("Intelligence record exposes recurring upkeep impact"),
		IntelligenceStruct->FindPropertyByName(TEXT("UpkeepDelta")));
	TestNotNull(TEXT("Intelligence record exposes actual currency impact"),
		IntelligenceStruct->FindPropertyByName(TEXT("CurrencyDelta")));
	TestNotNull(TEXT("Intelligence record can link a durable economy or assault record"),
		IntelligenceStruct->FindPropertyByName(TEXT("SourceRecordID")));
	const FProperty* HeadlineProperty = IntelligenceStruct->FindPropertyByName(TEXT("Headline"));
	const FProperty* SequenceProperty = IntelligenceStruct->FindPropertyByName(TEXT("Sequence"));
	TestTrue(TEXT("Intelligence report content is marked for Narrative save serialization"),
		HeadlineProperty && HeadlineProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("Intelligence report ordering is marked for Narrative save serialization"),
		SequenceProperty && SequenceProperty->HasAnyPropertyFlags(CPF_SaveGame));
	const UTerritoryPlayerManagementComponent* ManagementDefaults =
		GetDefault<UTerritoryPlayerManagementComponent>();
	TestEqual(TEXT("Databank retains a useful 200-report default"),
		ManagementDefaults->MaxLiveEventHistory, 200);
	TestTrue(TEXT("Archived reports persist until the bounded history fills by default"),
		ManagementDefaults->ExpiredEventRetentionDuration < 0.f);
	TestEqual(TEXT("Espionage has a safe anti-spam cooldown by default"),
		ManagementDefaults->EspionageCooldown, 30.f);
	TestNotNull(TEXT("Live event row exists for authored Reports presentation"),
		UTerritoryLiveEventRowWidget::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIAuthoredRowCaptionTest,
	"TerritoryFramework.UI.Regression.AuthoredRowClearsButtonCaption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIAuthoredRowCaptionTest::RunTest(const FString& Parameters)
{
	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	UClass* ButtonClass = Settings->DefaultNarrativeButtonClass.LoadSynchronous();
	TestNotNull(TEXT("Authored row uses the configured Narrative button template"), ButtonClass);
	if (!ButtonClass) return false;

	UTerritoryDistrictRowWidget* Row = NewObject<UTerritoryDistrictRowWidget>();
	Row->Initialize();
	TestNotNull(TEXT("Authored row has a widget tree"), Row->WidgetTree.Get());
	if (!Row->WidgetTree) return false;

	// Reproduce a Blueprint-authored tree. A pre-existing root skips BuildNativeLayout,
	// so its selection button must receive the same caption cleanup as native rows.
	UOverlay* Root = Row->WidgetTree->ConstructWidget<UOverlay>();
	Row->WidgetTree->RootWidget = Root;
	UNarrativeCommonButtonBase* SelectButton =
		Row->WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
			ButtonClass, TEXT("SelectDistrictButton"));
	Root->AddChild(SelectButton);
	SelectButton->SetButtonText(FText::FromString(TEXT("Button Text")));
	SelectButton->TakeWidget();
	UTextBlock* Caption = Cast<UTextBlock>(
		SelectButton->GetWidgetFromName(TEXT("ButtonTextBlock")));
	TestNotNull(TEXT("Narrative's template supplies the visible caption"), Caption);
	if (!Caption) return false;
	TestFalse(TEXT("Fixture starts with the unwanted template caption"), Caption->GetText().IsEmpty());

	Row->TakeWidget();
	TestTrue(TEXT("Constructing an authored row clears the overlaid default caption"),
		Caption->GetText().IsEmpty());
	TestTrue(TEXT("The row retains the Narrative button as its focus target"),
		Row->GetEntryFocusTarget() == SelectButton);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIReadableTypographyTest,
	"TerritoryFramework.UI.ReadableTypography",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIReadableTypographyTest::RunTest(const FString& Parameters)
{
	UNarrativeCommonTextBlock* Body = NewObject<UNarrativeCommonTextBlock>();
	TestNotNull(TEXT("Readable body test creates a Narrative CommonText block"), Body);
	if (!Body)
	{
		return false;
	}

	Body->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	TerritoryUITheme::ApplyText(Body, TerritoryTypography::Body,
		FLinearColor::White, ETerritoryTextRole::Body);
	Body->TakeWidget();

	TestEqual(TEXT("Body copy keeps the compact Territory size after Slate synchronization"),
		Body->GetFont().Size, static_cast<float>(TerritoryTypography::Body));
	TestEqual(TEXT("Body copy does not inherit the Narrative template's forced all-caps transform"),
		Body->GetTextTransformPolicy(), ETextTransformPolicy::None);
	TestNotNull(TEXT("Body copy uses the configured readable interface font"),
		Body->GetFont().FontObject.Get());

	TestEqual(TEXT("Screen title follows the shared Territory hierarchy"),
		TerritoryTypography::ScreenTitle, 30);
	TestEqual(TEXT("Panel title follows the shared Territory hierarchy"),
		TerritoryTypography::PanelTitle, 22);
	TestEqual(TEXT("Section title follows the shared Territory hierarchy"),
		TerritoryTypography::SectionTitle, 18);
	TestEqual(TEXT("Card title follows the shared Territory hierarchy"),
		TerritoryTypography::CardTitle, 16);
	TestEqual(TEXT("Metadata follows the shared Territory hierarchy"),
		TerritoryTypography::Metadata, 12);
	TestEqual(TEXT("Caption follows the shared Territory hierarchy"),
		TerritoryTypography::Caption, 11);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIExclusiveTabSelectionTest,
	"TerritoryFramework.UI.ExclusiveTabSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIExclusiveTabSelectionTest::RunTest(const FString& Parameters)
{
	const UTerritoryDeveloperSettings* Settings =
		GetDefault<UTerritoryDeveloperSettings>();
	UClass* ButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
	TestNotNull(TEXT("Tab selection test has a concrete Narrative button class"),
		ButtonClass);
	if (!ButtonClass)
	{
		return false;
	}

	UNarrativeCommonButtonBase* First =
		NewObject<UNarrativeCommonButtonBase>(GetTransientPackage(), ButtonClass);
	UNarrativeCommonButtonBase* Second =
		NewObject<UNarrativeCommonButtonBase>(GetTransientPackage(), ButtonClass);
	TestNotNull(TEXT("First tab is created from the configured concrete class"), First);
	TestNotNull(TEXT("Second tab is created from the configured concrete class"), Second);
	if (!First || !Second)
	{
		return false;
	}

	First->SetIsSelectable(true);
	Second->SetIsSelectable(true);
	TerritoryUITheme::SetTabSelected(First, true);
	TerritoryUITheme::SetTabSelected(Second, false);
	TestTrue(TEXT("The clicked first tab is selected"), First->GetSelected());
	TestFalse(TEXT("The unclicked second tab is not selected"), Second->GetSelected());

	TerritoryUITheme::SetTabSelected(First, false);
	TerritoryUITheme::SetTabSelected(Second, true);
	TestFalse(TEXT("The previously clicked tab clears its selected state"),
		First->GetSelected());
	TestTrue(TEXT("Only the newly clicked tab remains selected"),
		Second->GetSelected());

	UClass* TabStyleClass = Settings
		? Settings->TerritoryTabButtonStyle.LoadSynchronous() : nullptr;
	const UCommonButtonStyle* TabStyle = TabStyleClass
		? Cast<UCommonButtonStyle>(TabStyleClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("Territory tabs have a configured CommonUI style"), TabStyle);
	if (TabStyle)
	{
		FSlateBrush NormalBrush;
		FSlateBrush HoveredBrush;
		FSlateBrush SelectedBrush;
		FSlateBrush SelectedHoveredBrush;
		TabStyle->GetNormalBaseBrush(NormalBrush);
		TabStyle->GetNormalHoveredBrush(HoveredBrush);
		TabStyle->GetSelectedBaseBrush(SelectedBrush);
		TabStyle->GetSelectedHoveredBrush(SelectedHoveredBrush);
		TestFalse(TEXT("An unselected tab has a visible hover treatment"),
			NormalBrush.TintColor.GetSpecifiedColor().Equals(
				HoveredBrush.TintColor.GetSpecifiedColor()));
		TestFalse(TEXT("A selected tab has a visible hover treatment"),
			SelectedBrush.TintColor.GetSpecifiedColor().Equals(
				SelectedHoveredBrush.TintColor.GetSpecifiedColor()));
		TestNotNull(TEXT("Unselected tabs have a normal text style"),
			TabStyle->GetNormalTextStyle());
		TestNotNull(TEXT("Selected tabs have a selected text style"),
			TabStyle->GetSelectedTextStyle());
	}

	UClass* ActionStyleClass = Settings
		? Settings->TerritoryActionButtonStyle.LoadSynchronous() : nullptr;
	const UCommonButtonStyle* ActionStyle = ActionStyleClass
		? Cast<UCommonButtonStyle>(ActionStyleClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("Territory management actions have a configured CommonUI style"),
		ActionStyle);
	if (ActionStyle)
	{
		FSlateBrush NormalActionBrush;
		FSlateBrush HoveredActionBrush;
		ActionStyle->GetNormalBaseBrush(NormalActionBrush);
		ActionStyle->GetNormalHoveredBrush(HoveredActionBrush);
		TestFalse(TEXT("District management actions have a visible hover treatment"),
			NormalActionBrush.TintColor.GetSpecifiedColor().Equals(
				HoveredActionBrush.TintColor.GetSpecifiedColor()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIPlayerFacingNamesTest,
	"TerritoryFramework.UI.PlayerFacingNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIPlayerFacingNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A grace-period assault uses a player-facing status"),
		UTerritoryUIBlueprintLibrary::GetAssaultStateText(
			ETerritoryAssaultState::Grace).ToString(), FString(TEXT("Preparing")));
	TestEqual(TEXT("An unresolved assault explains that its outcome is pending"),
		UTerritoryUIBlueprintLibrary::GetAssaultResolutionText(
			ETerritoryAssaultResolution::None).ToString(), FString(TEXT("Pending")));
	TestEqual(TEXT("Diplomacy history uses a readable action"),
		UTerritoryUIBlueprintLibrary::GetDiplomacyEventTypeText(
			EDiplomacyEventType::DeclaredWar).ToString(),
		FString(TEXT("War declared")));

	const FGameplayTag Capability = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.Capability.GuardStaffing"), false);
	TestTrue(TEXT("The Guard Staffing capability tag exists"), Capability.IsValid());
	if (Capability.IsValid())
	{
		UArsenalSettings* NarrativeSettings = GetMutableDefault<UArsenalSettings>();
		TestNotNull(TEXT("Narrative Pro tag display settings are available"),
			NarrativeSettings);
		if (NarrativeSettings)
		{
			const TMap<FGameplayTag, FText> OriginalNames =
				NarrativeSettings->TagFriendlyDisplayNames;
			NarrativeSettings->TagFriendlyDisplayNames.Add(Capability,
				FText::FromString(TEXT("Garrison Command")));
			TestEqual(TEXT("Territory respects Narrative Pro's authored tag name"),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(
					Capability).ToString(), FString(TEXT("Garrison Command")));
			NarrativeSettings->TagFriendlyDisplayNames = OriginalNames;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIAvailabilityStatusTest,
	"TerritoryFramework.UI.Status.AvailabilityPrecedesPoliticalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIAvailabilityStatusTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A locked contested Place is presented as Locked"),
		UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
			ETerritoryAvailability::Locked, ETerritoryState::Contested).ToString(),
		FString(TEXT("Locked")));
	TestEqual(TEXT("A locked claimed Place is still presented as Locked"),
		UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
			ETerritoryAvailability::Locked, ETerritoryState::Claimed).ToString(),
		FString(TEXT("Locked")));
	TestEqual(TEXT("An unlocked contested Place exposes its political state"),
		UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
			ETerritoryAvailability::Unlocked, ETerritoryState::Contested).ToString(),
		FString(TEXT("Contested")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIThemeTypographyTest,
	"TerritoryFramework.UI.Theme.CompactNarrativeTypography",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIThemeTypographyTest::RunTest(const FString& Parameters)
{
	UNarrativeCommonTextBlock* Text = NewObject<UNarrativeCommonTextBlock>();
	TestNotNull(TEXT("Narrative CommonUI text can be created"), Text);
	if (!Text)
	{
		return false;
	}

	TerritoryUITheme::ApplyText(Text, 11,
		FLinearColor(0.9f, 0.8f, 0.7f, 1.f), ETerritoryTextRole::Body);
	TestEqual(TEXT("Territory responsive size overrides the large Narrative theme default"),
		Text->GetFont().Size, 11.f);
	TestTrue(TEXT("Territory body copy remains readable when a row is narrow"),
		Text->GetAutoWrapText());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIEspionageStrengthTest,
	"TerritoryFramework.UI.EspionageStrength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIEspionageStrengthTest::RunTest(const FString& Parameters)
{
	using Component = UTerritoryPlayerManagementComponent;
	const float NoHold = Component::CalculateEspionageSuccessChance(0, 4, 0, 0);
	const float HalfHold = Component::CalculateEspionageSuccessChance(2, 4, 0, 4);
	const float HalfHoldStaffed =
		Component::CalculateEspionageSuccessChance(2, 4, 4, 4);
	const float FullHoldStaffed =
		Component::CalculateEspionageSuccessChance(4, 4, 4, 4);

	TestEqual(TEXT("A faction without a territorial hold keeps the 15 percent base chance"),
		NoHold, 0.15f);
	TestTrue(TEXT("Controlling more Districts never reduces espionage success"),
		HalfHold >= NoHold);
	TestTrue(TEXT("Staffing more assigned friendly guards never reduces espionage success"),
		HalfHoldStaffed >= HalfHold);
	TestEqual(TEXT("Half control plus fully staffed garrisons gives the documented 65 percent example"),
		HalfHoldStaffed, 0.65f);
	TestEqual(TEXT("Even an overwhelming hold respects the 90 percent uncertainty cap"),
		FullHoldStaffed, 0.90f);
	TestEqual(TEXT("Invalid negative inputs are safely clamped"),
		Component::CalculateEspionageSuccessChance(-3, 4, -8, 6), 0.15f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUILiveEventExpiryTest,
	"TerritoryFramework.UI.LiveEventExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUILiveEventExpiryTest::RunTest(const FString& Parameters)
{
	FTerritoryLiveEvent Event;
	Event.CreatedRealTime = 100.0;
	Event.ActiveDuration = 30.f;
	TestFalse(TEXT("Live event remains active before its exact deadline"),
		Event.IsExpiredAt(129.999));
	TestTrue(TEXT("Live event expires at its exact deadline"),
		Event.IsExpiredAt(130.0));
	Event.ActiveDuration = -1.f;
	TestFalse(TEXT("Negative duration creates a non-expiring authored entry"),
		Event.IsExpiredAt(10000.0));

	Event.SourceRecordID = FGuid::NewGuid();
	Event.Headline = FText::FromString(TEXT("District attacked"));
	Event.Detail = FText::FromString(TEXT("Three attackers remain"));
	Event.Sequence = 7;
	const uint32 StableRevision = Event.GetPresentationRevision();
	Event.EventID = FGuid::NewGuid();
	Event.CreatedRealTime = 500.0;
	TestEqual(TEXT("Transient event identity and query time do not rebuild the notification list"),
		Event.GetPresentationRevision(), StableRevision);
	Event.Headline = FText::FromString(TEXT("District secured"));
	TestNotEqual(TEXT("Visible report text invalidates the notification row"),
		Event.GetPresentationRevision(), StableRevision);
	const uint32 UpdatedTextRevision = Event.GetPresentationRevision();
	Event.bExpired = true;
	TestNotEqual(TEXT("Active-to-archived presentation changes invalidate the notification row"),
		Event.GetPresentationRevision(), UpdatedTextRevision);

	UTerritoryMapMarker* Marker = NewObject<UTerritoryMapMarker>();
	TestNotNull(TEXT("Territory marker can be created without a world"), Marker);
	if (Marker)
	{
		TestFalse(TEXT("Territory marker is not tracked by default"), Marker->IsTracked());
		Marker->SetTracked(true);
		TestFalse(TEXT("An unbound marker cannot leak into Narrative navigation"),
			Marker->IsTracked());
		Marker->SetTracked(false);
		TestFalse(TEXT("Clearing tracking demotes the marker"), Marker->IsTracked());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIPlayerLocationDistrictTest,
	"TerritoryFramework.UI.Regression.PlaceResolvesPlayerLocationDistrict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIPlayerLocationDistrictTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Location query world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	APlayerController* PlayerController = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(), FTransform::Identity, SpawnParams);
	APawn* Pawn = World->SpawnActor<APawn>(
		APawn::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("District created"), District);
	TestNotNull(TEXT("Place created"), Property);
	TestNotNull(TEXT("Player controller created"), PlayerController);
	TestNotNull(TEXT("Player pawn created"), Pawn);
	if (!District || !Property || !PlayerController || !Pawn)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag DistrictTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill"), false);
	const FGameplayTag PropertyTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	UTerritoryDistrictDefinition* DistrictDefinition =
		NewObject<UTerritoryDistrictDefinition>();
	UTerritoryPlaceDefinition* PlaceDefinition =
		NewObject<UTerritoryPlaceDefinition>();
	UTerritoryCityDefinition* CityDefinition =
		NewObject<UTerritoryCityDefinition>();
	TestFalse(TEXT("Broad City Definitions hide the passive gameplay HUD by default"),
		CityDefinition->bShowGameplayHUD);
	TestTrue(TEXT("District Definitions preserve the passive gameplay HUD by default"),
		DistrictDefinition->bShowGameplayHUD);
	TestTrue(TEXT("Place Definitions show the passive gameplay HUD by default"),
		PlaceDefinition->bShowGameplayHUD);
	DistrictDefinition->TerritoryTag = DistrictTag;
	DistrictDefinition->StableTerritoryGUID = FGuid::NewGuid();
	DistrictDefinition->TerritoryActorClass = ATerritoryDistrict::StaticClass();
	PlaceDefinition->TerritoryTag = PropertyTag;
	PlaceDefinition->StableTerritoryGUID = FGuid::NewGuid();
	PlaceDefinition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	DistrictDefinition->Places = {PlaceDefinition};
	DistrictDefinition->RefreshHierarchyLinks();
	TestTrue(TEXT("District Definition applies to the District"),
		DistrictDefinition->ApplyToTerritory(District));
	TestTrue(TEXT("Place Definition applies to the Place"),
		PlaceDefinition->ApplyToTerritory(Property));
	TestTrue(TEXT("A Place actor reads its Definition gameplay-HUD policy"),
		Property->ShouldShowGameplayHUD());
	PlaceDefinition->bShowGameplayHUD = false;
	TestFalse(TEXT("Disabling one Place Definition hides only its passive gameplay HUD"),
		Property->ShouldShowGameplayHUD());
	PlaceDefinition->bShowGameplayHUD = true;
	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Registry created"), Registry);
	if (Registry)
	{
		TestEqual(TEXT("District registers"), Registry->RegisterTerritory(District),
			ETerritoryRegistrationResult::Success);
		TestEqual(TEXT("Place registers"), Registry->RegisterTerritory(Property),
			ETerritoryRegistrationResult::Success);
		Pawn->SetActorLocation(Property->GetTerritoryBounds().GetCenter());
		PlayerController->Possess(Pawn);
		TestTrue(TEXT("A player inside a Place resolves its owning District"),
			UTerritoryUIBlueprintLibrary::GetDistrictAtPlayerLocation(
				World, PlayerController) == District);
		TestTrue(TEXT("An unlocked Place is the most-specific visible HUD Territory"),
			UTerritoryUIBlueprintLibrary::GetVisibleTerritoryAtLocation(
				World, Pawn->GetActorLocation()) == Property);
		Property->LockTerritory(FText::FromString(TEXT("Story secret")));
		TestTrue(TEXT("A locked Place stays unnamed and HUD falls back to its unlocked District"),
			UTerritoryUIBlueprintLibrary::GetVisibleTerritoryAtLocation(
				World, Pawn->GetActorLocation()) == District);
		District->LockTerritory(FText::FromString(TEXT("Story branch secret")));
		TestNull(TEXT("A completely locked hierarchy branch stays silent in the HUD"),
			UTerritoryUIBlueprintLibrary::GetVisibleTerritoryAtLocation(
				World, Pawn->GetActorLocation()));
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIDistrictWaypointResolutionTest,
	"TerritoryFramework.UI.Regression.DistrictWaypointResolvesVisiblePlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIDistrictWaypointResolutionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Waypoint resolver world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryDistrict* District = World->SpawnActor<ATerritoryDistrict>(
		ATerritoryDistrict::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryProperty* Farm = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform(FVector(100.f, 0.f, 0.f)), SpawnParams);
	ATerritoryProperty* Blacksmith = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform(FVector(1000.f, 0.f, 0.f)), SpawnParams);
	APlayerController* PlayerController = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(), FTransform::Identity, SpawnParams);
	APawn* Pawn = World->SpawnActor<APawn>(
		APawn::StaticClass(), FTransform::Identity, SpawnParams);
	if (!District || !Farm || !Blacksmith || !PlayerController || !Pawn)
	{
		AddError(TEXT("Waypoint resolver fixture actors could not be created"));
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag DistrictTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill"), false);
	District->TerritoryTag = DistrictTag;
	District->TerritoryGUID = FGuid::NewGuid();
	Farm->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	Farm->ParentTerritoryTag = DistrictTag;
	Farm->TerritoryGUID = FGuid::NewGuid();
	Blacksmith->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	Blacksmith->ParentTerritoryTag = DistrictTag;
	Blacksmith->TerritoryGUID = FGuid::NewGuid();
	PlayerController->Possess(Pawn);

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Waypoint resolver registry created"), Registry);
	if (Registry)
	{
		TestEqual(TEXT("Waypoint District registers"),
			Registry->RegisterTerritory(District), ETerritoryRegistrationResult::Success);
		TestEqual(TEXT("Near Place registers"),
			Registry->RegisterTerritory(Farm), ETerritoryRegistrationResult::Success);
		TestEqual(TEXT("Far Place registers"),
			Registry->RegisterTerritory(Blacksmith), ETerritoryRegistrationResult::Success);

		Farm->ForceSetTerritoryState(ETerritoryState::Locked);
		TestTrue(TEXT("A District command skips a locked Place and resolves another visible Place"),
			UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
				PlayerController, District) == Blacksmith);

		TestTrue(TEXT("A waypoint becomes visible only after an explicit availability unlock"),
			Farm->TryUnlockWithContext(FTerritoryTransitionContext(), true));
		TestTrue(TEXT("A District command chooses the nearest visible Place"),
			UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
				PlayerController, District) == Farm);
		TestTrue(TEXT("A direct Place command retains its physical POI"),
			UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
				PlayerController, Farm) == Farm);

		District->ForceSetTerritoryState(ETerritoryState::Locked);
		TestNull(TEXT("A locked District hierarchy has no waypoint target"),
			UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
				PlayerController, District));
		TestNull(TEXT("A Place below a locked District is also silent"),
			UTerritoryUIBlueprintLibrary::ResolveTerritoryWaypointTarget(
				PlayerController, Farm));
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUILockedMarkerSilenceTest,
	"TerritoryFramework.UI.Regression.LockedMarkerLeavesNarrativeDomains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUILockedMarkerSilenceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Marker policy world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Place)
	{
		AddError(TEXT("Marker policy Place could not be created"));
		World->DestroyWorld(false);
		return false;
	}
	Place->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	Place->TerritoryGUID = FGuid::NewGuid();
	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Marker policy registry created"), Registry);
	if (!Registry
		|| Registry->RegisterTerritory(Place) != ETerritoryRegistrationResult::Success)
	{
		AddError(TEXT("Marker policy Place could not be registered"));
		World->DestroyWorld(false);
		return false;
	}

	UTerritoryMapMarker* Marker = NewObject<UTerritoryMapMarker>(World);
	Marker->SetTerritoryVolume(Place);
	const FStructProperty* DomainProperty = FindFProperty<FStructProperty>(
		UMapMarker::StaticClass(), TEXT("MarkerDomain"));
	TestNotNull(TEXT("Narrative marker domain remains inspectable"), DomainProperty);
	auto GetDomains = [DomainProperty, Marker]() -> const FGameplayTagContainer*
	{
		return DomainProperty
			? DomainProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(Marker)
			: nullptr;
	};
	const FGameplayTagContainer* Domains = GetDomains();
	TestTrue(TEXT("An unlocked Place is available on Narrative map surfaces"),
		Domains && Domains->HasTagExact(
			FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap));
	TestFalse(TEXT("An untracked Place stays off the compass"),
		Domains && Domains->HasTagExact(
			FNavigatorGameplayTags::Get().NavigatorTypes_Compass));

	Marker->SetTracked(true);
	Domains = GetDomains();
	TestTrue(TEXT("Tracking promotes only the Place to the compass"),
		Domains && Domains->HasTagExact(
			FNavigatorGameplayTags::Get().NavigatorTypes_Compass));
	TestTrue(TEXT("Tracking promotes the Place to screen-space guidance"),
		Domains && Domains->HasTagExact(
			FNavigatorGameplayTags::Get().NavigatorTypes_Screenspace));

	Place->ForceSetTerritoryState(ETerritoryState::Locked);
	Domains = GetDomains();
	TestFalse(TEXT("Locking automatically clears tracked state"), Marker->IsTracked());
	TestTrue(TEXT("A locked Place has no Narrative navigation domain"),
		Domains && Domains->IsEmpty());

	Marker->ClearTerritoryBinding();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIPlaceNarrativePOIBridgeTest,
	"TerritoryFramework.UI.Regression.PlaceRegistersNarrativePOIData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIPlaceNarrativePOIBridgeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Narrative POI bridge world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	APlayerController* PlayerController = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryProperty* Place = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	UNarrativeNavigationComponent* NavigationComponent = PlayerController
		? NewObject<UNarrativeNavigationComponent>(PlayerController) : nullptr;
	if (!PlayerController || !Place || !NavigationComponent)
	{
		AddError(TEXT("Narrative POI bridge fixture could not be created"));
		World->DestroyWorld(false);
		return false;
	}
	PlayerController->AddInstanceComponent(NavigationComponent);
	NavigationComponent->RegisterComponent();

	const FGameplayTag PlaceTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	Place->TerritoryTag = PlaceTag;
	Place->TerritoryGUID = FGuid::NewGuid();
	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	TestNotNull(TEXT("Narrative POI bridge registry created"), Registry);
	if (!Registry
		|| Registry->RegisterTerritory(Place) != ETerritoryRegistrationResult::Success)
	{
		AddError(TEXT("Narrative POI bridge Place could not be registered"));
		World->DestroyWorld(false);
		return false;
	}

	UTerritoryMapMarker* Marker = NewObject<UTerritoryMapMarker>(World);
	Marker->SetTerritoryVolume(Place);
	// The isolated automation world has no ULocalPlayer, so drive the same callback
	// Narrative invokes after adding a marker to a local navigation component.
	Marker->OnMarkerAdded(NavigationComponent);
	TestTrue(TEXT("A streamed Territory Place contributes Narrative POI lookup data"),
		NavigationComponent->POILookupMap.Contains(PlaceTag));

	Marker->SetTracked(true);
	TestTrue(TEXT("Domain re-registration preserves the Narrative POI lookup"),
		NavigationComponent->POILookupMap.Contains(PlaceTag));

	Marker->OnMarkerRemoved(NavigationComponent);
	Marker->ClearTerritoryBinding();
	TestFalse(TEXT("A streamed-out dynamic Place removes only its contributed POI data"),
		NavigationComponent->POILookupMap.Contains(PlaceTag));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIPlaceFirstDiscoveryTest,
	"TerritoryFramework.UI.Regression.PlaceEntryUsesNarrativeFirstDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIPlaceFirstDiscoveryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Place discovery world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	APlayerController* PlayerController = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(), FTransform::Identity, SpawnParams);
	APawn* Pawn = World->SpawnActor<APawn>(
		APawn::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryProperty* VisiblePlace = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	ATerritoryProperty* LockedPlace = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform(FVector(2000.f, 0.f, 0.f)), SpawnParams);
	UNarrativeNavigationComponent* NavigationComponent = PlayerController
		? NewObject<UNarrativeNavigationComponent>(PlayerController) : nullptr;
	UTerritoryPlayerManagementComponent* ManagementComponent = PlayerController
		? NewObject<UTerritoryPlayerManagementComponent>(PlayerController) : nullptr;
	if (!PlayerController || !Pawn || !VisiblePlace || !LockedPlace
		|| !NavigationComponent || !ManagementComponent)
	{
		AddError(TEXT("Place discovery fixture could not be created"));
		World->DestroyWorld(false);
		return false;
	}
	PlayerController->Possess(Pawn);
	PlayerController->AddInstanceComponent(NavigationComponent);
	NavigationComponent->RegisterComponent();

	const FGameplayTag VisibleTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	const FGameplayTag LockedTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	VisiblePlace->TerritoryTag = VisibleTag;
	VisiblePlace->TerritoryGUID = FGuid::NewGuid();
	LockedPlace->TerritoryTag = LockedTag;
	LockedPlace->TerritoryGUID = FGuid::NewGuid();
	LockedPlace->ForceSetTerritoryState(ETerritoryState::Locked);

	UTerritoryRegistrySubsystem* Registry =
		World->GetSubsystem<UTerritoryRegistrySubsystem>();
	if (!Registry
		|| Registry->RegisterTerritory(VisiblePlace) != ETerritoryRegistrationResult::Success
		|| Registry->RegisterTerritory(LockedPlace) != ETerritoryRegistrationResult::Success)
	{
		AddError(TEXT("Place discovery Territories could not be registered"));
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Entering a visible Place forwards first discovery to Narrative"),
		ManagementComponent->RefreshTerritoryPOIDiscovery());
	TestTrue(TEXT("Narrative remembers the discovered Place"),
		NavigationComponent->HasDiscoveredPOI(VisibleTag));
	TestFalse(TEXT("A repeated check does not rediscover the same Place"),
		ManagementComponent->RefreshTerritoryPOIDiscovery());

	Pawn->SetActorLocation(LockedPlace->GetActorLocation());
	TestFalse(TEXT("A story-locked Place remains silent on entry"),
		ManagementComponent->RefreshTerritoryPOIDiscovery());
	TestFalse(TEXT("A locked Place is not added to Narrative discovery state"),
		NavigationComponent->HasDiscoveredPOI(LockedTag));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIDistrictSearchTest,
	"TerritoryFramework.UI.DistrictSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIDistrictSearchTest::RunTest(const FString& Parameters)
{
	FTerritoryDistrictOperationsView View;
	View.DisplayName = FText::FromString(TEXT("Castle Hill"));
	View.DistrictTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Territory.HavenReach.CastleHill")), false);
	View.OwnerFaction = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Narrative.Factions.Heroes")), false);
	View.TerritoryState = ETerritoryState::Claimed;
	View.AvailabilityReason = FText::FromString(TEXT("Owned by your faction"));
	View.ThreatSummary = FText::FromString(TEXT("Bandit assault warning"));
	FTerritoryGarrisonOperationsView Garrison;
	Garrison.DisplayName = FText::FromString(TEXT("North Gate"));
	Garrison.TerritoryTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Territory.HavenReach.CastleHill.NorthGate")), false);
	View.GarrisonTargets.Add(Garrison);

	TestTrue(TEXT("Empty search preserves every district"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("   ")));
	TestTrue(TEXT("Display-name search is case insensitive"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("castle")));
	TestTrue(TEXT("Stable Territory tag is searchable"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("HavenReach")));
	TestTrue(TEXT("Owner and state tokens can be combined"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("Heroes Claimed")));
	TestTrue(TEXT("Threat text is searchable"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("bandit warning")));
	TestTrue(TEXT("Child garrison names are searchable"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("north gate")));
	TestFalse(TEXT("Every token must match some indexed field"),
		UTerritoryUIBlueprintLibrary::DoesDistrictMatchSearch(View, TEXT("castle neutral")));
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
	View.bHierarchyVisible = true;
	TestTrue(TEXT("Unlocked visible districts stay listed when a quest or diplomacy gate temporarily blocks capture"),
		UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View));
	View.bAvailable = true;
	TestTrue(TEXT("An actionable unlocked district remains in the operations list"),
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

namespace TerritoryUIRevisionCoverage
{
	/**
	 * The fields the revision deliberately ignores, as a hard allow-list.
	 * These are the loaded-actor handles: they are not displayed content, and their
	 * addresses change on streaming and GC in ways that would force needless rebuilds.
	 * Everything a widget can render about load state is already covered by `bRegistered`,
	 * `bRuntimeLoaded` and the stable tags, so nothing is lost by excluding them.
	 * The test asserts this set is exactly what reflection finds, so a NEW handle has to be
	 * argued for here rather than silently escaping coverage.
	 */
	const TSet<FString>& ExpectedHandleNames()
	{
		static const TSet<FString> Expected = {
			TEXT("District"), TEXT("City"), TEXT("Territory") };
		return Expected;
	}

	/** True for a plain instance handle (FObjectProperty but not TSubclassOf, which IS hashed). */
	bool IsHandleProperty(const FProperty* Prop)
	{
		return Prop->IsA<FObjectProperty>() && !Prop->IsA<FClassProperty>();
	}

	/**
	 * A struct-typed property cannot be "changed" as a whole, so it is not probeable directly.
	 * Its members are probed individually by the recursion instead, which is stronger anyway.
	 */
	bool IsProbeableLeaf(const FProperty* Prop)
	{
		return !Prop->IsA<FStructProperty>();
	}

	/** Give every struct array one default element so nested rows are reachable. */
	void PopulateArrays(void* Container, const UStruct* Struct, int32 Depth)
	{
		if (Depth > 6) return;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (FStructProperty* S = CastField<FStructProperty>(Prop))
			{
				PopulateArrays(S->ContainerPtrToValuePtr<void>(Container), S->Struct, Depth + 1);
				continue;
			}
			FArrayProperty* Array = CastField<FArrayProperty>(Prop);
			if (!Array) continue;
			FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Container));
			if (Helper.Num() == 0) Helper.AddValue();
			if (FStructProperty* ElementStruct = CastField<FStructProperty>(Array->Inner))
			{
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					PopulateArrays(Helper.GetRawPtr(Index), ElementStruct->Struct, Depth + 1);
				}
			}
		}
	}

	/**
	 * Set one property to a different value. False means the test cannot probe this type,
	 * which is a test failure rather than a pass: an unprobeable field might be unhashed.
	 * Every accessor below takes a value pointer and is defined by the engine property class
	 * it is called on, so no pointer arithmetic is re-derived here.
	 */
	bool PerturbProperty(void* ValuePtr, FProperty* Prop)
	{
		if (FBoolProperty* Bool = CastField<FBoolProperty>(Prop))
		{
			Bool->SetPropertyValue(ValuePtr, !Bool->GetPropertyValue(ValuePtr));
			return true;
		}
		// FEnumProperty stores the underlying number in place, so its own value pointer is
		// exactly what the underlying numeric accessors expect.
		if (FEnumProperty* Enum = CastField<FEnumProperty>(Prop))
		{
			FNumericProperty* Underlying = Enum->GetUnderlyingProperty();
			if (!Underlying) return false;
			Underlying->SetIntPropertyValue(ValuePtr, Underlying->GetSignedIntPropertyValue(ValuePtr) + 1);
			return true;
		}
		// FNumericProperty covers int32/int64/uint32/uint64/float/double/uint8 in one place.
		if (FNumericProperty* Numeric = CastField<FNumericProperty>(Prop))
		{
			if (Numeric->IsFloatingPoint())
			{
				Numeric->SetFloatingPointPropertyValue(ValuePtr,
					Numeric->GetFloatingPointPropertyValue(ValuePtr) + 1.0);
			}
			else
			{
				Numeric->SetIntPropertyValue(ValuePtr, Numeric->GetSignedIntPropertyValue(ValuePtr) + 1);
			}
			return true;
		}
		if (FNameProperty* Name = CastField<FNameProperty>(Prop))
		{
			Name->SetPropertyValue(ValuePtr, FName(TEXT("RevisionProbe")));
			return true;
		}
		if (FStrProperty* Str = CastField<FStrProperty>(Prop))
		{
			Str->SetPropertyValue(ValuePtr, TEXT("RevisionProbe"));
			return true;
		}
		if (FTextProperty* Text = CastField<FTextProperty>(Prop))
		{
			Text->SetPropertyValue(ValuePtr, FText::FromString(TEXT("RevisionProbe")));
			return true;
		}
		if (FClassProperty* Class = CastField<FClassProperty>(Prop))
		{
			// MetaClass is itself a valid TSubclassOf of itself, so this needs no asset.
			if (UClass* Meta = Class->MetaClass)
			{
				Class->SetObjectPropertyValue(ValuePtr, Meta);
				return true;
			}
			return false;
		}
		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Prop))
		{
			// FSoftObjectProperty stores an FSoftObjectPtr in place; SetObjectPropertyValue
			// would try to resolve it to a live UObject, so write the soft pointer directly.
			FSoftObjectPtr* SoftPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
			*SoftPtr = FSoftObjectPtr(FSoftObjectPath(TEXT("/Engine/EngineMeshes/Cube.Cube")));
			return true;
		}
		// Only reachable as an array's element type; the array itself is probed by count.
		if (FArrayProperty* Array = CastField<FArrayProperty>(Prop))
		{
			FScriptArrayHelper Helper(Array, ValuePtr);
			Helper.AddValue();
			return true;
		}
		return false;
	}

	/** Walk a property chain, taking element 0 whenever it steps through an array. */
	bool PerturbChain(FTerritoryDistrictOperationsView& View, const TArray<FProperty*>& Chain)
	{
		void* Container = &View;
		for (int32 Step = 0; Step < Chain.Num(); ++Step)
		{
			FProperty* Prop = Chain[Step];
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Container);
			if (Step == Chain.Num() - 1) return PerturbProperty(ValuePtr, Prop);
			if (FStructProperty* S = CastField<FStructProperty>(Prop))
			{
				Container = ValuePtr;
				continue;
			}
			if (FArrayProperty* A = CastField<FArrayProperty>(Prop))
			{
				FScriptArrayHelper Helper(A, ValuePtr);
				if (Helper.Num() == 0) Helper.AddValue();
				Container = Helper.GetRawPtr(0);
				continue;
			}
			return false;
		}
		return false;
	}

	FString ChainPath(const TArray<FProperty*>& Chain)
	{
		FString Path;
		for (const FProperty* Prop : Chain)
		{
			Path += TEXT(".");
			Path += Prop->GetName();
		}
		return Path;
	}

	/** Collect one probe per hashed field, at every reachable depth. */
	void CollectProbes(const UStruct* Struct, const TArray<FProperty*>& Prefix, int32 Depth,
		TArray<TArray<FProperty*>>& Out, TSet<FString>& SeenHandles)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			if (IsHandleProperty(Prop))
			{
				// Recorded rather than ignored, so the test can prove the exclusion list is
				// exactly the set of handles that actually exist.
				SeenHandles.Add(Prop->GetName());
				continue;
			}

			TArray<FProperty*> Chain = Prefix;
			Chain.Add(Prop);
			if (IsProbeableLeaf(Prop)) Out.Add(Chain);

			if (Depth > 5) continue;
			if (FStructProperty* S = CastField<FStructProperty>(Prop))
			{
				CollectProbes(S->Struct, Chain, Depth + 1, Out, SeenHandles);
			}
			else if (FArrayProperty* A = CastField<FArrayProperty>(Prop))
			{
				// Probing the array itself only proves the element COUNT is hashed, so probe
				// one element's value too — otherwise a hash that folded in only `Num()`
				// would pass. Struct elements are excluded here because their members are
				// reached by the recursion below.
				if (FStructProperty* ElementStruct = CastField<FStructProperty>(A->Inner))
				{
					CollectProbes(ElementStruct->Struct, Chain, Depth + 1, Out, SeenHandles);
				}
				else if (!IsHandleProperty(A->Inner))
				{
					TArray<FProperty*> ElementChain = Chain;
					ElementChain.Add(A->Inner);
					Out.Add(ElementChain);
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIRevisionCoverageRegressionTest,
	"TerritoryFramework.UI.Regression.EveryDisplayedFieldInvalidatesRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIRevisionCoverageRegressionTest::RunTest(const FString& Parameters)
{
	using namespace TerritoryUIRevisionCoverage;

	// The revision exists to decide whether the Command Center must rebuild. A field that is
	// displayed but missing from it means the panel keeps showing a stale value — which is
	// exactly what happened before: PlannedAttackers, AssaultResolution, ThreatSummary, all
	// four failure-reason texts, the four guard-strength floats, and both `Hierarchy` and
	// `ResourceFlows` were omitted while the widget happily rendered them.
	//
	// Instead of spot-checking a few fields, this walks the struct by reflection and asserts
	// that changing ANY field moves the revision. A field added later is covered for free,
	// and a field the test cannot probe fails loudly rather than passing quietly.

	FTerritoryDistrictOperationsView Populated;
	PopulateArrays(&Populated, FTerritoryDistrictOperationsView::StaticStruct(), 0);
	const int32 Baseline = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(Populated);
	TestTrue(TEXT("The revision is non-negative so a `> 0` Blueprint test is safe"), Baseline >= 0);

	TArray<TArray<FProperty*>> Probes;
	TSet<FString> SeenHandles;
	CollectProbes(FTerritoryDistrictOperationsView::StaticStruct(), {}, 0, Probes, SeenHandles);

	// The excluded handles must be exactly the ones that exist, or a newly added handle would
	// quietly drop out of coverage.
	const TSet<FString>& ExpectedHandles = ExpectedHandleNames();
	TestEqual(TEXT("No undocumented handle pointer is excluded from the revision"),
		SeenHandles.Num(), ExpectedHandles.Num());
	for (const FString& Name : ExpectedHandles)
	{
		TestTrue(FString::Printf(TEXT("%s is still a documented revision exclusion"), *Name),
			SeenHandles.Contains(Name));
	}

	TestTrue(TEXT("Reflection found fields to probe"), Probes.Num() > 40);

	int32 Unprobeable = 0;
	for (const TArray<FProperty*>& Chain : Probes)
	{
		FTerritoryDistrictOperationsView Probe = Populated;
		if (!PerturbChain(Probe, Chain))
		{
			++Unprobeable;
			AddError(FString::Printf(
				TEXT("Revision coverage: cannot probe %s (%s). Either hash it, or add it to the "
					 "documented exclusion list with a reason."),
				*ChainPath(Chain), *Chain.Last()->GetCPPType()));
			continue;
		}
		const int32 Changed = UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(Probe);
		TestTrue(FString::Printf(
			TEXT("Changing %s invalidates the Command Center revision"), *ChainPath(Chain)),
			Baseline != Changed);
	}
	TestEqual(TEXT("Every reflected field was probeable"), Unprobeable, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIManagementRefusalTextTest,
	"TerritoryFramework.UI.Regression.ManagementRefusalTextIsLocalizable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIManagementRefusalTextTest::RunTest(const FString& Parameters)
{
	// These are the most frequently seen messages in the whole plugin: every failed guard
	// purchase or removal lands on one of them. They used to be built with
	// `FText::FromString`, which produces text with no namespace and no key — invisible to the
	// localization pipeline, so a translated build would show English here forever.
	//
	// Two properties matter and both are checked below: the text is *gatherable* (it has a
	// namespace and key, so the string table can pick it up), and buy and remove share one
	// instance rather than two copies that can drift apart.

	struct FExpected
	{
		const TCHAR* Key;
		const TCHAR* Sentence;
	};

	const FExpected Expected[] = {
		{ TEXT("ManagementUnavailable"), TEXT("Territory management is not installed on this PlayerController.") },
		{ TEXT("OutOfManagementRange"),  TEXT("Move closer to the district management point.") },
		{ TEXT("NoGarrisonSelected"),    TEXT("No district or Property garrison is selected.") },
	};
	const FText Messages[] = {
		UTerritoryDistrictManagementWidget::GetManagementUnavailableReason(),
		UTerritoryDistrictManagementWidget::GetOutOfManagementRangeReason(),
		UTerritoryDistrictManagementWidget::GetNoGarrisonSelectedReason(),
	};
	static_assert(UE_ARRAY_COUNT(Expected) == UE_ARRAY_COUNT(Messages), "one expectation per message");

	TSet<FString> SeenKeys;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Messages); ++Index)
	{
		const FText& Message = Messages[Index];
		const FString Where = FString::Printf(TEXT("message '%s'"), Expected[Index].Key);

		const TOptional<FString> Namespace = FTextInspector::GetNamespace(Message);
		const TOptional<FString> Key = FTextInspector::GetKey(Message);
		const FString* Source = FTextInspector::GetSourceString(Message);

		TestTrue(FString::Printf(TEXT("%s carries a localization namespace"), *Where),
			Namespace.IsSet() && Namespace.GetValue() == TEXT("TerritoryManagement"));
		TestTrue(FString::Printf(TEXT("%s carries a localization key"), *Where),
			Key.IsSet() && Key.GetValue() == Expected[Index].Key);
		TestNotNull(FString::Printf(TEXT("%s has a source string"), *Where), Source);
		if (!Source) continue;

		TestEqual(FString::Printf(TEXT("%s still says what it always said"), *Where),
			*Source, FString(Expected[Index].Sentence));

		if (Key.IsSet())
		{
			TestFalse(FString::Printf(TEXT("%s does not share a key with another message"), *Where),
				SeenKeys.Contains(Key.GetValue()));
			SeenKeys.Add(Key.GetValue());
		}
	}

	// Permanent negative control. Every assertion above passes if these messages are localizable;
	// this proves the checks can tell the difference, by building the very same sentence the old
	// way and requiring it to be *rejected*. Without it, an FTextInspector that reported a
	// namespace for everything would make the whole test pass vacuously.
	const FText LegacyStyle = FText::FromString(
		TEXT("Territory management is not installed on this PlayerController."));
	TestFalse(TEXT("The old FText::FromString spelling has no localization namespace"),
		FTextInspector::GetNamespace(LegacyStyle).IsSet());
	TestFalse(TEXT("The old FText::FromString spelling has no localization key"),
		FTextInspector::GetKey(LegacyStyle).IsSet());
	// Note if you extend this: `ShouldGatherForLocalization()` is deliberately *not* asserted.
	// It inspects only the source string, not the namespace or key, so it answers true for a
	// fresh `FText::FromString` and for a keyed message alike — it cannot tell these two apart.
	// The namespace/key checks above are the ones that discriminate.
	// ...and the sentence itself is byte-identical, so the key is the only thing that differs.
	// That is the whole point: the wording was never the problem, the missing key was.
	const FString* LegacySource = FTextInspector::GetSourceString(LegacyStyle);
	const FString* MessageSource = FTextInspector::GetSourceString(Messages[0]);
	TestTrue(TEXT("The old and new spellings read identically to a player"),
		LegacySource && MessageSource && *LegacySource == *MessageSource);

	// Buy and remove returning *the same* message rather than two hand-written copies is a
	// property of the code shape, not something this test observes: both call sites now call the
	// shared accessors above. It is deliberately not asserted here, because the only way to reach
	// those call sites is to instantiate the widget, and this widget is UCLASS(Abstract). A test
	// that loaded the WBP subclass to prove it would fail for widget-asset reasons and report them
	// as localization failures. The key-uniqueness check above is what catches the realistic
	// regression: someone adding a fourth near-duplicate sentence for the same situation.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIUnloadedDistrictDirectoryTest,
	"TerritoryFramework.UI.WorldPartition.UnloadedDistrictRemainsVisible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIUnloadedDistrictDirectoryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Directory UI test world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	ATerritoryWorldState* WorldState = World->SpawnActor<ATerritoryWorldState>(
		ATerritoryWorldState::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Directory UI WorldState spawned"), WorldState);
	if (!WorldState)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FGameplayTag CityTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach"), false);
	const FGameplayTag DistrictTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill"), false);
	const FGameplayTag PlaceTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.CastleHill.Farm"), false);
	FReplicatedCaptureSummary City;
	City.TerritoryTag = CityTag;
	City.DisplayName = FText::FromString(TEXT("Haven Reach"));
	City.HierarchyLevel = ETerritoryHierarchyLevel::City;
	City.TotalChildren = 1;
	City.bDefinitionBacked = true;
	FReplicatedCaptureSummary District;
	District.TerritoryTag = DistrictTag;
	District.ParentTerritoryTag = CityTag;
	District.DisplayName = FText::FromString(TEXT("Castle Hill"));
	District.HierarchyLevel = ETerritoryHierarchyLevel::District;
	District.TotalChildren = 1;
	District.bDefinitionBacked = true;
	FReplicatedCaptureSummary Place;
	Place.TerritoryTag = PlaceTag;
	Place.ParentTerritoryTag = DistrictTag;
	Place.DisplayName = FText::FromString(TEXT("Farm"));
	Place.HierarchyLevel = ETerritoryHierarchyLevel::Place;
	Place.bDefinitionBacked = true;
	WorldState->SetCaptureSummary(City);
	WorldState->SetCaptureSummary(District);
	WorldState->SetCaptureSummary(Place);

	const TArray<FTerritoryDistrictOperationsView> Views =
		UTerritoryUIBlueprintLibrary::GetPlayerVisibleDistrictOperationsViews(
			World, nullptr, ETerritoryOperationsFilter::All);
	TestEqual(TEXT("Unloaded District still appears in the strategic list"),
		Views.Num(), 1);
	if (!Views.IsEmpty())
	{
		TestNull(TEXT("Directory row does not fake a live actor"), Views[0].District);
		TestFalse(TEXT("Directory row reports runtime actor as unloaded"),
			Views[0].bRuntimeLoaded);
		TestTrue(TEXT("Directory row remains registered for list filtering"),
			Views[0].bRegistered);
		TestTrue(TEXT("Unlocked directory row enters Active Territories"),
			UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(Views[0]));
		TestEqual(TEXT("Unlocked child Place remains visible by stable metadata"),
			Views[0].VisiblePlaces.Num(), 1);
	}

	World->DestroyActor(WorldState);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIStatusTextFamilyTest,
	"TerritoryFramework.UI.Regression.StatusTextIsAlwaysReadable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Every player-facing status label the UI library hands to a widget must be readable and
 * translatable, for any input a save file can carry.
 *
 * The load-bearing assertions are the two that the previous implementation would fail:
 *   - Namespace. These labels used to be read from the enum's own `DisplayName` metadata, which
 *     resolves to the engine's `UObjectDisplayNames` namespace. That namespace is only populated
 *     under `#if WITH_EDITOR`, so a packaged build got `FText::FromString` with no key at all.
 *     Requiring our own namespace is what makes the label translatable in a shipped game.
 *   - Unknown values. `UEnum::GetDisplayNameTextByValue` returns `FText::GetEmpty()` when no entry
 *     matches the value, so an unrecognised state byte printed a blank status label. This enum
 *     already keeps a legacy serialized value for compatibility, so unknown bytes are a real
 *     input rather than a theoretical one.
 *
 * Honest limit: a test running in an editor build cannot observe the `WITH_EDITOR` difference
 * itself — both spellings return keyed text here. What it can observe is *which* namespace the
 * text carries, and that is what discriminates the two implementations.
 */
bool FTerritoryUIStatusTextFamilyTest::RunTest(const FString& Parameters)
{
	TSet<FString> SeenKeys;

	// Fails if the label is blank, keyless, or keyed into a namespace this plugin does not own.
	auto ExpectReadable = [this, &SeenKeys](
		const FText& Label, const TCHAR* What, const TCHAR* ExpectedSource, bool bTrackKey)
	{
		if (!TestFalse(FString::Printf(TEXT("%s is not blank"), What), Label.IsEmpty()))
		{
			return;
		}

		const TOptional<FString> Namespace = FTextInspector::GetNamespace(Label);
		const TOptional<FString> Key = FTextInspector::GetKey(Label);
		TestTrue(FString::Printf(TEXT("%s carries a localization namespace"), What),
			Namespace.IsSet() && Namespace.GetValue() == TEXT("TerritoryOperations"));
		TestTrue(FString::Printf(TEXT("%s carries a localization key"), What), Key.IsSet());

		const FString* Source = FTextInspector::GetSourceString(Label);
		TestTrue(FString::Printf(TEXT("%s reads as authored English"), What),
			Source != nullptr && *Source == ExpectedSource);

		if (bTrackKey && Key.IsSet())
		{
			TestFalse(FString::Printf(TEXT("%s has its own key, so translators can word it separately"),
				What), SeenKeys.Contains(Key.GetValue()));
			SeenKeys.Add(Key.GetValue());
		}
	};

	// The four known political states, each with its own key.
	const TPair<ETerritoryState, const TCHAR*> KnownStates[] = {
		{ ETerritoryState::Unclaimed, TEXT("Unclaimed") },
		{ ETerritoryState::Claimed,   TEXT("Claimed") },
		{ ETerritoryState::Contested, TEXT("Contested") },
		{ ETerritoryState::Locked,    TEXT("Locked") },
	};
	for (const TPair<ETerritoryState, const TCHAR*>& Entry : KnownStates)
	{
		ExpectReadable(
			UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
				ETerritoryAvailability::Unlocked, Entry.Key),
			*FString::Printf(TEXT("The '%s' status label"), Entry.Value),
			Entry.Value, /*bTrackKey=*/true);
	}

	// Availability outranks the political state; the state cannot change this label.
	for (const TPair<ETerritoryState, const TCHAR*>& Entry : KnownStates)
	{
		ExpectReadable(
			UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
				ETerritoryAvailability::Locked, Entry.Key),
			*FString::Printf(TEXT("The locked '%s' status label"), Entry.Value),
			TEXT("Locked"), /*bTrackKey=*/false);
	}

	// A state byte this build does not recognise. 255 is the value a corrupted or
	// newer-build save would most plausibly carry.
	for (const uint8 UnknownState : { static_cast<uint8>(200), static_cast<uint8>(255) })
	{
		ExpectReadable(
			UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
				ETerritoryAvailability::Unlocked,
				static_cast<ETerritoryState>(UnknownState)),
			*FString::Printf(TEXT("An unrecognised political state (%d)"), UnknownState),
			TEXT("Unknown state"), /*bTrackKey=*/false);
	}

	// Faction relationships reach the player through the same path.
	const TPair<EDiplomacyState, const TCHAR*> KnownRelations[] = {
		{ EDiplomacyState::None,           TEXT("Neutral / No Treaty") },
		{ EDiplomacyState::Alliance,       TEXT("Alliance") },
		{ EDiplomacyState::TradeAgreement, TEXT("Trade Agreement") },
		{ EDiplomacyState::NonAggression,  TEXT("Non-Aggression Pact") },
		{ EDiplomacyState::War,            TEXT("War") },
		{ EDiplomacyState::Ceasefire,      TEXT("Ceasefire") },
	};
	for (const TPair<EDiplomacyState, const TCHAR*>& Entry : KnownRelations)
	{
		ExpectReadable(
			UTerritoryUIBlueprintLibrary::GetDiplomacyStateText(Entry.Key),
			*FString::Printf(TEXT("The '%s' relationship label"), Entry.Value),
			Entry.Value, /*bTrackKey=*/true);
	}
	for (const uint8 UnknownRelation : { static_cast<uint8>(200), static_cast<uint8>(255) })
	{
		ExpectReadable(
			UTerritoryUIBlueprintLibrary::GetDiplomacyStateText(
				static_cast<EDiplomacyState>(UnknownRelation)),
			*FString::Printf(TEXT("An unrecognised relationship (%d)"), UnknownRelation),
			TEXT("Unknown relationship"), /*bTrackKey=*/false);
	}

	// The rest of the family already spelled its labels out by hand, so these assertions pass
	// before this change as well. They are here as a contract for the next helper: a switch with a
	// keyed `default:` is what keeps an unknown value from reaching a text block as a blank.
	ExpectReadable(UTerritoryUIBlueprintLibrary::GetAssaultStateText(
		static_cast<ETerritoryAssaultState>(200)), TEXT("An unrecognised assault state"),
		TEXT("Unknown status"), false);
	ExpectReadable(UTerritoryUIBlueprintLibrary::GetAssaultResolutionText(
		static_cast<ETerritoryAssaultResolution>(200)), TEXT("An unrecognised assault outcome"),
		TEXT("Unknown outcome"), false);
	ExpectReadable(UTerritoryUIBlueprintLibrary::GetDiplomacyEventTypeText(
		static_cast<EDiplomacyEventType>(200)), TEXT("An unrecognised diplomacy event"),
		TEXT("Diplomacy updated"), false);
	ExpectReadable(UTerritoryUIBlueprintLibrary::GetThreatLevelText(
		static_cast<ETerritoryThreatLevel>(200)), TEXT("An unrecognised threat level"),
		TEXT("Secure"), false);
	ExpectReadable(UTerritoryUIBlueprintLibrary::GetProductionStatusText(
		static_cast<ETerritoryProductionStatus>(200)), TEXT("An unrecognised production status"),
		TEXT("Not evaluated"), false);

	// Negative control. Without this, the namespace and key assertions above could be vacuous:
	// they would still pass if FTextInspector reported a namespace for unkeyed text. The old
	// spelling of these very labels is the counter-example, so it must be rejected.
	const FText Unkeyed = FText::FromString(TEXT("Unknown state"));
	TestTrue(TEXT("Control: the message text reads the same"), Unkeyed.ToString() == TEXT("Unknown state"));
	TestFalse(TEXT("Control: but it owns no localization namespace"),
		FTextInspector::GetNamespace(Unkeyed).IsSet());
	TestFalse(TEXT("Control: and no localization key"),
		FTextInspector::GetKey(Unkeyed).IsSet());

	// Controls that run the engine's own enum reflection, the implementation these labels used
	// before. They exist so the two load-bearing assertions above are proven to discriminate
	// rather than merely asserted to. Both were checked by running them; if either ever starts
	// failing, the reasoning in the doc comment is what needs revisiting, not the fix.
	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	if (TestNotNull(TEXT("Control: the state enum is reflected"), StateEnum))
	{
		const FText EngineKnown = StateEnum->GetDisplayNameTextByValue(
			static_cast<int64>(ETerritoryState::Contested));
		TestEqual(TEXT("Control: the engine path spells a known state the same way we do"),
			EngineKnown.ToString(), FString(TEXT("Contested")));
		const TOptional<FString> EngineKnownNamespace = FTextInspector::GetNamespace(EngineKnown);
		TestTrue(TEXT("Control: but files it under the engine's editor-only namespace, not ours"),
			!EngineKnownNamespace.IsSet()
			|| EngineKnownNamespace.GetValue() != TEXT("TerritoryOperations"));

		const FText EngineUnknown = StateEnum->GetDisplayNameTextByValue(200);
		TestTrue(TEXT("Control: the engine path returns genuinely blank text for an unrecognised state"),
			EngineUnknown.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIJournalIdentifierTextTest,
	"TerritoryFramework.UI.Regression.JournalRowsShowNamesNotIdentifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Two journal rows are built from raw engine data — assault-route IDs and a source-territory
 * gameplay tag — and both used to print that data straight at the player, inside sentences that
 * were `FText::FromString` and therefore untranslatable.
 *
 * The load-bearing assertion is the leak check: the raw tag used to appear verbatim, so a line
 * that no longer contains it is the fix, and a line that still contains it is the bug.
 */
bool FTerritoryUIJournalIdentifierTextTest::RunTest(const FString& Parameters)
{
	// --- Assault routes ----------------------------------------------------------------------
	const FText NoRoutes =
		UTerritoryJournalWidget::GetAssaultApproachListText(TArray<FName>());
	TestFalse(TEXT("An operation with no selected routes does not render a blank line"),
		NoRoutes.IsEmpty());
	TestEqual(TEXT("An operation with no selected routes says so"),
		NoRoutes.ToString(), FString(TEXT("No assault routes selected.")));
	TestTrue(TEXT("The empty-routes sentence can be translated"),
		FTextInspector::GetKey(NoRoutes).IsSet());

	// "RemovedDeparture" is the real shape of an authored route ID.
	const FText OneRoute = UTerritoryJournalWidget::GetAssaultApproachListText(
		{ FName(TEXT("RemovedDeparture")) });
	TestTrue(TEXT("A route ID is split into readable words"),
		OneRoute.ToString().Contains(TEXT("Removed Departure")));
	TestFalse(TEXT("The raw identifier never reaches the player"),
		OneRoute.ToString().Contains(TEXT("RemovedDeparture")));
	// Deliberately no key assertion on a joined line. `FTextInspector::GetKey` reads
	// `GetTextHistory().GetTextId()`, and text composed by `FText::Join` / `FText::Format` has no
	// text id of its own — the key belongs to the *pattern* the formatter owns. Asserting a key here
	// fails against a correct implementation, which is how this test first failed. The keyed cases
	// assertable from outside are the sentences returned directly (here and below); the composed
	// lines are covered by their content and leak checks instead.

	const FText TwoRoutes = UTerritoryJournalWidget::GetAssaultApproachListText(
		{ FName(TEXT("RemovedDeparture")), FName(TEXT("NorthRoad")) });
	TestTrue(TEXT("Both routes are shown"), TwoRoutes.ToString().Contains(TEXT("Removed Departure"))
		&& TwoRoutes.ToString().Contains(TEXT("North Road")));
	TestFalse(TEXT("No technical punctuation leaks through the joined route line"),
		TwoRoutes.ToString().Contains(TEXT("_")) || TwoRoutes.ToString().Contains(TEXT(".")));

	// --- Transaction lines -------------------------------------------------------------------
	// GuardStaffing is declared natively by this plugin, so it is registered in any project that
	// loads the plugin. Asserting its validity is what keeps the two leak checks below honest: with
	// an unregistered tag there would be no bracket on the line, and "the raw tag is absent" would
	// pass for entirely the wrong reason.
	const FGameplayTag SourceTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("Territory.Capability.GuardStaffing")));
	if (!TestTrue(TEXT("Fixture: the plugin's native GuardStaffing tag is registered"),
		SourceTag.IsValid()))
	{
		return false;
	}
	const FText TagName = UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(SourceTag);
	TestTrue(TEXT("Fixture: the tag resolves to a friendly name rather than itself"),
		!TagName.IsEmpty() && TagName.ToString() != SourceTag.ToString());

	const FText Gain = UTerritoryJournalWidget::GetTransactionLineText(
		50, TEXT("Capture reward"), SourceTag);
	TestTrue(TEXT("A gain shows its sign"), Gain.ToString().Contains(TEXT("+50")));
	TestTrue(TEXT("A gain shows its reason"), Gain.ToString().Contains(TEXT("Capture reward")));
	TestTrue(TEXT("The source territory is shown by its friendly name"),
		Gain.ToString().Contains(TagName.ToString()));
	TestTrue(TEXT("The source territory is bracketed"), Gain.ToString().Contains(TEXT("[")));
	TestFalse(TEXT("The raw gameplay tag never reaches the player"),
		Gain.ToString().Contains(SourceTag.ToString()));
	TestFalse(TEXT("The technical tag namespace never reaches the player"),
		Gain.ToString().Contains(TEXT("Territory.")));
	// No key assertion on the composed line, for the reason given at the route line above.

	// Controls proving the two leak assertions are not vacuous: both transformations must actually
	// remove the raw token the old lines printed, or "the raw value is absent" would pass because
	// the fixture never contained it in the first place.
	const FString RawApproach = FName(TEXT("RemovedDeparture")).ToString();
	const FString ReadableApproach = FName::NameToDisplayString(RawApproach, false);
	TestFalse(TEXT("Control: the readable route name no longer contains the raw identifier"),
		ReadableApproach.Contains(RawApproach));
	TestFalse(TEXT("Control: the friendly tag name no longer contains the raw tag"),
		TagName.ToString().Contains(SourceTag.ToString()));

	const FText Loss = UTerritoryJournalWidget::GetTransactionLineText(
		-20, FString(), FGameplayTag());
	TestTrue(TEXT("A loss shows its sign"), Loss.ToString().Contains(TEXT("-20")));
	TestTrue(TEXT("A missing reason is described, not left blank"),
		Loss.ToString().Contains(TEXT("Unspecified transaction")));
	TestFalse(TEXT("A line without a source territory omits the bracket"),
		Loss.ToString().Contains(TEXT("[")));

	// --- The audit block ---------------------------------------------------------------------
	const FText EmptyAudit = UTerritoryJournalWidget::GetTransactionAuditText(TArray<FText>());
	TestEqual(TEXT("An empty audit says so"), EmptyAudit.ToString(),
		FString(TEXT("No recent transactions.")));
	TestTrue(TEXT("The empty-audit sentence can be translated"),
		FTextInspector::GetKey(EmptyAudit).IsSet());

	const FText FilledAudit = UTerritoryJournalWidget::GetTransactionAuditText({ Gain, Loss });
	TestTrue(TEXT("The audit shows every line"), FilledAudit.ToString().Contains(TEXT("+50"))
		&& FilledAudit.ToString().Contains(TEXT("-20")));

	// Negative control: without this, "carries a localization key" above could be vacuous.
	const FText Unkeyed = FText::FromString(TEXT("No recent transactions."));
	TestEqual(TEXT("Control: the old spelling reads identically"),
		Unkeyed.ToString(), EmptyAudit.ToString());
	TestFalse(TEXT("Control: but it has no key, so a translated build would show English forever"),
		FTextInspector::GetKey(Unkeyed).IsSet());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIFocusReachabilityTest,
	"TerritoryFramework.UI.Regression.FocusNeverTargetsAHiddenWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Focus selection must never land on a widget the player cannot see.
 *
 * The trap this covers: collapsing a panel leaves every child's *own* visibility at `Visible`, so
 * the previous `IsVisible()` check accepted a button inside a collapsed pane and handed it focus.
 * `IsVisible()` also reads the widget's own cached Slate visibility, so it answers about the widget
 * and not about where the widget sits.
 */
bool FTerritoryUIFocusReachabilityTest::RunTest(const FString& Parameters)
{
	UOverlay* Root = NewObject<UOverlay>();
	UOverlay* DetailPane = NewObject<UOverlay>();
	UTextBlock* PaneChild = NewObject<UTextBlock>();
	if (!TestNotNull(TEXT("Root panel"), Root)
		|| !TestNotNull(TEXT("Detail pane"), DetailPane)
		|| !TestNotNull(TEXT("Pane child"), PaneChild))
	{
		return false;
	}
	Root->AddChild(DetailPane);
	DetailPane->AddChild(PaneChild);

	// A plain visible child under an expanded parent is reachable.
	TestTrue(TEXT("A visible widget under a visible parent can take focus"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(PaneChild));

	// The load-bearing case. Collapsing the pane must make the child unreachable even though the
	// child was never told anything about visibility.
	DetailPane->SetVisibility(ESlateVisibility::Collapsed);
	TestFalse(TEXT("A widget inside a collapsed pane can never take focus"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(PaneChild));
	TestFalse(TEXT("The collapsed pane itself can never take focus"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(DetailPane));

	// Control: this is *why* looking at the widget alone answers the wrong question. The child still
	// reports its own visibility as Visible while the pane above it is collapsed, so any check that
	// skips the ancestor walk accepts it. This is the exact value the old `IsVisible()` path saw.
	TestEqual(TEXT("Control: the child still reports its own visibility as Visible"),
		PaneChild->GetVisibility(), ESlateVisibility::Visible);

	DetailPane->SetVisibility(ESlateVisibility::Hidden);
	TestFalse(TEXT("A widget inside a hidden pane can never take focus"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(PaneChild));

	DetailPane->SetVisibility(ESlateVisibility::Visible);
	TestTrue(TEXT("Restoring the pane restores reachability"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(PaneChild));

	// Boundary: these block the mouse but are still drawn, and a gamepad focus path never hit-tests,
	// so they stay reachable. Recorded here so the distinction is deliberate, not accidental.
	DetailPane->SetVisibility(ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("A drawn-but-not-clickable pane still offers focus targets"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(PaneChild));
	DetailPane->SetVisibility(ESlateVisibility::Visible);

	// Disabled and null.
	PaneChild->SetIsEnabled(false);
	TestFalse(TEXT("A disabled widget can never take focus"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(PaneChild));
	TestFalse(TEXT("A null widget can never take focus"),
		UTerritoryActivatableWidget::IsFocusTargetReachable(nullptr));

	Root->ClearChildren();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerritoryUIShippedStyleAssetsLoadTest,
	"TerritoryFramework.UI.Regression.ShippedStyleAssetsAreLoadable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIShippedStyleAssetsLoadTest::RunTest(const FString& Parameters)
{
	// Why this test exists.
	//
	// The shipped Territory UI styles are *assets that reference other assets*, and a reference can rot
	// without a single line of C++ changing. Reparenting a Blueprint to a class that later moves into a
	// plugin, or pointing a style at a theme asset that was never committed, leaves a dangling path
	// frozen inside a .uasset. No compiler sees it, the plugin still builds, and the UI still comes up -
	// the asset just fails to load and the widget quietly falls back to a default look. Nothing reports
	// a problem to the developer.
	//
	// This is not hypothetical. ButtonStyle_TerritoryTab shipped parented to
	// /Game/NP_RPGUITheme/Style/MasterStyles/Button/ButtonStyle_NarrativeMaster, a path that has never
	// existed for this plugin: NP_RPGUITheme is a plugin whose content mounts at /NP_RPGUITheme/...,
	// never under /Game/. The Blueprint failed to load, so
	// UTerritoryDeveloperSettings::TerritoryTabButtonStyle resolved to null and every Territory
	// navigation tab fell back to the default button style. WBP_TerritoryButton_Text carried the same
	// class of dangling reference to /Game/NP_RPGUITheme/.../TextStyle_Master_Primary_H2, and that
	// widget is used by the Command Row, the District Management widget and the Journal.
	//
	// The check is "the class compiles", not "the file exists" - deliberately. An asset can sit on disk
	// and still be unusable, which is exactly what a dangling parent produces, so asserting existence
	// would pass straight through the bug this guards.
	//
	// Scope note: this covers the plugin's own shipped content under /TerritoryFramework/UI, so it is
	// portable to any project that installs the plugin. It does not cover a consuming project's own
	// copies of these styles - this project duplicates them under /Game/TerritoryFramework/UI/Styles
	// and overrides the settings to match, which is what TerritoryFramework.UI.ExclusiveTabSelection
	// exercises.

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Named first, so renaming or moving a shipped style cannot quietly shrink the sweep below.
	const TCHAR* RequiredAssets[] = {
		TEXT("/TerritoryFramework/UI/Styles/ButtonStyle_TerritoryAction"),
		TEXT("/TerritoryFramework/UI/Styles/ButtonStyle_TerritoryPrimary"),
		TEXT("/TerritoryFramework/UI/Styles/ButtonStyle_TerritorySecondary"),
		TEXT("/TerritoryFramework/UI/Styles/ButtonStyle_TerritoryTab"),
		TEXT("/TerritoryFramework/UI/Styles/TextStyle_TerritoryButton"),
		TEXT("/TerritoryFramework/UI/Styles/TextStyle_TerritoryButtonDisabled"),
		TEXT("/TerritoryFramework/UI/Styles/TextStyle_TerritoryButtonSelected"),
		TEXT("/TerritoryFramework/UI/Styles/WBP_TerritoryButton_Text"),
	};

	auto ReportAsset = [this](const FString& AssetPath) -> bool
	{
		UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *AssetPath), Asset))
		{
			return false;
		}
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			// A Blueprint that loads but cannot compile has a broken parent or a broken reference.
			return TestNotNull(
				*FString::Printf(TEXT("%s compiles (its parent class resolves)"), *AssetPath),
				Blueprint->GeneratedClass.Get());
		}
		return true;
	};

	for (const TCHAR* Required : RequiredAssets)
	{
		ReportAsset(Required);
	}

	TArray<FAssetData> ShippedAssets;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/TerritoryFramework/UI")), ShippedAssets,
		/*bRecursive=*/true);
	TestTrue(TEXT("The shipped UI folder is discoverable in the asset registry"),
		ShippedAssets.Num() > 0);

	int32 UnusableCount = 0;
	for (const FAssetData& AssetData : ShippedAssets)
	{
		if (!ReportAsset(AssetData.GetSoftObjectPath().ToString()))
		{
			++UnusableCount;
		}
	}

	AddInfo(FString::Printf(
		TEXT("Checked %d shipped UI assets under /TerritoryFramework/UI; %d unusable."),
		ShippedAssets.Num(), UnusableCount));
	return true;
}

// -------------------------------------------------------------------------------------------------
// Dependency guard: a Territory asset must not reference a package that cannot exist.
//
// Why this test exists. The loadability test above cannot catch this class of defect. Six stale
// references to /Game/NP_RPGUITheme/... were found across four Territory widgets, and every one of
// them is invisible to that test: the asset still loads and its class still compiles, because a
// dangling reference held by a child widget does not stop either. The only symptom is a "LoadErrors"
// line in the log, and nothing asserted on it.
//
// The asset registry does retain unresolvable content-package imports, so the defect IS visible from
// C++ even though the widget tree is not reachable from Python - reading
// /Game/NP_RPGUITheme/Style/MasterStyles/Text/Primary/TextStyle_Master_Primary_H2 straight out of
// WBP_TerritoryButton_Text's dependency list is how these were found.
//
// /Script/... entries are skipped deliberately: they are code modules, not packages, so they are
// never in the asset registry and every asset would otherwise report as broken.
//
// Confirmed red before any fix, at 7 assets and 10 references. It is expected to stay red until the
// stale references are repointed, which needs the editor - the references live in widget trees.
// -------------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTerritoryUIDependenciesResolveTest,
	"TerritoryFramework.UI.Regression.ShippedUIDependenciesResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerritoryUIDependenciesResolveTest::RunTest(const FString& Parameters)
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetRegistryDependencyOptions Options;
	Options.bIncludeHardPackageReferences = true;

	// The plugin's own shipped UI is always checked. The project's duplicate copy is checked only if
	// this project actually has one, so a consumer project keeping a single authored copy in the
	// plugin does not fail for its absence.
	TArray<FString> Scopes;
	Scopes.Add(TEXT("/TerritoryFramework/UI"));

	TArray<FAssetData> ProjectCopies;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/Game/TerritoryFramework/UI")), ProjectCopies,
		/*bRecursive=*/true);
	if (ProjectCopies.Num() > 0)
	{
		Scopes.Add(TEXT("/Game/TerritoryFramework/UI"));
	}
	else
	{
		AddInfo(TEXT("No /Game/TerritoryFramework/UI copy here; checking the plugin copy only."));
	}

	int32 CheckedAssets = 0;
	int32 BrokenAssets = 0;
	int32 DanglingReferences = 0;

	for (const FString& Scope : Scopes)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPath(FName(*Scope), Assets, /*bRecursive=*/true);

		for (const FAssetData& AssetData : Assets)
		{
			++CheckedAssets;

			TArray<FName> Dependencies;
			AssetRegistry.K2_GetDependencies(AssetData.PackageName, Options, Dependencies);

			TArray<FString> Missing;
			for (const FName& Dependency : Dependencies)
			{
				const FString DependencyName = Dependency.ToString();

				if (DependencyName.StartsWith(TEXT("/Script/")))
				{
					continue; // A code module, not a package - never in the asset registry.
				}

				TArray<FAssetData> DependencyAssets;
				if (!AssetRegistry.GetAssetsByPackageName(Dependency, DependencyAssets)
					|| DependencyAssets.Num() == 0)
				{
					Missing.Add(DependencyName);
				}
			}

			if (Missing.Num() > 0)
			{
				++BrokenAssets;
				DanglingReferences += Missing.Num();
				AddError(FString::Printf(
					TEXT("%s references %d package(s) that do not exist: %s. NP_RPGUITheme is a plugin ")
					TEXT("mounting at /NP_RPGUITheme/, so a /Game/NP_RPGUITheme/... reference can never ")
					TEXT("resolve - point it at the plugin mount instead."),
					*AssetData.PackageName.ToString(), Missing.Num(),
					*FString::Join(Missing, TEXT(", "))));
			}
		}
	}

	// A guard that inspects nothing passes vacuously. If the sweep ever stops finding assets, this
	// is what says so instead of the test quietly becoming a no-op.
	TestTrue(*FString::Printf(TEXT("The sweep inspected assets (checked %d)"), CheckedAssets),
		CheckedAssets > 0);

	AddInfo(FString::Printf(
		TEXT("Checked %d Territory UI assets across %d scope(s); %d asset(s) hold %d unresolvable ")
		TEXT("reference(s)."),
		CheckedAssets, Scopes.Num(), BrokenAssets, DanglingReferences));

	return true;
}

#endif
