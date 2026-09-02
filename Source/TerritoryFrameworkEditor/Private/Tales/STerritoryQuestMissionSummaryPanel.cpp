#include "Tales/STerritoryQuestMissionSummaryPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/QuestTask.h"
#include "Tales/TerritoryQuestCascadeRecipe.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FString ConditionText(const UNarrativeCondition* Condition)
	{
		if (!Condition) return TEXT("EMPTY CONDITION");
		const FString Text = const_cast<UNarrativeCondition*>(Condition)
			->GetGraphDisplayText();
		return Text.IsEmpty() ? GetNameSafe(Condition) : Text;
	}

	FString EventText(const UNarrativeEvent* Event)
	{
		if (!Event) return TEXT("EMPTY EVENT");
		const FString Text = const_cast<UNarrativeEvent*>(Event)
			->GetGraphDisplayText();
		return Text.IsEmpty() ? GetNameSafe(Event) : Text;
	}

	FString StateTypeText(const ETerritoryQuestCascadeStateType Type)
	{
		switch (Type)
		{
		case ETerritoryQuestCascadeStateType::Objective: return TEXT("OBJECTIVE");
		case ETerritoryQuestCascadeStateType::Success: return TEXT("SUCCESS ENDING");
		case ETerritoryQuestCascadeStateType::Failure: return TEXT("FAILURE ENDING");
		default: return TEXT("UNKNOWN");
		}
	}

	FString CheckpointModeText(const ETerritoryQuestCheckpointMode Mode)
	{
		switch (Mode)
		{
		case ETerritoryQuestCheckpointMode::Disabled:
			return TEXT("No automatic checkpoints");
		case ETerritoryQuestCheckpointMode::ObjectiveStates:
			return TEXT("Save at every Objective state");
		case ETerritoryQuestCheckpointMode::EveryState:
			return TEXT("Save at every state, including endings");
		default:
			return TEXT("Unknown checkpoint policy");
		}
	}
}

void STerritoryQuestMissionSummaryPanel::Construct(const FArguments& InArgs)
{
	Recipe = InArgs._Recipe;
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
		this, &STerritoryQuestMissionSummaryPanel::HandleObjectPropertyChanged);
	Rebuild();
}

STerritoryQuestMissionSummaryPanel::~STerritoryQuestMissionSummaryPanel()
{
	if (PropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	}
}

void STerritoryQuestMissionSummaryPanel::HandleObjectPropertyChanged(
	UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	UTerritoryQuestCascadeRecipe* Current = Recipe.Get();
	if (!Current || !Object || (Object != Current && !Object->IsIn(Current)))
	{
		return;
	}
	if (bRefreshScheduled) return;
	bRefreshScheduled = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(
		this, &STerritoryQuestMissionSummaryPanel::HandleDeferredRefresh));
}

EActiveTimerReturnType
STerritoryQuestMissionSummaryPanel::HandleDeferredRefresh(
	double CurrentTime, float DeltaTime)
{
	bRefreshScheduled = false;
	Rebuild();
	return EActiveTimerReturnType::Stop;
}

TSharedRef<SWidget> STerritoryQuestMissionSummaryPanel::BuildBranchBody(
	const FTerritoryQuestCascadeBranch& Branch,
	const int32 SharedConditionCount) const
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.f, 1.f, 0.f, 5.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(
			TEXT("Destination: %s\nLogic: %d state + %d route condition(s) must remain true; every non-optional task must complete.%s\nEvents: %d"),
			*Branch.DestinationStateID.ToString(), SharedConditionCount,
			Branch.Conditions.Num(), Branch.bHidden
				? TEXT("\nPresentation: hidden route") : TEXT(""),
			Branch.Events.Num())))
		.AutoWrapText(true)
	];

	for (const UNarrativeCondition* Condition : Branch.Conditions)
	{
		Box->AddSlot().AutoHeight().Padding(8.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("REQUIRE  •  ") + ConditionText(Condition)))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.78f, 0.30f)))
			.AutoWrapText(true)
		];
	}
	for (const UNarrativeTask* Task : Branch.Tasks)
	{
		const FString Flags = Task
			? FString::Printf(TEXT("%s%s%s"),
				Task->bOptional ? TEXT(" • OPTIONAL") : TEXT(" • REQUIRED"),
				Task->bHidden ? TEXT(" • HIDDEN") : TEXT(""),
				Task->MarkerSettings.bAddNavigationMarker
					? TEXT(" • MARKER") : TEXT(""))
			: FString();
		Box->AddSlot().AutoHeight().Padding(8.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("TASK  •  %s  •  x%d%s"),
				Task ? *Task->GetTaskDescription().ToString() : TEXT("EMPTY TASK"),
				Task ? Task->RequiredQuantity : 0, *Flags)))
			.AutoWrapText(true)
		];
	}
	for (const UNarrativeEvent* Event : Branch.Events)
	{
		Box->AddSlot().AutoHeight().Padding(8.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("EVENT  •  ") + EventText(Event)))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.38f, 0.68f, 0.95f)))
			.AutoWrapText(true)
		];
	}
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.Padding(FMargin(9.f, 6.f))[Box];
}

void STerritoryQuestMissionSummaryPanel::Rebuild()
{
	if (!Content.IsValid()) return;
	Content->ClearChildren();
	const UTerritoryQuestCascadeRecipe* Current = Recipe.Get();
	if (!Current) return;
	const FTerritoryQuestCascadeLogicSummary Summary =
		Current->BuildMissionLogicSummary();
	const bool bHasRuntimeQuest = !Current->NarrativeQuestGraph.IsNull();
	const FTerritoryQuestCascadeLogicSummary RuntimeSummary =
		Current->BuildSelectedRuntimeQuestSummary();
	const FString RuntimeReport = bHasRuntimeQuest
		? Current->BuildSelectedRuntimeQuestReport()
		: FString(TEXT("Select a Narrative Quest Graph above, then press Refresh Runtime Quest Summary. The report reads the compiled Quest template used by Narrative at runtime; it never changes or starts the selected Quest."));

	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
	[
		SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(bHasRuntimeQuest
						? FString::Printf(TEXT("RUNTIME QUEST GRAPH  |  %s  |  %s"),
							*RuntimeSummary.InspectedQuestName.ToString(),
							RuntimeSummary.bValid ? TEXT("READY") : TEXT("CHECK FINDINGS"))
						: TEXT("RUNTIME QUEST GRAPH  |  NOT SELECTED")))
					.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
					.ColorAndOpacity(!bHasRuntimeQuest
						? FSlateColor(FLinearColor(0.65f, 0.72f, 0.78f))
						: RuntimeSummary.bValid
							? FSlateColor(FLinearColor(0.32f, 0.78f, 0.55f))
							: FSlateColor(FLinearColor(0.95f, 0.72f, 0.25f)))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(bHasRuntimeQuest
						? RuntimeSummary.Headline
						: TEXT("Inspect the complete compiled Narrative Quest, including options that were authored outside this recipe.")))
					.AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Refresh Runtime Quest Summary")))
				.ToolTipText(FText::FromString(
					TEXT("Reload the selected Quest class and rebuild this report from its compiled runtime graph. Use this after compiling or saving the Narrative Quest.")))
				.OnClicked(this,
					&STerritoryQuestMissionSummaryPanel::RefreshRuntimeQuestSummary)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Copy Runtime Report")))
				.IsEnabled(bHasRuntimeQuest)
				.ToolTipText(FText::FromString(
					TEXT("Copy every compiled state, route, task option, condition, event, journal setting, and validation finding.")))
				.OnClicked(this,
					&STerritoryQuestMissionSummaryPanel::CopyRuntimeQuestReportToClipboard)
			]
		]
		.BodyContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
			.Padding(FMargin(8.f, 6.f))
			[
				SNew(SBox)
				.MaxDesiredHeight(520.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SMultiLineEditableText)
						.Text(FText::FromString(RuntimeReport))
						.IsReadOnly(true)
						.AutoWrapText(true)
					]
				]
			]
		]
	];

	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  |  %s"),
					*Current->QuestName.ToString(), Summary.bValid
						? TEXT("READY TO GENERATE") : TEXT("NEEDS SETUP"))))
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
				.ColorAndOpacity(Summary.bValid
					? FSlateColor(FLinearColor(0.32f, 0.78f, 0.55f))
					: FSlateColor(FLinearColor(0.95f, 0.30f, 0.25f)))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Summary.Headline))
				.AutoWrapText(true)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Copy Mission Report")))
			.ToolTipText(FText::FromString(
				TEXT("Copy the read-only mission flow, task options, conditions, events, and validation findings.")))
			.OnClicked(this,
				&STerritoryQuestMissionSummaryPanel::CopyReportToClipboard)
		]
	];

	Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 7.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(
			TEXT("State conditions gate every route leaving that state. Branch conditions gate only that route. Conditions use AND logic. Separate branches are alternative routes. Events keep their own optional conditions. Checkpoint policy: %s."),
			*CheckpointModeText(Current->CheckpointMode))))
		.AutoWrapText(true)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.72f, 0.78f)))
	];

	for (int32 StateIndex = 0; StateIndex < Current->States.Num(); ++StateIndex)
	{
		const FTerritoryQuestCascadeState& State = Current->States[StateIndex];
		TSharedRef<SVerticalBox> StateBody = SNew(SVerticalBox);
		if (!State.Description.IsEmpty())
		{
			StateBody->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 5.f)
			[
				SNew(STextBlock).Text(State.Description).AutoWrapText(true)
			];
		}
		for (const UNarrativeCondition* Condition : State.Conditions)
		{
			StateBody->AddSlot().AutoHeight().Padding(4.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(
					TEXT("SHARED REQUIREMENT  •  ") + ConditionText(Condition)))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.78f, 0.30f)))
				.AutoWrapText(true)
			];
		}
		for (const UNarrativeEvent* Event : State.Events)
		{
			StateBody->AddSlot().AutoHeight().Padding(4.f, 1.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("STATE EVENT  •  ") + EventText(Event)))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.38f, 0.68f, 0.95f)))
				.AutoWrapText(true)
			];
		}
		for (const FTerritoryQuestCascadeBranch& Branch : State.Branches)
		{
			StateBody->AddSlot().AutoHeight().Padding(0.f, 4.f)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(false)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("ROUTE %s  ->  %s"), *Branch.BranchID.ToString(),
						*Branch.DestinationStateID.ToString())))
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
				]
				.BodyContent()
				[
					BuildBranchBody(Branch, State.Conditions.Num())
				]
			];
		}

		Content->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(StateIndex > 1)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  |  %s  |  %d route(s)"),
					*State.StateID.ToString(), *StateTypeText(State.Type),
					State.Branches.Num())))
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
			]
			.BodyContent()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
				.Padding(FMargin(9.f, 6.f))[StateBody]
			]
		];
	}

	for (const FText& Error : Summary.Errors)
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("ERROR — ") + Error.ToString()))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.30f, 0.25f)))
			.AutoWrapText(true)
		];
	}
	for (const FText& Warning : Summary.Warnings)
	{
		Content->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("WARNING — ") + Warning.ToString()))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.25f)))
			.AutoWrapText(true)
		];
	}
}

FReply STerritoryQuestMissionSummaryPanel::CopyReportToClipboard()
{
	if (const UTerritoryQuestCascadeRecipe* Current = Recipe.Get())
	{
		FPlatformApplicationMisc::ClipboardCopy(
			*Current->BuildPlainTextPreview());
	}
	return FReply::Handled();
}

FReply STerritoryQuestMissionSummaryPanel::RefreshRuntimeQuestSummary()
{
	Rebuild();
	return FReply::Handled();
}

FReply STerritoryQuestMissionSummaryPanel::CopyRuntimeQuestReportToClipboard()
{
	if (const UTerritoryQuestCascadeRecipe* Current = Recipe.Get())
	{
		FPlatformApplicationMisc::ClipboardCopy(
			*Current->BuildSelectedRuntimeQuestReport());
	}
	return FReply::Handled();
}
