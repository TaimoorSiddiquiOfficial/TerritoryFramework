#include "UI/TerritoryJournalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "UI/TerritoryDistrictRowWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "TimerManager.h"

namespace
{
	const FString AllOwnersOption = TEXT("All owners");
	const FString AllStatesOption = TEXT("All states");
	const FString AllOperationsOption = TEXT("All districts");
	const FString UnlockedOption = TEXT("Unlocked");
	const FString AvailableOption = TEXT("Available");
	const FString OwnedOption = TEXT("Owned");
	const FString ManageableOption = TEXT("Manageable");
	const FString UnderAttackOption = TEXT("Under attack");
	const FString ContestedOption = TEXT("Contested");
	const FString LockedOption = TEXT("Locked");
	const FString FinancialRiskOption = TEXT("Financial risk");

	ETerritoryOperationsFilter GetOperationsFilter(const FString& Option)
	{
		if (Option == UnlockedOption) return ETerritoryOperationsFilter::Unlocked;
		if (Option == AvailableOption) return ETerritoryOperationsFilter::Available;
		if (Option == OwnedOption) return ETerritoryOperationsFilter::Owned;
		if (Option == ManageableOption) return ETerritoryOperationsFilter::Manageable;
		if (Option == UnderAttackOption) return ETerritoryOperationsFilter::UnderAttack;
		if (Option == ContestedOption) return ETerritoryOperationsFilter::Contested;
		if (Option == LockedOption) return ETerritoryOperationsFilter::Locked;
		if (Option == FinancialRiskOption) return ETerritoryOperationsFilter::FinancialRisk;
		return ETerritoryOperationsFilter::All;
	}

	FString GetOperationsOption(ETerritoryOperationsFilter Filter)
	{
		switch (Filter)
		{
		case ETerritoryOperationsFilter::Unlocked: return UnlockedOption;
		case ETerritoryOperationsFilter::Available: return AvailableOption;
		case ETerritoryOperationsFilter::Owned: return OwnedOption;
		case ETerritoryOperationsFilter::Manageable: return ManageableOption;
		case ETerritoryOperationsFilter::UnderAttack: return UnderAttackOption;
		case ETerritoryOperationsFilter::Contested: return ContestedOption;
		case ETerritoryOperationsFilter::Locked: return LockedOption;
		case ETerritoryOperationsFilter::FinancialRisk: return FinancialRiskOption;
		case ETerritoryOperationsFilter::All:
		default: return AllOperationsOption;
		}
	}

	FString GetStateOption(const ATerritoryDistrict* District)
	{
		if (!District)
		{
			return FString();
		}

		const UEnum* StateEnum = StaticEnum<ETerritoryState>();
		return StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(District->GetTerritoryState())).ToString()
			: FString();
	}
}

void UTerritoryJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_TerritoryTab)
	{
		Btn_TerritoryTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleTerritoryTabClicked);
		Btn_TerritoryTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "TerritoryTab", "TERRITORY"));
	}
	if (Btn_EarningsTab)
	{
		Btn_EarningsTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleEarningsTabClicked);
		Btn_EarningsTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "EarningsTab", "EARNINGS"));
	}
	if (Btn_LossTab)
	{
		Btn_LossTab->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleLossTabClicked);
		Btn_LossTab->SetButtonText(NSLOCTEXT("TerritoryJournal", "LossTab", "LOSSES"));
	}
	if (Btn_CommandAddGuard)
	{
		Btn_CommandAddGuard->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandAddGuardClicked);
		Btn_CommandAddGuard->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandAddGuard", "+ 1 GUARD"));
	}
	if (Btn_CommandRemoveGuard)
	{
		Btn_CommandRemoveGuard->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandRemoveGuardClicked);
		Btn_CommandRemoveGuard->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandRemoveGuard", "- 1 GUARD"));
	}
	if (Btn_CommandAddFiveGuards)
	{
		Btn_CommandAddFiveGuards->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandAddFiveGuardsClicked);
		Btn_CommandAddFiveGuards->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandAddFiveGuards", "+ 5 GUARDS"));
	}
	if (Btn_CommandRemoveFiveGuards)
	{
		Btn_CommandRemoveFiveGuards->OnClicked().AddUObject(this, &UTerritoryJournalWidget::HandleCommandRemoveFiveGuardsClicked);
		Btn_CommandRemoveFiveGuards->SetButtonText(NSLOCTEXT("TerritoryJournal", "CommandRemoveFiveGuards", "- 5 GUARDS"));
	}
	if (DistrictSearchBox)
	{
		DistrictSearchBox->OnTextChanged.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleSearchChanged);
	}
	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->OnSelectionChanged.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleOwnerFilterChanged);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->OnSelectionChanged.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleStateFilterChanged);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->OnSelectionChanged.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleOperationalFilterChanged);
	}

	BindTerritoryDelegates();
	BindManagementComponent();
	RefreshFilterOptions();
	RefreshDistrictList();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UTerritoryJournalWidget::RefreshDistrictList,
			1.f,
			true);
	}
}

void UTerritoryJournalWidget::NativeDestruct()
{
	if (Btn_TerritoryTab)
	{
		Btn_TerritoryTab->OnClicked().RemoveAll(this);
	}
	if (Btn_EarningsTab)
	{
		Btn_EarningsTab->OnClicked().RemoveAll(this);
	}
	if (Btn_LossTab)
	{
		Btn_LossTab->OnClicked().RemoveAll(this);
	}
	if (Btn_CommandAddGuard) Btn_CommandAddGuard->OnClicked().RemoveAll(this);
	if (Btn_CommandRemoveGuard) Btn_CommandRemoveGuard->OnClicked().RemoveAll(this);
	if (Btn_CommandAddFiveGuards) Btn_CommandAddFiveGuards->OnClicked().RemoveAll(this);
	if (Btn_CommandRemoveFiveGuards) Btn_CommandRemoveFiveGuards->OnClicked().RemoveAll(this);
	if (DistrictSearchBox)
	{
		DistrictSearchBox->OnTextChanged.RemoveDynamic(this, &UTerritoryJournalWidget::HandleSearchChanged);
	}
	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->OnSelectionChanged.RemoveDynamic(this, &UTerritoryJournalWidget::HandleOwnerFilterChanged);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->OnSelectionChanged.RemoveDynamic(this, &UTerritoryJournalWidget::HandleStateFilterChanged);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->OnSelectionChanged.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleOperationalFilterChanged);
	}

	UnbindTerritoryDelegates();
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleGuardManagementResult);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UTerritoryJournalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::BindTerritoryDelegates()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleTerritoryUnregistered);
		}
	}
}

void UTerritoryJournalWidget::UnbindTerritoryDelegates()
{
	if (UWorld* World = GetWorld())
	{
		if (UTerritoryRegistrySubsystem* Registry = World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Registry->OnTerritoryRegistered.RemoveDynamic(this, &UTerritoryJournalWidget::HandleTerritoryRegistered);
			Registry->OnTerritoryUnregistered.RemoveDynamic(this, &UTerritoryJournalWidget::HandleTerritoryUnregistered);
		}
	}
}

void UTerritoryJournalWidget::BindManagementComponent()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UTerritoryPlayerManagementComponent* Component = PlayerController
		? UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController)
		: nullptr;
	if (ManagementComponent.Get() == Component)
	{
		return;
	}

	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryJournalWidget::HandleGuardManagementResult);
	}
	ManagementComponent = Component;
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.AddUniqueDynamic(
			this, &UTerritoryJournalWidget::HandleGuardManagementResult);
	}
}

void UTerritoryJournalWidget::RefreshFilterOptions()
{
	TArray<ATerritoryDistrict*> Districts = UTerritoryBlueprintLibrary::GetAllDistricts(this);

	if (!bFiltersInitialized)
	{
		SelectedOwnerFilter = AllOwnersOption;
		SelectedStateFilter = AllStatesOption;
	}

	OwnerFilterTags.Empty();
	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->ClearOptions();
		DistrictOwnerFilter->AddOption(AllOwnersOption);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->ClearOptions();
		DistrictStateFilter->AddOption(AllStatesOption);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->ClearOptions();
		DistrictOperationalFilter->AddOption(AllOperationsOption);
		DistrictOperationalFilter->AddOption(UnlockedOption);
		DistrictOperationalFilter->AddOption(AvailableOption);
		DistrictOperationalFilter->AddOption(OwnedOption);
		DistrictOperationalFilter->AddOption(ManageableOption);
		DistrictOperationalFilter->AddOption(UnderAttackOption);
		DistrictOperationalFilter->AddOption(ContestedOption);
		DistrictOperationalFilter->AddOption(LockedOption);
		DistrictOperationalFilter->AddOption(FinancialRiskOption);
	}

	TSet<FString> OwnerNames;
	TSet<FString> StateNames;
	for (ATerritoryDistrict* District : Districts)
	{
		if (!District)
		{
			continue;
		}

		const FGameplayTag OwnerTag = District->GetOwningFaction();
		const FString OwnerName = UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(OwnerTag).ToString();
		if (!OwnerName.IsEmpty() && !OwnerNames.Contains(OwnerName))
		{
			OwnerNames.Add(OwnerName);
			OwnerFilterTags.Add(OwnerName, OwnerTag);
			if (DistrictOwnerFilter)
			{
				DistrictOwnerFilter->AddOption(OwnerName);
			}
		}

		const FString StateName = GetStateOption(District);
		if (!StateName.IsEmpty() && !StateNames.Contains(StateName))
		{
			StateNames.Add(StateName);
			if (DistrictStateFilter)
			{
				DistrictStateFilter->AddOption(StateName);
			}
		}
	}

	// CommonUI-backed controls may synchronously broadcast selection changes.
	// Mark initialization complete before restoring the selected options.
	bFiltersInitialized = true;

	if (DistrictOwnerFilter)
	{
		DistrictOwnerFilter->SetSelectedOption(SelectedOwnerFilter.IsEmpty() ? AllOwnersOption : SelectedOwnerFilter);
	}
	if (DistrictStateFilter)
	{
		DistrictStateFilter->SetSelectedOption(SelectedStateFilter.IsEmpty() ? AllStatesOption : SelectedStateFilter);
	}
	if (DistrictOperationalFilter)
	{
		DistrictOperationalFilter->SetSelectedOption(GetOperationsOption(SelectedOperationsFilter));
	}
}

bool UTerritoryJournalWidget::PassesFilters(const FTerritoryDistrictOperationsView& View) const
{
	const ATerritoryDistrict* District = View.District;
	if (!District)
	{
		return false;
	}

	const FString DistrictName = District->GetTerritoryDisplayName().ToString();
	if (!SearchFilter.IsEmpty() && !DistrictName.Contains(SearchFilter, ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (!SelectedOwnerFilter.IsEmpty() && SelectedOwnerFilter != AllOwnersOption)
	{
		const FGameplayTag* OwnerTag = OwnerFilterTags.Find(SelectedOwnerFilter);
		if (!OwnerTag || District->GetOwningFaction() != *OwnerTag)
		{
			return false;
		}
	}

	const bool bPassesState = SelectedStateFilter.IsEmpty()
		|| SelectedStateFilter == AllStatesOption
		|| SelectedStateFilter == GetStateOption(District);
	return bPassesState
		&& UTerritoryUIBlueprintLibrary::DoesDistrictMatchFilter(View, SelectedOperationsFilter);
}

void UTerritoryJournalWidget::RefreshDistrictList()
{
	if (!bFiltersInitialized)
	{
		RefreshFilterOptions();
	}

	// Include every displayed authority in the revision. Count+filter caching left guard,
	// economy, capture, and assault rows stale whenever the number of districts was stable.
	const TArray<FTerritoryDistrictOperationsView> AllViews =
		UTerritoryUIBlueprintLibrary::GetDistrictOperationsViews(
			this, GetOwningPlayer(), ETerritoryOperationsFilter::All);
	uint32 Revision = GetTypeHash(SelectedOwnerFilter);
	Revision = HashCombineFast(Revision, GetTypeHash(SelectedStateFilter));
	Revision = HashCombineFast(Revision, GetTypeHash(SearchFilter));
	Revision = HashCombineFast(Revision, GetTypeHash(static_cast<uint8>(SelectedOperationsFilter)));
	for (const FTerritoryDistrictOperationsView& View : AllViews)
	{
		Revision = HashCombineFast(Revision,
			static_cast<uint32>(UTerritoryUIBlueprintLibrary::GetDistrictOperationsRevision(View)));
	}
	if (LastOperationsRevision == static_cast<int32>(Revision))
	{
		return;
	}
	LastOperationsRevision = static_cast<int32>(Revision);

	if (DistrictList)
	{
		DistrictList->ClearChildren();
	}

	int32 VisibleCount = 0;
	ATerritoryDistrict* FirstVisibleDistrict = nullptr;
	bool bSelectedStillVisible = false;
	for (const FTerritoryDistrictOperationsView& View : AllViews)
	{
		if (!PassesFilters(View))
		{
			continue;
		}

		if (!FirstVisibleDistrict)
		{
			FirstVisibleDistrict = View.District;
		}
		bSelectedStillVisible |= SelectedDistrict.Get() == View.District;
		++VisibleCount;

		if (DistrictList)
		{
			if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
			{
				DistrictList->AddChild(Row);
			}
		}
	}

	if (VisibleCount == 0 && DistrictList)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NoDistrictsMessage"));
		EmptyText->SetText(NSLOCTEXT("TerritoryJournal", "NoDistricts", "No districts match the current filters."));
		DistrictList->AddChild(EmptyText);
	}
	if (VisibleCount > 0 && (!SelectedDistrict.IsValid() || !bSelectedStillVisible))
	{
		UpdateSelectedDistrict(FirstVisibleDistrict);
	}
	else if (SelectedDistrict.IsValid())
	{
		UpdateSelectedDistrict(SelectedDistrict.Get());
	}

	if (Text_FilterSummary)
	{
		Text_FilterSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "FilterSummary", "{0} districts"),
			FText::AsNumber(VisibleCount)));
	}
	RefreshOperationalSummaries(AllViews);
}

UTerritoryDistrictRowWidget* UTerritoryJournalWidget::CreateOperationsRow(
	const FTerritoryDistrictOperationsView& View)
{
	TSubclassOf<UTerritoryDistrictRowWidget> RowClass = DistrictRowWidgetClass;
	if (!RowClass)
	{
		RowClass = UTerritoryDistrictRowWidget::StaticClass();
	}
	UTerritoryDistrictRowWidget* Row = CreateWidget<UTerritoryDistrictRowWidget>(this, RowClass);
	if (Row)
	{
		Row->InitializeOperationsView(View);
		Row->OnDistrictSelected.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleDistrictSelected);
		Row->OnGuardActionRequested.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleGuardActionRequested);
	}
	return Row;
}

void UTerritoryJournalWidget::UpdateSelectedDistrict(ATerritoryDistrict* District)
{
	SelectedDistrict = District;
	if (!District)
	{
		if (Text_EmptySelection)
		{
			Text_EmptySelection->SetVisibility(ESlateVisibility::Visible);
		}
		if (Text_CommandDistrictName)
		{
			Text_CommandDistrictName->SetText(NSLOCTEXT(
				"TerritoryJournal", "NoCommandSelection", "SELECT A DISTRICT"));
		}
		if (Btn_CommandAddGuard) Btn_CommandAddGuard->SetIsEnabled(false);
		if (Btn_CommandRemoveGuard) Btn_CommandRemoveGuard->SetIsEnabled(false);
		if (Btn_CommandAddFiveGuards) Btn_CommandAddFiveGuards->SetIsEnabled(false);
		if (Btn_CommandRemoveFiveGuards) Btn_CommandRemoveFiveGuards->SetIsEnabled(false);
		return;
	}
	FTerritoryDistrictOperationsView View;
	if (!UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, District, GetOwningPlayer(), View))
	{
		return;
	}

	if (Text_EmptySelection)
	{
		Text_EmptySelection->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Text_SelectedEyebrow)
	{
		Text_SelectedEyebrow->SetText(NSLOCTEXT("TerritoryJournal", "SelectedEyebrow", "DISTRICT COMMAND"));
	}
	if (QuestTitle)
	{
		QuestTitle->SetText(View.DisplayName);
	}

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	const FText StateText = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(View.TerritoryState))
		: FText::GetEmpty();
	const FText ReserveText = View.bReserveCountKnown
		? FText::AsNumber(View.ReserveGuards)
		: NSLOCTEXT("TerritoryJournal", "ReserveUnknown", "server snapshot required");
	const FText DetailText = FText::Format(
		NSLOCTEXT("TerritoryJournal", "DistrictDetails",
			"Owner: {0}\nState: {1}\nAvailability: {2}\n"
			"Garrison: {3} active / {4} assigned / {5} maximum\nReserve: {6}\n"
			"Guard purchase: {7}\nIncome: {8}\nUpkeep: {9}\nNet: {10}\nAvailable funds: {11}\n"
			"Security: quality {12}, fortification {13}, allied support {14}, strategic value {15}\nThreat: {16}"),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.OwnerFaction),
		StateText,
		View.AvailabilityReason,
		FText::AsNumber(View.ActiveGuards),
		FText::AsNumber(View.DesiredGuards),
		FText::AsNumber(View.MaximumGuards),
		ReserveText,
		FText::AsNumber(View.GuardPurchaseCost),
		FText::AsNumber(View.PeriodicIncome),
		FText::AsNumber(View.GuardUpkeep),
		FText::AsNumber(View.NetIncome),
		FText::AsNumber(View.AvailableFunds),
		FText::AsNumber(View.GuardQuality),
		FText::AsNumber(View.Fortification),
		FText::AsNumber(View.AlliedSupport),
		FText::AsNumber(View.StrategicValue),
		View.ThreatSummary);
	if (RichText_QuestDescription)
	{
		RichText_QuestDescription->SetText(DetailText);
	}
	if (Text_CaptureLabel)
	{
		Text_CaptureLabel->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "ControlLabel", "Control {0}%"),
			FText::AsNumber(FMath::RoundToInt(View.CaptureProgress * 100.f))));
	}
	if (CaptureProgressBar)
	{
		CaptureProgressBar->SetPercent(View.CaptureProgress);
	}
	if (RichText_CurrentStateDescription)
	{
		RichText_CurrentStateDescription->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CurrentState", "{0}  |  {1}  |  Attackers {2}  |  Planned force {3}"),
			StateText,
			UTerritoryUIBlueprintLibrary::GetThreatLevelText(View.ThreatLevel),
			View.bAttackerCountKnown
				? FText::AsNumber(View.ActiveAttackers)
				: NSLOCTEXT("TerritoryJournal", "AttackerCountUnknown", "unknown"),
			FText::AsNumber(View.PlannedAttackers)));
	}
	if (Text_OperationalSummary) Text_OperationalSummary->SetText(View.AvailabilityReason);
	if (Text_SecuritySummary)
	{
		Text_SecuritySummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "SecuritySummary", "Active {0} / Assigned {1} / Maximum {2} / Reserve {3}"),
			FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
			FText::AsNumber(View.MaximumGuards), ReserveText));
	}
	if (Text_FinanceSummary)
	{
		Text_FinanceSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "FinanceSummary", "Income {0} - Upkeep {1} = Net {2}"),
			FText::AsNumber(View.PeriodicIncome), FText::AsNumber(View.GuardUpkeep),
			FText::AsNumber(View.NetIncome)));
	}
	if (Text_AssaultSummary) Text_AssaultSummary->SetText(View.ThreatSummary);

	if (Text_CommandDistrictName) Text_CommandDistrictName->SetText(View.DisplayName);
	if (Text_CommandOwnerState)
	{
		Text_CommandOwnerState->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandOwnerState", "OWNER  {0}    |    STATE  {1}    |    VIEWER  {2}"),
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.OwnerFaction),
			StateText,
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ViewerFaction)));
	}
	if (Text_CommandAvailability)
	{
		Text_CommandAvailability->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandAvailability", "OPERATIONS\n{0}\nManagement: {1}"),
			View.AvailabilityReason,
			View.bManageable
				? NSLOCTEXT("TerritoryJournal", "ManagementReady", "Command authority confirmed")
				: View.ManagementFailureReason));
	}
	if (Text_CommandSecurity)
	{
		Text_CommandSecurity->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandSecurity",
				"SECURITY\nActive {0}  |  Assigned {1}  |  Capacity {2}  |  Reserve {3}\n"
				"Quality {4}  |  Fortification {5}  |  Allied support {6}  |  Strategic value {7}"),
			FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
			FText::AsNumber(View.MaximumGuards), ReserveText,
			FText::AsNumber(View.GuardQuality), FText::AsNumber(View.Fortification),
			FText::AsNumber(View.AlliedSupport), FText::AsNumber(View.StrategicValue)));
	}
	if (Text_CommandFinance)
	{
		Text_CommandFinance->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandFinance",
				"FINANCE\nFunds {0}  |  Guard price {1}\nIncome {2}  -  Upkeep {3}  =  Net {4}\nStatus: {5}"),
			FText::AsNumber(View.AvailableFunds), FText::AsNumber(View.GuardPurchaseCost),
			FText::AsNumber(View.PeriodicIncome), FText::AsNumber(View.GuardUpkeep),
			FText::AsNumber(View.NetIncome),
			View.bFinancialRisk
				? NSLOCTEXT("TerritoryJournal", "FinanceAtRisk", "AT RISK")
				: NSLOCTEXT("TerritoryJournal", "FinanceStable", "STABLE")));
	}
	if (Text_CommandThreat)
	{
		Text_CommandThreat->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandThreat",
				"THREAT  {0}\n{1}\nContesting faction: {2}  |  Capture attackers: {3}"),
			UTerritoryUIBlueprintLibrary::GetThreatLevelText(View.ThreatLevel),
			View.ThreatSummary,
			UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.ContestingFaction),
			View.bAttackerCountKnown
				? FText::AsNumber(View.ActiveAttackers)
				: NSLOCTEXT("TerritoryJournal", "CommandAttackersUnknown", "unknown")));
	}
	if (Text_CommandAssault)
	{
		Text_CommandAssault->SetText(View.AssaultID.IsValid()
			? FText::Format(
				NSLOCTEXT("TerritoryJournal", "CommandAssault",
					"ASSAULT  {0}\nAttacker {1}\nPlanned {2}  |  Alive {3}  |  Reserve {4}  |  Killed {5}  |  Withdrawn {6}\n"
					"Launch {7}  |  Estimated success {8}"),
				UTerritoryUIBlueprintLibrary::GetAssaultStateText(View.AssaultState),
				UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(View.AttackingFaction),
				FText::AsNumber(View.PlannedAttackers), FText::AsNumber(View.AliveAttackers),
				FText::AsNumber(View.PendingReserveAttackers), FText::AsNumber(View.KilledAttackers),
				FText::AsNumber(View.WithdrawnAttackers),
				FText::AsPercent(FMath::Clamp(View.LaunchProbability, 0.f, 1.f)),
				FText::AsPercent(FMath::Clamp(View.EstimatedSuccessProbability, 0.f, 1.f)))
			: NSLOCTEXT("TerritoryJournal", "NoCommandAssault", "ASSAULT\nNo scheduled operation."));
	}
	if (Text_CommandApproaches)
	{
		TArray<FString> ApproachNames;
		for (const FName Approach : View.SelectedApproaches)
		{
			ApproachNames.Add(Approach.ToString());
		}
		Text_CommandApproaches->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandApproaches", "APPROACHES\n{0}"),
			FText::FromString(ApproachNames.IsEmpty()
				? FString(TEXT("No assault routes selected."))
				: FString::Join(ApproachNames, TEXT("  |  ")))));
	}
	if (Text_CommandCaptureProgress)
	{
		Text_CommandCaptureProgress->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CommandCaptureProgress", "CONTROL PRESSURE  {0}%"),
			FText::AsNumber(FMath::RoundToInt(View.CaptureProgress * 100.f))));
	}
	if (CommandCaptureProgressBar) CommandCaptureProgressBar->SetPercent(View.CaptureProgress);

	AActor* ViewerActor = GetOwningPlayerPawn()
		? static_cast<AActor*>(GetOwningPlayerPawn())
		: static_cast<AActor*>(GetOwningPlayer());
	FText AddFiveFailure;
	FText RemoveFiveFailure;
	const bool bCanAddFive = View.bManageable
		&& District->CanPurchaseGuards(ViewerActor, 5, AddFiveFailure);
	const bool bCanRemoveFive = View.bManageable
		&& District->CanRemoveGuards(ViewerActor, 5, RemoveFiveFailure);
	if (!View.bManageable)
	{
		AddFiveFailure = View.ManagementFailureReason;
		RemoveFiveFailure = View.ManagementFailureReason;
	}
	if (Btn_CommandAddGuard)
	{
		Btn_CommandAddGuard->SetIsEnabled(View.bCanAddGuard);
		Btn_CommandAddGuard->SetToolTipText(View.bCanAddGuard
			? NSLOCTEXT("TerritoryJournal", "AddGuardReady", "Purchase and deploy one guard.")
			: View.AddGuardFailureReason);
	}
	if (Btn_CommandRemoveGuard)
	{
		Btn_CommandRemoveGuard->SetIsEnabled(View.bCanRemoveGuard);
		Btn_CommandRemoveGuard->SetToolTipText(View.bCanRemoveGuard
			? NSLOCTEXT("TerritoryJournal", "RemoveGuardReady", "Withdraw one active guard.")
			: View.RemoveGuardFailureReason);
	}
	if (Btn_CommandAddFiveGuards)
	{
		Btn_CommandAddFiveGuards->SetIsEnabled(bCanAddFive);
		Btn_CommandAddFiveGuards->SetToolTipText(bCanAddFive
			? NSLOCTEXT("TerritoryJournal", "AddFiveReady", "Purchase and deploy five guards atomically.")
			: AddFiveFailure);
	}
	if (Btn_CommandRemoveFiveGuards)
	{
		Btn_CommandRemoveFiveGuards->SetIsEnabled(bCanRemoveFive);
		Btn_CommandRemoveFiveGuards->SetToolTipText(bCanRemoveFive
			? NSLOCTEXT("TerritoryJournal", "RemoveFiveReady", "Withdraw five active guards atomically.")
			: RemoveFiveFailure);
	}
}

void UTerritoryJournalWidget::RefreshOperationalSummaries(
	const TArray<FTerritoryDistrictOperationsView>& Views)
{
	if (ActiveQuestsBox) ActiveQuestsBox->ClearChildren();
	if (FinishedQuestsBox) FinishedQuestsBox->ClearChildren();
	if (EarningsList) EarningsList->ClearChildren();
	if (LossReportList) LossReportList->ClearChildren();

	int32 OwnedCount = 0;
	int32 AvailableCount = 0;
	int32 ThreatCount = 0;
	int32 AvailableUnlockedCount = 0;
	int32 RiskCount = 0;
	int32 GuardShortfall = 0;
	int64 OwnedIncome = 0;
	int64 OwnedUpkeep = 0;
	FGameplayTag ViewerFaction;
	for (const FTerritoryDistrictOperationsView& View : Views)
	{
		if (!ViewerFaction.IsValid()) ViewerFaction = View.ViewerFaction;
		AvailableCount += View.bAvailable ? 1 : 0;
		ThreatCount += (View.bUnderAttack || View.bAttackScheduled) ? 1 : 0;
		if (UTerritoryUIBlueprintLibrary::IsDistrictAvailableUnlocked(View))
		{
			++AvailableUnlockedCount;
			if (ActiveQuestsBox)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					ActiveQuestsBox->AddChild(Row);
				}
			}
		}
		if (UTerritoryUIBlueprintLibrary::IsDistrictCapturedOwned(View))
		{
			++OwnedCount;
			OwnedIncome += View.PeriodicIncome;
			OwnedUpkeep += View.GuardUpkeep;
			GuardShortfall += FMath::Max(0, View.DesiredGuards - View.ActiveGuards);
			if (FinishedQuestsBox)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					FinishedQuestsBox->AddChild(Row);
				}
			}
			if (EarningsList)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					EarningsList->AddChild(Row);
				}
			}
		}
		if (View.bUnderAttack || View.bAttackScheduled || View.bFinancialRisk)
		{
			++RiskCount;
			if (LossReportList)
			{
				if (UTerritoryDistrictRowWidget* Row = CreateOperationsRow(View))
				{
					LossReportList->AddChild(Row);
				}
			}
		}
	}

	auto AddEmptyMessage = [this](UVerticalBox* Box, const FText& Message, FName WidgetName)
	{
		if (!Box || !WidgetTree)
		{
			return;
		}
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		EmptyText->SetText(Message);
		Box->AddChild(EmptyText);
	};
	if (AvailableUnlockedCount == 0)
	{
		AddEmptyMessage(ActiveQuestsBox,
			NSLOCTEXT("TerritoryJournal", "NoAvailableUnlocked", "No unlocked districts are currently available."),
			TEXT("NoAvailableUnlockedDistricts"));
	}
	if (OwnedCount == 0)
	{
		AddEmptyMessage(FinishedQuestsBox,
			NSLOCTEXT("TerritoryJournal", "NoCapturedOwned", "Your faction controls no loaded districts."),
			TEXT("NoCapturedOwnedDistricts"));
		AddEmptyMessage(EarningsList,
			NSLOCTEXT("TerritoryJournal", "NoOwnedEarnings", "Capture a district to establish recurring income."),
			TEXT("NoOwnedEarningsDistricts"));
	}
	if (RiskCount == 0)
	{
		AddEmptyMessage(LossReportList,
			NSLOCTEXT("TerritoryJournal", "NoOperationalRisks", "No district threats or operating risks detected."),
			TEXT("NoOperationalRiskDistricts"));
	}

	const FTerritoryEconomyOperationsView Economy =
		UTerritoryUIBlueprintLibrary::BuildEconomyOperationsView(
			this, GetOwningPlayer(), ViewerFaction, 10);
	if (Text_ActiveQuestCount)
	{
		Text_ActiveQuestCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "ActiveDistrictCount", "AVAILABLE / UNLOCKED  {0}"),
			FText::AsNumber(AvailableUnlockedCount)));
	}
	if (Text_FinishedQuestCount)
	{
		Text_FinishedQuestCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CapturedDistrictCount", "CAPTURED / OWNED  {0}"),
			FText::AsNumber(OwnedCount)));
	}
	if (Text_HeaderStatus)
	{
		Text_HeaderStatus->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "HeaderStatus", "{0} available  |  {1} owned  |  {2} threatened  |  {3} funds"),
			FText::AsNumber(AvailableUnlockedCount), FText::AsNumber(OwnedCount), FText::AsNumber(ThreatCount),
			FText::AsNumber(Economy.AvailableFunds)));
	}
	if (Text_AvailableUnlockedCount)
	{
		Text_AvailableUnlockedCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "AvailableKPI", "{0}\nAVAILABLE"),
			FText::AsNumber(AvailableUnlockedCount)));
	}
	if (Text_OwnedDistrictCount)
	{
		Text_OwnedDistrictCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "OwnedKPI", "{0}\nOWNED"),
			FText::AsNumber(OwnedCount)));
	}
	if (Text_ThreatenedDistrictCount)
	{
		Text_ThreatenedDistrictCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "ThreatenedKPI", "{0}\nTHREATENED"),
			FText::AsNumber(ThreatCount)));
	}
	if (Text_RiskDistrictCount)
	{
		Text_RiskDistrictCount->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "RiskKPI", "{0}\nAT RISK"),
			FText::AsNumber(RiskCount)));
	}
	if (Text_TotalWeeklyEarnings)
	{
		Text_TotalWeeklyEarnings->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "GrossIncomeKPI", "GROSS  {0} / CYCLE"),
			FText::AsNumber(OwnedIncome)));
	}
	if (Text_TotalGuardCost)
	{
		Text_TotalGuardCost->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "GuardUpkeepKPI", "GUARDS  {0} / CYCLE"),
			FText::AsNumber(OwnedUpkeep)));
	}
	if (Text_NetProfit)
	{
		Text_NetProfit->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "NetIncomeKPI", "NET  {0} / CYCLE"),
			FText::AsNumber(OwnedIncome - OwnedUpkeep)));
	}
	if (Text_TotalEarningsLost)
	{
		Text_TotalEarningsLost->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "OperationalRisk", "Recent debits {0}  |  {1} districts at risk"),
			FText::AsNumber(Economy.RecentDebits), FText::AsNumber(RiskCount)));
	}
	if (RichText_EarningsSummary)
	{
		RichText_EarningsSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "EarningsSummary",
				"Available funds: {0}\nFaction income: {1}\nOperating costs: {2}\nNet per cycle: {3}\nRecent credits: {4}\nOwned districts: {5}"),
			FText::AsNumber(Economy.AvailableFunds), FText::AsNumber(Economy.IncomePerTick),
			FText::AsNumber(Economy.CostsPerTick), FText::AsNumber(Economy.NetPerTick),
			FText::AsNumber(Economy.RecentCredits), FText::AsNumber(OwnedCount)));
	}
	if (RichText_LossSummary)
	{
		RichText_LossSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "LossSummary",
				"Recent debits: {0}\nOwned district upkeep: {1}\nOwned district net: {2}\nGuard shortfall: {3}\nThreatened districts: {4}\nCurrently available districts: {5}"),
			FText::AsNumber(Economy.RecentDebits), FText::AsNumber(OwnedUpkeep),
			FText::AsNumber(OwnedIncome - OwnedUpkeep), FText::AsNumber(GuardShortfall),
			FText::AsNumber(ThreatCount), FText::AsNumber(AvailableCount)));
	}
}

void UTerritoryJournalWidget::HandleTerritoryTabClicked()
{
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(0);
	}
}

void UTerritoryJournalWidget::HandleEarningsTabClicked()
{
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(1);
	}
}

void UTerritoryJournalWidget::HandleLossTabClicked()
{
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(2);
	}
}

void UTerritoryJournalWidget::HandleSearchChanged(const FText& Text)
{
	SearchFilter = Text.ToString();
	LastOperationsRevision = INDEX_NONE;
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleOwnerFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SelectedOwnerFilter = MoveTemp(SelectedItem);
	LastOperationsRevision = INDEX_NONE;
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleStateFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SelectedStateFilter = MoveTemp(SelectedItem);
	LastOperationsRevision = INDEX_NONE;
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleOperationalFilterChanged(
	FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SetOperationsFilter(GetOperationsFilter(SelectedItem));
}

void UTerritoryJournalWidget::HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)Territory;
	(void)bWasUnregistered;
	LastOperationsRevision = INDEX_NONE;
	RefreshFilterOptions();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)Territory;
	(void)bWasUnregistered;
	LastOperationsRevision = INDEX_NONE;
	RefreshFilterOptions();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleDistrictSelected(ATerritoryDistrict* District)
{
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(0);
	}
	UpdateSelectedDistrict(District);
}

void UTerritoryJournalWidget::HandleGuardActionRequested(ATerritoryDistrict* District, int32 Delta)
{
	if (!District)
	{
		return;
	}
	BindManagementComponent();
	if (!ManagementComponent.IsValid())
	{
		if (Text_CommandStatus)
		{
			Text_CommandStatus->SetText(NSLOCTEXT("TerritoryJournal", "ManagementUnavailable", "Territory management is unavailable."));
		}
		return;
	}

	if (Text_CommandStatus)
	{
		Text_CommandStatus->SetText(NSLOCTEXT("TerritoryJournal", "Submitting", "Submitting district command..."));
	}
	if (Delta > 0)
	{
		ManagementComponent->RequestPurchaseGuardsForDistrict(District, Delta);
	}
	else if (Delta < 0)
	{
		ManagementComponent->RequestRemoveGuardsForDistrict(District, -Delta);
	}
}

void UTerritoryJournalWidget::HandleCommandAddGuardClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), 1);
}

void UTerritoryJournalWidget::HandleCommandRemoveGuardClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), -1);
}

void UTerritoryJournalWidget::HandleCommandAddFiveGuardsClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), 5);
}

void UTerritoryJournalWidget::HandleCommandRemoveFiveGuardsClicked()
{
	HandleGuardActionRequested(SelectedDistrict.Get(), -5);
}

void UTerritoryJournalWidget::HandleGuardManagementResult(
	ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId)
{
	(void)bSuccess;
	(void)RequestId;
	if (Text_CommandStatus)
	{
		Text_CommandStatus->SetText(Message);
	}
	RefreshDistrictList();
	if (Territory && Territory == SelectedDistrict.Get())
	{
		UpdateSelectedDistrict(SelectedDistrict.Get());
	}
}

FTerritoryDistrictOperationsView UTerritoryJournalWidget::GetSelectedDistrictOperationsView() const
{
	FTerritoryDistrictOperationsView View;
	UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, SelectedDistrict.Get(), GetOwningPlayer(), View);
	return View;
}

void UTerritoryJournalWidget::SetOperationsFilter(ETerritoryOperationsFilter Filter)
{
	SelectedOperationsFilter = Filter;
	LastOperationsRevision = INDEX_NONE;
	if (DistrictOperationalFilter)
	{
		const FString Option = GetOperationsOption(Filter);
		if (DistrictOperationalFilter->GetSelectedOption() != Option)
		{
			DistrictOperationalFilter->SetSelectedOption(Option);
		}
	}
	RefreshDistrictList();
}

UWidget* UTerritoryJournalWidget::NativeGetDesiredFocusTarget() const
{
	if (Btn_TerritoryTab && Btn_TerritoryTab->GetIsEnabled() && Btn_TerritoryTab->IsVisible())
	{
		return Btn_TerritoryTab;
	}
	return Super::NativeGetDesiredFocusTarget();
}
