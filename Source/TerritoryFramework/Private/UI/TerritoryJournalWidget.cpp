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
#include "Widgets/NarrativeCommonButtonBase.h"
#include "TimerManager.h"

namespace
{
	const FString AllOwnersOption = TEXT("All owners");
	const FString AllStatesOption = TEXT("All states");

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
}

bool UTerritoryJournalWidget::PassesFilters(const ATerritoryDistrict* District) const
{
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

	return SelectedStateFilter.IsEmpty()
		|| SelectedStateFilter == AllStatesOption
		|| SelectedStateFilter == GetStateOption(District);
}

void UTerritoryJournalWidget::RefreshDistrictList()
{
	if (!DistrictList)
	{
		return;
	}
	if (!bFiltersInitialized)
	{
		RefreshFilterOptions();
	}

	DistrictList->ClearChildren();
	TArray<ATerritoryDistrict*> Districts = UTerritoryBlueprintLibrary::GetAllDistricts(this);
	Districts.Sort([](const ATerritoryDistrict& A, const ATerritoryDistrict& B)
	{
		return A.GetTerritoryDisplayName().ToString() < B.GetTerritoryDisplayName().ToString();
	});

	int32 VisibleCount = 0;
	ATerritoryDistrict* FirstVisibleDistrict = nullptr;
	for (ATerritoryDistrict* District : Districts)
	{
		if (!PassesFilters(District))
		{
			continue;
		}

		UTerritoryDistrictRowWidget* Row = CreateWidget<UTerritoryDistrictRowWidget>(
			this, UTerritoryDistrictRowWidget::StaticClass());
		if (!Row)
		{
			continue;
		}
		Row->InitializeDistrict(District);
		Row->OnDistrictSelected.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleDistrictSelected);
		Row->OnGuardActionRequested.AddUniqueDynamic(this, &UTerritoryJournalWidget::HandleGuardActionRequested);

		FText AddFailure;
		FText RemoveFailure;
		APlayerController* PlayerController = GetOwningPlayer();
		const bool bCanAdd = PlayerController && District->CanPurchaseGuards(PlayerController, 1, AddFailure);
		const bool bCanRemove = PlayerController && District->CanRemoveGuards(PlayerController, 1, RemoveFailure);
		Row->SetGuardActionState(bCanAdd, bCanRemove, bCanAdd || bCanRemove ? FText::GetEmpty() : AddFailure);
		DistrictList->AddChild(Row);

		if (!FirstVisibleDistrict)
		{
			FirstVisibleDistrict = District;
		}
		++VisibleCount;
	}

	if (VisibleCount == 0)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NoDistrictsMessage"));
		EmptyText->SetText(NSLOCTEXT("TerritoryJournal", "NoDistricts", "No districts match the current filters."));
		DistrictList->AddChild(EmptyText);
	}
	else if (!SelectedDistrict.IsValid() || !PassesFilters(SelectedDistrict.Get()))
	{
		UpdateSelectedDistrict(FirstVisibleDistrict);
	}
	else
	{
		UpdateSelectedDistrict(SelectedDistrict.Get());
	}

	if (Text_FilterSummary)
	{
		Text_FilterSummary->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "FilterSummary", "{0} districts"),
			FText::AsNumber(VisibleCount)));
	}
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
		QuestTitle->SetText(District->GetTerritoryDisplayName());
	}

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	const FText StateText = StateEnum
		? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(District->GetTerritoryState()))
		: FText::GetEmpty();
	const int64 GuardUpkeep = static_cast<int64>(District->GetGuardCost()) * District->GetDesiredGuardCount();
	const int64 Net = District->GetEffectiveIncome() - GuardUpkeep;
	const FText DetailText = FText::Format(
		NSLOCTEXT("TerritoryJournal", "DistrictDetails", "Owner: {0}\nState: {1}\nGarrison: {2}/{3}\nGuard cost: {4} each\nIncome: {5}\nNet: {6}"),
		UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(District->GetOwningFaction()),
		StateText,
		FText::AsNumber(District->GetDesiredGuardCount()),
		FText::AsNumber(District->GetMaxGuardCount()),
		FText::AsNumber(District->GetGuardCost()),
		FText::AsNumber(District->GetEffectiveIncome()),
		FText::AsNumber(Net));
	if (RichText_QuestDescription)
	{
		RichText_QuestDescription->SetText(DetailText);
	}
	if (Text_CaptureLabel)
	{
		Text_CaptureLabel->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "ControlLabel", "Control {0}%"),
			FText::AsNumber(FMath::RoundToInt(District->GetControlProgress() * 100.f))));
	}
	if (CaptureProgressBar)
	{
		CaptureProgressBar->SetPercent(District->GetControlProgress());
	}
	if (RichText_CurrentStateDescription)
	{
		RichText_CurrentStateDescription->SetText(FText::Format(
			NSLOCTEXT("TerritoryJournal", "CurrentState", "{0}  |  Defenders {1}"),
			StateText,
			FText::AsNumber(District->GetDefenderCount())));
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
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleOwnerFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SelectedOwnerFilter = MoveTemp(SelectedItem);
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleStateFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	SelectedStateFilter = MoveTemp(SelectedItem);
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleTerritoryRegistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)Territory;
	(void)bWasUnregistered;
	RefreshFilterOptions();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleTerritoryUnregistered(ATerritoryVolume* Territory, bool bWasUnregistered)
{
	(void)Territory;
	(void)bWasUnregistered;
	RefreshFilterOptions();
	RefreshDistrictList();
}

void UTerritoryJournalWidget::HandleDistrictSelected(ATerritoryDistrict* District)
{
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
