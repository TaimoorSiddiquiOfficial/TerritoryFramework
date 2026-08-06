#include "UI/TerritoryDistrictManagementWidget.h"
#include "Interaction/TerritoryDistrictManagementPoint.h"
#include "Interaction/TerritoryPlayerManagementComponent.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryHierarchy.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/TerritoryUIBlueprintLibrary.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"
#include "Widgets/NarrativeSpinBox.h"

void UTerritoryDistrictManagementWidget::InitializeManagement(
	ATerritoryDistrictManagementPoint* InManagementPoint)
{
	ManagementPoint = InManagementPoint;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		ManagedFaction = UTerritoryBlueprintLibrary::GetActorPrimaryFaction(this, PlayerController->GetPawn());
	}
	BindManagementComponent();
	RefreshManagementDisplay();
}

void UTerritoryDistrictManagementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (AddGuardButton)
	{
		AddGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleAddGuardClicked);
		AddGuardButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "AddGuard", "ADD GUARD"));
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleRemoveGuardClicked);
		RemoveGuardButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "RemoveGuard", "REMOVE GUARD"));
	}
	if (CloseButton)
	{
		CloseButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleCloseClicked);
		CloseButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "Close", "CLOSE"));
	}
	BuildGarrisonControls();
	BindManagementComponent();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this,
			&UTerritoryDistrictManagementWidget::RefreshManagementDisplay, 0.5f, true);
	}
}

void UTerritoryDistrictManagementWidget::NativeDestruct()
{
	if (AddGuardButton)
	{
		AddGuardButton->OnClicked().RemoveAll(this);
	}
	if (RemoveGuardButton)
	{
		RemoveGuardButton->OnClicked().RemoveAll(this);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
	}
	if (GarrisonTargetSelector)
	{
		GarrisonTargetSelector->OnSelectionChanged.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGarrisonTargetChanged);
	}
	if (GuardTargetSpinBox)
	{
		GuardTargetSpinBox->OnValueChanged.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardTargetSpinChanged);
	}
	if (ApplyGuardTargetButton) ApplyGuardTargetButton->OnClicked().RemoveAll(this);
	if (ZeroGuardTargetButton) ZeroGuardTargetButton->OnClicked().RemoveAll(this);
	if (MaxGuardTargetButton) MaxGuardTargetButton->OnClicked().RemoveAll(this);
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UTerritoryDistrictManagementWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshManagementDisplay();
}

void UTerritoryDistrictManagementWidget::BuildGarrisonControls()
{
	if (GarrisonTargetSelector || !WidgetTree) return;
	UPanelWidget* Host = nullptr;
	for (const FName Candidate : { FName(TEXT("ManagementStack")), FName(TEXT("ContentStack")),
		FName(TEXT("CommandStack")) })
	{
		if (UVerticalBox* Box = Cast<UVerticalBox>(WidgetTree->FindWidget(Candidate)))
		{
			Host = Box;
			break;
		}
	}
	if (!Host)
	{
		TArray<UWidget*> Widgets;
		WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (UVerticalBox* Box = Cast<UVerticalBox>(Widget))
			{
				Host = Box;
				break;
			}
		}
	}
	if (!Host) return;

	UTextBlock* Heading = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_LocalGarrisonHeading"));
	Heading->SetText(NSLOCTEXT("TerritoryManagement", "LocalGarrisonHeading",
		"GARRISON TARGET & LOCAL PROFIT / LOSS"));
	Host->AddChild(Heading);
	GarrisonTargetSelector = WidgetTree->ConstructWidget<UNarrativeComboBoxString>(
		UNarrativeComboBoxString::StaticClass(), TEXT("LocalGarrisonTargetSelector"));
	GarrisonTargetSelector->SetToolTipText(NSLOCTEXT("TerritoryManagement", "GarrisonSelectorTip",
		"Select the managed District or one of its registered child Property garrisons."));
	GarrisonTargetSelector->OnSelectionChanged.AddUniqueDynamic(
		this, &UTerritoryDistrictManagementWidget::HandleGarrisonTargetChanged);
	Host->AddChild(GarrisonTargetSelector);
	GuardTargetSpinBox = WidgetTree->ConstructWidget<UNarrativeSpinBox>(
		UNarrativeSpinBox::StaticClass(), TEXT("LocalGuardTargetSpinBox"));
	GuardTargetSpinBox->SetToolTipText(NSLOCTEXT("TerritoryManagement", "GuardTargetTip",
		"Set the exact assigned target and review recruitment, upkeep, and net yield."));
	GuardTargetSpinBox->SetMinValue(0.f);
	GuardTargetSpinBox->SetMinSliderValue(0.f);
	GuardTargetSpinBox->SetDelta(1.f);
	GuardTargetSpinBox->SetMinFractionalDigits(0);
	GuardTargetSpinBox->SetMaxFractionalDigits(0);
	GuardTargetSpinBox->OnValueChanged.AddUniqueDynamic(
		this, &UTerritoryDistrictManagementWidget::HandleGuardTargetSpinChanged);
	Host->AddChild(GuardTargetSpinBox);
	GarrisonTargetPreview = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(
		UNarrativeCommonTextBlock::StaticClass(), TEXT("Text_LocalGarrisonPreview"));
	GarrisonTargetPreview->SetAutoWrapText(true);
	Host->AddChild(GarrisonTargetPreview);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	TSubclassOf<UNarrativeCommonButtonBase> ButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
	if (!ButtonClass) return;
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("LocalGarrisonTargetActions"));
	Host->AddChild(Actions);
	ApplyGuardTargetButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("ApplyLocalGuardTargetButton"));
	ApplyGuardTargetButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "ApplyLocalTarget", "APPLY"));
	ApplyGuardTargetButton->SetToolTipText(NSLOCTEXT("TerritoryManagement", "ApplyLocalTargetTip",
		"Submit this exact staffing target to the authoritative server."));
	ApplyGuardTargetButton->OnClicked().AddUObject(this,
		&UTerritoryDistrictManagementWidget::HandleApplyGuardTargetClicked);
	Actions->AddChild(ApplyGuardTargetButton);
	ZeroGuardTargetButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("ZeroLocalGuardTargetButton"));
	ZeroGuardTargetButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "ZeroLocalTarget", "SET 0"));
	ZeroGuardTargetButton->SetToolTipText(NSLOCTEXT("TerritoryManagement", "ZeroLocalTargetTip",
		"Withdraw this garrison and reduce its future upkeep to zero."));
	ZeroGuardTargetButton->OnClicked().AddUObject(this,
		&UTerritoryDistrictManagementWidget::HandleZeroGuardTargetClicked);
	Actions->AddChild(ZeroGuardTargetButton);
	MaxGuardTargetButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("MaxLocalGuardTargetButton"));
	MaxGuardTargetButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "MaxLocalTarget", "SET MAX"));
	MaxGuardTargetButton->SetToolTipText(NSLOCTEXT("TerritoryManagement", "MaxLocalTargetTip",
		"Set this garrison to its authored physical capacity."));
	MaxGuardTargetButton->OnClicked().AddUObject(this,
		&UTerritoryDistrictManagementWidget::HandleMaxGuardTargetClicked);
	Actions->AddChild(MaxGuardTargetButton);
}

ATerritoryDistrict* UTerritoryDistrictManagementWidget::GetManagedDistrict() const
{
	return ManagementPoint.IsValid() ? ManagementPoint->ResolveDistrict() : nullptr;
}

FGameplayTag UTerritoryDistrictManagementWidget::GetManagedFaction() const
{
	return ManagedFaction;
}

int32 UTerritoryDistrictManagementWidget::GetDistrictIncome() const
{
	const ATerritoryDistrict* District = GetManagedDistrict();
	return District ? District->GetEffectiveIncome() : 0;
}

bool UTerritoryDistrictManagementWidget::CanPurchaseGuard(FText& OutFailureReason) const
{
	const ATerritoryVolume* Target = SelectedGarrisonTarget.Get();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!ManagementComponent.IsValid())
	{
		OutFailureReason = FText::FromString(TEXT("Territory management is not installed on this PlayerController."));
		return false;
	}
	if (!ManagementPoint.IsValid() || !PlayerController
		|| !ManagementPoint->CanManage(PlayerController->GetPawn(), OutFailureReason))
	{
		return false;
	}
	if (!ManagementPoint->IsInteractorInRange(PlayerController->GetPawn()))
	{
		OutFailureReason = FText::FromString(TEXT("Move closer to the district management point."));
		return false;
	}
	if (!Target)
	{
		OutFailureReason = FText::FromString(TEXT("No district or Property garrison is selected."));
		return false;
	}
	int32 RecruitmentCost = 0;
	return Target->CanSetDesiredGuardCount(PlayerController->GetPawn(),
		Target->GetDesiredGuardCount() + 1, OutFailureReason, RecruitmentCost);
}

bool UTerritoryDistrictManagementWidget::CanRemoveGuard(FText& OutFailureReason) const
{
	const ATerritoryVolume* Target = SelectedGarrisonTarget.Get();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!ManagementComponent.IsValid())
	{
		OutFailureReason = FText::FromString(TEXT("Territory management is not installed on this PlayerController."));
		return false;
	}
	if (!ManagementPoint.IsValid() || !PlayerController || !ManagementPoint->CanManage(PlayerController->GetPawn(), OutFailureReason))
	{
		return false;
	}
	if (!ManagementPoint->IsInteractorInRange(PlayerController->GetPawn()))
	{
		OutFailureReason = FText::FromString(TEXT("Move closer to the district management point."));
		return false;
	}
	if (!Target)
	{
		OutFailureReason = FText::FromString(TEXT("No district or Property garrison is selected."));
		return false;
	}
	int32 RecruitmentCost = 0;
	return Target->CanSetDesiredGuardCount(PlayerController->GetPawn(),
		Target->GetDesiredGuardCount() - 1, OutFailureReason, RecruitmentCost);
}

void UTerritoryDistrictManagementWidget::RefreshGarrisonControls()
{
	TWeakObjectPtr<ATerritoryVolume> Preferred = SelectedGarrisonTarget;
	SelectedGarrisonTarget.Reset();
	TMap<FString, TWeakObjectPtr<ATerritoryVolume>> NewOptions;
	TArray<FString> NewOptionOrder;
	for (const FTerritoryGarrisonOperationsView& Garrison : OperationsView.GarrisonTargets)
	{
		if (!Garrison.Territory) continue;
		const FString Option = FString::Printf(TEXT("%s — %s [%s]"),
			*Garrison.DisplayName.ToString(),
			Garrison.bDistrictGarrison ? TEXT("District") : TEXT("Property"),
			*Garrison.TerritoryTag.ToString());
		NewOptions.Add(Option, Garrison.Territory);
		NewOptionOrder.Add(Option);
		if (Preferred.Get() == Garrison.Territory) SelectedGarrisonTarget = Garrison.Territory;
	}
	bool bOptionsChanged = NewOptions.Num() != GarrisonTargetOptions.Num();
	if (!bOptionsChanged)
	{
		for (const TPair<FString, TWeakObjectPtr<ATerritoryVolume>>& Pair : NewOptions)
		{
			const TWeakObjectPtr<ATerritoryVolume>* Existing = GarrisonTargetOptions.Find(Pair.Key);
			if (!Existing || *Existing != Pair.Value)
			{
				bOptionsChanged = true;
				break;
			}
		}
	}
	GarrisonTargetOptions = MoveTemp(NewOptions);
	if (GarrisonTargetSelector && bOptionsChanged)
	{
		GarrisonTargetSelector->ClearOptions();
		for (const FString& Option : NewOptionOrder)
		{
			GarrisonTargetSelector->AddOption(Option);
		}
	}
	if (!SelectedGarrisonTarget.IsValid())
	{
		for (const FTerritoryGarrisonOperationsView& Garrison : OperationsView.GarrisonTargets)
		{
			if (Garrison.bManageable && Garrison.MaximumGuards > 0)
			{
				SelectedGarrisonTarget = Garrison.Territory;
				break;
			}
		}
	}
	if (!SelectedGarrisonTarget.IsValid() && OperationsView.GarrisonTargets.Num() > 0)
	{
		SelectedGarrisonTarget = OperationsView.GarrisonTargets[0].Territory;
	}
	if (GarrisonTargetSelector && SelectedGarrisonTarget.IsValid())
	{
		for (const TPair<FString, TWeakObjectPtr<ATerritoryVolume>>& Pair : GarrisonTargetOptions)
		{
			if (Pair.Value == SelectedGarrisonTarget)
			{
				GarrisonTargetSelector->SetSelectedOption(Pair.Key);
				break;
			}
		}
	}
	if (GuardTargetSpinBox)
	{
		const ATerritoryVolume* Target = SelectedGarrisonTarget.Get();
		const float Maximum = Target ? Target->GetMaxGuardCount() : 0.f;
		GuardTargetSpinBox->SetMaxValue(Maximum);
		GuardTargetSpinBox->SetMaxSliderValue(Maximum);
		GuardTargetSpinBox->SetValue(Target ? Target->GetDesiredGuardCount() : 0.f);
		GuardTargetSpinBox->SetIsEnabled(Target && Maximum > 0.f);
	}
	UpdateGarrisonPreview();
}

void UTerritoryDistrictManagementWidget::UpdateGarrisonPreview()
{
	FTerritoryGarrisonOperationsView View;
	const bool bHasTarget = UTerritoryUIBlueprintLibrary::BuildGarrisonOperationsView(
		this, SelectedGarrisonTarget.Get(), GetOwningPlayer(), View);
	const int32 Proposed = GuardTargetSpinBox
		? FMath::RoundToInt(GuardTargetSpinBox->GetValue()) : View.DesiredGuards;
	if (GarrisonTargetPreview)
	{
		if (!bHasTarget)
		{
			GarrisonTargetPreview->SetText(NSLOCTEXT("TerritoryManagement", "NoLocalGarrison",
				"No loaded garrison is available."));
		}
		else
		{
			const int32 Recruitment = FMath::Max(0, Proposed - View.DesiredGuards)
				* View.RecruitmentCostPerGuard;
			const int64 Upkeep = static_cast<int64>(Proposed) * View.UpkeepPerGuard;
			GarrisonTargetPreview->SetText(FText::Format(
				NSLOCTEXT("TerritoryManagement", "LocalGarrisonPreview",
					"{0}: active {1}, target {2} -> {3}, max {4}, reserve {5}, pending {6}\n"
					"Recruitment {7}; upkeep {8}/cycle; income {9}; net {10}"),
				View.DisplayName, FText::AsNumber(View.ActiveGuards), FText::AsNumber(View.DesiredGuards),
				FText::AsNumber(Proposed), FText::AsNumber(View.MaximumGuards),
				FText::AsNumber(View.ReserveGuards), FText::AsNumber(View.PendingDeployments),
				FText::AsNumber(Recruitment), FText::AsNumber(Upkeep),
				FText::AsNumber(View.PeriodicIncome), FText::AsNumber(View.PeriodicIncome - Upkeep)));
		}
	}
	const bool bCanApply = bHasTarget && View.bManageable
		&& Proposed != View.DesiredGuards && Proposed >= 0 && Proposed <= View.MaximumGuards;
	if (ApplyGuardTargetButton) ApplyGuardTargetButton->SetIsEnabled(bCanApply);
	if (ZeroGuardTargetButton) ZeroGuardTargetButton->SetIsEnabled(
		bHasTarget && View.bManageable && View.DesiredGuards > 0);
	if (MaxGuardTargetButton) MaxGuardTargetButton->SetIsEnabled(
		bHasTarget && View.bManageable && View.DesiredGuards < View.MaximumGuards);
}

void UTerritoryDistrictManagementWidget::RefreshManagementDisplay()
{
	ATerritoryDistrict* District = GetManagedDistrict();
	if (!District)
	{
		OperationsView = FTerritoryDistrictOperationsView();
		if (StatusText)
		{
			StatusText->SetText(NSLOCTEXT("TerritoryManagement", "DistrictUnavailable", "District is not loaded or registered."));
		}
		return;
	}

	UTerritoryUIBlueprintLibrary::BuildDistrictOperationsView(
		this, District, GetOwningPlayer(), OperationsView);
	RefreshGarrisonControls();
	FTerritoryGarrisonOperationsView SelectedGarrison;
	const bool bHasSelectedGarrison = UTerritoryUIBlueprintLibrary::BuildGarrisonOperationsView(
		this, SelectedGarrisonTarget.Get(), GetOwningPlayer(), SelectedGarrison);

	if (DistrictNameText) DistrictNameText->SetText(OperationsView.DisplayName);
	if (OwnerText) OwnerText->SetText(UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName(OperationsView.OwnerFaction));
	if (StateText)
	{
		const UEnum* StateEnum = StaticEnum<ETerritoryState>();
		StateText->SetText(StateEnum ? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(OperationsView.TerritoryState)) : FText::GetEmpty());
	}
	if (GuardCountText)
	{
		GuardCountText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "GuardCounts",
				"District total: active {0} / target {1} / capacity {2}\nSelected {3}: active {4} / target {5} / capacity {6}"),
			FText::AsNumber(OperationsView.ActiveGuards), FText::AsNumber(OperationsView.DesiredGuards),
			FText::AsNumber(OperationsView.MaximumGuards),
			bHasSelectedGarrison ? SelectedGarrison.DisplayName : FText::GetEmpty(),
			FText::AsNumber(SelectedGarrison.ActiveGuards), FText::AsNumber(SelectedGarrison.DesiredGuards),
			FText::AsNumber(SelectedGarrison.MaximumGuards)));
	}
	if (GuardCostText)
	{
		GuardCostText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "GuardCosts",
				"Recruit {0} each | Upkeep {1} each / cycle"),
			FText::AsNumber(SelectedGarrison.RecruitmentCostPerGuard),
			FText::AsNumber(SelectedGarrison.UpkeepPerGuard)));
	}
	if (EarningsText) EarningsText->SetText(FText::AsNumber(OperationsView.PeriodicIncome));
	if (TreasuryText) TreasuryText->SetText(FText::AsNumber(OperationsView.AvailableFunds));
	if (NetIncomeText)
	{
		NetIncomeText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "NetIncome", "Net per cycle: {0}"),
			FText::AsNumber(OperationsView.NetIncome)));
	}
	if (ReserveGuardText)
	{
		ReserveGuardText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "ReserveCount", "Reserve: {0} | Pending: {1}"),
			FText::AsNumber(SelectedGarrison.ReserveGuards),
			FText::AsNumber(SelectedGarrison.PendingDeployments)));
	}
	if (ThreatText) ThreatText->SetText(OperationsView.ThreatSummary);
	if (AvailabilityText) AvailabilityText->SetText(OperationsView.AvailabilityReason);

	FText FailureReason;
	const bool bCanPurchase = CanPurchaseGuard(FailureReason);
	if (AddGuardButton) AddGuardButton->SetIsEnabled(bCanPurchase);
	FText RemoveFailureReason;
	const bool bCanRemove = CanRemoveGuard(RemoveFailureReason);
	if (RemoveGuardButton) RemoveGuardButton->SetIsEnabled(bCanRemove);
	if (StatusText)
	{
		const FText DisabledReason = !FailureReason.IsEmpty() ? FailureReason : RemoveFailureReason;
		StatusText->SetText(bCanPurchase || bCanRemove ? FText::GetEmpty() : DisabledReason);
	}
	OnManagementRefreshed();
}

void UTerritoryDistrictManagementWidget::HandleAddGuardClicked()
{
	RequestAddGuards(1);
}

void UTerritoryDistrictManagementWidget::HandleRemoveGuardClicked()
{
	RequestRemoveGuards(1);
}

void UTerritoryDistrictManagementWidget::HandleGarrisonTargetChanged(
	FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectionType;
	if (const TWeakObjectPtr<ATerritoryVolume>* Target = GarrisonTargetOptions.Find(SelectedItem))
	{
		SelectedGarrisonTarget = *Target;
		if (GuardTargetSpinBox && SelectedGarrisonTarget.IsValid())
		{
			const float Maximum = SelectedGarrisonTarget->GetMaxGuardCount();
			GuardTargetSpinBox->SetMaxValue(Maximum);
			GuardTargetSpinBox->SetMaxSliderValue(Maximum);
			GuardTargetSpinBox->SetValue(SelectedGarrisonTarget->GetDesiredGuardCount());
			GuardTargetSpinBox->SetIsEnabled(Maximum > 0.f);
		}
		UpdateGarrisonPreview();
	}
}

void UTerritoryDistrictManagementWidget::HandleGuardTargetSpinChanged(float NewValue)
{
	(void)NewValue;
	UpdateGarrisonPreview();
}

void UTerritoryDistrictManagementWidget::HandleApplyGuardTargetClicked()
{
	if (GuardTargetSpinBox)
	{
		SubmitGuardTarget(FMath::RoundToInt(GuardTargetSpinBox->GetValue()));
	}
}

void UTerritoryDistrictManagementWidget::HandleZeroGuardTargetClicked()
{
	SubmitGuardTarget(0);
}

void UTerritoryDistrictManagementWidget::HandleMaxGuardTargetClicked()
{
	if (SelectedGarrisonTarget.IsValid())
	{
		SubmitGuardTarget(SelectedGarrisonTarget->GetMaxGuardCount());
	}
}

void UTerritoryDistrictManagementWidget::SubmitGuardTarget(int32 NewDesiredGuardCount)
{
	BindManagementComponent();
	if (!ManagementComponent.IsValid() || !ManagementPoint.IsValid()
		|| !SelectedGarrisonTarget.IsValid())
	{
		if (StatusText) StatusText->SetText(NSLOCTEXT("TerritoryManagement", "GarrisonContextUnavailable",
			"Garrison management context is unavailable."));
		return;
	}
	if (StatusText) StatusText->SetText(NSLOCTEXT("TerritoryManagement", "SettingGuardTarget",
		"Submitting garrison staffing target..."));
	ManagementComponent->RequestSetGuardTarget(ManagementPoint.Get(),
		SelectedGarrisonTarget.Get(), NewDesiredGuardCount);
}

void UTerritoryDistrictManagementWidget::RequestAddGuards(int32 Count)
{
	if (Count <= 0 || !SelectedGarrisonTarget.IsValid())
	{
		if (StatusText) StatusText->SetText(NSLOCTEXT("TerritoryManagement", "InvalidGuardIncrease",
			"Select a garrison and enter a positive increase."));
		return;
	}
	SubmitGuardTarget(SelectedGarrisonTarget->GetDesiredGuardCount() + Count);
}

void UTerritoryDistrictManagementWidget::RequestRemoveGuards(int32 Count)
{
	if (Count <= 0 || !SelectedGarrisonTarget.IsValid())
	{
		if (StatusText) StatusText->SetText(NSLOCTEXT("TerritoryManagement", "InvalidGuardReduction",
			"Select a garrison and enter a positive reduction."));
		return;
	}
	SubmitGuardTarget(SelectedGarrisonTarget->GetDesiredGuardCount() - Count);
}

void UTerritoryDistrictManagementWidget::HandleCloseClicked()
{
	CloseTerritoryWidget();
}

void UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult(
	ATerritoryVolume* Territory, bool bSuccess, FText Message, int32 RequestId)
{
	(void)RequestId;
	if (!Territory || Territory == GetManagedDistrict() || Territory == SelectedGarrisonTarget.Get())
	{
		if (StatusText) StatusText->SetText(Message);
		RefreshManagementDisplay();
	}
}

void UTerritoryDistrictManagementWidget::BindManagementComponent()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UTerritoryPlayerManagementComponent* Component = PlayerController
		? UTerritoryPlayerManagementComponent::FindOrCreateForPlayerController(PlayerController)
		: nullptr;
	if (ManagementComponent.Get() == Component) return;

	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.RemoveDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
	ManagementComponent = Component;
	if (ManagementComponent.IsValid())
	{
		ManagementComponent->OnGuardPurchaseResult.AddUniqueDynamic(
			this, &UTerritoryDistrictManagementWidget::HandleGuardPurchaseResult);
	}
}

UWidget* UTerritoryDistrictManagementWidget::NativeGetDesiredFocusTarget() const
{
	if (AddGuardButton && AddGuardButton->GetIsEnabled() && AddGuardButton->IsVisible())
	{
		return AddGuardButton;
	}
	if (RemoveGuardButton && RemoveGuardButton->GetIsEnabled() && RemoveGuardButton->IsVisible())
	{
		return RemoveGuardButton;
	}
	if (CloseButton && CloseButton->GetIsEnabled() && CloseButton->IsVisible())
	{
		return CloseButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}
