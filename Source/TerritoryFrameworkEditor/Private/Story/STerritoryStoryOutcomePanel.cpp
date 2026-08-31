#include "Story/STerritoryStoryOutcomePanel.h"

#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryStealthProfile.h"
#include "Economy/TerritoryProductionProfile.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Story/TerritoryStoryOutcomeAnalyzer.h"
#include "Styling/AppStyle.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FSlateColor CertaintyColor(ETerritoryStoryOutcomeCertainty Certainty)
	{
		switch (Certainty)
		{
		case ETerritoryStoryOutcomeCertainty::Configured:
			return FSlateColor(FLinearColor(0.32f, 0.78f, 0.55f));
		case ETerritoryStoryOutcomeCertainty::RuntimeConditional:
			return FSlateColor(FLinearColor(0.95f, 0.78f, 0.30f));
		case ETerritoryStoryOutcomeCertainty::ChanceBased:
			return FSlateColor(FLinearColor(0.95f, 0.55f, 0.22f));
		case ETerritoryStoryOutcomeCertainty::CustomBlueprint:
			return FSlateColor(FLinearColor(0.38f, 0.68f, 0.95f));
		case ETerritoryStoryOutcomeCertainty::Warning:
		default:
			return FSlateColor(FLinearColor(0.95f, 0.30f, 0.25f));
		}
	}

	bool IsRelevantOutcomeObject(const UObject* Object)
	{
		return Object && (Object->IsA<UTerritoryDefinition>()
			|| Object->IsA<UTerritoryCounterAttackProfile>()
			|| Object->IsA<UTerritoryProductionProfile>()
			|| Object->IsA<UTerritoryStealthProfile>()
			|| Object->IsA<UNarrativeCondition>()
			|| Object->IsA<UNarrativeEvent>());
	}
}

void STerritoryStoryOutcomePanel::Construct(const FArguments& InArgs)
{
	Definition = InArgs._Definition;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(FMargin(10.f, 8.f))
		[
			SAssignNew(Content, SVerticalBox)
		]
	];

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(
		this, &STerritoryStoryOutcomePanel::HandleObjectPropertyChanged);
	Rebuild();
}

STerritoryStoryOutcomePanel::~STerritoryStoryOutcomePanel()
{
	if (PropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	}
}

void STerritoryStoryOutcomePanel::HandleObjectPropertyChanged(UObject* Object,
	FPropertyChangedEvent& PropertyChangedEvent)
{
	UTerritoryDefinition* Current = Definition.Get();
	if (!Current || !Object) return;
	if (Object != Current && !Object->IsIn(Current) && !IsRelevantOutcomeObject(Object))
	{
		return;
	}
	if (bRefreshScheduled) return;
	bRefreshScheduled = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(
		this, &STerritoryStoryOutcomePanel::HandleDeferredRefresh));
}

EActiveTimerReturnType STerritoryStoryOutcomePanel::HandleDeferredRefresh(
	double CurrentTime, float DeltaTime)
{
	bRefreshScheduled = false;
	Rebuild();
	return EActiveTimerReturnType::Stop;
}

void STerritoryStoryOutcomePanel::AddExplanationRow(
	const TSharedRef<SVerticalBox>& Box, const FString& Label,
	const FString& Value) const
{
	if (Value.IsEmpty()) return;
	Box->AddSlot()
	.AutoHeight()
	.Padding(0.f, 2.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label + TEXT(":")))
			.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.78f, 0.82f)))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Value))
			.AutoWrapText(true)
			.WrapTextAt(980.f)
		]
	];
}

TSharedRef<SWidget> STerritoryStoryOutcomePanel::BuildScenarioBody(
	const FTerritoryStoryOutcomeScenario& Scenario) const
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	AddExplanationRow(Box, TEXT("When"), Scenario.When);
	AddExplanationRow(Box, TEXT("Only if"), Scenario.OnlyIf);
	AddExplanationRow(Box, TEXT("Then"), Scenario.Then);
	AddExplanationRow(Box, TEXT("If not"), Scenario.IfNot);
	AddExplanationRow(Box, TEXT("Also affects"), Scenario.AlsoAffects);
	AddExplanationRow(Box, TEXT("Based on"), Scenario.Source);
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(FMargin(10.f, 7.f))
		[Box];
}

void STerritoryStoryOutcomePanel::Rebuild()
{
	if (!Content.IsValid()) return;
	Content->ClearChildren();
	const UTerritoryDefinition* Current = Definition.Get();
	const FTerritoryStoryOutcomeReport Report =
		FTerritoryStoryOutcomeAnalyzer::Analyze(Current, true);

	Content->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  |  %s"),
					*Report.DefinitionName, *Report.DefinitionType)))
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Report.Headline))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.68f, 0.72f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(10.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Copy Report")))
			.ToolTipText(FText::FromString(
				TEXT("Copy the generated read-only story report. This does not change the asset.")))
			.OnClicked(this, &STerritoryStoryOutcomePanel::CopyReportToClipboard)
		]
	];

	FString LastCategory;
	for (int32 Index = 0; Index < Report.Scenarios.Num(); ++Index)
	{
		const FTerritoryStoryOutcomeScenario& Scenario = Report.Scenarios[Index];
		if (Scenario.Category != LastCategory)
		{
			LastCategory = Scenario.Category;
			Content->AddSlot()
			.AutoHeight()
			.Padding(0.f, Index == 0 ? 3.f : 10.f, 0.f, 3.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(LastCategory.ToUpper()))
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.82f, 0.90f)))
			];
		}

		Content->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(Index > 1)
			.BorderImage(FAppStyle::GetBrush(TEXT("DetailsView.CategoryTop")))
			.HeaderContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Scenario.Title))
					.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.f, 0.f, 4.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(
						FTerritoryStoryOutcomeAnalyzer::CertaintyLabel(
							Scenario.Certainty)))
					.ColorAndOpacity(CertaintyColor(Scenario.Certainty))
				]
			]
			.BodyContent()
			[
				BuildScenarioBody(Scenario)
			]
		];
	}

	if (!Report.ValidationErrors.IsEmpty() || !Report.ValidationWarnings.IsEmpty())
	{
		TSharedRef<SVerticalBox> Health = SNew(SVerticalBox);
		for (const FString& Error : Report.ValidationErrors)
		{
			Health->AddSlot().AutoHeight().Padding(0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("ERROR — ") + Error))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.30f, 0.25f)))
			];
		}
		for (const FString& Warning : Report.ValidationWarnings)
		{
			Health->AddSlot().AutoHeight().Padding(0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("WARNING — ") + Warning))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.25f)))
			];
		}
		Content->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(
					TEXT("SETUP HEALTH  //  %d ERRORS  •  %d WARNINGS"),
					Report.ValidationErrors.Num(), Report.ValidationWarnings.Num())))
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
			]
			.BodyContent()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
				.Padding(8.f)
				[Health]
			]
		];
	}
}

FReply STerritoryStoryOutcomePanel::CopyReportToClipboard()
{
	const FTerritoryStoryOutcomeReport Report =
		FTerritoryStoryOutcomeAnalyzer::Analyze(Definition.Get(), true);
	FPlatformApplicationMisc::ClipboardCopy(*Report.BuildPlainText());
	return FReply::Handled();
}
