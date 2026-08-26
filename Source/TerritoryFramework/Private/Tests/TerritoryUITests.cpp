#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/ScrollBox.h"
#include "NarrativeActivatableWidget.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Navigation/TerritoryMapMarker.h"
#include "UI/TerritoryActivatableWidget.h"
#include "UI/TerritoryDistrictManagementWidget.h"
#include "UI/TerritoryDistrictRowWidget.h"
#include "UI/TerritoryHUDWidget.h"
#include "UI/TerritoryJournalWidget.h"
#include "UI/TerritoryLiveEventRowWidget.h"
#include "UI/TerritoryLiveEventTypes.h"
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
	TestTrue(TEXT("Journal exposes Captured Territory population diagnostics"),
		TerritoryUITest::IsBlueprintPure(
			UTerritoryJournalWidget::StaticClass(), TEXT("GetCapturedTerritoryEntryCount")));
	const FObjectPropertyBase* ActiveTerritoriesProperty = CastField<FObjectPropertyBase>(
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(TEXT("ActiveTerritoriesBox")));
	TestTrue(TEXT("Active Territories uses the Quest Journal ScrollBox template"),
		ActiveTerritoriesProperty
			&& ActiveTerritoriesProperty->PropertyClass->IsChildOf(UScrollBox::StaticClass()));
	const FObjectPropertyBase* CapturedTerritoriesProperty = CastField<FObjectPropertyBase>(
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(TEXT("CapturedTerritoriesBox")));
	TestTrue(TEXT("Captured Territories uses the Quest Journal ScrollBox template"),
		CapturedTerritoriesProperty
			&& CapturedTerritoriesProperty->PropertyClass->IsChildOf(UScrollBox::StaticClass()));
	TestNotNull(TEXT("Journal exposes the persistent selected Territory information pane"),
		UTerritoryJournalWidget::StaticClass()->FindPropertyByName(
			TEXT("SelectedTerritoryInfoBox")));
	TestTrue(TEXT("Embedded Territory activatables accept focus"),
		GetDefault<UTerritoryActivatableWidget>()->IsFocusable());

	const UClass* LibraryClass = UTerritoryUIBlueprintLibrary::StaticClass();
	TestTrue(TEXT("OpenTerritoryMenu is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("OpenTerritoryMenu")));
	TestTrue(TEXT("Set Territory waypoint is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("SetTerritoryWaypoint")));
	TestTrue(TEXT("Clear Territory waypoint is Blueprint callable"),
		TerritoryUITest::IsBlueprintCallable(LibraryClass, TEXT("ClearTerritoryWaypoint")));
	TestTrue(TEXT("Tracked Territory query is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetTrackedTerritory")));
	TestTrue(TEXT("District operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildDistrictOperationsView")));
	TestTrue(TEXT("District operations list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetDistrictOperationsViews")));
	TestTrue(TEXT("Player-visible District list is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("GetPlayerVisibleDistrictOperationsViews")));
	TestTrue(TEXT("Hierarchy operations builder is Blueprint pure"),
		TerritoryUITest::IsBlueprintPure(LibraryClass, TEXT("BuildHierarchyOperationsView")));
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
		TestTrue(TEXT("Tracking promotes exactly the selected marker"), Marker->IsTracked());
		Marker->SetTracked(false);
		TestFalse(TEXT("Clearing tracking demotes the marker"), Marker->IsTracked());
	}
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

#endif
