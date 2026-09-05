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
#include "UI/TerritoryUITheme.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/NarrativeCommonTextBlock.h"
#include "Widgets/NarrativeSpinBox.h"

namespace
{
	void StyleGeneratedManagementText(UTextBlock* Text, int32 FontSize,
		const FLinearColor& Color,
		ETerritoryTextRole Role = ETerritoryTextRole::Body)
	{
		TerritoryUITheme::ApplyText(Text, FontSize, Color, Role);
	}
}

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
	const FLinearColor PrimaryText(0.94f, 0.93f, 0.89f, 1.f);
	const FLinearColor MutedText(0.62f, 0.66f, 0.65f, 1.f);
	const FLinearColor AccentText(0.92f, 0.70f, 0.24f, 1.f);
	auto ThemeTextByName = [this](const FName Name, int32 FontSize,
		const FLinearColor& Color, ETerritoryTextRole Role,
		const FText& Replacement = FText::GetEmpty())
	{
		if (!WidgetTree) return;
		if (UTextBlock* Text = Cast<UTextBlock>(WidgetTree->FindWidget(Name)))
		{
			if (!Replacement.IsEmpty()) Text->SetText(Replacement);
			TerritoryUITheme::ApplyText(Text, FontSize, Color, Role);
		}
	};

	ThemeTextByName(TEXT("CommandLabel"), TerritoryTypography::Caption,
		AccentText, ETerritoryTextRole::Heading,
		NSLOCTEXT("TerritoryManagement", "CommandEyebrow", "District command"));
	ThemeTextByName(TEXT("DistrictNameText"), TerritoryTypography::PanelTitle,
		PrimaryText, ETerritoryTextRole::Title);
	ThemeTextByName(TEXT("StatusText"), TerritoryTypography::Metadata,
		MutedText, ETerritoryTextRole::Muted);
	ThemeTextByName(TEXT("SecurityHeading"), TerritoryTypography::SectionTitle,
		PrimaryText, ETerritoryTextRole::Heading,
		NSLOCTEXT("TerritoryManagement", "SecurityHeading", "Security and garrison"));
	ThemeTextByName(TEXT("EconomyHeading"), TerritoryTypography::SectionTitle,
		PrimaryText, ETerritoryTextRole::Heading,
		NSLOCTEXT("TerritoryManagement", "EconomyHeading", "Local economy"));
	for (const TPair<FName, FText>& Label : {
		TPair<FName, FText>(TEXT("OwnerRowLabel"),
			NSLOCTEXT("TerritoryManagement", "OwnerLabel", "Owner")),
		TPair<FName, FText>(TEXT("StateRowLabel"),
			NSLOCTEXT("TerritoryManagement", "StatusLabel", "Status")),
		TPair<FName, FText>(TEXT("TreasuryRowLabel"),
			NSLOCTEXT("TerritoryManagement", "FundsLabel", "Available funds")),
		TPair<FName, FText>(TEXT("EarningsRowLabel"),
			NSLOCTEXT("TerritoryManagement", "IncomeLabel", "Income / cycle")),
		TPair<FName, FText>(TEXT("GuardCostRowLabel"),
			NSLOCTEXT("TerritoryManagement", "GuardCostLabel", "Recruit / upkeep")) })
	{
		ThemeTextByName(Label.Key, TerritoryTypography::Metadata,
			AccentText, ETerritoryTextRole::Heading, Label.Value);
	}
	for (const FName ValueName : { FName(TEXT("OwnerText")), FName(TEXT("StateText")),
		FName(TEXT("TreasuryText")), FName(TEXT("EarningsText")),
		FName(TEXT("GuardCostText")) })
	{
		ThemeTextByName(ValueName, TerritoryTypography::Body,
			PrimaryText, ETerritoryTextRole::Body);
	}
	for (const FName DetailName : { FName(TEXT("GuardCountText")),
		FName(TEXT("ReserveGuardText")), FName(TEXT("ThreatText")),
		FName(TEXT("AvailabilityText")), FName(TEXT("NetIncomeText")),
		FName(TEXT("ProductionText")) })
	{
		ThemeTextByName(DetailName, TerritoryTypography::Metadata,
			MutedText, ETerritoryTextRole::Muted);
	}
	if (AddGuardButton)
	{
		TerritoryUITheme::ApplyButton(AddGuardButton);
		AddGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleAddGuardClicked);
		AddGuardButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "AddGuard", "Add guard"));
	}
	if (RemoveGuardButton)
	{
		TerritoryUITheme::ApplyButton(RemoveGuardButton);
		RemoveGuardButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleRemoveGuardClicked);
		RemoveGuardButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "RemoveGuard", "Remove guard"));
	}
	if (CloseButton)
	{
		TerritoryUITheme::ApplyButton(CloseButton);
		CloseButton->OnClicked().AddUObject(this, &UTerritoryDistrictManagementWidget::HandleCloseClicked);
		CloseButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "Close", "Close"));
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
		"Garrison staffing plan"));
	StyleGeneratedManagementText(Heading, TerritoryTypography::SectionTitle,
		FLinearColor(0.95f, 0.96f, 0.95f, 1.f), ETerritoryTextRole::Heading);
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
	StyleGeneratedManagementText(GarrisonTargetPreview, TerritoryTypography::Body,
		FLinearColor(0.90f, 0.89f, 0.85f, 1.f));
	Host->AddChild(GarrisonTargetPreview);

	const UTerritoryDeveloperSettings* Settings = GetDefault<UTerritoryDeveloperSettings>();
	TSubclassOf<UNarrativeCommonButtonBase> ButtonClass = Settings
		? Settings->DefaultNarrativeButtonClass.LoadSynchronous() : nullptr;
	auto ApplyTerritoryStyle = [](UNarrativeCommonButtonBase* Button)
	{
		TerritoryUITheme::ApplyButton(Button);
	};
	if (!ButtonClass) return;
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("LocalGarrisonTargetActions"));
	Host->AddChild(Actions);
	ApplyGuardTargetButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("ApplyLocalGuardTargetButton"));
	ApplyTerritoryStyle(ApplyGuardTargetButton);
	ApplyGuardTargetButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "ApplyLocalTarget", "Apply plan"));
	ApplyGuardTargetButton->SetToolTipText(NSLOCTEXT("TerritoryManagement", "ApplyLocalTargetTip",
		"Submit this exact staffing target to the authoritative server."));
	ApplyGuardTargetButton->OnClicked().AddUObject(this,
		&UTerritoryDistrictManagementWidget::HandleApplyGuardTargetClicked);
	Actions->AddChild(ApplyGuardTargetButton);
	ZeroGuardTargetButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("ZeroLocalGuardTargetButton"));
	ApplyTerritoryStyle(ZeroGuardTargetButton);
	ZeroGuardTargetButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "ZeroLocalTarget", "Empty post"));
	ZeroGuardTargetButton->SetToolTipText(NSLOCTEXT("TerritoryManagement", "ZeroLocalTargetTip",
		"Withdraw this garrison and reduce its future upkeep to zero."));
	ZeroGuardTargetButton->OnClicked().AddUObject(this,
		&UTerritoryDistrictManagementWidget::HandleZeroGuardTargetClicked);
	Actions->AddChild(ZeroGuardTargetButton);
	MaxGuardTargetButton = WidgetTree->ConstructWidget<UNarrativeCommonButtonBase>(
		ButtonClass, TEXT("MaxLocalGuardTargetButton"));
	ApplyTerritoryStyle(MaxGuardTargetButton);
	MaxGuardTargetButton->SetButtonText(NSLOCTEXT("TerritoryManagement", "MaxLocalTarget", "Full capacity"));
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
		const FString Option = FString::Printf(TEXT("%s — %s"),
			*Garrison.DisplayName.ToString(),
			Garrison.bDistrictGarrison ? TEXT("District command") : TEXT("Property post"));
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
					"{0}\nActive {1}  •  Assigned {2} -> {3}  •  Capacity {4}\n"
					"Reserve {5}  •  Deploying {6}  •  Recruitment {7}\n"
					"Income {9} / cycle  •  Upkeep {8} / cycle  •  Projected net {10}"),
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
		StateText->SetText(UTerritoryUIBlueprintLibrary::GetTerritoryStatusText(
			OperationsView.Availability, OperationsView.TerritoryState));
	}
	if (GuardCountText)
	{
		GuardCountText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "GuardCounts",
				"District defence  •  {0} active / {1} assigned / {2} capacity\n"
				"{3}  •  {4} active / {5} assigned / {6} capacity"),
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
				"Recruit {0} per guard  •  Upkeep {1} per guard / cycle"),
			FText::AsNumber(SelectedGarrison.RecruitmentCostPerGuard),
			FText::AsNumber(SelectedGarrison.UpkeepPerGuard)));
	}
	if (EarningsText) EarningsText->SetText(FText::AsNumber(OperationsView.PeriodicIncome));
	if (TreasuryText) TreasuryText->SetText(FText::AsNumber(OperationsView.AvailableFunds));
	if (NetIncomeText)
	{
		NetIncomeText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "NetIncome", "Net {0} / cycle"),
			FText::AsNumber(OperationsView.NetIncome)));
	}
	if (ReserveGuardText)
	{
		ReserveGuardText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "ReserveCount", "Reserve {0}  •  Deploying {1}"),
			FText::AsNumber(SelectedGarrison.ReserveGuards),
			FText::AsNumber(SelectedGarrison.PendingDeployments)));
	}
	if (ThreatText) ThreatText->SetText(OperationsView.ThreatSummary);
	if (AvailabilityText) AvailabilityText->SetText(OperationsView.AvailabilityReason);
	if (ProductionText)
	{
		ProductionText->SetText(FText::Format(
			NSLOCTEXT("TerritoryManagement", "ProductionSummary",
				"Production  •  {0} active  •  {1} blocked  •  {2} resource flows"),
			FText::AsNumber(OperationsView.ProducingSiteCount),
			FText::AsNumber(OperationsView.BlockedProductionSiteCount),
			FText::AsNumber(OperationsView.ResourceFlows.Num())));
	}

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
